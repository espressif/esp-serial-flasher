/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Semantic key operations (write_key and friends) built on top of the verified
 * field/burn engine. They stage into a caller-owned write context; the caller
 * commits. This file starts with the precondition checks the higher-level
 * operations rely on.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "esp_loader.h"
#include "esp_loader_efuse.h"
#include "esp_loader_efuse_private.h"
#include "esp_targets.h"

/* ── BLK image bit helpers ──────────────────────────────────────────────────── */

bool efuse_blk_bit_is_set(const esp_loader_efuse_block_t *blk, uint16_t bit)
{
    uint8_t word = (uint8_t)(bit / 32u);
    uint8_t bit_in_word = bit % 32u;
    if (word >= blk->word_count) {
        return false;
    }
    return ((blk->words[word] >> bit_in_word) & 1u) != 0u;
}

static uint32_t blk_extract_field(const esp_loader_efuse_block_t *blk, uint16_t start, uint8_t width)
{
    uint32_t value = 0;
    for (uint8_t i = 0; i < width; i++) {
        if (efuse_blk_bit_is_set(blk, start + i)) {
            value |= (1u << i);
        }
    }
    return value;
}

/* ── Key purpose lookup ─────────────────────────────────────────────────────── */

const efuse_key_purpose_row_t *efuse_lookup_purpose(target_chip_t target,
        esp_loader_key_purpose_t purpose)
{
    const efuse_chip_layout_t *layout = efuse_get_chip_layout(target);
    if (layout == NULL || layout->purposes == NULL) {
        return NULL;
    }
    for (uint8_t i = 0; i < layout->purpose_count; i++) {
        if (layout->purposes[i].purpose == (uint8_t)purpose) {
            return &layout->purposes[i];
        }
    }
    return NULL;
}

esp_loader_error_t efuse_purpose_needs_read_protect(target_chip_t target,
        esp_loader_key_purpose_t purpose, bool *out)
{
    const efuse_key_purpose_row_t *row = efuse_lookup_purpose(target, purpose);
    if (row == NULL) {
        return ESP_LOADER_ERROR_INVALID_PARAM;
    }
    *out = (row->needs_rd_protect != 0u);
    return ESP_LOADER_SUCCESS;
}

esp_loader_error_t efuse_key_purpose_needs_reverse(target_chip_t target,
        esp_loader_key_purpose_t purpose, bool *out)
{
    const efuse_key_purpose_row_t *row = efuse_lookup_purpose(target, purpose);
    if (row == NULL) {
        return ESP_LOADER_ERROR_INVALID_PARAM;
    }
    *out = (row->needs_reverse != 0u);
    return ESP_LOADER_SUCCESS;
}

/* ── Key block availability ─────────────────────────────────────────────────── */

/* P4 v3.0+ adds a fifth KEY_PURPOSE bit per key (KEY_PURPOSE_n_H). It is not
 * next to the contiguous 4-bit field: KEY0–4 use BLK0 bit_start 155+key_idx,
 * KEY5 uses 164. WR_DIS for that H-bit is the same fuse as the low 4-bit field. */
static uint16_t p4_key_purpose_h_bit(uint8_t key_idx)
{
    return (key_idx == 5u) ? 164u : (uint16_t)(155u + key_idx);
}

/*
 * Reports whether a key block is free to receive a new key. Mirrors espefuse's
 * _key_block_is_unused: a block is free only when ALL of these hold —
 *   1. the block is writeable     (its WR_DIS bit is clear)
 *   2. the block is readable      (its RD_DIS bit is clear)
 *   3. KEY_PURPOSE_N is USER (0)  (incl. P4 H-bit clear)
 *   4. KEY_PURPOSE_N is writeable (its WR_DIS bit is clear)
 *   5. the block content is all zero on the chip
 *
 * Only KEY_PURPOSE-bearing chips (modern family) are handled; ESP32/C2 have no
 * KEY_PURPOSE and (for C2) a split per-half RD_DIS that this single-bit check
 * does not model, so they return ESP_LOADER_ERROR_UNSUPPORTED_FUNC. Passing a
 * non-key block returns ESP_LOADER_ERROR_INVALID_PARAM. On success *out_free is
 * set; a transport/read failure is reported via the return code.
 */
static esp_loader_error_t efuse_key_block_is_free(esp_loader_t *loader, uint8_t block,
        bool p4_h_present, bool *out_free)
{
    *out_free = false;

    const efuse_chip_layout_t *layout = efuse_get_chip_layout(loader->_target);
    if (layout == NULL || layout->key_purpose.purpose_width == 0) {
        return ESP_LOADER_ERROR_UNSUPPORTED_FUNC;
    }
    if (block >= layout->block_count) {
        return ESP_LOADER_ERROR_INVALID_PARAM;
    }

    const efuse_blk_layout_t *blk = &layout->blocks[block];
    uint8_t key_idx = blk->purpose_key_index;
    if (key_idx == EFUSE_INDEX_NONE) {
        return ESP_LOADER_ERROR_INVALID_PARAM; /* not a key block */
    }

    /* BLK0 holds WR_DIS / RD_DIS / KEY_PURPOSE; the target block holds the key. */
    esp_loader_efuse_block_t blk0;
    esp_loader_efuse_block_t data;
    RETURN_ON_ERROR(esp_loader_efuse_read_block(loader, 0, &blk0));
    RETURN_ON_ERROR(esp_loader_efuse_read_block(loader, block, &data));

    const efuse_key_purpose_field_t *kp = &layout->key_purpose;

    /* 1. Block writeable. */
    if (blk->wr_dis_bit != ESP_LOADER_EFUSE_BIT_NONE && efuse_blk_bit_is_set(&blk0, blk->wr_dis_bit)) {
        return ESP_LOADER_SUCCESS;
    }
    /* 2. Block readable. */
    if (blk->rd_dis_bit != ESP_LOADER_EFUSE_BIT_NONE && efuse_blk_bit_is_set(&blk0, blk->rd_dis_bit)) {
        return ESP_LOADER_SUCCESS;
    }
    /* 3. KEY_PURPOSE_N == USER (0), including P4's split H-bit. */
    uint16_t purpose_bit = (uint16_t)(kp->purpose0_bit + key_idx * kp->purpose_width);
    if (blk_extract_field(&blk0, purpose_bit, kp->purpose_width) != 0u) {
        return ESP_LOADER_SUCCESS;
    }
    if (loader->_target == ESP32P4_CHIP && p4_h_present
            && efuse_blk_bit_is_set(&blk0, p4_key_purpose_h_bit(key_idx))) {
        return ESP_LOADER_SUCCESS;
    }
    /* 4. KEY_PURPOSE_N writeable. */
    if (efuse_blk_bit_is_set(&blk0, kp->purpose0_wr_dis_bit + key_idx)) {
        return ESP_LOADER_SUCCESS;
    }
    /* 5. Block content all zero. */
    for (uint8_t w = 0; w < data.word_count; w++) {
        if (data.words[w] != 0u) {
            return ESP_LOADER_SUCCESS;
        }
    }

    *out_free = true;
    return ESP_LOADER_SUCCESS;
}

/* ── Key burning ────────────────────────────────────────────────────────────── */

/* Stage a run of bits at an absolute (block, bit) position by synthesising a
 * throwaway descriptor and reusing the validated write-field path. All write_key
 * multi-bit targets (key data, KEY_PURPOSE code) fit a single descriptor.
 * Single bits go through the public esp_loader_efuse_write_bit primitive. */
static esp_loader_error_t stage_bits(esp_loader_t *loader, esp_loader_efuse_ctx_t *ctx,
                                     uint8_t block, uint16_t bit_start, uint16_t bits,
                                     const uint8_t *src)
{
    const esp_loader_efuse_desc_t desc = { .efuse_block = block, .bit_start = bit_start, .bit_count = bits };
    const esp_loader_efuse_desc_t *field[] = { &desc, NULL };
    return esp_loader_efuse_write_field(loader, ctx, field, src, bits);
}

/* KEY_PURPOSE_5 (BLK9) cannot hold any reverse-purpose (XTS_AES / ECDSA) key on
 * chips with SOC_EFUSE_BLOCK9_KEY_PURPOSE_QUIRK (C3/C6/S3/H2; H4 when dispatched) —
 * a silicon bug. Other chips (S2/P4/C5/C61) have a KEY5 block but no bug, so KEY5
 * accepts these normally. IDF forbids XTS_AES_128 + XTS_AES_256_1/2 + ECDSA on
 * BLK9; on every quirk chip that set is exactly the chip's reverse-purposes, so
 * testing needs_reverse is equivalent and future-proof. */
static bool purpose5_quirk_violation(target_chip_t target, uint8_t key_idx,
                                     const efuse_key_purpose_row_t *info)
{
    if (key_idx != 5u) {
        return false;
    }
    switch (target) {
    case ESP32C3_CHIP:
    case ESP32C6_CHIP:
    case ESP32S3_CHIP:
    case ESP32H2_CHIP:
        return info->needs_reverse;
    default:
        return false;
    }
}

/* Resolve a key index (KEY_PURPOSE_N numbering) to its absolute block on this
 * chip via the layout, so no fixed-offset/consecutiveness is assumed. Returns
 * EFUSE_INDEX_NONE if the chip has no such key block. */
static uint8_t block_for_key_index(const efuse_chip_layout_t *layout, uint8_t key_idx)
{
    for (uint8_t b = 0; b < layout->block_count; b++) {
        if (layout->blocks[b].purpose_key_index == key_idx) {
            return b;
        }
    }
    return EFUSE_INDEX_NONE;
}

esp_loader_error_t esp_loader_efuse_write_key(esp_loader_t *loader,
        esp_loader_efuse_ctx_t *ctx, esp_loader_efuse_key_block_t key_block,
        esp_loader_key_purpose_t purpose, const uint8_t *key, size_t key_size,
        const esp_loader_efuse_write_key_config_t *config)
{
    esp_loader_efuse_write_key_config_t cfg = { false, false };
    if (config != NULL) {
        cfg = *config;
    }

    const efuse_chip_layout_t *layout = efuse_get_chip_layout(loader->_target);
    if (layout == NULL || layout->key_purpose.purpose_width == 0) {
        return ESP_LOADER_ERROR_UNSUPPORTED_FUNC; /* no KEY_PURPOSE (ESP32/C2) */
    }

    uint8_t key_idx = (uint8_t)key_block;
    uint8_t block = block_for_key_index(layout, key_idx);
    if (block == EFUSE_INDEX_NONE) {
        return ESP_LOADER_ERROR_INVALID_PARAM; /* chip has no such key block */
    }
    const efuse_blk_layout_t *blk = &layout->blocks[block];

    uint16_t block_bytes = blk->word_count * EFUSE_WORD_BYTES;
    if (key_size == 0u || key_size > block_bytes) {
        return ESP_LOADER_ERROR_INVALID_PARAM;
    }

    const efuse_key_purpose_row_t *info = efuse_lookup_purpose(loader->_target, purpose);
    if (info == NULL) {
        return ESP_LOADER_ERROR_INVALID_PARAM; /* purpose not supported on this chip */
    }
    if (purpose5_quirk_violation(loader->_target, key_idx, info)) {
        return ESP_LOADER_ERROR_INVALID_PARAM;
    }

    const efuse_key_purpose_field_t *kp = &layout->key_purpose;
    /* P4's contiguous KEY_PURPOSE field is 4 bits; anything that doesn't fit
     * needs the v3.0 split H-bit (ROM eco >= 5). */
    const bool purpose_needs_h = info->code >= (1u << kp->purpose_width);
    bool p4_h_present = false;
    if (loader->_target == ESP32P4_CHIP) {
        esp_loader_target_security_info_t sec;
        RETURN_ON_ERROR(esp_loader_get_security_info(loader, &sec));
        p4_h_present = sec.eco_version >= ESP32P4_ECO_REV3_MIN;
        if (purpose_needs_h && !p4_h_present) {
            return ESP_LOADER_ERROR_UNSUPPORTED_FUNC;
        }
    }

    bool is_free = false;
    RETURN_ON_ERROR(efuse_key_block_is_free(loader, block, p4_h_present, &is_free));
    if (!is_free) {
        return ESP_LOADER_ERROR_EFUSE_BLOCK_IN_USE;
    }

    /* Build a block-sized buffer, then reverse if the purpose needs it.
     * Short ECDSA keys use leading zero-pad before reverse (espefuse PEM:
     * 8×0 + 24-byte P192). Other purposes keep the key in the low bytes. */
    uint8_t buf[ESP_LOADER_EFUSE_MAX_WORDS_PER_BLOCK * EFUSE_WORD_BYTES] = { 0 };
    const bool ecdsa_key =
        (purpose == ESP_LOADER_KEY_PURPOSE_ECDSA_KEY_P256)
        || (purpose == ESP_LOADER_KEY_PURPOSE_ECDSA_KEY_P192)
        || (purpose == ESP_LOADER_KEY_PURPOSE_ECDSA_KEY_P384_L)
        || (purpose == ESP_LOADER_KEY_PURPOSE_ECDSA_KEY_P384_H);
    if (ecdsa_key && key_size < block_bytes) {
        memcpy(buf + (block_bytes - key_size), key, key_size);
    } else {
        memcpy(buf, key, key_size);
    }
    if (info->needs_reverse) {
        for (size_t i = 0; i < block_bytes / 2u; i++) {
            uint8_t tmp = buf[i];
            buf[i] = buf[block_bytes - 1u - i];
            buf[block_bytes - 1u - i] = tmp;
        }
    }

    uint16_t purpose_bit = (uint16_t)(kp->purpose0_bit + key_idx * kp->purpose_width);

    /* Key data into the key block; commit programs it before BLK0. */
    RETURN_ON_ERROR(stage_bits(loader, ctx, block, 0u, block_bytes * 8u, buf));
    /* Contiguous KEY_PURPOSE_N bits from the layout; if the code does not fit,
     * also set P4's non-contiguous H-bit. WR_DIS locks both (shared fuse). */
    RETURN_ON_ERROR(stage_bits(loader, ctx, 0u, purpose_bit, kp->purpose_width, &info->code));
    if (p4_h_present && purpose_needs_h) {
        RETURN_ON_ERROR(esp_loader_efuse_write_bit(loader, ctx, 0u, p4_key_purpose_h_bit(key_idx)));
    }
    RETURN_ON_ERROR(esp_loader_efuse_write_bit(loader, ctx, 0u, kp->purpose0_wr_dis_bit + key_idx));
    /* Key-block write protection (unless opted out). */
    if (!cfg.no_write_protect && blk->wr_dis_bit != ESP_LOADER_EFUSE_BIT_NONE) {
        RETURN_ON_ERROR(esp_loader_efuse_write_bit(loader, ctx, 0u, blk->wr_dis_bit));
    }
    /* Key-block read protection (when the purpose needs it and not opted out). */
    if (info->needs_rd_protect && !cfg.no_read_protect && blk->rd_dis_bit != ESP_LOADER_EFUSE_BIT_NONE) {
        RETURN_ON_ERROR(esp_loader_efuse_write_bit(loader, ctx, 0u, blk->rd_dis_bit));
    }

    return ESP_LOADER_SUCCESS;
}

/* ── Legacy (no-KEY_PURPOSE) key burning: ESP32 & C2 ─────────────────────────── */

/* C2 BLK0 bit for XTS_KEY_LENGTH_256 (efuse_defs esp32c2.yaml: start 42). */
#define C2_XTS_KEY_LENGTH_256_BIT 42u

/* Reject staging over an in-use block: the block must be writeable, and the
 * target's own words must be all-zero on the chip. Mirrors the spirit of
 * efuse_key_block_is_free without the KEY_PURPOSE checks (legacy chips have
 * none). *out_free is set on success. */
static esp_loader_error_t efuse_key_block_is_free_legacy(esp_loader_t *loader,
        const efuse_blk_layout_t *blk, uint8_t block,
        uint8_t word_offset, uint8_t data_words, bool *out_free)
{
    *out_free = false;

    esp_loader_efuse_block_t blk0;
    esp_loader_efuse_block_t data;
    RETURN_ON_ERROR(esp_loader_efuse_read_block(loader, 0, &blk0));
    RETURN_ON_ERROR(esp_loader_efuse_read_block(loader, block, &data));

    if (blk->wr_dis_bit != ESP_LOADER_EFUSE_BIT_NONE && efuse_blk_bit_is_set(&blk0, blk->wr_dis_bit)) {
        return ESP_LOADER_SUCCESS; /* not writeable */
    }
    for (uint8_t w = word_offset; w < word_offset + data_words && w < data.word_count; w++) {
        if (data.words[w] != 0u) {
            return ESP_LOADER_SUCCESS; /* already holds data */
        }
    }

    *out_free = true;
    return ESP_LOADER_SUCCESS;
}

/* Per-target facts, indexed by esp_loader_legacy_key_target_t. For C2,
 * [first_half, last_half] selects the key-half range from efuse_get_key_halves
 * (whole = 0..1, low = 0..0, high = 1..1); ESP32 ignores them and uses the whole
 * block. Geometry (words, RD_DIS bits) is chip state, resolved from the layout /
 * half table at run time, so it is not part of this table. */
typedef struct {
    target_chip_t chip;
    uint8_t block;
    uint8_t first_half;
    uint8_t last_half;
    bool reverse;
    bool read_protect;
    bool require_coding_none;
    uint16_t side_effect_bit;
} legacy_key_target_info_t;

static const legacy_key_target_info_t legacy_key_targets[] = {
    [ESP_LOADER_LEGACY_KEY_ESP32_FLASH_ENCRYPTION] = {
        .chip = ESP32_CHIP, .block = 1u,
        .reverse = true, .read_protect = true,
        .side_effect_bit = ESP_LOADER_EFUSE_BIT_NONE,
    },
    [ESP_LOADER_LEGACY_KEY_ESP32_SECURE_BOOT_V1] = {
        .chip = ESP32_CHIP, .block = 2u,
        .reverse = true, .read_protect = true,
        .side_effect_bit = ESP_LOADER_EFUSE_BIT_NONE,
    },
    /* RSA public-key digest in BLK2: public (not reversed, not read-protected),
     * NONE coding only; ESP32 v3.0+ (checked below). */
    [ESP_LOADER_LEGACY_KEY_ESP32_SECURE_BOOT_V2] = {
        .chip = ESP32_CHIP, .block = 2u,
        .reverse = false, .read_protect = false,
        .require_coding_none = true,
        .side_effect_bit = ESP_LOADER_EFUSE_BIT_NONE,
    },
    [ESP_LOADER_LEGACY_KEY_C2_XTS_AES_128] = {
        .chip = ESP32C2_CHIP, .block = 3u,
        .first_half = 0u, .last_half = 1u,
        .reverse = true, .read_protect = true,
        .side_effect_bit = C2_XTS_KEY_LENGTH_256_BIT,
    },
    [ESP_LOADER_LEGACY_KEY_C2_XTS_AES_128_DERIVED] = {
        .chip = ESP32C2_CHIP, .block = 3u,
        .first_half = 0u, .last_half = 0u,
        .reverse = true, .read_protect = true,
        .side_effect_bit = ESP_LOADER_EFUSE_BIT_NONE,
    },
    [ESP_LOADER_LEGACY_KEY_C2_SECURE_BOOT_DIGEST] = {
        .chip = ESP32C2_CHIP, .block = 3u,
        .first_half = 1u, .last_half = 1u,
        .reverse = false, .read_protect = false,
        .side_effect_bit = ESP_LOADER_EFUSE_BIT_NONE,
    },
};

/* Which words of the block the key occupies and which RD_DIS bit(s) protect
 * them. Chip state, resolved at run time from the coding scheme / key-half
 * table, so it is not part of legacy_key_targets[]. */
typedef struct {
    uint8_t word_offset;
    uint8_t data_words;
    uint16_t rd_dis_bits[2];   /* unused slots stay ESP_LOADER_EFUSE_BIT_NONE; C2 whole key = both halves */
} legacy_key_geometry_t;

/* Resolve the geometry above for one target. Also rejects a target that cannot
 * live under the block's coding scheme (Secure Boot V2 digest under 3/4), which
 * is a policy check rather than geometry, but the coding scheme is only known
 * here. Both failures return ESP_LOADER_ERROR_UNSUPPORTED_FUNC. */
static esp_loader_error_t legacy_key_resolve_geometry(esp_loader_t *loader,
        const efuse_chip_layout_t *layout, const efuse_blk_layout_t *blk,
        const legacy_key_target_info_t *info, legacy_key_geometry_t *out)
{
    out->word_offset = 0u;
    out->data_words = 0u;
    out->rd_dis_bits[0] = ESP_LOADER_EFUSE_BIT_NONE;
    out->rd_dis_bits[1] = ESP_LOADER_EFUSE_BIT_NONE;

    if (info->chip == ESP32_CHIP) {
        /* Whole block; usable size follows the coding scheme (3/4 stores 6
         * real-data words in the 8-word block, encoded by the burn path). */
        out->data_words = blk->word_count;
        out->rd_dis_bits[0] = blk->rd_dis_bit;
        efuse_coding_t coding;
        uint32_t base = esp_targets_get_efuse_base(loader->_target);
        RETURN_ON_ERROR(efuse_get_block_coding(loader, loader->_target, layout, base, info->block, &coding));
        if (coding == EFUSE_CODING_PARITY) {
            /* A 32-byte Secure Boot V2 digest cannot fit a 3/4-coded (24-byte)
             * block; espefuse rejects burn-key-digest under any non-NONE scheme. */
            if (info->require_coding_none) {
                return ESP_LOADER_ERROR_UNSUPPORTED_FUNC;
            }
            out->data_words = 6u;
        }
    } else {
        /* C2: sum the selected key-half range */
        uint8_t half_count = 0u;
        const efuse_key_half_t *halves = efuse_get_key_halves(loader->_target, &half_count);
        if (halves == NULL || info->last_half >= half_count) {
            return ESP_LOADER_ERROR_UNSUPPORTED_FUNC;
        }
        out->word_offset = halves[info->first_half].word_offset;
        for (uint8_t h = info->first_half; h <= info->last_half; h++) {
            out->data_words = (uint8_t)(out->data_words + halves[h].word_count);
            out->rd_dis_bits[h - info->first_half] = halves[h].rd_dis_bit;
        }
    }

    return ESP_LOADER_SUCCESS;
}

esp_loader_error_t esp_loader_efuse_write_key_legacy(esp_loader_t *loader,
        esp_loader_efuse_ctx_t *ctx, esp_loader_legacy_key_target_t target,
        const uint8_t *key, size_t key_size,
        const esp_loader_efuse_write_key_config_t *config)
{
    esp_loader_efuse_write_key_config_t cfg = { false, false };
    if (config != NULL) {
        cfg = *config;
    }

    if (target >= sizeof(legacy_key_targets) / sizeof(legacy_key_targets[0])) {
        return ESP_LOADER_ERROR_INVALID_PARAM;
    }
    const legacy_key_target_info_t *info = &legacy_key_targets[target];

    if (info->chip != loader->_target) {
        return ESP_LOADER_ERROR_INVALID_PARAM; /* target not for the detected chip */
    }
    if (target == ESP_LOADER_LEGACY_KEY_ESP32_SECURE_BOOT_V2) {
        /* espefuse burn_key_digest: reject pre-ECO3 (revision < 300). */
        uint16_t rev_x100;
        RETURN_ON_ERROR(loader_read_chip_revision(loader, ESP32_CHIP, &rev_x100));
        if (rev_x100 < 300u) {
            return ESP_LOADER_ERROR_UNSUPPORTED_FUNC;
        }
    }
    const efuse_chip_layout_t *layout = efuse_get_chip_layout(loader->_target);
    if (layout == NULL || info->block >= layout->block_count) {
        return ESP_LOADER_ERROR_UNSUPPORTED_FUNC;
    }
    const efuse_blk_layout_t *blk = &layout->blocks[info->block];

    legacy_key_geometry_t geom;
    RETURN_ON_ERROR(legacy_key_resolve_geometry(loader, layout, blk, info, &geom));

    uint16_t data_bytes = geom.data_words * EFUSE_WORD_BYTES;
    if (key_size == 0u || key_size > data_bytes) {
        return ESP_LOADER_ERROR_INVALID_PARAM;
    }

    bool is_free = false;
    RETURN_ON_ERROR(efuse_key_block_is_free_legacy(loader, blk, info->block, geom.word_offset, geom.data_words, &is_free));
    if (!is_free) {
        return ESP_LOADER_ERROR_EFUSE_BLOCK_IN_USE;
    }

    /* Build a target-sized buffer: key in the low bytes, zero-padded. For
     * reverse targets reverse the whole buffer (espefuse on a padded file). */
    uint8_t buf[ESP_LOADER_EFUSE_MAX_WORDS_PER_BLOCK * EFUSE_WORD_BYTES] = { 0 };
    memcpy(buf, key, key_size);
    if (info->reverse) {
        for (uint16_t i = 0; i < data_bytes / 2u; i++) {
            uint8_t tmp = buf[i];
            buf[i] = buf[data_bytes - 1u - i];
            buf[data_bytes - 1u - i] = tmp;
        }
    }

    /* Key data into the block; commit programs it (and any coding) before BLK0. */
    RETURN_ON_ERROR(stage_bits(loader, ctx, info->block, geom.word_offset * 32u, data_bytes * 8u, buf));

    /* Structural side-effect (C2 whole key sets XTS_KEY_LENGTH_256). */
    if (info->side_effect_bit != ESP_LOADER_EFUSE_BIT_NONE) {
        RETURN_ON_ERROR(esp_loader_efuse_write_bit(loader, ctx, 0u, info->side_effect_bit));
    }
    /* Write protection (unless opted out). */
    if (!cfg.no_write_protect && blk->wr_dis_bit != ESP_LOADER_EFUSE_BIT_NONE) {
        RETURN_ON_ERROR(esp_loader_efuse_write_bit(loader, ctx, 0u, blk->wr_dis_bit));
    }
    /* Read protection (when the target needs it and not opted out); for the C2
     * whole key that is both halves' bits. */
    if (info->read_protect && !cfg.no_read_protect) {
        for (uint8_t i = 0; i < sizeof(geom.rd_dis_bits) / sizeof(geom.rd_dis_bits[0]); i++) {
            if (geom.rd_dis_bits[i] != ESP_LOADER_EFUSE_BIT_NONE) {
                RETURN_ON_ERROR(esp_loader_efuse_write_bit(loader, ctx, 0u, geom.rd_dis_bits[i]));
            }
        }
    }

    return ESP_LOADER_SUCCESS;
}
