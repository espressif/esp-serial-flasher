/* Copyright 2020-2023 Espressif Systems (Shanghai) CO LTD
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

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <thread>
#include <chrono>
#include <memory>
#include "serial/serial.h"

#include "serial_port.h"

using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::duration;
using std::chrono::milliseconds;
using namespace std::chrono_literals;

loader_serial_config_t serial_config;
std::unique_ptr<serial::Serial> serial_port;
std::chrono::steady_clock::time_point serial_timer;

#ifdef SERIAL_FLASHER_DEBUG_TRACE
static void transfer_debug_print(const uint8_t *data, uint16_t size, bool write)
{
    static bool write_prev = false;

    if (write_prev != write) {
        write_prev = write;
        printf("\n--- %s ---\n", write ? "WRITE" : "READ");
    }

    for (uint32_t i = 0; i < size; i++) {
        printf("%02x ", data[i]);
    }
}
#endif

void setTimeout(uint32_t timeout)
{
    if(timeout == serial_config.timeout)
        return;

    if(!serial_port || !serial_port->isOpen())
        return;

    serial_port->setTimeout(serial::Timeout::simpleTimeout(timeout));
    serial_config.timeout = timeout;
}

esp_loader_error_t loader_port_write(const uint8_t *data, uint16_t size, uint32_t timeout)
{
    try {
        if(!serial_port || !serial_port->isOpen())
            return ESP_LOADER_ERROR_FAIL;

        setTimeout(timeout);
        size_t result = serial_port->write(data, size);
        if (result != size) {
            return ESP_LOADER_ERROR_FAIL;
        }
    } catch (std::exception &e){
        loader_port_debug_print(e.what());
        return ESP_LOADER_ERROR_FAIL;
    }

    #ifdef SERIAL_FLASHER_DEBUG_TRACE
            transfer_debug_print(data, size, true);
    #endif

    return ESP_LOADER_SUCCESS;
}


esp_loader_error_t loader_port_read(uint8_t *data, uint16_t size, uint32_t timeout)
{
    try {
        if(!serial_port || !serial_port->isOpen())
            return ESP_LOADER_ERROR_FAIL;

        setTimeout(timeout);
        size_t result = serial_port->read(data, size);
        if (result != size) {
            return ESP_LOADER_ERROR_FAIL;
        }
    } catch (std::exception &e){
        loader_port_debug_print(e.what());
        return ESP_LOADER_ERROR_FAIL;
    }

    #ifdef SERIAL_FLASHER_DEBUG_TRACE
        transfer_debug_print(data, size, true);
    #endif

    return ESP_LOADER_SUCCESS;
}

esp_loader_error_t loader_port_serial_init(const loader_serial_config_t *config)
{
    serial_config = *config;

    try {
        serial_port = std::make_unique<serial::Serial>(serial_config.portName, config->baudrate, serial::Timeout::simpleTimeout(config->timeout));
        if ( serial_port->isOpen() == false ) {
            return ESP_LOADER_ERROR_FAIL;
        }
    } catch (std::exception &e){
        loader_port_debug_print(e.what());
        return ESP_LOADER_ERROR_FAIL;
    }
    
    return ESP_LOADER_SUCCESS;
}

// Set GPIO0 LOW, then
// assert reset pin for 100 milliseconds.
void loader_port_enter_bootloader(void)
{
    // todo
    loader_port_reset_target();
}


void loader_port_reset_target(void)
{
    // todo
}


void loader_port_delay_ms(uint32_t ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}


void loader_port_start_timer(uint32_t ms)
{
    serial_timer = high_resolution_clock::now() + (ms * 1ms);
}


uint32_t loader_port_remaining_time(void)
{
    auto time_now = high_resolution_clock::now();
    int32_t remaining = (duration_cast<milliseconds>(serial_timer - time_now)).count();
    return (remaining > 0) ? (uint32_t)remaining : 0;
}


void loader_port_debug_print(const char *str)
{
    printf("DEBUG: %s", str);
}

esp_loader_error_t loader_port_change_transmission_rate(uint32_t baudrate)
{
    if(!serial_port || !serial_port->isOpen())
        return ESP_LOADER_ERROR_FAIL;
    
    try {
        serial_port->setBaudrate(baudrate);
    } catch (std::exception &e){
        loader_port_debug_print(e.what());
        return ESP_LOADER_ERROR_FAIL;
    }

    return ESP_LOADER_SUCCESS;
}
