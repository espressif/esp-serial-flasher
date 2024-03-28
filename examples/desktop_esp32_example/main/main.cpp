/* Flash multiple partitions example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "stdio.h"
#include <string.h>

#include "wjwwood_serial_port.h"
#include "esp_loader.h"
#include "esp_loader_io.h"
extern "C" {
#include "example_common.h"

}

#include "serial/serial.h"

#define HIGHER_BAUDRATE 230400

int main(int argv, char **argc)
{
    if(argv < 2) {
        printf("Usage: %s <port>\n", argc[0]);
        return 1;
    }

    example_binaries_t bin;

    loader_wjwwood_serial_config_t config;
    config.portName = argc[1];
    //config.portName = "COM4";
    config.baudrate = 115200;
    config.timeout = 600;

    if (loader_port_wjwwood_serial_init((const loader_wjwwood_serial_config_t*)&config) != ESP_LOADER_SUCCESS) {
        printf("Serial initialization failed.\n");
        return -1;
    }

    printf("Connecting...\n");
    if (connect_to_target(HIGHER_BAUDRATE) == ESP_LOADER_SUCCESS) {

        get_example_binaries(esp_loader_get_target(), &bin);

        printf("Loading bootloader...\n");
        flash_binary(bin.boot.data, bin.boot.size, bin.boot.addr);
        printf("Loading partition table...\n");
        flash_binary(bin.part.data, bin.part.size, bin.part.addr);
        printf("Loading app...\n");
        flash_binary(bin.app.data,  bin.app.size,  bin.app.addr);
        printf("Done!\n");
        loader_port_wjwwood_serial_deinit();
    } else {
        printf("Connect failed\n");
        loader_port_wjwwood_serial_deinit();
        return -1;
    }

    return 0;
}
