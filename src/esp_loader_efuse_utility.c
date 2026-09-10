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

/* ── Field read ────────────────────────────────────────────────────────────── */

esp_loader_error_t esp_loader_efuse_read_field(
    esp_loader_t *loader,
    const esp_loader_efuse_desc_t *field[],
    uint8_t *dst,
    size_t dst_size_bits)
{
    const efuse_chip_layout_t *layout = efuse_get_chip_layout(loader->_target);
    if (layout == NULL) {
        return ESP_LOADER_ERROR_UNSUPPORTED_FUNC;
    }

    uint32_t base = esp_targets_get_efuse_base(loader->_target);
    memset(dst, 0, (dst_size_bits + 7u) / 8u);
    size_t bit_counter = 0;

    for (size_t i = 0; field[i] != NULL && bit_counter < dst_size_bits; i++) {
        const esp_loader_efuse_desc_t *d = field[i];

        if (d->efuse_block >= layout->block_count) {
            return ESP_LOADER_ERROR_INVALID_PARAM;
        }

        uint16_t bit_start = d->bit_start;
        uint16_t remaining = d->bit_count;

        while (remaining > 0 && bit_counter < dst_size_bits) {
            uint32_t word_num    = bit_start / 32u;
            uint32_t bit_in_word = bit_start % 32u;
            uint32_t chunk       = 32u - bit_in_word;

            uint32_t bits_left = (uint32_t)(dst_size_bits - bit_counter);
            if (chunk > remaining) {
                chunk = remaining;
            }
            if (chunk > bits_left) {
                chunk = bits_left;
            }
            if (word_num >= layout->blocks[d->efuse_block].word_count) {
                return ESP_LOADER_ERROR_INVALID_PARAM;
            }

            uint32_t addr = base + layout->blocks[d->efuse_block].read_offset + word_num * EFUSE_WORD_BYTES;
            uint32_t word_val = 0;
            esp_loader_error_t err = esp_loader_read_register(loader, addr, &word_val);
            if (err != ESP_LOADER_SUCCESS) {
                return err;
            }

            uint32_t mask = (chunk == 32u) ? 0xFFFFFFFFu : ((1u << chunk) - 1u);
            uint32_t bits = (word_val >> bit_in_word) & mask;

            /* Scatter each extracted bit into its position in the caller's byte array. */
            for (uint32_t b = 0; b < chunk; b++) {
                if ((bits >> b) & 1u) {
                    dst[(bit_counter + b) / 8u] |= 1u << ((bit_counter + b) % 8u);
                }
            }
            bit_counter += chunk;
            bit_start   += chunk;
            remaining   -= chunk;
        }
    }

    return ESP_LOADER_SUCCESS;
}

/* ── Field write (staged) ──────────────────────────────────────────────────── */

esp_loader_error_t esp_loader_efuse_write_field(
    esp_loader_t *loader,
    esp_loader_efuse_ctx_t *ctx,
    const esp_loader_efuse_desc_t *field[],
    const uint8_t *src,
    size_t src_size_bits)
{
    const efuse_chip_layout_t *layout = efuse_get_chip_layout(loader->_target);
    if (layout == NULL) {
        return ESP_LOADER_ERROR_UNSUPPORTED_FUNC;
    }

    size_t bit_counter = 0;

    for (size_t i = 0; field[i] != NULL && bit_counter < src_size_bits; i++) {
        const esp_loader_efuse_desc_t *d = field[i];

        if (d->efuse_block >= layout->block_count) {
            return ESP_LOADER_ERROR_INVALID_PARAM;
        }

        uint16_t bit_start = d->bit_start;
        uint16_t remaining = d->bit_count;

        while (remaining > 0 && bit_counter < src_size_bits) {
            uint32_t word_num    = bit_start / 32u;
            uint32_t bit_in_word = bit_start % 32u;
            uint32_t chunk       = 32u - bit_in_word;

            uint32_t bits_left = (uint32_t)(src_size_bits - bit_counter);
            if (chunk > remaining) {
                chunk = remaining;
            }
            if (chunk > bits_left) {
                chunk = bits_left;
            }
            if (word_num >= layout->blocks[d->efuse_block].word_count) {
                return ESP_LOADER_ERROR_INVALID_PARAM;
            }

            uint32_t src_bits = 0;
            for (uint32_t b = 0; b < chunk; b++) {
                size_t pos = bit_counter + b;
                if (src[pos / 8u] & (1u << (pos % 8u))) {
                    src_bits |= 1u << b;
                }
            }
            ctx->_buf[d->efuse_block * ESP_LOADER_EFUSE_MAX_WORDS_PER_BLOCK + word_num] |= src_bits << bit_in_word;

            bit_counter += chunk;
            bit_start   += chunk;
            remaining   -= chunk;
        }
    }

    return ESP_LOADER_SUCCESS;
}

esp_loader_error_t esp_loader_efuse_write_field_bit(
    esp_loader_t *loader,
    esp_loader_efuse_ctx_t *ctx,
    const esp_loader_efuse_desc_t *field[])
{
    uint8_t one = 1u;
    return esp_loader_efuse_write_field(loader, ctx, field, &one, 1u);
}

esp_loader_error_t esp_loader_efuse_write_bit(
    esp_loader_t *loader,
    esp_loader_efuse_ctx_t *ctx,
    uint8_t block,
    uint16_t bit_num)
{
    const esp_loader_efuse_desc_t desc = { .efuse_block = block, .bit_start = bit_num, .bit_count = 1u };
    const esp_loader_efuse_desc_t *field[] = { &desc, NULL };
    return esp_loader_efuse_write_field_bit(loader, ctx, field);
}

/* ── Inspection helper (test/debug only) ───────────────────────────────────── */

const uint32_t *efuse_get_write_buf(const esp_loader_efuse_ctx_t *ctx, uint8_t block)
{
    if (block >= ESP_LOADER_EFUSE_MAX_BLOCKS) {
        return NULL;
    }
    return &ctx->_buf[block * ESP_LOADER_EFUSE_MAX_WORDS_PER_BLOCK];
}

/* ── Coding-scheme classification ──────────────────────────────────────────── */

esp_loader_error_t efuse_get_block_coding(
    esp_loader_t *loader,
    target_chip_t target,
    const efuse_chip_layout_t *layout,
    uint32_t base,
    uint8_t block,
    efuse_coding_t *coding)
{
    if (block == 0u) {
        /* BLOCK0 is always CODING_SCHEME_NONE, modern chips and ESP32 alike. */
        *coding = EFUSE_CODING_NONE;
        return ESP_LOADER_SUCCESS;
    }

    if (target == ESP32_CHIP) {
        uint8_t scheme;
        RETURN_ON_ERROR(esp32_read_coding_scheme(loader, base, layout, &scheme));
        *coding = (scheme == ESP32_CODING_SCHEME_NONE) ? EFUSE_CODING_NONE : EFUSE_CODING_PARITY;
        return ESP_LOADER_SUCCESS;
    }

    /* Modern chips: BLK1+ is always RS-coded, fixed at compile time. */
    *coding = EFUSE_CODING_PARITY;
    return ESP_LOADER_SUCCESS;
}

/* ── Pre-commit validation ─────────────────────────────────────────────────── */

esp_loader_error_t efuse_validate_staged_writes(
    esp_loader_t *loader,
    esp_loader_efuse_ctx_t *ctx)
{
    target_chip_t target = loader->_target;

    const efuse_chip_layout_t *layout = efuse_get_chip_layout(target);
    if (layout == NULL) {
        return ESP_LOADER_ERROR_UNSUPPORTED_FUNC;
    }

    uint32_t base = esp_targets_get_efuse_base(target);

    for (uint8_t block = 0; block < layout->block_count; block++) {
        uint8_t   word_count = layout->blocks[block].word_count;
        uint32_t *buf        = &ctx->_buf[block * ESP_LOADER_EFUSE_MAX_WORDS_PER_BLOCK];

        bool has_staged = false;
        for (uint8_t w = 0; w < word_count; w++) {
            if (buf[w] != 0u) {
                has_staged = true;
                break;
            }
        }
        if (!has_staged) {
            continue;
        }

        uint32_t chip[ESP_LOADER_EFUSE_MAX_WORDS_PER_BLOCK] = {0};
        for (uint8_t w = 0; w < word_count; w++) {
            uint32_t addr = base + layout->blocks[block].read_offset + w * EFUSE_WORD_BYTES;
            esp_loader_error_t err = esp_loader_read_register(loader, addr, &chip[w]);
            if (err != ESP_LOADER_SUCCESS) {
                return err;
            }
        }

        efuse_coding_t coding;
        RETURN_ON_ERROR(efuse_get_block_coding(loader, target, layout, base, block, &coding));

        if (coding == EFUSE_CODING_NONE) {
            /* OTP, OR-only staging: any new 1-bit is always valid, cannot
             * produce a 1→0 transition. */
        } else {
            /* Parity-coded (RS on modern chips, 3/4 on ESP32): must be empty
             * on chip, or already identical. */
            bool already_written = true;
            for (uint8_t w = 0; w < word_count; w++) {
                if (chip[w] != buf[w]) {
                    already_written = false;
                    break;
                }
            }
            if (already_written) {
                memset(buf, 0, word_count * sizeof(buf[0]));
                continue;
            }

            for (uint8_t w = 0; w < word_count; w++) {
                if (chip[w] != 0u) {
                    return ESP_LOADER_ERROR_EFUSE_BLOCK_IN_USE;
                }
            }
        }
    }

    return ESP_LOADER_SUCCESS;
}
