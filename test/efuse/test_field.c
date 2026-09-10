/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Behavioral tests for esp_loader_efuse_write_field() / read_field(), run
 * through the public API against the in-memory fake (write_field only stages
 * into ctx; read_field is what needs the fake's register seam). Descriptors are
 * synthesized against each chip's real BLK0 rather than any generated named
 * field, so these stay pure bit-packing tests — the field tables themselves are
 * test_fields.c's job. Sweeping the chips exercises every block-0 word_count
 * the layouts define (2 on C2, 6 on modern chips, 7 on ESP32).
 */

#include "test_util.h"
#include "fake_efuse.h"
#include "esp_loader_efuse_private.h"
#include "efuse_test_bits.h"

TEST(write_field_stages_bits_mid_word)
{
    for (target_chip_t chip = 0; chip < ESP_MAX_CHIP; chip++) {
        const efuse_chip_layout_t *L = efuse_get_chip_layout(chip);
        if (L == NULL) {
            continue;
        }

        esp_loader_t loader = fake_loader(chip);
        esp_loader_efuse_ctx_t ctx = { 0 };

        /* Not word- or byte-aligned: bit_start=5 forces the LSB-first packing
         * path within a single word. */
        const esp_loader_efuse_desc_t desc = { .efuse_block = 0, .bit_start = 5, .bit_count = 4 };
        const esp_loader_efuse_desc_t *field[] = { &desc, NULL };
        uint8_t src = 0x0Bu; /* 0b1011 */

        esp_loader_error_t err = esp_loader_efuse_write_field(&loader, &ctx, field, &src, 4u);
        CHECK_EQ(err, ESP_LOADER_SUCCESS, "chip=%d: write_field failed", (int)chip);

        const uint32_t *buf = efuse_get_write_buf(&ctx, 0);
        CHECK_EQ(staged_field(buf, 5, 4), src,
                 "chip=%d: bits [5:9) not staged", (int)chip);
        /* Neighbouring bits must stay untouched. */
        CHECK_EQ(staged_bit(buf, 4), 0u, "chip=%d: bit 4 below the field was written",
                 (int)chip);
        CHECK_EQ(staged_bit(buf, 9), 0u, "chip=%d: bit 9 above the field was written",
                 (int)chip);
    }
    return true;
}

TEST(read_field_extracts_seeded_bits_crossing_word)
{
    for (target_chip_t chip = 0; chip < ESP_MAX_CHIP; chip++) {
        const efuse_chip_layout_t *L = efuse_get_chip_layout(chip);
        if (L == NULL || L->blocks[0].word_count < 2u) {
            continue;
        }

        esp_loader_t loader = fake_loader(chip);
        fake_efuse_reset();
        fake_set_efuse_word(chip, 0, 0, 0xC0000000u); /* top 2 bits of word0 set   */
        fake_set_efuse_word(chip, 0, 1, 0x0000000Fu); /* bottom 4 bits of word1 set */

        /* bit_start=30, count=6 spans word0 bits[30:32) then word1 bits[0:4). */
        const esp_loader_efuse_desc_t desc = { .efuse_block = 0, .bit_start = 30, .bit_count = 6 };
        const esp_loader_efuse_desc_t *field[] = { &desc, NULL };
        uint8_t dst = 0;

        esp_loader_error_t err = esp_loader_efuse_read_field(&loader, field, &dst, 6u);
        CHECK_EQ(err, ESP_LOADER_SUCCESS, "chip=%d: read_field failed", (int)chip);
        CHECK_EQ(dst, 0x3Fu, "chip=%d: field spanning the word boundary did not "
                 "read back as all 6 seeded bits", (int)chip);
    }
    return true;
}

TEST(write_then_read_field_round_trip)
{
    for (target_chip_t chip = 0; chip < ESP_MAX_CHIP; chip++) {
        const efuse_chip_layout_t *L = efuse_get_chip_layout(chip);
        if (L == NULL || L->blocks[0].word_count < 2u) {
            continue;
        }

        esp_loader_t loader = fake_loader(chip);
        esp_loader_efuse_ctx_t ctx = { 0 };
        fake_efuse_reset();

        /* Two byte-sized ranges, neither word-aligned nor in the same word:
         * exercises staging + read-back together across a word boundary. */
        const esp_loader_efuse_desc_t desc0 = { .efuse_block = 0, .bit_start = 5,  .bit_count = 8 };
        const esp_loader_efuse_desc_t desc1 = { .efuse_block = 0, .bit_start = 40, .bit_count = 8 };
        const esp_loader_efuse_desc_t *field[] = { &desc0, &desc1, NULL };
        uint8_t src[2] = { 0xA5u, 0x3Cu };

        esp_loader_error_t err = esp_loader_efuse_write_field(&loader, &ctx, field, src, 16u);
        CHECK_EQ(err, ESP_LOADER_SUCCESS, "chip=%d: write_field failed", (int)chip);

        /* Model a burn: copy every staged word of BLK0 onto the fake "chip". */
        const uint32_t *buf = efuse_get_write_buf(&ctx, 0);
        for (uint8_t w = 0; w < L->blocks[0].word_count; w++) {
            fake_set_efuse_word(chip, 0, w, buf[w]);
        }

        uint8_t dst[2] = { 0 };
        err = esp_loader_efuse_read_field(&loader, field, dst, 16u);
        CHECK_EQ(err, ESP_LOADER_SUCCESS, "chip=%d: read_field failed", (int)chip);
        CHECK_MEM_EQ(dst, src, sizeof(src),
                     "chip=%d: round trip did not return the written bytes", (int)chip);
    }
    return true;
}

TEST(field_out_of_range_returns_invalid_param)
{
    for (target_chip_t chip = 0; chip < ESP_MAX_CHIP; chip++) {
        const efuse_chip_layout_t *L = efuse_get_chip_layout(chip);
        if (L == NULL) {
            continue;
        }

        esp_loader_t loader = fake_loader(chip);
        esp_loader_efuse_ctx_t ctx = { 0 };

        /* Block index past block_count. */
        const esp_loader_efuse_desc_t bad_block = {
            .efuse_block = L->block_count, .bit_start = 0, .bit_count = 1
        };
        const esp_loader_efuse_desc_t *bad_block_field[] = { &bad_block, NULL };
        uint8_t buf8 = 0;
        CHECK_EQ(esp_loader_efuse_write_field(&loader, &ctx, bad_block_field, &buf8, 1u),
                 ESP_LOADER_ERROR_INVALID_PARAM,
                 "chip=%d: write_field accepted block %u, past the last block",
                 (int)chip, L->block_count);
        CHECK_EQ(esp_loader_efuse_read_field(&loader, bad_block_field, &buf8, 1u),
                 ESP_LOADER_ERROR_INVALID_PARAM,
                 "chip=%d: read_field accepted block %u, past the last block",
                 (int)chip, L->block_count);

        /* Valid block, but a bit position past its word_count. */
        const esp_loader_efuse_desc_t bad_word = {
            .efuse_block = 0,
            .bit_start = (uint16_t)(L->blocks[0].word_count * WORD_BITS),
            .bit_count = 1
        };
        const esp_loader_efuse_desc_t *bad_word_field[] = { &bad_word, NULL };
        CHECK_EQ(esp_loader_efuse_write_field(&loader, &ctx, bad_word_field, &buf8, 1u),
                 ESP_LOADER_ERROR_INVALID_PARAM,
                 "chip=%d: write_field accepted a bit past the end of BLK0", (int)chip);
        CHECK_EQ(esp_loader_efuse_read_field(&loader, bad_word_field, &buf8, 1u),
                 ESP_LOADER_ERROR_INVALID_PARAM,
                 "chip=%d: read_field accepted a bit past the end of BLK0", (int)chip);
    }
    return true;
}

TEST_MAIN()
