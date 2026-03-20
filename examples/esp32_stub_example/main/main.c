/* Flash multiple partitions example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <sys/param.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp32_port.h"
#include "esp_loader.h"
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

static const char *TAG = "serial_stub_flasher";

// Max line size
#define BUF_LEN 128
static uint8_t buf[BUF_LEN] = {0};

void slave_monitor(void *arg)
{
#if (HIGHER_BAUDRATE != 115200)
    uart_flush_input(UART_NUM_1);
    uart_flush(UART_NUM_1);
    uart_set_baudrate(UART_NUM_1, 115200);
#endif
    while (1) {
        int rxBytes = uart_read_bytes(UART_NUM_1, buf, BUF_LEN, 100 / portTICK_PERIOD_MS);
        buf[rxBytes] = '\0';
        printf("%s", buf);
    }
}

void app_main(void)
{
    esp_loader_t loader;

    esp32_port_t port = {
        .port.ops          = &esp32_uart_ops,
        .baud_rate         = 115200,
        .uart_port         = UART_NUM_1,
        .uart_rx_pin       = GPIO_NUM_5,
        .uart_tx_pin       = GPIO_NUM_4,
        .reset_trigger_pin = GPIO_NUM_25,
        .gpio0_trigger_pin = GPIO_NUM_26,
    };

    if (esp_loader_init_uart(&loader, &port.port) != ESP_LOADER_SUCCESS) {
        ESP_LOGE(TAG, "serial initialization failed.");
        return;
    }

    if (connect_to_target_with_stub(&loader, 115200, 230400) == ESP_LOADER_SUCCESS) {

        // Not necessary, just to demonstrate the erase functions
        esp_loader_error_t err;
        err = esp_loader_flash_erase(&loader);
        if (err != ESP_LOADER_SUCCESS) {
            ESP_LOGE(TAG, "Failed to erase flash");
            return;
        }
        err = esp_loader_flash_erase_region(&loader, 0, 0x1000);
        if (err != ESP_LOADER_SUCCESS) {
            ESP_LOGE(TAG, "Failed to erase flash region");
            return;
        }

        ESP_LOGI(TAG, "Loading bootloader...");
        target_chip_t chip = esp_loader_get_target(&loader);
        uint32_t bootloader_addr = get_bootloader_address(chip);
        flash_binary(&loader, bootloader_bin, bootloader_bin_size, bootloader_addr);
        ESP_LOGI(TAG, "Loading partition table...");
        flash_binary(&loader, partition_table_bin, partition_table_bin_size, PARTITION_TABLE_ADDRESS);
        ESP_LOGI(TAG, "Loading app...");
        flash_binary(&loader, app_bin, app_bin_size, APPLICATION_ADDRESS);
        ESP_LOGI(TAG, "Done!");
        esp_loader_reset_target(&loader);

        // Delay for skipping the boot message of the targets
        vTaskDelay(500 / portTICK_PERIOD_MS);

        // Forward slave's serial output
        ESP_LOGI(TAG, "********************************************");
        ESP_LOGI(TAG, "*** Logs below are print from slave .... ***");
        ESP_LOGI(TAG, "********************************************");
        xTaskCreate(slave_monitor, "slave_monitor", 2048, NULL, configMAX_PRIORITIES - 1, NULL);
    }
    vTaskDelete(NULL);
}
