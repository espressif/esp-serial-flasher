import pytest
from pytest_embedded import Dut


@pytest.mark.esp32
def test_esp32_deflate_example(dut: Dut) -> None:
    dut.expect("Connected to target")
    dut.expect("Transmission rate changed")

    dut.expect("Flashing bootloader via DEFLATE", timeout=30)
    dut.expect(r"Progress: \d+ %")
    dut.expect("Verifying bootloader via MD5...")
    dut.expect("bootloader MD5 matches.")

    dut.expect("Flashing partition table via DEFLATE", timeout=30)
    dut.expect(r"Progress: \d+ %")
    dut.expect("Verifying partition table via MD5...")
    dut.expect("partition table MD5 matches.")

    dut.expect("Flashing application via DEFLATE", timeout=30)
    dut.expect(r"Progress: \d+ %")
    dut.expect("Verifying application via MD5...")
    dut.expect("application MD5 matches.")
    dut.expect("All flashes verified. Success!")

    dut.expect("Logs below are print from slave")
    dut.expect("Hello world!", timeout=10)
