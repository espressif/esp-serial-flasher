#pragma once

#include <zephyr/sys/iterable_sections.h>

struct bin_image {
    const uint8_t *data;
    uint32_t offset;
    uint32_t size;
    const char *md5;
    const char *fname;
};

#define DEFINE_IMAGE(name, _d, _o, _s, _m, _f) \
         STRUCT_SECTION_ITERABLE(bin_image, name) = { \
                 .data = _d, \
                 .offset = _o, \
                 .size = _s, \
                 .md5 = _m, \
                 .fname = _f, \
         }
