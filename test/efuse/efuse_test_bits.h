/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Bit arithmetic shared by the eFuse tests: reading bits back out of a word
 * buffer, and range containment for layout bit fields. Pure — no fake, no
 * register I/O — so the layout/field tests can include it too. Kept out of
 * test_util.h, which stays framework-only (TEST / CHECK / main) and includes no
 * eFuse headers.
 */

#pragma once

#include <stdint.h>
#include "esp_loader_efuse_private.h"

#define WORD_BITS (EFUSE_WORD_BYTES * 8u)

/* Extract bit `bit` (LSB-first) from a ctx-style word buffer. */
static inline uint32_t staged_bit(const uint32_t *buf, uint16_t bit)
{
    return (buf[bit / WORD_BITS] >> (bit % WORD_BITS)) & 1u;
}

/* The `count` bits starting at `start`, as a value. */
static inline uint32_t staged_field(const uint32_t *buf, uint16_t start, uint16_t count)
{
    uint32_t v = 0;

    for (uint16_t i = 0; i < count; i++) {
        v |= staged_bit(buf, (uint16_t)(start + i)) << i;
    }
    return v;
}

/* Whether an absolute BLK0 bit position falls inside a layout bit range. */
static inline int in_range(uint16_t bit, efuse_bit_range_t r)
{
    return bit >= r.bit_start && bit < (uint16_t)(r.bit_start + r.bit_count);
}
