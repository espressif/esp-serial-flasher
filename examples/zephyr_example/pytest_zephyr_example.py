# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
import pytest
from pytest_embedded import Dut
from esptool.cmds import detect_chip
import time

FLASH_ADDRESS = 0x1000
BIN_FILE = "/zephyr/zephyr.bin"


# This fixture is used to specify the path to the Zephyr app as it is not in the examples directory.
# It is used by conftest.py to find the app path to build directory.
@pytest.fixture
def app_path() -> str:
    return "zephyrproject-rtos/zephyr/"


@pytest.fixture(autouse=True)
def flash_zephyr(app_path: str, build_dir: str, port: str) -> None:
    with detect_chip(port) as esp:
        esp = esp.run_stub()
        path = app_path + build_dir + BIN_FILE
        with open(path, "rb") as binary:
            binary_data = binary.read()
            total_size = len(binary_data)

            esp.flash_begin(total_size, FLASH_ADDRESS)
            for i in range(0, total_size, esp.FLASH_WRITE_SIZE):
                block = binary_data[i : i + esp.FLASH_WRITE_SIZE]
                # Pad the last block
                block = block + bytes([0xFF]) * (esp.FLASH_WRITE_SIZE - len(block))
                esp.flash_block(block, i + FLASH_ADDRESS)
            esp.flash_finish()
            print("\n")
            print("Flashed successfully")

            esp.hard_reset()


def reset_target(dut: Dut) -> None:
    dut.serial.proc.setDTR(False)
    dut.serial.proc.setRTS(True)
    time.sleep(0.1)
    dut.serial.proc.setRTS(False)


@pytest.mark.zephyr
def test_zephyr_example(dut: Dut) -> None:
    # When esptool reset is done, dut sometimes does not catch the input after boot,
    # so we need to reset the target manually.
    reset_target(dut)

    # Check initial connection
    dut.expect("Connected to target")
    dut.expect("Transmission rate changed")

    # Check bootloader programming
    dut.expect("Loading bootloader")
    dut.expect("Erasing flash")
    dut.expect("Start programming")
    dut.expect("Finished programming")
    dut.expect("Flash verified")

    # Check partition table programming
    dut.expect("Loading partition table")
    dut.expect("Erasing flash")
    dut.expect("Start programming")
    dut.expect("Finished programming")
    dut.expect("Flash verified")

    # Check app programming
    dut.expect("Loading app")
    dut.expect("Erasing flash")
    dut.expect("Start programming")
    dut.expect("Finished programming")
    dut.expect("Flash verified")

    # Check target output
    dut.expect("Hello world!")
