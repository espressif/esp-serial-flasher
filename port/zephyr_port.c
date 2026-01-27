/*
 * Copyright (c) 2026 Espressif Systems (Shanghai) Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT espressif_esp_loader

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/console/tty.h>

#include <zephyr_port.h>
#include <esp_loader.h>
#include <esp_loader_io.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(esf, CONFIG_ESP_SERIAL_FLASHER_LOG_LEVEL);

static void zephyr_debug_print(esp_loader_port_t *port, const char *str)
{
    ARG_UNUSED(port);

    LOG_DBG("%s", str);
}

static esp_loader_error_t configure_tty(zephyr_port_t *p)
{
    if (tty_init(&p->_tty, p->uart_dev) < 0 ||
            tty_set_rx_buf(&p->_tty, p->_tty_rx_buf, sizeof(p->_tty_rx_buf)) < 0 ||
            tty_set_tx_buf(&p->_tty, p->_tty_tx_buf, sizeof(p->_tty_tx_buf)) < 0) {
        return ESP_LOADER_ERROR_FAIL;
    }
    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t zephyr_port_init(esp_loader_port_t *port)
{
    zephyr_port_t *p = container_of(port, zephyr_port_t, port);

    if (!device_is_ready(p->uart_dev)) {
        LOG_ERR("ESP UART not ready");
        return ESP_LOADER_ERROR_INVALID_RESPONSE;
    }

    if (!device_is_ready(p->boot_spec.port)) {
        LOG_ERR("ESP boot GPIO not ready");
        return ESP_LOADER_ERROR_INVALID_RESPONSE;
    }

    if (!device_is_ready(p->enable_spec.port)) {
        LOG_ERR("ESP reset GPIO not ready");
        return ESP_LOADER_ERROR_INVALID_RESPONSE;
    }

    /* From Zephyrs DT perspective, the "ACTIVE" state
     * is high, unless "GPIO_ACTIVE_LOW" flag is used
     */
    gpio_pin_configure_dt(&p->boot_spec, GPIO_OUTPUT_ACTIVE);
    gpio_pin_configure_dt(&p->enable_spec, GPIO_OUTPUT_ACTIVE);

    return configure_tty(p);
}

static void zephyr_port_deinit(esp_loader_port_t *port)
{
    zephyr_port_t *p = container_of(port, zephyr_port_t, port);

    /* Disabling UART interrupts allows target to reset properly */
    uart_irq_tx_disable(p->uart_dev);
    uart_irq_rx_disable(p->uart_dev);
    gpio_pin_configure_dt(&p->boot_spec, GPIO_DISCONNECTED);
    gpio_pin_configure_dt(&p->enable_spec, GPIO_DISCONNECTED);
}

static esp_loader_error_t zephyr_uart_read(esp_loader_port_t *port, uint8_t *data, const uint16_t size, const uint32_t timeout)
{
    zephyr_port_t *p = container_of(port, zephyr_port_t, port);

    if (!device_is_ready(p->uart_dev) || data == NULL || size == 0) {
        return ESP_LOADER_ERROR_FAIL;
    }

    ssize_t total_read = 0;
    ssize_t remaining = size;

    tty_set_rx_timeout(&p->_tty, timeout);
    while (remaining > 0) {
        const uint16_t chunk_size = remaining < CONFIG_ESP_SERIAL_FLASHER_UART_BUFSIZE ?
                                    remaining : CONFIG_ESP_SERIAL_FLASHER_UART_BUFSIZE;
        ssize_t read = tty_read(&p->_tty, &data[total_read], chunk_size);
        if (read < 0) {
            return ESP_LOADER_ERROR_TIMEOUT;
        }
        LOG_HEXDUMP_DBG(data, read, "READ");

        total_read += read;
        remaining -= read;
    }

    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t zephyr_uart_write(esp_loader_port_t *port, const uint8_t *data, const uint16_t size, const uint32_t timeout)
{
    zephyr_port_t *p = container_of(port, zephyr_port_t, port);

    if (!device_is_ready(p->uart_dev) || data == NULL || size == 0) {
        return ESP_LOADER_ERROR_FAIL;
    }

    ssize_t total_written = 0;
    ssize_t remaining = size;

    tty_set_tx_timeout(&p->_tty, timeout);
    while (remaining > 0) {
        const uint16_t chunk_size = remaining < CONFIG_ESP_SERIAL_FLASHER_UART_BUFSIZE ?
                                    remaining : CONFIG_ESP_SERIAL_FLASHER_UART_BUFSIZE;
        ssize_t written = tty_write(&p->_tty, &data[total_written], chunk_size);
        if (written < 0) {
            return ESP_LOADER_ERROR_TIMEOUT;
        }
        LOG_HEXDUMP_DBG(data, written, "WRITE");

        total_written += written;
        remaining -= written;
    }

    return ESP_LOADER_SUCCESS;
}

static void zephyr_uart_delay_ms(esp_loader_port_t *port, uint32_t ms)
{
    (void)port;
    k_msleep(ms);
}

static void zephyr_uart_start_timer(esp_loader_port_t *port, uint32_t ms)
{
    zephyr_port_t *p = container_of(port, zephyr_port_t, port);
    p->_time_end = sys_timepoint_calc(Z_TIMEOUT_MS(ms)).tick;
}

static uint32_t zephyr_uart_remaining_time(esp_loader_port_t *port)
{
    zephyr_port_t *p = container_of(port, zephyr_port_t, port);
    int64_t remaining = k_ticks_to_ms_floor64(p->_time_end - k_uptime_ticks());
    return (remaining > 0) ? (uint32_t)remaining : 0;
}

static void zephyr_uart_reset_target(esp_loader_port_t *port)
{
    zephyr_port_t *p = container_of(port, zephyr_port_t, port);

    /* Prevent TX/RX pin from interfering with ROM loader.
     * Without this the target won't run after reset toggle. */
    uart_irq_tx_disable(p->uart_dev);
    uart_irq_rx_disable(p->uart_dev);

    gpio_pin_set_dt(&p->enable_spec, SERIAL_FLASHER_RESET_INVERT ? true : false);
    k_msleep(CONFIG_SERIAL_FLASHER_RESET_HOLD_TIME_MS);
    gpio_pin_set_dt(&p->enable_spec, SERIAL_FLASHER_RESET_INVERT ? false : true);

    k_msleep(200);

    /* Re-configure the uart after the reset sequence */
    if (port->ops->change_transmission_rate != NULL) {
        if (port->ops->change_transmission_rate(port, p->baud_rate) != ESP_LOADER_SUCCESS) {
            LOG_ERR("Failed resetting transmission rate");
        }
    }
}

static void zephyr_uart_enter_bootloader(esp_loader_port_t *port)
{
    zephyr_port_t *p = container_of(port, zephyr_port_t, port);

    gpio_pin_set_dt(&p->boot_spec, SERIAL_FLASHER_BOOT_INVERT ? true : false);
    gpio_pin_set_dt(&p->enable_spec, SERIAL_FLASHER_RESET_INVERT ? true : false);
    k_msleep(CONFIG_SERIAL_FLASHER_RESET_HOLD_TIME_MS);
    gpio_pin_set_dt(&p->enable_spec, SERIAL_FLASHER_RESET_INVERT ? false : true);
    k_msleep(CONFIG_SERIAL_FLASHER_BOOT_HOLD_TIME_MS);
    gpio_pin_set_dt(&p->boot_spec, SERIAL_FLASHER_BOOT_INVERT ? false : true);
}

static esp_loader_error_t zephyr_uart_change_rate(esp_loader_port_t *port, uint32_t baudrate)
{
    zephyr_port_t *p = container_of(port, zephyr_port_t, port);
    struct uart_config uart_config;

    if (uart_config_get(p->uart_dev, &uart_config) != 0) {
        return ESP_LOADER_ERROR_FAIL;
    }
    uart_config.baudrate = baudrate;

    if (uart_configure(p->uart_dev, &uart_config) != 0) {
        return ESP_LOADER_ERROR_FAIL;
    }
    /* bitrate-change can require tty re-configuration */
    return configure_tty(p);
}

static const esp_loader_port_ops_t zephyr_uart_ops = {
    .init                     = zephyr_port_init,
    .deinit                   = zephyr_port_deinit,
    .enter_bootloader         = zephyr_uart_enter_bootloader,
    .reset_target             = zephyr_uart_reset_target,
    .start_timer              = zephyr_uart_start_timer,
    .remaining_time           = zephyr_uart_remaining_time,
    .delay_ms                 = zephyr_uart_delay_ms,
    .debug_print              = zephyr_debug_print,
    .change_transmission_rate = zephyr_uart_change_rate,
    .write                    = zephyr_uart_write,
    .read                     = zephyr_uart_read,
};

#define ESP_LOADER_ENTRY(node_id) _CONCAT(esf_loader_, node_id)
#define ESP_INTERFACE_ENTRY(node_id) _CONCAT(esf_interface_, node_id)
#define ESP_CONNECT_CFG_ENTRY(node_id) _CONCAT(esf_connect_cfg_, node_id)

/*
 * Declare the port as static so that _tty, _tty_rx_buf, and _tty_tx_buf
 * reside in BSS rather than on the thread stack. In the previous API these
 * buffers were static globals; moving them into a stack-local zephyr_port_t
 * risks stack overflow on targets with small default stack sizes.
 */
#define ESP_LOADER_DEFINE(inst)                                                \
    static esp_loader_t ESP_LOADER_ENTRY(DT_DRV_INST(inst));                   \
    static zephyr_port_t ESP_INTERFACE_ENTRY(DT_DRV_INST(inst)) = {            \
        .port.ops       = &zephyr_uart_ops,                                    \
        .uart_dev       = DEVICE_DT_GET(DT_INST_PHANDLE(inst, uart)),          \
        .enable_spec    = GPIO_DT_SPEC_GET(DT_DRV_INST(inst), reset_gpios),    \
        .boot_spec      = GPIO_DT_SPEC_GET(DT_DRV_INST(inst), boot_gpios),     \
        .baud_rate      = DT_INST_PROP(inst, default_baudrate),                \
        .baud_rate_high = DT_INST_PROP(inst, higher_baudrate),                 \
    };                                                                         \
    static esp_loader_connect_args_t ESP_CONNECT_CFG_ENTRY(DT_DRV_INST(inst)) = { \
        .sync_timeout = DT_INST_PROP(inst, sync_timeout_ms),                   \
        .trials = DT_INST_PROP(inst, num_trials),                              \
    };                                                                         \

DT_INST_FOREACH_STATUS_OKAY(ESP_LOADER_DEFINE)

esp_loader_t *esp_loader_default(void)
{
    BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) > 0, "No esp_loader instances in DT");

    return &ESP_LOADER_ENTRY(DT_CHOSEN(zephyr_esp_loader));
}

esp_loader_t *esp_loader_instance(int index)
{
#define ESP_LOADER_INSTANCE_CASE(n) case n: return &ESP_LOADER_ENTRY(DT_DRV_INST(n));

    switch (index) {
        DT_INST_FOREACH_STATUS_OKAY(ESP_LOADER_INSTANCE_CASE)
    default: return NULL;
    }
}

zephyr_port_t *esp_loader_interface_default(void)
{
    BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) > 0, "No esp_loader instances in DT");

    return &ESP_INTERFACE_ENTRY(DT_CHOSEN(zephyr_esp_loader));
}

zephyr_port_t *esp_loader_interface_instance(int index)
{
#define ESP_LOADER_INTERFACE_CASE(n) case n: return &ESP_INTERFACE_ENTRY(DT_DRV_INST(n));

    switch (index) {
        DT_INST_FOREACH_STATUS_OKAY(ESP_LOADER_INTERFACE_CASE)
    default: return NULL;
    }
}

esp_loader_t *esp_loader_connect_default(bool stub)
{
    BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) > 0, "No esp_loader instances in DT");

    esp_loader_t *loader = &ESP_LOADER_ENTRY(DT_CHOSEN(zephyr_esp_loader));
    esp_loader_port_t *port = &ESP_INTERFACE_ENTRY(DT_CHOSEN(zephyr_esp_loader)).port;
    esp_loader_connect_args_t *ccfg = &ESP_CONNECT_CFG_ENTRY(DT_CHOSEN(zephyr_esp_loader));

    if (esp_loader_init_uart(loader, port) != ESP_LOADER_SUCCESS) {
        LOG_ERR("esp_loader init failed");
        return NULL;
    }

    if (stub) {
        if (esp_loader_connect_with_stub(loader, ccfg) != ESP_LOADER_SUCCESS) {
            LOG_ERR("esp_loader connect failed");
            return NULL;
        }
    } else {
        if (esp_loader_connect(loader, ccfg) != ESP_LOADER_SUCCESS) {
            LOG_ERR("esp_loader connect failed");
            return NULL;
        }
    }

    return loader;
}

esp_loader_t *esp_loader_connect_instance(int index, bool stub)
{
#define ESP_LOADER_CONNECT_CASE(n) \
        case n: loader = &ESP_LOADER_ENTRY(DT_DRV_INST(n));                    \
                ccfg = &ESP_CONNECT_CFG_ENTRY(DT_DRV_INST(n));                 \
                port = &ESP_INTERFACE_ENTRY(DT_DRV_INST(n)).port;              \
                break;

    esp_loader_t *loader;
    esp_loader_port_t *port;
    esp_loader_connect_args_t *ccfg;

    switch (index) {
        DT_INST_FOREACH_STATUS_OKAY(ESP_LOADER_CONNECT_CASE)
    default: return NULL;
    }

    if (esp_loader_init_uart(loader, port) != ESP_LOADER_SUCCESS) {
        LOG_ERR("esp_loader%d init failed", index);
        return NULL;
    }
    if (stub) {
        if (esp_loader_connect_with_stub(loader, ccfg) != ESP_LOADER_SUCCESS) {
            LOG_ERR("esp_loader%d connect failed", index);
            return NULL;
        }
    } else {
        if (esp_loader_connect(loader, ccfg) != ESP_LOADER_SUCCESS) {
            LOG_ERR("esp_loader%d connect failed", index);
            return NULL;
        }
    }

    return loader;
}
