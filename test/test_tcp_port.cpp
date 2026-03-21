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

// Global test port instance — sock=0, file not open, time_end=epoch by default
test_tcp_port_t test_tcp_port;

/*
 * test_tcp_port_t has non-standard-layout members (std::ofstream) so
 * container_of / offsetof trigger -Werror=invalid-offsetof in C++.
 * Since esp_loader_port_t is always the FIRST member (offset == 0), a
 * double static_cast through void * gives the same result with no UB.
 */
static inline test_tcp_port_t *port_instance(esp_loader_port_t *base)
{
    return static_cast<test_tcp_port_t *>(static_cast<void *>(base));
}

#if SERIAL_FLASHER_DEBUG_TRACE
static void transfer_debug_print(test_tcp_port_t *p, const uint8_t *data, uint16_t size, bool write)
{
    if (p->write_prev != write) {
        p->write_prev = write;
        cout << endl << "--- " << (write ? "WRITE" : "READ") << " ---" << endl;
    }

    for (uint32_t i = 0; i < size; i++) {
        cout << hex << static_cast<unsigned int>(data[i]) << dec << ' ';
    }
}
#endif

static esp_loader_error_t test_port_write(esp_loader_port_t *port, const uint8_t *data, uint16_t size, uint32_t timeout)
{
    test_tcp_port_t *p = port_instance(port);
    (void)timeout;
    uint32_t written = 0;

    do {
        int bytes_written = send(p->sock, &data[written], size - written, 0);
        if (bytes_written == -1) {
            cout << "Socket send failed\n";
            return ESP_LOADER_ERROR_FAIL;
        }
#if SERIAL_FLASHER_DEBUG_TRACE
        transfer_debug_print(p, data, bytes_written, true);
#endif
        written += bytes_written;
    } while (written != size);

    // Give QEMU time to complete the async MTD write before the next command arrives,
    // otherwise the ROM may respond with INVALID_CRC or INVALID_COMMAND.
    this_thread::sleep_for(chrono::milliseconds(10));
    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t test_port_read(esp_loader_port_t *port, uint8_t *data, uint16_t size, uint32_t timeout)
{
    test_tcp_port_t *p = port_instance(port);
    const struct timeval timeout_values = {
        .tv_sec  = timeout / 1000,
        .tv_usec = (timeout % 1000) * 1000
    };

    if (setsockopt(p->sock, SOL_SOCKET, SO_RCVTIMEO,
                   (const char *)&timeout_values, sizeof(timeout_values)) != 0) {
        cout << "Could not set socket read timeout\n";
        return ESP_LOADER_ERROR_FAIL;
    }

    const int bytes_read = read(p->sock, data, size);

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
    transfer_debug_print(p, data, bytes_read, false);
#endif

    p->file.write((const char *)data, size);
    p->file.flush();

    return ESP_LOADER_SUCCESS;
}

static void test_port_enter_bootloader(esp_loader_port_t *port)
{
    (void)port;
    // GPIO0 and GPIO2 must be LOW, then Reset
}

static void test_port_reset_target(esp_loader_port_t *port)
{
    (void)port;
    // Toggle reset pin
}

static void test_port_delay_ms(esp_loader_port_t *port, uint32_t ms)
{
    (void)port;
    this_thread::sleep_for(chrono::milliseconds(ms));
}

static void test_port_start_timer(esp_loader_port_t *port, uint32_t ms)
{
    test_tcp_port_t *p = port_instance(port);
    p->time_end = chrono::steady_clock::now() + chrono::milliseconds(ms);
}

static uint32_t test_port_remaining_time(esp_loader_port_t *port)
{
    test_tcp_port_t *p = port_instance(port);
    const auto remaining = p->time_end - chrono::steady_clock::now();
    const auto remaining_ms = chrono::duration_cast<chrono::milliseconds>(remaining).count();
    return (remaining_ms > 0) ? (uint32_t)remaining_ms : 0;
}

/*
 * Positional initialization is used here instead of C99 designated initializers
 * because this file is compiled as C++14 which does not support them.
 * The order must match esp_loader_port_ops_t in esp_loader_io.h exactly.
 */
static const esp_loader_port_ops_t test_port_ops = {
    /* init                     = */ nullptr,
    /* deinit                   = */ nullptr,
    /* enter_bootloader         = */ test_port_enter_bootloader,
    /* reset_target             = */ test_port_reset_target,
    /* start_timer              = */ test_port_start_timer,
    /* remaining_time           = */ test_port_remaining_time,
    /* delay_ms                 = */ test_port_delay_ms,
    /* debug_print              = */ nullptr,
    /* change_transmission_rate = */ nullptr,
    /* write                    = */ test_port_write,
    /* read                     = */ test_port_read,
    /* spi_set_cs               = */ nullptr,
    /* sdio_write               = */ nullptr,
    /* sdio_read                = */ nullptr,
    /* sdio_card_init           = */ nullptr,
};

esp_loader_error_t esp_loader_port_test_init(test_tcp_port_t *p)
{
    p->port.ops = &test_port_ops;

    struct sockaddr_in serv_addr;

    if ((p->sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        cout << "Socket creation error\n";
        return ESP_LOADER_ERROR_FAIL;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        cout << "Invalid address / Address not supported\n";
        return ESP_LOADER_ERROR_FAIL;
    }

    if (connect(p->sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        cout << "Connection failed\n";
        return ESP_LOADER_ERROR_FAIL;
    }

    p->file.open("received_data.bin", ios::binary | ios::out);
    if (!p->file.is_open()) {
        cout << "Cannot open file\n";
        return ESP_LOADER_ERROR_FAIL;
    }

    return ESP_LOADER_SUCCESS;
}

void esp_loader_port_test_deinit(test_tcp_port_t *p)
{
    if (p->sock != 0) {
        shutdown(p->sock, 0);
        close(p->sock);
        p->sock = 0;
    }

    if (p->file.is_open()) {
        p->file.close();
    }
}
