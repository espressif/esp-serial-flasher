# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
import pytest
import pexpect


@pytest.mark.linux
@pytest.mark.parametrize("mode", ["dtr-rts", "gpio"])
def test_linux_example(port, mode):
    with pexpect.spawn(
        "./examples/linux_example/build/linux_flasher"
        f" -p {port}"
        f" -m {mode}"
        " 0x0 examples/linux_example/target-firmware/bootloader.bin"
        " 0x8000 examples/linux_example/target-firmware/partition-table.bin"
        " 0x10000 examples/linux_example/target-firmware/app.bin",
        timeout=60,
        encoding="utf-8",
    ) as p:
        # Check initial connection
        p.expect("Connected to target")
        p.expect("Transmission rate changed")

        # Check bootloader programming
        p.expect("Erasing flash")
        p.expect("Start programming")
        p.expect("Finished programming")
        p.expect("Flash verified")

        # Check partition table programming
        p.expect("Erasing flash")
        p.expect("Start programming")
        p.expect("Finished programming")
        p.expect("Flash verified")

        # Check app programming
        p.expect("Erasing flash")
        p.expect("Start programming")
        p.expect("Finished programming")
        p.expect("Flash verified")

        # Check target reset
        p.expect("All done! Resetting target...")
