# SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""
Generate per-chip stub source files and bundled providers from esp-flasher-stub release JSON files.

Usage:
    gen_stub_sources.py <version> <base_url> <repo_root> [override_path]

Each chip gets its own C file in <repo_root>/src/stubs/.
The bundled-provider source and public stub declaration header are also written/updated.

JSON format (espressif/esp-flasher-stub):
    { "entry": <uint32>, "text": "<base64>", "text_start": <uint32>,
      "data": "<base64>", "data_start": <uint32>, "bss_start": <uint32> }
"""

import base64
import json
import os
import sys
import urllib.request
from datetime import datetime, timezone
from string import Template


# ---------------------------------------------------------------------------
# Configuration for the bundled provider in include/esp_loader.h target order.
# Each name derives: enum = NAME.upper()+"_CHIP", json = name+".json", c_var = "esp_stub_"+name
# ---------------------------------------------------------------------------
def _chip(name: str):
    return (name.upper() + "_CHIP", f"{name}.json", f"esp_stub_{name}")


CHIPS = [
    _chip(n)
    for n in [
        "esp8266",
        "esp32",
        "esp32s2",
        "esp32c3",
        "esp32s3",
        "esp32c2",
        "esp32c5",
        "esp32h2",
        "esp32c6",
        "esp32p4",
        "esp32c61",
        "esp32s31",
        "esp32h21",
        "esp32h4",
    ]
]

# Extra stubs: own source file, selected by special logic in the bundled provider.
# Tuple: (json_filename, c_var_name)  — json names may differ from the standard pattern.
EXTRA_STUBS = [
    # ESP32-P4 ECO5-6 (chip revision v1.x / v2.x) — selected at runtime based on
    # eco_version from GET_SECURITY_INFO when loader->_target == ESP32P4_CHIP.
    ("esp32p4-rev1.json", "esp_stub_esp32p4rev1"),
]

LICENSE_HEADER = """\
// SPDX-FileCopyrightText: 2025-{year} Espressif Systems (Shanghai) CO LTD
// SPDX-License-Identifier: Apache-2.0 OR MIT
"""

PUBLIC_HEADER_LICENSE = """\
// SPDX-FileCopyrightText: 2026{year_suffix} Espressif Systems (Shanghai) CO LTD
// SPDX-License-Identifier: Apache-2.0
"""

BUNDLED_PROVIDER_LICENSE = """\
// SPDX-FileCopyrightText: 2026{year_suffix} Espressif Systems (Shanghai) CO LTD
// SPDX-License-Identifier: Apache-2.0 OR MIT
"""


def hex_array(data: bytes, indent: int = 4) -> str:
    """Format raw bytes as a C hex initialiser list."""
    pad = " " * indent
    per_line = 16
    lines = []
    for i in range(0, len(data), per_line):
        chunk = data[i : i + per_line]
        lines.append(pad + ", ".join(f"0x{b:02x}" for b in chunk))
    return ",\n".join(lines)


def load_stub_json(file_obj):
    stub = json.load(file_obj)
    entry = stub["entry"]
    text = base64.b64decode(stub["text"])
    text_start = stub["text_start"]
    try:
        data = base64.b64decode(stub["data"])
        data_start = stub["data_start"]
    except KeyError:
        data = b""
        data_start = 0
    return entry, text, text_start, data, data_start


_CHIP_FILE_TEMPLATE = Template("""\
$license_header
// auto-generated from esp-flasher-stub v$version — $json_name
// Source: https://github.com/espressif/esp-flasher-stub/releases/tag/v$version

#include "esp_loader.h"

static const uint8_t ${c_var}_text[] = {
$text_hex
};

${data_array}\
static const esp_loader_bin_segment_t ${c_var}_segments[] = {
    {
        .addr = $text_start,
        .size = sizeof(${c_var}_text),
        .data = ${c_var}_text,
    },
${data_segment}\
};

const esp_stub_t $c_var = {
    .header = {
        .entrypoint = $entrypoint,
    },
    .segments = ${c_var}_segments,
    .segment_count = sizeof(${c_var}_segments) / sizeof(${c_var}_segments[0]),
};
""")


def write_chip_file(
    path: str,
    version: str,
    year: int,
    json_name: str,
    c_var: str,
    entry: int,
    text: bytes,
    text_start: int,
    data: bytes,
    data_start: int,
) -> None:
    data_array = (
        f"static const uint8_t {c_var}_data[] = {{\n{hex_array(data)}\n}};\n\n"
        if data
        else ""
    )
    data_segment = (
        "    {\n"
        f"        .addr = 0x{data_start:08x},\n"
        f"        .size = sizeof({c_var}_data),\n"
        f"        .data = {c_var}_data,\n"
        "    },\n"
        if data
        else ""
    )
    content = _CHIP_FILE_TEMPLATE.substitute(
        license_header=LICENSE_HEADER.format(year=year),
        version=version,
        json_name=json_name,
        c_var=c_var,
        text_hex=hex_array(text) if text else "",
        data_array=data_array,
        data_segment=data_segment,
        entrypoint=f"0x{entry:08x}",
        text_start=f"0x{text_start:08x}",
    )
    with open(path, "w", newline="\n") as f:
        f.write(content)


def write_bundled_provider_file(path: str, version: str, year: int, chips) -> None:
    lines = [
        BUNDLED_PROVIDER_LICENSE.format(year_suffix=f"-{year}" if year > 2026 else ""),
        f"// auto-generated from esp-flasher-stub v{version}",
        f"// Source: https://github.com/espressif/esp-flasher-stub/releases/tag/v{version}",
        "",
        '#include "esp_loader_stubs.h"',
        '#include "esp_targets.h"',
        "",
        "static const esp_stub_t *bundled_provider(esp_loader_t *loader, target_chip_t chip, void *ctx)",
        "{",
        "    (void)ctx;",
        "",
        "    switch (chip) {",
    ]

    for enum_name, _json, c_var in chips:
        if enum_name == "ESP32P4_CHIP":
            lines.extend(
                [
                    f"    case {enum_name}: {{",
                    "        esp_loader_target_security_info_t info;",
                    "        bool got_info = (esp_loader_get_security_info(loader, &info) == ESP_LOADER_SUCCESS);",
                    "        return (got_info && info.eco_version >= ESP32P4_ECO_REV3_MIN)",
                    f"               ? &{c_var} : &esp_stub_esp32p4rev1;",
                    "    }",
                ]
            )
        else:
            lines.extend([f"    case {enum_name}:", f"        return &{c_var};"])
    lines.extend(
        [
            "    default:",
            "        return NULL;",
            "    }",
            "}",
            "",
            "esp_loader_error_t esp_loader_connect_with_stub(esp_loader_t *loader,",
            "        esp_loader_connect_args_t *connect_args)",
            "{",
            "    return esp_loader_connect_with_stub_provider(loader, connect_args, bundled_provider, NULL);",
            "}",
            "",
        ]
    )

    with open(path, "w", newline="\n") as f:
        f.write("\n".join(lines))


def write_header_file(path: str, version: str, year: int, chips, extra_stubs) -> None:
    declarations = [
        f"extern const esp_stub_t {c_var};" for _enum, _json, c_var in chips
    ]
    declarations.extend(
        f"extern const esp_stub_t {c_var};" for _json, c_var in extra_stubs
    )
    content = (
        PUBLIC_HEADER_LICENSE.format(year_suffix=f"-{year}" if year > 2026 else "")
        + f"""\
// auto-generated from esp-flasher-stub v{version}
// Source: https://github.com/espressif/esp-flasher-stub/releases/tag/v{version}

#pragma once

#include "esp_loader.h"

#ifdef __cplusplus
extern "C" {{
#endif

/** @brief Bundled per-chip flasher stubs. */
{chr(10).join(declarations)}

#ifdef __cplusplus
}}
#endif
"""
    )
    with open(path, "w", newline="\n") as f:
        f.write(content)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
if __name__ == "__main__":
    if len(sys.argv) < 4:
        print(f"Usage: {sys.argv[0]} <version> <base_url> <repo_root> [override_path]")
        sys.exit(1)

    version = sys.argv[1]
    base_url = sys.argv[2]
    repo_root = sys.argv[3]
    override_path = sys.argv[4] if len(sys.argv) >= 5 else None

    year = datetime.now(timezone.utc).year
    stubs_src = os.path.join(repo_root, "src", "stubs")
    public_inc = os.path.join(repo_root, "include")

    os.makedirs(stubs_src, exist_ok=True)

    def fetch_and_write(json_name, c_var):
        print(f"  Processing {json_name} → {c_var}.c ...")
        if override_path:
            json_path = os.path.join(override_path, json_name)
            with open(json_path) as fh:
                entry, text, text_start, data, data_start = load_stub_json(fh)
        else:
            url = f"{base_url}/v{version}/{json_name}"
            with urllib.request.urlopen(url) as resp:
                entry, text, text_start, data, data_start = load_stub_json(resp)
        out_path = os.path.join(stubs_src, f"{c_var}.c")
        write_chip_file(
            out_path,
            version,
            year,
            json_name,
            c_var,
            entry,
            text,
            text_start,
            data,
            data_start,
        )

    for _enum, json_name, c_var in CHIPS:
        fetch_and_write(json_name, c_var)

    for json_name, c_var in EXTRA_STUBS:
        fetch_and_write(json_name, c_var)

    provider_path = os.path.join(stubs_src, "esp_stub_bundled.c")
    print(f"  Writing {provider_path} ...")
    write_bundled_provider_file(provider_path, version, year, CHIPS)

    header_path = os.path.join(public_inc, "esp_loader_stubs.h")
    print(f"  Writing {header_path} ...")
    write_header_file(header_path, version, year, CHIPS, EXTRA_STUBS)

    total = len(CHIPS) + len(EXTRA_STUBS)
    print(
        f"Done — {total} stub files generated from v{version} ({len(CHIPS)} bundled, {len(EXTRA_STUBS)} extra)."
    )
