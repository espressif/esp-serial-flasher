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
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_RD_DIS[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_DIS_ICACHE[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_DIS_USB_JTAG[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_POWERGLITCH_EN[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_DIS_FORCE_DOWNLOAD[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_SPI_DOWNLOAD_MSPI_DIS[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_DIS_TWAI[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_JTAG_SEL_ENABLE[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_DIS_PAD_JTAG[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_DIS_DOWNLOAD_MANUAL_ENCRYPT[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_POWERGLITCH_EN1[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_WDT_DELAY_SEL[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_SPI_BOOT_CRYPT_CNT[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_SECURE_BOOT_KEY_REVOKE0[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_SECURE_BOOT_KEY_REVOKE1[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_SECURE_BOOT_KEY_REVOKE2[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_KEY_PURPOSE_0[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_KEY_PURPOSE_1[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_KEY_PURPOSE_2[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_KEY_PURPOSE_3[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_KEY_PURPOSE_4[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_KEY_PURPOSE_5[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_XTS_DPA_PSEUDO_LEVEL[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_SEC_DPA_LEVEL[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_CRYPT_DPA_ENABLE[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_SECURE_BOOT_EN[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_SECURE_BOOT_AGGRESSIVE_REVOKE[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_ECDSA_CURVE_MODE[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_ECC_FORCE_CONST_TIME[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_FLASH_TPUW[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_DIS_DOWNLOAD_MODE[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_DIS_DIRECT_BOOT[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_DIS_USB_SERIAL_JTAG_ROM_PRINT[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_DIS_USB_SERIAL_JTAG_DOWNLOAD_MODE[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_ENABLE_SECURITY_DOWNLOAD[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_UART_PRINT_CONTROL[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_FORCE_SEND_RESUME[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_SECURE_VERSION[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_SECURE_BOOT_DISABLE_FAST_WAKE[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_HYS_EN_PAD0[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_HYS_EN_PAD1[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_BLK1[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_MAC[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_MAC_EXT[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_RXIQ_VERSION[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_RXIQ_0[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_RXIQ_1[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_ACTIVE_HP_DBIAS[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_ACTIVE_LP_DBIAS[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_DSLP_DBIAS[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_DBIAS_VOL_GAP[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_LSLP_HP_DBIAS[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_WAFER_VERSION_MINOR[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_WAFER_VERSION_MAJOR[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_DISABLE_WAFER_VERSION_MAJOR[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_FLASH_CAP[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_FLASH_TEMP[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_FLASH_VENDOR[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_PKG_VERSION[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_SYS_DATA_PART1[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_OPTIONAL_UNIQUE_ID[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_BLK_VERSION_MINOR[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_BLK_VERSION_MAJOR[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_DISABLE_BLK_VERSION_MAJOR[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_TEMP_CALIB[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_ADC1_AVE_INITCODE_ATTEN0[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_ADC1_AVE_INITCODE_ATTEN1[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_ADC1_AVE_INITCODE_ATTEN2[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_ADC1_AVE_INITCODE_ATTEN3[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_ADC1_HI_DOUT_ATTEN0[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_ADC1_HI_DOUT_ATTEN1[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_ADC1_HI_DOUT_ATTEN2[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_ADC1_HI_DOUT_ATTEN3[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_ADC1_CH0_ATTEN0_INITCODE_DIFF[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_ADC1_CH1_ATTEN0_INITCODE_DIFF[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_ADC1_CH2_ATTEN0_INITCODE_DIFF[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_ADC1_CH3_ATTEN0_INITCODE_DIFF[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_ADC1_CH4_ATTEN0_INITCODE_DIFF[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_BLOCK_USR_DATA[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_CUSTOM_MAC[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_BLOCK_KEY0[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_BLOCK_KEY1[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_BLOCK_KEY2[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_BLOCK_KEY3[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_BLOCK_KEY4[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_BLOCK_KEY5[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_BLOCK_SYS_DATA2[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_USB_EXCHG_PINS[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_VDD_SPI_AS_GPIO[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WR_DIS_SOFT_DIS_JTAG[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_RD_DIS[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_RD_DIS_BLOCK_KEY0[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_RD_DIS_BLOCK_KEY1[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_RD_DIS_BLOCK_KEY2[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_RD_DIS_BLOCK_KEY3[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_RD_DIS_BLOCK_KEY4[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_RD_DIS_BLOCK_KEY5[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_RD_DIS_BLOCK_SYS_DATA2[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_DIS_ICACHE[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_DIS_USB_JTAG[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_POWERGLITCH_EN[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_DIS_FORCE_DOWNLOAD[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_SPI_DOWNLOAD_MSPI_DIS[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_DIS_TWAI[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_JTAG_SEL_ENABLE[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_SOFT_DIS_JTAG[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_DIS_PAD_JTAG[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_DIS_DOWNLOAD_MANUAL_ENCRYPT[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_USB_EXCHG_PINS[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_VDD_SPI_AS_GPIO[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_ECDSA_CURVE_MODE[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_ECC_FORCE_CONST_TIME[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_XTS_DPA_PSEUDO_LEVEL[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WDT_DELAY_SEL[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_SPI_BOOT_CRYPT_CNT[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_SECURE_BOOT_KEY_REVOKE0[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_SECURE_BOOT_KEY_REVOKE1[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_SECURE_BOOT_KEY_REVOKE2[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_KEY_PURPOSE_0[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_KEY_PURPOSE_1[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_KEY_PURPOSE_2[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_KEY_PURPOSE_3[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_KEY_PURPOSE_4[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_KEY_PURPOSE_5[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_SEC_DPA_LEVEL[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_CRYPT_DPA_ENABLE[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_SECURE_BOOT_EN[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_SECURE_BOOT_AGGRESSIVE_REVOKE[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_POWERGLITCH_EN1[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_FLASH_TPUW[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_DIS_DOWNLOAD_MODE[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_DIS_DIRECT_BOOT[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_DIS_USB_SERIAL_JTAG_ROM_PRINT[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_DIS_USB_SERIAL_JTAG_DOWNLOAD_MODE[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_ENABLE_SECURITY_DOWNLOAD[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_UART_PRINT_CONTROL[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_FORCE_SEND_RESUME[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_SECURE_VERSION[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_SECURE_BOOT_DISABLE_FAST_WAKE[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_HYS_EN_PAD0[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_HYS_EN_PAD1[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_MAC[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_MAC_EXT[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_RXIQ_VERSION[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_RXIQ_0[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_RXIQ_1[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_ACTIVE_HP_DBIAS[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_ACTIVE_LP_DBIAS[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_DSLP_DBIAS[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_DBIAS_VOL_GAP[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_LSLP_HP_DBIAS[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WAFER_VERSION_MINOR[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_WAFER_VERSION_MAJOR[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_DISABLE_WAFER_VERSION_MAJOR[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_FLASH_CAP[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_FLASH_TEMP[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_FLASH_VENDOR[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_PKG_VERSION[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_OPTIONAL_UNIQUE_ID[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_BLK_VERSION_MINOR[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_BLK_VERSION_MAJOR[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_DISABLE_BLK_VERSION_MAJOR[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_TEMP_CALIB[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_ADC1_AVE_INITCODE_ATTEN0[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_ADC1_AVE_INITCODE_ATTEN1[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_ADC1_AVE_INITCODE_ATTEN2[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_ADC1_AVE_INITCODE_ATTEN3[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_ADC1_HI_DOUT_ATTEN0[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_ADC1_HI_DOUT_ATTEN1[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_ADC1_HI_DOUT_ATTEN2[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_ADC1_HI_DOUT_ATTEN3[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_ADC1_CH0_ATTEN0_INITCODE_DIFF[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_ADC1_CH1_ATTEN0_INITCODE_DIFF[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_ADC1_CH2_ATTEN0_INITCODE_DIFF[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_ADC1_CH3_ATTEN0_INITCODE_DIFF[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_ADC1_CH4_ATTEN0_INITCODE_DIFF[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_USER_DATA[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_USER_DATA_MAC_CUSTOM[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_KEY0[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_KEY1[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_KEY2[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_KEY3[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_KEY4[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_KEY5[];
extern const esp_loader_efuse_desc_t* ESP32H2_EFUSE_SYS_DATA_PART2[];

extern const esp_loader_efuse_field_set_t ESP32H2_V0_0_V1_1_EFUSE_FIELDS;

extern const esp_loader_efuse_field_set_t ESP32H2_EFUSE_FIELDS;

/* esp_loader_efuse_table_v0.0_v1.1.c */
extern const esp_loader_efuse_desc_t* ESP32H2_V0_0_V1_1_EFUSE_WR_DIS_ECDSA_FORCE_USE_HARDWARE_K[];
extern const esp_loader_efuse_desc_t* ESP32H2_V0_0_V1_1_EFUSE_ECDSA_FORCE_USE_HARDWARE_K[];

#ifdef __cplusplus
}
#endif
