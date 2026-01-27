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

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/console/tty.h>

#include <zephyr_port.h>
#include <esp_loader_io.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(esf, CONFIG_ESP_SERIAL_FLASHER_LOG_LEVEL);

/* Get chip interface from the DTS */
#define ESF_NODE DT_NODELABEL(esp_loader0)
static const struct device *uart_dev = DEVICE_DT_GET(DT_PHANDLE(ESF_NODE, uart));
static struct gpio_dt_spec reset_spec = GPIO_DT_SPEC_GET(ESF_NODE, reset_gpios);
static struct gpio_dt_spec boot_spec = GPIO_DT_SPEC_GET(ESF_NODE, boot_gpios);

static struct tty_serial tty;
static char tty_rx_buf[CONFIG_ESP_SERIAL_FLASHER_UART_BUFSIZE];
static char tty_tx_buf[CONFIG_ESP_SERIAL_FLASHER_UART_BUFSIZE];

esp_loader_error_t configure_tty()
{
    if (tty_init(&tty, uart_dev) < 0 ||
            tty_set_rx_buf(&tty, tty_rx_buf, sizeof(tty_rx_buf)) < 0 ||
            tty_set_tx_buf(&tty, tty_tx_buf, sizeof(tty_tx_buf)) < 0) {
        return ESP_LOADER_ERROR_FAIL;
    }

    return ESP_LOADER_SUCCESS;
}

esp_loader_error_t loader_port_read(uint8_t *data, const uint16_t size, const uint32_t timeout)
{
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
        LOG_HEXDUMP_DBG(data, read, "READ");

        total_read += read;
        remaining -= read;
    }

    return ESP_LOADER_SUCCESS;
}

esp_loader_error_t loader_port_write(const uint8_t *data, const uint16_t size, const uint32_t timeout)
{
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
        LOG_HEXDUMP_DBG(data, written, "WRITE");

        total_written += written;
        remaining -= written;
    }

    return ESP_LOADER_SUCCESS;
}

void loader_port_reset_target(void)
{
#ifdef CONFIG_SERIAL_FLASHER_RESET_INVERT
    gpio_pin_set_dt(&reset_spec, 0);
#else
    gpio_pin_set_dt(&reset_spec, 1);
#endif
    loader_port_delay_ms(CONFIG_SERIAL_FLASHER_RESET_HOLD_TIME_MS);

#ifdef CONFIG_SERIAL_FLASHER_RESET_INVERT
    gpio_pin_set_dt(&reset_spec, 1);
#else
    gpio_pin_set_dt(&reset_spec, 0);
#endif
}

void loader_port_enter_bootloader(void)
{
#ifdef CONFIG_SERIAL_FLASHER_BOOT_INVERT
    gpio_pin_set_dt(&boot_spec, 0);
#else
    gpio_pin_set_dt(&boot_spec, 1);
#endif

    loader_port_reset_target();
    loader_port_delay_ms(CONFIG_SERIAL_FLASHER_BOOT_HOLD_TIME_MS);

#ifdef CONFIG_SERIAL_FLASHER_BOOT_INVERT
    gpio_pin_set_dt(&boot_spec, 1);
#else
    gpio_pin_set_dt(&boot_spec, 0);
#endif
}

void loader_port_delay_ms(uint32_t ms)
{
    k_msleep(ms);
}

static uint64_t s_time_end;

void loader_port_start_timer(uint32_t ms)
{
    s_time_end = sys_timepoint_calc(Z_TIMEOUT_MS(ms)).tick;
}

uint32_t loader_port_remaining_time(void)
{
    int64_t remaining = k_ticks_to_ms_floor64(s_time_end - k_uptime_ticks());
    return (remaining > 0) ? (uint32_t)remaining : 0;
}

esp_loader_error_t loader_port_change_transmission_rate(uint32_t baudrate)
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
    return configure_tty();
}

esp_loader_error_t loader_port_zephyr_init(void)
{
    if (!device_is_ready(uart_dev)) {
        printk("ESP UART not ready");
        return ESP_LOADER_ERROR_INVALID_RESPONSE;
    }

    if (!device_is_ready(boot_spec.port)) {
        printk("ESP boot GPIO not ready");
        return ESP_LOADER_ERROR_INVALID_RESPONSE;
    }

    if (!device_is_ready(reset_spec.port)) {
        printk("ESP reset GPIO not ready");
        return ESP_LOADER_ERROR_INVALID_RESPONSE;
    }

    /* Disengage pins */
    gpio_pin_configure_dt(&boot_spec, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&reset_spec, GPIO_OUTPUT_INACTIVE);

    return configure_tty();
}

static int loader_port_init(void)
{
    if (loader_port_zephyr_init() != ESP_LOADER_SUCCESS) {
        LOG_ERR("ESP loader init failed");
        return -EIO;
    }
    LOG_DBG("ESP loader init OK");

    return 0;
}

SYS_INIT(loader_port_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
