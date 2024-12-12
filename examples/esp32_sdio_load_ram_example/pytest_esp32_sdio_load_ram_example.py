import pytest
from pytest_embedded import Dut


@pytest.mark.esp32
def test_esp32_sdio_load_ram_example(dut: Dut) -> None:
    dut.expect("Finished loading")
    dut.expect("Hello world!")
