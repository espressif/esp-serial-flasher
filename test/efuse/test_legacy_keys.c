/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Behavioral tests for esp_loader_efuse_write_key_legacy() staging — the path
 * for chips with no KEY_PURPOSE field (ESP32, C2). Like write_key it only
 * stages into ctx, so the assertions are on the staged buffer; nothing burns.
 *
 * Every test here is chip-pinned, because production switches on the target
 * enum to pick the chip and semantics. Layout, key-half, and generated-field
 * data are trusted inputs; the tests verify that the API routes and transforms
 * key material according to them.
 *
 * Reference behaviour is espefuse: esp32/operations.py burn_key +
 * burn_key_digest, esp32c2/operations.py burn_key.
 */

#include <string.h>
#include "test_util.h"
#include "fake_efuse.h"
#include "esp_loader_efuse_private.h"
#include "efuse_test_bits.h"

/* SYSCON APB_CTL_DATE; bit 31 is the top bit of ESP32's major revision. */
#define APB_CTL_DATE_ADDR 0x3FF6607Cu
#define ESP32_CODING_WORD 6u
#define ESP32_CODING_34   1u
#define C2_XTS_KEY_LENGTH_256_BIT 42u

#define MAX_BLOCK_BYTES (ESP_LOADER_EFUSE_MAX_WORDS_PER_BLOCK * EFUSE_WORD_BYTES)

/* Make loader_read_chip_revision() report v3.0: major 3 needs all three revision
 * bits set (word3 bit 15, word5 bit 20, APB_CTL_DATE bit 31 -> combine == 7),
 * which is the gate burn_key_digest checks. Anything less reads as < v3.0. */
static void esp32_set_rev3(void)
{
    fake_set_efuse_bit(ESP32_CHIP, 0, 3u * 32u + 15u);
    fake_set_efuse_bit(ESP32_CHIP, 0, 5u * 32u + 20u);
    fake_set_reg(APB_CTL_DATE_ADDR, 1u << 31);
}

static void esp32_set_coding_34(void)
{
    fake_set_efuse_word(ESP32_CHIP, 0, ESP32_CODING_WORD, ESP32_CODING_34);
}

static uint8_t byte_of(const uint32_t *buf, uint16_t i)
{
    return (uint8_t)(buf[i / EFUSE_WORD_BYTES] >> ((i % EFUSE_WORD_BYTES) * 8u));
}

/* Bytes of `block`'s staging, from `word_offset` for `count` words. */
static void staged_bytes(const esp_loader_efuse_ctx_t *ctx, uint8_t block,
                         uint8_t word_offset, uint8_t word_count, uint8_t *out)
{
    const uint32_t *buf = efuse_get_write_buf(ctx, block);

    for (uint8_t i = 0; i < word_count * EFUSE_WORD_BYTES; i++) {
        out[i] = byte_of(buf, (uint16_t)(word_offset * EFUSE_WORD_BYTES + i));
    }
}

/* True when every staged word of `block` is zero — i.e. nothing was staged. */
static bool block_untouched(const esp_loader_efuse_ctx_t *ctx, uint8_t block,
                            uint8_t word_count)
{
    const uint32_t *buf = efuse_get_write_buf(ctx, block);
    uint32_t any = 0;

    for (uint8_t w = 0; w < word_count; w++) {
        any |= buf[w];
    }
    return any == 0u;
}

/* The half table entry for C2's low (0) / high (1) 128 bits. */
static const efuse_key_half_t *c2_half(uint8_t index)
{
    uint8_t count = 0;
    const efuse_key_half_t *halves = efuse_get_key_halves(ESP32C2_CHIP, &count);

    return (halves != NULL && index < count) ? &halves[index] : NULL;
}

/*
 * espefuse reverses the key with data[::-1] over a buffer that is exactly the
 * target's size — it rejects any other length outright. write_key_legacy is
 * more permissive: it zero-pads a short key to the target size and reverses
 * that whole buffer, so a short key ends up in the HIGH bytes, not reversed in
 * place. This builds the same expectation independently of the code under test.
 */
static void expect_staged(const uint8_t *key, size_t key_size, uint16_t data_bytes,
                          bool reverse, uint8_t *out)
{
    memset(out, 0, data_bytes);
    memcpy(out, key, key_size);
    if (reverse) {
        for (uint16_t i = 0; i < data_bytes / 2u; i++) {
            uint8_t t = out[i];
            out[i] = out[data_bytes - 1u - i];
            out[data_bytes - 1u - i] = t;
        }
    }
}

TEST(esp32_legacy_key_reversal_follows_the_target)
{
    /* espefuse reverses for flash_encryption / secure_boot_v1 (an AES key fed to
     * the hardware), never for the secure_boot_v2 RSA digest. */
    static const struct {
        esp_loader_legacy_key_target_t target;
        uint8_t block;
        bool reverse;
    } cases[] = {
        { ESP_LOADER_LEGACY_KEY_ESP32_FLASH_ENCRYPTION, 1u, true  },
        { ESP_LOADER_LEGACY_KEY_ESP32_SECURE_BOOT_V1,   2u, true  },
        { ESP_LOADER_LEGACY_KEY_ESP32_SECURE_BOOT_V2,   2u, false },
    };

    const efuse_chip_layout_t *L = efuse_get_chip_layout(ESP32_CHIP);
    CHECK(L != NULL, "ESP32 must have a layout");

    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        const uint8_t block = cases[c].block;
        const uint8_t word_count = L->blocks[block].word_count;
        const uint16_t data_bytes = (uint16_t)(word_count * EFUSE_WORD_BYTES);

        fake_efuse_reset();
        esp32_set_rev3(); /* secure_boot_v2 is refused below v3.0 */
        esp_loader_t loader = fake_loader(ESP32_CHIP);
        esp_loader_efuse_ctx_t ctx = { 0 };

        /* Distinct, non-palindromic bytes: reversed and non-reversed differ in
         * every position, so neither can pass for the other. */
        uint8_t key[MAX_BLOCK_BYTES];
        for (uint16_t i = 0; i < data_bytes; i++) {
            key[i] = (uint8_t)(i + 1u);
        }

        esp_loader_error_t err = esp_loader_efuse_write_key_legacy(
                                     &loader, &ctx, cases[c].target, key, data_bytes, NULL);
        CHECK_EQ(err, ESP_LOADER_SUCCESS, "target=%u", cases[c].target);

        uint8_t want[MAX_BLOCK_BYTES];
        uint8_t got[MAX_BLOCK_BYTES];
        expect_staged(key, data_bytes, data_bytes, cases[c].reverse, want);
        staged_bytes(&ctx, block, 0u, word_count, got);
        CHECK_MEM_EQ(got, want, data_bytes,
                     "target=%u: block %u staged bytes (reverse=%d)",
                     cases[c].target, block, cases[c].reverse);
    }
    return true;
}

TEST(esp32_legacy_short_key_is_padded_then_reversed)
{
    /* A short key is zero-padded to the block, and reversing happens over the
     * padded buffer — so the key lands in the HIGH bytes and the low bytes stay
     * zero. Staging it "reversed in place" at the bottom would be wrong. */
    const efuse_chip_layout_t *L = efuse_get_chip_layout(ESP32_CHIP);
    const uint8_t word_count = L->blocks[1].word_count;
    const uint16_t data_bytes = (uint16_t)(word_count * EFUSE_WORD_BYTES);

    fake_efuse_reset();
    esp_loader_t loader = fake_loader(ESP32_CHIP);
    esp_loader_efuse_ctx_t ctx = { 0 };

    const uint8_t key[4] = { 0xA1u, 0xB2u, 0xC3u, 0xD4u };
    esp_loader_error_t err = esp_loader_efuse_write_key_legacy(
                                 &loader, &ctx, ESP_LOADER_LEGACY_KEY_ESP32_FLASH_ENCRYPTION,
                                 key, sizeof(key), NULL);
    CHECK_EQ(err, ESP_LOADER_SUCCESS, "short key rejected");

    uint8_t got[MAX_BLOCK_BYTES];
    staged_bytes(&ctx, 1u, 0u, word_count, got);

    for (uint16_t i = 0; i < data_bytes - sizeof(key); i++) {
        CHECK_EQ(got[i], 0u, "byte %u below the padded key is not zero", i);
    }
    for (uint16_t i = 0; i < sizeof(key); i++) {
        uint16_t at = (uint16_t)(data_bytes - 1u - i);
        CHECK_EQ(got[at], key[i], "key byte %u belongs at the top, offset %u", i, at);
    }
    return true;
}

TEST(c2_legacy_key_targets_select_their_half)
{
    /* The three C2 targets differ only in which part of BLOCK_KEY0 they fill:
     * whole block, low 128 bits, high 128 bits. The untouched part staying zero
     * is what proves the selection — filling the block always would pass the
     * first assertion alone. */
    const efuse_key_half_t *low  = c2_half(0);
    const efuse_key_half_t *high = c2_half(1);
    CHECK(low != NULL && high != NULL, "C2 must have two BLOCK_KEY0 halves");

    static const struct {
        esp_loader_legacy_key_target_t target;
        uint8_t half;      /* 0 low, 1 high, 2 both */
        bool reverse;
    } cases[] = {
        { ESP_LOADER_LEGACY_KEY_C2_XTS_AES_128,         2u, true  },
        { ESP_LOADER_LEGACY_KEY_C2_XTS_AES_128_DERIVED, 0u, true  },
        { ESP_LOADER_LEGACY_KEY_C2_SECURE_BOOT_DIGEST,  1u, false },
    };

    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        const bool whole = (cases[c].half == 2u);
        const efuse_key_half_t *h = (cases[c].half == 1u) ? high : low;
        const uint8_t word_offset = whole ? low->word_offset : h->word_offset;
        const uint8_t word_count  = whole ? (uint8_t)(low->word_count + high->word_count)
                                    : h->word_count;
        const uint16_t data_bytes = (uint16_t)(word_count * EFUSE_WORD_BYTES);

        fake_efuse_reset();
        esp_loader_t loader = fake_loader(ESP32C2_CHIP);
        esp_loader_efuse_ctx_t ctx = { 0 };

        uint8_t key[MAX_BLOCK_BYTES];
        for (uint16_t i = 0; i < data_bytes; i++) {
            key[i] = (uint8_t)(0x10u + i);
        }

        esp_loader_error_t err = esp_loader_efuse_write_key_legacy(
                                     &loader, &ctx, cases[c].target, key, data_bytes, NULL);
        CHECK_EQ(err, ESP_LOADER_SUCCESS, "target=%u", cases[c].target);

        uint8_t want[MAX_BLOCK_BYTES];
        uint8_t got[MAX_BLOCK_BYTES];
        expect_staged(key, data_bytes, data_bytes, cases[c].reverse, want);
        staged_bytes(&ctx, low->block, word_offset, word_count, got);
        CHECK_MEM_EQ(got, want, data_bytes,
                     "target=%u: words [%u,%u) of BLOCK_KEY0",
                     cases[c].target, word_offset, word_offset + word_count);

        /* The half this target does not own must be untouched. */
        if (!whole) {
            const efuse_key_half_t *other = (cases[c].half == 1u) ? low : high;
            uint8_t other_bytes[MAX_BLOCK_BYTES];
            staged_bytes(&ctx, other->block, other->word_offset, other->word_count, other_bytes);
            for (uint8_t i = 0; i < other->word_count * EFUSE_WORD_BYTES; i++) {
                CHECK_EQ(other_bytes[i], 0u,
                         "target=%u: byte %u of the other half was written",
                         cases[c].target, i);
            }
        }
    }
    return true;
}

TEST(c2_legacy_xts_key_length_bit_is_whole_key_only)
{
    /* espefuse sets XTS_KEY_LENGTH_256 for XTS_AES_128_KEY (the whole 256-bit
     * block) and for no other purpose — it is what tells the ROM the key is
     * 256-bit rather than derived from 128 bits. */
    static const struct {
        esp_loader_legacy_key_target_t target;
        uint8_t half;
        uint32_t want_bit;
    } cases[] = {
        { ESP_LOADER_LEGACY_KEY_C2_XTS_AES_128,         2u, 1u },
        { ESP_LOADER_LEGACY_KEY_C2_XTS_AES_128_DERIVED, 0u, 0u },
        { ESP_LOADER_LEGACY_KEY_C2_SECURE_BOOT_DIGEST,  1u, 0u },
    };
    const efuse_key_half_t *low = c2_half(0);
    const efuse_key_half_t *high = c2_half(1);
    CHECK(low != NULL && high != NULL, "C2 must have two BLOCK_KEY0 halves");

    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        const uint8_t word_count = (cases[c].half == 2u)
                                   ? (uint8_t)(low->word_count + high->word_count)
                                   : ((cases[c].half == 1u) ? high->word_count : low->word_count);

        fake_efuse_reset();
        esp_loader_t loader = fake_loader(ESP32C2_CHIP);
        esp_loader_efuse_ctx_t ctx = { 0 };

        uint8_t key[MAX_BLOCK_BYTES] = { 1 };
        esp_loader_error_t err = esp_loader_efuse_write_key_legacy(
                                     &loader, &ctx, cases[c].target, key,
                                     word_count * EFUSE_WORD_BYTES, NULL);
        CHECK_EQ(err, ESP_LOADER_SUCCESS, "target=%u", cases[c].target);

        CHECK_EQ(staged_bit(efuse_get_write_buf(&ctx, 0),
                            C2_XTS_KEY_LENGTH_256_BIT), cases[c].want_bit,
                 "target=%u: XTS_KEY_LENGTH_256", cases[c].target);
    }
    return true;
}

TEST(c2_legacy_whole_key_read_protects_both_halves)
{
    /* BLOCK_KEY0 has one RD_DIS bit per half (espefuse read_disable_bit [0, 1]).
     * A whole-block key must set both, or half the key stays readable. */
    const efuse_key_half_t *low = c2_half(0);
    const efuse_key_half_t *high = c2_half(1);
    CHECK(low != NULL && high != NULL, "C2 must have two BLOCK_KEY0 halves");

    fake_efuse_reset();
    esp_loader_t loader = fake_loader(ESP32C2_CHIP);
    esp_loader_efuse_ctx_t ctx = { 0 };

    const uint8_t word_count = (uint8_t)(low->word_count + high->word_count);
    uint8_t key[MAX_BLOCK_BYTES] = { 1 };
    esp_loader_error_t err = esp_loader_efuse_write_key_legacy(
                                 &loader, &ctx, ESP_LOADER_LEGACY_KEY_C2_XTS_AES_128,
                                 key, word_count * EFUSE_WORD_BYTES, NULL);
    CHECK_EQ(err, ESP_LOADER_SUCCESS, "whole key rejected");

    const uint32_t *blk0 = efuse_get_write_buf(&ctx, 0);
    CHECK(staged_bit(blk0, low->rd_dis_bit),
          "low half RD_DIS (bit %u) not staged", low->rd_dis_bit);
    CHECK(staged_bit(blk0, high->rd_dis_bit),
          "high half RD_DIS (bit %u) not staged", high->rd_dis_bit);
    return true;
}

TEST(legacy_key_protection_follows_target_and_config)
{
    /* Write protection applies to every target; read protection only to the
     * ones holding a secret (espefuse read-protects flash_encryption /
     * secure_boot_v1 / XTS_AES_*, never a Secure Boot digest). Both are
     * opt-out through config. */
    static const struct {
        target_chip_t chip;
        esp_loader_legacy_key_target_t target;
        uint8_t block;     /* EFUSE_INDEX_NONE: C2, resolved from the half table */
        uint8_t half;      /* C2 only: which 128-bit half the target fills */
        bool read_protect; /* what the target does with config == NULL */
    } targets[] = {
        { ESP32_CHIP,   ESP_LOADER_LEGACY_KEY_ESP32_FLASH_ENCRYPTION, 1u, 0u, true  },
        { ESP32_CHIP,   ESP_LOADER_LEGACY_KEY_ESP32_SECURE_BOOT_V1,   2u, 0u, true  },
        { ESP32_CHIP,   ESP_LOADER_LEGACY_KEY_ESP32_SECURE_BOOT_V2,   2u, 0u, false },
        { ESP32C2_CHIP, ESP_LOADER_LEGACY_KEY_C2_XTS_AES_128_DERIVED, EFUSE_INDEX_NONE, 0u, true  },
        { ESP32C2_CHIP, ESP_LOADER_LEGACY_KEY_C2_SECURE_BOOT_DIGEST,  EFUSE_INDEX_NONE, 1u, false },
    };
    static const esp_loader_efuse_write_key_config_t configs[] = {
        { false, false }, { true, false }, { false, true }, { true, true },
    };

    for (size_t t = 0; t < sizeof(targets) / sizeof(targets[0]); t++) {
        const efuse_chip_layout_t *L = efuse_get_chip_layout(targets[t].chip);
        const efuse_key_half_t *half = (targets[t].block == EFUSE_INDEX_NONE)
                                       ? c2_half(targets[t].half) : NULL;
        const uint8_t block = (half != NULL) ? half->block : targets[t].block;
        const efuse_blk_layout_t *blk = &L->blocks[block];

        for (size_t c = 0; c < sizeof(configs) / sizeof(configs[0]); c++) {
            const esp_loader_efuse_write_key_config_t cfg = configs[c];

            /* Size the key to the target: a C2 half takes half a block, and
             * carries its own RD_DIS bit (the block row's stays NONE). */
            const uint8_t word_count = (half != NULL) ? half->word_count : blk->word_count;
            const uint16_t rd_dis_bit = (half != NULL) ? half->rd_dis_bit : blk->rd_dis_bit;

            fake_efuse_reset();
            esp32_set_rev3();
            esp_loader_t loader = fake_loader(targets[t].chip);
            esp_loader_efuse_ctx_t ctx = { 0 };

            uint8_t key[MAX_BLOCK_BYTES] = { 1 };
            esp_loader_error_t err = esp_loader_efuse_write_key_legacy(
                                         &loader, &ctx, targets[t].target, key,
                                         word_count * EFUSE_WORD_BYTES, &cfg);
            CHECK_EQ(err, ESP_LOADER_SUCCESS, "target=%u cfg=%zu", targets[t].target, c);

            const uint32_t *blk0 = efuse_get_write_buf(&ctx, 0);
            if (blk->wr_dis_bit != ESP_LOADER_EFUSE_BIT_NONE) {
                CHECK_EQ(staged_bit(blk0, blk->wr_dis_bit), cfg.no_write_protect ? 0u : 1u,
                         "target=%u cfg=%zu: WR_DIS bit %u", targets[t].target, c, blk->wr_dis_bit);
            }
            uint32_t want_rd = (targets[t].read_protect && !cfg.no_read_protect) ? 1u : 0u;
            CHECK_EQ(staged_bit(blk0, rd_dis_bit), want_rd,
                     "target=%u cfg=%zu: RD_DIS bit %u", targets[t].target, c, rd_dis_bit);
        }
    }
    return true;
}

TEST(legacy_key_refuses_a_block_already_in_use)
{
    /* The in-use check is scoped to the words the target actually fills, so on
     * C2 a used high half must not block a low-half key. A whole-block check
     * would pass the first case and fail the second. */
    static const struct {
        target_chip_t chip;
        esp_loader_legacy_key_target_t target;
        uint8_t block;      /* EFUSE_INDEX_NONE: C2, resolved from the half table */
        uint8_t seed_half;  /* C2 only: which half to dirty */
        bool refused;
    } cases[] = {
        /* ESP32: the target owns the whole block, so any word blocks it. */
        { ESP32_CHIP,   ESP_LOADER_LEGACY_KEY_ESP32_FLASH_ENCRYPTION, 1u,               0u, true  },
        /* C2 low-half key, low half dirty -> refused; high half dirty -> allowed. */
        { ESP32C2_CHIP, ESP_LOADER_LEGACY_KEY_C2_XTS_AES_128_DERIVED, EFUSE_INDEX_NONE, 0u, true  },
        { ESP32C2_CHIP, ESP_LOADER_LEGACY_KEY_C2_XTS_AES_128_DERIVED, EFUSE_INDEX_NONE, 1u, false },
    };

    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        const efuse_chip_layout_t *L = efuse_get_chip_layout(cases[c].chip);
        const bool split = (cases[c].block == EFUSE_INDEX_NONE);
        const uint8_t block = split ? c2_half(0)->block : cases[c].block;
        const uint8_t word_count = split ? c2_half(0)->word_count
                                   : L->blocks[block].word_count;
        const uint8_t dirty_word = split ? c2_half(cases[c].seed_half)->word_offset : 0u;

        fake_efuse_reset();
        fake_set_efuse_word(cases[c].chip, block, dirty_word, 0xDEADBEEFu);

        esp_loader_t loader = fake_loader(cases[c].chip);
        esp_loader_efuse_ctx_t ctx = { 0 };

        /* Non-zero, so "nothing staged" is a claim a staged key would break —
         * all-zero is what an unburned block reads as. */
        uint8_t key[MAX_BLOCK_BYTES] = { 1 };
        esp_loader_error_t err = esp_loader_efuse_write_key_legacy(
                                     &loader, &ctx, cases[c].target, key,
                                     word_count * EFUSE_WORD_BYTES, NULL);

        if (cases[c].refused) {
            CHECK_EQ(err, ESP_LOADER_ERROR_EFUSE_BLOCK_IN_USE,
                     "case %zu: word %u of block %u already holds data",
                     c, dirty_word, block);
            CHECK(block_untouched(&ctx, block, L->blocks[block].word_count),
                  "case %zu: block %u staged after a refusal", c, block);
        } else {
            CHECK_EQ(err, ESP_LOADER_SUCCESS,
                     "case %zu: a used half must not block the other half", c);
        }
    }
    return true;
}

TEST(esp32_legacy_secure_boot_v2_requires_rev3_and_none_coding)
{
    /* espefuse burn_key_digest refuses a 3/4-coded block (a 32-byte digest does
     * not fit 24 bytes) and any chip below v3.0. Both are checked before
     * anything is staged. */
    const efuse_chip_layout_t *L = efuse_get_chip_layout(ESP32_CHIP);
    const uint8_t word_count = L->blocks[2].word_count;
    uint8_t key[MAX_BLOCK_BYTES] = { 1 };

    /* Pre-v3.0: refused even with NONE coding. */
    fake_efuse_reset();
    esp_loader_t loader = fake_loader(ESP32_CHIP);
    esp_loader_efuse_ctx_t ctx = { 0 };
    esp_loader_error_t err = esp_loader_efuse_write_key_legacy(
                                 &loader, &ctx, ESP_LOADER_LEGACY_KEY_ESP32_SECURE_BOOT_V2,
                                 key, word_count * EFUSE_WORD_BYTES, NULL);
    CHECK_EQ(err, ESP_LOADER_ERROR_UNSUPPORTED_FUNC, "pre-v3.0 chip must be refused");
    CHECK(block_untouched(&ctx, 2u, word_count), "block 2 staged despite refusal");

    /* v3.0 but 3/4 coding: still refused, and now by the coding check. */
    fake_efuse_reset();
    esp32_set_rev3();
    esp32_set_coding_34();
    ctx = (esp_loader_efuse_ctx_t) {
        0
    };
    err = esp_loader_efuse_write_key_legacy(
              &loader, &ctx, ESP_LOADER_LEGACY_KEY_ESP32_SECURE_BOOT_V2,
              key, word_count * EFUSE_WORD_BYTES, NULL);
    CHECK_EQ(err, ESP_LOADER_ERROR_UNSUPPORTED_FUNC, "3/4 coding must be refused");
    CHECK(block_untouched(&ctx, 2u, word_count), "block 2 staged despite refusal");

    /* v3.0 with NONE coding: accepted. Without this the two refusals above
     * would also pass an implementation that refuses unconditionally. */
    fake_efuse_reset();
    esp32_set_rev3();
    ctx = (esp_loader_efuse_ctx_t) {
        0
    };
    err = esp_loader_efuse_write_key_legacy(
              &loader, &ctx, ESP_LOADER_LEGACY_KEY_ESP32_SECURE_BOOT_V2,
              key, word_count * EFUSE_WORD_BYTES, NULL);
    CHECK_EQ(err, ESP_LOADER_SUCCESS, "v3.0 + NONE coding must be accepted");
    return true;
}

TEST(esp32_legacy_34_coding_shrinks_the_usable_block)
{
    /* Under 3/4 coding a block stores 6 words of real data in 8 words of
     * silicon (espefuse: KEYBLOCKS_192 instead of KEYBLOCKS_256), so a key
     * sized for the uncoded block no longer fits. */
    const efuse_chip_layout_t *L = efuse_get_chip_layout(ESP32_CHIP);
    const uint16_t full_bytes = (uint16_t)(L->blocks[1].word_count * EFUSE_WORD_BYTES);
    const uint16_t coded_bytes = 6u * EFUSE_WORD_BYTES;
    CHECK(full_bytes > coded_bytes, "3/4 coding must shrink the block");

    uint8_t key[MAX_BLOCK_BYTES] = { 1 };

    fake_efuse_reset();
    esp32_set_coding_34();
    esp_loader_t loader = fake_loader(ESP32_CHIP);
    esp_loader_efuse_ctx_t ctx = { 0 };

    esp_loader_error_t err = esp_loader_efuse_write_key_legacy(
                                 &loader, &ctx, ESP_LOADER_LEGACY_KEY_ESP32_FLASH_ENCRYPTION,
                                 key, full_bytes, NULL);
    CHECK_EQ(err, ESP_LOADER_ERROR_INVALID_PARAM,
             "%u bytes must not fit a 3/4-coded block", full_bytes);

    /* The coded size is what fits — otherwise "too big" could just mean the
     * target rejects every size. */
    ctx = (esp_loader_efuse_ctx_t) {
        0
    };
    err = esp_loader_efuse_write_key_legacy(
              &loader, &ctx, ESP_LOADER_LEGACY_KEY_ESP32_FLASH_ENCRYPTION,
              key, coded_bytes, NULL);
    CHECK_EQ(err, ESP_LOADER_SUCCESS, "%u bytes must fit a 3/4-coded block", coded_bytes);
    return true;
}

TEST(legacy_key_rejects_bad_arguments)
{
    const efuse_chip_layout_t *L = efuse_get_chip_layout(ESP32_CHIP);
    const uint16_t data_bytes = (uint16_t)(L->blocks[1].word_count * EFUSE_WORD_BYTES);
    uint8_t key[MAX_BLOCK_BYTES] = { 1 };

    fake_efuse_reset();
    esp_loader_t loader = fake_loader(ESP32_CHIP);
    esp_loader_efuse_ctx_t ctx = { 0 };

    CHECK_EQ(esp_loader_efuse_write_key_legacy(&loader, &ctx,
             ESP_LOADER_LEGACY_KEY_ESP32_FLASH_ENCRYPTION, key, 0u, NULL),
             ESP_LOADER_ERROR_INVALID_PARAM, "empty key accepted");
    CHECK_EQ(esp_loader_efuse_write_key_legacy(&loader, &ctx,
             ESP_LOADER_LEGACY_KEY_ESP32_FLASH_ENCRYPTION, key,
             (size_t)data_bytes + 1u, NULL),
             ESP_LOADER_ERROR_INVALID_PARAM, "oversized key accepted");

    /* A C2 target on an ESP32 loader: the target names the chip, so a mismatch
     * against the detected one is a caller error, not a silent reinterpretation. */
    CHECK_EQ(esp_loader_efuse_write_key_legacy(&loader, &ctx,
             ESP_LOADER_LEGACY_KEY_C2_XTS_AES_128, key, data_bytes, NULL),
             ESP_LOADER_ERROR_INVALID_PARAM, "C2 target accepted on an ESP32 loader");
    return true;
}

TEST_MAIN()
