/**
 * Author: Christopher Stewart (Christopher.ray.stewart@gmail.com)
 * Date: 09062024
 * Description: NRF24L01 program to receive and stdout 32 byte packets
 * 
 * gcc -o NRF24L01_RX NRF24L01_RX.c -lgpiod
 * 
 * ./NRF24L01_RX | ./defragment_4bit_grayscale | ./grayscale_4bit_to_16bit_RGB565 | ./ST7735S_LCD_stdin_stream
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

#define GPIO_CHIP_NAME 	"gpiochip0"         
#define GPIO_OFFSET_CE 	24                  // Chip Enable Activates RX or TX mode

#define SPI_DEVICE 		"/dev/spidev0.0"    
#define SPI_HZ 			10000000             

#define R_REGISTER 		0x00
#define W_REGISTER 		0x20

#define NRF_CONFIG  0x00
#define MASK_RX_DR  1
#define MASK_TX_DS  1
#define MASK_MAX_RT 1
#define EN_CRC      1
#define CRCO        0
#define PWR_UP      1
#define PRIM_RX     1

#define EN_AA       0x01
#define ENAA_P5      0
#define ENAA_P4      0
#define ENAA_P3      0
#define ENAA_P2      0
#define ENAA_P1      0
#define ENAA_P0      0

#define EN_RXADDR   0x02
#define ERX_P5      0
#define ERX_P4      0
#define ERX_P3      0
#define ERX_P2      0
#define ERX_P1      0
#define ERX_P0      1

#define SETUP_AW    0x03
#define AW_3_BYTES   0
#define AW_4_BYTES   0
#define AW_5_BYTES   1

#define RX_ADDR_P0      0x0A

#define RX_PW_P0        0x11
#define P0_PACKET_SIZE  32

#define R_RX_PAYLOAD    0x61
#define RF24_NOP      	0xFF
#define FLUSH_TX      0xE1
#define FLUSH_RX      0xE2

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

void flush(int fd, struct gpiod_line* ce){
    uint8_t tx_buffer[1] = {RF24_NOP};
	uint8_t rx_buffer[1] = {0};

    //tx_buffer[0] = FLUSH_TX;
    //_spi_transfer(fd, tx_buffer, rx_buffer, 1);

    tx_buffer[0] = FLUSH_RX;
    _spi_transfer(fd, tx_buffer, rx_buffer, 1);
}

void init(int fd, struct gpiod_line* ce) {
    uint8_t tx_buffer[33] = {RF24_NOP};
	uint8_t rx_buffer[33] = {0};

	tx_buffer[0] = W_REGISTER | NRF_CONFIG;
    tx_buffer[1] = (MASK_RX_DR<<6) + (MASK_TX_DS<<5) + (MASK_MAX_RT<<4) + (EN_CRC<<3) + (CRCO<<2) + (PWR_UP<<1) + (PRIM_RX<<0);
    _spi_transfer(fd, tx_buffer, rx_buffer, 2);
	usleep(1500);

    tx_buffer[0] = W_REGISTER | EN_AA;
    tx_buffer[1] = (ENAA_P5<<5) + (ENAA_P4<<4) + (ENAA_P3<<3) + (ENAA_P2<<2) + (ENAA_P1<<1) + (ENAA_P0<<0);
    _spi_transfer(fd, tx_buffer, rx_buffer, 2);

    tx_buffer[0] = W_REGISTER | EN_RXADDR;
    tx_buffer[1] = (ERX_P5<<5) + (ERX_P4<<4) + (ERX_P3<<3) + (ERX_P2<<2) + (ERX_P1<<1) + (ERX_P0<<0);
    _spi_transfer(fd, tx_buffer, rx_buffer, 2);

    tx_buffer[0] = W_REGISTER | RX_PW_P0;
    tx_buffer[1] = P0_PACKET_SIZE;
    _spi_transfer(fd, tx_buffer, rx_buffer, 2);

    tx_buffer[0] = W_REGISTER | RX_ADDR_P0;
	for(int i = 1; i < 7; i++){
		tx_buffer[i] = 0xC4;
	}// set rx addr to 0xC4C4C4C4C4
    _spi_transfer(fd, tx_buffer, rx_buffer, 6);

    flush(fd, ce);
}

void rx(int fd, struct gpiod_line* ce) {
    //_gpio_low(ce);
    uint8_t tx_buffer[33] = {RF24_NOP};
	uint8_t rx_buffer[33] = {0};
    static int counter = 0;
    // send NOP to read STATUS response
    tx_buffer[0] = RF24_NOP;
    _spi_transfer(fd, tx_buffer, rx_buffer, 33);

    bool RX_FIFO_EMPTY = rx_buffer[0] & 0xe;
    if(!RX_FIFO_EMPTY){// P0 received packet(s)
        tx_buffer[0] = R_RX_PAYLOAD;
        _spi_transfer(fd, tx_buffer, rx_buffer, 33);
        printf("%d,%d\n", rx_buffer[1], rx_buffer[2]);
        write(1, &(rx_buffer[1]), 32);
        counter++;
    }
    
    //_gpio_high(ce);// Activate 
    if(counter >= 341){
        counter = 0;
        usleep(10000);
    } else if(RX_FIFO_EMPTY){
        usleep(1000);
    }
}

int main() {
    struct gpiod_chip* chip;
    struct gpiod_line* ce;
    init_gpio(&chip, &ce);
    int fd; // SPI file descriptor
    init_spi(&fd);
	init(fd, ce);

    _gpio_high(ce);// Activate 
    usleep(130);
    while (1) { 
        rx(fd, ce);
    }
    if (ce)
        gpiod_line_release(ce);
    if (chip)
        gpiod_chip_close(chip);
    close(fd);
    return EXIT_SUCCESS;
}