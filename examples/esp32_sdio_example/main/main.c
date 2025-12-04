/* Example of flashing the program through SDIO

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
#include "esp32_sdio_port.h"
#include "esp_loader.h"
#include "example_common.h"
#include "freertos/FreeRTOS.h"

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

static const char *TAG = "sdio_ram_loader";

// Max line size
#define BUF_LEN 128
static uint8_t buf[BUF_LEN] = {0};

void slave_monitor(void *arg)
{
    // Initialize UART
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        .source_clk = UART_SCLK_DEFAULT,
#endif
    };

    ESP_ERROR_CHECK(uart_param_config(UART_NUM_2, &uart_config));

    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_2, GPIO_NUM_46, GPIO_NUM_45,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_2, BUF_LEN * 4, BUF_LEN * 4, 0, NULL, 0));

    while (1) {
        int rxBytes = uart_read_bytes(UART_NUM_2, buf, BUF_LEN - 1, 100 / portTICK_PERIOD_MS);
        buf[rxBytes] = '\0';
        printf("%s", buf);
    }
}

void app_main(void)
{

    const loader_esp32_sdio_config_t config = {
        .slot = SDMMC_HOST_SLOT_1,
        .max_freq_khz = SDMMC_FREQ_DEFAULT,
        .reset_trigger_pin = GPIO_NUM_54,
        .boot_pin = GPIO_NUM_53,
        .bus_width = SDIO_4BIT,
        .sdio_d0_pin = GPIO_NUM_50,
        .sdio_d1_pin = GPIO_NUM_49,
        .sdio_d2_pin = GPIO_NUM_48,
        .sdio_d3_pin = GPIO_NUM_47,
        .sdio_clk_pin = GPIO_NUM_51,
        .sdio_cmd_pin = GPIO_NUM_52,
    };

    if (loader_port_esp32_sdio_init(&config) != ESP_LOADER_SUCCESS) {
        ESP_LOGE(TAG, " SDIO initialization failed.");
        abort();
    }

    if (connect_to_target(0) == ESP_LOADER_SUCCESS) {

        ESP_LOGI(TAG, "Loading bootloader...");
        target_chip_t chip = esp_loader_get_target();
        uint32_t bootloader_addr = get_bootloader_address(chip);
        flash_binary(bootloader_bin, bootloader_bin_size, bootloader_addr);
        ESP_LOGI(TAG, "Loading partition table...");
        flash_binary(partition_table_bin, partition_table_bin_size, PARTITION_TABLE_ADDRESS);
        ESP_LOGI(TAG, "Loading app...");
        flash_binary(app_bin, app_bin_size, APPLICATION_ADDRESS);
        ESP_LOGI(TAG, "Done!");
        esp_loader_reset_target();

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
