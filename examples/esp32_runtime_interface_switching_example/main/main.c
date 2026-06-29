/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* Example of runtime interface switching.

   The host board is an ESP32-P4 that flashes a target over SDIO, UART, and USB
   back to back.
*/

#include <sys/param.h>
#include <string.h>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp32_port.h"
#include "esp32_sdio_port.h"
#include "esp32_usb_cdc_acm_port.h"
#include "esp_loader.h"
#include "example_common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "usb/usb_host.h"
#include "usb/cdc_acm_host.h"

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

static const char *TAG = "runtime_interface_switching";

// Target over SDIO
#define SDIO_RESET_PIN    CONFIG_EXAMPLE_SDIO_RESET_PIN
#define SDIO_BOOT_PIN     CONFIG_EXAMPLE_SDIO_BOOT_PIN
#define SDIO_D0_PIN       CONFIG_EXAMPLE_SDIO_D0_PIN
#define SDIO_D1_PIN       CONFIG_EXAMPLE_SDIO_D1_PIN
#define SDIO_D2_PIN       CONFIG_EXAMPLE_SDIO_D2_PIN
#define SDIO_D3_PIN       CONFIG_EXAMPLE_SDIO_D3_PIN
#define SDIO_CLK_PIN      CONFIG_EXAMPLE_SDIO_CLK_PIN
#define SDIO_CMD_PIN      CONFIG_EXAMPLE_SDIO_CMD_PIN

// Target over UART
#define UART_PORT         UART_NUM_1
#define UART_TX_PIN       CONFIG_EXAMPLE_UART_TX_PIN
#define UART_RX_PIN       CONFIG_EXAMPLE_UART_RX_PIN
#define UART_RESET_PIN    CONFIG_EXAMPLE_UART_RESET_PIN
#define UART_BOOT_PIN     CONFIG_EXAMPLE_UART_BOOT_PIN

// Target over USB
#define USB_PERIPHERAL_MAP BIT(CONFIG_EXAMPLE_USB_PERIPHERAL_INDEX)

#define USB_CONNECT_RETRY_MS 500
#define USB_CONNECT_MAX_RETRIES 10

static void usb_lib_task(void *arg)
{
    while (1) {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_ERROR_CHECK(usb_host_device_free_all());
        }
    }
}

static esp_loader_error_t init_usb_host(void)
{
    ESP_LOGI(TAG, "Installing USB Host");
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
        .peripheral_map = USB_PERIPHERAL_MAP,
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));

    BaseType_t task_created = xTaskCreate(usb_lib_task, "usb_lib", 4096, NULL, 20, NULL);
    if (task_created != pdTRUE) {
        return ESP_LOADER_ERROR_FAIL;
    }

    ESP_LOGI(TAG, "Installing USB CDC-ACM driver");
    ESP_ERROR_CHECK(cdc_acm_host_install(NULL));

    return ESP_LOADER_SUCCESS;
}

static void flash_target_firmware(esp_loader_t *loader, const char *interface_label)
{
    ESP_LOGI(TAG, "Flashing %s...", interface_label);
    ESP_LOGI(TAG, "Loading bootloader...");
    target_chip_t chip = esp_loader_get_target(loader);
    uint32_t bootloader_addr = get_bootloader_address(chip);
    flash_binary(loader, bootloader_bin, bootloader_bin_size, bootloader_addr);
    ESP_LOGI(TAG, "Loading partition table...");
    flash_binary(loader, partition_table_bin, partition_table_bin_size, PARTITION_TABLE_ADDRESS);
    ESP_LOGI(TAG, "Loading app...");
    flash_binary(loader, app_bin, app_bin_size, APPLICATION_ADDRESS);
    ESP_LOGI(TAG, "%s flashing done!", interface_label);
}

static esp_loader_error_t flash_target_over_sdio(esp_loader_t *loader)
{
    esp32_sdio_port_t port = {
        .port.ops          = &esp32_sdio_ops,
        .slot              = SDMMC_HOST_SLOT_1,
        .max_freq_khz      = SDMMC_FREQ_DEFAULT,
        .reset_pin         = SDIO_RESET_PIN,
        .boot_pin          = SDIO_BOOT_PIN,
        .bus_width         = SDIO_4BIT,
        .sdio_d0_pin       = SDIO_D0_PIN,
        .sdio_d1_pin       = SDIO_D1_PIN,
        .sdio_d2_pin       = SDIO_D2_PIN,
        .sdio_d3_pin       = SDIO_D3_PIN,
        .sdio_clk_pin      = SDIO_CLK_PIN,
        .sdio_cmd_pin      = SDIO_CMD_PIN,
    };

    if (esp_loader_init_sdio(loader, &port.port) != ESP_LOADER_SUCCESS) {
        ESP_LOGE(TAG, "SDIO initialization failed.");
        abort();
    }

    if (connect_to_target(loader, 0) != ESP_LOADER_SUCCESS) {
        esp_loader_deinit(loader);
        return ESP_LOADER_ERROR_FAIL;
    }

    flash_target_firmware(loader, "target over SDIO");
    esp_loader_reset_target(loader);
    esp_loader_deinit(loader);

    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t flash_target_over_uart(esp_loader_t *loader)
{
    esp32_port_t port = {
        .port.ops          = &esp32_uart_ops,
        .baud_rate         = 115200,
        .uart_port         = UART_PORT,
        .uart_rx_pin       = UART_RX_PIN,
        .uart_tx_pin       = UART_TX_PIN,
        .reset_pin         = UART_RESET_PIN,
        .boot_pin          = UART_BOOT_PIN,
    };

    if (esp_loader_init_serial(loader, &port.port) != ESP_LOADER_SUCCESS) {
        ESP_LOGE(TAG, "UART initialization failed.");
        return ESP_LOADER_ERROR_FAIL;
    }

    if (connect_to_target(loader, 0) != ESP_LOADER_SUCCESS) {
        esp_loader_deinit(loader);
        return ESP_LOADER_ERROR_FAIL;
    }

    flash_target_firmware(loader, "target over UART");
    esp_loader_reset_target(loader);
    esp_loader_deinit(loader);

    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t flash_target_over_usb(esp_loader_t *loader)
{
    esp32_usb_cdc_acm_port_t port = {
        .port.ops              = &esp32_usb_cdc_acm_ops,
        .device_vid            = USB_VID_PID_AUTO_DETECT,
        .device_pid            = USB_VID_PID_AUTO_DETECT,
        .connection_timeout_ms = 1000,
        .out_buffer_size       = 4096,
    };

    ESP_LOGI(TAG, "Waiting for target over USB...");
    for (int retry = 0; retry < USB_CONNECT_MAX_RETRIES; retry++) {
        if (esp_loader_init_serial(loader, &port.port) == ESP_LOADER_SUCCESS) {
            break;
        }
        if (retry == USB_CONNECT_MAX_RETRIES - 1) {
            ESP_LOGE(TAG, "USB device not found.");
            return ESP_LOADER_ERROR_FAIL;
        }
        vTaskDelay(pdMS_TO_TICKS(USB_CONNECT_RETRY_MS));
    }

    if (connect_to_target(loader, 0) != ESP_LOADER_SUCCESS) {
        esp_loader_deinit(loader);
        return ESP_LOADER_ERROR_FAIL;
    }

    flash_target_firmware(loader, "target over USB");
    esp_loader_reset_target(loader);
    esp_loader_deinit(loader);

    return ESP_LOADER_SUCCESS;
}

void app_main(void)
{
    esp_loader_t loader;

    if (init_usb_host() != ESP_LOADER_SUCCESS) {
        ESP_LOGE(TAG, "USB host initialization failed.");
        return;
    }

    if (flash_target_over_usb(&loader) != ESP_LOADER_SUCCESS) {
        ESP_LOGE(TAG, "Target over USB flashing failed.");
        return;
    }

    if (flash_target_over_sdio(&loader) != ESP_LOADER_SUCCESS) {
        ESP_LOGE(TAG, "Target over SDIO flashing failed.");
        return;
    }

    if (flash_target_over_uart(&loader) != ESP_LOADER_SUCCESS) {
        ESP_LOGE(TAG, "Target over UART flashing failed.");
        return;
    }

    ESP_LOGI(TAG, "All targets flashed.");
}
