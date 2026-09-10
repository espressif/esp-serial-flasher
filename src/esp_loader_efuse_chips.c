/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_loader_efuse_private.h"

/*
 * Read-register offsets from efuse_base, derived from IDF soc/efuse_reg.h per chip.
 *
 * Modern chips (S2, C3, C5, C6, C61, H2, P4, S3):
 *   BLK0 = RD_WR_DIS..RD_REPEAT_DATA4   (6 words, offset 0x02C)
 *   BLK1 = RD_MAC_SPI_SYS_0..5          (6 words, offset 0x044)
 *   BLK2 = RD_SYS_PART1_DATA0..7        (8 words, offset 0x05C)
 *   BLK3 = RD_USR_DATA0..7              (8 words, offset 0x07C)
 *   BLK4..9 = RD_KEY0..5_DATA0..7       (8 words each)
 *   BLK10 = RD_SYS_PART2_DATA0..7       (8 words, offset 0x15C)
 *
 * ESP32 (legacy):
 *   BLK0 = BLK0_RDATA0..6               (7 words, offset 0x000)
 *   BLK1 = BLK1_RDATA0..7               (8 words, offset 0x038)
 *   BLK2 = BLK2_RDATA0..7               (8 words, offset 0x058)
 *   BLK3 = BLK3_RDATA0..7               (8 words, offset 0x078)
 *
 * ESP32-C2 (reduced):
 *   BLK0 = RD_WR_DIS..RD_REPEAT_DATA0  (2 words, offset 0x02C)
 *   BLK1 = RD_BLK1_DATA0..2            (3 words, offset 0x034)
 *   BLK2 = RD_BLK2_DATA0..7            (8 words, offset 0x040)
 *   BLK3 = RD_BLK3_DATA0..7            (8 words, offset 0x060)
 */

/* ── Block read layouts ─────────────────────────────────────────────────────── */

/* KEY_PURPOSE field geometry (efuse_key_purpose_field_t) lives in
 * efuse_chip_layout_t, not here. All modern-family chips share this single
 * block array. */

#define N  ESP_LOADER_EFUSE_BIT_NONE    /* absent bit position   */
#define NI EFUSE_INDEX_NONE  /* absent block/key index */

/* wr_dis / rd_dis are absolute BLK0 bit positions (WR_DIS field starts at bit 0,
 * RD_DIS at bit 32 on modern chips). N = no such fuse, NI = not a key block. */
/*                          read_off wds  key_idx  wr_dis  rd_dis */
static const efuse_blk_layout_t modern_blocks[] = {
    { 0x02C, 6,  NI,   N,   N },  /* BLK0  WR_DIS + REPEAT_DATA  */
    { 0x044, 6,  NI,  20,   N },  /* BLK1  MAC_SPI_SYS            */
    { 0x05C, 8,  NI,  21,   N },  /* BLK2  SYS_PART1              */
    { 0x07C, 8,  NI,  22,   N },  /* BLK3  USR_DATA               */
    { 0x09C, 8,   0,  23,  32 },  /* BLK4  KEY0                   */
    { 0x0BC, 8,   1,  24,  33 },  /* BLK5  KEY1                   */
    { 0x0DC, 8,   2,  25,  34 },  /* BLK6  KEY2                   */
    { 0x0FC, 8,   3,  26,  35 },  /* BLK7  KEY3                   */
    { 0x11C, 8,   4,  27,  36 },  /* BLK8  KEY4                   */
    { 0x13C, 8,   5,  28,  37 },  /* BLK9  KEY5                   */
    { 0x15C, 8,  NI,  29,  38 },  /* BLK10 SYS_PART2              */
};

/* ESP32 legacy: RD_DIS field starts at BLK0 bit 16, WR_DIS at bit 7. */
static const efuse_blk_layout_t esp32_blocks[] = {
    { 0x000, 7,  NI,   N,   N },  /* BLK0 */
    { 0x038, 8,  NI,   7,  16 },  /* BLK1 */
    { 0x058, 8,  NI,   8,  17 },  /* BLK2 */
    { 0x078, 8,  NI,   9,  18 },  /* BLK3 */
};

static const efuse_blk_layout_t esp32c2_blocks[] = {
    { 0x02C, 2,  NI,   N,   N },  /* BLK0 */
    { 0x034, 3,  NI,   5,   N },  /* BLK1 */
    { 0x040, 8,  NI,   6,   N },  /* BLK2 */
    { 0x060, 8,  NI,   7,   N },  /* BLK3 KEY0 (no KEY_PURPOSE on C2) */
};

#undef N
#undef NI

/* Every block array must fit the fixed-size staged-write buffer and image structs,
 * which are dimensioned by these maxima. A chip whose block array exceeds
 * ESP_LOADER_EFUSE_MAX_BLOCKS would overflow esp_loader_efuse_ctx_t._buf.
 *
 * Lower bounds required by the chips currently dispatched (modern family has 11
 * blocks; BLK0 may be 9 words on S31, others ≤ 8). A future chip with a larger
 * block or word count would require these maxima raised. The per-block
 * word_count <= MAX check is enforced at runtime (not visible to _Static_assert). */
#if __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(modern_blocks) / sizeof(modern_blocks[0]) <= ESP_LOADER_EFUSE_MAX_BLOCKS,
               "modern_blocks exceeds ESP_LOADER_EFUSE_MAX_BLOCKS");
_Static_assert(sizeof(esp32_blocks) / sizeof(esp32_blocks[0]) <= ESP_LOADER_EFUSE_MAX_BLOCKS,
               "esp32_blocks exceeds ESP_LOADER_EFUSE_MAX_BLOCKS");
_Static_assert(sizeof(esp32c2_blocks) / sizeof(esp32c2_blocks[0]) <= ESP_LOADER_EFUSE_MAX_BLOCKS,
               "esp32c2_blocks exceeds ESP_LOADER_EFUSE_MAX_BLOCKS");
#endif

/* ── PGM write register offsets ─────────────────────────────────────────────── */

static const efuse_pgm_layout_t common_pgm_layout = {
    .pgm_data0_off    = 0x000,  /* EFUSE_PGM_DATA0_REG        */
    .check_value0_off = 0x020,  /* EFUSE_PGM_CHECK_VALUE0_REG */
    .pgm_data_words   = 8,
};

/* ── Controller config (op-codes) ───────────────────────────────────────────── */

/* All modern chips share the same op-codes. Timing values live in efuse_set_timing. */
static const efuse_ctrl_cfg_t common_ctrl_cfg = {
    .write_op        = 0x5A5Au,
    .read_op         = 0x5AA5u,
    .pgm_cmd         = 0x2u,
    .read_cmd        = 0x1u,
    .cmd_block_shift = 0x2u,
};

/* ── Burn register offsets ──────────────────────────────────────────────────── */

/*
 * C3 / C6 / C61 / H2 / P4 / S3
 * Factory timing: C6, C61, H2, P4, S3.  Needs timing setup: C3.
 */
static const efuse_burn_regs_t burn_regs_common = {
    .pgm              = &common_pgm_layout,
    .ctrl             = &common_ctrl_cfg,
    .conf_off         = 0x1CC,
    .cmd_off          = 0x1D4,
    .rs_err0_off      = 0x1C0,
    .repeat_err0_off  = 0x17C,
    .repeat_err_words = 5,
};

/*
 * S2 — needs timing setup (40 MHz only).
 * Different RS_ERR and WR_TIM offsets; has WR_TIM_CONF0.
 */
static const efuse_burn_regs_t burn_regs_esp32s2 = {
    .pgm              = &common_pgm_layout,
    .ctrl             = &common_ctrl_cfg,
    .conf_off         = 0x1CC,
    .cmd_off          = 0x1D4,
    .rs_err0_off      = 0x194,
    .repeat_err0_off  = 0x17C,
    .repeat_err_words = 5,
};

/*
 * C5 — factory timing.
 * CMD_REG, RS_ERR, DAC_CONF at different offsets from common.
 */
static const efuse_burn_regs_t burn_regs_esp32c5 = {
    .pgm              = &common_pgm_layout,
    .ctrl             = &common_ctrl_cfg,
    .conf_off         = 0x1CC,
    .cmd_off          = 0x1D8,
    .rs_err0_off      = 0x190,
    .repeat_err0_off  = 0x17C,
    .repeat_err_words = 5,
};

/*
 * C2 — needs timing setup (26 or 40 MHz).
 * Entirely different register map; only BLK1-3, so RS_ERR1 is never reached.
 */
static const efuse_burn_regs_t burn_regs_esp32c2 = {
    .pgm              = &common_pgm_layout,
    .ctrl             = &common_ctrl_cfg,
    .conf_off         = 0x08C,
    .cmd_off          = 0x094,
    .rs_err0_off      = 0x084,
    .repeat_err0_off  = 0x080,
    .repeat_err_words = 1,
};

/* ── Key purpose tables ─────────────────────────────────────────────────────── */

/* Columns: { purpose tag, silicon code, needs_reverse, needs_rd_protect, is_digest }.
 * Verified against espefuse KEY_PURPOSES (esptool espefuse/efuse/<chip>/fields.py).
 * RESERVED and host-side "virtual" multi-block split purposes are omitted. The
 * geometry differs from the block tables, so chips that share modern_blocks may
 * still have different purpose sets (e.g. C61 has no HMAC; H2 adds ECDSA). */
#define P(name) ESP_LOADER_KEY_PURPOSE_##name

/* C3 / C6: 128-bit XTS, HMAC, secure-boot digests. */
static const efuse_key_purpose_row_t purposes_c3_c6[] = {
    { P(USER),                         0,  0, 0, 0 },
    { P(XTS_AES_128_KEY),              4,  1, 1, 0 },
    { P(HMAC_DOWN_ALL),                5,  0, 1, 0 },
    { P(HMAC_DOWN_JTAG),               6,  0, 1, 0 },
    { P(HMAC_DOWN_DIGITAL_SIGNATURE),  7,  0, 1, 0 },
    { P(HMAC_UP),                      8,  0, 1, 0 },
    { P(SECURE_BOOT_DIGEST0),          9,  0, 0, 1 },
    { P(SECURE_BOOT_DIGEST1),          10, 0, 0, 1 },
    { P(SECURE_BOOT_DIGEST2),          11, 0, 0, 1 },
};

/* H2: C3/C6 set plus ECDSA_KEY (P256) at code 1. */
static const efuse_key_purpose_row_t purposes_h2[] = {
    { P(USER),                         0,  0, 0, 0 },
    { P(ECDSA_KEY_P256),               1,  1, 1, 0 },
    { P(XTS_AES_128_KEY),              4,  1, 1, 0 },
    { P(HMAC_DOWN_ALL),                5,  0, 1, 0 },
    { P(HMAC_DOWN_JTAG),               6,  0, 1, 0 },
    { P(HMAC_DOWN_DIGITAL_SIGNATURE),  7,  0, 1, 0 },
    { P(HMAC_UP),                      8,  0, 1, 0 },
    { P(SECURE_BOOT_DIGEST0),          9,  0, 0, 1 },
    { P(SECURE_BOOT_DIGEST1),          10, 0, 0, 1 },
    { P(SECURE_BOOT_DIGEST2),          11, 0, 0, 1 },
};

/* C61: ECDSA + 128-bit XTS + digests, no HMAC. */
static const efuse_key_purpose_row_t purposes_c61[] = {
    { P(USER),                         0,  0, 0, 0 },
    { P(ECDSA_KEY_P256),               1,  1, 1, 0 },
    { P(XTS_AES_128_KEY),              4,  1, 1, 0 },
    { P(SECURE_BOOT_DIGEST0),          9,  0, 0, 1 },
    { P(SECURE_BOOT_DIGEST1),          10, 0, 0, 1 },
    { P(SECURE_BOOT_DIGEST2),          11, 0, 0, 1 },
};

/* S2 / S3: C3 set plus the two 256-bit XTS halves (codes 2, 3). */
static const efuse_key_purpose_row_t purposes_s2_s3[] = {
    { P(USER),                         0,  0, 0, 0 },
    { P(XTS_AES_256_KEY_1),            2,  1, 1, 0 },
    { P(XTS_AES_256_KEY_2),            3,  1, 1, 0 },
    { P(XTS_AES_128_KEY),              4,  1, 1, 0 },
    { P(HMAC_DOWN_ALL),                5,  0, 1, 0 },
    { P(HMAC_DOWN_JTAG),               6,  0, 1, 0 },
    { P(HMAC_DOWN_DIGITAL_SIGNATURE),  7,  0, 1, 0 },
    { P(HMAC_UP),                      8,  0, 1, 0 },
    { P(SECURE_BOOT_DIGEST0),          9,  0, 0, 1 },
    { P(SECURE_BOOT_DIGEST1),          10, 0, 0, 1 },
    { P(SECURE_BOOT_DIGEST2),          11, 0, 0, 1 },
};

/* C5: 128-bit XTS (+ PSRAM), full ECDSA range, KM_INIT_KEY; no 256-bit XTS. */
static const efuse_key_purpose_row_t purposes_c5[] = {
    { P(USER),                         0,  0, 0, 0 },
    { P(ECDSA_KEY_P256),               1,  1, 1, 0 },
    { P(XTS_AES_128_KEY),              4,  1, 1, 0 },
    { P(HMAC_DOWN_ALL),                5,  0, 1, 0 },
    { P(HMAC_DOWN_JTAG),               6,  0, 1, 0 },
    { P(HMAC_DOWN_DIGITAL_SIGNATURE),  7,  0, 1, 0 },
    { P(HMAC_UP),                      8,  0, 1, 0 },
    { P(SECURE_BOOT_DIGEST0),          9,  0, 0, 1 },
    { P(SECURE_BOOT_DIGEST1),          10, 0, 0, 1 },
    { P(SECURE_BOOT_DIGEST2),          11, 0, 0, 1 },
    { P(KM_INIT_KEY),                  12, 0, 1, 0 },
    { P(XTS_AES_128_PSRAM_KEY),        15, 1, 1, 0 },
    { P(ECDSA_KEY_P192),               16, 1, 1, 0 },
    { P(ECDSA_KEY_P384_L),             17, 1, 1, 0 },
    { P(ECDSA_KEY_P384_H),             18, 1, 1, 0 },
};

/* P4: 256-bit XTS halves, 128-bit XTS, full ECDSA range, KM_INIT_KEY. */
static const efuse_key_purpose_row_t purposes_p4[] = {
    { P(USER),                         0,  0, 0, 0 },
    { P(ECDSA_KEY_P256),               1,  1, 1, 0 },
    { P(XTS_AES_256_KEY_1),            2,  1, 1, 0 },
    { P(XTS_AES_256_KEY_2),            3,  1, 1, 0 },
    { P(XTS_AES_128_KEY),              4,  1, 1, 0 },
    { P(HMAC_DOWN_ALL),                5,  0, 1, 0 },
    { P(HMAC_DOWN_JTAG),               6,  0, 1, 0 },
    { P(HMAC_DOWN_DIGITAL_SIGNATURE),  7,  0, 1, 0 },
    { P(HMAC_UP),                      8,  0, 1, 0 },
    { P(SECURE_BOOT_DIGEST0),          9,  0, 0, 1 },
    { P(SECURE_BOOT_DIGEST1),          10, 0, 0, 1 },
    { P(SECURE_BOOT_DIGEST2),          11, 0, 0, 1 },
    { P(KM_INIT_KEY),                  12, 0, 1, 0 },
    { P(ECDSA_KEY_P192),               16, 1, 1, 0 },
    { P(ECDSA_KEY_P384_L),             17, 1, 1, 0 },
    { P(ECDSA_KEY_P384_H),             18, 1, 1, 0 },
};

#undef P

/* ── Chip layouts ───────────────────────────────────────────────────────────── */

/* BLK0 WR_DIS / RD_DIS ranges from espefuse efuse_defs YAML (start/len). */
#define EFUSE_PROTECT_MODERN \
    .wr_dis = { .bit_start = 0,  .bit_count = 32 }, \
    .rd_dis = { .bit_start = 32, .bit_count = 7 }

/* All KEY_PURPOSE-bearing modern chips share modern_blocks and {88,4,8} geometry
 * (except C5/C61 base bits), but each carries its own purpose table, so they get
 * separate layouts even when block array + burn regs match. P4 v3.0's extra split
 * H-bit is handled in the burn code. */

/* C3 / C6: 4-bit KEY_PURPOSE at BLK0 bits 88+key*4. */
static const efuse_chip_layout_t esp32c3_c6_layout = {
    .blocks        = modern_blocks,
    .burn          = &burn_regs_common,
    .purposes      = purposes_c3_c6,
    .block_count   = sizeof(modern_blocks) / sizeof(modern_blocks[0]),
    .purpose_count = sizeof(purposes_c3_c6) / sizeof(purposes_c3_c6[0]),
    .key_purpose   = { .purpose0_bit = 88, .purpose_width = 4, .purpose0_wr_dis_bit = 8 },
    EFUSE_PROTECT_MODERN,
    .dis_download_mode_bit        = 128,
    .enable_security_download_bit = 133,
};

/* H2: same geometry as C3/C6, adds ECDSA. */
static const efuse_chip_layout_t esp32h2_layout = {
    .blocks        = modern_blocks,
    .burn          = &burn_regs_common,
    .purposes      = purposes_h2,
    .block_count   = sizeof(modern_blocks) / sizeof(modern_blocks[0]),
    .purpose_count = sizeof(purposes_h2) / sizeof(purposes_h2[0]),
    .key_purpose   = { .purpose0_bit = 88, .purpose_width = 4, .purpose0_wr_dis_bit = 8 },
    EFUSE_PROTECT_MODERN,
    .dis_download_mode_bit        = 128,
    .enable_security_download_bit = 133,
};

/* S3: same geometry as C3/C6, adds 256-bit XTS. */
static const efuse_chip_layout_t esp32s3_layout = {
    .blocks        = modern_blocks,
    .burn          = &burn_regs_common,
    .purposes      = purposes_s2_s3,
    .block_count   = sizeof(modern_blocks) / sizeof(modern_blocks[0]),
    .purpose_count = sizeof(purposes_s2_s3) / sizeof(purposes_s2_s3[0]),
    .key_purpose   = { .purpose0_bit = 88, .purpose_width = 4, .purpose0_wr_dis_bit = 8 },
    EFUSE_PROTECT_MODERN,
    .dis_download_mode_bit        = 128,
    .enable_security_download_bit = 133,
};

/* P4: same geometry, full key set; v3.0 split H-bit handled in burn code. */
static const efuse_chip_layout_t esp32p4_layout = {
    .blocks        = modern_blocks,
    .burn          = &burn_regs_common,
    .purposes      = purposes_p4,
    .block_count   = sizeof(modern_blocks) / sizeof(modern_blocks[0]),
    .purpose_count = sizeof(purposes_p4) / sizeof(purposes_p4[0]),
    .key_purpose   = { .purpose0_bit = 88, .purpose_width = 4, .purpose0_wr_dis_bit = 8 },
    EFUSE_PROTECT_MODERN,
    .dis_download_mode_bit        = 128,
    .enable_security_download_bit = 133,
};

/* S2: same key-purpose geometry as modern, different burn register offsets,
 * shares S3's purpose set. */
static const efuse_chip_layout_t esp32s2_layout = {
    .blocks        = modern_blocks,
    .burn          = &burn_regs_esp32s2,
    .purposes      = purposes_s2_s3,
    .block_count   = sizeof(modern_blocks) / sizeof(modern_blocks[0]),
    .purpose_count = sizeof(purposes_s2_s3) / sizeof(purposes_s2_s3[0]),
    .key_purpose   = { .purpose0_bit = 88, .purpose_width = 4, .purpose0_wr_dis_bit = 8 },
    EFUSE_PROTECT_MODERN,
    .dis_download_mode_bit        = 128,
    .enable_security_download_bit = 133,
};

/* C61: 4-bit KEY_PURPOSE at BLK0 bits 64+key*4 */
static const efuse_chip_layout_t esp32c61_layout = {
    .blocks        = modern_blocks,
    .burn          = &burn_regs_common,
    .purposes      = purposes_c61,
    .block_count   = sizeof(modern_blocks) / sizeof(modern_blocks[0]),
    .purpose_count = sizeof(purposes_c61) / sizeof(purposes_c61[0]),
    .key_purpose   = { .purpose0_bit = 64, .purpose_width = 4, .purpose0_wr_dis_bit = 8 },
    EFUSE_PROTECT_MODERN,
    .dis_download_mode_bit        = 96,
    .enable_security_download_bit = 100,
};

/* C5: 5-bit contiguous KEY_PURPOSE at BLK0 bits 86+key*5 */
static const efuse_chip_layout_t esp32c5_layout = {
    .blocks        = modern_blocks,
    .burn          = &burn_regs_esp32c5,
    .purposes      = purposes_c5,
    .block_count   = sizeof(modern_blocks) / sizeof(modern_blocks[0]),
    .purpose_count = sizeof(purposes_c5) / sizeof(purposes_c5[0]),
    .key_purpose   = { .purpose0_bit = 86, .purpose_width = 5, .purpose0_wr_dis_bit = 8 },
    EFUSE_PROTECT_MODERN,
    .dis_download_mode_bit        = 128,
    .enable_security_download_bit = 133,
};

#undef EFUSE_PROTECT_MODERN

/* ESP32 and C2 have no KEY_PURPOSE: key_purpose.purpose_width stays 0 and
 * purposes stays NULL. */
static const efuse_chip_layout_t esp32_layout = {
    .blocks        = esp32_blocks,
    .burn          = NULL,   /* legacy burn path */
    .block_count   = sizeof(esp32_blocks) / sizeof(esp32_blocks[0]),
    .wr_dis        = { .bit_start = 0,  .bit_count = 16 },
    .rd_dis        = { .bit_start = 16, .bit_count = 4 },
    .dis_download_mode_bit        = ESP_LOADER_EFUSE_BIT_NONE,
    .enable_security_download_bit = ESP_LOADER_EFUSE_BIT_NONE,
};

static const efuse_chip_layout_t esp32c2_layout = {
    .blocks        = esp32c2_blocks,
    .burn          = &burn_regs_esp32c2,
    .block_count   = sizeof(esp32c2_blocks) / sizeof(esp32c2_blocks[0]),
    .wr_dis        = { .bit_start = 0,  .bit_count = 8 },
    .rd_dis        = { .bit_start = 32, .bit_count = 2 },
    .dis_download_mode_bit        = 46,
    .enable_security_download_bit = 48,
};

/* ── Dispatch ───────────────────────────────────────────────────────────────── */

const efuse_chip_layout_t *efuse_get_chip_layout(target_chip_t target)
{
    switch (target) {
    case ESP32_CHIP:    return &esp32_layout;
    case ESP32C2_CHIP:  return &esp32c2_layout;
    case ESP32S2_CHIP:  return &esp32s2_layout;
    case ESP32S3_CHIP:  return &esp32s3_layout;
    case ESP32C5_CHIP:  return &esp32c5_layout;
    case ESP32C61_CHIP: return &esp32c61_layout;
    case ESP32H2_CHIP:  return &esp32h2_layout;
    case ESP32P4_CHIP:  return &esp32p4_layout;
    case ESP32C3_CHIP:
    case ESP32C6_CHIP:
        return &esp32c3_c6_layout;
    default:
        return NULL;
    }
}

const efuse_key_half_t *efuse_get_key_halves(target_chip_t target, uint8_t *count)
{
    switch (target) {
    case ESP32C2_CHIP: {
        /* C2 BLOCK_KEY0 (physical BLK3) splits into two 128-bit halves, each
         * with its own RD_DIS bit (efuse_defs esp32c2.yaml: BLOCK_KEY0 rd_dis
         * 0 1; the RD_DIS field starts at BLK0 bit 32, so absolute bits 32/33).
         * wr_dis is shared (bit 7), so it is not split. The whole-block
         * XTS_AES_128_KEY case read-protects by setting both halves' rd_dis_bit,
         * so only the halves are listed (no whole-block row). */
        /*                          blk  woff  wc  rd_dis */
        static const efuse_key_half_t halves[] = {
            {  3,    0,   4,   32 },  /* BLOCK_KEY0_LOW_128 — flash-enc key (derived-128) */
            {  3,    4,   4,   33 },  /* BLOCK_KEY0_HI_128  — secure-boot digest          */
        };
        *count = sizeof(halves) / sizeof(halves[0]);
        return halves;
    }
    default:
        *count = 0;
        return NULL;
    }
}
