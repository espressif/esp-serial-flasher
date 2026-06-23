# Serial Flasher Tests

## Overview

Three kinds of tests are written for serial flasher:

- Qemu tests
- Target tests
- Test apps

## Qemu Tests

Qemu tests use emulated esp32 to test the correctness of the library.

### Installation

Install QEMU for ESP-IDF by following the [official ESP-IDF QEMU guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/tools/qemu.html).

`qemu-system-xtensa` must be available on `PATH`. This is the case by default when using the ESP-IDF installer or the `espressif/idf` Docker image.

Optionally, if your `qemu-system-xtensa` binary is not on `PATH`, set `QEMU_PATH` to point to it:

```bash
export QEMU_PATH=path_to_qemu-system-xtensa
```

### Running

```bash
./run_qemu_test.sh
```

## Target Tests

To install all the necessary tools for running the Build and Target tests just run the following command:

`pip install -r test/requirements_test.txt`

### Build

The build process consists of two steps:

1. **Build target firmware binaries** - These are the firmware images that will be flashed onto the target ESP devices
2. **Build host examples** - These are the examples that run on the host device and perform the flashing

#### Step 1: Build Target Firmware Binaries

Target firmware binaries must be built separately before building the host examples. These binaries are built from the source in `test/target-example-src/` and then copied to the appropriate `examples/*/target-firmware/` directories.

For detailed instructions on building target firmware, see [test/target-example-src/README.md](target-example-src/README.md).

After building the target firmware, copy the binaries to the example directories using the provided script:

```bash
python3 test/copy_target_binaries.py
```

This script will automatically copy the appropriate binaries from `test/target-example-src/hello-world-ESP32-src/build-*/` to the corresponding `examples/*/target-firmware/` directories based on the chip type and build configuration.

#### Step 2: Build Host Examples

Each example can be built according its README. To make things simpler, there is a tool to build all Espressif SoC examples with one command called [idf-build-apps](https://docs.espressif.com/projects/idf-build-apps/en/latest/). Before executing the [idf-build-apps](https://docs.espressif.com/projects/idf-build-apps/en/latest/), you need to run export script of [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html). After this, you are able to run the following command to build the examples:

```bash
python -m idf_build_apps build -v -p .
      --recursive
      --exclude ./test/target-example-src
      --config "sdkconfig.defaults*"
      --build-dir "build_"@w
      --check-warnings
```

To build examples for other SoCs, please refer to the README of each example.

### Test

Pytest is used to test the built examples. There is a test written for each example. To execute it, just run the following command with replacing target_name and port with your settings:

```bash
pytest --target=<target_name> --port=<port>
```

The examples for Espressif SoC are built for ESP32 and some of them for ESP32-S3 (`esp32_spi_load_ram_example` and `esp32_usb_cdc_acm_example`). Please be aware that tests for ESP32-S3 need to be run separately as `esp32_usb_cdc_acm_example` fails when running after `esp32_spi_load_ram_example`.

## Test Apps

`test/test_apps/` holds standalone executables that link the library directly and test it themselves. They cover what the example-based target tests cannot: API that no example uses, error paths, and scenarios needing more than one target at once.

A test app is not driven by pytest. It runs its own cases, prints a marker per case, and exits `0` when every case passed or was skipped and `1` when any case failed, so CI only has to build the binary, run it, and read the exit code.

| App          | What it covers                                                                                                                         |
| ------------ | -------------------------------------------------------------------------------------------------------------------------------------- |
| `linux_host` | Linux port and loader against a real serial port: register read/write, levelled logging dispatch, and flashing two targets in parallel |

### The shared harness

`test_apps/test_common/` is a small harness shared by every test app. It only uses `printf`, so it works on a Linux host and on an ESP-IDF target alike.

A test case is a parameterless function returning `test_result_t` (`TEST_PASS`, `TEST_FAIL` or `TEST_SKIP`) that sets up whatever it needs itself. Collect the cases into a `test_case_t` array and hand it to `RUN_TEST_CASES()`:

```c
#include "test_common.h"

static test_result_t test_something(void)
{
    CHECK_EQ(esp_loader_connect(&loader, &args), ESP_LOADER_SUCCESS, "cannot connect on %s", port);
    CHECK(response[0] == 0xAA, "unexpected first byte");
    return TEST_PASS;
}

static const test_case_t test_cases[] = {
    { "something", test_something },
};

int main(void)
{
    return RUN_TEST_CASES(test_cases);
}
```

Three macros are available. `CHECK` and `CHECK_EQ` print the failing expression with its file and line, then return `TEST_FAIL` from the case; the trailing printf-style note is optional in all of them.

| Macro                             | Use                                                                          |
| --------------------------------- | ---------------------------------------------------------------------------- |
| `CHECK(cond, ...)`                | Any boolean condition                                                        |
| `CHECK_EQ(actual, expected, ...)` | Equality, reporting both operands (in hex too, once they are register-sized) |
| `TEST_PRINT_MSG(...)`             | Print a note without failing, such as why a case skipped                     |

`CHECK` returns immediately, so it cannot be used after a case has acquired something it must release. Put those checks in a helper and let the caller own the cleanup, the way `test_register_read_write` in `linux_host/main.c` wraps `register_read_write`.

The runner prints one line per case and a summary:

```
RUN  : register_read_write
PASS : register_read_write
RUN  : parallel_flashing
    --port2 not given, nothing to flash in parallel
SKIP : parallel_flashing
Tests finished: 4 passed, 0 failed, 1 skipped
```

### Building and running

```bash
cmake -S test/test_apps/linux_host -B test/test_apps/linux_host/build
cmake --build test/test_apps/linux_host/build
./test/test_apps/linux_host/build/linux_host --port /dev/ttyUSB0
```

`--port2 <device>` adds a second target, which enables the `parallel_flashing` case; it is skipped without it. `--verbose` prints the protocol trace of the cases that talk to hardware, which is off by default so the output stays down to the per-case markers.

`linux_host` is compiled with `SERIAL_FLASHER_LOG_LEVEL=DEBUG`, so its logging cases exercise every `LOADER_LOG*` macro, and with `-Wall -Wextra -Werror`. It is the only build that compiles the whole library for the Linux port, so it is where library warnings surface.

In CI, `.test_linux_host_template` in `.gitlab-ci.yml` builds and runs the app once per attached device. It builds on the runner rather than reusing a build artifact, because each device carries its own runner tag.

### Adding a test app

1. Create `test/test_apps/<name>/` with a `main.c` and a `CMakeLists.txt`. Copying `linux_host/CMakeLists.txt` is the quickest start: it compiles `../test_common/test_common.c`, puts `../test_common` on the include path, and pulls in the library with `add_subdirectory()` after selecting a `PORT`.
2. Write the cases as above and return `RUN_TEST_CASES()` from `main()`.
3. Add a job to `.gitlab-ci.yml` that builds the app and runs the binary. Extend `.test_linux_host_template` if the app runs on Linux against a tagged device.
