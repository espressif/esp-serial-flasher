# Target Firmware

Place the ESP firmware binaries in this directory (for example `app.bin`,
`bootloader.bin`, `partition-table.bin`).

## Generating Pre-compressed Binary

Use the provided script to compress all binaries in this directory:

```bash
python compress_firmware.py
```

This reads all `.bin` files (excluding `*_deflated.bin`) and generates
matching `*_deflated.bin` outputs using zlib compression.

You can also specify custom input/output paths:

```bash
python compress_firmware.py /path/to/input.bin /path/to/output_deflated.bin
```

If you pass a single input file, the output will be written to
`<input>_deflated.bin`:

```bash
python compress_firmware.py app.bin
```
