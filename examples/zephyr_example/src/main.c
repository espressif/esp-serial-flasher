/*
 * ESP Flasher Library Example for Zephyr
 * Written in 2022 by KT-Elektronik, Klaucke und Partner GmbH
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 *
 * Copyright (c) 2023 Espressif Systems (Shanghai) Co., Ltd.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/util.h>

#include <stdio.h>
#include <string.h>
#include <zephyr_port.h>
#include <esp_loader.h>
#include "example_common.h"

// Embedded binary files using bin2array.cmake
extern const uint8_t bootloader_bin[];
extern const uint32_t bootloader_bin_size;
extern const uint8_t bootloader_bin_md5[];
extern const uint8_t partition_table_bin[];
extern const uint32_t partition_table_bin_size;
extern const uint8_t partition_table_bin_md5[];
extern const uint8_t app_bin[];
extern const uint32_t app_bin_size;
extern const uint8_t app_bin_md5[];

#define HIGHER_BAUDRATE 230400
#define DEFAULT_BAUDRATE 115200

/* Get UART DTS entry used as flash interface */
static const struct device *esp_uart_dev = DEVICE_DT_GET(DT_ALIAS(uart));
/* Get GPIO pin connected to the ESP's enable pin. */
static const struct gpio_dt_spec esp_enable_spec = GPIO_DT_SPEC_GET(DT_ALIAS(en), gpios);
/* Get GPIO pin  connected to the ESP's boot pin. */
static const struct gpio_dt_spec esp_boot_spec = GPIO_DT_SPEC_GET(DT_ALIAS(boot), gpios);

int main(void)
{

    const loader_zephyr_config_t config = {
        .uart_dev = esp_uart_dev,
        .enable_spec = esp_enable_spec,
        .boot_spec = esp_boot_spec
    };

    printk("Running ESP Flasher from Zephyr\r\n");

    if (!device_is_ready(esp_uart_dev)) {
        printk("ESP UART not ready");
        return -ENODEV;
    }

    if (!device_is_ready(esp_boot_spec.port)) {
        printk("ESP boot GPIO not ready");
        return -ENODEV;
    }

    if (!device_is_ready(esp_enable_spec.port)) {
        printk("ESP enable GPIO not ready");
        return -ENODEV;
    }

    gpio_pin_configure_dt(&esp_boot_spec, GPIO_OUTPUT_ACTIVE);
    gpio_pin_configure_dt(&esp_enable_spec, GPIO_OUTPUT_INACTIVE);

    if (loader_port_zephyr_init(&config) != ESP_LOADER_SUCCESS) {
        printk("ESP loader init failed");
        return -EIO;
    }

    if (connect_to_target(HIGHER_BAUDRATE) == ESP_LOADER_SUCCESS) {
        printk("Loading bootloader...\n");
        target_chip_t chip = esp_loader_get_target();
        uint32_t bootloader_addr = get_bootloader_address(chip);
        flash_binary(bootloader_bin, bootloader_bin_size, bootloader_addr);
        printk("Loading partition table...\n");
        flash_binary(partition_table_bin, partition_table_bin_size, PARTITION_TABLE_ADDRESS);
        printk("Loading app...\n");
        flash_binary(app_bin, app_bin_size, APPLICATION_ADDRESS);
    }

    esp_loader_reset_target();

    if (loader_port_change_transmission_rate(DEFAULT_BAUDRATE) == ESP_LOADER_SUCCESS) {
        // Delay for skipping the boot message of the targets
        k_msleep(500);

        printk("********************************************\n");
        printk("*** Logs below are print from slave .... ***\n");
        printk("********************************************\n");
        while (1) {
            uint8_t c;
            if (uart_poll_in(esp_uart_dev, &c) == 0) {
                printk("%c", c);
            }
        }
    }
    return 0;
}
