# Copilot Instructions for esp-serial-flasher

## Repository Overview

`esp-serial-flasher` is a portable C library for flashing or loading applications to RAM of Espressif SoCs from other host microcontrollers. The library supports multiple host platforms (STM32, Raspberry Pi, ESP32, Zephyr OS, Pi Pico) and target chips (ESP32 family, ESP8266) using various communication interfaces (UART, SPI, USB CDC ACM, SDIO).

**Repository size**: ~500 files, primarily C source code with CMake build system
**Languages**: C (primary), Python (testing), CMake
**Target runtime**: Embedded systems, cross-platform compatibility
**Primary framework**: ESP-IDF component + standalone CMake library

## Build Instructions

### Prerequisites and Environment Setup

**Always run the following first for any STM32-related builds:**

```bash
git submodule update --init
```

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
**Common build flags**: `-DMD5_ENABLED=1`, `-DSERIAL_FLASHER_INTERFACE_UART=1`

### STM32 Build

**Required environment variables:**

- `STM32_TOOLCHAIN_PATH`: Path to ARM GCC toolchain
- `STM32_CUBE_H7_PATH`: Path to STM32Cube libraries
- `STM32_CHIP`: Target chip (e.g., STM32H743VI)
- `CORE_USED`: Core for multicore devices (M7/M4)

```bash
cd examples/stm32_example
mkdir build && cd build
cmake -DSTM32_TOOLCHAIN_PATH="path_to_toolchain" \
      -DSTM32_CUBE_H7_PATH="path_to_cube_libraries" \
      -DSTM32_CHIP="STM32H743VI" \
      -DCORE_USED=M7 \
      -DPORT="STM32" \
      -DMD5_ENABLED=1 \
      ..
cmake --build .
```

**Time required**: 3-10 minutes
**Common errors**: Missing toolchain path, missing STM32Cube libraries

### Zephyr Build

**Requires Zephyr SDK and west workspace setup**

```bash
export ZEPHYR_SDK_INSTALL_DIR=/path/to/zephyr-sdk
export ZEPHYR_TOOLCHAIN_VARIANT="zephyr"
cd zephyr_workspace/zephyr
west build -p -b esp32_devkitc_wroom/esp32/procpu \
    path/to/examples/zephyr_example \
    -DZEPHYR_EXTRA_MODULES=path/to/esp-serial-flasher \
    -DMD5_ENABLED=1
```

**Time required**: 5-15 minutes
**Prerequisites**: Zephyr SDK v0.17.0+, west installed

### Raspberry Pi Pico Build

**Requires Pico SDK and ARM toolchain**

```bash
export PICO_SDK_PATH=/path/to/pico-sdk
cd examples/pi_pico_example
mkdir build && cd build
cmake -DMD5_ENABLED=1 ..
cmake --build .
```

**Time required**: 1-3 minutes
**Output**: `.uf2` file for flashing to Pico

### Raspberry Pi Build

**Requires pigpio library**

```bash
# Install dependencies first
apt-get update && apt-get install -y cmake gcc g++ make pigpio
cd examples/raspberry_example
mkdir build && cd build
cmake -DMD5_ENABLED=1 .. && cmake --build .
```

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
pytest --target=stm32 --port=/dev/ttyACM0
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
├── esp_stubs.c       # Flash stub binaries
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
├── raspberry_port.c  # Raspberry Pi pigpio implementation
├── pi_pico_port.c    # Pi Pico SDK implementation
└── zephyr_port.c     # Zephyr RTOS implementation
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

**Interface Selection (choose one):**

- `SERIAL_FLASHER_INTERFACE_UART` (default)
- `SERIAL_FLASHER_INTERFACE_SPI` (RAM loading only)
- `SERIAL_FLASHER_INTERFACE_USB`
- `SERIAL_FLASHER_INTERFACE_SDIO` (experimental)

**Common Options:**

- `MD5_ENABLED`: Enable flash verification (default: enabled, disable for ESP8266)
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
├── stm32_example/              # STM32 host implementation
├── raspberry_example/          # Raspberry Pi host
├── pi_pico_example/            # Pi Pico host
├── zephyr_example/             # Zephyr RTOS integration
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

- **ESP-IDF compatibility**: Test with multiple ESP-IDF versions (v4.3+)
- **Cross-platform builds**: Verify STM32, Zephyr, Pi Pico builds don't break
- **Hardware testing**: Use pytest framework for hardware validation when possible

## Dependencies and Requirements

### Build Tools

- **CMake**: 3.5+ (3.10+ recommended to avoid warnings)
- **GCC/Clang**: Standard C99 compiler
- **Python**: 3.11+ for testing and pre-commit

### Platform-Specific Dependencies

- **ESP-IDF**: v4.3 or later for ESP32 builds
- **STM32**: ARM GCC toolchain 13.2+, STM32Cube H7 v1.11.1+
- **Zephyr**: Zephyr SDK v0.17.0+, west build tool
- **Pi Pico**: Pico SDK v1.5.1+, ARM GCC toolchain
- **Raspberry Pi**: pigpio library for GPIO control

### Python Dependencies (Testing)

```bash
pip install -r test/requirements_test.txt
# Includes: pytest, pytest-embedded, idf-build-apps, stm32loader
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
- **STM32 build failures**: Always verify toolchain paths and ensure submodules are initialized
- **ESP32-S3 test interference**: Run SPI and USB CDC ACM tests separately
- **Missing dependencies**: Each platform example README contains specific setup instructions
