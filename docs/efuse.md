# eFuse Support

ESP Serial Flasher can read and burn the eFuses of a connected ESP target over the serial bootloader protocol, from any host the library runs on.

> [!CAUTION]
> **Burning eFuses is irreversible.** An eFuse bit can only go from 0 to 1, and there is no way to undo it. Burning the wrong bit can permanently disable download mode, JTAG, or the chip's ability to boot your firmware. [Reading eFuses](#reading-efuses) is always safe; treat every call to [`esp_loader_efuse_commit()`](#committing-burning) as permanent.

- [eFuse Support](#efuse-support)
  - [Concepts](#concepts)
  - [Chip Support](#chip-support)
  - [Reading eFuses](#reading-efuses)
    - [Raw Blocks](#raw-blocks)
    - [Fields](#fields)
  - [Named Fields](#named-fields)
  - [Writing eFuses](#writing-efuses)
    - [Staging](#staging)
    - [Committing (Burning)](#committing-burning)
    - [Commit Ordering](#commit-ordering)
    - [Partial Failure](#partial-failure)
  - [Burning Keys](#burning-keys)
    - [Modern Chips — `esp_loader_efuse_write_key()`](#modern-chips--esp_loader_efuse_write_key)
    - [ESP32 and ESP32-C2 — `esp_loader_efuse_write_key_legacy()`](#esp32-and-esp32-c2--esp_loader_efuse_write_key_legacy)
  - [Error Codes](#error-codes)
  - [Flash Footprint](#flash-footprint)

All eFuse APIs live in [include/esp_loader_efuse.h](../include/esp_loader_efuse.h) and require a connected loader context (`esp_loader_connect()`), because the chip must be detected before its eFuse layout is known.

## Concepts

**eFuses are one-time-programmable.** Every eFuse bit starts as 0 and can be burned to 1 exactly once. Nothing in this library — or in the hardware — can clear a burned bit.

**eFuses are organised in blocks.** BLK0 holds configuration and the protection fuses; the remaining blocks hold MAC/system data, user data, and key material. Block count and size are chip-specific (4 blocks on ESP32 and ESP32-C2, 11 on the modern family).

**Blocks other than BLK0 are parity-coded.** On modern chips BLK1 and above are protected by RS(44,32) encoding, which is computed over the whole block; ESP32 optionally uses a 3/4 coding scheme. A parity-coded block can therefore only be written **once, in full** — you cannot burn a few more bits into a block that already has content. BLK0 is plain OTP and can be burned bit by bit.

**BLK0 holds the protection fuses.** `WR_DIS` bits make other fuses permanently unwritable, `RD_DIS` bits make them unreadable by software (the hardware then returns zeros).

**Writes are staged, never immediate.** Every `esp_loader_efuse_write_*` function only accumulates bits in a caller-owned RAM buffer (`esp_loader_efuse_ctx_t`). The single function that touches hardware in a destructive way is `esp_loader_efuse_commit()`:

```c
esp_loader_efuse_ctx_t ctx = {0};            /* must be zero-initialised */

esp_loader_efuse_write_field(&loader, &ctx, FIELD, data, bits);  /* RAM only */
esp_loader_efuse_write_field_bit(&loader, &ctx, OTHER_FIELD);    /* RAM only */

esp_loader_efuse_commit(&loader, &ctx);      /* <-- burns, irreversibly */
```

This makes it safe to build up, inspect, and abandon a set of writes. It also lets the library burn everything in the hardware-safe order (see [Commit Ordering](#commit-ordering)).

## Chip Support

| Target    | eFuse support | Key blocks              |
| :-------- | :-----------: | :---------------------- |
| ESP8266   |      ❌       | —                       |
| ESP32     |      ✅       | BLK1, BLK2 (positional) |
| ESP32-S2  |      ✅       | KEY0–KEY5               |
| ESP32-S3  |      ✅       | KEY0–KEY5               |
| ESP32-C2  |      ✅       | BLOCK_KEY0 (2 halves)   |
| ESP32-C3  |      ✅       | KEY0–KEY5               |
| ESP32-C5  |      ✅       | KEY0–KEY5               |
| ESP32-C6  |      ✅       | KEY0–KEY5               |
| ESP32-C61 |      ✅       | KEY0–KEY5               |
| ESP32-H2  |      ✅       | KEY0–KEY5               |
| ESP32-H21 |      🚧       | –                       |
| ESP32-P4  |      ✅       | KEY0–KEY5               |
| ESP32-S31 |      🚧       | —                       |

**Legend**: ✅ Supported | ❌ Not supported | 🚧 Under development

A target without eFuse support returns `ESP_LOADER_ERROR_UNSUPPORTED_FUNC` from every eFuse function.

## Reading eFuses

Reading never burns anything and needs no write context.

### Raw Blocks

```c
esp_loader_efuse_image_t image;
if (esp_loader_efuse_read_image(&loader, &image) != ESP_LOADER_SUCCESS) { /* ... */ }

for (uint8_t blk = 0; blk < image.block_count; blk++) {
    const esp_loader_efuse_block_t *b = &image.blocks[blk];
    printf("BLOCK%u:", b->block);
    for (uint8_t w = 0; w < b->word_count; w++) {
        if (b->read_protected_word_mask & (1u << w)) {
            printf(" ????????");            /* masked by RD_DIS — not a real value */
        } else {
            printf(" %08" PRIx32, b->words[w]);
        }
    }
    printf("\n");
}
```

`esp_loader_efuse_read_block()` reads a single block if you do not need the whole image.

> [!IMPORTANT]
> **A zero word is not necessarily a zero fuse.** When a block (or, on ESP32-C2, one half of the key block) is read-protected, the hardware returns zeros. Always check `read_protected_word_mask` — bit *N* set means `words[N]` is masked, not read. Because resolving this requires the live `RD_DIS` bits, reading any block other than BLK0 also reads BLK0 (one extra round-trip).

### Fields

`esp_loader_efuse_read_field()` extracts a named field, concatenating each of its bit ranges LSB-first into a caller buffer:

```c
#include "efuse/esp32c6/esp_loader_efuse_table.h"

uint8_t mac[6] = {0};
esp_loader_efuse_read_field(&loader, ESP32C6_EFUSE_MAC, mac, sizeof(mac) * 8u);
```

Each call reads the chip.

## Named Fields

Field descriptors are generated from ESP-IDF's eFuse CSV tables, one header and one source file per chip:

```
include/efuse/<chip>/esp_loader_efuse_table.h   declarations (ESP32C6_EFUSE_MAC, …)
src/efuse/<chip>/esp_loader_efuse_table.c       descriptor data
```

A field is a `NULL`-terminated array of `esp_loader_efuse_desc_t` (block, start bit, bit count) — the same shape ESP-IDF uses — so a field may span several non-contiguous ranges. Bits are always packed **LSB-first**; for anything wider than a byte use a `uint8_t` array rather than a native integer type, which would give wrong results on a big-endian host.

Some chips have revision-specific tables (`ESP32P4_EFUSE_*` vs. `ESP32P4_REV1_EFUSE_*`, and a separate ESP32-H2 v0.0–v1.1 table), so naming a table directly means picking the one that matches the silicon in front of you. Where flash size is not a constraint, let `esp_loader_efuse_get_field_info()` do that: it resolves a chip and revision to the right field set at runtime, including the correct revision-specific table, and hands back each field's name, descriptor and protection bits:

```c
const esp_loader_efuse_field_set_t *set;
if (esp_loader_efuse_get_field_info(chip, revision, &set) == ESP_LOADER_SUCCESS) {
    for (uint16_t i = 0; i < set->count; i++) {
        printf("%s\n", set->fields[i].name);   /* also: wr_dis_bit / rd_dis_bit */
    }
}
```

`revision` is `major * 100 + minor` (v3.0 → `300`), matching ESP-IDF's `esp_chip_info_t.revision`. Obtain it from [`esp_loader_get_chip_revision()`](../include/esp_loader.h), which reads the chip's wafer version eFuses:

```c
uint16_t revision;
esp_loader_get_chip_revision(&loader, &revision);   /* 300 on ESP32-P4 v3.0 */
```

It works on every target with a chip revision (all but the ESP8266, which returns `ESP_LOADER_ERROR_UNSUPPORTED_CHIP`) and over any protocol, since it only reads registers. Note this is not the same number as `eco_version` in `esp_loader_get_security_info()`, which is an ECO count.

> [!NOTE]
> `esp_loader_efuse_get_field_info()` references every chip's table, so calling it links all of them. See [Flash Footprint](#flash-footprint).

## Writing eFuses

### Staging

| Function                              | Stages                                         | Touches the chip |
| :------------------------------------ | :--------------------------------------------- | :--------------- |
| `esp_loader_efuse_write_field()`      | *n* bits of a named field                      | no               |
| `esp_loader_efuse_write_field_bit()`  | a single-bit named field (flags, disable bits) | no               |
| `esp_loader_efuse_write_bit()`        | one bit at an absolute (block, bit) position   | no               |
| `esp_loader_efuse_write_key()`        | key + KEY_PURPOSE + protection bits            | yes (validation) |
| `esp_loader_efuse_write_key_legacy()` | key + protection bits (ESP32 / C2)             | yes (validation) |

The context is a plain struct holding nothing but the staged words, so where it lives is up to you — stack, static storage, or the heap. What matters is that it is **zero-initialised** and stays alive until `commit()`:

```c
esp_loader_efuse_ctx_t ctx = {0};        /* or memset() a heap allocation */
```

Staging only ORs bits in, so staging the same field twice is cumulative, and one context can hold writes to several blocks at once. Its internal layout is private; inspect the result by committing, or by reading the chip afterwards.

### Committing (Burning)

```c
esp_loader_error_t err = esp_loader_efuse_commit(&loader, &ctx);
```

`commit()` validates the staged writes against the chip's current state, burns them, verifies the result by reading back (with one exception, see [Commit Ordering](#commit-ordering)), and clears the context on success.

Validation rejects, before anything is burned, a **parity-coded** block (BLK1+ on modern chips, ESP32 blocks under 3/4 coding) whose content on the chip is neither empty nor already exactly equal to what you staged — `ESP_LOADER_ERROR_EFUSE_BLOCK_IN_USE`. If it is already equal, that block is silently dropped from the context and not re-burned. BLK0 has no such restriction: it is plain OTP, and staging can only add 1 bits.

### Commit Ordering

1. Blocks are burned from the highest index down to BLK1, skipping empty ones.
2. BLK0 is burned last, in two passes: everything except the `WR_DIS` / `RD_DIS` ranges first, then those protection bits — data must land before the fuses that lock it. `DIS_DOWNLOAD_MODE` and `ENABLE_SECURITY_DOWNLOAD` ride along in that second pass, because they end the download session that would be needed to burn anything after them.

Each burn is retried up to three times and read back; a block that cannot be verified fails the commit with `ESP_LOADER_ERROR_EFUSE_BURN_FAILED`.

The one exception is a second pass carrying `DIS_DOWNLOAD_MODE` or `ENABLE_SECURITY_DOWNLOAD`: the chip stops answering the moment they are programmed, so read-back is impossible and that pass is not verified. `commit()` reports success once the programming command has been issued. This matches `espefuse`, which skips confirmation for the same fuses.

### Partial Failure

If a commit fails part-way, the context is left holding **exactly the work that was not burned**: successfully burned blocks are cleared from it, while the failed one and everything not yet attempted stay staged. Fix the cause and call `commit()` again to retry the remainder — do not re-stage from scratch, or already-burned blocks will be staged a second time.

> [!WARNING]
> Staging irreversible security bits in the *same* commit as ordinary data can leave those bits burned even when the commit reports failure. `DIS_DOWNLOAD_MODE` and `ENABLE_SECURITY_DOWNLOAD` are handled — they are burned last, so an earlier failure never reaches them — but fuses such as `SECURE_BOOT_EN` and `SPI_BOOT_CRYPT_CNT` are not, and burn in the first BLK0 pass. If that matters, burn and verify the data first, then commit the security bits separately.

> [!NOTE]
> After burning `DIS_DOWNLOAD_MODE` the chip stops responding immediately — not at the next reset. `commit()` still reports success, but any later call on the same connection fails with `ESP_LOADER_ERROR_TIMEOUT`. That is expected, and the library does not attempt to reconnect. On the ESP32 the equivalent fuse is `UART_DOWNLOAD_DIS`. Read-back still succeeds there, so it is burned and verified like any other bit; the connection becomes unusable only afterwards, and on revision 3.0 and later only.

## Burning Keys

Key blocks are more than raw storage: a key must be accompanied by its purpose and the right protection fuses, and some purposes require the bytes to be stored reversed. Both key APIs handle all of that; like every other write function, they only **stage** — `commit()` burns.

### Modern Chips — `esp_loader_efuse_write_key()`

```c
esp_loader_efuse_ctx_t ctx = {0};
const uint8_t key[32] = { /* ... */ };

esp_loader_error_t err = esp_loader_efuse_write_key(
        &loader, &ctx,
        ESP_LOADER_EFUSE_KEY0,                     /* KEYn selector, not a block index */
        ESP_LOADER_KEY_PURPOSE_XTS_AES_128_KEY,
        key, sizeof(key),
        NULL);                                     /* NULL = safe defaults */

if (err == ESP_LOADER_SUCCESS) {
    err = esp_loader_efuse_commit(&loader, &ctx);
}
```

With the default config (`NULL`, or a zeroed `esp_loader_efuse_write_key_config_t`) the block is write-protected, and read-protected when the purpose requires secrecy. Secure Boot digests are never read-protected — the boot ROM must be able to read them. `KEY_PURPOSE` itself is always write-protected, regardless of the config flags.

`ESP_LOADER_EFUSE_KEY0..KEY5` are *key indices*, resolved to the chip's actual block at runtime; selecting a key block the chip does not have returns `ESP_LOADER_ERROR_INVALID_PARAM`. The target block must be **free** — writeable, readable, `KEY_PURPOSE` unset, and all-zero on the chip — otherwise the call returns `ESP_LOADER_ERROR_EFUSE_BLOCK_IN_USE`. Freeness is checked against the chip, not against the context. (Staging two keys into the same block before committing is therefore caught later, by commit validation.)

Purposes supported per chip:

| Purpose                    | S2  | S3  | C3  | C6  | C61 | H2  | C5  | P4  |
| :------------------------- | :-: | :-: | :-: | :-: | :-: | :-: | :-: | :-: |
| `USER`                     | ✅  | ✅  | ✅  | ✅  | ✅  | ✅  | ✅  | ✅  |
| `XTS_AES_128_KEY`          | ✅  | ✅  | ✅  | ✅  | ✅  | ✅  | ✅  | ✅  |
| `XTS_AES_256_KEY_1` / `_2` | ✅  | ✅  | ❌  | ❌  | ❌  | ❌  | ❌  | ✅  |
| `XTS_AES_128_PSRAM_KEY`    | ❌  | ❌  | ❌  | ❌  | ❌  | ❌  | ✅  | ❌  |
| `HMAC_UP` / `HMAC_DOWN_*`  | ✅  | ✅  | ✅  | ✅  | ❌  | ✅  | ✅  | ✅  |
| `SECURE_BOOT_DIGEST0..2`   | ✅  | ✅  | ✅  | ✅  | ✅  | ✅  | ✅  | ✅  |
| `ECDSA_KEY_P256`           | ❌  | ❌  | ❌  | ❌  | ✅  | ✅  | ✅  | ✅  |
| `ECDSA_KEY_P192`           | ❌  | ❌  | ❌  | ❌  | ❌  | ❌  | ✅  | ✅  |
| `ECDSA_KEY_P384_L` / `_H`  | ❌  | ❌  | ❌  | ❌  | ❌  | ❌  | ✅  | ✅  |
| `KM_INIT_KEY`              | ❌  | ❌  | ❌  | ❌  | ❌  | ❌  | ✅  | ✅  |

An unsupported purpose is rejected with `ESP_LOADER_ERROR_INVALID_PARAM`. 512-bit keys (`XTS_AES_256`, `ECDSA_P384`) have no combined selector — burn each half into its own block with the concrete `_KEY_1`/`_KEY_2` or `_L`/`_H` purpose.

Keys shorter than the 32-byte block are zero-padded. For ECDSA purposes the padding is applied as leading bytes before the block is reversed; other reversed purposes keep the key in the low bytes and then reverse.

On **ESP32-P4**, purposes whose silicon code does not fit the contiguous 4-bit `KEY_PURPOSE` field need the split fifth bit that only v3.0 silicon has. On earlier P4 revisions those purposes return `ESP_LOADER_ERROR_UNSUPPORTED_FUNC`.

### ESP32 and ESP32-C2 — `esp_loader_efuse_write_key_legacy()`

These chips have no `KEY_PURPOSE` fuse: a key's role is structural, so it is selected by target rather than by a (block, purpose) pair.

| Target                                         | Chip  | Storage                      | Behaviour                                                |
| :--------------------------------------------- | :---- | :--------------------------- | :------------------------------------------------------- |
| `ESP_LOADER_LEGACY_KEY_ESP32_FLASH_ENCRYPTION` | ESP32 | BLK1                         | reversed, read-protected                                 |
| `ESP_LOADER_LEGACY_KEY_ESP32_SECURE_BOOT_V1`   | ESP32 | BLK2 (AES key)               | reversed, read-protected                                 |
| `ESP_LOADER_LEGACY_KEY_ESP32_SECURE_BOOT_V2`   | ESP32 | BLK2 (RSA public-key digest) | not reversed, stays readable; v3.0+ and NONE coding only |
| `ESP_LOADER_LEGACY_KEY_C2_XTS_AES_128`         | C2    | whole BLOCK_KEY0             | reversed, read-protected, sets `XTS_KEY_LENGTH_256`      |
| `ESP_LOADER_LEGACY_KEY_C2_XTS_AES_128_DERIVED` | C2    | low 128 bits of BLOCK_KEY0   | reversed, read-protected                                 |
| `ESP_LOADER_LEGACY_KEY_C2_SECURE_BOOT_DIGEST`  | C2    | high 128 bits of BLOCK_KEY0  | not reversed, stays readable                             |

A target belonging to a different chip than the detected one is rejected with `ESP_LOADER_ERROR_INVALID_PARAM`. On ESP32 the usable block size follows the runtime-detected coding scheme: 24 bytes under 3/4 coding, 32 bytes otherwise.

Secure Boot V2 takes the 32-byte SHA-256 digest of the RSA public key. This library does not compute digests or parse PEM files — supply the bytes.

## Error Codes

| Code                                  | Typical cause                                                                                                                                                                                 |
| :------------------------------------ | :-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `ESP_LOADER_ERROR_UNSUPPORTED_FUNC`   | Chip has no eFuse support in this library; key API used on a chip without `KEY_PURPOSE`; P4 purpose needing v3.0 silicon                                                                      |
| `ESP_LOADER_ERROR_INVALID_PARAM`      | A caller mistake: out-of-range block, a field descriptor the detected chip does not have, unsupported key purpose, or bad key size                                                            |
| `ESP_LOADER_ERROR_EFUSE_BLOCK_IN_USE` | The chip's state blocks the write: key block not free, or a parity-coded block already holds different content. Nothing is burned — pick another block                                        |
| `ESP_LOADER_ERROR_EFUSE_BURN_FAILED`  | Programming was attempted and the chip answered: it reported an error, or read-back verification failed after three tries. Fuses may be partially burned; the context keeps the unburned work |
| `ESP_LOADER_ERROR_UNSUPPORTED_CHIP`   | `esp_loader_efuse_get_field_info()` has no table for that chip and revision                                                                                                                   |
| `ESP_LOADER_ERROR_INVALID_TARGET`     | Chip layout inconsistent with the compiled-in maxima (should not happen)                                                                                                                      |
| `ESP_LOADER_ERROR_TIMEOUT`            | Transport timeout, reported as itself rather than as a burn failure — including the expected connection drop after burning `DIS_DOWNLOAD_MODE`. Fuses may already be programmed               |

## Flash Footprint

The eFuse code itself is small, but the generated per-chip **field tables** are not — and they are linked only when your application actually references them. See [Flash Size Footprint](../README.md#flash-size-footprint) in the main README for the orders of magnitude involved and the rules that decide whether you pay for them.
