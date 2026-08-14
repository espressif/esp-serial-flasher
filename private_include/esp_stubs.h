// SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
// SPDX-License-Identifier: Apache-2.0 OR MIT
// auto-generated from esp-flasher-stub v0.8.0
// Source: https://github.com/espressif/esp-flasher-stub/releases/tag/v0.8.0

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_loader.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const esp_stub_t *const esp_stub[ESP_MAX_CHIP];

// Extra stubs not in the lookup table — selected at runtime by application code.
#if defined(ESP_STUB_BUNDLE_ALL) || defined(ESP_STUB_BUNDLE_ESP32P4)
extern const esp_stub_t esp_stub_esp32p4rev1;
#endif

#ifdef __cplusplus
}
#endif
