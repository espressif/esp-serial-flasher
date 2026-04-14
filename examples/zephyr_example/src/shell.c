/*
 * Copyright (c) 2026 Espressif Systems (Shanghai) Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <zephyr/devicetree.h>
#include <zephyr/shell/shell.h>
#include <zephyr/drivers/uart.h>
#include <esp_loader.h>
#include <esp_loader_io.h>
#include <bin_image.h>
#include <zephyr_port.h>

static esp_loader_t *loader;
static zephyr_port_t *port;
static bool stub;

/* Helper functions from the main module */
const char *get_target_name(target_chip_t chip);
uint32_t get_boot_offset(target_chip_t chip);
int install_image(esp_loader_t *loader, const void *bin, size_t size, size_t address);

static int cmd_esf_connect_rom(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    stub = false;
    port = esp_loader_interface_default();
    loader = esp_loader_connect_default(false);

    if (!loader) {
        shell_print(sh, "Failed to connect ROM");
        return -1;
    }

    shell_print(sh, "ROM connected at %d bps", port->baud_rate);

    return 0;
}

static int cmd_esf_connect_stub(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    stub = true;
    port = esp_loader_interface_default();
    loader = esp_loader_connect_default(stub);

    if (!loader) {
        shell_print(sh, "Failed to connect STUB");
        return -1;
    }
    shell_print(sh, "STUB connected at %d", port->baud_rate);

    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_esf_connect,
                               SHELL_CMD_ARG(rom, NULL, "Connect /w ROM functions", cmd_esf_connect_rom, 1, 0),
                               SHELL_CMD_ARG(stub, NULL, "Connect /w STUB functions", cmd_esf_connect_stub, 1, 0),
                               SHELL_SUBCMD_SET_END);

static int cmd_esf_reset(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    if (!loader) {
        shell_print(sh, "Target hasn't been connected");
        return -1;
    }
    esp_loader_reset_target(loader);
    shell_print(sh, "Target reset done");

    return 0;
}

static int cmd_esf_target_info(const struct shell *sh, size_t argc, char **argv)
{
    esp_loader_target_security_info_t info;
    target_chip_t id;
    uint32_t size, offset;
    uint8_t mac[10];
    const char *name;

    if (!loader) {
        shell_print(sh, "Target not connected");
        return -1;
    }
    id = esp_loader_get_target(loader);
    name = get_target_name(id);
    offset = get_boot_offset(id);

    if (esp_loader_read_mac(loader, mac)) {
        memset(mac, 0, sizeof(mac));
    }

    if (esp_loader_flash_detect_size(loader, &size)) {
        shell_print(sh, "Failed to detect flash size");
        return -1;
    }

    if (esp_loader_get_security_info(loader, &info)) {
        shell_print(sh, "Failed to read security information");
        return -1;
    }

    if (argc > 1) {
        if (strcmp(argv[1], "chip") == 0) {
            shell_print(sh, "%s", name);
            return 0;
        }
        if (strcmp(argv[1], "flash") == 0) {
            shell_print(sh, "%d MB", size / 1024 / 1024);
            return 0;
        }
        if (strcmp(argv[1], "boot") == 0) {
            shell_print(sh, "0x%x", offset);
            return 0;
        }
    }
    shell_print(sh, "%s, boot offset: 0x%x", name, offset);
    shell_print(sh, "Flash size: %d (%d MB)", size, size / 1024 / 1024);
    if (!mac[0] && !mac[1] && !mac[2]) {
        shell_print(sh, "MAC read error");
    } else {
        shell_print(sh, "MAC %x:%x:%x:%x:%x:%x  %x:%x", mac[0], mac[1], mac[2],
                    mac[3], mac[4], mac[5], mac[6], mac[7]);
    }
    shell_print(sh, "Target chip : %d", info.target_chip);
    shell_print(sh, "ECO version : %X", info.eco_version);
    shell_print(sh, "USB : %s", info.usb_disabled ? "disabled" : "enabled");
    shell_print(sh, "JTAG software : %s",
                info.jtag_software_disabled ? "disabled" : "enabled");
    shell_print(sh, "JTAG hardware : %s",
                info.jtag_hardware_disabled ? "disabled" : "enabled");
    shell_print(sh, "Flash encryption : %s",
                info.flash_encryption_enabled ? "enabled" : "disabled");
    shell_print(sh, "Secure boot : %s", info.secure_boot_enabled ? "enabled" : "disabled");
    for (int i = 0; i < 3; i++) {
        shell_print(sh, "Secure boot revoked keys.[%d] %s", i,
                    info.secure_boot_revoked_keys[i] ? "enabled" : "disabled");
    }
    shell_print(sh, "Secure boot aggressive revoke : %s",
                info.secure_boot_aggressive_revoke_enabled ? "enabled" : "disabled");
    shell_print(sh, "Secure download mode : %s",
                info.secure_download_mode_enabled ? "enabled" : "disabled");
    shell_print(sh, "Dcache in UART download : %s",
                info.dcache_in_uart_download_disabled ? "disabled" : "enabled");
    shell_print(sh, "Icache in UART download : %s",
                info.icache_in_uart_download_disabled ? "disabled" : "enabled");

    return 0;
}

static int cmd_esf_flash_read(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    uint8_t buf[128];
    uint32_t addr = 0;
    uint32_t size = 1;

    if (!loader) {
        shell_print(sh, "Target not connected");
        return -1;
    }

    if (argc > 1) {
        addr = strtoul(argv[1], NULL, 16);
    }
    if (argc > 2) {
        size = strtoul(argv[2], NULL, 16);
    }
    shell_print(sh, "Reading %d bytes from offset 0x%x", size, addr);

    while (size > 0) {
        size_t to_read = MIN(size, sizeof(buf));

        if (esp_loader_flash_read(loader, buf, addr, to_read) != ESP_LOADER_SUCCESS) {
            shell_print(sh, "Failed to read flash");
            return -1;
        }
        shell_hexdump_line(sh, addr, buf, to_read);
        size -= to_read;
        addr += to_read;
    }

    return 0;
}

static int cmd_esf_img_install(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    int i = 0;
    int selected = -1;

    if (!loader) {
        shell_print(sh, "Target not connected");
        return -1;
    }

    if (argc > 1) {
        selected = strtoul((argv)[1], NULL, 10);
    }

    STRUCT_SECTION_FOREACH(bin_image, t) {
        if (selected >= 0) {
            if (selected == i++) {
                shell_print(sh, "Installing image '%s' at offset 0x%x", t->fname, t->offset);
                install_image(loader, t->data, t->size, t->offset);
            }
        } else {
            shell_print(sh, "Installing image '%s' at offset 0x%x", t->fname, t->offset);
            install_image(loader, t->data, t->size, t->offset);
        }
    }

    shell_print(sh, "Done");

    return 0;
}

static int cmd_esf_flash_erase(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    uint32_t offset, size;

    if (!loader) {
        shell_print(sh, "Target not connected");
        return -1;
    }

    if (argc > 1) {
        offset = strtoul(argv[1], NULL, 16);
        if (argc > 2) {
            size = strtoul(argv[2], NULL, 16);
        } else {
            size = 1;
        }
        if (esp_loader_flash_erase_region(loader, offset, size)) {
            shell_print(sh, "Failed to erase region");
            return -1;
        }
    } else {
        if (esp_loader_flash_erase(loader)) {
            shell_print(sh, "Failed to erase whole flash");
            return -1;
        }
    }
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_esf_flash,
                               SHELL_CMD_ARG(read, NULL, "<address> [<Dword count>]", cmd_esf_flash_read, 2, 2),
                               SHELL_CMD_ARG(image, NULL, "[<img_index>], OR install all", cmd_esf_img_install, 1, 1),
                               SHELL_CMD_ARG(erase, NULL, "Erase flash", cmd_esf_flash_erase, 1, 1),
                               SHELL_SUBCMD_SET_END);

static int cmd_esf_register_read(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    uint32_t reg = 0;
    uint32_t val = 0;

    if (!loader) {
        shell_print(sh, "Target not connected");
        return -1;
    }

    if (argc > 1) {
        reg = strtoul((argv)[1], NULL, 16);
    }
    if (esp_loader_read_register(loader, reg, &val)) {
        shell_print(sh, "Failed to read register");
        return -1;
    }
    shell_print(sh, "0x%x = 0x%x", reg, val);

    return 0;
}

static int cmd_esf_register_write(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    uint32_t reg = 0;
    uint32_t val = 0;

    if (!loader) {
        shell_print(sh, "Target not connected");
        return -1;
    }

    if (argc > 1) {
        reg = strtoul((argv)[1], NULL, 16);
    }
    if (argc > 2) {
        val = strtoul((argv)[2], NULL, 16);
    }
    if (esp_loader_write_register(loader, reg, val)) {
        shell_print(sh, "Failed to write to register 0x%x", reg);
        return -1;
    }
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_esf_register,
                               SHELL_CMD_ARG(read, NULL, "Read register", cmd_esf_register_read, 1, 1),
                               SHELL_CMD_ARG(write, NULL, "Write register", cmd_esf_register_write, 2, 2),
                               SHELL_SUBCMD_SET_END);

static int cmd_esf_speed_default(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    esp_loader_error_t err;

    if (!loader) {
        shell_print(sh, "Target not connected");
        return -1;
    }

    if (stub) {
        err = esp_loader_change_transmission_rate_stub(loader, port->baud_rate, port->baud_rate_high);
    } else {
        err = esp_loader_change_transmission_rate(loader, port->baud_rate);
    }
    if (err != ESP_LOADER_SUCCESS) {
        shell_print(sh, "Failed to set transmission rate to %d", port->baud_rate);
        return -1;
    }
    shell_print(sh, "Transmission rate set to %d", port->baud_rate);

    return 0;
}

static int cmd_esf_speed_higher(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    esp_loader_error_t err;

    if (!loader) {
        shell_print(sh, "Target not connected");
        return -1;
    }
    if (stub) {
        err = esp_loader_change_transmission_rate_stub(loader, port->baud_rate, port->baud_rate_high);
    } else {
        err = esp_loader_change_transmission_rate(loader, port->baud_rate_high);
    }
    if (err != ESP_LOADER_SUCCESS) {
        shell_print(sh, "Failed to set transmission rate to %d", port->baud_rate_high);
        return -1;
    }
    shell_print(sh, "Transmission rate set to %d", port->baud_rate_high);

    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_esf_speed,
                               SHELL_CMD_ARG(default, NULL, "Default speed", cmd_esf_speed_default, 0, 0),
                               SHELL_CMD_ARG(higher, NULL, "Higher speed", cmd_esf_speed_higher, 0, 0),
                               SHELL_SUBCMD_SET_END);

static int cmd_esf_images(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    int i = 0;

    STRUCT_SECTION_FOREACH(bin_image, t) {
        shell_print(sh, "%d: md5: '%s' length: %d offset: 0x%x name: '%s'", i++,
                    t->md5, t->size, t->offset, t->fname);
    }
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_esf,
                               SHELL_CMD(reset, NULL, "Reset target", cmd_esf_reset),
                               SHELL_CMD(info, NULL, "Show target info", cmd_esf_target_info),
                               SHELL_CMD(images, NULL, "List available images", cmd_esf_images),
                               SHELL_CMD(connect, &sub_esf_connect, "Connect target", NULL),
                               SHELL_CMD(speed, &sub_esf_speed, "Change transmission speed", NULL),
                               SHELL_CMD(flash, &sub_esf_flash, "Flash operations", NULL),
                               SHELL_CMD(register, &sub_esf_register, "Register operations", NULL),
                               SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(esf, &sub_esf, "ESP Serial Flasher commands", NULL);

static int esp_loader_init_default(void)
{
    port = esp_loader_interface_default();
    loader = esp_loader_default();

    if (esp_loader_init_uart(loader, &port->port) != ESP_LOADER_SUCCESS) {
        printf("Failed to init default loader\n");
        return -EIO;
    }
    printf("Default loader init OK\n");

    return 0;
}

SYS_INIT(esp_loader_init_default, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
