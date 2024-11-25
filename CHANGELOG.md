## v1.7.0 (2024-11-25)

### New Features

- Add support for reading from target flash
- Add support for inverting reset and boot pins
- Update zephyr example to v4.0.0
- **examples**: Provide more useful error messages

### Bug Fixes

- Clarify and validate alignment requirements for flashing
- ROUNDUP calculation fix
- Zephyr example
- Handling of USB buffer overflow
- Callback when USB port is disconnected

## v1.6.2 (2024-10-18)

### Bug Fixes

- Fix the ESP USB CDC ACM port read return value

## v1.6.1 (2024-10-15)

### Bug Fixes

- Fix ESP32-H2 flash detection

## v1.6.0 (2024-09-26)

### New Features

- Add support for Secure Download Mode
- Add support for getting target security info
- Add logging from target to rpi pico port

### Bug Fixes

- Add delay to usb example to ensure connection
- Do not enable logging if flashing error - STM
- Add missing esp32c3 chip magic numbers
- Multiple timeout fixes
- Only check if image fits into flash when we know flash size
- Duplicate word in logging message

### Code Refactoring

- **protocol**: Rework command and response handling code
- **protocol**: Add support for receiving variably sized SLIP packets

## v1.5.0 (2024-08-13)

### New Features

- Add the Raspberry Pi Pico port

## v1.4.1 (2024-07-16)

### Bug Fixes

- Add correct MD5 calculation with stub enabled
- Remove duplicate word in logging

## v1.4.0 (2024-07-02)

### New Features

- Add stub-support

### Bug Fixes

- Fix MD5 option handling

## v1.3.1 (2024-06-26)

### Bug Fixes

- Fix ESP SPI port duplicate tracing
- SPI interface/esp port alignment requirements fix
- Upload to RAM examples monitor task priority fix

## v1.3.0 (2024-03-22)

### New Features

- Add a convenient public API way to read the WIFI MAC

### Bug Fixes

- Correctly compare image size with memory size including offset

## v1.2.0 (2024-02-28)

### New Features

- Move flash size detection functionality to the public API

### Bug Fixes

- Fix inferring flash size from the flash ID
- **docs**: Fix table in SPI load to RAM example

## v1.1.0 (2024-02-13)

### New Features

- USB CDC ACM interface support
- Add the ability for ESP ports to not initialize peripherals

### Bug Fixes

- **docs**: Remove notes about SPI interface being experimental
- Document size_id and improve comments in places

## v1.0.2 (2023-12-20)

### Bug Fixes

- Fix flash size ID sanity checks

## v1.0.1 (2023-12-19)

### Bug Fixes

- Fix md5 timeout values
- **ci**: Add more compiler warnings for the flasher in the examples

## v1.0.0 (2023-12-10)

- Initial release
