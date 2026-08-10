/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_loader.h"
#include "esp_loader_efuse.h"
#include "esp_loader_efuse_private.h"
#include "esp_targets.h"

esp_loader_error_t esp_loader_efuse_read_block(
    esp_loader_t *loader,
    uint8_t block,
    esp_loader_efuse_block_t *out)
{
    target_chip_t target = loader->_target;
    uint32_t base = esp_targets_get_efuse_base(target);

    const efuse_chip_layout_t *layout = efuse_get_chip_layout(target);
    if (layout == NULL) {
        return ESP_LOADER_ERROR_UNSUPPORTED_FUNC;
    }
    if (block >= layout->block_count) {
        return ESP_LOADER_ERROR_INVALID_PARAM;
    }

    const efuse_blk_layout_t *b = &layout->blocks[block];
    if (b->word_count > ESP_LOADER_EFUSE_MAX_WORDS_PER_BLOCK) {
        return ESP_LOADER_ERROR_INVALID_TARGET;
    }

    memset(out, 0, sizeof(*out));
    out->block = block;
    out->word_count = b->word_count;

    for (uint8_t w = 0; w < b->word_count; w++) {
        uint32_t addr = base + b->read_offset + (uint32_t)w * EFUSE_WORD_BYTES;
        esp_loader_error_t err = esp_loader_read_register(loader, addr, &out->words[w]);
        if (err != ESP_LOADER_SUCCESS) {
            return err;
        }
    }

    /* BLK0 itself is never RD_DIS-maskable on any chip. */
    if (block != 0u) {
        esp_loader_efuse_block_t blk0;
        RETURN_ON_ERROR(esp_loader_efuse_read_block(loader, 0u, &blk0));

        if (b->rd_dis_bit != ESP_LOADER_EFUSE_BIT_NONE && efuse_blk_bit_is_set(&blk0, b->rd_dis_bit)) {
            out->read_protected_word_mask = (uint16_t)((1u << b->word_count) - 1u);
        }
        uint8_t half_count = 0u;
        const efuse_key_half_t *halves = efuse_get_key_halves(target, &half_count);
        for (uint8_t h = 0; h < half_count; h++) {
            if (halves[h].block == block && efuse_blk_bit_is_set(&blk0, halves[h].rd_dis_bit)) {
                out->read_protected_word_mask |= (uint16_t)(((1u << halves[h].word_count) - 1u)
                                                 << halves[h].word_offset);
            }
        }
    }

    return ESP_LOADER_SUCCESS;
}

esp_loader_error_t esp_loader_efuse_read_image(
    esp_loader_t *loader,
    esp_loader_efuse_image_t *out)
{
    const efuse_chip_layout_t *layout = efuse_get_chip_layout(loader->_target);
    if (layout == NULL) {
        return ESP_LOADER_ERROR_UNSUPPORTED_FUNC;
    }
    if (layout->block_count > ESP_LOADER_EFUSE_MAX_BLOCKS) {
        return ESP_LOADER_ERROR_INVALID_TARGET;
    }

    memset(out, 0, sizeof(*out));
    out->block_count = layout->block_count;

    for (uint8_t block = 0; block < out->block_count; block++) {
        esp_loader_error_t err = esp_loader_efuse_read_block(loader, block, &out->blocks[block]);
        if (err != ESP_LOADER_SUCCESS) {
            return err;
        }
    }

    return ESP_LOADER_SUCCESS;
}
