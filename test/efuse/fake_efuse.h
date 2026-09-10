/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * In-memory stand-in for the register-I/O seam. Linked into tests instead of
 * esp_loader.c, it defines esp_loader_read_register / _write_register /
 * get_security_info over a sparse address->word store, so the eFuse engine runs
 * against a "chip" that is just memory. Reads default to 0 (a blank chip: every
 * key block free, no protection); tests seed specific words to model other
 * states. Writes are stored and appended to a transcript for burn-order asserts.
 */

#pragma once

#include <stdint.h>
#include "esp_loader.h"

/* Clear the register store, the transcript, and the security-info block. */
void fake_efuse_reset(void);

/* Make `block` word `word` already hold `value`, as if it had been burned before
 * the test started: a key block that is already in use, a protection bit that is
 * already set, data for a read to find. Written at the address the read path
 * resolves to (efuse base + block read offset + word * 4). */
void fake_set_efuse_word(target_chip_t target, uint8_t block, uint8_t word, uint32_t value);

/* Read the final device-side image of one eFuse word. This deliberately reads
 * the fake chip, not ctx staging or the PGM_DATA registers. */
uint32_t fake_get_efuse_word(target_chip_t target, uint8_t block, uint8_t word);

/* Set a register by raw address, for the few reads that do not go through a
 * block/word (ESP32's APB_CTL_DATE, which chip-revision detection folds in). */
void fake_set_reg(uint32_t addr, uint32_t value);

/* Set a single bit of `block`, ORing into whatever is already there — unlike
 * fake_set_efuse_word, which overwrites the whole word. For protection bits,
 * where several land in one word and setting one must not clear the rest. */
void fake_set_efuse_bit(target_chip_t target, uint8_t block, uint16_t bit);

/* Value returned by esp_loader_get_security_info (zeroed by reset). */
void fake_set_security_info(const esp_loader_target_security_info_t *info);

/* Write transcript (for burn-order / postpone assertions in commit tests). */
typedef struct {
    uint32_t addr;
    uint32_t value;
} fake_reg_write_t;

const fake_reg_write_t *fake_writes(void);
uint32_t fake_write_count(void);

/*
 * Reset, and make writes to `target`'s command register act like a chip: a
 * PGM_CMD burn shows up in the block's read-side registers and the command bit
 * self-clears, so the burn path's poll-and-verify loop completes. Tests that
 * never issue a command just call fake_efuse_reset().
 *
 * Modern chips only — ESP32 legacy's burn registers are private to
 * esp_loader_efuse_burn.c, so passing ESP32_CHIP aborts.
 */
void fake_efuse_reset_for_burn(target_chip_t target);

/* Fail the next `attempts` PGM_CMD burns of `block` (read-back verify sees
 * stale/zero data, driving esp_loader_efuse_burn.c's retry loop), then let
 * subsequent attempts succeed normally. Cleared by fake_efuse_reset(). Set
 * block to EFUSE_INDEX_NONE (or just don't call this) for no injection. */
void fake_inject_error(uint8_t block, uint8_t attempts);

/* Minimal loader for `chip`, backed by the fake's private port. */
esp_loader_t fake_loader(target_chip_t chip);
