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
