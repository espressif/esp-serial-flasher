// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
// SPDX-License-Identifier: Apache-2.0 OR MIT

// auto-generated from esp-flasher-stub v1.3.0
// Source: https://github.com/espressif/esp-flasher-stub/releases/tag/v1.3.0

#include "esp_loader_stubs.h"
#include "esp_targets.h"

static const esp_stub_t *bundled_provider(esp_loader_t *loader, target_chip_t chip, void *ctx)
{
    (void)ctx;

    switch (chip) {
    case ESP8266_CHIP:
        return &esp_stub_esp8266;
    case ESP32_CHIP:
        return &esp_stub_esp32;
    case ESP32S2_CHIP:
        return &esp_stub_esp32s2;
    case ESP32C3_CHIP:
        return &esp_stub_esp32c3;
    case ESP32S3_CHIP:
        return &esp_stub_esp32s3;
    case ESP32C2_CHIP:
        return &esp_stub_esp32c2;
    case ESP32C5_CHIP:
        return &esp_stub_esp32c5;
    case ESP32H2_CHIP:
        return &esp_stub_esp32h2;
    case ESP32C6_CHIP:
        return &esp_stub_esp32c6;
    case ESP32P4_CHIP: {
        esp_loader_target_security_info_t info;
        bool got_info = (esp_loader_get_security_info(loader, &info) == ESP_LOADER_SUCCESS);
        return (got_info && info.eco_version >= ESP32P4_ECO_REV3_MIN)
               ? &esp_stub_esp32p4 : &esp_stub_esp32p4rev1;
    }
    case ESP32C61_CHIP:
        return &esp_stub_esp32c61;
    case ESP32S31_CHIP:
        return &esp_stub_esp32s31;
    case ESP32H21_CHIP:
        return &esp_stub_esp32h21;
    default:
        return NULL;
    }
}

esp_loader_error_t esp_loader_connect_with_stub(esp_loader_t *loader,
        esp_loader_connect_args_t *connect_args)
{
    return esp_loader_connect_with_stub_provider(loader, connect_args, bundled_provider, NULL);
}
