/*
 * SPDX-FileCopyrightText: 2018-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_loader.h"

#ifdef __cplusplus
#include <chrono>
#include <fstream>

/**
 * @brief Concrete TCP test port instance.
 *
 * Embeds esp_loader_port_t as the first member so container_of() works.
 * Declare one globally, call esp_loader_port_test_init() to open the TCP
 * socket, then pass &port.port to esp_loader_init_uart().
 *
 * @code
 *   esp_loader_port_test_init(&test_tcp_port);
 *   esp_loader_t loader;
 *   esp_loader_init_uart(&loader, &test_tcp_port.port);
 * @endcode
 */
struct test_tcp_port_t {
    esp_loader_port_t port;  /*!< Must be first member for container_of */
    int sock;
    std::ofstream file;
    std::chrono::time_point<std::chrono::steady_clock> time_end;
#if SERIAL_FLASHER_DEBUG_TRACE
    bool write_prev;
#endif
};

extern test_tcp_port_t test_tcp_port;

esp_loader_error_t esp_loader_port_test_init(test_tcp_port_t *p);
void esp_loader_port_test_deinit(test_tcp_port_t *p);

#endif /* __cplusplus */
