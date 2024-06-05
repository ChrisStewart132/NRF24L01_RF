/**
 * Author: Christopher Stewart (Christopher.ray.stewart@gmail.com)
 * Date: 03062024
 * Description: program to test a NRF24 transceiver, register 0x2 is set to 1, registers 0x0 - 0x17 are read and should mostly be non-zero.
 * 
 * gcc -o test_rx test_rx.c -lgpiod
 * ./test_rx
 */

#include <gpiod.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#define GPIO_CHIP_NAME "gpiochip0"
#define GPIO_OFFSET_CE 25 // Chip Enable Activates RX or TX mode

#define SPI_DEVICE "/dev/spidev0.1"
#define SPI_HZ 8000000

#define R_REGISTER 0x00
#define W_REGISTER 0x20

void _spi_transfer(int fd, uint8_t* tx_buffer, uint8_t* rx_buffer, int size) {
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx_buffer,
        .rx_buf = (unsigned long)rx_buffer,
        .len = size,
        .speed_hz = SPI_HZ,
        .delay_usecs = 0,
        .bits_per_word = 8,
        .cs_change = 0,
    };
    int ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
    if (ret < 1) {
        perror("Failed to send SPI message");
        close(fd);
        exit(EXIT_FAILURE);
    }
}

void _gpio_high(struct gpiod_line* line) {
    gpiod_line_set_value(line, 1);
}

void _gpio_low(struct gpiod_line* line) {
    gpiod_line_set_value(line, 0);
}

void init_gpio(struct gpiod_chip** chip, struct gpiod_line** ce) {
    *chip = gpiod_chip_open_by_name(GPIO_CHIP_NAME);
    if (!*chip) {
        perror("gpiod_chip_open_by_name");
        exit(EXIT_FAILURE);
    }
    *ce = gpiod_chip_get_line(*chip, GPIO_OFFSET_CE);
    if (!*ce) {
        perror("gpiod_chip_get_line");
        gpiod_chip_close(*chip);
        exit(EXIT_FAILURE);
    }
    if (gpiod_line_request_output(*ce, "CE", 0) < 0) {
        perror("gpiod_line_request_output");
        gpiod_chip_close(*chip);
        exit(EXIT_FAILURE);
    }
}

void init_spi(int* fd) {
    *fd = open(SPI_DEVICE, O_RDWR);
    if (*fd < 0) {
        perror("Failed to open SPI device");
        exit(EXIT_FAILURE);
    }
}

void test(int fd, struct gpiod_line* ce) {
    printf("\ntest_rx\n");
    uint8_t tx_buffer[2], rx_buffer[2];

    tx_buffer[0] = W_REGISTER | 0x2;
    tx_buffer[1] = 1;// only enable ERX_P0 (data pipe 1)
    _spi_transfer(fd, tx_buffer, rx_buffer, 2);


    for(int i = 0x00; i <= 0x17; i++){
        tx_buffer[0] = R_REGISTER | i;
        tx_buffer[1] = 0xff;// no op
        _spi_transfer(fd, tx_buffer, rx_buffer, 2);
        printf("   reg_addr: 0x%x, status: 0x%x, reg: 0x%x\n", i, rx_buffer[0], rx_buffer[1]);
        usleep(100000);
    }
    printf("\n");
}

int main() {
    struct gpiod_chip* chip;
    struct gpiod_line* ce;
    init_gpio(&chip, &ce);
    int fd; // SPI file descriptor
    init_spi(&fd);
    _gpio_high(ce);// Activate 
    while (1) {
        sleep(1);
        test(fd, ce);
        break;
    }
    if (ce)
        gpiod_line_release(ce);
    if (chip)
        gpiod_chip_close(chip);
    close(fd);
    return EXIT_SUCCESS;
}