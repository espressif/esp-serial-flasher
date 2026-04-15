/* Flash multiple partitions example - Linux USB port

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "linux_port.h"
#include "esp_loader.h"
#include "example_common.h"

#define DEFAULT_SERIAL_DEVICE  "/dev/ttyUSB0"
#define DEFAULT_BAUD_RATE      115200
#define HIGHER_BAUD_RATE       460800

static void print_usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s [OPTIONS] <addr1> <file1> [<addr2> <file2> ...]\n"
            "\n"
            "Options:\n"
            "  -p, --port <device>   Serial device (default: %s)\n"
            "  -b, --baud <rate>     Baud rate     (default: %d)\n"
            "  -m, --mode <mode>     GPIO mode: dtr-rts | gpio | none (default: dtr-rts)\n"
            "  -h, --help\n"
            "\n"
            "Note: USB JTAG Serial devices (ESP32-C3/S3/C6/H2/P4 native USB,\n"
            "      appearing as /dev/ttyACM*) are auto-detected within dtr-rts mode.\n"
            "\n"
            "Examples:\n"
            "  %s 0x1000 bootloader.bin 0x8000 partition-table.bin 0x10000 app.bin\n"
            "  %s -p /dev/ttyACM0 0x1000 bootloader.bin 0x8000 partition-table.bin 0x10000 app.bin\n"
            "  %s -p /dev/ttyUSB0 -b 115200 -m dtr-rts 0x1000 bl.bin\n",
            prog, DEFAULT_SERIAL_DEVICE, DEFAULT_BAUD_RATE, prog, prog, prog);
}

static uint8_t *read_file(const char *path, size_t *out_size)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Error: cannot open file '%s'\n", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (len <= 0) {
        fprintf(stderr, "Error: file '%s' is empty or unreadable\n", path);
        fclose(f);
        return NULL;
    }

    uint8_t *buf = malloc((size_t)len);
    if (!buf) {
        fprintf(stderr, "Error: out of memory\n");
        fclose(f);
        return NULL;
    }

    if (fread(buf, 1, (size_t)len, f) != (size_t)len) {
        fprintf(stderr, "Error: could not read file '%s'\n", path);
        free(buf);
        fclose(f);
        return NULL;
    }

    fclose(f);
    *out_size = (size_t)len;
    return buf;
}

int main(int argc, char *argv[])
{
    const char       *device    = DEFAULT_SERIAL_DEVICE;
    uint32_t          baud_rate = DEFAULT_BAUD_RATE;
    linux_gpio_mode_t gpio_mode = LINUX_GPIO_DTR_RTS;

    static const struct option long_opts[] = {
        { "port",  required_argument, NULL, 'p' },
        { "baud",  required_argument, NULL, 'b' },
        { "mode",  required_argument, NULL, 'm' },
        { "help",  no_argument,       NULL, 'h' },
        { NULL, 0, NULL, 0 }
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "p:b:m:h", long_opts, NULL)) != -1) {
        switch (opt) {
        case 'p':
            device = optarg;
            break;
        case 'b':
            baud_rate = (uint32_t)strtoul(optarg, NULL, 10);
            if (baud_rate == 0) {
                fprintf(stderr, "Invalid baud rate: %s\n", optarg);
                return 1;
            }
            break;
        case 'm':
            if (strcmp(optarg, "dtr-rts") == 0) {
                gpio_mode = LINUX_GPIO_DTR_RTS;
            } else if (strcmp(optarg, "gpio") == 0) {
                gpio_mode = LINUX_GPIO_GPIOD;
            } else if (strcmp(optarg, "none") == 0) {
                gpio_mode = LINUX_GPIO_NONE;
            } else {
                fprintf(stderr, "Unknown mode '%s'. Use: dtr-rts | gpio | none\n", optarg);
                return 1;
            }
            break;
        case 'h':
            print_usage(argv[0]);
            return 0;
        default:
            print_usage(argv[0]);
            return 1;
        }
    }

    /* Remaining args must be <addr> <file> pairs */
    int remaining = argc - optind;
    if (remaining == 0) {
        fprintf(stderr, "Error: no <addr> <file> pairs given.\n\n");
        print_usage(argv[0]);
        return 1;
    }
    if (remaining % 2 != 0) {
        fprintf(stderr, "Error: odd number of remaining arguments — expected <addr> <file> pairs.\n\n");
        print_usage(argv[0]);
        return 1;
    }

    int num_pairs = remaining / 2;
    char **pair_args = argv + optind;

    printf("Serial device : %s\n", device);
    printf("Baud rate     : %" PRIu32 "\n", baud_rate);
    printf("GPIO mode     : %s\n",
           gpio_mode == LINUX_GPIO_DTR_RTS ? "dtr-rts" :
           gpio_mode == LINUX_GPIO_GPIOD ? "gpio" : "none");

    esp_loader_t loader;

    linux_port_t port = {
        .port.ops  = &linux_uart_ops,
        .device    = device,
        .baudrate  = baud_rate,
        .gpio_mode = gpio_mode,
    };

    if (gpio_mode == LINUX_GPIO_GPIOD) {
        port.gpio_chip_path    = "/dev/gpiochip0";
        port.reset_pin         = 2;   /* RPi GPIO2 → ESP RESET */
        port.boot_pin          = 3;   /* RPi GPIO3 → ESP BOOT */
    }

    if (esp_loader_init_uart(&loader, &port.port) != ESP_LOADER_SUCCESS) {
        return 1;
    }

    if (connect_to_target(&loader, HIGHER_BAUD_RATE) != ESP_LOADER_SUCCESS) {
        return 1;
    }

    for (int i = 0; i < num_pairs; i++) {
        const char *addr_str = pair_args[i * 2];
        const char *file_path = pair_args[i * 2 + 1];

        char *endptr;
        uint32_t addr = (uint32_t)strtoul(addr_str, &endptr, 0);
        if (*endptr != '\0') {
            fprintf(stderr, "Error: invalid address '%s'\n", addr_str);
            return 1;
        }

        size_t size = 0;
        uint8_t *buf = read_file(file_path, &size);
        if (!buf) {
            return 1;
        }

        printf("\nFlashing '%s' at 0x%" PRIx32 " (%zu bytes)...\n", file_path, addr, size);
        esp_loader_error_t err = flash_binary(&loader, buf, (uint32_t)size, addr);
        free(buf);

        if (err != ESP_LOADER_SUCCESS) {
            fprintf(stderr, "Error: failed to flash '%s'\n", file_path);
            return 1;
        }
    }

    printf("\nAll done! Resetting target...\n");
    esp_loader_reset_target(&loader);
    esp_loader_deinit(&loader);

    return 0;
}
