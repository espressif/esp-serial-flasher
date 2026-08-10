/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Behavioral tests for esp_loader_efuse_read_block(), run through the public
 * API against the in-memory fake. What is under test is read_protected_word_mask:
 * that it is derived from the live RD_DIS bits in BLK0, whole-block and — on
 * C2 — per half, where only part of a block is masked. Expected values come
 * from the layout, so this adapts per chip; nothing here burns anything.
 */

#include "test_util.h"
#include "fake_efuse.h"
#include "esp_loader_efuse_private.h"

/* A value unique to (block, word), so a read from the wrong register is a
 * mismatch rather than a coincidence. */
static uint32_t seed_value(uint8_t block, uint8_t word)
{
    return 0xB10C0000u | ((uint32_t)block << 8) | word;
}

static void seed_block(target_chip_t chip, const efuse_chip_layout_t *L, uint8_t block)
{
    for (uint8_t w = 0; w < L->blocks[block].word_count; w++) {
        fake_set_efuse_word(chip, block, w, seed_value(block, w));
    }
}

TEST(read_block_flags_every_word_when_the_block_is_rd_dis)
{
    for (target_chip_t chip = 0; chip < ESP_MAX_CHIP; chip++) {
        const efuse_chip_layout_t *L = efuse_get_chip_layout(chip);
        if (L == NULL) {
            continue;
        }
        esp_loader_t loader = fake_loader(chip);

        for (uint8_t b = 1; b < L->block_count; b++) { /* BLK0 is never maskable */
            uint16_t rd_dis = L->blocks[b].rd_dis_bit;
            if (rd_dis == ESP_LOADER_EFUSE_BIT_NONE) {
                continue; /* block has no read protection of its own (C2's halves) */
            }
            fake_efuse_reset();
            fake_set_efuse_bit(chip, 0, rd_dis);

            esp_loader_efuse_block_t out;
            esp_loader_error_t err = esp_loader_efuse_read_block(&loader, b, &out);
            CHECK_EQ(err, ESP_LOADER_SUCCESS, "chip=%u block=%u", chip, b);

            uint16_t all = (uint16_t)((1u << L->blocks[b].word_count) - 1u);
            CHECK_EQ(out.read_protected_word_mask, all,
                     "chip=%u block=%u: RD_DIS bit %u set, every word must be flagged",
                     chip, b, rd_dis);
        }
    }
    return true;
}

TEST(c2_read_block_flags_only_the_rd_dis_half)
{
    /* C2's BLOCK_KEY0 is the only split key block: two 128-bit halves with a
     * RD_DIS bit each, so protecting one must leave the other readable. */
    uint8_t half_count = 0;
    const efuse_key_half_t *halves = efuse_get_key_halves(ESP32C2_CHIP, &half_count);
    CHECK_EQ(half_count, 2u, "C2 must have two BLOCK_KEY0 halves");

    const efuse_chip_layout_t *L = efuse_get_chip_layout(ESP32C2_CHIP);
    esp_loader_t loader = fake_loader(ESP32C2_CHIP);

    for (uint8_t h = 0; h < half_count; h++) {
        const efuse_key_half_t *kh = &halves[h];

        fake_efuse_reset();
        seed_block(ESP32C2_CHIP, L, kh->block);
        fake_set_efuse_bit(ESP32C2_CHIP, 0, kh->rd_dis_bit);

        esp_loader_efuse_block_t out;
        esp_loader_error_t err = esp_loader_efuse_read_block(&loader, kh->block, &out);
        CHECK_EQ(err, ESP_LOADER_SUCCESS, "half=%u", h);

        uint16_t expect = (uint16_t)(((1u << kh->word_count) - 1u) << kh->word_offset);
        CHECK_EQ(out.read_protected_word_mask, expect,
                 "half=%u: RD_DIS bit %u covers words [%u,%u)", h, kh->rd_dis_bit,
                 kh->word_offset, kh->word_offset + kh->word_count);

        /* The words themselves still come back; the flag is the only signal. */
        for (uint8_t w = 0; w < L->blocks[kh->block].word_count; w++) {
            CHECK_EQ(out.words[w], seed_value(kh->block, w), "half=%u word=%u", h, w);
        }
    }
    return true;
}

TEST_MAIN()
