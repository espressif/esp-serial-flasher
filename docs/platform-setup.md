# Platform Setup Guide

This document provides setup instructions for using ESP Serial Flasher on different host platforms.

## Prerequisites

Before setting up a specific platform, ensure you have the following:

- CMake 3.5 or later
- Git (for cloning with submodules)
- Platform-specific toolchain (see sections below)

## ESP-IDF Support

### Supported Versions
- ESP-IDF v4.3 or later

### Setup
No additional setup is required when using ESP Serial Flasher with ESP-IDF projects. The library integrates directly with the ESP-IDF build system.

## STM32 Setup

**Testing Status**: The library has been tested with STM32CubeH7 v1.11.1 and arm-gnu-toolchain-13.2.

### Prerequisites
- STM32 HAL libraries for your target chip
- ARM GCC toolchain
- stm32-cmake support package (included as submodule)

### Initial Setup

1. Clone the repository with submodules:
```bash
git clone --recursive https://github.com/espressif/esp-serial-flasher.git
```

If you've already cloned without `--recursive`, initialize the submodule:
```bash
git submodule update --init
```

### Required CMake Variables

The following variables must be defined when building for STM32:

- `STM32_TOOLCHAIN_PATH`: Path to ARM toolchain
- `STM32_CUBE_<CHIP_FAMILY>_PATH`: Path to STM32 Cube libraries
- `STM32_CHIP`: Target STM32 chip name
- `CORE_USED`: Core used on multicore devices (e.g., M7 or M4)
- `PORT`: Must be set to `STM32`

### Build Example

```bash
cmake \
  -DSTM32_TOOLCHAIN_PATH="/path/to/gcc-arm-none-eabi" \
  -DSTM32_CUBE_H7_PATH="/path/to/STM32Cube_FW_H7" \
  -DSTM32_CHIP="STM32H743VI" \
  -DCORE_USED="M7" \
  -DPORT="STM32" \
  .. && cmake --build .
```

### Alternative: CMake Variables File

You can set these variables in your top-level CMakeLists.txt:
```cmake
set(STM32_TOOLCHAIN_PATH /path/to/gcc-arm-none-eabi)
set(STM32_CUBE_H7_PATH /path/to/STM32Cube_FW_H7)
set(STM32_CHIP STM32H743VI)
set(CORE_USED M7)
set(PORT STM32)
```

## Zephyr Setup

**Testing Status**: The library has been tested with Zephyr RTOS v4.0.0 and Zephyr SDK v0.17.0.

### Integration as Zephyr Module

Add the following to your manifest file (`west.yml`):

```yaml
manifest:
  projects:
    - name: esp-flasher
      url: https://github.com/espressif/esp-serial-flasher
      revision: master
      path: modules/lib/esp_flasher
```

### Project Configuration

Add these options to your project configuration (`prj.conf`):

```
CONFIG_ESP_SERIAL_FLASHER=y
CONFIG_CONSOLE_GETCHAR=y
CONFIG_SERIAL_FLASHER_MD5_ENABLED=y
```

### Example Code

See [examples/zephyr_example](../examples/zephyr_example) for a complete implementation example.

## Raspberry Pi Pico Setup

**Testing Status**: The library has been tested with Raspberry Pi Pico SDK v1.5.1 and arm-gnu-toolchain-13.2.

### Prerequisites
- Raspberry Pi Pico SDK
- ARM GCC toolchain

### Setup

The Raspberry Pi Pico port allows using the RP2040 microcontroller as a host for programming ESP devices.

### Example Code

See [examples/pi_pico_example](../examples/pi_pico_example) for a complete implementation example.

## Raspberry Pi Setup

**Testing Status**: The library has been tested with the latest Raspberry Pi OS.

### Prerequisites
- Raspberry Pi OS
- pigpio library for GPIO control

### Setup

The Raspberry Pi port allows using a Raspberry Pi SBC as a host for programming ESP devices.

### Example Code

See [examples/raspberry_example](../examples/raspberry_example) for a complete implementation example.

## Dependencies Summary

### Required Dependencies
- CMake 3.5+
- Git
- Platform-specific toolchain

### Optional Dependencies (Platform-Specific)
- **STM32**: STM32 HAL libraries, ARM GCC toolchain
- **Zephyr**: Zephyr RTOS v4.0.0+, Zephyr SDK v0.17.0+
- **Raspberry Pi Pico**: Raspberry Pi Pico SDK v1.5.1+, ARM GCC toolchain
- **Raspberry Pi**: pigpio library

## Common Issues

### Submodule Not Initialized
If you encounter build errors related to missing files, ensure submodules are initialized:
```bash
git submodule update --init --recursive
```

### Toolchain Path Issues
Ensure all paths to toolchains and libraries are absolute paths and accessible from your build environment.