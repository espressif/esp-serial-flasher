/*
 * SPDX-FileCopyrightText: 2026 FEmbed
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "esp_loader.h"

static esp_loader_error_t p4_update_register(esp_loader_t *loader, uint32_t address,
        uint32_t clear, uint32_t set)
{
    uint32_t value;
    RETURN_ON_ERROR(esp_loader_read_register(loader, address, &value));
    return esp_loader_write_register(loader, address, (value & ~clear) | set);
}

static esp_loader_error_t loader_prepare_p4_flash_power(esp_loader_t *loader)
{
    if (loader->_target != ESP32P4_CHIP || loader->_protocol_type != ESP_LOADER_PROTOCOL_SERIAL) {
        return ESP_LOADER_SUCCESS;
    }
    esp_loader_target_security_info_t info;
    RETURN_ON_ERROR(esp_loader_get_security_info(loader, &info));
    // ECO6/7 are v3.1/v3.2. Secure download mode leaves register access to ROM.
    if (info.secure_download_mode_enabled || (info.eco_version != 6 && info.eco_version != 7)) {
        return ESP_LOADER_SUCCESS;
    }
    const uint32_t efuse_repeat1 = 0x5012d034;
    const uint32_t pad_power = 0x5011010c;
    const uint32_t ldo_analog = 0x501151bc;
    const uint32_t ldo_control = 0x501151b8;
    const uint32_t pmu_date = 0x501153fc;
    if (info.eco_version == 7) {
        uint32_t efuse;
        RETURN_ON_ERROR(esp_loader_read_register(loader, efuse_repeat1, &efuse));
        if (efuse & (1U << 16)) {
            // Release force-on when ROM powered the flash, including repeated UART resets.
            return esp_loader_write_register(loader, pmu_date, 0);
        }
    }
    // Match esptool power_on_flash; retain the eFuse voltage without burning eFuses.
    RETURN_ON_ERROR(esp_loader_write_register(loader, pad_power, 1));
    loader->_port->ops->delay_ms(loader->_port, 10);
    RETURN_ON_ERROR(p4_update_register(loader, ldo_analog, 0, 1U << 27));
    RETURN_ON_ERROR(p4_update_register(loader, ldo_control, 0, 1U << 7));
    RETURN_ON_ERROR(p4_update_register(loader, pmu_date, 0, 3));
    // Round up to the port's millisecond resolution for the 50 us / 1.8 ms waits.
    loader->_port->ops->delay_ms(loader->_port, 1);
    RETURN_ON_ERROR(p4_update_register(loader, ldo_analog, 1U << 27, 0));
    RETURN_ON_ERROR(p4_update_register(loader, ldo_control, 0xffU << 23, 0));
    RETURN_ON_ERROR(p4_update_register(loader, ldo_control, 0, 1U << 7));
    RETURN_ON_ERROR(p4_update_register(loader, ldo_control, 1U << 7, 0));
    loader->_port->ops->delay_ms(loader->_port, 2);
    return ESP_LOADER_SUCCESS;
}
