#!/usr/bin/env python3
#
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0
"""Copy, namespace, and strip ESP-IDF eFuse tables for esp-serial-flasher.

One section each below:

  1. clean    -- delete whatever a previous run generated
  2. stage    -- copy IDF's .c and .csv tables into a scratch dir under our names
  3. tables   -- rewrite the staged .c into our namespace, with (4) appended
  4. metadata -- resolve field names and protection bits from the staged .csv
  5. headers  -- declare what the tables define, one header per chip

Section 4 is a helper of section 3 rather than a stage of its own; it is kept
apart because it is the only part that reads a CSV, and the name/protection
resolution it does is the subtlest thing here.

Only (2) reads ESP-IDF; everything after it reads only what was staged, so the
rename is settled in one place. The scratch dir is temporary: the CSVs are an
input to generation, not something the library ships, so nothing of (2)
survives the run.
"""

from __future__ import annotations

import argparse
import re
import shutil
import tempfile
from collections import defaultdict
from pathlib import Path
from typing import NamedTuple


# Generated files are new to this repository, so they carry a single year.
COPYRIGHT_YEAR = 2026

SUPPORTED_CHIPS = (
    "esp32",
    "esp32c2",
    "esp32c3",
    "esp32c5",
    "esp32c6",
    "esp32c61",
    "esp32h2",
    "esp32p4",
    "esp32s2",
    "esp32s3",
)


# ---------------------------------------------------------------------------
# Chip-specific handling
#
# Anything a single chip needs that the generic transform must not apply to the
# others lives here, keyed by chip, so per-chip quirks stay collected in one
# place instead of scattered through the generic path.
#
#   substitutions -- literal (old, new) text replacements applied after the
#                    generic transforms.
#   tables        -- per-file overrides, {idf_source: (output, prefix, rev_min,
#                    rev_max)}. output/prefix may be None to keep the default.
#                    Revisions are major*100 + minor, the format ESP-IDF's
#                    esp_chip_info_t.revision and this library's
#                    loader_read_chip_revision() both use, and both bounds are
#                    inclusive. Tables not listed here are still picked up; they
#                    keep IDF's filename, get the prefix derived from it, and
#                    apply to every revision. Nothing needs listing to be found.
#   extends       -- {supplement: base}, for a variant that ADDS fields to
#                    another table rather than replacing it. Its fields are
#                    appended to the base metadata, so both revisions share one
#                    array and the newer one is a shorter view of it.
#
# esp32: the only chip whose table parameterises the block width on the coding
#   scheme via CONFIG_EFUSE_MAX_BLK_LEN; we pin it to the 256-bit "None" scheme,
#   the only layout the common table supports. Every other chip already
#   hardcodes 256 in the IDF source, so they need no substitution.
# esp32p4: the one chip whose naming has to be inverted. v3.0+ is the mainstream
#   silicon, so it takes the plain name, and IDF's base table (pre-v3, revs
#   0.0-2.0) is demoted to _rev1 -- mirroring the stub split (esp_stub_esp32p4.c
#   vs esp_stub_esp32p4rev1.c). Without this both would keep IDF's names and the
#   legacy table would be the one called "the" table.
# esp32h2: ECDSA_FORCE_USE_HARDWARE_K exists only on revisions v0.0-v1.1 and is
#   absent from the base table, so this variant adds a field instead of replacing
#   the table. IDF says the same by compiling both unconditionally, where for P4
#   it picks one with an if/else.
# ---------------------------------------------------------------------------
REVISION_ANY = 0xFFFF  # no upper bound; mirrors ESP_LOADER_CHIP_REV_ANY

CHIP_SPECIFIC: dict[str, dict] = {
    "esp32": {
        "substitutions": [
            ("CONFIG_EFUSE_MAX_BLK_LEN", "256"),
        ],
    },
    "esp32p4": {
        "tables": {
            "esp_efuse_table_v3.0.c": (
                "esp_loader_efuse_table.c",
                "ESP32P4_EFUSE_",
                300,
                REVISION_ANY,
            ),
            "esp_efuse_table.c": (
                "esp_loader_efuse_table_rev1.c",
                "ESP32P4_REV1_EFUSE_",
                0,
                299,
            ),
        },
    },
    "esp32h2": {
        "tables": {
            "esp_efuse_table_v0.0_v1.1.c": (None, None, 0, 101),
        },
        "extends": {"esp_efuse_table_v0.0_v1.1.c": "esp_efuse_table.c"},
    },
}


# The only readers that know the shape of a CHIP_SPECIFIC entry -- one per
# quirk. Each is consumed in a different phase (substitutions rewrite table
# text, overrides steer staging, extends steers metadata), so they cannot share
# a single call site, but the dict layout stops here: nothing below digs into
# CHIP_SPECIFIC itself.


def apply_chip_specific(chip: str, text: str) -> str:
    """Apply a chip's literal text substitutions (a no-op for most chips)."""
    for old, new in CHIP_SPECIFIC.get(chip, {}).get("substitutions", ()):
        text = text.replace(old, new)
    if chip == "esp32c2":
        text = split_esp32c2_key_block(text)
    return text


def split_esp32c2_key_block(text: str) -> str:
    """Rewrite ESP32-C2's BLOCK_KEY0 descriptors into whole + two named halves.

    C2 is the only chip whose read protection is finer than one bit per block:
    BLOCK_KEY0 holds either a 256-bit key or two independent 128-bit halves --
    low = Flash-Encryption, high = Secure-Boot -- each with its own read-disable
    bit (32 / 33). IDF spells the halves as purpose aliases (KEY0.FE_128BIT /
    KEY0.SB_128BIT) plus a whole-block duplicate (KEY0.FE_256BIT). Rename the
    halves to position names so the metadata can carry the two rd bits as
    separate single-bit fields, and drop the duplicate of KEY0.

    Renaming the tokens updates the static array, its exported pointer array, and
    (via declare_fields, which reads the header off this text) the extern too.
    The matching metadata rows are produced in field_info_entries.
    """
    text = text.replace("KEY0_FE_128BIT", "KEY0_LOW_128")
    text = text.replace("KEY0_SB_128BIT", "KEY0_HI_128")
    # Drop the KEY0_FE_256BIT descriptor and its exported pointer array -- an
    # exact duplicate of KEY0 that the split leaves redundant.
    return re.sub(
        r"\n(?:static )?const esp_loader_efuse_desc_t\*? \w*KEY0_FE_256BIT\[\]"
        r" = \{.*?\};\n",
        "",
        text,
        flags=re.S,
    )


def table_overrides(chip: str) -> dict[str, tuple]:
    """A chip's per-file (output, prefix, rev_min, rev_max) overrides."""
    return CHIP_SPECIFIC.get(chip, {}).get("tables", {})


def extends_map(chip: str) -> dict[str, str]:
    """A chip's {supplement: base} table links (empty for most chips)."""
    return CHIP_SPECIFIC.get(chip, {}).get("extends", {})


def default_prefix(chip: str, filename: str) -> str:
    """Symbol prefix for a table nothing in CHIP_SPECIFIC renames.

    The base table gets the plain <CHIP>_EFUSE_; a revision-suffixed one folds
    that suffix in, so esp_efuse_table_v0.0_v1.1.c becomes
    ESP32H2_V0_0_V1_1_EFUSE_ and cannot collide with the base table's symbols.
    """
    variant = Path(filename).stem[len("esp_efuse_table") :].strip("_")
    parts = [chip.upper()]
    if variant:
        parts.append(re.sub(r"[^0-9A-Za-z]+", "_", variant).upper())
    return "_".join(parts) + "_EFUSE_"


def chip_enum(chip: str) -> str:
    """target_chip_t constant for a chip directory name (esp32c3 -> ESP32C3_CHIP)."""
    return f"{chip.upper()}_CHIP"


def our_output(idf_name: str) -> str:
    """Our filename for an IDF table (esp_efuse_table.c -> esp_loader_efuse_table.c).

    The whole vendored set is renamed off IDF's own basename so nothing collides
    when the flasher is built as an ESP-IDF component beside the real efuse
    component -- the same reason the descriptor type is esp_loader_efuse_desc_t.
    """
    return idf_name.replace("esp_efuse_table", "esp_loader_efuse_table", 1)


def resolve_tables(
    chip: str, component_dir: Path
) -> list[tuple[str, str, str, int, int]]:
    """(idf_source, our_output, symbol_prefix, rev_min, rev_max) per IDF table.

    Discovered rather than enumerated, so a table ESP-IDF adds is picked up
    instead of silently skipped. What it should be *called* is still a decision,
    which is why CHIP_SPECIFIC can override the name and prefix per file -- but a
    table nobody has thought about yet still turns up, and shows in CI as a diff
    rather than as nothing at all.

    Sorted by output name so mainstream tables come before revision variants and
    the generated files are stable across runs.
    """
    overrides = table_overrides(chip)

    resolved = []
    for source in component_dir.glob("esp_efuse_table*.c"):
        out_name, prefix, rev_min, rev_max = overrides.get(
            source.name, (None, None, 0, REVISION_ANY)
        )
        resolved.append(
            (
                source.name,
                out_name or our_output(source.name),
                prefix or default_prefix(chip, source.name),
                rev_min,
                rev_max,
            )
        )

    return sorted(resolved, key=lambda entry: entry[1])


# ===========================================================================
# 1 -- clean
#
# The script only ever writes files, so without this a table that gets renamed
# or dropped survives as an orphan: still compiled, still shipped, no longer
# regenerated. Both trees exist solely to hold what the sections below produce,
# so they go wholesale rather than by filename pattern -- which also clears
# output from an older version of this script that used other names.
# ===========================================================================


def clean_generated(include_root: Path, source_root: Path) -> None:
    """Delete every previously generated table and header."""
    for root in (include_root, source_root):
        shutil.rmtree(root, ignore_errors=True)


# ===========================================================================
# 2 -- staging
#
# Copy verbatim, rename only, into a scratch directory. Applying the rename
# once, up front, is what lets the later sections refer to a table by the name
# it will have in this repo rather than repeating the IDF-to-us mapping.
# ===========================================================================


class StagedTable(NamedTuple):
    chip: str
    idf_name: str
    prefix: str
    rev_min: int
    rev_max: int
    source: Path  # staged esp_efuse_table*.c, already under our filename
    table: Path  # staged esp_efuse_table*.csv, likewise


def csv_for(name: str) -> str:
    """CSV filename beside a table source (only the trailing .c is replaced)."""
    return str(Path(name).with_suffix(".csv"))


def stage_tables(idf_path: Path, staging_root: Path) -> list[StagedTable]:
    """Copy every table we track out of ESP-IDF, under our own filenames."""
    staged = []

    for chip in SUPPORTED_CHIPS:
        component_dir = idf_path / "components" / "efuse" / chip
        staging_dir = staging_root / chip
        staging_dir.mkdir(parents=True, exist_ok=True)

        for idf_name, out_name, prefix, rev_min, rev_max in resolve_tables(
            chip, component_dir
        ):
            idf_csv = component_dir / csv_for(idf_name)
            if not idf_csv.is_file():
                raise SystemExit(
                    f"{chip}/{idf_name} has no {idf_csv.name} beside it. The names and "
                    "protection bits come from the CSV, so the table cannot be generated."
                )

            source = staging_dir / out_name
            table = staging_dir / csv_for(out_name)
            shutil.copyfile(component_dir / idf_name, source)
            shutil.copyfile(idf_csv, table)
            staged.append(
                StagedTable(chip, idf_name, prefix, rev_min, rev_max, source, table)
            )

    return staged


# ===========================================================================
# 3 -- table sources
#
# Rewrite each staged .c into the repo: drop IDF's includes and sdkconfig
# guards, move the symbols into our namespace, and declare the result in a
# per-chip header so we need none of IDF's.
# ===========================================================================


def strip_includes(text: str) -> str:
    """Drop every IDF include; the ones we need are re-added as a preamble."""
    lines = [
        line for line in text.splitlines() if not line.strip().startswith("#include")
    ]
    return "\n".join(lines) + "\n"


def insert_preamble(text: str, preamble: str) -> str:
    """Insert our own directives just below the SPDX license comment."""
    marker = "*/\n"
    index = text.find(marker) + len(marker)
    return text[:index] + preamble + text[index:]


def replace_block_constants(text: str) -> str:
    """Replace EFUSE_BLK<n> enum constants with raw block numbers."""
    return re.sub(r"\bEFUSE_BLK([0-9]+)\b", r"\1", text)


def strip_config_guards(text: str) -> str:
    """Drop sdkconfig-driven guards, which only ever wrapped stripped includes."""
    output: list[str] = []
    depth = 0

    for line in text.splitlines():
        stripped = line.strip()
        if re.match(r"#if\s+!?\(?CONFIG_", stripped):
            depth += 1
            continue
        if depth and stripped.startswith(("#else", "#elif", "#endif")):
            if stripped.startswith("#endif"):
                depth -= 1
            continue
        output.append(line)

    return "\n".join(output) + "\n"


def normalize_copyright(text: str) -> str:
    """Date the generated file to its own creation, not the IDF source's history.

    The vendored table is a new file in this repository, so it carries a single
    year. Provenance stays in the preamble comment, and the copyright holder is
    the same either way.
    """
    return re.sub(
        r"SPDX-FileCopyrightText: \d{4}(-\d{4})? Espressif Systems",
        f"SPDX-FileCopyrightText: {COPYRIGHT_YEAR} Espressif Systems",
        text,
        count=1,
    )


def transform_source(chip: str, prefix: str, text: str) -> str:
    text = strip_includes(text)
    text = strip_config_guards(text)
    text = normalize_copyright(text)
    text = re.sub(r"\bESP_EFUSE_", prefix, text)
    text = re.sub(r"\besp_efuse_desc_t\b", "esp_loader_efuse_desc_t", text)
    text = apply_chip_specific(chip, text)
    text = replace_block_constants(text)
    text = re.sub(r"\n{3,}", "\n\n", text)  # close the gaps the strips left
    return insert_preamble(
        text,
        "\n/* Vendored from ESP-IDF components/efuse — adapted and regenerated"
        " by tools/copy_efuse_headers.py. */\n"
        f'#include "esp_loader_efuse.h"\n#include "efuse/{chip}/esp_loader_efuse_table.h"\n',
    )


FIELD_RE = re.compile(r"^const esp_loader_efuse_desc_t\* (\w+)\[\] = \{", re.M)

HEADER_TEMPLATE = """\
/*
 * SPDX-FileCopyrightText: {year} Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>

#include "esp_loader_efuse.h"

#ifdef __cplusplus
extern "C" {{
#endif

{declarations}

#ifdef __cplusplus
}}
#endif
"""


def declare_fields(name: str, text: str) -> str:
    """Declare everything one transformed table defines, so we need no IDF header."""
    fields = "\n".join(
        f"extern const esp_loader_efuse_desc_t* {field}[];"
        for field in FIELD_RE.findall(text)
    )
    return f"/* {name} */\n{fields}"


def group_by_chip(staged: list[StagedTable]) -> dict[str, list[StagedTable]]:
    grouped: dict[str, list[StagedTable]] = defaultdict(list)
    for entry in staged:
        grouped[entry.chip].append(entry)
    return grouped


def generate_tables(
    staged: list[StagedTable], source_root: Path
) -> tuple[dict[str, list[str]], dict[str, list[str]]]:
    """Write each namespaced table and return (header declarations, set names).

    A table listed under "extends" contributes its descriptors but no metadata of
    its own; its fields are appended to the table it extends.
    """
    declarations: dict[str, list[str]] = {}
    field_sets: dict[str, list[str]] = {}

    for chip, tables in group_by_chip(staged).items():
        source_dir = source_root / chip
        source_dir.mkdir(parents=True, exist_ok=True)

        extends = extends_map(chip)
        declarations[chip], field_sets[chip] = [], []

        for entry in tables:
            text = transform_source(chip, entry.prefix, entry.source.read_text())
            # Read the field declarations off the table alone: the metadata
            # appended below is a different type and must not be picked up.
            chip_declarations = [declare_fields(entry.source.name, text)]

            if entry.idf_name not in extends:
                supplements = [
                    t for t in tables if extends.get(t.idf_name) == entry.idf_name
                ]
                metadata, set_declarations, names = render_metadata(entry, supplements)
                text += metadata
                chip_declarations.extend(set_declarations)
                field_sets[chip].extend(names)

            (source_dir / entry.source.name).write_text(text, encoding="utf-8")
            declarations[chip].append("\n\n".join(chip_declarations))

    return declarations, field_sets


# ===========================================================================
# 4 -- field metadata
#
# The tables above carry geometry but no names, and nothing ties a field to the
# fuse that protects it. Anything that addresses a field the way a user names it
# -- looking one up, reporting whether it is still writable, listing the lot --
# needs both, so we read them out of the CSV staged in (2) and render an
# esp_loader_efuse_field_info_t table onto the end of each table source.
# ===========================================================================

BIT_NONE = "ESP_LOADER_EFUSE_BIT_NONE"


def protected_name(name: str, kind: str) -> str | None:
    """The field a WR_DIS/RD_DIS row protects, or None if it is not such a row.

    Nearly every table spells these WR_DIS.<field>, but a supplementary table
    that does not itself define the WR_DIS root must use an underscore instead:
    IDF's efuse_table_gen.py rejects a dotted name whose root is missing from
    the same block. ESP32-H2's v0.0_v1.1 table is the one case today. Accept
    both, or its ECDSA_FORCE_USE_HARDWARE_K silently loses its write protection
    and the protection fuse gets listed as if it were a field.

    The bare aggregates WR_DIS and RD_DIS have no separator, so they do not
    match and keep their own entries, which is what a caller wants.
    """
    match = re.fullmatch(rf"{kind}_DIS[._](.+)", name)
    return match.group(1) if match else None


def is_protection_row(name: str) -> bool:
    return any(protected_name(name, kind) is not None for kind in ("WR", "RD"))


class CsvRow(NamedTuple):
    name: str
    block: int
    bit_start: int
    bit_count: int
    alt_names: list[str]


def parse_csv(path: Path) -> list[CsvRow]:
    """Parse an IDF eFuse CSV into field rows, joining continuation rows.

    A row with an empty name column continues the previous field with another
    bit range rather than starting a new one; IDF's efuse_table_gen.py resolves
    it the same way, which is how a field ends up with several ranges.
    """
    rows: list[CsvRow] = []
    last_name: str | None = None

    for line in path.read_text().splitlines():
        columns = [column.strip() for column in line.split("#")[0].split(",")]
        if len(columns) < 5 or not columns[1].startswith("EFUSE_BLK"):
            continue
        try:
            bit_start, bit_count = int(columns[2], 0), int(columns[3], 0)
        except ValueError:
            continue  # a non-numeric geometry cell means prose, not a field

        name = columns[0] or last_name
        if name is None:
            continue
        last_name = name

        alt = re.match(r"\[(.*?)\]", columns[4])
        rows.append(
            CsvRow(
                name=name,
                block=int(columns[1][len("EFUSE_BLK") :]),
                bit_start=bit_start,
                bit_count=bit_count,
                alt_names=alt.group(1).split() if alt else [],
            )
        )

    return rows


def protection_map(rows: list[CsvRow], kind: str) -> dict[str, int]:
    """Every field name a WR_DIS/RD_DIS row can stand for, mapped to its bit.

    IDF names these rows after the field they protect, but not always by that
    field's current name. Three cases, all covered by registering more than one
    key per row so the later lookup is a plain hit on the field's real name:

      * whole-block fields are referred to by their alternative name -- the row
        is WR_DIS.BLOCK_KEY0 while the field is KEY0 [BLOCK_KEY0];
      * a field merged from two halves keeps the halves' rows, so the row is
        WR_DIS.WAFER_VERSION_MAJOR_LO while the field is WAFER_VERSION_MAJOR;
      * sub-fields are dotted, and become underscores as C symbols.

    Both halves of a merged field always name the same bit, and no key is ever
    claimed twice with different bits, so a conflict means IDF changed something
    this mapping assumes. Refuse to guess: these bits gate irreversible burns.
    """
    bits: dict[str, int] = {}

    def register(key: str, bit: int) -> None:
        key = key.replace(".", "_")
        if bits.setdefault(key, bit) != bit:
            raise SystemExit(
                f"{kind}_DIS.{key} resolves to bit {bits[key]} and bit {bit}. "
                "ESP-IDF changed how protection rows are named; resolve by hand."
            )

    for row in rows:
        target = protected_name(row.name, kind)
        if target is None:
            continue

        names = [target]
        for alt in row.alt_names:
            alt_target = protected_name(alt, kind)
            if alt_target is not None:
                names.append(alt_target)

        for name in names:
            register(name, row.bit_start)
        register(re.sub(r"_(H|HI|LO)$", "", names[0]), row.bit_start)

    return bits


class FieldInfo(NamedTuple):
    name: str
    symbol: str
    wr_dis: str
    rd_dis: str


def field_info_entries(rows: list[CsvRow], prefix: str, chip: str) -> list[FieldInfo]:
    """One metadata entry per real field, with its protection bits resolved.

    WR_DIS.x / RD_DIS.x rows get no entry of their own: their whole content is
    the bit they contribute to the field they protect, which is attached here.
    The aggregate WR_DIS and RD_DIS fields are undotted and so do get entries;
    they are the ones a caller reads to evaluate every other field's bits.
    """
    wr_dis = protection_map(rows, "WR")
    rd_dis = protection_map(rows, "RD")

    entries: list[FieldInfo] = []
    seen: set[str] = set()

    for row in rows:
        if is_protection_row(row.name) or row.name in seen:
            continue  # protection rows are consumed above; merged fields are one symbol
        seen.add(row.name)
        key = row.name.replace(".", "_")
        entries.append(
            FieldInfo(
                name=row.name,
                symbol=prefix + key,
                wr_dis=str(wr_dis.get(key, BIT_NONE)),
                rd_dis=str(rd_dis.get(key, BIT_NONE)),
            )
        )

    if chip == "esp32c2":
        entries = split_esp32c2_key_metadata(entries, prefix, wr_dis, rd_dis)

    return entries


def split_esp32c2_key_metadata(
    entries: list[FieldInfo], prefix: str, wr_dis: dict, rd_dis: dict
) -> list[FieldInfo]:
    """Match split_esp32c2_key_block on the metadata side.

    IDF's KEY0.FE_256BIT / FE_128BIT / SB_128BIT aliases resolve to no protection
    bit (their bits are named after KEY0, not the aliases), so drop them. KEY0's
    read protection spans two bits that a single rd_dis_bit cannot both hold, so
    the whole KEY0 keeps the low/base bit (32) -- its high half is exposed via
    KEY0_HI_128 -- and each renamed half carries its own bit (low 32, high 33).
    All three bits come from the CSV's RD_DIS.KEY0[.LOW/.HI] rows, not literals.
    """
    wr = str(wr_dis.get("KEY0", BIT_NONE))
    halves = [
        FieldInfo(
            "KEY0_LOW_128",
            prefix + "KEY0_LOW_128",
            wr,
            str(rd_dis.get("KEY0_LOW", BIT_NONE)),
        ),
        FieldInfo(
            "KEY0_HI_128",
            prefix + "KEY0_HI_128",
            wr,
            str(rd_dis.get("KEY0_HI", BIT_NONE)),
        ),
    ]
    result: list[FieldInfo] = []
    for entry in entries:
        if entry.name.startswith("KEY0."):
            continue  # FE_256BIT/FE_128BIT/SB_128BIT folded into KEY0 + the halves
        result.append(entry)
        if entry.name == "KEY0":
            result.extend(halves)
    return result


METADATA_BLOCK_TEMPLATE = """

/* Field metadata, generated from {sources} alongside the table above: the names
 * and protection bits the descriptors do not carry.
 *
 * The array is private; callers reach it through the field sets below, which is
 * what lets a revision that lacks the trailing fields be served by a shorter
 * view of this same array instead of a second copy of it. Nothing outside this
 * file may assume the whole array applies to every revision. */
static const esp_loader_efuse_field_info_t field_info[] = {{
{rows}
}};

{sets}
"""

FIELD_SET_TEMPLATE = """\
/* revisions {range} */
const esp_loader_efuse_field_set_t {name} = {{
    .chip = {chip}, .fields = field_info, .count = {count},
    .rev_min = {rev_min}, .rev_max = {rev_max},
}};"""


def revision_literal(revision: int) -> str:
    return "ESP_LOADER_CHIP_REV_ANY" if revision == REVISION_ANY else str(revision)


def count_expr(count: int, total: int) -> str:
    """How a set states its field count.

    A set spanning the whole array lets the compiler derive it, so the number
    cannot drift from the array it describes. Only a shorter prefix view -- a
    revision that stops before the array's revision-gated tail -- needs an
    explicit literal, and that one is the sole place the count is a bare number.
    """
    if count == total:
        return "sizeof(field_info) / sizeof(field_info[0])"
    return str(count)


def describe_range(rev_min: int, rev_max: int) -> str:
    """v-notation for a major*100+minor range, for a generated comment."""

    def rev(value: int) -> str:
        return f"v{value // 100}.{value % 100}"

    if rev_min == 0 and rev_max == REVISION_ANY:
        return "all"
    if rev_max == REVISION_ANY:
        return f"{rev(rev_min)} and later"
    if rev_min == 0:
        return f"{rev(rev_max)} and earlier"
    return f"{rev(rev_min)}-{rev(rev_max)}"


def render_rows(entries: list[FieldInfo], gated_from: int, gated_range: str) -> str:
    """The array body, columns aligned, with the revision-gated tail marked."""
    name_width = max(len(entry.name) for entry in entries) + 3  # quotes and comma
    symbol_width = max(len(entry.symbol) for entry in entries) + 1
    wr_width = max(len(entry.wr_dis) for entry in entries) + 1

    lines = []
    for index, entry in enumerate(entries):
        if index == gated_from:
            lines.append(f"    /* revisions {gated_range} only */")
        quoted = f'"{entry.name}",'
        lines.append(
            f"    {{ {quoted:<{name_width}} "
            f"{entry.symbol + ',':<{symbol_width}} "
            f"{entry.wr_dis + ',':<{wr_width}} {entry.rd_dis} }},"
        )
    return "\n".join(lines)


def render_metadata(
    base: StagedTable, supplements: list[StagedTable]
) -> tuple[str, list[str], list[str]]:
    """(block to append to the table source, header declarations, set names).

    A supplement's fields are appended after the base ones and exposed as a
    second, longer set over the same array. That only works while the base stays
    a prefix of the result, so a supplement that redefines a base field is
    rejected rather than silently mis-sized.
    """
    if len(supplements) > 1:
        raise SystemExit(
            f"{base.chip}: {[s.idf_name for s in supplements]} all extend {base.idf_name}. "
            "Only one can be appended and still leave the base a prefix of the result."
        )

    entries = field_info_entries(parse_csv(base.table), base.prefix, base.chip)
    base_count = len(entries)
    sources = [base.table.name]
    gated_from, gated_range = -1, ""
    sets = []

    for supplement in supplements:
        extra = field_info_entries(
            parse_csv(supplement.table), supplement.prefix, supplement.chip
        )
        clashes = {e.name for e in extra} & {e.name for e in entries}
        if clashes:
            raise SystemExit(
                f"{base.chip}: {supplement.idf_name} redefines {sorted(clashes)} from "
                f"{base.idf_name}. That replaces the table rather than extending it; "
                "give it its own revision range instead of listing it under extends."
            )
        gated_from, gated_range = (
            len(entries),
            describe_range(supplement.rev_min, supplement.rev_max),
        )
        entries = entries + extra
        sources.append(supplement.table.name)
        # Narrower range first: the dispatcher takes the first match.
        sets.append((f"{supplement.prefix}FIELDS", len(entries), supplement))

    sets.append((f"{base.prefix}FIELDS", base_count, base))

    rendered = "\n\n".join(
        FIELD_SET_TEMPLATE.format(
            name=name,
            chip=chip_enum(base.chip),
            count=count_expr(count, len(entries)),
            rev_min=revision_literal(table.rev_min),
            rev_max=revision_literal(table.rev_max),
            range=describe_range(table.rev_min, table.rev_max),
        )
        for name, count, table in sets
    )

    block = METADATA_BLOCK_TEMPLATE.format(
        sources=" + ".join(sources),
        rows=render_rows(entries, gated_from, gated_range),
        sets=rendered,
    )
    declarations = [
        f"extern const esp_loader_efuse_field_set_t {name};" for name, _, _ in sets
    ]
    return block, declarations, [name for name, _, _ in sets]


DISPATCHER_TEMPLATE = """\
/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Generated by tools/copy_efuse_headers.py. Resolves a chip and its revision to
 * the field metadata that describes it.
 *
 * Its own translation unit on purpose: it names every chip's field set, so
 * linking it pulls them all in. Kept apart, an application that never calls
 * esp_loader_efuse_get_field_info() never pulls this object out of the archive
 * and pays nothing for the tables -- with or without --gc-sections.
 */

#include "esp_loader_efuse.h"

{includes}

/* Ordered: a set with a narrow revision range precedes the catch-all for the
 * same chip, and the first match wins. */
static const esp_loader_efuse_field_set_t *const field_sets[] = {{
{sets}
}};

esp_loader_error_t esp_loader_efuse_get_field_info(
    target_chip_t chip,
    uint16_t revision,
    const esp_loader_efuse_field_set_t **set)
{{
    for (size_t i = 0; i < sizeof(field_sets) / sizeof(field_sets[0]); i++) {{
        const esp_loader_efuse_field_set_t *candidate = field_sets[i];

        if (candidate->chip == chip &&
            revision >= candidate->rev_min && revision <= candidate->rev_max) {{
            *set = candidate;
            return ESP_LOADER_SUCCESS;
        }}
    }}

    return ESP_LOADER_ERROR_UNSUPPORTED_CHIP;
}}
"""


def generate_dispatcher(source_root: Path, field_sets: dict[str, list[str]]) -> None:
    includes = [
        f'#include "efuse/{chip}/esp_loader_efuse_table.h"'
        for chip in sorted(field_sets)
    ]
    entries = [
        f"    &{name}," for chip in sorted(field_sets) for name in field_sets[chip]
    ]

    (source_root / "esp_loader_efuse_field_info.c").write_text(
        DISPATCHER_TEMPLATE.format(
            includes="\n".join(includes), sets="\n".join(entries)
        ),
        encoding="utf-8",
    )


SOURCE_LIST_TEMPLATE = """\
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

# Generated by tools/copy_efuse_headers.py -- do not edit.
#
# The per-chip field tables, listed rather than globbed. A glob would either go
# stale until the next manual re-configure, or need CONFIGURE_DEPENDS, which
# CMake rejects in the script mode ESP-IDF uses to collect component
# requirements. Regenerating the tables rewrites this list, so it cannot drift.
#
# Only sets a variable: what to do with it is the includer's business, and
# CMAKE_CURRENT_LIST_DIR keeps the paths correct wherever it is included from.
set(EFUSE_TABLE_SOURCES
{sources}
)
"""


def generate_source_list(source_root: Path) -> None:
    """Write the CMake list of generated table sources."""
    sources = sorted(
        path.relative_to(source_root).as_posix()
        for path in source_root.glob("*/esp_loader_efuse_table*.c")
    )

    (source_root / "efuse_tables.cmake").write_text(
        SOURCE_LIST_TEMPLATE.format(
            sources="\n".join(
                f"    ${{CMAKE_CURRENT_LIST_DIR}}/{source}" for source in sources
            )
        ),
        encoding="utf-8",
    )


def generate_headers(include_root: Path, declarations: dict[str, list[str]]) -> None:
    """One header per chip, declaring everything its tables define."""
    for chip, chip_declarations in declarations.items():
        include_dir = include_root / chip
        include_dir.mkdir(parents=True, exist_ok=True)
        (include_dir / "esp_loader_efuse_table.h").write_text(
            HEADER_TEMPLATE.format(
                year=COPYRIGHT_YEAR,
                declarations="\n\n".join(chip_declarations),
            ),
            encoding="utf-8",
        )


# ===========================================================================
# 5 -- headers, and orchestration
# ===========================================================================


def main() -> None:
    repo_path = Path(__file__).resolve().parents[1]

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "idf_path",
        nargs="?",
        default="~/esp/esp-idf",
        help="Path to ESP-IDF checkout",
    )
    args = parser.parse_args()

    include_root = repo_path / "include" / "efuse"
    source_root = repo_path / "src" / "efuse"

    clean_generated(include_root, source_root)

    # The staged copies are inputs to generation, not something we ship, so they
    # live in a scratch dir that goes away even if a later phase raises.
    with tempfile.TemporaryDirectory(prefix="efuse-tables-") as staging_root:
        staged = stage_tables(Path(args.idf_path).resolve(), Path(staging_root))
        declarations, field_sets = generate_tables(staged, source_root)
        generate_dispatcher(source_root, field_sets)
        generate_source_list(source_root)
        generate_headers(include_root, declarations)


if __name__ == "__main__":
    main()
