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
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_RD_DIS[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_WDT_DELAY_SEL[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_DIS_PAD_JTAG[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_DIS_DOWNLOAD_ICACHE[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_DIS_DOWNLOAD_MANUAL_ENCRYPT[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_SPI_BOOT_CRYPT_CNT[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_XTS_KEY_LENGTH_256[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_SECURE_BOOT_EN[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_UART_PRINT_CONTROL[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_FORCE_SEND_RESUME[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_DIS_DOWNLOAD_MODE[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_DIS_DIRECT_BOOT[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_ENABLE_SECURITY_DOWNLOAD[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_FLASH_TPUW[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_SECURE_VERSION[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_CUSTOM_MAC_USED[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_DISABLE_WAFER_VERSION_MAJOR[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_DISABLE_BLK_VERSION_MAJOR[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_CUSTOM_MAC[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_MAC[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_WAFER_VERSION_MINOR[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_WAFER_VERSION_MAJOR[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_PKG_VERSION[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_BLK_VERSION_MINOR[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_BLK_VERSION_MAJOR[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_OCODE[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_TEMP_CALIB[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_ADC1_INIT_CODE_ATTEN0[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_ADC1_INIT_CODE_ATTEN3[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_ADC1_CAL_VOL_ATTEN0[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_ADC1_CAL_VOL_ATTEN3[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_DIG_DBIAS_HVT[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_DIG_LDO_SLP_DBIAS2[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_DIG_LDO_SLP_DBIAS26[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_DIG_LDO_ACT_DBIAS26[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_DIG_LDO_ACT_STEPD10[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_RTC_LDO_SLP_DBIAS13[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_RTC_LDO_SLP_DBIAS29[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_RTC_LDO_SLP_DBIAS31[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_RTC_LDO_ACT_DBIAS31[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_RTC_LDO_ACT_DBIAS13[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_ADC_CALIBRATION_3[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_FLASH_VENDOR[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_FLASH_TEMP[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_FLASH_CAP[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WR_DIS_BLOCK_KEY0[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_RD_DIS[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_RD_DIS_KEY0[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_RD_DIS_KEY0_LOW[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_RD_DIS_KEY0_HI[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WDT_DELAY_SEL[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_DIS_PAD_JTAG[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_DIS_DOWNLOAD_ICACHE[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_DIS_DOWNLOAD_MANUAL_ENCRYPT[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_SPI_BOOT_CRYPT_CNT[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_XTS_KEY_LENGTH_256[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_UART_PRINT_CONTROL[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_FORCE_SEND_RESUME[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_DIS_DOWNLOAD_MODE[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_DIS_DIRECT_BOOT[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_ENABLE_SECURITY_DOWNLOAD[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_FLASH_TPUW[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_SECURE_BOOT_EN[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_SECURE_VERSION[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_CUSTOM_MAC_USED[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_DISABLE_WAFER_VERSION_MAJOR[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_DISABLE_BLK_VERSION_MAJOR[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_USER_DATA[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_USER_DATA_MAC_CUSTOM[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_MAC[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WAFER_VERSION_MINOR[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_WAFER_VERSION_MAJOR[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_PKG_VERSION[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_BLK_VERSION_MINOR[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_BLK_VERSION_MAJOR[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_OCODE[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_TEMP_CALIB[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_ADC1_INIT_CODE_ATTEN0[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_ADC1_INIT_CODE_ATTEN3[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_ADC1_CAL_VOL_ATTEN0[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_ADC1_CAL_VOL_ATTEN3[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_DIG_DBIAS_HVT[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_DIG_LDO_SLP_DBIAS2[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_DIG_LDO_SLP_DBIAS26[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_DIG_LDO_ACT_DBIAS26[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_DIG_LDO_ACT_STEPD10[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_RTC_LDO_SLP_DBIAS13[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_RTC_LDO_SLP_DBIAS29[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_RTC_LDO_SLP_DBIAS31[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_RTC_LDO_ACT_DBIAS31[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_RTC_LDO_ACT_DBIAS13[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_ADC_CALIBRATION_3[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_FLASH_VENDOR[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_FLASH_TEMP[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_FLASH_CAP[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_KEY0[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_KEY0_LOW_128[];
extern const esp_loader_efuse_desc_t* ESP32C2_EFUSE_KEY0_HI_128[];

extern const esp_loader_efuse_field_set_t ESP32C2_EFUSE_FIELDS;

#ifdef __cplusplus
}
#endif
