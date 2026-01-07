/* Deflate flash example

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

/* Embedded binary files using bin2array.cmake
 * *_bin: Original uncompressed binary (for verification)
 * *_deflated_bin: Pre-compressed binary (for flashing)
 * *_bin_md5: MD5 hash as hex string (32 chars)
 */
extern const uint8_t bootloader_bin[];
extern const uint32_t bootloader_bin_size;
extern const uint8_t bootloader_deflated_bin[];
extern const uint32_t bootloader_deflated_bin_size;
extern const uint8_t bootloader_bin_md5[];
extern const uint8_t partition_table_bin[];
extern const uint32_t partition_table_bin_size;
extern const uint8_t partition_table_deflated_bin[];
extern const uint32_t partition_table_deflated_bin_size;
extern const uint8_t partition_table_bin_md5[];
extern const uint8_t app_bin[];
extern const uint32_t app_bin_size;
extern const uint8_t app_deflated_bin[];
extern const uint32_t app_deflated_bin_size;
extern const uint8_t app_bin_md5[];

static const char *TAG = "serial_flasher_deflate";

#define HIGHER_BAUDRATE 230400
#define DEFLATE_BLOCK_SIZE 1024

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
        int rxBytes = uart_read_bytes(UART_NUM_1, buf, BUF_LEN - 1, 100 / portTICK_PERIOD_MS);
        if (rxBytes > 0) {
            buf[rxBytes] = '\0';
            printf("%s", buf);
        }
    }
}


static esp_loader_error_t flash_deflated_binary(const uint8_t *compressed_data,
        uint32_t compressed_size,
        uint32_t uncompressed_size,
        uint32_t address)
{
    esp_loader_error_t err;
    static uint8_t payload[DEFLATE_BLOCK_SIZE];

    err = esp_loader_flash_deflate_start(address, uncompressed_size, compressed_size, DEFLATE_BLOCK_SIZE);
    if (err != ESP_LOADER_SUCCESS) {
        ESP_LOGE(TAG, "Failed to start deflate flash (%d)", err);
        return err;
    }

    uint32_t offset = 0;
    uint32_t written = 0;
    while (offset < compressed_size) {
        const uint32_t to_write = MIN(compressed_size - offset, DEFLATE_BLOCK_SIZE);
        memcpy(payload, compressed_data + offset, to_write);

        err = esp_loader_flash_deflate_write(payload, to_write);
        if (err != ESP_LOADER_SUCCESS) {
            ESP_LOGE(TAG, "Failed to write deflate block at offset %u (%d)",
                     (unsigned int)offset, err);
            esp_loader_flash_deflate_finish(false);
            return err;
        }
        offset += to_write;
        written += to_write;

        int progress = (int)(((float)written / compressed_size) * 100);
        printf("\rProgress: %d %%", progress);
    }
    printf("\n");

    err = esp_loader_flash_deflate_finish(false);
    if (err != ESP_LOADER_SUCCESS) {
        ESP_LOGE(TAG, "Failed to finish deflate flash (%d)", err);
    }

    return err;
}

static esp_loader_error_t flash_deflated_and_verify(const char *label,
        const uint8_t *compressed_data,
        uint32_t compressed_size,
        const uint8_t *raw_md5,
        uint32_t raw_size,
        uint32_t address)
{
    esp_loader_error_t err;

    ESP_LOGI(TAG, "Flashing %s via DEFLATE (compressed size: %u, uncompressed: %u)...",
             label, (unsigned int)compressed_size, (unsigned int)raw_size);

    err = flash_deflated_binary(compressed_data, compressed_size, raw_size,
                                address);
    if (err != ESP_LOADER_SUCCESS) {
        ESP_LOGE(TAG, "Compressed flash failed for %s (%d)", label, err);
        return err;
    }

    ESP_LOGI(TAG, "Verifying %s via MD5...", label);
    err = esp_loader_flash_verify_known_md5(address, raw_size, raw_md5);
    if (err != ESP_LOADER_SUCCESS) {
        ESP_LOGE(TAG, "%s MD5 verification failed (%d)", label, err);
        return err;
    }

    ESP_LOGI(TAG, "%s MD5 matches.", label);
    return ESP_LOADER_SUCCESS;
}

void app_main(void)
{
    const loader_esp32_config_t config = {
        .baud_rate = 115200,
        .uart_port = UART_NUM_1,
        .uart_rx_pin = GPIO_NUM_5,
        .uart_tx_pin = GPIO_NUM_4,
        .reset_trigger_pin = GPIO_NUM_25,
        .gpio0_trigger_pin = GPIO_NUM_26,
    };

    if (loader_port_esp32_init(&config) != ESP_LOADER_SUCCESS) {
        ESP_LOGE(TAG, "serial initialization failed.");
        return;
    }

    if (connect_to_target_with_stub(115200, HIGHER_BAUDRATE) == ESP_LOADER_SUCCESS) {
        target_chip_t chip = esp_loader_get_target();
        uint32_t bootloader_addr = get_bootloader_address(chip);
        uint32_t partition_addr = PARTITION_TABLE_ADDRESS;
        uint32_t app_addr = APPLICATION_ADDRESS;

        bool all_ok = true;
        esp_loader_error_t err = flash_deflated_and_verify("bootloader",
                                 bootloader_deflated_bin, bootloader_deflated_bin_size,
                                 bootloader_bin_md5, bootloader_bin_size, bootloader_addr);
        if (err != ESP_LOADER_SUCCESS) {
            all_ok = false;
            ESP_LOGW(TAG, "Continuing despite bootloader failure...");
        }

        err = flash_deflated_and_verify("partition table",
                                        partition_table_deflated_bin, partition_table_deflated_bin_size,
                                        partition_table_bin_md5, partition_table_bin_size, partition_addr);
        if (err != ESP_LOADER_SUCCESS) {
            all_ok = false;
            ESP_LOGW(TAG, "Continuing despite partition table failure...");
        }

        err = flash_deflated_and_verify("application",
                                        app_deflated_bin, app_deflated_bin_size,
                                        app_bin_md5, app_bin_size, app_addr);
        if (err != ESP_LOADER_SUCCESS) {
            all_ok = false;
            ESP_LOGW(TAG, "Continuing despite application failure...");
        }

        if (all_ok) {
            ESP_LOGI(TAG, "All flashes verified. Success!");
            esp_loader_reset_target();

            // Delay for skipping the boot message of the targets
            vTaskDelay(500 / portTICK_PERIOD_MS);

            // Forward slave's serial output
            ESP_LOGI(TAG, "********************************************");
            ESP_LOGI(TAG, "*** Logs below are print from slave .... ***");
            ESP_LOGI(TAG, "********************************************");
            xTaskCreate(slave_monitor, "slave_monitor", 2048, NULL, configMAX_PRIORITIES - 1, NULL);
        } else {
            ESP_LOGE(TAG, "One or more flashes failed verification.");
        }
    }

    vTaskDelete(NULL);
}
