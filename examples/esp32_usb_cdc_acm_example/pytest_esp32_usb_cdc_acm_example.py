import pytest
from pytest_embedded import Dut


@pytest.mark.esp32s3
def test_esp32_usb_cdc_acm_example(dut: Dut) -> None:
    dut.expect("Finished programming")
    for i in range(3):
        dut.expect("Flash verified")
