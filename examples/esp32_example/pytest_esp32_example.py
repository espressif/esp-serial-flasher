# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
import pytest
from pytest_embedded import Dut


@pytest.mark.esp32
def test_esp32_example(dut: Dut) -> None:
    # Check initial connection
    dut.expect("Connected to target")
    dut.expect("Transmission rate changed")

    # Check bootloader programming
    dut.expect("Loading bootloader...")
    dut.expect("Erasing flash")
    dut.expect("Start programming")
    dut.expect("Finished programming")
    dut.expect("Flash verified")
    dut.expect("Loading partition table...")

    # Check partition table programming
    dut.expect("Erasing flash")
    dut.expect("Start programming")
    dut.expect("Finished programming")
    dut.expect("Flash verified")
    dut.expect("Loading app...")

    # Check app programming
    dut.expect("Erasing flash")
    dut.expect("Start programming")
    dut.expect("Finished programming")
    dut.expect("Flash verified")
    dut.expect("Hello world!")
