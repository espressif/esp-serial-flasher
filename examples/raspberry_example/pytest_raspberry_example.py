import pytest
import pexpect


@pytest.mark.raspberry
def test_raspberry_example():
    with pexpect.spawn(
        "./examples/raspberry_example/build/raspberry_flasher",
        timeout=10,
        encoding="utf-8",
    ) as p:
        p.expect("Hello world!")
