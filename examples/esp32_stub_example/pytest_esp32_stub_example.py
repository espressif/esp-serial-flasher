import pytest
from pytest_embedded import Dut


@pytest.mark.esp32
def test_esp32_stub_example(dut: Dut) -> None:
    dut.expect("Finished programming")
    for i in range(3):
        dut.expect("Flash verified")
    dut.expect("Hello world!")
