// auto-generated stubs from esptool v4.7.0

#pragma once

#include <stdint.h>
#include "esp_loader.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    esp_loader_bin_header_t header;
    esp_loader_bin_segment_t segments[2];
} esp_stub_t;

#if STUB_ENABLED
extern const esp_stub_t esp_stub[ESP_MAX_CHIP];
#endif

#ifdef __cplusplus
}
#endif
