#!/usr/bin/env python3
# This code is in the Public Domain (or CC0 licensed, at your option.)

"""
Compress firmware binaries for deflate flashing.

This script reads uncompressed .bin files and generates *_deflated.bin outputs
which can be flashed using the deflate protocol commands.

Usage:
    python compress_firmware.py [input_file] [output_file]

If no arguments provided, compresses all .bin files in the script directory
except *_deflated.bin.
"""

import zlib
import os
import sys


def compress_firmware(input_path, output_path):
    """Compress firmware with zlib for deflate flashing."""
    if not os.path.exists(input_path):
        print(f"Error: {input_path} not found", file=sys.stderr)
        return False

    with open(input_path, "rb") as f:
        data = f.read()

    # Compress with best compression level
    compressed = zlib.compress(data, 9)

    with open(output_path, "wb") as f:
        f.write(compressed)

    print(f"Input:       {input_path}")
    print(f"Input size:  {len(data)} bytes")
    print(f"Compressed:  {len(compressed)} bytes")
    print(f"Ratio:       {len(compressed) / len(data) * 100:.1f}%")
    print(f"Output:      {output_path}")

    return True


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))

    if len(sys.argv) >= 3:
        input_path = sys.argv[1]
        output_path = sys.argv[2]
        if not compress_firmware(input_path, output_path):
            sys.exit(1)
        return

    if len(sys.argv) == 2:
        input_path = sys.argv[1]
        if input_path.endswith("_deflated.bin"):
            print(f"Error: {input_path} already looks deflated", file=sys.stderr)
            sys.exit(1)
        output_path = f"{os.path.splitext(input_path)[0]}_deflated.bin"
        if not compress_firmware(input_path, output_path):
            sys.exit(1)
        return

    input_bins = sorted(
        name
        for name in os.listdir(script_dir)
        if name.endswith(".bin") and not name.endswith("_deflated.bin")
    )
    if not input_bins:
        print(f"Error: no input .bin files found in {script_dir}", file=sys.stderr)
        sys.exit(1)

    for name in input_bins:
        input_path = os.path.join(script_dir, name)
        output_name = f"{name[:-4]}_deflated.bin"
        output_path = os.path.join(script_dir, output_name)
        if not compress_firmware(input_path, output_path):
            sys.exit(1)
        print("")


if __name__ == "__main__":
    main()
