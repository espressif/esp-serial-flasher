/*
 * SPDX-FileCopyrightText: 2020-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_loader.h"
#include "esp_loader_protocol.h"
#include "esp_targets.h"
#include <stddef.h>

typedef esp_loader_error_t (*read_spi_config_t)(esp_loader_t *loader, uint32_t efuse_base, uint32_t *spi_config);

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
#define ESP32C61_SPI_REG_BASE 0x60003000
#define ESP32S31_SPI_REG_BASE 0x20501000
#define ESP32xx_SPI_REG_BASE 0x60002000
#define ESP32_SPI_REG_BASE   0x3ff42000

#define CHIP_ID_NONE 0xFF

// Used for ESP32P4 chip detection, other chips uses ROM magic value
#define ESP32P4_SPI_DATE_REG 0x500d0000
#define ESP32P4_SPI_DATE_REG_MASK 0x7FFFFFF
#define ESP32P4_SPI_DATE_REG_VALUE 0x2207202

static esp_loader_error_t spi_config_esp32(esp_loader_t *loader, uint32_t efuse_base, uint32_t *spi_config);
static esp_loader_error_t spi_config_esp32xx(esp_loader_t *loader, uint32_t efuse_base, uint32_t *spi_config);
static esp_loader_error_t spi_config_unsupported(esp_loader_t *loader, uint32_t efuse_base, uint32_t *spi_config);

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
        .chip_magic_value = (const uint32_t[]){ 0x6f51306f, 0x7c41a06f, 0x0C21E06F },
        .magic_values_count = 3,
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
        .chip_magic_value = (const uint32_t[]){ 0x1101406F, 0x5fd1406f },
        .magic_values_count = 2,
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

    // ESP32C61
    {
        .regs = {
            .cmd  = ESP32C61_SPI_REG_BASE + 0x00,
            .usr  = ESP32C61_SPI_REG_BASE + 0x18,
            .usr1 = ESP32C61_SPI_REG_BASE + 0x1c,
            .usr2 = ESP32C61_SPI_REG_BASE + 0x20,
            .w0   = ESP32C61_SPI_REG_BASE + 0x58,
            .mosi_dlen = ESP32C61_SPI_REG_BASE + 0x24,
            .miso_dlen = ESP32C61_SPI_REG_BASE + 0x28,
        },
        .efuse_base = 0x600B4800,
        .chip_magic_value = (const uint32_t[]){ 0x7211606F },
        .magic_values_count = 1,
        .read_spi_config = spi_config_unsupported,
        .mac_efuse_offset = 0x44,
        .encryption_in_begin_flash_cmd = true,
        .chip_id = 20,
    },

    // ESP32S31
    {
        .regs = {
            .cmd  = ESP32S31_SPI_REG_BASE + 0x00,
            .usr  = ESP32S31_SPI_REG_BASE + 0x18,
            .usr1 = ESP32S31_SPI_REG_BASE + 0x1c,
            .usr2 = ESP32S31_SPI_REG_BASE + 0x20,
            .w0   = ESP32S31_SPI_REG_BASE + 0x58,
            .mosi_dlen = ESP32S31_SPI_REG_BASE + 0x24,
            .miso_dlen = ESP32S31_SPI_REG_BASE + 0x28,
        },
        .efuse_base = 0x20715000,
        .chip_magic_value = NULL,
        .magic_values_count = 0,
        .read_spi_config = spi_config_unsupported,
        .mac_efuse_offset = 0x50,
        .encryption_in_begin_flash_cmd = true,
        .chip_id = 32,
    },
};

const target_registers_t *get_esp_target_data(target_chip_t chip)
{
    return &esp_target[chip].regs;
}

esp_loader_error_t loader_detect_chip(esp_loader_t *loader)
{

    /* If the chip is already known (e.g. identified by SDIO card enumeration during
     * initialize_conn), skip the detection sequence and only look up the register table. */
    if (loader->_target != ESP_UNKNOWN_CHIP) {
        loader->_reg = &esp_target[loader->_target].regs;
        return ESP_LOADER_SUCCESS;
    }

    /* First, attempt to get the target info using GET_SECURITY_INFO command.
       This won't work if the target does not support the command. */
    esp_loader_target_security_info_t security_info;
    if (esp_loader_get_security_info(loader, &security_info) == ESP_LOADER_SUCCESS) {
        if (security_info.target_chip == ESP_UNKNOWN_CHIP) {
            return ESP_LOADER_ERROR_INVALID_TARGET;
        }
        loader->_target = security_info.target_chip;
        goto success;
    }

    uint32_t magic_value;
    RETURN_ON_ERROR(esp_loader_read_register(loader, CHIP_DETECT_MAGIC_REG_ADDR, &magic_value));

    for (int chip = 0; chip < ESP_MAX_CHIP; chip++) {
        for (int index = 0; index < esp_target[chip].magic_values_count; index++) {
            if (magic_value == esp_target[chip].chip_magic_value[index]) {
                loader->_target = (target_chip_t)chip;
                goto success;
            }
        }
    }

    // ESP32-P4 has different memory map, so the same register as for other chips cannot be used
    // to detect the chip. Date register of SPI peripheral is used instead. There is low probability
    // that the date register will have same value as for other chips and it will also be at different
    // address.
    RETURN_ON_ERROR(esp_loader_read_register(loader, ESP32P4_SPI_DATE_REG, &magic_value));
    if ((magic_value & ESP32P4_SPI_DATE_REG_MASK) == ESP32P4_SPI_DATE_REG_VALUE) {
        loader->_target = ESP32P4_CHIP;
        goto success;
    }

    return ESP_LOADER_ERROR_INVALID_TARGET;

success:
    loader->_reg = &esp_target[loader->_target].regs;
    return ESP_LOADER_SUCCESS;
}

esp_loader_error_t loader_read_spi_config(esp_loader_t *loader, target_chip_t target_chip, uint32_t *spi_config)
{
    const esp_target_t *target = &esp_target[target_chip];
    return target->read_spi_config(loader, target->efuse_base, spi_config);
}

esp_loader_error_t loader_read_mac(esp_loader_t *loader, const target_chip_t target_code, uint8_t *mac)
{
    const esp_target_t *target = &esp_target[target_code];

    uint32_t part1 = 0;
    uint32_t part2 = 0;

    RETURN_ON_ERROR(esp_loader_read_register(loader, target->efuse_base + target->mac_efuse_offset, &part1));
    RETURN_ON_ERROR(esp_loader_read_register(loader, target->efuse_base + target->mac_efuse_offset + sizeof(uint32_t), &part2));

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


static esp_loader_error_t spi_config_esp32(esp_loader_t *loader, uint32_t efuse_base, uint32_t *spi_config)
{
    *spi_config = 0;

    uint32_t reg5, reg3;
    RETURN_ON_ERROR( esp_loader_read_register(loader, efuse_word_addr(efuse_base, 5), &reg5) );
    RETURN_ON_ERROR( esp_loader_read_register(loader, efuse_word_addr(efuse_base, 3), &reg3) );

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
static esp_loader_error_t spi_config_esp32xx(esp_loader_t *loader, uint32_t efuse_base, uint32_t *spi_config)
{
    *spi_config = 0;

    uint32_t reg1, reg2;
    RETURN_ON_ERROR( esp_loader_read_register(loader, efuse_word_addr(efuse_base, 18), &reg1) );
    RETURN_ON_ERROR( esp_loader_read_register(loader, efuse_word_addr(efuse_base, 19), &reg2) );

    uint32_t pins = ((reg1 >> 16) | ((reg2 & 0xfffff) << 16)) & 0x3fffffff;

    if (pins == 0 || pins == 0xffffffff) {
        return ESP_LOADER_SUCCESS;
    }

    *spi_config = pins;
    return ESP_LOADER_SUCCESS;
}

// Some newer chips like the esp32c6 do not support configurable SPI
static esp_loader_error_t spi_config_unsupported(esp_loader_t *loader, uint32_t efuse_base, uint32_t *spi_config)
{
    (void)(loader);
    (void)(efuse_base);

    *spi_config = 0;
    return ESP_LOADER_SUCCESS;
}

/* Chip revision (wafer version) reads. The word indexes and bit positions below follow
 * esptool's per-target get_major_chip_version() / get_minor_chip_version(). Each case
 * reads its words once; nothing here is shared because almost nothing is common. */

// Offset of the eFuse block holding the wafer version fields, relative to efuse_base
#define EFUSE_BLOCK1_OFFSET 0x44
#define EFUSE_BLOCK2_OFFSET 0x5C
#define ESP32C2_EFUSE_BLOCK2_OFFSET 0x40
#define ESP32S31_EFUSE_BLOCK1_OFFSET 0x50

/* The ESP32's third major version bit is not an eFuse but a bit of the SYSCON date
 * register, and the three bits are cumulative rather than a plain number. */
#define ESP32_APB_CTL_DATE_REG 0x3ff6607c

static esp_loader_error_t read_efuse_word(esp_loader_t *loader, uint32_t block, uint32_t word,
        uint32_t *value)
{
    return esp_loader_read_register(loader, efuse_word_addr(block, word), value);
}

esp_loader_error_t loader_read_chip_revision(esp_loader_t *loader, const target_chip_t target_code,
        uint16_t *revision)
{
    if (target_code >= ESP_MAX_CHIP) {
        return ESP_LOADER_ERROR_UNSUPPORTED_CHIP;
    }

    const uint32_t base = esp_target[target_code].efuse_base;
    const uint32_t blk1 = base + EFUSE_BLOCK1_OFFSET;
    uint32_t major = 0;
    uint32_t minor = 0;

    switch (target_code) {

    case ESP32_CHIP: {
        /* BLOCK0 words, so no block offset, plus the SYSCON bit. Only the four
         * combinations below name a revision; anything else is v0. */
        uint32_t word3, word5, apb_ctl_date;
        RETURN_ON_ERROR( read_efuse_word(loader, base, 3, &word3) );
        RETURN_ON_ERROR( read_efuse_word(loader, base, 5, &word5) );
        RETURN_ON_ERROR( esp_loader_read_register(loader, ESP32_APB_CTL_DATE_REG, &apb_ctl_date) );

        const uint32_t combined = ((word3 >> 15) & 0x1)
                                  | (((word5 >> 20) & 0x1) << 1)
                                  | (((apb_ctl_date >> 31) & 0x1) << 2);
        switch (combined) {
        case 1:  major = 1; break;
        case 3:  major = 2; break;
        case 7:  major = 3; break;
        default: major = 0; break;
        }
        minor = (word5 >> 24) & 0x3;
        break;
    }

    case ESP32S2_CHIP: {
        uint32_t word3, word4;
        RETURN_ON_ERROR( read_efuse_word(loader, blk1, 3, &word3) );
        RETURN_ON_ERROR( read_efuse_word(loader, blk1, 4, &word4) );

        major = (word3 >> 18) & 0x3;
        minor = (((word3 >> 20) & 0x1) << 3) | ((word4 >> 4) & 0x7);
        break;
    }

    case ESP32C3_CHIP: {
        uint32_t word3, word5;
        RETURN_ON_ERROR( read_efuse_word(loader, blk1, 3, &word3) );
        RETURN_ON_ERROR( read_efuse_word(loader, blk1, 5, &word5) );

        major = (word5 >> 24) & 0x3;
        minor = (((word5 >> 23) & 0x1) << 3) | ((word3 >> 18) & 0x7);
        break;
    }

    case ESP32S3_CHIP: {
        /* Same fields as the ESP32-C3, but on silicon carrying block version v1.1 the
         * major version bits were allocated to another purpose. Only chip v0.0 ever has
         * that block version, so BLK_VERSION decides whether the bits mean anything. */
        uint32_t word3, word5;
        RETURN_ON_ERROR( read_efuse_word(loader, blk1, 3, &word3) );
        RETURN_ON_ERROR( read_efuse_word(loader, blk1, 5, &word5) );

        major = (word5 >> 24) & 0x3;
        minor = (((word5 >> 23) & 0x1) << 3) | ((word3 >> 18) & 0x7);

        if ((minor & 0x7) == 0) {
            uint32_t blk2_word4;
            RETURN_ON_ERROR( read_efuse_word(loader, base + EFUSE_BLOCK2_OFFSET, 4, &blk2_word4) );

            const uint32_t blk_version_major = blk2_word4 & 0x3;
            const uint32_t blk_version_minor = (word3 >> 24) & 0x7;
            if (blk_version_major == 1 && blk_version_minor == 1) {
                major = 0;
                minor = 0;
            }
        }
        break;
    }

    case ESP32C2_CHIP: {
        /* The only chip whose wafer version sits in BLOCK2 */
        uint32_t word1;
        RETURN_ON_ERROR( read_efuse_word(loader, base + ESP32C2_EFUSE_BLOCK2_OFFSET, 1, &word1) );

        major = (word1 >> 20) & 0x3;
        minor = (word1 >> 16) & 0xF;
        break;
    }

    case ESP32H2_CHIP: {
        uint32_t word3;
        RETURN_ON_ERROR( read_efuse_word(loader, blk1, 3, &word3) );

        major = (word3 >> 21) & 0x3;
        minor = (word3 >> 18) & 0x7;
        break;
    }

    case ESP32C6_CHIP: {
        uint32_t word3;
        RETURN_ON_ERROR( read_efuse_word(loader, blk1, 3, &word3) );

        major = (word3 >> 22) & 0x3;
        minor = (word3 >> 18) & 0xF;
        break;
    }

    case ESP32C5_CHIP:
    case ESP32C61_CHIP: {
        uint32_t word2;
        RETURN_ON_ERROR( read_efuse_word(loader, blk1, 2, &word2) );

        major = (word2 >> 4) & 0x3;
        minor = (word2 >> 0) & 0xF;
        break;
    }

    case ESP32P4_CHIP: {
        /* The major version is 3 bits wide here, and its top bit sits apart from the
         * other two — that third bit is what makes v3.0 representable. */
        uint32_t word2;
        RETURN_ON_ERROR( read_efuse_word(loader, blk1, 2, &word2) );

        major = (((word2 >> 23) & 0x1) << 2) | ((word2 >> 4) & 0x3);
        minor = (word2 >> 0) & 0xF;
        break;
    }

    case ESP32S31_CHIP: {
        /* Same bit layout as the ESP32-C6, but the only chip whose BLOCK1 is not
         * at EFUSE_BLOCK1_OFFSET, so it cannot share that case. */
        uint32_t word3;
        RETURN_ON_ERROR( read_efuse_word(loader, base + ESP32S31_EFUSE_BLOCK1_OFFSET, 3, &word3) );

        major = (word3 >> 22) & 0x3;
        minor = (word3 >> 18) & 0xF;
        break;
    }

    case ESP8266_CHIP:
    default:
        return ESP_LOADER_ERROR_UNSUPPORTED_CHIP;   // No chip revision on this target
    }

    *revision = (uint16_t)(major * 100 + minor);
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

uint32_t esp_targets_get_efuse_base(target_chip_t target)
{
    if (target >= ESP_MAX_CHIP) {
        return 0;
    }
    return esp_target[target].efuse_base;
}

esp_loader_error_t get_crystal_frequency_esp32c2(esp_loader_t *loader, uint32_t *frequency)
{
    /*
    There is a bug in the ESP32-C2 ROM that causes it to think it has a 40 MHz crystal,
    even though it might be 26 MHz. That is why we need to check frequency and adjust
    the transmission rate accordingly.

    The logic here is:
    - We know that our baud rate and the target's UART baud rate are roughly the same,
    or we couldn't communicate
    - We can read the UART clock divider register to know how the ESP derives this
    from the APB bus frequency
    - Multiplying these two together gives us the bus frequency which is either
    the crystal frequency or multiple of the crystal frequency (for some chips).
    */

    const uint32_t ESP32C2_CRYSTAL_26MHZ = 26;
    const uint32_t ESP32C2_CRYSTAL_40MHZ = 40;
    const uint32_t CRYSTAL_FREQ_THRESHOLD = 33;
    const uint32_t UART_CLK_DIV_REG = 0x60000014;
    const uint32_t UART_CLK_DIV_REG_MASK = 0xFFFFF;

    *frequency = 0;
    uint32_t est_freq;
    RETURN_ON_ERROR(esp_loader_read_register(loader, UART_CLK_DIV_REG, &est_freq));
    est_freq &= UART_CLK_DIV_REG_MASK;
    est_freq = (115200u * est_freq) / 1000000U;

    *frequency = (est_freq > CRYSTAL_FREQ_THRESHOLD) ? ESP32C2_CRYSTAL_40MHZ : ESP32C2_CRYSTAL_26MHZ;
    return ESP_LOADER_SUCCESS;
}
