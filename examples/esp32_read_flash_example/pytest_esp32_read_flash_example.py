# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
import pytest
from pytest_embedded import Dut


@pytest.mark.esp32
def test_esp32_read_flash_example(dut: Dut) -> None:
    dut.expect("Loading example data")
    dut.expect("Start programming")
    dut.expect("Finished programming", not_matching="Error")
    dut.expect("Flash contents match example data")
