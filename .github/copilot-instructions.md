# Copilot Instructions for esp-serial-flasher

## Repository Overview

`esp-serial-flasher` is a portable C library for flashing or loading applications to RAM of Espressif SoCs from other host microcontrollers. The library supports multiple host platforms (STM32, ESP32, Zephyr OS, Pi Pico, Linux) and target chips (ESP32 family, ESP8266) using various communication interfaces (UART, SPI, USB CDC ACM, SDIO).

**Repository size**: ~500 files, primarily C source code with CMake build system
**Languages**: C (primary), Python (testing), CMake
**Target runtime**: Embedded systems, cross-platform compatibility
**Primary framework**: ESP-IDF component + standalone CMake library

## Build Instructions

### Prerequisites and Environment Setup

**Always install pre-commit hooks before making changes:**

```bash
pip install pre-commit
pre-commit install
pre-commit install -t commit-msg
```

### Basic Library Build (Standalone)

For basic library compilation without platform-specific ports:

```bash
mkdir build && cd build
cmake .. -DPORT=USER_DEFINED
make
```

**Time required**: ~30 seconds
**Postcondition**: Creates `libflasher.a` static library
**Note**: CMake warnings about missing project() call are expected and harmless

### ESP-IDF Component Build

**Requires ESP-IDF environment setup (idf.py must be in PATH)**

```bash
# From any example directory (e.g., examples/esp32_example)
idf.py build
```

**Time required**: 2-5 minutes depending on configuration
**Configuration options**: Set via `idf.py menuconfig` or `-D` flags

### STM32 Build

No pre-built example project is provided for STM32. See [examples/stm32_example/README.md](../examples/stm32_example/README.md) for a guide on generating a CubeMX project for your MCU and integrating esp-serial-flasher.

### Zephyr Build

**Requires Zephyr SDK and west workspace setup**

```bash
export ZEPHYR_SDK_INSTALL_DIR=/path/to/zephyr-sdk
export ZEPHYR_TOOLCHAIN_VARIANT="zephyr"
cd zephyr_workspace/zephyr
west build -p -b esp32_devkitc_wroom/esp32/procpu \
    path/to/examples/zephyr_example \
    -DZEPHYR_EXTRA_MODULES=path/to/esp-serial-flasher
```

**Time required**: 5-15 minutes
**Prerequisites**: Zephyr SDK v0.17.0+, west installed

### Raspberry Pi Pico Build

**Requires Pico SDK and ARM toolchain**

```bash
export PICO_SDK_PATH=/path/to/pico-sdk
cd examples/pi_pico_example
mkdir build && cd build
cmake ..
cmake --build .
```

**Time required**: 1-3 minutes
**Output**: `.uf2` file for flashing to Pico

## Testing Instructions

### Pre-commit Validation

**Always run before committing changes:**

```bash
pre-commit run --all-files
```

**Checks performed**: trailing whitespace, line endings, astyle formatting, ruff linting, conventional commit messages

### QEMU Testing

**Requires compiled QEMU with ESP32 support**

```bash
export QEMU_PATH=/path/to/qemu-system-xtensa
cd test
./run_qemu_test.sh
```

**Time required**: 2-5 minutes
**Prerequisites**: QEMU built with ESP32 machine support

### Hardware-in-the-Loop Testing

**Requires physical hardware setup**

```bash
# Install test dependencies
pip install -r test/requirements_test.txt

# Run tests for specific target
pytest --target=esp32 --port=/dev/ttyUSB0
pytest --target=esp32s3 --port=/dev/ttyUSB1
pytest --target=pi_pico --port=/dev/ttyACM1
pytest --target=zephyr --port=/dev/ttyUSB2
```

**Important**: Some ESP32-S3 tests must run separately:

```bash
# Run these separately, not together
pytest --target=esp32s3 --port=/dev/ttyUSB1 -k 'test_esp32_spi_load_ram_example'
pytest --target=esp32s3 --port=/dev/ttyUSB1 -k 'not test_esp32_spi_load_ram_example'
```

### Build All Examples (ESP-IDF)

**Requires ESP-IDF environment and idf-build-apps**

```bash
pip install -U idf-build-apps
python -m idf_build_apps build -v -p . \
    --recursive \
    --exclude ./examples/binaries \
    --config "sdkconfig.defaults*" \
    --build-dir "build_@w" \
    --check-warnings
```

**Time required**: 10-30 minutes depending on configuration

## Project Layout and Architecture

### Core Library Structure

```
src/                    # Core library implementation
├── esp_loader.c       # Main API implementation
├── protocol_uart.c   # UART communication protocol
├── protocol_spi.c    # SPI communication protocol
├── protocol_sdio.c   # SDIO communication protocol
├── esp_targets.c     # Target chip definitions
├── stubs/            # Per-chip flash stub binaries (auto-generated)
│   ├── esp_stub_esp32.c
│   ├── esp_stub_esp32c2.c
│   └── ...           # one file per supported chip + esp_stubs_table.c
└── md5_hash.c        # Optional MD5 verification

include/               # Public API headers
├── esp_loader.h      # Main API header
├── esp_loader_io.h   # I/O interface definitions
└── serial_io.h       # Serial I/O definitions

port/                  # Platform-specific implementations
├── esp32_port.c      # ESP32 UART implementation
├── esp32_spi_port.c  # ESP32 SPI implementation
├── esp32_usb_cdc_acm_port.c # ESP32 USB implementation
├── stm32_port.c      # STM32 HAL implementation
├── pi_pico_port.c    # Pi Pico SDK implementation
├── zephyr_port.c     # Zephyr RTOS implementation
└── linux_port.c      # Linux POSIX serial + optional libgpiod
```

### Configuration Files

- `Kconfig`: ESP-IDF component configuration options
- `idf_component.yml`: ESP-IDF component manifest
- `CMakeLists.txt`: Main build configuration
- `.pre-commit-config.yaml`: Code quality checks
- `astyle_config`: C code formatting rules
- `pytest.ini`: Test framework configuration

### CI/CD Pipelines

- **GitLab CI** (`.gitlab-ci.yml`): Primary CI with hardware testing
- **GitHub Actions** (`.github/workflows/`): Pre-commit validation, issue management
- **Pre-commit hooks**: Local validation before commit

### Key Configuration Options

**Interface / Protocol Selection:**

In v2 there are no compile-time interface flags. The protocol is selected at runtime by calling the appropriate init function before any other `esp_loader_*` call:

- `esp_loader_init_uart(&loader, &port.port)` — UART
- `esp_loader_init_usb(&loader, &port.port)` — USB CDC-ACM
- `esp_loader_init_spi(&loader, &port.port)` — SPI (RAM download only)
- `esp_loader_init_sdio(&loader, &port.port)` — SDIO (experimental)

For ESP-IDF / Kconfig builds, enable the port implementations you need with:

- `CONFIG_SERIAL_FLASHER_PORT_UART=y`
- `CONFIG_SERIAL_FLASHER_PORT_USB_CDC_ACM=y`
- `CONFIG_SERIAL_FLASHER_PORT_SPI=y`
- `CONFIG_SERIAL_FLASHER_PORT_SDIO=y`

Each enabled option compiles the matching `*_port.c` and exposes a `const esp_loader_port_ops_t` vtable (e.g. `esp32_uart_ops`, `esp32_spi_ops`). The caller owns the port struct (e.g. `esp32_port_t`) and sets `.port.ops` to point at the vtable. Multiple ports can be enabled simultaneously.

**Common Options:**

- `SERIAL_FLASHER_WRITE_BLOCK_RETRIES`: Number of write retries (default: 3)
- `SERIAL_FLASHER_RESET_HOLD_TIME_MS`: Reset assertion time (default: 100ms)
- `SERIAL_FLASHER_BOOT_HOLD_TIME_MS`: Boot pin assertion time (default: 50ms)

### Examples Directory Structure

```
examples/
├── esp32_example/              # Basic ESP32 UART flashing
├── esp32_spi_load_ram_example/ # SPI RAM loading
├── esp32_usb_cdc_acm_example/  # USB CDC ACM flashing
├── esp32_sdio_example/         # SDIO flashing (experimental)
├── stm32_example/              # STM32 setup guide (no pre-built project; generate with CubeMX)
├── pi_pico_example/            # Pi Pico host
├── zephyr_example/             # Zephyr RTOS integration
├── linux_example/              # Linux host (PC, SBC, Raspberry Pi)
└── binaries/                   # Pre-built binaries for testing
```

## Validation Steps

### Before Making Changes

1. **Always run basic build test**: `mkdir build && cd build && cmake .. -DPORT=USER_DEFINED && make`
2. **Install and run pre-commit**: `pre-commit install && pre-commit run --all-files`
3. **Check existing issues**: Review GitLab CI failures and GitHub issue tracker

### After Making Changes

1. **Run pre-commit validation**: `pre-commit run --all-files`
2. **Test affected examples**: Build and test relevant example applications
3. **Run QEMU tests if available**: `cd test && ./run_qemu_test.sh`
4. **Verify configuration options**: Test with different interface and MD5 settings

### Integration Validation

- **ESP-IDF compatibility**: Test with multiple ESP-IDF versions (v5.5+)
- **Cross-platform builds**: Verify Zephyr, Pi Pico builds don't break
- **Hardware testing**: Use pytest framework for hardware validation when possible

## Dependencies and Requirements

### Build Tools

- **CMake**: 3.22+
- **GCC/Clang**: Standard C99 compiler
- **Python**: 3.11+ for testing and pre-commit

### Platform-Specific Dependencies

- **ESP-IDF**: v5.5 or later for ESP32 builds
- **Zephyr**: Zephyr SDK v0.17.0+, west build tool
- **Pi Pico**: Pico SDK v1.5.1+, ARM GCC toolchain
- **Linux**: libgpiod ≥ 2.0 (for GPIO mode on SBCs)

### Python Dependencies (Testing)

```bash
pip install -r test/requirements_test.txt
# Includes: pytest, pytest-embedded, idf-build-apps
```

## Trust These Instructions

These instructions have been validated against the current codebase and CI/CD pipelines. Only perform additional exploration if:

- Instructions are incomplete for your specific use case
- You encounter errors not covered in the common issues
- Working with newly added features not documented here
- Integration with platforms not listed in supported hosts/targets

When in doubt, refer to example applications in the `examples/` directory as they contain working reference implementations for each supported platform.

## Common Issues and Workarounds

- **CMake warnings**: project() and version warnings are expected in standalone builds - they don't affect functionality
- **Pre-commit network timeouts**: Retry installation or skip if network connectivity is limited
- **ESP32-S3 test interference**: Run SPI and USB CDC ACM tests separately
- **Missing dependencies**: Each platform example README contains specific setup instructions
