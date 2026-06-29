# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
import pytest
from pytest_embedded import Dut


def _expect_flashing_session(dut: Dut, interface_label: str) -> None:
    # Check initial connection
    dut.expect("Connected to target")
    dut.expect(f"Flashing {interface_label}")

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

    dut.expect(f"{interface_label} flashing done!")


@pytest.mark.esp32p4
def test_esp32_runtime_interface_switching_example(dut: Dut) -> None:
    # The host flashes the same target over three interfaces
    _expect_flashing_session(dut, "target over USB")
    _expect_flashing_session(dut, "target over SDIO")
    _expect_flashing_session(dut, "target over UART")

    # All interfaces flashed successfully
    dut.expect("All targets flashed")
