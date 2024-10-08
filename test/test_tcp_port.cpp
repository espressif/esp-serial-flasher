/* Copyright 2018-2024 Espressif Systems (Shanghai) CO LTD
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

#include "esp_loader_io.h"
#include "test_port.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

#include <chrono>
#include <thread>

#include <iostream>
#include <fstream>

using namespace std;

const uint32_t PORT = 5555;
static int sock = 0;
ofstream file;
static chrono::time_point<chrono::steady_clock> s_time_end;

#if SERIAL_FLASHER_DEBUG_TRACE
static void transfer_debug_print(const uint8_t *data, uint16_t size, bool write)
{
    static bool write_prev = false;

    if (write_prev != write) {
        write_prev = write;
        cout << endl << "--- " << (write ? "WRITE" : "READ") << " ---" << endl;
    }

    for (uint32_t i = 0; i < size; i++) {
        cout << hex << static_cast<unsigned int>(data[i]) << dec << ' ';
    }
}
#endif

esp_loader_error_t loader_port_test_init(const loader_serial_config_t *config)
{
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        cout << "Socket creation error \n";
        return ESP_LOADER_ERROR_FAIL;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        cout << "Invalid address/ Address not supported \n";
        return ESP_LOADER_ERROR_FAIL;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        cout << "Connection Failed \n";
        return ESP_LOADER_ERROR_FAIL;
    }

    file.open ("received_data.bin", ios::binary | ios::out);
    if (!file.is_open()) {
        cout << "Cannot open file\n";
        return ESP_LOADER_ERROR_FAIL;
    }

    return ESP_LOADER_SUCCESS;
}


void loader_port_test_deinit()
{
    if (sock != 0) {
        shutdown(sock, 0);
        close(sock);
    }

    if (file.is_open()) {
        file.close();
    }
}


esp_loader_error_t loader_port_write(const uint8_t *data, uint16_t size, uint32_t timeout)
{
    uint32_t written = 0;

    do {
        int bytes_written = send(sock, &data[written], size - written, 0);
        if (bytes_written == -1) {
            cout << "Socket send failed\n";
            return ESP_LOADER_ERROR_FAIL;
        }
#if SERIAL_FLASHER_DEBUG_TRACE
        transfer_debug_print(data, bytes_written, true);
#endif
        written += bytes_written;
    } while (written != size);
    return ESP_LOADER_SUCCESS;
}

esp_loader_error_t loader_port_read(uint8_t *data, uint16_t size, uint32_t timeout)
{
    // Timeout is specified in milliseconds, split to seconds and microsecond remainder
    const struct timeval timeout_values = {
        .tv_sec = timeout / 1000,
        .tv_usec = (timeout % 1000) * 1000
    };

    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
                   (const char *)&timeout_values, sizeof(timeout_values)) != 0) {
        cout << "Could not set socket read timeout\n";
        return ESP_LOADER_ERROR_FAIL;
    }

    const int bytes_read = read(sock, data, size);

    if (bytes_read != size) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            cout << "A socket read timeout occurred\n";
            return ESP_LOADER_ERROR_TIMEOUT;
        } else {
            cout << "Socket connection lost\n";
            return ESP_LOADER_ERROR_FAIL;
        }
    }

#if SERIAL_FLASHER_DEBUG_TRACE
    transfer_debug_print(data, bytes_read, false);
#endif

    file.write((const char *)data, size);
    file.flush();

    return ESP_LOADER_SUCCESS;
}

void loader_port_enter_bootloader()
{
    // GPIO0 and GPIO2 must be LOW
    // Then Reset
}

void loader_port_reset_target()
{
    // Toggle reset pin
}

void loader_port_delay_ms(uint32_t ms)
{
    this_thread::sleep_for(chrono::milliseconds(ms));
}

void loader_port_start_timer(uint32_t ms)
{
    s_time_end = chrono::steady_clock::now() + chrono::milliseconds(ms);
}


uint32_t loader_port_remaining_time(void)
{
    const auto remaining = s_time_end - chrono::steady_clock::now();

    const auto remaining_ms = chrono::duration_cast<chrono::milliseconds>(remaining).count();

    return (remaining_ms > 0) ? (uint32_t)remaining_ms : 0;
}
