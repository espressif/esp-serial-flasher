/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Static layout-invariant sweep across every eFuse-capable chip. Pure data
 * checks over the linked efuse_chip_layout_t / key-half tables — no register
 * I/O, no fake, no fields. Cheap breadth: catches a malformed generated table
 * or a hand-written layout whose protection bits drifted out of range, for all
 * chips at once. In particular it enforces the absolute-BLK0 convention: every
 * block/half WR_DIS / RD_DIS bit must fall inside the chip's own WR_DIS / RD_DIS
 * field range (e.g. C2's two BLOCK_KEY0 halves at bits 32/33 within RD_DIS).
 * Also checks each chip's KEY_PURPOSE table is self-consistent (codes fit, a
 * Secure Boot digest is never read-protected, no duplicate purpose tags).
 */

#include "test_util.h"
#include "esp_loader_efuse_private.h"
#include "efuse_test_bits.h"

TEST(chip_layouts_are_self_consistent)
{
    uint32_t halves_seen = 0;

    /* No layout (ESP8266, or a chip not wired for eFuse yet) resolves to NULL. */
    for (target_chip_t t = 0; t < ESP_MAX_CHIP; t++) {
        const efuse_chip_layout_t *L = efuse_get_chip_layout(t);
        if (L == NULL) {
            continue;
        }

        CHECK(L->block_count >= 1, "chip=%d", (int)t);
        CHECK(L->block_count <= ESP_LOADER_EFUSE_MAX_BLOCKS, "chip=%d", (int)t);

        const uint16_t blk0_bits = (uint16_t)(L->blocks[0].word_count * EFUSE_WORD_BYTES * 8u);

        /* WR_DIS / RD_DIS fields live in BLK0 and do not overlap (WR before RD). */
        CHECK(L->wr_dis.bit_start + L->wr_dis.bit_count <= blk0_bits, "chip=%d", (int)t);
        CHECK(L->rd_dis.bit_start + L->rd_dis.bit_count <= blk0_bits, "chip=%d", (int)t);
        CHECK(L->wr_dis.bit_start + L->wr_dis.bit_count <= L->rd_dis.bit_start, "chip=%d", (int)t);

        /* purposes pointer and count agree. */
        CHECK((L->purpose_count > 0) == (L->purposes != NULL), "chip=%d", (int)t);

        /* KEY_PURPOSE geometry, when the chip has one. */
        if (L->key_purpose.purpose_width != 0) {
            CHECK(L->key_purpose.purpose_width == 4 || L->key_purpose.purpose_width == 5,
                  "chip=%d", (int)t);
            CHECK(L->key_purpose.purpose0_bit + L->key_purpose.purpose_width <= blk0_bits,
                  "chip=%d", (int)t);
            CHECK(in_range(L->key_purpose.purpose0_wr_dis_bit, L->wr_dis), "chip=%d", (int)t);
        } else {
            /* no KEY_PURPOSE field => no purpose table */
            CHECK(L->purpose_count == 0, "chip=%d", (int)t);
        }

        /* KEY_PURPOSE table self-consistency. */
        for (uint8_t i = 0; i < L->purpose_count; i++) {
            const efuse_key_purpose_row_t *row = &L->purposes[i];
            CHECK(row->purpose <= ESP_LOADER_KEY_PURPOSE_ECDSA_KEY_P384_H,
                  "chip=%d purpose-row=%u", (int)t, i);
            /* KEY_PURPOSE fields are at most 5 bits */
            CHECK(row->code < 32u, "chip=%d purpose-row=%u", (int)t, i);
            /* digests stay readable */
            CHECK(!(row->is_digest && row->needs_rd_protect),
                  "chip=%d purpose-row=%u", (int)t, i);
            for (uint8_t j = i + 1; j < L->purpose_count; j++) {
                if (row->purpose == L->purposes[j].purpose) {
                    TEST_FAIL("chip=%d: duplicate purpose tag %u", (int)t, row->purpose);
                }
            }
        }

        for (uint8_t b = 0; b < L->block_count; b++) {
            const efuse_blk_layout_t *blk = &L->blocks[b];
            CHECK(blk->word_count >= 1, "chip=%d block=%u", (int)t, b);
            CHECK(blk->word_count <= ESP_LOADER_EFUSE_MAX_WORDS_PER_BLOCK,
                  "chip=%d block=%u", (int)t, b);

            if (blk->wr_dis_bit != ESP_LOADER_EFUSE_BIT_NONE) {
                CHECK(in_range(blk->wr_dis_bit, L->wr_dis), "chip=%d block=%u", (int)t, b);
            }
            if (blk->rd_dis_bit != ESP_LOADER_EFUSE_BIT_NONE) {
                CHECK(in_range(blk->rd_dis_bit, L->rd_dis), "chip=%d block=%u", (int)t, b);
            }

            /* A key block's KEY_PURPOSE_N field and its WR_DIS bit must fit. */
            if (blk->purpose_key_index != EFUSE_INDEX_NONE
                    && L->key_purpose.purpose_width != 0) {
                uint16_t n = blk->purpose_key_index;
                uint16_t hi = (uint16_t)(L->key_purpose.purpose0_bit + (n + 1) * L->key_purpose.purpose_width);
                CHECK(hi <= blk0_bits, "chip=%d block=%u", (int)t, b);
                CHECK(in_range((uint16_t)(L->key_purpose.purpose0_wr_dis_bit + n), L->wr_dis),
                      "chip=%d block=%u", (int)t, b);
            }
        }

        /* Split key-block halves (only C2 today). */
        uint8_t half_count = 0;
        const efuse_key_half_t *halves = efuse_get_key_halves(t, &half_count);
        halves_seen += half_count;
        for (uint8_t h = 0; h < half_count; h++) {
            const efuse_key_half_t *kh = &halves[h];
            CHECK(kh->block < L->block_count, "chip=%d key-half=%u names block %u, "
                  "chip has %u blocks", (int)t, h, kh->block, L->block_count);
            CHECK(kh->word_count >= 1, "chip=%d key-half=%u is empty", (int)t, h);
            CHECK(kh->word_offset + kh->word_count <= L->blocks[kh->block].word_count,
                  "chip=%d key-half=%u spans past the end of block %u",
                  (int)t, h, kh->block);
            /* Halves always carry a real RD_DIS bit, inside the RD_DIS range. */
            CHECK(kh->rd_dis_bit != ESP_LOADER_EFUSE_BIT_NONE, "chip=%d key-half=%u", (int)t, h);
            CHECK(in_range(kh->rd_dis_bit, L->rd_dis), "chip=%d key-half=%u", (int)t, h);
        }
    }

    /* Non-vacuity: reaching C2's two BLOCK_KEY0 halves proves the loop ran,
     * resolved a layout, and exercised the checks (guards against a no-op). */
    CHECK(halves_seen >= 2, "saw %u split key-block halves, want >= 2 (C2's two "
          "BLOCK_KEY0 halves) — the sweep resolved no layout", halves_seen);
    return true;
}

TEST_MAIN()
