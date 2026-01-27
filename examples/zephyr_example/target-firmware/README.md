# Target Firmware

Put ESP32 firmware binaries here and define them in `images.csv`: one line per image as `file;offset` (semicolon, offset in hex without `0x`).

**Copy the block below into a new file and save it as `images.csv`**, then edit filenames and offsets for your target.

```
# image_file;offset (hex, no 0x prefix)
bootloader.bin;0
partition-table.bin;8000
app.bin;10000
```

The build embeds these images in the flasher; get the binaries from your ESP-IDF or CI build and keep the CSV in sync.
