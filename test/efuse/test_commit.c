/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * esp_loader_efuse_commit() against the fake with device response enabled — the
 * only tests that issue real PGM_CMD/READ_CMD writes and drive the
 * poll-and-verify/retry loop in esp_loader_efuse_burn.c. Staging belongs to
 * test_keys.c / test_field.c; here ctx is poked directly rather than through
 * write_field/write_key, keeping these focused on burn/commit control flow:
 * order, the BLK0 postpone split, and retry. Modern chips only — see the note
 * above TEST_MAIN() for ESP32 legacy.
 */

#include "test_util.h"
#include "fake_efuse.h"
#include "esp_loader_efuse_private.h"
#include "esp_targets.h"

/* The value stage_block() puts in `block` word `word` — 0xFFFF0102 is block 1,
 * word 2. The marker keeps it non-zero for every block/word pair, which
 * block_drained() relies on to tell "commit consumed this" from "commit left it
 * alone"; the block/word bytes make a wrong-slot mismatch legible. */
static uint32_t staged_word(uint8_t block, uint8_t word)
{
    return 0xFFFF0000u | ((uint32_t)block << 8) | word;
}

/* Fill `block`'s staging as if write_field/write_key had run (test_field.c and
 * test_keys.c cover those). Distinct values make final-device mismatches name
 * the wrong block/word directly. */
static void stage_block(esp_loader_efuse_ctx_t *ctx, uint8_t block, uint8_t word_count)
{
    for (uint8_t w = 0; w < word_count; w++) {
        ctx->_buf[block * ESP_LOADER_EFUSE_MAX_WORDS_PER_BLOCK + w] = staged_word(block, w);
    }
}

/* True iff `block`'s staging is back to all-zero — i.e. commit() consumed it.
 * A successful commit must drain staging; a failed one must not. */
static bool block_drained(const esp_loader_efuse_ctx_t *ctx, uint8_t block, uint8_t word_count)
{
    const uint32_t *buf = efuse_get_write_buf(ctx, block);
    for (uint8_t w = 0; w < word_count; w++) {
        if (buf[w] != 0u) {
            return false;
        }
    }
    return true;
}

/* "When (if ever) did commit() burn `block`?" Finds the PGM_CMD write targeting
 * this block and returns its position in the transcript, so ordering questions
 * ("did BLK2 burn before BLK1?") reduce to comparing two positions. */
static bool block_burn_position(const efuse_burn_regs_t *regs, uint32_t base, uint8_t block, uint32_t *out_position)
{
    uint32_t cmd_addr = base + regs->cmd_off;
    const fake_reg_write_t *w = fake_writes();
    uint32_t n = fake_write_count();
    for (uint32_t i = 0; i < n; i++) {
        if (w[i].addr == cmd_addr && (w[i].value & regs->ctrl->pgm_cmd) &&
                (uint8_t)(w[i].value >> regs->ctrl->cmd_block_shift) == block) {
            *out_position = i;
            return true;
        }
    }
    return false;
}

/* Every PGM_CMD hit for `block`, not just the first — BLK0 burns in two passes
 * and the test inspects each. Always scans the whole transcript and returns the
 * true total, even past `max_out`, which only caps *storage*: capping the scan
 * would truncate an unexpected extra burn back into the expected count. */
static uint32_t block_burn_positions(const efuse_burn_regs_t *regs, uint32_t base, uint8_t block,
                                     uint32_t *out, uint32_t max_out)
{
    uint32_t cmd_addr = base + regs->cmd_off;
    const fake_reg_write_t *w = fake_writes();
    uint32_t n = fake_write_count(), count = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (w[i].addr == cmd_addr && (w[i].value & regs->ctrl->pgm_cmd) &&
                (uint8_t)(w[i].value >> regs->ctrl->cmd_block_shift) == block) {
            if (count < max_out) {
                out[count] = i;
            }
            count++;
        }
    }
    return count;
}

/* What did this burn pass actually send? Walks the transcript back from
 * `before_position` to the most recent PGM_DATA write for `word_index`, since
 * pass ordering alone doesn't prove what a pass contained.
 *
 * False means the pass never wrote that word — a distinct failure from writing
 * the wrong value, and one no in-band sentinel can express, since every 32-bit
 * value is a legitimate thing to stage. */
static bool word_staged_before(const efuse_burn_regs_t *regs, uint32_t base, uint32_t before_position,
                               uint8_t word_index, uint32_t *out_value)
{
    uint32_t addr = base + regs->pgm->pgm_data0_off + word_index * EFUSE_WORD_BYTES;
    const fake_reg_write_t *w = fake_writes();
    for (uint32_t i = before_position; i-- > 0;) {
        if (w[i].addr == addr) {
            *out_value = w[i].value;
            return true;
        }
    }
    return false; /* the word was never staged for this pass */
}

/* The part of BLK0 bit range `r` that falls inside `word`. Written out here
 * rather than reusing the production clear_bit_range/copy_bit_range it checks. */
static uint32_t word_protect_mask(efuse_bit_range_t r, uint8_t word)
{
    uint32_t mask = 0;
    for (uint16_t bit = r.bit_start; bit < (uint16_t)(r.bit_start + r.bit_count); bit++) {
        if (bit / 32u == word) {
            mask |= 1u << (bit % 32u);
        }
    }
    return mask;
}

/* Everything commit() defers to the final BLK0 pass: the WR_DIS / RD_DIS ranges
 * (burning those in pass 1 would write-protect the rest of BLK0 before it is
 * burned), plus the fuses that end the download transport (those must share the
 * protect bits' PGM so a dead transport cannot strand them). Derived from the
 * layout, so a chip whose positions differ is covered without touching this
 * test. */
static uint32_t blk0_postponed_mask(const efuse_chip_layout_t *L, uint8_t word)
{
    uint32_t mask = word_protect_mask(L->wr_dis, word) | word_protect_mask(L->rd_dis, word);

    const uint16_t late_bits[] = { L->dis_download_mode_bit, L->enable_security_download_bit };
    for (size_t i = 0; i < sizeof(late_bits) / sizeof(late_bits[0]); i++) {
        if (late_bits[i] != ESP_LOADER_EFUSE_BIT_NONE && (late_bits[i] / 32u) == word) {
            mask |= 1u << (late_bits[i] % 32u);
        }
    }
    return mask;
}

TEST(commit_burns_blocks_in_reverse_order)
{
    for (target_chip_t chip = 0; chip < ESP_MAX_CHIP; chip++) {
        const efuse_chip_layout_t *L = efuse_get_chip_layout(chip);
        if (L == NULL || L->burn == NULL) {
            continue;
        }
        uint32_t base = esp_targets_get_efuse_base(chip);

        fake_efuse_reset_for_burn(chip);

        esp_loader_t loader = fake_loader(chip);
        esp_loader_efuse_ctx_t ctx = { 0 };

        /* Every block but BLK0, whose postpone behaviour is covered separately. */
        for (uint8_t block = 1; block < L->block_count; block++) {
            stage_block(&ctx, block, L->blocks[block].word_count);
        }

        esp_loader_error_t err = esp_loader_efuse_commit(&loader, &ctx);
        CHECK_EQ(err, ESP_LOADER_SUCCESS, "chip=%d: commit() failed", (int)chip);

        uint32_t positions[ESP_LOADER_EFUSE_MAX_BLOCKS] = { 0 };
        for (uint8_t block = 1; block < L->block_count; block++) {
            CHECK(block_burn_position(L->burn, base, block, &positions[block]),
                  "chip=%d: staged block=%u was never burned", (int)chip, block);
        }
        for (uint8_t block = 2; block < L->block_count; block++) {
            /* higher block burns first */
            CHECK(positions[block] < positions[block - 1],
                  "chip=%d: blocks burned out of order — block=%u burns at write %u, "
                  "after block=%u at write %u", (int)chip, block, positions[block],
                  block - 1, positions[block - 1]);
        }
        for (uint8_t block = 1; block < L->block_count; block++) {
            CHECK(block_drained(&ctx, block, L->blocks[block].word_count),
                  "chip=%d: block=%u still staged after a successful commit",
                  (int)chip, block);
            for (uint8_t w = 0; w < L->blocks[block].word_count; w++) {
                CHECK_EQ(fake_get_efuse_word(chip, block, w), staged_word(block, w),
                         "chip=%d: block=%u word=%u did not reach the device image",
                         (int)chip, block, w);
            }
        }
    }
    return true;
}

TEST(commit_postpones_blk0_protect_and_transport_bits_to_second_pass)
{
    for (target_chip_t chip = 0; chip < ESP_MAX_CHIP; chip++) {
        const efuse_chip_layout_t *L = efuse_get_chip_layout(chip);
        if (L == NULL || L->burn == NULL) {
            continue;
        }
        uint32_t base = esp_targets_get_efuse_base(chip);

        fake_efuse_reset_for_burn(chip);

        esp_loader_t loader = fake_loader(chip);
        esp_loader_efuse_ctx_t ctx = { 0 };

        /* Fill BLK0 solid so both ordinary data and protect bits are staged in
         * every word, with no representative bit to hand-pick. The expectations
         * below derive from `staged`, so the two can't drift apart. */
        const uint32_t staged = 0xFFFFFFFFu;
        uint8_t word_count = L->blocks[0].word_count;
        for (uint8_t w = 0; w < word_count; w++) {
            ctx._buf[w] = staged;
        }

        esp_loader_error_t err = esp_loader_efuse_commit(&loader, &ctx);
        CHECK_EQ(err, ESP_LOADER_SUCCESS, "chip=%d: commit() failed", (int)chip);

        /* Two passes: data (with the postponed bits stripped) then those alone. */
        uint32_t positions[2];
        uint32_t n = block_burn_positions(L->burn, base, 0, positions, 2);
        CHECK_EQ(n, 2u, "chip=%d did not split BLK0 into a data pass and a "
                 "protect-bit pass", (int)chip);

        const uint32_t data_pass    = positions[0];
        const uint32_t protect_pass = positions[1];

        for (uint8_t w = 0; w < word_count; w++) {
            uint32_t postponed = blk0_postponed_mask(L, w);
            uint32_t sent = 0;

            CHECK(word_staged_before(L->burn, base, data_pass, w, &sent),
                  "chip=%d BLK0 word=%u: pass 1 never staged it", (int)chip, w);
            CHECK_EQ(sent, staged & ~postponed,
                     "chip=%d BLK0 word=%u: pass 1 must burn the data with the "
                     "protect and transport-disabling bits stripped", (int)chip, w);

            CHECK(word_staged_before(L->burn, base, protect_pass, w, &sent),
                  "chip=%d BLK0 word=%u: pass 2 never staged it", (int)chip, w);
            CHECK_EQ(sent, postponed,
                     "chip=%d BLK0 word=%u: pass 2 must burn the postponed bits alone",
                     (int)chip, w);
            CHECK_EQ(fake_get_efuse_word(chip, 0, w), staged,
                     "chip=%d BLK0 word=%u: the split burn lost staged data or "
                     "protection bits", (int)chip, w);
        }
        CHECK(block_drained(&ctx, 0, word_count),
              "chip=%d: BLK0 still staged after a successful commit", (int)chip);
    }
    return true;
}

TEST(commit_retries_and_recovers_within_budget)
{
    /* Block 1 exists on every chip here (the smallest, ESP32-C2, has 4). */
    for (target_chip_t chip = 0; chip < ESP_MAX_CHIP; chip++) {
        const efuse_chip_layout_t *L = efuse_get_chip_layout(chip);
        if (L == NULL || L->burn == NULL) {
            continue;
        }

        fake_efuse_reset_for_burn(chip);
        fake_inject_error(1, 2); /* fail block 1's first 2 attempts, succeed on the 3rd */

        esp_loader_t loader = fake_loader(chip);
        esp_loader_efuse_ctx_t ctx = { 0 };
        stage_block(&ctx, 1, L->blocks[1].word_count);

        esp_loader_error_t err = esp_loader_efuse_commit(&loader, &ctx);
        CHECK_EQ(err, ESP_LOADER_SUCCESS, "chip=%d: commit() failed", (int)chip);
        CHECK(block_drained(&ctx, 1, L->blocks[1].word_count),
              "chip=%d: block=1 still staged after a successful retry", (int)chip);
        for (uint8_t w = 0; w < L->blocks[1].word_count; w++) {
            CHECK_EQ(fake_get_efuse_word(chip, 1, w), staged_word(1, w),
                     "chip=%d: block=1 word=%u did not survive retries",
                     (int)chip, w);
        }
    }
    return true;
}

TEST(commit_fails_after_exhausting_retries_and_keeps_staging)
{
    for (target_chip_t chip = 0; chip < ESP_MAX_CHIP; chip++) {
        const efuse_chip_layout_t *L = efuse_get_chip_layout(chip);
        if (L == NULL || L->burn == NULL) {
            continue;
        }

        fake_efuse_reset_for_burn(chip);
        /* 3 == BURN_MAX_RETRIES, private to esp_loader_efuse_burn.c: exhausts the
         * budget, so commit() must fail without draining the block. */
        fake_inject_error(1, 3);

        esp_loader_t loader = fake_loader(chip);
        esp_loader_efuse_ctx_t ctx = { 0 };
        uint8_t word_count = L->blocks[1].word_count;
        stage_block(&ctx, 1, word_count);

        esp_loader_error_t err = esp_loader_efuse_commit(&loader, &ctx);
        CHECK_EQ(err, ESP_LOADER_ERROR_EFUSE_BURN_FAILED,
                 "chip=%d: commit() did not fail after the retry budget ran out",
                 (int)chip);

        /* Staging must survive a total failure so a caller can retry the commit. */
        const uint32_t *buf = efuse_get_write_buf(&ctx, 1);
        for (uint8_t w = 0; w < word_count; w++) {
            CHECK_EQ(buf[w], staged_word(1, w),
                     "chip=%d: block=1 word=%u lost its staging after a failed "
                     "commit, so the caller cannot retry", (int)chip, w);
            CHECK_EQ(fake_get_efuse_word(chip, 1, w), 0u,
                     "chip=%d: block=1 word=%u changed despite all attempts failing",
                     (int)chip, w);
        }
    }
    return true;
}

/*
 * ESP32 legacy commit path (burn_block_once_esp32 in esp_loader_efuse_burn.c —
 * NONE vs 3/4 coding word counts, read-back against the original 6 words) is
 * not emulated: its register layout is private to that file (no
 * efuse_burn_regs_t), so fake_efuse_reset_for_burn() explicitly rejects it.
 */

TEST_MAIN()
