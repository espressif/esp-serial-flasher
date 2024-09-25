# Serial flasher Tests

## Qemu tests

Qemu tests use an emulated esp32 to test the correctness of the library. 

### Installation

Please refer to [building qemu](https://github.com/espressif/qemu) for instructions how to compile.

### Build and run

QEMU_PATH environment variable pointing to compiled `qemu/build/xtensa-softmmu/qemu-system-xtensa` has to be defined.
```
export QEMU_PATH=path_to_qemu-system-xtensa
./run_test.sh qemu
```
