/* Flash multiple partitions example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pigpio.h>
#include <sys/param.h>
#include "raspberry_port.h"
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

#define TARGET_RST_Pin 2
#define TARGET_IO0_Pin 3

#define DEFAULT_BAUD_RATE 115200
#define HIGHER_BAUD_RATE  460800
#define SERIAL_DEVICE     "/dev/ttyS0"

int main(void)
{

    const loader_raspberry_config_t config = {
        .device = SERIAL_DEVICE,
        .baudrate = DEFAULT_BAUD_RATE,
        .reset_trigger_pin = TARGET_RST_Pin,
        .gpio0_trigger_pin = TARGET_IO0_Pin,
    };

    loader_port_raspberry_init(&config);

    if (connect_to_target(HIGHER_BAUD_RATE) == ESP_LOADER_SUCCESS) {

        printf("Loading bootloader...\n");
        target_chip_t chip = esp_loader_get_target();
        uint32_t bootloader_addr = get_bootloader_address(chip);
        flash_binary(bootloader_bin, bootloader_bin_size, bootloader_addr);
        printf("Loading partition table...\n");
        flash_binary(partition_table_bin, partition_table_bin_size, PARTITION_TABLE_ADDRESS);
        printf("Loading app...\n");
        flash_binary(app_bin, app_bin_size, APPLICATION_ADDRESS);
        printf("Done!\n");
        esp_loader_reset_target();
        loader_port_deinit();


        if (gpioInitialise() < 0) {
            fprintf(stderr, "pigpio initialization failed\n");
            return 1;
        }

        int serial = serOpen(SERIAL_DEVICE, DEFAULT_BAUD_RATE, 0);
        if (serial < 0) {
            printf("Serial port could not be opened!\n");
        }

        printf("********************************************\n");
        printf("*** Logs below are print from slave .... ***\n");
        printf("********************************************\n");

        // Delay for skipping the boot message of the targets
        gpioDelay(500000);
        while (1) {
            int byte = serReadByte(serial);
            if (byte != PI_SER_READ_NO_DATA) {
                printf("%c", byte);
            }
        }
    }

}
