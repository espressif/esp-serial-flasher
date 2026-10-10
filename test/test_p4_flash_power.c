/*
 * SPDX-FileCopyrightText: 2026 FEmbed
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include "esp32p4_flash_power.h"

static unsigned eco, xpd, calls, fail_at, secure;
static uint32_t registers[5] = {0xa5a50200, 0x12550122, 0xdeadbeef, 0x89abcde1, 0};
static const uint32_t addresses[5] = {0x501151bc, 0x501151b8, 0x501153fc, 0x5011010c, 0x5012d034};

static int reg_index(uint32_t address)
{
    for (int i = 0; i < 5; ++i) if (addresses[i] == address) {
            return i;
        }
    abort();
}

esp_loader_error_t esp_loader_get_security_info(esp_loader_t *loader, esp_loader_target_security_info_t *info)
{
    (void)loader;
    *info = (esp_loader_target_security_info_t) {
        .eco_version = eco, .secure_download_mode_enabled = secure
    };
    return ++calls == fail_at ? ESP_LOADER_ERROR_TIMEOUT : ESP_LOADER_SUCCESS;
}

esp_loader_error_t esp_loader_read_register(esp_loader_t *loader, uint32_t address, uint32_t *value)
{
    (void)loader;
    printf("[\"read\",%u],", address);
    *value = registers[reg_index(address)];
    return ++calls == fail_at ? ESP_LOADER_ERROR_TIMEOUT : ESP_LOADER_SUCCESS;
}

esp_loader_error_t esp_loader_write_register(esp_loader_t *loader, uint32_t address, uint32_t value)
{
    (void)loader;
    printf("[\"write\",%u,%u],", address, value);
    if (++calls == fail_at) {
        return ESP_LOADER_ERROR_TIMEOUT;
    }
    registers[reg_index(address)] = value;
    return ESP_LOADER_SUCCESS;
}

static void delay_ms(esp_loader_port_t *port, uint32_t milliseconds)
{
    (void)port;
    printf("[\"delay_ms\",%u],", milliseconds);
}

int main(int argc, char **argv)
{
    if (argc != 7) {
        return 1;
    }
    eco = (unsigned)atoi(argv[1]);
    xpd = (unsigned)atoi(argv[2]);
    fail_at = (unsigned)atoi(argv[3]);
    secure = (unsigned)atoi(argv[6]);
    registers[4] = xpd ? 1U << 16 : 0;
    esp_loader_port_ops_t ops = {.delay_ms = delay_ms};
    esp_loader_port_t port = {.ops = &ops};
    esp_loader_t loader = {._target = atoi(argv[4]) ? ESP32P4_CHIP : ESP32S3_CHIP,
                           ._protocol_type = (esp_loader_protocol_t)atoi(argv[5]), ._port = &port
                          };
    printf("{\"events\":[");
    esp_loader_error_t result = loader_prepare_p4_flash_power(&loader);
    printf("[\"end\"]],\"result\":%d,\"calls\":%u}\n", result, calls);
    return 0;
}
