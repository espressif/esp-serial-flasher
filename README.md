# ESP Serial Flasher

ESP Serial Flasher is a portable C library for flashing applications or loading code to RAM of Espressif SoCs from other host microcontrollers.

## Overview

This library enables you to program ESP32-series microcontrollers from various host platforms using different communication interfaces. It provides a unified API that abstracts the underlying communication protocol, making it easy to integrate ESP device programming into your projects.

### Supported Host Platforms

- **STM32** microcontrollers
- **Raspberry Pi** SBC
- **ESP32 series** microcontrollers  
- **Zephyr OS** compatible devices
- **Raspberry Pi Pico** (RP2040)

### Supported Target Devices

- ESP32
- ESP8266
- ESP32-S2
- ESP32-S3
- ESP32-C3
- ESP32-C2
- ESP32-H2
- ESP32-C6
- ESP32-C5 (stub support coming soon)
- ESP32-P4 (stub support coming soon)

### Supported Communication Interfaces

- **UART** - Universal asynchronous communication
- **USB CDC ACM** - USB virtual serial port
- **SPI** - Serial Peripheral Interface (RAM download only)
- **SDIO** - Secure Digital Input/Output (experimental)

> **Note:** SDIO interface is experimental and currently supported only with ESP32-P4 as host and ESP32-C6 as target. The implementation uses a custom stub that will be made available as part of the migration to [esp-flasher-stub](https://github.com/espressif/esp-flasher-stub).

## Getting Started

### Prerequisites

To use ESP Serial Flasher, you need:

- **CMake 3.5 or later** - Build system
- **Git** - For cloning the repository with submodules
- **Platform-specific toolchain** - Depends on your host platform
  - ESP-IDF for ESP32 hosts
  - ARM GCC toolchain for STM32/Raspberry Pi Pico
  - Raspberry Pi OS development tools for Raspberry Pi
  - Zephyr SDK for Zephyr OS

### Basic Usage

1. **Clone the repository:**
   ```bash
   git clone --recursive https://github.com/espressif/esp-serial-flasher.git
   ```

2. **Include in your project:**
   ```cmake
   add_subdirectory(esp-serial-flasher)
   target_link_libraries(your_project flasher)
   ```

3. **Basic API usage:**
   ```c
   #include "esp_loader.h"

   // Initialize and connect
   esp_loader_connect_args_t connect_config = ESP_LOADER_CONNECT_DEFAULT();
   esp_loader_connect(&connect_config);

   // Flash binary
   esp_loader_flash_start(target_address, binary_size, sizeof(block));
   esp_loader_flash_write(data, sizeof(data));
   esp_loader_flash_finish(true);
   ```

### Examples

For complete implementation examples, see the [examples](examples/) directory:

- [ESP32 Example](examples/esp32_example/) - ESP32 as host
- [STM32 Example](examples/stm32_example/) - STM32 as host  
- [Raspberry Pi Example](examples/raspberry_example/) - Raspberry Pi as host
- [Zephyr Example](examples/zephyr_example/) - Zephyr OS integration
- [Raspberry Pi Pico Example](examples/pi_pico_example/) - RP2040 as host

### Educational Resources

- [YouTube Tutorial](https://www.youtube.com/watch?v=hYqkOew8y8w) - Comprehensive guide covering library usage, internals, and custom port implementation

## Configuration

ESP Serial Flasher provides several configuration options to customize its behavior. These options are set as **CMake cache variables**.

### Basic Configuration

The most common configuration options:

```bash
# Enable SPI interface instead of UART
cmake -DSERIAL_FLASHER_INTERFACE_SPI=1 ..

# Disable MD5 verification (required for ESP8266)
cmake -DMD5_ENABLED=0 ..

# Set custom retry count
cmake -DSERIAL_FLASHER_WRITE_BLOCK_RETRIES=5 ..
```

### Interface Selection

Choose one interface (UART is default):

- `SERIAL_FLASHER_INTERFACE_UART` - UART communication (default)
- `SERIAL_FLASHER_INTERFACE_SPI` - SPI communication  
- `SERIAL_FLASHER_INTERFACE_USB` - USB CDC ACM
- `SERIAL_FLASHER_INTERFACE_SDIO` - SDIO (experimental)

For complete configuration reference, see [Configuration Documentation](docs/configuration.md).

## Platform Setup

Different host platforms require specific setup procedures:

- **ESP32 series**: Works with ESP-IDF v4.3 or later
- **STM32**: Requires STM32 HAL libraries and ARM toolchain
- **Zephyr**: Integrates as Zephyr module with specific Kconfig options
- **Raspberry Pi Pico**: Uses Pico SDK
- **Raspberry Pi**: Requires pigpio library

For detailed setup instructions, see [Platform Setup Guide](docs/platform-setup.md).

## Contributing

We welcome contributions to the ESP Serial Flasher project! Before starting work on a new feature or significant change, please [open an issue](https://github.com/espressif/esp-serial-flasher/issues) to discuss your proposal.

For detailed contribution guidelines, including code style, commit message format, and development setup, see [Contributing Guide](docs/contributing.md).

### Quick Start for Contributors

1. Install pre-commit hooks:
```bash
pip install pre-commit
pre-commit install
pre-commit install -t commit-msg
```

2. Follow [ESP-IDF contribution guidelines](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/contribute/style-guide.html)
3. Use [Conventional Commits](https://www.conventionalcommits.org/) format

### Adding New Platform Support

If you want to add support for a new host platform, see [Supporting New Targets Guide](docs/supporting-new-targets.md).

## License

This project is licensed under the Apache 2.0 License - see the [LICENSE](LICENSE) file for details.

## Known Limitations

The following limitations are currently known:

- Binary image size must be known before flashing
- ESP8266 targets require `MD5_ENABLED=0` due to ROM bootloader limitations
- SPI interface only supports RAM download operations
- SDIO interface is experimental with limited platform support

For additional limitations and current issues, see the [GitHub Issues](https://github.com/espressif/esp-serial-flasher/issues) page.
