/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Known-answer tests for the two pre-burn encoders in esp_loader_efuse_coding.c:
 *
 *   - efuse_rs_encode()       RS(44,32), 12 parity bytes (BLK1+ on modern chips)
 *   - efuse_coding34_encode() ESP32 3/4 scheme (ESP32 legacy BLK1..3)
 *
 * RS vectors were generated offline with reedsolo.RSCodec(12); the 3/4 vectors
 * with espefuse's chunk transform (espefuse/efuse/esp32/fields.py::
 * apply_coding_scheme, in byte-index order, no outer block reversal). Neither
 * library is invoked at test time — the vectors below are baked in.
 *
 * Both encoders are word-oriented (little-endian). The RS reference vectors are
 * byte arrays, so they are packed to words / unpacked from words here.
 */

#include "test_util.h"
#include "esp_loader_efuse_private.h" /* efuse_rs_encode / efuse_coding34_encode */

/* ── RS(44,32) ───────────────────────────────────────────────────────────────── */

#define RS_DATA_BYTES   32
#define RS_PARITY_BYTES 12

typedef struct {
    const char *name;
    uint8_t     data[RS_DATA_BYTES];
    uint8_t     expected[RS_PARITY_BYTES];
} rs_vector_t;

/* Reference vectors from reedsolo.RSCodec(12) (generated offline). */
static const rs_vector_t rs_vectors[] = {
    { "all_zero", { 0 }, { 0 } },
    {
        "all_ff",
        {
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
        },
        { 0x13, 0xa3, 0x62, 0x92, 0x59, 0x74, 0x04, 0x25, 0x73, 0x75, 0xe0, 0xaa },
    },
    {
        "alternating",
        {
            0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55,
            0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55,
            0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55,
            0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55
        },
        { 0x24, 0x22, 0x54, 0xf1, 0x77, 0x16, 0xfa, 0x99, 0xaa, 0x89, 0x9c, 0x1e },
    },
    {
        "counting",
        {
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
            0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
            0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
            0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f
        },
        { 0xa0, 0x4c, 0x47, 0x0d, 0x3f, 0xfc, 0xb2, 0x03, 0xda, 0xe9, 0xf4, 0x13 },
    },
    {
        "random1",
        {
            0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
            0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
            0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00,
            0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef
        },
        { 0x14, 0x7e, 0xdb, 0x34, 0x2e, 0xde, 0x44, 0x7f, 0xf1, 0x19, 0xd8, 0x7e },
    },
};

TEST(rs_encode_known_answers)
{
    for (size_t v = 0; v < sizeof(rs_vectors) / sizeof(rs_vectors[0]); v++) {
        const rs_vector_t *vec = &rs_vectors[v];

        uint32_t data_words[RS_DATA_BYTES / 4]   = {0};
        uint32_t parity_words[RS_PARITY_BYTES / 4] = {0};
        for (int i = 0; i < RS_DATA_BYTES; i++) {
            data_words[i / 4] |= (uint32_t)vec->data[i] << ((i % 4) * 8);
        }
        efuse_rs_encode(data_words, parity_words);

        uint8_t parity[RS_PARITY_BYTES];
        for (int i = 0; i < RS_PARITY_BYTES; i++) {
            parity[i] = (uint8_t)(parity_words[i / 4] >> ((i % 4) * 8));
        }
        for (int i = 0; i < RS_PARITY_BYTES; i++) {
            if (parity[i] != vec->expected[i]) {
                TEST_FAIL("RS vector '%s' parity byte %d: got 0x%02x, want 0x%02x",
                          vec->name, i, parity[i], vec->expected[i]);
            }
        }
    }
    return true;
}

/* ── ESP32 3/4 coding scheme ─────────────────────────────────────────────────── */

#define C34_IN_WORDS  6
#define C34_OUT_WORDS 8

typedef struct {
    const char *name;
    uint32_t    in[C34_IN_WORDS];
    uint32_t    expected[C34_OUT_WORDS];
} coding34_vector_t;

static const coding34_vector_t coding34_vectors[] = {
    /* espefuse 3/4 chunk transform, generated offline (see file header). */
    {
        "all_zero",
        { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
        { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    },
    {
        "all_ff",
        { 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff },
        { 0xffffffff, 0xa800ffff, 0xffffffff, 0xa800ffff, 0xffffffff, 0xa800ffff, 0xffffffff, 0xa800ffff },
    },
    {
        "counting",
        { 0x03020100, 0x07060504, 0x0b0a0908, 0x0f0e0d0c, 0x13121110, 0x17161514 },
        { 0x03020100, 0x1e010504, 0x09080706, 0x2f010b0a, 0x0f0e0d0c, 0x32011110, 0x15141312, 0x41011716 },
    },
    {
        "alternating",
        { 0x55aa55aa, 0x55aa55aa, 0x55aa55aa, 0x55aa55aa, 0x55aa55aa, 0x55aa55aa },
        { 0x55aa55aa, 0x54ff55aa, 0x55aa55aa, 0x54ff55aa, 0x55aa55aa, 0x54ff55aa, 0x55aa55aa, 0x54ff55aa },
    },
    {
        "mixed",
        { 0xb979379e, 0x157c4a7f, 0x60c09cf3, 0x34c8ed5c, 0x0fa36410, 0x0299e13d },
        { 0xb979379e, 0x675c4a7f, 0x9cf3157c, 0x43a660c0, 0x34c8ed5c, 0x3c396410, 0xe13d0fa3, 0x45eb0299 },
    },
};

TEST(coding34_known_answers)
{
    for (size_t v = 0; v < sizeof(coding34_vectors) / sizeof(coding34_vectors[0]); v++) {

        uint32_t out[C34_OUT_WORDS];
        memset(out, 0xCC, sizeof(out));
        efuse_coding34_encode(coding34_vectors[v].in, out);
        for (int w = 0; w < C34_OUT_WORDS; w++) {
            if (out[w] != coding34_vectors[v].expected[w]) {
                TEST_FAIL("3/4 vector '%s' word %d: got 0x%08x, want 0x%08x",
                          coding34_vectors[v].name, w, out[w],
                          coding34_vectors[v].expected[w]);
            }
        }
    }
    return true;
}

TEST_MAIN()
