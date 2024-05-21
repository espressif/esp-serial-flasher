// auto-generated stubs from esptool v4.7.0

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_loader.h"

#ifdef __cplusplus
extern "C" {
#endif

extern bool esp_stub_running;

#if (defined SERIAL_FLASHER_INTERFACE_UART) || (defined SERIAL_FLASHER_INTERFACE_USB)

typedef struct {
    esp_loader_bin_header_t header;
    esp_loader_bin_segment_t segments[2];
} esp_stub_t;

extern const esp_stub_t esp_stub[ESP_MAX_CHIP];

#endif

#ifdef __cplusplus
}
#endif
