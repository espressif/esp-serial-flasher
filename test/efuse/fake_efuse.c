/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * The fake "chip" the eFuse tests run against. It replaces esp_loader.c, whose
 * three register-I/O functions are the seam between the eFuse engine and real
 * hardware, with a plain address -> value map. Reading this file in order:
 *
 *   state          — everything the fake remembers, in one struct
 *   register store — the map itself, plus the write log
 *   test setup     — what a test calls to arrange a starting state
 *   device response— optional: makes writes behave like a chip responding
 *   the seam       — the three functions the engine actually calls
 *   port ops       — the three callbacks the burn path needs from a port
 *
 * With device response off, a write is just stored. With it on, the fake also
 * acts on commands: a PGM_CMD makes the burn take effect and the command
 * register self-clears, so the engine's poll-and-verify loop can complete.
 * Only the wire protocol is modelled — RS/3-4 parity, WR_DIS/RD_DIS semantics
 * and timing are left to the real engine under test.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fake_efuse.h"
#include "esp_loader_efuse_private.h"
#include "esp_targets.h"

/* Generous fixed capacities: the fake only ever holds one chip's eFuse words
 * plus the writes of a single test case. Overflow means the fake stopped
 * modelling what the code under test did, so it aborts rather than dropping the
 * entry — a dropped register or write would let a test pass on a fake that no
 * longer matches reality. */
#define FAKE_MAX_REGS   512
#define FAKE_MAX_WRITES 512

#define FAKE_FATAL(...)                                                        \
    do {                                                                       \
        fprintf(stderr, "fake_efuse: " __VA_ARGS__);                           \
        abort();                                                               \
    } while (0)

typedef struct {
    uint32_t addr;
    uint32_t value;
} fake_reg_t;

/* ── State ──────────────────────────────────────────────────────────────────── */

/* One struct rather than a dozen file-scope variables, so fake_efuse_reset()
 * is a single assignment and state added later is cleared without anyone
 * having to remember to clear it. The seam functions the fake implements
 * (esp_loader_read_register and friends) take no context argument, so the
 * state has to be file-scope; this at least keeps it in one place. */
typedef struct {
    fake_reg_t       regs[FAKE_MAX_REGS];
    uint32_t         reg_count;

    fake_reg_write_t writes[FAKE_MAX_WRITES];
    uint32_t         write_count;

    esp_loader_target_security_info_t sec_info;

    /* The layout of the chip being modelled, and NULL when none is: a write is
     * then only stored, rather than acted on as a command (see
     * esp_loader_write_register). efuse_base is that chip's eFuse peripheral
     * base, cached to turn the layout's register offsets into absolute
     * addresses. */
    const efuse_chip_layout_t *chip_layout;
    uint32_t                   efuse_base;

    uint8_t err_block;
    uint8_t err_attempts_left;

    esp_loader_port_t port;
    bool              poll_available;
} fake_state_t;

static fake_state_t g;

void fake_efuse_reset(void)
{
    g = (fake_state_t) {
        0
    };
    g.err_block = EFUSE_INDEX_NONE;
}

/* ── The register store ─────────────────────────────────────────────────────
 * A sparse address -> value map. Sparse rather than a flat array because
 * esp_loader_read_register() gets an absolute address with no target context,
 * so the fake cannot know the eFuse base or bounds at read time.
 */

static fake_reg_t *find_reg(uint32_t addr)
{
    for (uint32_t i = 0; i < g.reg_count; i++) {
        if (g.regs[i].addr == addr) {
            return &g.regs[i];
        }
    }
    return NULL;
}

/* An address never written to reads 0 — a blank chip: every key block free,
 * nothing protected. */
static uint32_t read_reg(uint32_t addr)
{
    const fake_reg_t *r = find_reg(addr);
    return (r != NULL) ? r->value : 0u;
}

static void set_reg(uint32_t addr, uint32_t value)
{
    fake_reg_t *r = find_reg(addr);
    if (r == NULL) {
        if (g.reg_count >= FAKE_MAX_REGS) {
            FAKE_FATAL("out of register slots (%d) at 0x%08x; raise "
                       "FAKE_MAX_REGS\n", FAKE_MAX_REGS, addr);
        }
        r = &g.regs[g.reg_count++];
        r->addr = addr;
    }
    r->value = value;
}

static void record_write(uint32_t addr, uint32_t value)
{
    if (g.write_count >= FAKE_MAX_WRITES) {
        FAKE_FATAL("write transcript full (%d) at 0x%08x; raise FAKE_MAX_WRITES\n",
                   FAKE_MAX_WRITES, addr);
    }
    g.writes[g.write_count].addr = addr;
    g.writes[g.write_count].value = value;
    g.write_count++;
}

/* ── Test setup: what a test calls before exercising the code ───────────────── */

void fake_set_efuse_word(target_chip_t target, uint8_t block, uint8_t word, uint32_t value)
{
    const efuse_chip_layout_t *layout = efuse_get_chip_layout(target);
    uint32_t base = esp_targets_get_efuse_base(target);
    set_reg(base + layout->blocks[block].read_offset + (uint32_t)word * EFUSE_WORD_BYTES, value);
}

uint32_t fake_get_efuse_word(target_chip_t target, uint8_t block, uint8_t word)
{
    const efuse_chip_layout_t *layout = efuse_get_chip_layout(target);
    uint32_t base = esp_targets_get_efuse_base(target);
    return read_reg(base + layout->blocks[block].read_offset + (uint32_t)word * EFUSE_WORD_BYTES);
}

void fake_set_reg(uint32_t addr, uint32_t value)
{
    set_reg(addr, value);
}

void fake_set_efuse_bit(target_chip_t target, uint8_t block, uint16_t bit)
{
    const efuse_chip_layout_t *layout = efuse_get_chip_layout(target);
    uint32_t base = esp_targets_get_efuse_base(target);
    uint32_t word = bit / (EFUSE_WORD_BYTES * 8u);
    uint32_t addr = base + layout->blocks[block].read_offset + word * EFUSE_WORD_BYTES;

    set_reg(addr, read_reg(addr) | (1u << (bit % (EFUSE_WORD_BYTES * 8u))));
}

/* The only lever on esp_loader_get_security_info(), whose result is not derived
 * from the register store. Nothing calls this yet — it is what a test for the
 * ESP32-P4 ECO rev3 path in esp_loader_efuse_keys.c would need. */
void fake_set_security_info(const esp_loader_target_security_info_t *info)
{
    g.sec_info = *info;
}

const fake_reg_write_t *fake_writes(void)
{
    return g.writes;
}

uint32_t fake_write_count(void)
{
    return g.write_count;
}

/* ── Device response ────────────────────────────────────────────────────────── */

void fake_efuse_reset_for_burn(target_chip_t target)
{
    fake_efuse_reset();
    g.chip_layout = efuse_get_chip_layout(target);
    g.efuse_base = esp_targets_get_efuse_base(target);

    /* ESP32 legacy's register layout is private to esp_loader_efuse_burn.c
     * (not part of efuse_chip_layout_t), so it isn't modelled here yet — see
     * EFUSE-FOLLOWUP list / next step. Fail loudly rather than dereference a
     * NULL ->burn or silently do nothing on commit. */
    if (g.chip_layout == NULL || g.chip_layout->burn == NULL) {
        FAKE_FATAL("device response not implemented for target %d (no "
                   "efuse_burn_regs_t; ESP32 legacy is a follow-up)\n", (int)target);
    }
}

void fake_inject_error(uint8_t block, uint8_t attempts)
{
    g.err_block = block;
    g.err_attempts_left = attempts;
}

/* True (and consumes one attempt) iff `block` is the one under error
 * injection and attempts remain — the caller skips reflecting real data, so
 * read-back verification in esp_loader_efuse_burn.c sees a mismatch and the
 * burn "fails", driving its retry loop exactly as a real RS/REPEAT error would. */
static bool consume_injected_failure(uint8_t block)
{
    if (g.err_block == block && g.err_attempts_left > 0u) {
        g.err_attempts_left--;
        return true;
    }
    return false;
}

/* The burn takes effect: OR the PGM_DATA registers the caller filled in into
 * the block's read-side registers, matching one-time-programmable accumulation.
 * Addresses come from the linked layout, so there are no chip-specific numbers
 * here. Skipped entirely under error injection, which is what makes the burn
 * look like it failed. */
static void apply_burn(uint8_t block)
{
    if (block >= g.chip_layout->block_count || consume_injected_failure(block)) {
        return;
    }
    const efuse_burn_regs_t *regs = g.chip_layout->burn;
    uint32_t pgm_data0 = g.efuse_base + regs->pgm->pgm_data0_off;
    uint32_t read_base = g.efuse_base + g.chip_layout->blocks[block].read_offset;
    uint8_t  word_count = g.chip_layout->blocks[block].word_count;

    for (uint8_t w = 0; w < word_count; w++) {
        uint32_t read_addr = read_base + w * EFUSE_WORD_BYTES;
        set_reg(read_addr, read_reg(read_addr) | read_reg(pgm_data0 + w * EFUSE_WORD_BYTES));
    }
}

/* ── The seam: the three functions esp_loader.c would have defined ──────────── */

esp_loader_error_t esp_loader_read_register(esp_loader_t *loader, uint32_t address, uint32_t *reg_value)
{
    (void)loader;
    *reg_value = read_reg(address);
    return ESP_LOADER_SUCCESS;
}

esp_loader_error_t esp_loader_write_register(esp_loader_t *loader, uint32_t address, uint32_t reg_value)
{
    (void)loader;
    record_write(address, reg_value);

    if (g.chip_layout == NULL) {
        set_reg(address, reg_value); /* no device response: a write is just stored */
        return ESP_LOADER_SUCCESS;
    }

    /* Modern chips only: ESP32 legacy has no efuse_burn_regs_t and
     * fake_efuse_reset_for_burn() aborts on it, so ->burn is non-NULL here.
     *
     * A write to the command register is a command being issued: PGM_CMD burns
     * the block it names. The register then reads back as 0 either way,
     * mirroring hardware clearing the bit once the command completes — which is
     * what lets esp_loader_efuse_burn.c's poll loop terminate. */
    const efuse_burn_regs_t *regs = g.chip_layout->burn;
    uint32_t cmd_addr = g.efuse_base + regs->cmd_off;

    if (address != cmd_addr) {
        set_reg(address, reg_value);
    } else {
        if (reg_value & regs->ctrl->pgm_cmd) {
            apply_burn((uint8_t)(reg_value >> regs->ctrl->cmd_block_shift));
        }
        set_reg(cmd_addr, 0u);
    }
    return ESP_LOADER_SUCCESS;
}

esp_loader_error_t esp_loader_get_security_info(esp_loader_t *loader,
        esp_loader_target_security_info_t *security_info)
{
    (void)loader;
    *security_info = g.sec_info;
    return ESP_LOADER_SUCCESS;
}

/* ── Minimal port required by the production poll loop ─────────────────────── */

static void fake_start_timer(esp_loader_port_t *port, uint32_t ms)
{
    (void)port;
    (void)ms;
    g.poll_available = true;
}

static uint32_t fake_remaining_time(esp_loader_port_t *port)
{
    (void)port;
    if (!g.poll_available) {
        return 0u;
    }
    g.poll_available = false;
    return 1u;
}

static void fake_delay_ms(esp_loader_port_t *port, uint32_t ms)
{
    (void)port;
    (void)ms;
}

static const esp_loader_port_ops_t fake_port_ops = {
    .start_timer     = fake_start_timer,
    .remaining_time  = fake_remaining_time,
    .delay_ms        = fake_delay_ms,
};

esp_loader_t fake_loader(target_chip_t chip)
{
    esp_loader_t loader = { 0 };

    g.port = (esp_loader_port_t) {
        .ops = &fake_port_ops,
    };
    loader._target = chip;
    loader._port = &g.port;
    return loader;
}
