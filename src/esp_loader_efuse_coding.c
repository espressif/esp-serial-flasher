/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>

#define RS_NROOTS  12
#define RS_DATA    32
#define RS_WORDS   (RS_DATA / sizeof(uint32_t))   /* 8 input words  */
#define RS_PAR_WORDS (RS_NROOTS / sizeof(uint32_t)) /* 3 parity words */

/* Primitive polynomial x^8 + x^4 + x^3 + x^2 + 1 = 0x11d.
 * When a << 1 overflows 8 bits, XOR with the lower byte 0x1d to reduce. */
#define GF_REDUCE  0x1du

/* Generator polynomial non-leading coefficients g[1..12] (g[0] = 1 is implicit).
 * Matches reedsolo.RSCodec(12) with default GF(2^8) parameters. */
static const uint8_t rs_gen[RS_NROOTS] = {
    0x44, 0x77, 0x43, 0x76, 0xdc, 0x1f, 0x07, 0x54, 0x5c, 0x7f, 0xd5, 0x61
};

static uint8_t gf_mul(uint8_t a, uint8_t b)
{
    uint8_t result = 0;
    while (b) {
        if (b & 1u) {
            result ^= a;
        }
        uint8_t carry = a & 0x80u;
        a = (uint8_t)(a << 1);
        if (carry) {
            a ^= GF_REDUCE;
        }
        b >>= 1;
    }
    return result;
}

/*
 * RS(44, 32) encoder — GF(2^8), 12 parity bytes.
 *
 * Input:  8 data words (little-endian; bytes extracted via shifts, host-endian agnostic).
 * Output: 3 parity words (little-endian; assembled via shifts).
 * Only used for BLK1+ on modern chips; BLK0 bypasses RS entirely.
 */
void efuse_rs_encode(const uint32_t *data, uint32_t *parity)
{
    uint8_t rem[RS_NROOTS] = {0};

    for (int i = 0; i < RS_DATA; i++) {
        uint8_t byte     = (uint8_t)(data[i / 4] >> ((i % 4) * 8u));
        uint8_t feedback = byte ^ rem[0];
        for (int j = 0; j < RS_NROOTS - 1; j++) {
            rem[j] = rem[j + 1] ^ gf_mul(feedback, rs_gen[j]);
        }
        rem[RS_NROOTS - 1] = gf_mul(feedback, rs_gen[RS_NROOTS - 1]);
    }

    for (uint32_t w = 0; w < RS_PAR_WORDS; w++) {
        parity[w] = (uint32_t)rem[w * 4 + 0]
                    | (uint32_t)rem[w * 4 + 1] <<  8u
                    | (uint32_t)rem[w * 4 + 2] << 16u
                    | (uint32_t)rem[w * 4 + 3] << 24u;
    }
}

/* ── 3/4 coding scheme encoder (ESP32 legacy, BLK1-3 only) ───────────────────── */

#define CODING34_CHUNK_BYTES 6u
#define CODING34_DATA_BYTES  24u
#define CODING34_OUT_WORDS   8u
#define CODING34_OUT_BYTES   (CODING34_OUT_WORDS * 4u)

static uint8_t popcount8(uint8_t b)
{
    uint8_t count = 0;
    while (b) {
        count = (uint8_t)(count + (b & 1u));
        b >>= 1u;
    }
    return count;
}

/*
 * 3/4 coding scheme encoder — ESP32 legacy BLK1-3 only (BLK0 always uses
 * CODING_SCHEME_NONE and bypasses this entirely).
 *
 * Processes 24 bytes of staged data as four independent 6-byte chunks; each
 * chunk gets two 1-byte checksums appended (XOR of all 6 bytes, and a
 * weighted population count), producing a 6+2=8 byte encoded chunk. Four
 * chunks -> 32 bytes -> 8 output words. No Galois-field arithmetic, no
 * relation to efuse_rs_encode() beyond both being pre-burn encoders.
 *
 * Input:  6 data words (little-endian; bytes extracted via shifts, host-endian agnostic).
 * Output: 8 words (little-endian; assembled via shifts).
 */
void efuse_coding34_encode(const uint32_t *data, uint32_t *out)
{
    uint8_t outbuf[CODING34_OUT_BYTES];
    uint8_t out_idx = 0;

    for (uint8_t chunk = 0; chunk < CODING34_DATA_BYTES / CODING34_CHUNK_BYTES; chunk++) {
        uint8_t  xor_res = 0;
        uint16_t mul_res = 0;

        for (uint8_t i = 0; i < CODING34_CHUNK_BYTES; i++) {
            uint8_t byte_idx = (uint8_t)(chunk * CODING34_CHUNK_BYTES + i);
            uint8_t b = (uint8_t)(data[byte_idx / 4u] >> ((byte_idx % 4u) * 8u));
            xor_res ^= b;
            mul_res = (uint16_t)(mul_res + (uint16_t)(i + 1u) * popcount8(b));
            outbuf[out_idx++] = b;
        }
        outbuf[out_idx++] = xor_res;
        outbuf[out_idx++] = (uint8_t)mul_res;
    }

    for (uint8_t w = 0; w < CODING34_OUT_WORDS; w++) {
        out[w] = (uint32_t)outbuf[w * 4u + 0u]
                 | (uint32_t)outbuf[w * 4u + 1u] <<  8u
                 | (uint32_t)outbuf[w * 4u + 2u] << 16u
                 | (uint32_t)outbuf[w * 4u + 3u] << 24u;
    }
}
