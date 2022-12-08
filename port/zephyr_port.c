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

#define MSG_SIZE 512

static struct tty_serial tty;
static char tty_rx_buf[MSG_SIZE];
static char tty_tx_buf[MSG_SIZE];

void configure_tty()
{
    tty_init(&tty, uart_dev);
    tty_set_rx_buf(&tty, tty_rx_buf, sizeof(tty_rx_buf));
    tty_set_tx_buf(&tty, tty_tx_buf, sizeof(tty_tx_buf));
}

esp_loader_error_t loader_port_serial_read(uint8_t *data, uint16_t size, uint32_t timeout)
{
    if (!device_is_ready(uart_dev) || data == NULL || size == 0) {
        return ESP_LOADER_ERROR_FAIL;
    }

    ssize_t bytes_read = 0;

    tty_set_rx_timeout(&tty, timeout);
    bytes_read = tty_read(&tty, data, size);
    if (bytes_read < 0 || bytes_read < size) {
        return ESP_LOADER_ERROR_TIMEOUT;
    }

    return ESP_LOADER_SUCCESS;
}

esp_loader_error_t loader_port_serial_write(const uint8_t *data, uint16_t size, uint32_t timeout)
{
    if (!device_is_ready(uart_dev) || data == NULL) {
        return ESP_LOADER_ERROR_FAIL;
    }

    ssize_t bytes_written = 0;

    tty_set_tx_timeout(&tty, timeout);
    bytes_written = tty_write(&tty, data, size);
    if (bytes_written < 0 || size != bytes_written) {
        return ESP_LOADER_ERROR_TIMEOUT;
    }

    return ESP_LOADER_SUCCESS;
}

void loader_port_zephyr_init(loader_zephyr_config_t *config)
{
    uart_dev = config->uart_dev;
    enable_spec = config->enable_spec;
    boot_spec = config->boot_spec;
    configure_tty();
}

void loader_port_reset_target(void)
{
    gpio_pin_set_dt(&enable_spec, false);
    loader_port_delay_ms(50);
    gpio_pin_set_dt(&enable_spec, true);
}

void loader_port_enter_bootloader(void)
{
    gpio_pin_set_dt(&boot_spec, false);
    loader_port_reset_target();
    loader_port_delay_ms(50);
    gpio_pin_set_dt(&boot_spec, true);
}

void loader_port_delay_ms(uint32_t ms)
{
    k_msleep(ms);
}

static uint64_t s_time_end;

void loader_port_start_timer(uint32_t ms)
{
    s_time_end = sys_clock_timeout_end_calc(Z_TIMEOUT_MS(ms));
}

uint32_t loader_port_remaining_time(void)
{
    int64_t remaining = k_ticks_to_ms_floor64(s_time_end - k_uptime_ticks());
    return (remaining > 0) ? (uint32_t)remaining : 0;
}

esp_loader_error_t loader_port_change_baudrate(uint32_t baudrate)
{
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
    configure_tty();

    return ESP_LOADER_SUCCESS;
}

