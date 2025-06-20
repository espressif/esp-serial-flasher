/* Copyright 2020 Espressif Systems (Shanghai) PTE LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "esp_targets.h"
#include <stddef.h>

typedef esp_loader_error_t (*read_spi_config_t)(uint32_t efuse_base, uint32_t *spi_config);

typedef struct {
    target_registers_t regs;
    uint32_t efuse_base;
    const uint32_t *chip_magic_value;  // Pointer to array of magic values
    uint8_t magic_values_count;        // Number of valid magic values for this chip
    uint32_t mac_efuse_offset;
    uint32_t chip_id;
    read_spi_config_t read_spi_config;
    bool encryption_in_begin_flash_cmd;
} esp_target_t;

// This ROM address has a different value on each chip model
#define CHIP_DETECT_MAGIC_REG_ADDR 0x40001000

#define ESP8266_SPI_REG_BASE 0x60000200
#define ESP32S2_SPI_REG_BASE 0x3f402000
#define ESP32H2_SPI_REG_BASE 0x60003000
#define ESP32C5_SPI_REG_BASE 0x60003000
#define ESP32C6_SPI_REG_BASE 0x60003000
#define ESP32P4_SPI_REG_BASE 0x5008d000
#define ESP32xx_SPI_REG_BASE 0x60002000
#define ESP32_SPI_REG_BASE   0x3ff42000

#define CHIP_ID_NONE 0xFF

// Used for ESP32P4 chip detection, other chips uses ROM magic value
#define ESP32P4_SPI_DATE_REG 0x500d0000
#define ESP32P4_SPI_DATE_REG_MASK 0x7FFFFFF
#define ESP32P4_SPI_DATE_REG_VALUE 0x2207202

static esp_loader_error_t spi_config_esp32(uint32_t efuse_base, uint32_t *spi_config);
static esp_loader_error_t spi_config_esp32xx(uint32_t efuse_base, uint32_t *spi_config);
static esp_loader_error_t spi_config_unsupported(uint32_t efuse_base, uint32_t *spi_config);

static const esp_target_t esp_target[ESP_MAX_CHIP] = {

    // ESP8266
    {
        .regs = {
            .cmd  = ESP8266_SPI_REG_BASE + 0x00,
            .usr  = ESP8266_SPI_REG_BASE + 0x1c,
            .usr1 = ESP8266_SPI_REG_BASE + 0x20,
            .usr2 = ESP8266_SPI_REG_BASE + 0x24,
            .w0   = ESP8266_SPI_REG_BASE + 0x40,
            .mosi_dlen  = 0,
            .miso_dlen  = 0,
        },
        .efuse_base = 0,            // Not used
        .chip_magic_value = (const uint32_t[]){ 0xfff0c101 },
        .magic_values_count = 1,
        .read_spi_config = NULL,    // Not used
        .mac_efuse_offset = 0, // Not used
        .encryption_in_begin_flash_cmd = false,
        .chip_id = CHIP_ID_NONE,
    },

    // ESP32
    {
        .regs = {
            .cmd  = ESP32_SPI_REG_BASE + 0x00,
            .usr  = ESP32_SPI_REG_BASE + 0x1c,
            .usr1 = ESP32_SPI_REG_BASE + 0x20,
            .usr2 = ESP32_SPI_REG_BASE + 0x24,
            .w0   = ESP32_SPI_REG_BASE + 0x80,
            .mosi_dlen = ESP32_SPI_REG_BASE + 0x28,
            .miso_dlen = ESP32_SPI_REG_BASE + 0x2c,
        },
        .efuse_base = 0x3ff5A000,
        .chip_magic_value = (const uint32_t[]){ 0x00f01d83 },
        .magic_values_count = 1,
        .read_spi_config = spi_config_esp32,
        .mac_efuse_offset = 0x04,
        .encryption_in_begin_flash_cmd = false,
        .chip_id = 0,
    },

    // ESP32S2
    {
        .regs = {
            .cmd  = ESP32S2_SPI_REG_BASE + 0x00,
            .usr  = ESP32S2_SPI_REG_BASE + 0x18,
            .usr1 = ESP32S2_SPI_REG_BASE + 0x1c,
            .usr2 = ESP32S2_SPI_REG_BASE + 0x20,
            .w0   = ESP32S2_SPI_REG_BASE + 0x58,
            .mosi_dlen = ESP32S2_SPI_REG_BASE + 0x24,
            .miso_dlen = ESP32S2_SPI_REG_BASE + 0x28,
        },
        .efuse_base = 0x3f41A000,
        .chip_magic_value = (const uint32_t[]){ 0x000007c6 },
        .magic_values_count = 1,
        .read_spi_config = spi_config_esp32xx,
        .mac_efuse_offset = 0x44,
        .encryption_in_begin_flash_cmd = true,
        .chip_id = 2,
    },

    // ESP32C3
    {
        .regs = {
            .cmd  = ESP32xx_SPI_REG_BASE + 0x00,
            .usr  = ESP32xx_SPI_REG_BASE + 0x18,
            .usr1 = ESP32xx_SPI_REG_BASE + 0x1c,
            .usr2 = ESP32xx_SPI_REG_BASE + 0x20,
            .w0   = ESP32xx_SPI_REG_BASE + 0x58,
            .mosi_dlen = ESP32xx_SPI_REG_BASE + 0x24,
            .miso_dlen = ESP32xx_SPI_REG_BASE + 0x28,
        },
        .efuse_base = 0x60008800,
        .chip_magic_value = (const uint32_t[]){ 0x6921506f, 0x1b31506f, 0x4881606F, 0x4361606F },
        .magic_values_count = 4,
        .read_spi_config = spi_config_esp32xx,
        .mac_efuse_offset = 0x44,
        .encryption_in_begin_flash_cmd = true,
        .chip_id = 5,
    },

    // ESP32S3
    {
        .regs = {
            .cmd  = ESP32xx_SPI_REG_BASE + 0x00,
            .usr  = ESP32xx_SPI_REG_BASE + 0x18,
            .usr1 = ESP32xx_SPI_REG_BASE + 0x1c,
            .usr2 = ESP32xx_SPI_REG_BASE + 0x20,
            .w0   = ESP32xx_SPI_REG_BASE + 0x58,
            .mosi_dlen = ESP32xx_SPI_REG_BASE + 0x24,
            .miso_dlen = ESP32xx_SPI_REG_BASE + 0x28,
        },
        .efuse_base = 0x60007000,
        .chip_magic_value = (const uint32_t[]){ 0x00000009 },
        .magic_values_count = 1,
        .read_spi_config = spi_config_esp32xx,
        .mac_efuse_offset = 0x44,
        .encryption_in_begin_flash_cmd = true,
        .chip_id = 9,
    },

    // ESP32C2
    {
        .regs = {
            .cmd  = ESP32xx_SPI_REG_BASE + 0x00,
            .usr  = ESP32xx_SPI_REG_BASE + 0x18,
            .usr1 = ESP32xx_SPI_REG_BASE + 0x1c,
            .usr2 = ESP32xx_SPI_REG_BASE + 0x20,
            .w0   = ESP32xx_SPI_REG_BASE + 0x58,
            .mosi_dlen = ESP32xx_SPI_REG_BASE + 0x24,
            .miso_dlen = ESP32xx_SPI_REG_BASE + 0x28,
        },
        .efuse_base = 0x60008800,
        .chip_magic_value = (const uint32_t[]){ 0x6f51306f, 0x7c41a06f },
        .magic_values_count = 2,
        .read_spi_config = spi_config_unsupported,
        .mac_efuse_offset = 0x40,
        .encryption_in_begin_flash_cmd = true,
        .chip_id = 12,
    },

    // ESP32C5
    {
        .regs = {
            .cmd  = ESP32C5_SPI_REG_BASE + 0x00,
            .usr  = ESP32C5_SPI_REG_BASE + 0x18,
            .usr1 = ESP32C5_SPI_REG_BASE + 0x1c,
            .usr2 = ESP32C5_SPI_REG_BASE + 0x20,
            .w0   = ESP32C5_SPI_REG_BASE + 0x58,
            .mosi_dlen = ESP32C5_SPI_REG_BASE + 0x24,
            .miso_dlen = ESP32C5_SPI_REG_BASE + 0x28,
        },
        .efuse_base = 0x600B4800,
        .chip_magic_value = (const uint32_t[]){ 0x1101406F },
        .magic_values_count = 1,
        .read_spi_config = spi_config_unsupported,
        .mac_efuse_offset = 0x44,
        .encryption_in_begin_flash_cmd = true,
        .chip_id = 23,
    },

    // ESP32H2
    {
        .regs = {
            .cmd  = ESP32H2_SPI_REG_BASE + 0x00,
            .usr  = ESP32H2_SPI_REG_BASE + 0x18,
            .usr1 = ESP32H2_SPI_REG_BASE + 0x1c,
            .usr2 = ESP32H2_SPI_REG_BASE + 0x20,
            .w0   = ESP32H2_SPI_REG_BASE + 0x58,
            .mosi_dlen = ESP32H2_SPI_REG_BASE + 0x24,
            .miso_dlen = ESP32H2_SPI_REG_BASE + 0x28,
        },
        .efuse_base = 0x600B0800,
        .chip_magic_value = (const uint32_t[]){ 0xd7b73e80 },
        .magic_values_count = 1,
        .read_spi_config = spi_config_unsupported,
        .mac_efuse_offset = 0x44,
        .encryption_in_begin_flash_cmd = true,
        .chip_id = 16,
    },

    // ESP32C6
    {
        .regs = {
            .cmd  = ESP32C6_SPI_REG_BASE + 0x00,
            .usr  = ESP32C6_SPI_REG_BASE + 0x18,
            .usr1 = ESP32C6_SPI_REG_BASE + 0x1c,
            .usr2 = ESP32C6_SPI_REG_BASE + 0x20,
            .w0   = ESP32C6_SPI_REG_BASE + 0x58,
            .mosi_dlen = ESP32C6_SPI_REG_BASE + 0x24,
            .miso_dlen = ESP32C6_SPI_REG_BASE + 0x28,
        },
        .efuse_base = 0x600B0800,
        .chip_magic_value = (const uint32_t[]){ 0x2CE0806F },
        .magic_values_count = 1,
        .read_spi_config = spi_config_unsupported,
        .mac_efuse_offset = 0x44,
        .encryption_in_begin_flash_cmd = true,
        .chip_id = 13,
    },

    // ESP32P4
    {
        .regs = {
            .cmd  = ESP32P4_SPI_REG_BASE + 0x00,
            .usr  = ESP32P4_SPI_REG_BASE + 0x18,
            .usr1 = ESP32P4_SPI_REG_BASE + 0x1c,
            .usr2 = ESP32P4_SPI_REG_BASE + 0x20,
            .w0   = ESP32P4_SPI_REG_BASE + 0x58,
            .mosi_dlen = ESP32P4_SPI_REG_BASE + 0x24,
            .miso_dlen = ESP32P4_SPI_REG_BASE + 0x28,
        },
        .efuse_base = 0x5012d000,
        .chip_magic_value = NULL,
        .magic_values_count = 0,
        .read_spi_config = spi_config_esp32xx,
        .mac_efuse_offset = 0x44,
        .encryption_in_begin_flash_cmd = true,
        .chip_id = 18,
    },

};

const target_registers_t *get_esp_target_data(target_chip_t chip)
{
    return (const target_registers_t *)&esp_target[chip];
}

esp_loader_error_t loader_detect_chip(target_chip_t *target_chip, const target_registers_t **target_data)
{
#if (defined SERIAL_FLASHER_INTERFACE_UART) || (defined SERIAL_FLASHER_INTERFACE_USB)
    /* First, attempt to get the target info using GET_SECURITY_INFO command.
       This won't work if the target does not support the command. */
    esp_loader_target_security_info_t security_info;

    if (esp_loader_get_security_info(&security_info) == ESP_LOADER_SUCCESS) {
        *target_chip = security_info.target_chip;
        *target_data = (target_registers_t *)&esp_target[security_info.target_chip];
        return ESP_LOADER_SUCCESS;
    }
#endif /* SERIAL_FLASHER_INTERFACE_UART || SERIAL_FLASHER_INTERFACE_USB */

    uint32_t magic_value;
    RETURN_ON_ERROR( esp_loader_read_register(CHIP_DETECT_MAGIC_REG_ADDR,  &magic_value) );

    for (int chip = 0; chip < ESP_MAX_CHIP; chip++) {
        for (int index = 0; index < esp_target[chip].magic_values_count; index++) {
            if (magic_value == esp_target[chip].chip_magic_value[index]) {
                *target_chip = (target_chip_t)chip;
                *target_data = (target_registers_t *)&esp_target[chip];
                return ESP_LOADER_SUCCESS;
            }
        }
    }

    // ESP32-P4 has different memory map, so the same register as for other chips cannot be used
    // to detect the chip. Date register of SPI peripheral is used instead. There is low probability
    // that the date register will have same value as for other chips and it will also be at different
    // address.
    RETURN_ON_ERROR(esp_loader_read_register(ESP32P4_SPI_DATE_REG, &magic_value));
    if ((magic_value & ESP32P4_SPI_DATE_REG_MASK) == ESP32P4_SPI_DATE_REG_VALUE) {
        *target_chip = ESP32P4_CHIP;
        *target_data = (target_registers_t *)&esp_target[ESP32P4_CHIP];
        return ESP_LOADER_SUCCESS;
    }
    return ESP_LOADER_ERROR_INVALID_TARGET;
}

esp_loader_error_t loader_read_spi_config(target_chip_t target_chip, uint32_t *spi_config)
{
    const esp_target_t *target = &esp_target[target_chip];
    return target->read_spi_config(target->efuse_base, spi_config);
}

esp_loader_error_t loader_read_mac(const target_chip_t target_code, uint8_t *mac)
{
    const esp_target_t *target = &esp_target[target_code];

    uint32_t part1 = 0;
    uint32_t part2 = 0;

    RETURN_ON_ERROR(esp_loader_read_register(target->efuse_base + target->mac_efuse_offset, &part1));
    RETURN_ON_ERROR(esp_loader_read_register(target->efuse_base + target->mac_efuse_offset + sizeof(uint32_t), &part2));

    mac[0] = (part2 >> 8) & 0xff;
    mac[1] = (part2 >> 0) & 0xff;
    mac[2] = (part1 >> 24) & 0xff;
    mac[3] = (part1 >> 16) & 0xff;
    mac[4] = (part1 >> 8) & 0xff;
    mac[5] = (part1 >> 0) & 0xff;

    return ESP_LOADER_SUCCESS;
}

static inline uint32_t efuse_word_addr(uint32_t efuse_base, uint32_t n)
{
    return efuse_base + (n * 4);
}

// 30->GPIO32 | 31->GPIO33
static inline uint8_t adjust_pin_number(uint8_t num)
{
    return (num >= 30) ? num + 2 : num;
}


static esp_loader_error_t spi_config_esp32(uint32_t efuse_base, uint32_t *spi_config)
{
    *spi_config = 0;

    uint32_t reg5, reg3;
    RETURN_ON_ERROR( esp_loader_read_register(efuse_word_addr(efuse_base, 5), &reg5) );
    RETURN_ON_ERROR( esp_loader_read_register(efuse_word_addr(efuse_base, 3), &reg3) );

    uint32_t pins = reg5 & 0xfffff;

    if (pins == 0 || pins == 0xfffff) {
        return ESP_LOADER_SUCCESS;
    }

    uint8_t clk = adjust_pin_number( (pins >> 0)  & 0x1f );
    uint8_t q   = adjust_pin_number( (pins >> 5)  & 0x1f );
    uint8_t d   = adjust_pin_number( (pins >> 10) & 0x1f );
    uint8_t cs  = adjust_pin_number( (pins >> 15) & 0x1f );
    uint8_t hd  = adjust_pin_number( (reg3 >> 4)  & 0x1f );

    if (clk == cs || clk == d || clk == q || q == cs || q == d || q == d) {
        return ESP_LOADER_SUCCESS;
    }

    *spi_config = (hd << 24) | (cs << 18) | (d << 12) | (q << 6) | clk;

    return ESP_LOADER_SUCCESS;
}

// Applies for esp32s2, esp32c3 and esp32c3
static esp_loader_error_t spi_config_esp32xx(uint32_t efuse_base, uint32_t *spi_config)
{
    *spi_config = 0;

    uint32_t reg1, reg2;
    RETURN_ON_ERROR( esp_loader_read_register(efuse_word_addr(efuse_base, 18), &reg1) );
    RETURN_ON_ERROR( esp_loader_read_register(efuse_word_addr(efuse_base, 19), &reg2) );

    uint32_t pins = ((reg1 >> 16) | ((reg2 & 0xfffff) << 16)) & 0x3fffffff;

    if (pins == 0 || pins == 0xffffffff) {
        return ESP_LOADER_SUCCESS;
    }

    *spi_config = pins;
    return ESP_LOADER_SUCCESS;
}

// Some newer chips like the esp32c6 do not support configurable SPI
static esp_loader_error_t spi_config_unsupported(uint32_t efuse_base, uint32_t *spi_config)
{
    (void)(efuse_base);

    *spi_config = 0;
    return ESP_LOADER_SUCCESS;
}

bool encryption_in_begin_flash_cmd(const target_chip_t target)
{
    return esp_target[target].encryption_in_begin_flash_cmd;
}

target_chip_t target_from_chip_id(const uint32_t chip_id)
{
    for (size_t chip = 0; chip < ESP_MAX_CHIP; chip++) {
        if (chip_id == esp_target[chip].chip_id) {
            return (target_chip_t)chip;
        }
    }

    return ESP_UNKNOWN_CHIP;
}
