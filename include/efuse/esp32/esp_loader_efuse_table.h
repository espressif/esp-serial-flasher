/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>

#include "esp_loader_efuse.h"

#ifdef __cplusplus
extern "C" {
#endif

/* esp_loader_efuse_table.c */
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS_RD_DIS[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS_WR_DIS[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS_FLASH_CRYPT_CNT[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS_UART_DOWNLOAD_DIS[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS_MAC[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS_MAC_CRC[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS_DISABLE_APP_CPU[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS_DISABLE_BT[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS_DIS_CACHE[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS_VOL_LEVEL_HP_INV[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS_CLK8M_FREQ[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS_ADC_VREF[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS_XPD_SDIO_REG[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS_XPD_SDIO_TIEH[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS_XPD_SDIO_FORCE[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS_SPI_PAD_CONFIG_CLK[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS_SPI_PAD_CONFIG_Q[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS_SPI_PAD_CONFIG_D[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS_SPI_PAD_CONFIG_CS0[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS_BLOCK1[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS_BLOCK2[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS_BLOCK3[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS_CUSTOM_MAC_CRC[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS_CUSTOM_MAC[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS_ADC1_TP_LOW[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS_ADC1_TP_HIGH[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS_ADC2_TP_LOW[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS_ADC2_TP_HIGH[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS_SECURE_VERSION[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS_MAC_VERSION[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS_BLK3_PART_RESERVE[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS_FLASH_CRYPT_CONFIG[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS_CODING_SCHEME[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS_KEY_STATUS[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS_ABS_DONE_0[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS_ABS_DONE_1[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS_JTAG_DISABLE[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS_CONSOLE_DEBUG_DISABLE[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS_DISABLE_DL_ENCRYPT[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS_DISABLE_DL_DECRYPT[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WR_DIS_DISABLE_DL_CACHE[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_RD_DIS[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_RD_DIS_BLOCK1[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_RD_DIS_BLOCK2[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_RD_DIS_BLOCK3[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_RD_DIS_CUSTOM_MAC_CRC[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_RD_DIS_CUSTOM_MAC[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_RD_DIS_ADC1_TP_LOW[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_RD_DIS_ADC1_TP_HIGH[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_RD_DIS_ADC2_TP_LOW[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_RD_DIS_ADC2_TP_HIGH[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_RD_DIS_SECURE_VERSION[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_RD_DIS_MAC_VERSION[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_RD_DIS_BLK3_PART_RESERVE[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_RD_DIS_FLASH_CRYPT_CONFIG[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_RD_DIS_CODING_SCHEME[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_RD_DIS_KEY_STATUS[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_FLASH_CRYPT_CNT[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_UART_DOWNLOAD_DIS[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_MAC[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_MAC_CRC[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_DISABLE_APP_CPU[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_DISABLE_BT[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_CHIP_PACKAGE_4BIT[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_DIS_CACHE[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_SPI_PAD_CONFIG_HD[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_CHIP_PACKAGE[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_CHIP_CPU_FREQ_LOW[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_CHIP_CPU_FREQ_RATED[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_BLK3_PART_RESERVE[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_CHIP_VER_REV1[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_CLK8M_FREQ[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_ADC_VREF[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_XPD_SDIO_REG[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_XPD_SDIO_TIEH[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_XPD_SDIO_FORCE[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_SPI_PAD_CONFIG_CLK[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_SPI_PAD_CONFIG_Q[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_SPI_PAD_CONFIG_D[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_SPI_PAD_CONFIG_CS0[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_CHIP_VER_REV2[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_VOL_LEVEL_HP_INV[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_WAFER_VERSION_MINOR[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_FLASH_CRYPT_CONFIG[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_CODING_SCHEME[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_CONSOLE_DEBUG_DISABLE[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_DISABLE_SDIO_HOST[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_ABS_DONE_0[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_ABS_DONE_1[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_JTAG_DISABLE[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_DISABLE_DL_ENCRYPT[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_DISABLE_DL_DECRYPT[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_DISABLE_DL_CACHE[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_KEY_STATUS[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_BLOCK1[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_BLOCK2[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_CUSTOM_MAC_CRC[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_MAC_CUSTOM[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_ADC1_TP_LOW[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_ADC1_TP_HIGH[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_ADC2_TP_LOW[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_ADC2_TP_HIGH[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_SECURE_VERSION[];
extern const esp_loader_efuse_desc_t* ESP32_EFUSE_MAC_VERSION[];

extern const esp_loader_efuse_field_set_t ESP32_EFUSE_FIELDS;

#ifdef __cplusplus
}
#endif
