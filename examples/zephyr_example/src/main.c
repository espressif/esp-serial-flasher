/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <esp_loader.h>
#include <esp_loader_io.h>
#include <bin_image.h>
#include <zephyr_port.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

const struct device *dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_esp_loader));

#ifndef CONFIG_ESP_SERIAL_FLASHER_SHELL
static uint8_t buf[128];
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
    [ESP32P4_CHIP] = 0x2000,
    [ESP32C61_CHIP] = 0x0
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
    [ESP32C61_CHIP] = "ESP32-C61",
    [ESP_UNKNOWN_CHIP] = "Unknown"
};

uint32_t get_boot_offset(target_chip_t chip)
{
    if (chip >= ESP_MAX_CHIP) {
        return 0;
    }
    return boot_offset[chip];
}

const char *get_target_name(target_chip_t chip)
{
    return target_name[chip];
}

int install_image(esp_loader_t *loader, const void *bin, size_t size, size_t address)
{
    esp_loader_error_t err;
    static uint8_t payload[1024];
    const uint8_t *bin_addr = bin;

    esp_loader_flash_cfg_t flash = {
        .offset = address,
        .image_size = size,
        .block_size = sizeof(payload),
    };

    printf("Erasing flash\n");
    err = esp_loader_flash_start(loader, &flash);
    if (err != ESP_LOADER_SUCCESS) {
        LOG_ERR("Erasing flash failed with error: %d", err);

        if (err == ESP_LOADER_ERROR_INVALID_PARAM) {
            LOG_ERR("If using Secure Download Mode, double check that the specified "
                    "target flash size is correct.");
        }
        return err;
    }

    printf("Start programming\n");

    size_t written = 0;
    size_t binary_size = size;

    while (size > 0) {
        size_t to_write = MIN(size, sizeof(payload));

        memcpy(payload, bin_addr, to_write);
        err = esp_loader_flash_write(loader, &flash, payload, to_write);

        if (err != ESP_LOADER_SUCCESS) {
            LOG_ERR("Packet could not be written! Error %d", err);
            return err;
        }
        size -= to_write;
        bin_addr += to_write;
        written += to_write;
        int progress = (int)(((float)written / binary_size) * 100);
        printf("\r%d %%", progress);
    }
    printf("\nFinished programming\n");

    err = esp_loader_flash_finish(loader, &flash);
    if (err == ESP_LOADER_ERROR_INVALID_MD5) {
        printf("MD5 does not match. Flash verification failed.\n");
        return err;
    } else if (err != ESP_LOADER_SUCCESS) {
        LOG_ERR("Flash finish failed with error: %d", err);
        return err;
    }

    if (!flash.skip_verify) {
        printf("Flash verified\n");
    }

    return 0;
}

int main(void)
{
#if !defined(CONFIG_ESP_SERIAL_FLASHER_SHELL)
    target_chip_t chip_id;
    uint32_t offset, size;
    esp_loader_t *loader;
    const esp_loader_config_t *conf;
    const esp_loader_connect_args_t *carg;

    loader = esp_loader_from_device(dev);
    conf = esp_loader_config_from_device(dev);
    carg = esp_loader_connect_args_from_device(dev);

    printf("Running ESP Flasher from Zephyr\r\n");

    LOG_INF("Running ESP Serial Flasher instance");
    LOG_INF("Transmission default speed: %d higher speed: %d",
            conf->baud_rate, conf->baud_rate_high);

    if (esp_loader_connect(loader, (esp_loader_connect_args_t *) carg) != ESP_LOADER_SUCCESS) {
        LOG_ERR("Could not connect target");
        return -EIO;
    }

    printf("\nConnected to target\n");

    LOG_INF("Setting higher transmission rate");

    if (conf->baud_rate_high) {
        if (esp_loader_change_transmission_rate(loader, conf->baud_rate_high) != ESP_LOADER_SUCCESS) {
            LOG_ERR("Unable to change transmission rate on target.");
            return -EIO;
        }
        printf("Transmission rate changed\n");
    }

    /*  Fetch chip information */
    chip_id = esp_loader_get_target(loader);
    offset = get_boot_offset(chip_id);
    LOG_INF("Target name: %s (id: %d), ROM boot offset: 0x%x",
            get_target_name(chip_id), chip_id, offset);

    if (esp_loader_flash_detect_size(loader, &size) != ESP_LOADER_SUCCESS) {
        LOG_ERR("Could not read target flash size!\n");
        return -1;
    }
    LOG_INF("Target flash size [B]: %u (%d MB)", size, size / 1024 / 1024);

    if (esp_loader_flash_read(loader, buf, offset, sizeof(buf)) != ESP_LOADER_SUCCESS) {
        LOG_ERR("Failed reading flash starting at address 0x%x", offset);
        return -1;
    }
    LOG_INF("Reading image header from boot offset:");
    LOG_HEXDUMP_INF(buf, 16, "ROM bootloader header:");

    if (buf[0] == 0xe9) {
        LOG_INF("Boot vector installed");
    } else {
        LOG_INF("Target chip not bootable!");
    }

    /* Flash images provided locally */
    LOG_INF("Installing target with available images:");

    STRUCT_SECTION_FOREACH(bin_image, t) {
        printf("Loading '%s' (size %d B, at 0x%x)\n", t->fname, t->size, t->offset);
        install_image(loader, t->data, t->size, t->offset);
    }
    printf("\nDONE\n");

    if (esp_loader_host_baudrate(loader, conf->baud_rate) != ESP_LOADER_SUCCESS) {
        LOG_ERR("Failed to set default host baudrate");
        return -1;
    }
    k_msleep(200);

    esp_loader_reset_target(loader);
    LOG_INF("Resetting target");

    printf("**********************************************\n");
    printf("*** Logs below are printed from target ... ***\n");
    printf("**********************************************\n");

    char c;
    while (1) {
        if (uart_poll_in(conf->uart_dev, &c) == 0) {
            printf("%c", c);
        } else {
            k_yield();
        }
    }
#else
    printf("Type 'esf help' to show serial flasher shell command options");
#endif

    return 0;
}
