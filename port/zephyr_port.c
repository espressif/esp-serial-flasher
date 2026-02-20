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

#include "zephyr_port.h"
#include <zephyr/drivers/uart.h>
#include <zephyr/console/tty.h>

static const struct device *uart_dev;
static struct gpio_dt_spec enable_spec;
static struct gpio_dt_spec boot_spec;

static struct tty_serial tty;
static char tty_rx_buf[CONFIG_ESP_SERIAL_FLASHER_UART_BUFSIZE];
static char tty_tx_buf[CONFIG_ESP_SERIAL_FLASHER_UART_BUFSIZE];

#if SERIAL_FLASHER_DEBUG_TRACE
static void transfer_debug_print(const uint8_t *data, uint16_t size, bool write)
{
    static bool write_prev = false;

    if (write_prev != write) {
        write_prev = write;
        printk("\n--- %s ---\n", write ? "WRITE" : "READ");
    }

    for (uint32_t i = 0; i < size; i++) {
        printk("%02x ", data[i]);
    }
}
#endif

static uint64_t s_time_end;

esp_loader_error_t configure_tty()
{
    if (tty_init(&tty, uart_dev) < 0 ||
            tty_set_rx_buf(&tty, tty_rx_buf, sizeof(tty_rx_buf)) < 0 ||
            tty_set_tx_buf(&tty, tty_tx_buf, sizeof(tty_tx_buf)) < 0) {
        return ESP_LOADER_ERROR_FAIL;
    }

    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t zephyr_uart_read(esp_loader_port_t *port, uint8_t *data, const uint16_t size, const uint32_t timeout)
{
    (void)port;
    if (!device_is_ready(uart_dev) || data == NULL || size == 0) {
        return ESP_LOADER_ERROR_FAIL;
    }

    ssize_t total_read = 0;
    ssize_t remaining = size;

    tty_set_rx_timeout(&tty, timeout);
    while (remaining > 0) {
        const uint16_t chunk_size = remaining < CONFIG_ESP_SERIAL_FLASHER_UART_BUFSIZE ?
                                    remaining : CONFIG_ESP_SERIAL_FLASHER_UART_BUFSIZE;
        ssize_t read = tty_read(&tty, &data[total_read], chunk_size);
        if (read < 0) {
            return ESP_LOADER_ERROR_TIMEOUT;
        }
#if SERIAL_FLASHER_DEBUG_TRACE
        transfer_debug_print(data, read, false);
#endif
        total_read += read;
        remaining -= read;
    }

    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t zephyr_uart_write(esp_loader_port_t *port, const uint8_t *data, const uint16_t size, const uint32_t timeout)
{
    (void)port;
    if (!device_is_ready(uart_dev) || data == NULL || size == 0) {
        return ESP_LOADER_ERROR_FAIL;
    }

    ssize_t total_written = 0;
    ssize_t remaining = size;

    tty_set_tx_timeout(&tty, timeout);
    while (remaining > 0) {
        const uint16_t chunk_size = remaining < CONFIG_ESP_SERIAL_FLASHER_UART_BUFSIZE ?
                                    remaining : CONFIG_ESP_SERIAL_FLASHER_UART_BUFSIZE;
        ssize_t written = tty_write(&tty, &data[total_written], chunk_size);
        if (written < 0) {
            return ESP_LOADER_ERROR_TIMEOUT;
        }
#if SERIAL_FLASHER_DEBUG_TRACE
        transfer_debug_print(data, written, true);
#endif
        total_written += written;
        remaining -= written;
    }

    return ESP_LOADER_SUCCESS;
}

esp_loader_error_t loader_port_zephyr_init(const loader_zephyr_config_t *config)
{
    uart_dev = config->uart_dev;
    enable_spec = config->enable_spec;
    boot_spec = config->boot_spec;
    return configure_tty();
}

static void zephyr_uart_delay_ms(esp_loader_port_t *port, uint32_t ms)
{
    (void)port;
    k_msleep(ms);
}

static void zephyr_uart_start_timer(esp_loader_port_t *port, uint32_t ms)
{
    (void)port;
    s_time_end = sys_timepoint_calc(Z_TIMEOUT_MS(ms)).tick;
}

static uint32_t zephyr_uart_remaining_time(esp_loader_port_t *port)
{
    (void)port;
    int64_t remaining = k_ticks_to_ms_floor64(s_time_end - k_uptime_ticks());
    return (remaining > 0) ? (uint32_t)remaining : 0;
}

static void zephyr_uart_reset_target(esp_loader_port_t *port)
{
    (void)port;
    gpio_pin_set_dt(&enable_spec, SERIAL_FLASHER_RESET_INVERT ? true : false);
    k_msleep(CONFIG_SERIAL_FLASHER_RESET_HOLD_TIME_MS);
    gpio_pin_set_dt(&enable_spec, SERIAL_FLASHER_RESET_INVERT ? false : true);
}

static void zephyr_uart_enter_bootloader(esp_loader_port_t *port)
{
    (void)port;
    gpio_pin_set_dt(&boot_spec, SERIAL_FLASHER_BOOT_INVERT ? true : false);
    gpio_pin_set_dt(&enable_spec, SERIAL_FLASHER_RESET_INVERT ? true : false);
    k_msleep(CONFIG_SERIAL_FLASHER_RESET_HOLD_TIME_MS);
    gpio_pin_set_dt(&enable_spec, SERIAL_FLASHER_RESET_INVERT ? false : true);
    k_msleep(CONFIG_SERIAL_FLASHER_BOOT_HOLD_TIME_MS);
    gpio_pin_set_dt(&boot_spec, SERIAL_FLASHER_BOOT_INVERT ? false : true);
}

static esp_loader_error_t zephyr_uart_change_rate(esp_loader_port_t *port, uint32_t baudrate)
{
    (void)port;
    struct uart_config uart_config;

    if (!device_is_ready(uart_dev)) {
        return ESP_LOADER_ERROR_FAIL;
    }

    if (uart_config_get(uart_dev, &uart_config) != 0) {
        return ESP_LOADER_ERROR_FAIL;
    }
    uart_config.baudrate = baudrate;

    if (uart_configure(uart_dev, &uart_config) != 0) {
        return ESP_LOADER_ERROR_FAIL;
    }

    /* bitrate-change can require tty re-configuration */
    return configure_tty();
}

static const esp_loader_port_ops_t zephyr_uart_ops = {
    .enter_bootloader         = zephyr_uart_enter_bootloader,
    .reset_target             = zephyr_uart_reset_target,
    .start_timer              = zephyr_uart_start_timer,
    .remaining_time           = zephyr_uart_remaining_time,
    .delay_ms                 = zephyr_uart_delay_ms,
    .debug_print              = NULL,
    .change_transmission_rate = zephyr_uart_change_rate,
    .write                    = zephyr_uart_write,
    .read                     = zephyr_uart_read,
};

esp_loader_port_t zephyr_uart_port = { .ops = &zephyr_uart_ops };
