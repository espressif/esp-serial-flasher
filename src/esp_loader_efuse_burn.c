/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <string.h>
#include "esp_loader.h"
#include "esp_loader_efuse.h"
#include "esp_loader_efuse_private.h"
#include "esp_targets.h"

#define IDLE_POLL_TIMEOUT_MS 250u
#define BURN_MAX_RETRIES     3u
#define READ_VERIFY_PASSES   5u

/* ── Helpers ────────────────────────────────────────────────────────────────── */

static esp_loader_error_t update_reg(esp_loader_t *loader, uint32_t addr, uint32_t mask, uint32_t val)
{
    uint32_t reg;
    RETURN_ON_ERROR(esp_loader_read_register(loader, addr, &reg));
    reg = (reg & ~mask) | (val & mask);
    return esp_loader_write_register(loader, addr, reg);
}

/*
 * Shared by both the modern and ESP32 legacy paths. The double-read is only
 * strictly required after READ_CMD on the modern controller (hardware
 * errata, lets the efuse clock stabilise) — but re-reading a status
 * register is always safe (read-only, idempotent, negligible extra
 * latency), so it's applied unconditionally rather than gated per chip.
 */
static esp_loader_error_t efuse_wait_idle(esp_loader_t *loader, uint32_t cmd_addr, uint32_t cmd_mask)
{
    loader->_port->ops->start_timer(loader->_port, IDLE_POLL_TIMEOUT_MS);
    while (loader->_port->ops->remaining_time(loader->_port) > 0) {
        uint32_t cmd;
        RETURN_ON_ERROR(esp_loader_read_register(loader, cmd_addr, &cmd));
        if (cmd & cmd_mask) {
            continue;
        }
        RETURN_ON_ERROR(esp_loader_read_register(loader, cmd_addr, &cmd));
        if (!(cmd & cmd_mask)) {
            return ESP_LOADER_SUCCESS;
        }
    }
    return ESP_LOADER_ERROR_TIMEOUT;
}

static bool block_has_staged(const uint32_t *buf, uint8_t word_count)
{
    for (uint8_t w = 0; w < word_count; w++) {
        if (buf[w] != 0u) {
            return true;
        }
    }
    return false;
}

/* Drop a block's staging after a verified-successful burn (remaining-work model). */
static void clear_staged_block(esp_loader_efuse_ctx_t *ctx, uint8_t block)
{
    memset(&ctx->_buf[block * ESP_LOADER_EFUSE_MAX_WORDS_PER_BLOCK], 0,
           ESP_LOADER_EFUSE_MAX_WORDS_PER_BLOCK * EFUSE_WORD_BYTES);
}

/* Contiguous BLK0 bit-range helpers for WR_DIS / RD_DIS postpone.
 * Walk word-by-word so ranges may span multiple words. Ranges come from the
 * static chip layout (already sized for that chip's BLK0). */
static void clear_bit_range(uint32_t *words, uint8_t word_count, const efuse_bit_range_t *range)
{
    uint16_t bit = range->bit_start;
    uint16_t end = bit + range->bit_count;

    while (bit < end) {
        uint16_t word = bit / 32u;
        if (word >= word_count) {
            break;
        }
        uint16_t shift = bit % 32u;
        uint16_t n = end - bit;
        if (n > 32u - shift) {
            n = 32u - shift;
        }
        uint32_t mask = (n >= 32u) ? 0xFFFFFFFFu : ((1u << n) - 1u);
        words[word] &= ~(mask << shift);
        bit += n;
    }
}

/* OR bits in `range` from src into dst (multi-word safe). */
static void copy_bit_range(uint32_t *dst, const uint32_t *src, uint8_t word_count,
                           const efuse_bit_range_t *range)
{
    uint16_t bit = range->bit_start;
    uint16_t end = bit + range->bit_count;

    while (bit < end) {
        uint16_t word = bit / 32u;
        if (word >= word_count) {
            break;
        }
        uint16_t shift = bit % 32u;
        uint16_t n = end - bit;
        if (n > 32u - shift) {
            n = 32u - shift;
        }
        uint32_t mask = (n >= 32u) ? 0xFFFFFFFFu : ((1u << n) - 1u);
        dst[word] |= src[word] & (mask << shift);
        bit += n;
    }
}

/* Single-bit helpers for the transport-disabling fuses, expressed as one-bit
 * ranges so they reuse clear_bit_range()/copy_bit_range(). */
static bool bit_is_staged(const uint32_t *words, uint8_t word_count, uint16_t bit)
{
    if (bit == ESP_LOADER_EFUSE_BIT_NONE) {
        return false;
    }
    uint16_t word = bit / 32u;
    return (word < word_count) && ((words[word] >> (bit % 32u)) & 1u);
}

static efuse_bit_range_t one_bit(uint16_t bit)
{
    efuse_bit_range_t r = { bit, 1u };
    return r;
}

static esp_loader_error_t efuse_clear_pgm_regs(esp_loader_t *loader, uint32_t base, const efuse_burn_regs_t *regs)
{
    uint32_t pgm_data0 = base + regs->pgm->pgm_data0_off;
    uint32_t cmd_addr  = base + regs->cmd_off;
    uint32_t cmd_mask  = regs->ctrl->pgm_cmd | regs->ctrl->read_cmd;

    RETURN_ON_ERROR(efuse_wait_idle(loader, cmd_addr, cmd_mask));
    for (uint8_t i = 0; i < regs->pgm->pgm_data_words; i++) {
        RETURN_ON_ERROR(esp_loader_write_register(loader, pgm_data0 + i * EFUSE_WORD_BYTES, 0u));
    }
    /* CHECK_VALUE registers are not cleared: the burn engine always overwrites
     * them with fresh RS parity before issuing PGM_CMD. */
    return ESP_LOADER_SUCCESS;
}

/* ── Timing setup ───────────────────────────────────────────────────────────── */

static esp_loader_error_t efuse_set_timing(esp_loader_t *loader, target_chip_t target, uint32_t base)
{
    switch (target) {
    case ESP32C3_CHIP: {
        const uint8_t  dac_clk_div = 0x28u;
        const uint8_t  dac_num     = 0xFFu;
        const uint16_t pwr_on_num  = 0x3000u;
        const uint16_t pwr_off_num = 0x190u;
        uint32_t dac = base + 0x1E8u;
        uint32_t wc1 = base + 0x1F0u;
        uint32_t wc2 = base + 0x1F4u;
        RETURN_ON_ERROR(update_reg(loader, dac, 0xFFu << 9u,   (uint32_t)dac_num    << 9u));
        RETURN_ON_ERROR(update_reg(loader, dac, 0xFFu,         dac_clk_div));
        RETURN_ON_ERROR(update_reg(loader, wc1, 0xFFFFu << 8u, (uint32_t)pwr_on_num << 8u));
        RETURN_ON_ERROR(update_reg(loader, wc2, 0xFFFFu,       pwr_off_num));
        break;
    }

    case ESP32S3_CHIP: {
        const uint8_t  dac_clk_div = 0x28u;
        const uint8_t  dac_num     = 0xFFu;
        const uint16_t pwr_on_num  = 0x3000u;
        const uint16_t pwr_off_num = 0x190u;
        uint32_t dac = base + 0x1E8u;
        uint32_t wc1 = base + 0x1F4u;
        uint32_t wc2 = base + 0x1F8u;
        RETURN_ON_ERROR(update_reg(loader, dac, 0xFFu << 9u,   (uint32_t)dac_num    << 9u));
        RETURN_ON_ERROR(update_reg(loader, dac, 0xFFu,         dac_clk_div));
        RETURN_ON_ERROR(update_reg(loader, wc1, 0xFFFFu << 8u, (uint32_t)pwr_on_num << 8u));
        RETURN_ON_ERROR(update_reg(loader, wc2, 0xFFFFu,       pwr_off_num));
        break;
    }

    case ESP32C2_CHIP: {
        /* DAC/PWR values are crystal-independent; only TPGM_INACTIVE differs
         * between a 26 MHz (130) and 40 MHz (200) crystal. */
        const uint8_t  dac_clk_div = 0x28u;
        const uint8_t  dac_num     = 0xFFu;
        const uint16_t pwr_on_num  = 0x3000u;
        const uint16_t pwr_off_num = 0x190u;
        uint32_t dac = base + 0x108u;
        uint32_t wc0 = base + 0x110u;
        uint32_t wc1 = base + 0x114u;
        uint32_t wc2 = base + 0x118u;
        uint32_t xtal_mhz = 40u;
        RETURN_ON_ERROR(get_crystal_frequency_esp32c2(loader, &xtal_mhz));
        uint32_t tpgm_inactive = (xtal_mhz == 26u) ? 130u : 200u;
        RETURN_ON_ERROR(update_reg(loader, dac, 0xFFu << 9u,   (uint32_t)dac_num    << 9u));
        RETURN_ON_ERROR(update_reg(loader, dac, 0xFFu,         dac_clk_div));
        RETURN_ON_ERROR(update_reg(loader, wc1, 0xFFFFu << 8u, (uint32_t)pwr_on_num << 8u));
        RETURN_ON_ERROR(update_reg(loader, wc2, 0xFFFFu,       pwr_off_num));
        RETURN_ON_ERROR(update_reg(loader, wc0, 0xFFu << 8u,   tpgm_inactive << 8u));
        break;
    }

    case ESP32S2_CHIP: {
        /* S2: 40 MHz crystal only (same limitation as espefuse). Does not program DAC_NUM.
         * WR_TIM_CONF1: TSUP_A (bits 0-7), PWR_ON_NUM (bits 8-23).
         * WR_TIM_CONF0: THP_A (bits 0-7), TPGM_INACTIVE (bits 8-15), TPGM (bits 16-31). */
        const uint8_t  dac_clk_div = 0x50u;
        const uint16_t pwr_on_num  = 0x5100u;
        const uint16_t pwr_off_num = 0x80u;
        uint32_t dac = base + 0x1E8u;
        uint32_t wc0 = base + 0x1F0u;
        uint32_t wc1 = base + 0x1F4u;
        uint32_t wc2 = base + 0x1F8u;
        RETURN_ON_ERROR(update_reg(loader, wc1, 0xFFu,          0x1u));          /* TSUP_A        */
        RETURN_ON_ERROR(update_reg(loader, wc0, 0xFFFFu << 16u, 0x190u << 16u)); /* TPGM          */
        RETURN_ON_ERROR(update_reg(loader, wc0, 0xFFu,          0x1u));          /* THP_A         */
        RETURN_ON_ERROR(update_reg(loader, wc0, 0xFFu << 8u,    0x2u << 8u));    /* TPGM_INACTIVE */
        RETURN_ON_ERROR(update_reg(loader, dac, 0xFFu,          dac_clk_div));
        RETURN_ON_ERROR(update_reg(loader, wc1, 0xFFFFu << 8u,  (uint32_t)pwr_on_num << 8u));
        RETURN_ON_ERROR(update_reg(loader, wc2, 0xFFFFu,        pwr_off_num));
        break;
    }

    default:
        /* Factory timing: no register writes needed. */
        break;
    }

    return ESP_LOADER_SUCCESS;
}

/* ── Error checks ───────────────────────────────────────────────────────────── */

static esp_loader_error_t efuse_check_blk0_error(esp_loader_t *loader, uint32_t base,
        const efuse_burn_regs_t *regs)
{
    for (uint8_t i = 0; i < regs->repeat_err_words; i++) {
        uint32_t val;
        RETURN_ON_ERROR(esp_loader_read_register(loader, base + regs->repeat_err0_off + i * EFUSE_WORD_BYTES, &val));
        if (val != 0u) {
            return ESP_LOADER_ERROR_EFUSE_BURN_FAILED;
        }
    }
    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t efuse_check_rs_error(esp_loader_t *loader, uint32_t base,
        const efuse_burn_regs_t *regs, uint8_t block)
{
    if (block == 0u) {
        return ESP_LOADER_SUCCESS;  /* BLK0 uses REPEAT_ERR, not RS_ERR */
    }
    /* Each RS block occupies 4 bits in RS_ERR0 (blocks 1-8) or RS_ERR1 (9-10).
     * Bit layout per block: [num_errors:3][fail:1] at offset (block-1)%8 * 4.
     * C3 hardware errata: fail bit is one block position ahead of err_num;
     * BLK10's fail bit is absent (shifted off the end) — skip it. */
    uint8_t idx = block - 1;
    uint8_t fail_idx = (loader->_target == ESP32C3_CHIP) ? idx + 1 : idx;
    if (fail_idx >= 10u) {
        return ESP_LOADER_SUCCESS;  /* BLK10 on C3: no fail bit */
    }
    /* RS_ERR1 always follows RS_ERR0 in the next word, on every chip. */
    uint32_t err_reg = base + regs->rs_err0_off + ((fail_idx < 8u) ? 0u : EFUSE_WORD_BYTES);
    /* (fail_idx % 8) * 4 is at most 28; cast is only to undo integer promotion. */
    uint8_t shift = (uint8_t)((fail_idx % 8u) * 4u);
    uint32_t fail_mask = 1u << (shift + 3u);

    uint32_t val;
    RETURN_ON_ERROR(esp_loader_read_register(loader, err_reg, &val));
    if (val & fail_mask) {
        return ESP_LOADER_ERROR_EFUSE_BURN_FAILED;
    }
    return ESP_LOADER_SUCCESS;
}

/* ── efuse_read: issue READ_CMD and refresh read registers ──────────────────── */

static esp_loader_error_t efuse_read(esp_loader_t *loader, uint32_t base,
                                     const efuse_burn_regs_t *regs)
{
    uint32_t conf_addr = base + regs->conf_off;
    uint32_t cmd_addr  = base + regs->cmd_off;
    uint32_t cmd_mask  = regs->ctrl->pgm_cmd | regs->ctrl->read_cmd;

    RETURN_ON_ERROR(efuse_wait_idle(loader, cmd_addr, cmd_mask));
    RETURN_ON_ERROR(esp_loader_write_register(loader, conf_addr, regs->ctrl->read_op));
    RETURN_ON_ERROR(esp_loader_write_register(loader, cmd_addr,  regs->ctrl->read_cmd));
    loader->_port->ops->delay_ms(loader->_port, 1u);
    return efuse_wait_idle(loader, cmd_addr, cmd_mask);
}

/* ── Burn one block ─────────────────────────────────────────────────────────── */

/* verify == false: the staged bits include a fuse that ends the download
 * transport, so nothing after PGM_CMD can be trusted to answer. Issue the
 * programming command, make a best-effort wait for the controller to go idle,
 * and report success without reading anything back — see the comment on
 * efuse_chip_layout_t::dis_download_mode_bit. */
static esp_loader_error_t burn_block_once(esp_loader_t *loader,
        const esp_loader_efuse_ctx_t *ctx,
        const efuse_chip_layout_t *layout,
        uint32_t base, uint8_t block, bool verify)
{
    const efuse_burn_regs_t *regs  = layout->burn;
    uint8_t word_count             = layout->blocks[block].word_count;
    const uint32_t *buf            = &ctx->_buf[block * ESP_LOADER_EFUSE_MAX_WORDS_PER_BLOCK];
    uint32_t conf_addr             = base + regs->conf_off;
    uint32_t cmd_addr              = base + regs->cmd_off;
    uint32_t pgm_data0             = base + regs->pgm->pgm_data0_off;
    uint32_t check_value0          = base + regs->pgm->check_value0_off;
    uint32_t cmd_mask              = regs->ctrl->pgm_cmd | regs->ctrl->read_cmd;

    /* Controller setup, redone before every burn attempt (matches espefuse's
     * efuse_controller_setup(), which runs set_efuse_timing() + clear_pgm_registers()
     * + wait_efuse_idle() at the top of every retry, for every block — not once
     * globally before the whole BLK10..BLK0 loop). */
    RETURN_ON_ERROR(efuse_set_timing(loader, loader->_target, base));
    RETURN_ON_ERROR(efuse_clear_pgm_regs(loader, base, regs));
    RETURN_ON_ERROR(efuse_wait_idle(loader, cmd_addr, cmd_mask));

    /* Write data words into PGM_DATA registers (extras already zeroed by clear). */
    for (uint8_t w = 0; w < word_count; w++) {
        RETURN_ON_ERROR(esp_loader_write_register(loader, pgm_data0 + w * EFUSE_WORD_BYTES, buf[w]));
    }

    if (block > 0u) {
        /* BLK1+: RS(44,32) produces 12 parity bytes = 3 CHECK_VALUE words.
         * Buf is zero-padded beyond word_count inside ctx, so encoding is always over 8 words. */
        uint32_t parity[3];
        efuse_rs_encode(buf, parity);
        for (uint8_t w = 0; w < 3u; w++) {
            RETURN_ON_ERROR(esp_loader_write_register(loader, check_value0 + w * EFUSE_WORD_BYTES, parity[w]));
        }
    }

    /* Issue PGM_CMD. */
    RETURN_ON_ERROR(esp_loader_write_register(loader, conf_addr, regs->ctrl->write_op));
    RETURN_ON_ERROR(esp_loader_write_register(loader, cmd_addr, regs->ctrl->pgm_cmd | ((uint32_t)block << regs->ctrl->cmd_block_shift)));

    if (!verify) {
        /* The transport may already be gone; a failed poll is expected here and
         * is not evidence that programming failed. */
        (void)efuse_wait_idle(loader, cmd_addr, cmd_mask);
        return ESP_LOADER_SUCCESS;
    }

    RETURN_ON_ERROR(efuse_wait_idle(loader, cmd_addr, cmd_mask));

    /* Clear PGM registers, then issue READ_CMD up to RS_READ_PASSES times, checking error status after each pass. */
    RETURN_ON_ERROR(efuse_clear_pgm_regs(loader, base, regs));

    for (uint8_t r = 0; r < READ_VERIFY_PASSES; r++) {
        RETURN_ON_ERROR(efuse_read(loader, base, regs));
        esp_loader_error_t err = (block == 0u) ? efuse_check_blk0_error(loader, base, regs) : efuse_check_rs_error(loader, base, regs, block);
        if (err != ESP_LOADER_SUCCESS) {
            return err;
        }
    }

    /* Read-back verification: all staged bits must be set in the read registers. */
    uint32_t read_base = base + layout->blocks[block].read_offset;
    for (uint8_t w = 0; w < word_count; w++) {
        uint32_t val;
        RETURN_ON_ERROR(esp_loader_read_register(loader, read_base + w * EFUSE_WORD_BYTES, &val));
        if ((val & buf[w]) != buf[w]) {
            return ESP_LOADER_ERROR_EFUSE_BURN_FAILED;
        }
    }

    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t efuse_burn_block(esp_loader_t *loader,
        const esp_loader_efuse_ctx_t *ctx,
        const efuse_chip_layout_t *layout,
        uint32_t base, uint8_t block, bool verify)
{
    esp_loader_error_t err = ESP_LOADER_ERROR_EFUSE_BURN_FAILED;
    for (uint8_t attempt = 0; attempt < BURN_MAX_RETRIES; attempt++) {
        err = burn_block_once(loader, ctx, layout, base, block, verify);
        if (err == ESP_LOADER_SUCCESS) {
            return ESP_LOADER_SUCCESS;
        }
    }
    return err;
}

/* ── ESP32 legacy burn path ────────────────────────────────────────────────── */

/*
 * ESP32 has a fundamentally different register model from modern chips:
 *  - each block has its own absolute write register address (no shared
 *    PGM_DATA + block-index scheme)
 *  - the coding scheme (NONE or 3/4) is detected at runtime from BLOCK0,
 *    not fixed per chip
 *  - error detection is a single EFUSE_REG_DEC_STATUS register shared by
 *    all blocks, not per-block RS_ERR/REPEAT_ERR
 * Crystal frequency is assumed to be 40 MHz, same simplification already
 * applied to S2 — no runtime detection.
 */

#define ESP32_CONF_OFF            0x0FCu
#define ESP32_CMD_OFF             0x104u
#define ESP32_DEC_STATUS_OFF      0x11Cu
#define ESP32_DEC_STATUS_MASK     0xFFFu
#define ESP32_DAC_CONF_OFF        0x118u
#define ESP32_CLK_OFF             0x0F8u
#define ESP32_CODING_SCHEME_WORD  6u    /* word index within BLOCK0 holding the coding-scheme bits */
#define ESP32_CODING_SCHEME_MASK  0x3u
/* ESP32_CODING_SCHEME_NONE (== 0) is declared in esp_loader_efuse_private.h,
 * shared with efuse_get_block_coding(). */
#define ESP32_CODING_34           1u
#define ESP32_CODING_NONE_RECOVERY 3u

#define ESP32_WRITE_OP  0x5A5Au
#define ESP32_READ_OP   0x5AA5u
#define ESP32_CMD_WRITE 0x2u
#define ESP32_CMD_READ  0x1u

/* 40 MHz clock-select values (clk_sel0, clk_sel1, dac_clk_div) — see
 * EFUSE_CLK_SETTINGS in espefuse's esp32/mem_definition.py. 26/80 MHz entries
 * intentionally omitted; this library assumes a 40 MHz crystal on ESP32. */
#define ESP32_CLK_SEL0     160u
#define ESP32_CLK_SEL1     255u
#define ESP32_DAC_CLK_DIV  80u

/* Per-block absolute write register offsets. Read offsets/word counts come
 * from esp32_layout->blocks (shared with the dump/read path). */
static const uint16_t esp32_write_off[4] = {
    0x01Cu, /* BLOCK0 */
    0x098u, /* BLOCK1 */
    0x0B8u, /* BLOCK2 */
    0x0D8u, /* BLOCK3 */
};

esp_loader_error_t esp32_read_coding_scheme(esp_loader_t *loader, uint32_t base,
        const efuse_chip_layout_t *layout, uint8_t *scheme)
{
    uint32_t word_addr = base + layout->blocks[0].read_offset + ESP32_CODING_SCHEME_WORD * EFUSE_WORD_BYTES;
    uint32_t val;
    RETURN_ON_ERROR(esp_loader_read_register(loader, word_addr, &val));
    uint8_t raw = (uint8_t)(val & ESP32_CODING_SCHEME_MASK);
    *scheme = (raw == ESP32_CODING_NONE_RECOVERY) ? ESP32_CODING_SCHEME_NONE : raw;
    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t esp32_efuse_read(esp_loader_t *loader, uint32_t base)
{
    uint32_t conf_addr = base + ESP32_CONF_OFF;
    uint32_t cmd_addr  = base + ESP32_CMD_OFF;
    uint32_t cmd_mask  = ESP32_CMD_WRITE | ESP32_CMD_READ;

    RETURN_ON_ERROR(efuse_wait_idle(loader, cmd_addr, cmd_mask));
    RETURN_ON_ERROR(esp_loader_write_register(loader, conf_addr, ESP32_READ_OP));
    RETURN_ON_ERROR(esp_loader_write_register(loader, cmd_addr, ESP32_CMD_READ));
    return efuse_wait_idle(loader, cmd_addr, cmd_mask);
}

static esp_loader_error_t esp32_check_error(esp_loader_t *loader, uint32_t base, uint8_t block)
{
    if (block == 0u) {
        return ESP_LOADER_SUCCESS;  /* DEC_STATUS is meaningless for BLOCK0 (always CODING_SCHEME_NONE) */
    }
    uint32_t val;
    RETURN_ON_ERROR(esp_loader_read_register(loader, base + ESP32_DEC_STATUS_OFF, &val));
    if ((val & ESP32_DEC_STATUS_MASK) != 0u) {
        return ESP_LOADER_ERROR_EFUSE_BURN_FAILED;
    }
    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t burn_block_once_esp32(esp_loader_t *loader,
        const esp_loader_efuse_ctx_t *ctx,
        const efuse_chip_layout_t *layout,
        uint32_t base, uint8_t block, uint8_t coding_scheme)
{
    uint8_t word_count  = layout->blocks[block].word_count;
    const uint32_t *buf = &ctx->_buf[block * ESP_LOADER_EFUSE_MAX_WORDS_PER_BLOCK];

    uint32_t encoded[8];
    const uint32_t *to_write = buf;
    uint8_t write_words      = word_count;

    if (block > 0u && coding_scheme == ESP32_CODING_34) {
        /* 3/4 scheme: 6 staged words (24 bytes) of real data encode into 8
         * write words (32 bytes). */
        efuse_coding34_encode(buf, encoded);
        to_write    = encoded;
        write_words = 8u;
    }

    /* Write data words first, then configure the clock — matches espefuse's
     * order exactly: ESP32's efuse_controller_setup() is a no-op, so the data
     * write loop in burn_words() runs before write_efuses() (which is where
     * the clock config actually happens), redone before every attempt. */
    uint32_t write_addr = base + esp32_write_off[block];
    for (uint8_t w = 0; w < write_words; w++) {
        RETURN_ON_ERROR(esp_loader_write_register(loader, write_addr + w * EFUSE_WORD_BYTES, to_write[w]));
    }

    RETURN_ON_ERROR(update_reg(loader, base + ESP32_DAC_CONF_OFF, 0xFFu, ESP32_DAC_CLK_DIV));
    RETURN_ON_ERROR(update_reg(loader, base + ESP32_CLK_OFF, 0x00FFu, ESP32_CLK_SEL0));
    RETURN_ON_ERROR(update_reg(loader, base + ESP32_CLK_OFF, 0xFF00u, ESP32_CLK_SEL1 << 8u));

    RETURN_ON_ERROR(esp_loader_write_register(loader, base + ESP32_CONF_OFF, ESP32_WRITE_OP));
    RETURN_ON_ERROR(esp_loader_write_register(loader, base + ESP32_CMD_OFF, ESP32_CMD_WRITE));

    for (uint8_t r = 0; r < READ_VERIFY_PASSES; r++) {
        RETURN_ON_ERROR(esp32_efuse_read(loader, base));
        esp_loader_error_t err = esp32_check_error(loader, base, block);
        if (err != ESP_LOADER_SUCCESS) {
            return err;
        }
    }

    /* Read-back verification: hardware decodes 3/4-coded blocks transparently
     * on read, exposing only the original 6 words (24 bytes) at read_offset —
     * not the 8 encoded write words. So this always compares against the
     * original staged buf, never the encoded form, same as the modern path
     * never compares against RS-encoded parity on read-back. */
    uint8_t verify_words = (block > 0u && coding_scheme == ESP32_CODING_34) ? 6u : word_count;
    uint32_t read_addr = base + layout->blocks[block].read_offset;
    for (uint8_t w = 0; w < verify_words; w++) {
        uint32_t val;
        RETURN_ON_ERROR(esp_loader_read_register(loader, read_addr + w * EFUSE_WORD_BYTES, &val));
        if ((val & buf[w]) != buf[w]) {
            return ESP_LOADER_ERROR_EFUSE_BURN_FAILED;
        }
    }

    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t efuse_burn_block_esp32(esp_loader_t *loader,
        const esp_loader_efuse_ctx_t *ctx,
        const efuse_chip_layout_t *layout,
        uint32_t base, uint8_t block, uint8_t coding_scheme)
{
    /* See efuse_burn_block(): keep the real error instead of flattening it. */
    esp_loader_error_t err = ESP_LOADER_ERROR_EFUSE_BURN_FAILED;
    for (uint8_t attempt = 0; attempt < BURN_MAX_RETRIES; attempt++) {
        err = burn_block_once_esp32(loader, ctx, layout, base, block, coding_scheme);
        if (err == ESP_LOADER_SUCCESS) {
            return ESP_LOADER_SUCCESS;
        }
    }
    return err;
}

/*
 * Burn BLK0 in up to two passes: non-protect bits first, then WR_DIS / RD_DIS.
 * Remaining-work staging: after a verified pass, drop those bits from ctx.
 * Pass-1 failure restores the full saved BLK0 (including protect bits).
 * Pass-2 failure leaves protect-only staging for retry.
 */
static esp_loader_error_t efuse_burn_blk0_with_postpone(esp_loader_t *loader,
        esp_loader_efuse_ctx_t *ctx,
        const efuse_chip_layout_t *layout,
        uint32_t base, bool esp32_legacy, uint8_t esp32_coding_scheme)
{
    uint8_t word_count = layout->blocks[0].word_count;
    uint32_t *buf = &ctx->_buf[0];

    if (!block_has_staged(buf, word_count)) {
        return ESP_LOADER_SUCCESS;
    }

    /* Always postpone WR_DIS / RD_DIS: strip them for pass 1, burn them alone
     * in pass 2. Empty passes are skipped (nothing staged). */
    uint32_t saved[ESP_LOADER_EFUSE_MAX_WORDS_PER_BLOCK];
    memcpy(saved, buf, word_count * EFUSE_WORD_BYTES);

    /* Fuses that kill the download transport ride along in the final pass, so
     * they are programmed by the same PGM as the protect bits and cannot strand
     * them. Their presence also means that pass cannot be verified. */
    efuse_bit_range_t dis_dl  = one_bit(layout->dis_download_mode_bit);
    efuse_bit_range_t sec_dl  = one_bit(layout->enable_security_download_bit);
    bool kills_transport = bit_is_staged(saved, word_count, layout->dis_download_mode_bit) ||
                           bit_is_staged(saved, word_count, layout->enable_security_download_bit);

    clear_bit_range(buf, word_count, &layout->wr_dis);
    clear_bit_range(buf, word_count, &layout->rd_dis);
    if (layout->dis_download_mode_bit != ESP_LOADER_EFUSE_BIT_NONE) {
        clear_bit_range(buf, word_count, &dis_dl);
    }
    if (layout->enable_security_download_bit != ESP_LOADER_EFUSE_BIT_NONE) {
        clear_bit_range(buf, word_count, &sec_dl);
    }

    if (block_has_staged(buf, word_count)) {
        esp_loader_error_t err;
        if (esp32_legacy) {
            err = efuse_burn_block_esp32(loader, ctx, layout, base, 0u, esp32_coding_scheme);
        } else {
            err = efuse_burn_block(loader, ctx, layout, base, 0u, true);
        }
        if (err != ESP_LOADER_SUCCESS) {
            memcpy(buf, saved, word_count * EFUSE_WORD_BYTES);
            return err;
        }
    }

    /* Pass 1 done (or was empty): staging becomes protect bits only. */
    memset(buf, 0, word_count * EFUSE_WORD_BYTES);
    copy_bit_range(buf, saved, word_count, &layout->wr_dis);
    copy_bit_range(buf, saved, word_count, &layout->rd_dis);
    if (layout->dis_download_mode_bit != ESP_LOADER_EFUSE_BIT_NONE) {
        copy_bit_range(buf, saved, word_count, &dis_dl);
    }
    if (layout->enable_security_download_bit != ESP_LOADER_EFUSE_BIT_NONE) {
        copy_bit_range(buf, saved, word_count, &sec_dl);
    }

    if (block_has_staged(buf, word_count)) {
        esp_loader_error_t err;
        if (esp32_legacy) {
            err = efuse_burn_block_esp32(loader, ctx, layout, base, 0u,
                                         esp32_coding_scheme);
        } else {
            err = efuse_burn_block(loader, ctx, layout, base, 0u, !kills_transport);
        }
        if (err != ESP_LOADER_SUCCESS) {
            return err;
        }
    }

    clear_staged_block(ctx, 0u);
    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t efuse_burn_esp32(esp_loader_t *loader, esp_loader_efuse_ctx_t *ctx)
{
    const efuse_chip_layout_t *layout = efuse_get_chip_layout(ESP32_CHIP);
    if (layout == NULL) {
        return ESP_LOADER_ERROR_UNSUPPORTED_FUNC;
    }

    RETURN_ON_ERROR(efuse_validate_staged_writes(loader, ctx));

    uint32_t base = esp_targets_get_efuse_base(ESP32_CHIP);

    uint8_t coding_scheme;
    RETURN_ON_ERROR(esp32_read_coding_scheme(loader, base, layout, &coding_scheme));

    /* Burn BLK3..BLK1 in reverse order, then BLK0 last (with protect postpone).
     * Clear each block's staging only after a verified-successful burn. */
    for (uint8_t block = layout->block_count - 1; block > 0; block--) {
        uint8_t word_count = layout->blocks[block].word_count;
        const uint32_t *buf = &ctx->_buf[block * ESP_LOADER_EFUSE_MAX_WORDS_PER_BLOCK];

        if (!block_has_staged(buf, word_count)) {
            continue;
        }

        RETURN_ON_ERROR(efuse_burn_block_esp32(loader, ctx, layout, base, block, coding_scheme));
        clear_staged_block(ctx, block);
    }

    RETURN_ON_ERROR(efuse_burn_blk0_with_postpone(loader, ctx, layout, base, true,
                    ESP32_CODING_SCHEME_NONE));

    return ESP_LOADER_SUCCESS;
}

/* ── Public commit API ──────────────────────────────────────────────────────── */

/*
 * commit() remaining-work staging: after each verified-successful burn unit
 * (BLK1+ block, or BLK0 pass), that unit is cleared from ctx. On failure,
 * ctx retains only what still needs burning (retry-friendly).
 *
 * BLK0 protect postpone (espefuse parity for WR_DIS / RD_DIS ranges):
 *   pass 1 — burn BLK0 with the postponed bits cleared from the staging buffer
 *   pass 2 — burn only those postponed bits
 * Postponed are WR_DIS / RD_DIS plus DIS_DOWNLOAD_MODE and
 * ENABLE_SECURITY_DOWNLOAD, which end the download session that pass 1 needs.
 * So a failed first pass leaves protect bits unset. Fuses espefuse postpones
 * for other reasons (SECURE_BOOT_EN, SPI_BOOT_CRYPT_CNT) are not split here;
 * prefer a second commit for those after verifying data.
 */

esp_loader_error_t esp_loader_efuse_commit(esp_loader_t *loader, esp_loader_efuse_ctx_t *ctx)
{
    if (loader->_target == ESP32_CHIP) {
        return efuse_burn_esp32(loader, ctx);
    }

    const efuse_chip_layout_t *layout = efuse_get_chip_layout(loader->_target);
    if (layout == NULL || layout->burn == NULL) {
        return ESP_LOADER_ERROR_UNSUPPORTED_FUNC;
    }

    RETURN_ON_ERROR(efuse_validate_staged_writes(loader, ctx));

    uint32_t base = esp_targets_get_efuse_base(loader->_target);

    /* Burn BLK10..BLK1 in reverse order, then BLK0 last (with protect postpone).
     * Clear each block's staging only after a verified-successful burn. */
    for (uint8_t block = layout->block_count - 1; block > 0; block--) {
        uint8_t word_count = layout->blocks[block].word_count;
        const uint32_t *buf = &ctx->_buf[block * ESP_LOADER_EFUSE_MAX_WORDS_PER_BLOCK];

        if (!block_has_staged(buf, word_count)) {
            continue;
        }

        RETURN_ON_ERROR(efuse_burn_block(loader, ctx, layout, base, block, true));
        clear_staged_block(ctx, block);
    }

    RETURN_ON_ERROR(efuse_burn_blk0_with_postpone(loader, ctx, layout, base, false, 0u));

    return ESP_LOADER_SUCCESS;
}
