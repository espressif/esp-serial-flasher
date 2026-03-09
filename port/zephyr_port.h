/*
 * Copyright (c) 2022 KT-Elektronik, Klaucke und Partner GmbH
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

#include "esp_loader_io.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/console/tty.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Concrete Zephyr UART port instance.
 *
 * Declare one of these, fill the config fields, then pass &port.port to
 * esp_loader_init_uart(). Hardware initialisation is called automatically
 * inside esp_loader_init_uart() — no separate init step is needed.
 *
 * @code
 *   zephyr_port_t port = {
 *       .port.ops    = &zephyr_uart_ops,
 *       .uart_dev    = DEVICE_DT_GET(DT_ALIAS(uart)),
 *       .enable_spec = GPIO_DT_SPEC_GET(DT_ALIAS(en), gpios),
 *       .boot_spec   = GPIO_DT_SPEC_GET(DT_ALIAS(boot), gpios),
 *   };
 *   esp_loader_t loader;
 *   esp_loader_init_uart(&loader, &port.port);
 * @endcode
 */
typedef struct {
    esp_loader_port_t       port;        /*!< Embedded port base */

    /* Configuration — fill before calling esp_loader_init_uart() */
    const struct device    *uart_dev;
    struct gpio_dt_spec     enable_spec;
    struct gpio_dt_spec     boot_spec;

    /* Private runtime state — do not access directly */
    uint64_t                _time_end;
    struct tty_serial       _tty;
    char                    _tty_rx_buf[CONFIG_ESP_SERIAL_FLASHER_UART_BUFSIZE];
    char                    _tty_tx_buf[CONFIG_ESP_SERIAL_FLASHER_UART_BUFSIZE];
} zephyr_port_t;

/** Port operations vtable for the Zephyr UART port. */
extern const esp_loader_port_ops_t zephyr_uart_ops;

#ifdef __cplusplus
}
#endif
