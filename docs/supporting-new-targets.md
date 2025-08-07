# Supporting New Host Targets

This guide explains how to add support for new host microcontrollers or platforms to ESP Serial Flasher.

## Overview

ESP Serial Flasher uses a port layer abstraction to support different host platforms. To add support for a new host target, you need to implement the required port functions for your specific hardware and interface.

## Required Port Functions

All host platforms must implement these core functions:

### Core Functions

#### `loader_port_read()`
- **Purpose**: Read data from the communication interface
- **Parameters**: Buffer, size, timeout
- **Returns**: Number of bytes read or error code

#### `loader_port_write()`
- **Purpose**: Write data to the communication interface  
- **Parameters**: Data buffer, size
- **Returns**: Success/error status

#### `loader_port_enter_bootloader()`
- **Purpose**: Put the target device into bootloader mode
- **Implementation**: Usually involves GPIO pin manipulation (reset/boot pins)

#### `loader_port_delay_ms()`
- **Purpose**: Provide millisecond delay functionality
- **Parameters**: Delay time in milliseconds

#### `loader_port_start_timer()`
- **Purpose**: Start a timer for timeout operations
- **Parameters**: Timeout value in milliseconds

#### `loader_port_remaining_time()`
- **Purpose**: Get remaining time on the current timer
- **Returns**: Remaining time in milliseconds

## Interface-Specific Functions

### SPI Interface

If implementing SPI interface support, also implement:

#### `loader_port_spi_set_cs()`
- **Purpose**: Control SPI chip select signal
- **Parameters**: CS state (active/inactive)

### SDIO Interface

If implementing SDIO interface support, also implement:

#### `loader_port_sdio_card_init()`
- **Purpose**: Initialize SDIO card communication
- **Returns**: Initialization status

#### `loader_port_wait_int()`
- **Purpose**: Wait for SDIO interrupt
- **Parameters**: Timeout value
- **Returns**: Interrupt status

## Optional Convenience Functions

These functions are part of [esp_loader_io.h](../include/esp_loader_io.h) for convenience but are not called directly by the library. You can implement them as needed for your platform:

#### `loader_port_change_transmission_rate()`
- **Purpose**: Change communication baud rate or speed
- **Use case**: Optimizing transfer speeds

#### `loader_port_reset_target()`
- **Purpose**: Hardware reset of target device
- **Use case**: Recovery from communication errors

#### `loader_port_debug_print()`
- **Purpose**: Debug output functionality
- **Use case**: Development and troubleshooting

## Implementation Steps

### 1. Create Port Directory

Create a new directory under `port/` for your platform:
```
port/
├── esp32_port/
├── stm32_port/
├── your_platform_port/  # New platform
└── ...
```

### 2. Implement Required Functions

Create source files implementing all required functions. See existing ports for reference:
- `port/esp32_port/` - ESP32 implementation
- `port/stm32_port/` - STM32 implementation

### 3. Create CMake Configuration

Add CMake configuration to integrate with the build system:

```cmake
# In your platform's CMakeLists.txt
if(PORT STREQUAL "YOUR_PLATFORM")
    target_sources(flasher PRIVATE
        port/your_platform_port/your_platform_port.c
    )
    target_include_directories(flasher PRIVATE
        port/your_platform_port/include
    )
endif()
```

### 4. Update Main CMakeLists.txt

Add your platform to the main CMake configuration:

```cmake
# Add to main CMakeLists.txt
if(PORT STREQUAL "YOUR_PLATFORM")
    add_subdirectory(port/your_platform_port)
endif()
```

## Function Prototypes

All function prototypes are defined in [include/esp_loader_io.h](../include/esp_loader_io.h). Refer to this file for exact function signatures and parameter details.

## Platform-Specific Considerations

### Hardware Requirements

Consider these hardware requirements for your platform:

- **GPIO pins**: For reset and boot control (UART interface)
- **Communication interface**: UART, SPI, USB, or SDIO hardware
- **Timing precision**: Microsecond-level timing may be required
- **Memory**: Sufficient RAM for buffers and protocol overhead

### Real-Time Constraints

Some operations have timing constraints:
- Boot sequence timing (reset/boot pin assertions)
- Communication timeouts
- Protocol response times

### Error Handling

Implement robust error handling:
- Communication timeouts
- Hardware errors
- Invalid responses from target

## Testing Your Implementation

### Basic Functionality Test

1. **Connection test**: Verify communication with target
2. **Sync test**: Ensure protocol synchronization works
3. **Memory operations**: Test RAM loading and flash operations

### Example Test Code

```c
#include "esp_loader.h"

// Basic connection test
esp_loader_error_t test_connection(void) {
    esp_loader_connect_args_t connect_config = ESP_LOADER_CONNECT_DEFAULT();
    return esp_loader_connect(&connect_config);
}
```

### Integration Testing

Test with actual ESP target devices:
- Flash programming
- RAM loading  
- Different target types (ESP32, ESP32-S3, etc.)

## Submitting Your Port

### Before Submitting

1. **Test thoroughly** on your platform
2. **Follow coding standards** (see [Contributing Guide](contributing.md))
3. **Add documentation** for your platform
4. **Create example** demonstrating usage

### Pull Request

1. **Open an issue first** to discuss the new platform support
2. **Create pull request** with your implementation
3. **Include example** and documentation
4. **Provide testing results**

## Getting Help

### Resources

- Study existing port implementations in `port/` directory
- Check [include/esp_loader_io.h](../include/esp_loader_io.h) for function specifications
- Review [examples/](../examples/) for usage patterns

### Support

- Open [GitHub issue](https://github.com/espressif/esp-serial-flasher/issues) for questions
- Tag issue with "new platform" or "porting" labels
- Provide details about your target platform

## Common Pitfalls

### Timing Issues
- Ensure precise timing for boot sequences
- Implement proper timeout handling

### GPIO Control
- Verify GPIO pin polarity (some may need inversion)
- Ensure proper pin initialization

### Communication Reliability
- Implement proper error detection
- Handle communication timeouts gracefully

### Memory Management
- Avoid memory leaks in port functions
- Use appropriate buffer sizes

## Examples

See existing ports for implementation examples:
- [ESP32 Port](../port/esp32_port/) - Full-featured implementation
- [STM32 Port](../port/stm32_port/) - HAL-based implementation