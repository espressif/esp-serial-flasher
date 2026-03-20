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

#pragma once

#include <stdint.h>
#include "esp_loader_io.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Concrete Raspberry Pi UART port instance.
 *
 * Declare one of these, fill the config fields, then pass &port.port to
 * esp_loader_init_uart(). Hardware initialisation is called automatically
 * inside esp_loader_init_uart() — no separate init step is needed.
 *
 * @code
 *   raspi_port_t port = {
 *       .port.ops          = &raspi_uart_ops,
 *       .device            = "/dev/ttyS0",
 *       .baudrate          = 115200,
 *       .reset_trigger_pin = TARGET_RST_Pin,
 *       .gpio0_trigger_pin = TARGET_IO0_Pin,
 *   };
 *   esp_loader_t loader;
 *   esp_loader_init_uart(&loader, &port.port);
 * @endcode
 */
typedef struct {
    esp_loader_port_t port;          /*!< Embedded port base */

    /* Configuration — fill before calling esp_loader_init_uart() */
    const char *device;
    uint32_t    baudrate;
    uint32_t    reset_trigger_pin;
    uint32_t    gpio0_trigger_pin;

    /* Private runtime state — do not access directly */
    int     _serial;
    int64_t _time_end;
} raspi_port_t;

/** Port operations vtable for the Raspberry Pi UART port. */
extern const esp_loader_port_ops_t raspi_uart_ops;

#ifdef __cplusplus
}
#endif
