/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Behavioral tests for esp_loader_efuse_write_key() staging, run through the
 * public API against the in-memory fake (a blank chip: every key block free).
 * write_key only stages into ctx — nothing is burned — so each test asserts on
 * the staged buffer (efuse_get_write_buf) that the engine placed the right bits:
 *   - key bytes reversed for reverse purposes (XTS-AES / ECDSA), not for others;
 *   - KEY_PURPOSE_N set to the chip's silicon code for the purpose;
 *   - KEY_PURPOSE always write-protected; key block write-protected; and read-
 *     protected iff the purpose needs it (keys yes, digests / USER no).
 * Layout and purpose data are trusted inputs here; tests do not inspect the
 * generated field-set tables. The all-chip pass below is intentionally only an
 * API/safety smoke test.
 */

#include <string.h>
#include "test_util.h"
#include "fake_efuse.h"
#include "esp_loader_efuse_private.h"
#include "esp_targets.h" /* ESP32P4_ECO_REV3_MIN */
#include "efuse_test_bits.h"

#define MAX_BLOCK_BYTES (ESP_LOADER_EFUSE_MAX_WORDS_PER_BLOCK * EFUSE_WORD_BYTES)

static uint8_t byte_of(const uint32_t *buf, uint16_t i)
{
    return (uint8_t)(buf[i / EFUSE_WORD_BYTES] >> ((i % EFUSE_WORD_BYTES) * 8u));
}

/* Which physical eFuse block holds KEY<key_idx> on this chip — KEY0 is BLK4 on
 * C3, BLK3 on C2, and so on. EFUSE_INDEX_NONE if the chip has no such key. */
static uint8_t block_idx_from_key_idx(const efuse_chip_layout_t *L, uint8_t key_idx)
{
    for (uint8_t b = 0; b < L->block_count; b++) {
        if (L->blocks[b].purpose_key_index == key_idx) {
            return b;
        }
    }
    return EFUSE_INDEX_NONE;
}

static bool check_c3_key(esp_loader_key_purpose_t purpose)
{
    const efuse_chip_layout_t *L = efuse_get_chip_layout(ESP32C3_CHIP);
    const efuse_key_purpose_row_t *row = efuse_lookup_purpose(ESP32C3_CHIP, purpose);
    CHECK(L != NULL && row != NULL, "C3 purpose=%u is not described", purpose);

    const uint8_t key_idx = ESP_LOADER_EFUSE_KEY0;
    const uint8_t block = block_idx_from_key_idx(L, key_idx);
    CHECK(block != EFUSE_INDEX_NONE, "C3 KEY0 has no physical block");
    const efuse_blk_layout_t *blk = &L->blocks[block];
    const uint16_t key_bytes = (uint16_t)(blk->word_count * EFUSE_WORD_BYTES);

    esp_loader_t loader = fake_loader(ESP32C3_CHIP);
    esp_loader_efuse_ctx_t ctx = { 0 };
    fake_efuse_reset();

    uint8_t key[MAX_BLOCK_BYTES];
    for (uint16_t i = 0; i < key_bytes; i++) {
        key[i] = (uint8_t)(i + 1u);
    }

    esp_loader_error_t err = esp_loader_efuse_write_key(
                                 &loader, &ctx, ESP_LOADER_EFUSE_KEY0,
                                 purpose, key, key_bytes, NULL);
    CHECK_EQ(err, ESP_LOADER_SUCCESS, "C3 purpose=%u", purpose);

    const uint32_t *kbuf = efuse_get_write_buf(&ctx, block);
    for (uint16_t i = 0; i < key_bytes; i++) {
        uint8_t expected = row->needs_reverse ? key[key_bytes - 1u - i] : key[i];
        if (byte_of(kbuf, i) != expected) {
            TEST_FAIL("C3 purpose=%u: key byte %u is 0x%02x, want 0x%02x",
                      purpose, i, byte_of(kbuf, i), expected);
        }
    }

    const uint32_t *blk0 = efuse_get_write_buf(&ctx, 0);
    const efuse_key_purpose_field_t *kp = &L->key_purpose;
    CHECK_EQ(staged_field(blk0, kp->purpose0_bit, kp->purpose_width), row->code,
             "C3 purpose=%u: KEY_PURPOSE0 value", purpose);
    CHECK(staged_bit(blk0, kp->purpose0_wr_dis_bit),
          "C3 purpose=%u: KEY_PURPOSE0 not write-protected", purpose);
    CHECK(staged_bit(blk0, blk->wr_dis_bit),
          "C3 purpose=%u: KEY0 not write-protected", purpose);
    CHECK_EQ(staged_bit(blk0, blk->rd_dis_bit), row->needs_rd_protect ? 1u : 0u,
             "C3 purpose=%u: KEY0 read protection", purpose);
    return true;
}

TEST(c3_keys_follow_layout_and_purpose_data)
{
    static const esp_loader_key_purpose_t purposes[] = {
        ESP_LOADER_KEY_PURPOSE_USER,
        ESP_LOADER_KEY_PURPOSE_XTS_AES_128_KEY,
        ESP_LOADER_KEY_PURPOSE_HMAC_UP,
        ESP_LOADER_KEY_PURPOSE_SECURE_BOOT_DIGEST0,
    };

    for (size_t i = 0; i < sizeof(purposes) / sizeof(purposes[0]); i++) {
        if (!check_c3_key(purposes[i])) {
            return false;
        }
    }
    return true;
}

TEST(write_key_all_chip_smoke)
{
    for (target_chip_t chip = 0; chip < ESP_MAX_CHIP; chip++) {
        const efuse_chip_layout_t *L = efuse_get_chip_layout(chip);
        if (L == NULL || efuse_lookup_purpose(chip, ESP_LOADER_KEY_PURPOSE_USER) == NULL) {
            continue;
        }
        uint8_t block = block_idx_from_key_idx(L, 0u);
        CHECK(block != EFUSE_INDEX_NONE, "chip=%u: KEY0 has no physical block", chip);

        /* Every byte non-zero, so the "staged something" check below does not
         * depend on where in the block byte 0 ends up — a reversing purpose
         * would move a lone 1 to the last word. */
        uint8_t key[MAX_BLOCK_BYTES];
        memset(key, 0xA5u, sizeof(key));
        size_t key_size = L->blocks[block].word_count * EFUSE_WORD_BYTES;
        esp_loader_t loader = fake_loader(chip);
        esp_loader_efuse_ctx_t ctx = { 0 };
        fake_efuse_reset();

        esp_loader_error_t err = esp_loader_efuse_write_key(
                                     &loader, &ctx, ESP_LOADER_EFUSE_KEY0,
                                     ESP_LOADER_KEY_PURPOSE_USER, key, key_size, NULL);
        CHECK_EQ(err, ESP_LOADER_SUCCESS, "chip=%u USER-key smoke", chip);
        CHECK(efuse_get_write_buf(&ctx, block)[0] != 0u,
              "chip=%u: successful write_key staged no key data", chip);
    }
    return true;
}

TEST(write_key5_rejects_reverse_purposes_only_on_quirk_chips)
{
    static const target_chip_t quirk_chips[] = {
        ESP32C3_CHIP,
        ESP32C6_CHIP,
        ESP32S3_CHIP,
        ESP32H2_CHIP,
    };

    uint8_t key[MAX_BLOCK_BYTES];
    memset(key, 0xA5u, sizeof(key));

    for (size_t i = 0; i < sizeof(quirk_chips) / sizeof(quirk_chips[0]); i++) {
        target_chip_t chip = quirk_chips[i];
        const efuse_chip_layout_t *L = efuse_get_chip_layout(chip);
        uint8_t block = block_idx_from_key_idx(L, ESP_LOADER_EFUSE_KEY5);
        CHECK(block != EFUSE_INDEX_NONE, "chip=%u: KEY5 has no physical block", chip);

        esp_loader_t loader = fake_loader(chip);
        esp_loader_efuse_ctx_t ctx = { 0 };
        fake_efuse_reset();

        esp_loader_error_t err = esp_loader_efuse_write_key(
                                     &loader, &ctx, ESP_LOADER_EFUSE_KEY5,
                                     ESP_LOADER_KEY_PURPOSE_XTS_AES_128_KEY,
                                     key, L->blocks[block].word_count * EFUSE_WORD_BYTES, NULL);
        CHECK_EQ(err, ESP_LOADER_ERROR_INVALID_PARAM,
                 "chip=%u: reverse purpose accepted in KEY5", chip);

        for (size_t w = 0; w < sizeof(ctx._buf) / sizeof(ctx._buf[0]); w++) {
            CHECK_EQ(ctx._buf[w], 0u,
                     "chip=%u: rejected KEY5 write left staging at word %zu", chip, w);
        }
    }

    /* P4 has KEY5 but not the silicon quirk; the same operation must succeed. */
    const efuse_chip_layout_t *L = efuse_get_chip_layout(ESP32P4_CHIP);
    uint8_t block = block_idx_from_key_idx(L, ESP_LOADER_EFUSE_KEY5);
    CHECK(block != EFUSE_INDEX_NONE, "P4: KEY5 has no physical block");

    esp_loader_t loader = fake_loader(ESP32P4_CHIP);
    esp_loader_efuse_ctx_t ctx = { 0 };
    fake_efuse_reset();
    CHECK_EQ(esp_loader_efuse_write_key(
                 &loader, &ctx, ESP_LOADER_EFUSE_KEY5,
                 ESP_LOADER_KEY_PURPOSE_XTS_AES_128_KEY,
                 key, L->blocks[block].word_count * EFUSE_WORD_BYTES, NULL),
             ESP_LOADER_SUCCESS, "P4 incorrectly applied the KEY5 quirk");
    return true;
}

TEST(write_key_refuses_used_block)
{
    for (target_chip_t chip = 0; chip < ESP_MAX_CHIP; chip++) {
        const efuse_chip_layout_t *L = efuse_get_chip_layout(chip);
        if (L == NULL || efuse_lookup_purpose(chip, ESP_LOADER_KEY_PURPOSE_XTS_AES_128_KEY) == NULL) {
            continue;
        }
        uint8_t block = block_idx_from_key_idx(L, 0u);
        if (block == EFUSE_INDEX_NONE) {
            continue;
        }
        esp_loader_t loader = fake_loader(chip);
        esp_loader_efuse_ctx_t ctx = { 0 };

        fake_efuse_reset();
        fake_set_efuse_word(chip, block, 0u, 0xDEADBEEFu); /* block already holds data */

        /* Not all-zero, so the "nothing staged" check below can tell a staged key
         * from an untouched block — all-zero is what an unburned block reads as. */
        uint8_t key[MAX_BLOCK_BYTES] = { 1 };
        esp_loader_error_t err = esp_loader_efuse_write_key(
                                     &loader, &ctx, ESP_LOADER_EFUSE_KEY0,
                                     ESP_LOADER_KEY_PURPOSE_XTS_AES_128_KEY, key,
                                     L->blocks[block].word_count * EFUSE_WORD_BYTES, NULL);
        CHECK_EQ(err, ESP_LOADER_ERROR_EFUSE_BLOCK_IN_USE,
                 "chip=%u: block %u already holds data", chip, block);

        /* Nothing should have been staged. */
        const uint32_t *kbuf = efuse_get_write_buf(&ctx, block);
        uint32_t any = 0;
        for (uint8_t w = 0; w < L->blocks[block].word_count; w++) {
            any |= kbuf[w];
        }
        CHECK_EQ(any, 0u, "chip=%u: block %u staged after a refusal", chip, block);
    }
    return true;
}

TEST(write_key_unsupported_without_key_purpose)
{
    /* ESP32 and C2 have no KEY_PURPOSE field. */
    static const target_chip_t legacy[] = { ESP32_CHIP, ESP32C2_CHIP };
    uint8_t key[MAX_BLOCK_BYTES] = { 1 };
    for (size_t i = 0; i < sizeof(legacy) / sizeof(legacy[0]); i++) {
        esp_loader_t loader = fake_loader(legacy[i]);
        esp_loader_efuse_ctx_t ctx = { 0 };
        fake_efuse_reset();
        esp_loader_error_t err = esp_loader_efuse_write_key(
                                     &loader, &ctx, ESP_LOADER_EFUSE_KEY0,
                                     ESP_LOADER_KEY_PURPOSE_XTS_AES_128_KEY, key, sizeof(key), NULL);
        CHECK_EQ(err, ESP_LOADER_ERROR_UNSUPPORTED_FUNC,
                 "chip=%u has no KEY_PURPOSE field", legacy[i]);
    }
    return true;
}

/* The config flags opt out of the two protections write_key applies by
 * default. C3 with a purpose that needs both is the case where all three bits
 * (KEY_PURPOSE WR_DIS, block WR_DIS, block RD_DIS) are in play at once. */
TEST(write_key_config_flags_opt_out_of_protection)
{
    static const struct {
        esp_loader_efuse_write_key_config_t cfg;
        uint32_t want_wr;
        uint32_t want_rd;
    } cases[] = {
        { { false, false }, 1u, 1u },
        { { true,  false }, 0u, 1u },
        { { false, true  }, 1u, 0u },
        { { true,  true  }, 0u, 0u },
    };
    const uint8_t key_idx = 0u;

    for (target_chip_t chip = 0; chip < ESP_MAX_CHIP; chip++) {
        const efuse_chip_layout_t *L = efuse_get_chip_layout(chip);
        const efuse_key_purpose_row_t *row =
            efuse_lookup_purpose(chip, ESP_LOADER_KEY_PURPOSE_XTS_AES_128_KEY);
        if (L == NULL || row == NULL || !row->needs_rd_protect) {
            continue;
        }
        const uint8_t block = block_idx_from_key_idx(L, key_idx);
        if (block == EFUSE_INDEX_NONE) {
            continue;
        }
        const efuse_key_purpose_field_t *kp = &L->key_purpose;
        const efuse_blk_layout_t *blk = &L->blocks[block];
        if (blk->wr_dis_bit == ESP_LOADER_EFUSE_BIT_NONE || blk->rd_dis_bit == ESP_LOADER_EFUSE_BIT_NONE) {
            continue; /* nothing for the flags to turn off */
        }

        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            esp_loader_t loader = fake_loader(chip);
            esp_loader_efuse_ctx_t ctx = { 0 };
            fake_efuse_reset();

            uint8_t key[MAX_BLOCK_BYTES] = { 1 };
            esp_loader_error_t err = esp_loader_efuse_write_key(
                                         &loader, &ctx, ESP_LOADER_EFUSE_KEY0,
                                         ESP_LOADER_KEY_PURPOSE_XTS_AES_128_KEY, key,
                                         blk->word_count * EFUSE_WORD_BYTES, &cases[i].cfg);
            CHECK_EQ(err, ESP_LOADER_SUCCESS, "chip=%u no_wr=%d no_rd=%d", chip,
                     cases[i].cfg.no_write_protect, cases[i].cfg.no_read_protect);

            const uint32_t *blk0 = efuse_get_write_buf(&ctx, 0);
            CHECK_EQ(staged_bit(blk0, blk->wr_dis_bit), cases[i].want_wr,
                     "chip=%u no_wr=%d: block %u WR_DIS bit %u", chip,
                     cases[i].cfg.no_write_protect, block, blk->wr_dis_bit);
            CHECK_EQ(staged_bit(blk0, blk->rd_dis_bit), cases[i].want_rd,
                     "chip=%u no_rd=%d: block %u RD_DIS bit %u", chip,
                     cases[i].cfg.no_read_protect, block, blk->rd_dis_bit);

            /* KEY_PURPOSE is locked whatever the flags say — a purpose that
             * could still be changed makes the key's protection meaningless. */
            CHECK(staged_bit(blk0, (uint16_t)(kp->purpose0_wr_dis_bit + key_idx)),
                  "chip=%u no_wr=%d: KEY_PURPOSE must stay wr-protected", chip,
                  cases[i].cfg.no_write_protect);
        }
    }
    return true;
}

/* ── P4 v3.0 split KEY_PURPOSE ──────────────────────────────────────────────── */

/* P384_L's purpose data requires P4's split H-bit on v3.0 silicon. */
#define P4_SPLIT_PURPOSE ESP_LOADER_KEY_PURPOSE_ECDSA_KEY_P384_L

/* Stage a key into P4's `key_block` against silicon reporting `eco_version`.
 * Returns write_key's status rather than asserting on it: whether it should
 * succeed is the thing under test. Which bytes the key holds does not matter —
 * reversal and padding are covered above — only that they are not zero. */
static esp_loader_error_t p4_stage_key(uint32_t eco_version,
                                       esp_loader_efuse_key_block_t key_block,
                                       esp_loader_key_purpose_t purpose,
                                       esp_loader_efuse_ctx_t *ctx)
{
    const efuse_chip_layout_t *L = efuse_get_chip_layout(ESP32P4_CHIP);
    const efuse_blk_layout_t *blk = &L->blocks[block_idx_from_key_idx(L, (uint8_t)key_block)];

    esp_loader_t loader = fake_loader(ESP32P4_CHIP);
    esp_loader_target_security_info_t sec = { .eco_version = eco_version };

    fake_efuse_reset();
    fake_set_security_info(&sec);

    /* Not all-zero, so "the block was left empty" is a claim a staged key would
     * break — all-zero is exactly what an unburned block reads as. */
    uint8_t key[MAX_BLOCK_BYTES] = { 1 };
    return esp_loader_efuse_write_key(&loader, ctx, key_block, purpose, key,
                                      blk->word_count * EFUSE_WORD_BYTES, NULL);
}

TEST(p4_split_purpose_refused_before_eco_rev3)
{
    /* One below the gate: this silicon has no H-bit, so the key cannot be given
     * a purpose that needs one. */
    esp_loader_efuse_ctx_t ctx = { 0 };
    esp_loader_error_t err = p4_stage_key(ESP32P4_ECO_REV3_MIN - 1u, ESP_LOADER_EFUSE_KEY0,
                                          P4_SPLIT_PURPOSE, &ctx);
    CHECK_EQ(err, ESP_LOADER_ERROR_UNSUPPORTED_FUNC,
             "eco %u has no H-bit", ESP32P4_ECO_REV3_MIN - 1u);

    /* A refusal must stage nothing: a later commit would burn what it left. */
    const efuse_chip_layout_t *L = efuse_get_chip_layout(ESP32P4_CHIP);
    for (uint8_t b = 0; b < L->block_count; b++) {
        const uint32_t *buf = efuse_get_write_buf(&ctx, b);
        for (uint8_t w = 0; w < L->blocks[b].word_count; w++) {
            CHECK_EQ(buf[w], 0u, "block %u word %u staged after a refusal", b, w);
        }
    }
    return true;
}

TEST(p4_rev1_ignores_key_purpose_h_bit_alias)
{
    const efuse_chip_layout_t *L = efuse_get_chip_layout(ESP32P4_CHIP);
    const uint8_t block = block_idx_from_key_idx(L, ESP_LOADER_EFUSE_KEY0);
    CHECK(block != EFUSE_INDEX_NONE, "P4: KEY0 has no physical block");

    esp_loader_t loader = fake_loader(ESP32P4_CHIP);
    esp_loader_target_security_info_t sec = {
        .eco_version = ESP32P4_ECO_REV3_MIN - 1u,
    };
    esp_loader_efuse_ctx_t ctx = { 0 };
    uint8_t key[MAX_BLOCK_BYTES] = { 1 };

    fake_efuse_reset();
    fake_set_security_info(&sec);
    /* On rev1, BLK0 bit 155 belongs to DCDC_VSET, not KEY_PURPOSE_0_H. */
    fake_set_efuse_bit(ESP32P4_CHIP, 0, 155u);

    CHECK_EQ(esp_loader_efuse_write_key(
                 &loader, &ctx, ESP_LOADER_EFUSE_KEY0,
                 ESP_LOADER_KEY_PURPOSE_XTS_AES_128_KEY, key,
                 L->blocks[block].word_count * EFUSE_WORD_BYTES, NULL),
             ESP_LOADER_SUCCESS, "rev1 DCDC_VSET made KEY0 appear used");
    return true;
}

static bool check_p4_split_purpose_field(esp_loader_efuse_key_block_t key_block,
        uint16_t h_bit)
{
    const efuse_chip_layout_t *L = efuse_get_chip_layout(ESP32P4_CHIP);
    const efuse_key_purpose_row_t *row =
        efuse_lookup_purpose(ESP32P4_CHIP, P4_SPLIT_PURPOSE);
    CHECK(row != NULL, "P4 split-purpose data unavailable");

    esp_loader_efuse_ctx_t ctx = { 0 };
    esp_loader_error_t err = p4_stage_key(
                                 ESP32P4_ECO_REV3_MIN, key_block,
                                 P4_SPLIT_PURPOSE, &ctx);
    CHECK_EQ(err, ESP_LOADER_SUCCESS, "P4 KEY%u", key_block);

    const efuse_key_purpose_field_t *kp = &L->key_purpose;
    uint16_t low_bit = (uint16_t)(kp->purpose0_bit + key_block * kp->purpose_width);
    uint32_t low_mask = (1u << kp->purpose_width) - 1u;
    const uint32_t *blk0 = efuse_get_write_buf(&ctx, 0);
    CHECK_EQ(staged_field(blk0, low_bit, kp->purpose_width), row->code & low_mask,
             "P4 KEY%u purpose low bits", key_block);
    CHECK_EQ(staged_bit(blk0, h_bit), row->code >> kp->purpose_width,
             "P4 KEY%u purpose H-bit", key_block);
    return true;
}

TEST(p4_split_purpose_stages_h_bits)
{
    /* Non-contiguous P4 v3.0 H-bit positions from the TRM/espefuse YAML.
     * KEY5 is intentionally not adjacent to KEY4. */
    if (!check_p4_split_purpose_field(ESP_LOADER_EFUSE_KEY0, 155u)) {
        return false;
    }
    return check_p4_split_purpose_field(ESP_LOADER_EFUSE_KEY5, 164u);
}

#undef P4_SPLIT_PURPOSE

TEST_MAIN()
