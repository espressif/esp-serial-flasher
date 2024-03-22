/* Get target info example

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

static const char *TAG = "serial_flasher";

#define HIGHER_BAUDRATE 230400

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

    if (connect_to_target(HIGHER_BAUDRATE) == ESP_LOADER_SUCCESS) {

        uint32_t flash_size = 0;
        if (esp_loader_flash_detect_size(&flash_size) == ESP_LOADER_SUCCESS) {
            ESP_LOGI(TAG, "Target flash size [B]: %u", (unsigned int)flash_size);
        } else {
            ESP_LOGE(TAG, "Could not read flash size!");
        }

        uint8_t mac[6] = {0};
        if (esp_loader_read_mac(mac) == ESP_LOADER_SUCCESS) {
            ESP_LOGI(TAG, "Target WIFI MAC:");
            ESP_LOG_BUFFER_HEX(TAG, mac, sizeof(mac));
        } else {
            ESP_LOGE(TAG, "Could not read WIFI MAC!");
        }
    }
}
