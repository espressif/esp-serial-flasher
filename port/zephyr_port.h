/*
 * Copyright (c) 2026 Espressif Systems (Shanghai) Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/console/tty.h>
#include <stdint.h>
#include <esp_loader_io.h>
#include <esp_loader.h>

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
    uint32_t                baud_rate;
    uint32_t                baud_rate_high;

    /* Private runtime state — do not access directly */
    uint64_t                _time_end;
    struct tty_serial       _tty;
    char                    _tty_rx_buf[CONFIG_ESP_SERIAL_FLASHER_UART_BUFSIZE];
    char                    _tty_tx_buf[CONFIG_ESP_SERIAL_FLASHER_UART_BUFSIZE];
} zephyr_port_t;

zephyr_port_t *esp_loader_interface_default(void);
zephyr_port_t *esp_loader_interface_instance(int);
esp_loader_t *esp_loader_default(void);
esp_loader_t *esp_loader_instance(int);
esp_loader_t *esp_loader_connect_default(bool);
esp_loader_t *esp_loader_connect_instance(int, bool);


#ifdef __cplusplus
}
#endif
