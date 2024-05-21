import base64
import json
import os
import sys
import urllib.request

"""
Generate flasher stubs from https://github.com/espressif/esptool/tree/master/esptool/targets/stub_flasher
"""

# Paths
esptool_version = sys.argv[1]
root_path = sys.argv[2]
priv_inc_path = os.path.join(root_path, "private_include")
src_path = os.path.join(root_path, "src")
hfile_path = os.path.join(priv_inc_path, "esp_stubs.h")
cfile_path = os.path.join(src_path, "esp_stubs.c")

# Matches order of target_chip_t enumeration
files_to_download = [
    "stub_flasher_8266.json",  # ESP8266_CHIP
    "stub_flasher_32.json",  # ESP32_CHIP
    "stub_flasher_32s2.json",  # ESP32S2_CHIP
    "stub_flasher_32c3.json",  # ESP32C3_CHIP
    "stub_flasher_32s3.json",  # ESP32S3_CHIP
    "stub_flasher_32c2.json",  # ESP32C2_CHIP
    None,  # ESP32_RESERVED0_CHIP
    "stub_flasher_32h2.json",  # ESP32H2_CHIP
    "stub_flasher_32c6.json",  # ESP32C6_CHIP
]

# .h and .c file to generate
hfile = open(hfile_path, "w")
cfile = open(cfile_path, "w")
codegen_notice = "// auto-generated stubs from esptool v" + esptool_version + "\n"

# Write .h file
hfile.write(codegen_notice)
hfile.write(
    "\n"
    "#pragma once\n"
    "\n"
    "#include <stdint.h>\n"
    "#include <stdbool.h>\n"
    '#include "esp_loader.h"\n'
    "\n"
    "#ifdef __cplusplus\n"
    'extern "C" {\n'
    "#endif\n"
    "\n"
    "extern bool esp_stub_running;\n"
    "\n"
    "#if (defined SERIAL_FLASHER_INTERFACE_UART) || (defined SERIAL_FLASHER_INTERFACE_USB)\n"
    "\n"
    "typedef struct {\n"
    "    esp_loader_bin_header_t header;\n"
    "    esp_loader_bin_segment_t segments[2];\n"
    "} esp_stub_t;\n"
    "\n"
    "extern const esp_stub_t esp_stub[ESP_MAX_CHIP];\n"
    "\n"
    "#endif\n"
    "\n"
    "#ifdef __cplusplus\n"
    "}\n"
    "#endif\n"
)

# Write .c file
cfile.write(codegen_notice)
cfile.write(
    "\n"
    '#include "esp_stubs.h"\n'
    "\n"
    "bool esp_stub_running = false;\n"
    "\n"
    "#if (defined SERIAL_FLASHER_INTERFACE_UART) || (defined SERIAL_FLASHER_INTERFACE_USB)\n"
    "\n"
    "#if __STDC_VERSION__ >= 201112L\n"
    '_Static_assert(ESP8266_CHIP == 0, "Stub order matches target_chip_t enumeration");\n'
    '_Static_assert(ESP32_CHIP == 1, "Stub order matches target_chip_t enumeration");\n'
    '_Static_assert(ESP32S2_CHIP == 2, "Stub order matches target_chip_t enumeration");\n'
    '_Static_assert(ESP32C3_CHIP == 3, "Stub order matches target_chip_t enumeration");\n'
    '_Static_assert(ESP32S3_CHIP == 4, "Stub order matches target_chip_t enumeration");\n'
    '_Static_assert(ESP32C2_CHIP == 5, "Stub order matches target_chip_t enumeration");\n'
    '_Static_assert(ESP32_RESERVED0_CHIP == 6, "Stub order matches target_chip_t enumeration");\n'
    '_Static_assert(ESP32H2_CHIP == 7, "Stub order matches target_chip_t enumeration");\n'
    '_Static_assert(ESP32C6_CHIP == 8, "Stub order matches target_chip_t enumeration");\n'
    "_Static_assert(ESP_MAX_CHIP == "
    + str(len(files_to_download))
    + ', "Stub order matches target_chip_t enumeration");\n'
    "#endif\n"
    "\n"
    "const esp_stub_t esp_stub[ESP_MAX_CHIP] = {\n"
    "\n"
)

for file_to_download in files_to_download:
    if file_to_download is None:
        cfile.write("    // placeholder\n" "    {},\n" "\n")
    else:
        with urllib.request.urlopen(
            "https://raw.githubusercontent.com/espressif/esptool/v"
            + esptool_version
            + "/esptool/targets/stub_flasher/"
            + file_to_download
        ) as url:
            # Read stub_flasher*.json
            stub = json.load(url)
            entry = stub["entry"]
            text = base64.b64decode(stub["text"])
            text_start = stub["text_start"]
            try:
                data = base64.b64decode(stub["data"])
                data_start = stub["data_start"]
            except KeyError:
                data = None
                data_start = None
            bss_start = stub["bss_start"]

            # According to esptool source those data could potentially be None
            text_str = ",".join([hex(b) for b in text])
            data_str = "" if data is None else ",".join([hex(b) for b in data])
            data_size_str = str(0) if data is None else str(len(data))
            data_start_str = str(0) if data_start is None else str(data_start)

            cfile.write(
                "    // " + file_to_download + "\n"
                "    {\n"
                "        .header = {\n"
                "            .entrypoint = " + str(entry) + ",\n"
                "        },\n"
                "        .segments = {\n"
                "            {\n"
                "                .addr = " + str(text_start) + ",\n"
                "                .size = " + str(len(text)) + ",\n"
                "                .data = (uint8_t[]){" + text_str + "},\n"
                "            },\n"
                "            {\n"
                "                .addr = " + data_start_str + ",\n"
                "                .size = " + data_size_str + ",\n"
                "                .data = (uint8_t[]){" + data_str + "},\n"
                "            },\n"
                "        },\n"
                "    },\n"
                "\n"
            )

cfile.write("};\n" "\n" "#endif\n")
