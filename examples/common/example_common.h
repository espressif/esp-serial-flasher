/*
 * SPDX-FileCopyrightText: 2020-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_loader.h"

// Standard flash addresses that are the same for all chips
#define PARTITION_TABLE_ADDRESS  0x8000
#define APPLICATION_ADDRESS      0x10000

esp_loader_error_t connect_to_target(esp_loader_t *loader, uint32_t higher_transmission_rate);
esp_loader_error_t connect_to_target_with_stub(esp_loader_t *loader,
        uint32_t current_transmission_rate,
        uint32_t higher_transmission_rate);
esp_loader_error_t flash_binary(esp_loader_t *loader, const uint8_t *bin, size_t size,
                                size_t address);
esp_loader_error_t load_ram_binary(esp_loader_t *loader, const uint8_t *bin);

// Address helper functions
uint32_t get_bootloader_address(target_chip_t chip);
