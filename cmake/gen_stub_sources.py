# SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""
Generate per-chip stub source files and bundled providers from esp-flasher-stub release JSON files.

Usage:
    gen_stub_sources.py <version> <base_url> <repo_root> [override_path]

Each chip gets its own C file in <repo_root>/src/stubs/.
The bundled-provider source and public stub declaration header are also written/updated.

For each plugin in PLUGINS that a JSON file has, the chip file also defines a
<c_var>_<plugin> stub with the core stub segments followed by the plugin segments.
The core stub data is shared, not duplicated.

JSON format (espressif/esp-flasher-stub):
    { "entry": <uint32>, "text": "<base64>", "text_start": <uint32>,
      "data": "<base64>", "data_start": <uint32>, "bss_start": <uint32>,
      "plugin_start_offset": <uint32>, "plugin_start_slots": <uint32>,
      "plugins": { "<name>": { "text": "<base64>", "text_start": <uint32>,
                               "data": "<base64>", "data_start": <uint32>,
                               "entry": <uint32> } } }
"""

import base64
import json
import os
import struct
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

# Plugins: JSON "plugins" names, each with its own stub variants. They are not in
# the public header: the SDIO protocol declares its stubs and uploads them itself
# during esp_loader_connect().
PLUGINS = ("sdio",)

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
    """Return the stub entry point, its segments and its plugins.

    Segments are (name, addr, bytes) tuples. The plugins map a name from PLUGINS
    to its segments: the plugin text, its data and the start hook that writes
    the plugin entry into the first plugin_start[] slot of the base stub data.
    The base stub calls the start function at boot, which zeroes the plugin BSS
    and registers the plugin. Plugins missing from the JSON are left out.
    """
    stub = json.load(file_obj)
    segments = [("text", stub["text_start"], base64.b64decode(stub["text"]))]
    if stub.get("data"):
        segments.append(("data", stub["data_start"], base64.b64decode(stub["data"])))

    plugins = {}
    for name in PLUGINS:
        p = stub.get("plugins", {}).get(name)
        if not p:
            continue
        plugin = [(f"{name}_text", p["text_start"], base64.b64decode(p["text"]))]
        if p.get("data"):
            plugin.append(
                (f"{name}_data", p["data_start"], base64.b64decode(p["data"]))
            )
        start_addr = stub["data_start"] + stub["plugin_start_offset"]
        plugin.append((f"{name}_start_hook", start_addr, struct.pack("<I", p["entry"])))
        plugins[name] = plugin
    return stub["entry"], segments, plugins


_CHIP_FILE_TEMPLATE = Template("""\
$license_header
// auto-generated from esp-flasher-stub v$version — $json_name
// Source: https://github.com/espressif/esp-flasher-stub/releases/tag/v$version

#include "esp_loader.h"

${arrays}\
${stubs}""")

_STUB_TEMPLATE = Template("""\
const esp_stub_t $stub_var = {
    .header = {
        .entrypoint = $entrypoint,
    },
    .segments = ${stub_var}_segments,
    .segment_count = sizeof(${stub_var}_segments) / sizeof(${stub_var}_segments[0]),
};
""")


def _segment_table(c_var: str, table: str, segments) -> str:
    entries = "".join(
        "    {\n"
        f"        .addr = 0x{addr:08x},\n"
        f"        .size = sizeof({c_var}_{name}),\n"
        f"        .data = {c_var}_{name},\n"
        "    },\n"
        for name, addr, _data in segments
    )
    return f"static const esp_loader_bin_segment_t {c_var}_{table}[] = {{\n{entries}}};\n\n"


def write_chip_file(
    path: str,
    version: str,
    year: int,
    json_name: str,
    c_var: str,
    entry: int,
    segments,
    plugins,
) -> None:
    arrays = "".join(
        f"static const uint8_t {c_var}_{name}[] = {{\n{hex_array(data)}\n}};\n\n"
        for name, _addr, data in segments + [s for p in plugins.values() for s in p]
    )
    # The plugin hook overwrites part of the core data segment, so the plugin
    # segments must follow the core segments.
    stubs = [(c_var, "", segments)] + [
        (f"{c_var}_{name}", f"{name}_", segments + plugin_segments)
        for name, plugin_segments in plugins.items()
    ]
    content = _CHIP_FILE_TEMPLATE.substitute(
        license_header=LICENSE_HEADER.format(year=year),
        version=version,
        json_name=json_name,
        arrays=arrays,
        stubs="\n".join(
            _segment_table(c_var, f"{prefix}segments", stub_segments)
            + _STUB_TEMPLATE.substitute(stub_var=stub_var, entrypoint=f"0x{entry:08x}")
            for stub_var, prefix, stub_segments in stubs
        ),
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
                entry, segments, plugins = load_stub_json(fh)
        else:
            url = f"{base_url}/v{version}/{json_name}"
            with urllib.request.urlopen(url) as resp:
                entry, segments, plugins = load_stub_json(resp)
        out_path = os.path.join(stubs_src, f"{c_var}.c")
        write_chip_file(
            out_path, version, year, json_name, c_var, entry, segments, plugins
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
