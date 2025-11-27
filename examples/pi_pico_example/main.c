/* Copyright 2020-2024 Espressif Systems (Shanghai) CO LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "pi_pico_port.h"
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

#define HIGHER_BAUDRATE 230400

int main(void)
{

    const loader_pi_pico_config_t config = {
        .uart_inst = uart1,
        .baudrate = 115200,
        .uart_rx_pin_num = 21,
        .uart_tx_pin_num = 20,
        .reset_trigger_pin_num = 19,
        .boot_pin_num = 18,
    };

    stdio_init_all();

    loader_port_pi_pico_init(&config);

    // delay for the test to have time to connect to the device
    sleep_ms(500);

    if (connect_to_target(HIGHER_BAUDRATE) == ESP_LOADER_SUCCESS) {

        printf("Loading bootloader...");
        target_chip_t chip = esp_loader_get_target();
        uint32_t bootloader_addr = get_bootloader_address(chip);
        flash_binary(bootloader_bin, bootloader_bin_size, bootloader_addr);
        printf("Loading partition table...");
        flash_binary(partition_table_bin, partition_table_bin_size, PARTITION_TABLE_ADDRESS);
        printf("Loading app...");
        flash_binary(app_bin, app_bin_size, APPLICATION_ADDRESS);
        printf("Done!");

        esp_loader_reset_target();
        loader_port_pi_pico_deinit();

        // Delay for skipping the boot message of the targets
        sleep_ms(500);
        uart_init(uart1, 115200);
        gpio_set_function(20, GPIO_FUNC_UART);
        gpio_set_function(21, GPIO_FUNC_UART);

        printf("********************************************\n");
        printf("*** Logs below are print from slave .... ***\n");
        printf("********************************************\n");
        char ch;
        while (true) {
            if (uart_is_readable(uart1)) {
                ch = uart_getc(uart1);
                printf("%c", ch);
            } else {
                tight_loop_contents();
            }
        }
    }

    while (true) {
        tight_loop_contents();
    }
}
