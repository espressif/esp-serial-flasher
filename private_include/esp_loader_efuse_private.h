/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_loader.h"
#include "esp_loader_efuse.h"

/* Absent *bit positions* use ESP_LOADER_EFUSE_BIT_NONE from esp_loader_efuse.h,
 * which the public field-info table shares. Index-valued fields use
 * EFUSE_INDEX_NONE below instead. */

/* Sentinel for an absent block or key *index* (purpose_key_index, and the block
 * lookup in block_for_key_index) — e.g. a block that isn't a key block. Stays
 * uint8_t-wide: indices are bounded by ESP_LOADER_EFUSE_MAX_BLOCKS, so they can
 * never collide with a real value. (KEY_PURPOSE absence for a whole chip is
 * signalled separately by key_purpose.purpose_width == 0.) */
#define EFUSE_INDEX_NONE 0xFFu

/* Bytes per eFuse register word; every word-index-to-register-offset and
 * word-count-to-byte-count computation in the efuse sources uses this. */
#define EFUSE_WORD_BYTES 4u

/* Field order is chosen so the struct packs with no padding (8 bytes): the two
 * uint8_t members sit together between read_offset and the uint16_t bit
 * positions. The layout tables use positional initialisers, so this order is
 * also the table column order. */
typedef struct {
    uint16_t read_offset;
    uint8_t  word_count;
    /* Key index N if this block holds KEY_PURPOSE_N's key (selects the purpose
     * and WR_DIS bits below); EFUSE_INDEX_NONE if not a key block. Stored per
     * block rather than derived from the block index, so key blocks need not be
     * consecutive or start at a fixed offset. */
    uint8_t  purpose_key_index;
    /* Absolute BLK0 bits that write-/read-protect this block, or
     * ESP_LOADER_EFUSE_BIT_NONE when the block has no such fuse. */
    uint16_t wr_dis_bit;
    uint16_t rd_dis_bit;
} efuse_blk_layout_t;

/* Guard the packed size: this struct is replicated per block/chip and is the
 * dominant rodata cost of the layout tables. C11 _Static_assert. */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(efuse_blk_layout_t) == 8,
               "efuse_blk_layout_t grew — update layouts / MAX packing intentionally");
#endif

/* A read-protectable sub-range of a key block. Only chips that split a key
 * block populate one: ESP32-C2's BLOCK_KEY0 (256-bit, physical BLK3) doubles as
 * two independent 128-bit halves, each with its own RD_DIS bit. The whole-block
 * case (XTS_AES_128_KEY) read-protects by setting BOTH halves' rd_dis_bit, so
 * this table is the single source of truth for C2's per-half RD_DIS; the block
 * row's rd_dis_bit stays ESP_LOADER_EFUSE_BIT_NONE. Chips without a split leave
 * key_halves NULL — efuse_blk_layout_t and its sizeof are untouched. */
typedef struct {
    uint8_t  block;       /* physical block the half lives in */
    uint8_t  word_offset; /* first word of the half within the block */
    uint8_t  word_count;  /* words in the half */
    uint16_t rd_dis_bit;  /* absolute BLK0 RD_DIS bit for this half */
} efuse_key_half_t;

typedef struct {
    uint16_t pgm_data0_off;    /* EFUSE_PGM_DATA0_REG        */
    uint16_t check_value0_off; /* BLK1+: EFUSE_PGM_CHECK_VALUE0_REG (RS parity) */
    uint8_t  pgm_data_words;   /* number of EFUSE_PGM_DATA registers */
} efuse_pgm_layout_t;

typedef struct {
    uint16_t write_op;        /* EFUSE_CONF_REG value to enable write        */
    uint16_t read_op;         /* EFUSE_CONF_REG value to enable read         */
    uint8_t  pgm_cmd;         /* EFUSE_CMD_REG PGM bit                       */
    uint8_t  read_cmd;        /* EFUSE_CMD_REG READ bit                      */
    uint8_t  cmd_block_shift; /* bit offset of block number in EFUSE_CMD_REG */
} efuse_ctrl_cfg_t;

/*
 * Control register offsets from efuse_base.
 * Timing register offsets (DAC_CONF, WR_TIM_CONF0/1/2) are chip-specific and
 * live as local constants inside efuse_set_timing's switch cases.
 */
typedef struct {
    const efuse_pgm_layout_t *pgm;
    const efuse_ctrl_cfg_t   *ctrl;
    uint16_t conf_off;           /* EFUSE_CONF_REG                            */
    uint16_t cmd_off;            /* EFUSE_CMD_REG                             */
    /* Post-burn error checks; which one applies depends on the block's coding. */
    uint16_t rs_err0_off;        /* BLK1+: RS_ERR0_REG, ERR1 is the next word */
    uint16_t repeat_err0_off;    /* BLK0: REPEAT_ERR0_REG                     */
    uint8_t  repeat_err_words;   /* BLK0: REPEAT_ERR registers to check       */
} efuse_burn_regs_t;

/* Geometry of the KEY_PURPOSE_0..N selector fields inside BLK0.
 *
 * The fields are packed consecutively (stride == width) and their WR_DIS bits
 * are consecutive too, so only the first field plus the width is stored:
 *   KEY_PURPOSE_N field   = BLK0 bits [purpose0_bit + N*purpose_width, +width)
 *   KEY_PURPOSE_N WR_DIS  = WR_DIS bit (purpose0_wr_dis_bit + N)
 * N is the block's purpose_key_index, not derived from the block index. This
 * consecutiveness is verified against espefuse for every chip
 * (S2/S3/C3/C5/C6/C61/H2/P4/H4/H21/E22/S31).
 *
 * Lone exception: P4 v3.0 adds a NON-contiguous 5th "H" bit per field (see the
 * comment on esp32p4_layout); that bit is written by the burn code, not modelled
 * here, so this struct still describes the contiguous low part for P4 v3.0 too. */
/* Both bit positions are absolute BLK0 positions, hence uint16_t (BLK0 reaches
 * 288 bits on ESP32-S31). Initialised with designated initialisers, so field
 * order here is independent of the chip layout tables. */
typedef struct {
    uint16_t purpose0_bit;        /* BLK0 bit position of KEY_PURPOSE_0; unused if width==0 */
    uint16_t purpose0_wr_dis_bit; /* WR_DIS bit that locks KEY_PURPOSE_0 (separate fuse
                                   * from the key data's wr_dis_bit in efuse_blk_layout_t) */
    uint8_t  purpose_width;       /* bits per field (4 or 5); 0 = chip has no KEY_PURPOSE */
} efuse_key_purpose_field_t;

/* One KEY_PURPOSE the chip understands. `purpose` is the stable library tag
 * (esp_loader_key_purpose_t); `code` is the silicon value actually written to
 * the KEY_PURPOSE_N field (differs per chip, hence per-chip tables). The flags
 * mirror espefuse's per-purpose columns:
 *   needs_reverse    — key bytes are stored reversed (XTS_AES / ECDSA keys)
 *   needs_rd_protect — block must be read-protected after burn (keys, not digests)
 *   is_digest        — Secure Boot digest (must stay boot-ROM-readable: no rd-protect) */
typedef struct {
    uint8_t purpose;             /* esp_loader_key_purpose_t, used uint8_t to save space */
    uint8_t code;                /* silicon KEY_PURPOSE value */
    uint8_t needs_reverse    : 1;
    uint8_t needs_rd_protect : 1;
    uint8_t is_digest        : 1;
} efuse_key_purpose_row_t;

/* Contiguous BLK0 bit range (espefuse YAML start/len for WR_DIS / RD_DIS).
 * uint16_t matches esp_loader_efuse_desc_t bit positions (BLK0 can exceed 255 bits). */
typedef struct {
    uint16_t bit_start;
    uint16_t bit_count;
} efuse_bit_range_t;

typedef struct {
    const efuse_blk_layout_t  *blocks;
    const efuse_burn_regs_t   *burn;
    const efuse_key_purpose_row_t *purposes; /* NULL if chip has no KEY_PURPOSE */
    uint8_t                    block_count;
    uint8_t                    purpose_count;
    efuse_key_purpose_field_t  key_purpose;
    /* BLK0 WR_DIS / RD_DIS field geometry (espefuse efuse_defs). commit()
     * postpones these ranges to a second BLK0 pass. espefuse also postpones a
     * few security fields by name (DIS_DOWNLOAD_MODE, etc.); those are not
     * ranges and stay out of this struct. */
    efuse_bit_range_t          wr_dis;
    efuse_bit_range_t          rd_dis;
    uint16_t                   dis_download_mode_bit;
    uint16_t                   enable_security_download_bit;
} efuse_chip_layout_t;

/* P4 v3.0 adds a non-contiguous H-bit to each KEY_PURPOSE field (5-bit split).
 * write_key gates on get_security_info().eco_version >= ESP32P4_ECO_REV3_MIN (5),
 * then stages the contiguous low bits + H-bit when code >= (1 << purpose_width):
 *   KEY0-4: H-bit at BLK0 bit 155+key_idx
 *   KEY5:   H-bit at BLK0 bit 164
 * (WR_DIS for H shares purpose0_wr_dis_bit + key_idx with the low field.) */

const efuse_chip_layout_t *efuse_get_chip_layout(target_chip_t target);

/* Tests bit `bit` (absolute BLK0 bit position) in an already-read BLK0 image.
 * Out-of-range (word beyond blk->word_count) reads as clear. Shared between
 * the write-side precondition checks and the read-protect mask computation. */
bool efuse_blk_bit_is_set(const esp_loader_efuse_block_t *blk, uint16_t bit);

/* Per-target key-block split table. Returns NULL / *count == 0 for chips whose
 * key blocks aren't split (everyone except C2, whose BLOCK_KEY0 doubles as two
 * 128-bit halves with independent RD_DIS bits). Consumers iterate the result
 * without branching on the target, so a future split-block chip is handled by
 * adding a case here — not by touching consumers. */
const efuse_key_half_t *efuse_get_key_halves(target_chip_t target, uint8_t *count);

/* Returns pointer to the staged words for one block. For test/debug only. */
const uint32_t *efuse_get_write_buf(const esp_loader_efuse_ctx_t *ctx, uint8_t block);

esp_loader_error_t efuse_validate_staged_writes(esp_loader_t *loader, esp_loader_efuse_ctx_t *ctx);

/* Looks up a key purpose in the chip's per-chip table. Returns NULL if the chip
 * has no KEY_PURPOSE field or does not support this purpose. */
const efuse_key_purpose_row_t *efuse_lookup_purpose(target_chip_t target,
        esp_loader_key_purpose_t purpose);

/* Whether a key burned with this purpose must be read-protected (XTS_AES, HMAC,
 * ECDSA keys) — digests and USER must stay readable. *out is set on success.
 * Unknown purpose for the chip -> ESP_LOADER_ERROR_INVALID_PARAM. */
esp_loader_error_t efuse_purpose_needs_read_protect(target_chip_t target,
        esp_loader_key_purpose_t purpose, bool *out);

/* Whether the key bytes must be stored byte-reversed for this purpose (XTS_AES /
 * ECDSA keys). *out is set on success. Unknown purpose -> ESP_LOADER_ERROR_INVALID_PARAM. */
esp_loader_error_t efuse_key_purpose_needs_reverse(target_chip_t target,
        esp_loader_key_purpose_t purpose, bool *out);

/*
 * Generic coding-scheme classification, used to decide whether a block's bits
 * can be staged/burned independently (NONE) or whether the whole block must
 * be written in one shot because parity covers it (PARITY — RS on modern
 * chips, 3/4 on ESP32 legacy). Distinct from ESP32's own NONE/3-4/recovery
 * raw scheme value (see esp32_read_coding_scheme), which the burn engine
 * still needs to pick the actual encoder.
 */
typedef enum {
    EFUSE_CODING_NONE,
    EFUSE_CODING_PARITY,
} efuse_coding_t;

/* Classifies one block's coding scheme. For modern chips this is a fixed
 * compile-time fact (BLK0 = NONE, BLK1+ = RS); for ESP32 legacy, blocks 1-3
 * require a runtime register read (BLOCK0 word 6), BLOCK0 itself is always
 * NONE. */
esp_loader_error_t efuse_get_block_coding(esp_loader_t *loader, target_chip_t target,
        const efuse_chip_layout_t *layout, uint32_t base, uint8_t block, efuse_coding_t *coding);

/* Reads the ESP32 legacy coding scheme (raw value: CODING_SCHEME_NONE/3-4/
 * recovery folded to NONE) from BLOCK0 word 6. Used both by the burn engine
 * (to pick the 3/4 encoder) and by efuse_get_block_coding(). */
esp_loader_error_t esp32_read_coding_scheme(esp_loader_t *loader, uint32_t base,
        const efuse_chip_layout_t *layout, uint8_t *scheme);

/* Canonical "no extra coding" value returned by esp32_read_coding_scheme()
 * (recovery is folded into this too). The 3/4 scheme's own raw value is an
 * implementation detail of the burn engine and stays local to it. */
#define ESP32_CODING_SCHEME_NONE 0u

/* RS(44,32) encoder — 8 data words in, 3 parity words out (both little-endian).
 * Byte order handled internally via shifts; host-endian agnostic.
 * Only for BLK1+ on modern chips; BLK0 bypasses RS entirely. */
void efuse_rs_encode(const uint32_t *data, uint32_t *parity);

/* 3/4 coding scheme encoder — 6 data words (24 bytes) in, 8 words out (both
 * little-endian). ESP32 legacy BLK1-3 only, when the chip's runtime-detected
 * coding scheme is CODING_SCHEME_34; BLK0 always bypasses this. */
void efuse_coding34_encode(const uint32_t *data, uint32_t *out);
