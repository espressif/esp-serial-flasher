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

static const char *TAG = "serial_flasher";
static const char example_data[] = "To be, or not to be: that is the question: "
                                   "Whether 'tis nobler in the mind to suffer "
                                   "The slings and arrows of outrageous fortune, "
                                   "Or to take arms against a sea of troubles, "
                                   "And by opposing end them? To die: to sleep; "
                                   "No more; and by a sleep to say we end "
                                   "The heart-ache and the thousand natural shocks "
                                   "That flesh is heir to, 'tis a consummation "
                                   "Devoutly to be wish'd. To die, to sleep; "
                                   "To sleep: perchance to dream: ay, there's the rub; "
                                   "For in that sleep of death what dreams may come "
                                   "When we have shuffled off this mortal coil, "
                                   "Must give us pause: there's the respect "
                                   "That makes calamity of so long life; "
                                   "For who would bear the whips and scorns of time, "
                                   "The oppressor's wrong, the proud man's contumely, "
                                   "The pangs of despised love, the law's delay, "
                                   "The insolence of office and the spurns "
                                   "That patient merit of the unworthy takes, "
                                   "When he himself might his quietus make "
                                   "With a bare bodkin? who would fardels bear, "
                                   "To grunt and sweat under a weary life";
static uint8_t read_buf[sizeof(example_data)];

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

        esp_loader_error_t err;
        err = esp_loader_flash_erase();
        if (err != ESP_LOADER_SUCCESS) {
            ESP_LOGE(TAG, "Failed to erase flash");
            return;
        }

        ESP_LOGI(TAG, "Loading example data");
        flash_binary((const uint8_t *)example_data, sizeof(example_data), 0x00000000);

        if (esp_loader_flash_read(read_buf, 0x00000000, sizeof(read_buf)) == ESP_LOADER_SUCCESS) {
            if (!memcmp(example_data, read_buf, sizeof(read_buf))) {
                ESP_LOGI(TAG, "Flash contents match example data");
            } else {
                ESP_LOGE(TAG, "Flash contents do not match example data");
                ESP_LOG_BUFFER_HEXDUMP("Programmed data: ", example_data, sizeof(read_buf), ESP_LOG_ERROR);
                ESP_LOG_BUFFER_HEXDUMP("Read data: ", read_buf, sizeof(read_buf), ESP_LOG_ERROR);
            }
        } else {
            ESP_LOGE(TAG, "Could not read from flash!");
        }


    }
    vTaskDelete(NULL);
}
