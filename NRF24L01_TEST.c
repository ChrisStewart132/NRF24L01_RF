/**
 * Author: Christopher Stewart (Christopher.ray.stewart@gmail.com)
 * Date: 14062024
 * Description: program to read the current state of a NRF24 transceiver, registers 0x0 - 0x17 are read and output in a human readable format, and the radio is powered down.
 * 
 * gcc -o NRF24L01_TEST NRF24L01_TEST.c -lgpiod
 * ./NRF24L01_TEST
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

#define GPIO_CHIP_NAME "gpiochip4"
#define GPIO_OFFSET_CE 25 // Chip Enable Activates RX or TX mode

#define SPI_DEVICE "/dev/spidev0.1"
#define SPI_HZ 8000000

#define R_REGISTER 0x00
#define W_REGISTER 0x20

enum register_address {
    CONFIG = 0,
    EN_AA,
    EN_RXADDR,
    SETUP_AW,
    SETUP_RETR,
    RF_CH,
    RF_SETUP,
    STATUS,
    OBSERVE_TX,
    CD,// reserved
    RX_ADDR_P0,
    RX_ADDR_P1,
    RX_ADDR_P2,
    RX_ADDR_P3,
    RX_ADDR_P4,
    RX_ADDR_P5,
    TX_ADDR,
    RX_PW_P0,
    RX_PW_P1,
    RX_PW_P2,
    RX_PW_P3,
    RX_PW_P4,
    RX_PW_P5,
    FIFO_STATUS
};

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

void print_register(enum register_address reg_addr, int value){
    switch (reg_addr) {
        case CONFIG:
            printf("CONFIG: 0x%x\n", value);
            printf("    MASK_RX_DR: %d\n", (value & (1<<6)) != 0);
            printf("    MASK_TX_DS: %d\n", (value & (1<<5)) != 0);
            printf("    MASK_MAX_RT: %d\n", (value & (1<<4)) != 0);
            printf("    EN_CRC: %d\n", (value & (1<<3)) != 0);
            printf("    CRCO: 1+%d bytes\n", (value & (1<<2)) != 0);
            printf("    PWR_UP: %d\n", (value & (1<<1)) != 0);
            printf("    PRIM_RX: %d\n", (value & (1<<0)) != 0);
            break;
        case EN_AA:
            printf("EN_AA: 0x%x\n", value);
            printf("    ENAA_P5: %d\n", (value & (1<<5)) != 0);
            printf("    ENAA_P4: %d\n", (value & (1<<4)) != 0);
            printf("    ENAA_P3: %d\n", (value & (1<<3)) != 0);
            printf("    ENAA_P2: %d\n", (value & (1<<2)) != 0);
            printf("    ENAA_P1: %d\n", (value & (1<<1)) != 0);
            printf("    ENAA_P0: %d\n", (value & (1<<0)) != 0);
            break;
        case EN_RXADDR:
            printf("EN_RXADDR: 0x%x\n", value);
            printf("    ERX_P5: %d\n", (value & (1<<5)) != 0);
            printf("    ERX_P4: %d\n", (value & (1<<4)) != 0);
            printf("    ERX_P3: %d\n", (value & (1<<3)) != 0);
            printf("    ERX_P2: %d\n", (value & (1<<2)) != 0);
            printf("    ERX_P1: %d\n", (value & (1<<1)) != 0);
            printf("    ERX_P0: %d\n", (value & (1<<0)) != 0);
            break;
        case SETUP_AW:
            printf("SETUP_AW: 0x%x\n", value);
            printf("    5_bytes: %d\n", value == 3);
            printf("    4_bytes: %d\n", value == 2);
            printf("    3_bytes: %d\n", value == 1);
            break;
        case SETUP_RETR:
            printf("SETUP_RETR: 0x%x\n", value);
            printf("    Auto Retransmit Delay: %dus\n", (value & 0xf0)*250);
            printf("    Auto Retransmit Count: %d\n", value & 0xf);
            break;
        case RF_CH:
            printf("RF_CH: 0x%x\n", value);
            printf("    RF_CH: %d\n", value & 0x3f);
            break;
        case RF_SETUP:
            printf("RF_SETUP: 0x%x\n", value);
            printf("    PLL_LOCK: %d\n", (value & (1<<4)) != 0);
            printf("    Air Data Rate: 1 + %d mbps\n", (value & (1<<3)) != 0);
            printf("    RF_PWR (0-3): %d\n", ((value>>1)&0x3));
            printf("    Setup LNA gain: %d\n", (value & (1<<0)) != 0);
            break;
        case STATUS:
            printf("STATUS: 0x%x\n", value);
            printf("    Data Ready RX: %d\n", (value & (1<<6)) != 0);
            printf("    TX Data Sent : %d\n", (value & (1<<5)) != 0);
            printf("    MAX_RT (must be 0 to ): %d\n", (value & (1<<4)) != 0);
            printf("    Data Pipe Number (7==empty): %d\n", ((value>>1)&0x7));
            printf("    TX_FULL: %d\n", (value & (1<<0)) != 0);
            break;
        case OBSERVE_TX:
            printf("OBSERVE_TX: 0x%x\n", value);
            printf("    PLOS_CNT: %d\n", value & 0xf0);
            printf("    ARC_CNT: %d\n", value & 0xf);
            break;
        case CD:
            printf("CD\n");
            printf("    Reserved.\n");
            break;
        case RX_ADDR_P0:
            printf("RX_ADDR_P0\n");
            printf("    addr: 0x%x\n", value);
            break;
        case RX_ADDR_P1:
            printf("RX_ADDR_P1\n");
            printf("    addr: 0x%x\n", value);
            break;
        case RX_ADDR_P2:
            printf("RX_ADDR_P2\n");
            printf("    addr: 0x%x\n", value);
            break;
        case RX_ADDR_P3:
            printf("RX_ADDR_P3\n");
            printf("    addr: 0x%x\n", value);
            break;
        case RX_ADDR_P4:
            printf("RX_ADDR_P4\n");
            printf("    addr: 0x%x\n", value);
            break;
        case RX_ADDR_P5:
            printf("RX_ADDR_P5\n");
            printf("    addr: 0x%x\n", value);
            break;
        case TX_ADDR:
            printf("TX_ADDR\n");
            printf("    addr: 0x%x\n", value);
            break;
        case RX_PW_P0:
            printf("RX_PW_P0\n");
            printf("    payload width: %d\n", value & 0x3f);
            break;
        case RX_PW_P1:
            printf("RX_PW_P1\n");
            printf("    payload width: %d\n", value & 0x3f);
            break;
        case RX_PW_P2:
            printf("RX_PW_P2\n");
            printf("    payload width: %d\n", value & 0x3f);
            break;
        case RX_PW_P3:
            printf("RX_PW_P3\n");
            printf("    payload width: %d\n", value & 0x3f);
            break;
        case RX_PW_P4:
            printf("RX_PW_P4\n");
            printf("    payload width: %d\n", value & 0x3f);
            break;
        case RX_PW_P5:
            printf("RX_PW_P5\n");
            printf("    payload width: %d\n", value & 0x3f);
            break;
        case FIFO_STATUS:
            printf("FIFO_STATUS: 0x%x\n", value);
            printf("    TX_REUSE: %d\n", (value & (1<<6)) != 0);
            printf("    TX_FULL: %d\n", (value & (1<<5)) != 0);
            printf("    TX_EMPTY: %d\n", (value & (1<<4)) != 0);
            printf("    RX_FULL: %d\n", (value & (1<<1)) != 0);
            printf("    RX_EMPTY: %d\n", (value & (1<<0)) != 0);
            break;
        default:
            printf("Unknown register: %d\n", reg_addr);
    }
}

void test(int fd, struct gpiod_line* ce) {
    uint8_t tx_buffer[2], rx_buffer[2];
    for(int i = 0x00; i <= 0x17; i++){
        tx_buffer[0] = R_REGISTER | i;
        tx_buffer[1] = 0xff;// no op
        _spi_transfer(fd, tx_buffer, rx_buffer, 2);
        print_register(i, rx_buffer[1]);
        usleep(10000);
    }
}

void power_down(int fd, struct gpiod_line* ce){
    uint8_t tx_buffer[2], rx_buffer[2];
    tx_buffer[0] = W_REGISTER | CONFIG;
    tx_buffer[1] = 0x78;// value
    _spi_transfer(fd, tx_buffer, rx_buffer, 2);
    _gpio_low(ce);// de-activate 
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
    power_down(fd, ce);
    if (ce)
        gpiod_line_release(ce);
    if (chip)
        gpiod_chip_close(chip);
    close(fd);
    return EXIT_SUCCESS;
}