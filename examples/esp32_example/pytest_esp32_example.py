import pytest
from pytest_embedded import Dut


@pytest.mark.esp32
def test_esp32_example(dut: Dut) -> None:
    for i in range(3):
        dut.expect("Finished programming")
    dut.expect("Hello world!")
