/*
 * Copyright (c) 2025 Espressif Systems (Shanghai) Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <esp_loader.h>
#include <esp_loader_io.h>
#include <bin_image.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

/* Make target messages visible */
#ifdef CONFIG_LOG
#include <rom/ets_sys.h>
#define print_now ets_printf
#else
#define print_now printk
#endif

#ifndef CONFIG_ESP_SERIAL_FLASHER_SHELL

static uint8_t buf[128];

/* Fetching flasher config from the dts node */
#define ESF_NODE DT_NODELABEL(esp_loader0)
static const struct device *uart = DEVICE_DT_GET(DT_PHANDLE(ESF_NODE, uart));

static esp_loader_connect_args_t esf_conn = {
    .sync_timeout = DT_PROP(ESF_NODE, sync_timeout_ms),
    .trials = DT_PROP(ESF_NODE, num_trials),
};
static uint32_t esf_default_speed = DT_PROP(ESF_NODE, default_baudrate);
static uint32_t esf_higher_speed = DT_PROP(ESF_NODE, higher_baudrate);

#endif /* CONFIG_ESP_SERIAL_FLASHER_SHELL */

static const uint32_t boot_offset[] = {
    [ESP8266_CHIP] = 0x0,
    [ESP32_CHIP]   = 0x1000,
    [ESP32S2_CHIP] = 0x1000,
    [ESP32C3_CHIP] = 0x0,
    [ESP32S3_CHIP] = 0x0,
    [ESP32C2_CHIP] = 0x0,
    [ESP32C5_CHIP] = 0x2000,
    [ESP32H2_CHIP] = 0x0,
    [ESP32C6_CHIP] = 0x0,
    [ESP32P4_CHIP] = 0x2000
};

/* If someone adds a new chip but forgets to update the array, compilation FAILS */
_Static_assert(sizeof(boot_offset) / sizeof(boot_offset[0]) == ESP_MAX_CHIP,
               "boot_offset array size mismatch! "
               "If you added a new chip to target_chip_t, you MUST add its address to boot_offset[]");

static const char *target_name[] = {
    [ESP8266_CHIP] = "ESP8266",
    [ESP32_CHIP]   = "ESP32",
    [ESP32S2_CHIP] = "ESP32-S2",
    [ESP32C3_CHIP] = "ESP32-C3",
    [ESP32S3_CHIP] = "ESP32-S3",
    [ESP32C2_CHIP] = "ESP32-C2",
    [ESP32C5_CHIP] = "ESP32-C5",
    [ESP32H2_CHIP] = "ESP32-H2",
    [ESP32C6_CHIP] = "ESP32-C6",
    [ESP32P4_CHIP] = "ESP32-P4",
    [ESP_UNKNOWN_CHIP] = "Unknown"
};

static const char *get_error_string(const esp_loader_error_t error)
{
    const char *mapping[ESP_LOADER_ERROR_INVALID_RESPONSE + 1] = {
        "NONE", "UNKNOWN", "TIMEOUT", "IMAGE SIZE", "INVALID MD5",
        "INVALID PARAMETER", "INVALID TARGET", "UNSUPPORTED CHIP",
        "UNSUPPORTED FUNCTION", "INVALID RESPONSE"
    };
    return mapping[error];
}

uint32_t get_boot_offset(target_chip_t chip)
{
    return boot_offset[chip];
}

const char *get_target_name(target_chip_t chip)
{
    return target_name[chip];
}

int set_transmission_rate(uint32_t rate)
{
    if (esp_loader_change_transmission_rate(rate) != ESP_LOADER_SUCCESS) {
        LOG_ERR("Unable to change transmission rate on target.");
        return -1;
    }
    if (loader_port_change_transmission_rate(rate) != ESP_LOADER_SUCCESS) {
        LOG_ERR("Unable to change transmission rate on host");
        return -1;
    }
    LOG_INF("Transmission rate updated");

    return 0;
}

int install_image(const void *bin, size_t size, size_t address)
{
    esp_loader_error_t err;
    static uint8_t payload[1024];
    const uint8_t *bin_addr = bin;

    print_now("Erasing flash\n");

    err = esp_loader_flash_start(address, size, sizeof(payload));
    if (err != ESP_LOADER_SUCCESS) {
        LOG_ERR("Erasing flash failed with error: %s.", get_error_string(err));

        if (err == ESP_LOADER_ERROR_INVALID_PARAM) {
            LOG_ERR("If using Secure Download Mode, double check that the specified "
                    "target flash size is correct.");
        }
        return err;
    }

    print_now("Start programming\n");

    size_t written = 0;
    size_t binary_size = size;

    while (size > 0) {
        size_t to_read = MIN(size, sizeof(payload));

        memcpy(payload, bin_addr, to_read);
        err = esp_loader_flash_write(payload, to_read);

        if (err != ESP_LOADER_SUCCESS) {
            LOG_ERR("Packet could not be written! Error %s.", get_error_string(err));
            return err;
        }
        size -= to_read;
        bin_addr += to_read;
        written += to_read;

        int progress = (int)(((float)written / binary_size) * 100);
        printf("\r%d %%", progress);
    }
    print_now("\nFinished programming\n");

#if CONFIG_SERIAL_FLASHER_MD5_ENABLED
    err = esp_loader_flash_verify();
    if (err == ESP_LOADER_ERROR_UNSUPPORTED_FUNC) {
        LOG_ERR("ESP8266 does not support flash verify command.");
        return err;
    } else if (err != ESP_LOADER_SUCCESS) {
        LOG_ERR("MD5 does not match. Error: %s\n", get_error_string(err));
        return err;
    }
    print_now("Flash verified\n");
#endif

    return ESP_LOADER_SUCCESS;
}

int main(void)
{
#ifndef CONFIG_ESP_SERIAL_FLASHER_SHELL
    target_chip_t chip_id;
    uint32_t offset, size;

    LOG_INF("Running ESP Serial Flasher instance");
    LOG_INF("Transmission initial speed: %d higher speed: %d",
            esf_default_speed, esf_higher_speed);
    LOG_INF("Sync time out ms: %d Num trials: %d", esf_conn.sync_timeout,
            esf_conn.trials);

    if (esp_loader_connect(&esf_conn) != ESP_LOADER_SUCCESS) {
        LOG_ERR("Could not connect target");
        return -1;
    }

    print_now("\nConnected to target\n");

    LOG_INF("Setting higher transmission rate");

    if (esf_higher_speed != esf_default_speed) {
        if (set_transmission_rate(esf_higher_speed)) {
            LOG_ERR("Failed to set higher transmission speed");
            return -1;
        }
    }

    print_now("Transmission rate changed\n");

    chip_id = esp_loader_get_target();
    offset = get_boot_offset(chip_id);
    LOG_INF("Target name: %s (id: %d), ROM boot offset: 0x%x",
            get_target_name(chip_id), chip_id, offset);

    if (esp_loader_flash_detect_size(&size) != ESP_LOADER_SUCCESS) {
        LOG_ERR("Could not read target flash size!\n");
    }
    LOG_INF("Target flash size [B]: %u (%d MB)", size, size / 1024 / 1024);

    if (esp_loader_flash_read(buf, offset, sizeof(buf)) != ESP_LOADER_SUCCESS) {
        LOG_ERR("Failed reading flash starting at address 0x%x", offset);
        return -1;
    }
    LOG_INF("Reading image header from boot offset:");
    LOG_HEXDUMP_INF(buf, 16, "ROM bootloader header:");

    if (buf[0] == 0xe9) {
        LOG_INF("Boot vector installed");
    } else {
        LOG_WRN("Target chip not bootable!");
    }

    LOG_INF("Installing target with available images");
    k_msleep(200);

    STRUCT_SECTION_FOREACH(bin_image, t) {
        printf("Loading %s (size %d B, at 0x%x)\n", t->fname, t->size, t->offset);
        install_image(t->data, t->size, t->offset);
    }

    LOG_INF("Resetting target");

    /* Disabling UART interrupts allows target to reset properly */
    uart_irq_tx_disable(uart);
    uart_irq_rx_disable(uart);

    esp_loader_reset_target();

    /* Delay for skipping the target boot messages */
    k_msleep(200);

    /* Enabling UART interrupts to read the target messages */
    uart_irq_tx_enable(uart);
    uart_irq_rx_enable(uart);

    if (loader_port_change_transmission_rate(esf_default_speed) == ESP_LOADER_SUCCESS) {
        print_now("*********************************************\n");
        print_now("*** Logs below are printed from slave ... ***\n");
        print_now("*********************************************\n");

        char c;
        while (1) {
            if (uart_poll_in(uart, &c) == 0) {
                printf("%c", c);
            }
        }
    }
#else
    printf("You can use 'esf help' command to show available shell commands");
#endif

    return 0;
}
