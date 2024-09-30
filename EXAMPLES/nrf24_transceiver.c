/**
 * Author: Christopher Stewart (Christopher.ray.stewart@gmail.com)
 * Date: 28092024
 * Description: NRF24L01 rpi program
 * 
 * gcc -o nrf24_transceiver nrf24_transceiver.c -lgpiod
 * 
 * ./nrf24_transceiver
 * 
 */

#include <gpiod.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include "nrf24_config.h"


struct NRF24_STATUS_DATA {
    bool RX_DR, TX_DS, MAX_RT, TX_FULL, TX_EMPTY, RX_EMPTY, RX_FULL;
    int RX_P_NO, ARC_CNT, PLOS_CNT, STATUS_BYTE; 
};

void print_status_data(struct NRF24_STATUS_DATA status) {
    printf("RX_DR: %d\n", status.RX_DR);
    printf("TX_DS: %d\n", status.TX_DS);
    printf("MAX_RT: %d\n", status.MAX_RT);
    printf("TX_FULL: %d\n", status.TX_FULL);
    printf("TX_EMPTY: %d\n", status.TX_EMPTY);
    printf("RX_EMPTY: %d\n", status.RX_EMPTY);
    printf("RX_FULL: %d\n", status.RX_FULL);
    printf("RX_P_NO: %d\n", status.RX_P_NO);
    printf("ARC_CNT: %d\n", status.ARC_CNT);
    printf("PLOS_CNT: %d\n", status.PLOS_CNT);
    printf("STATUS_BYTE: %d\n", status.STATUS_BYTE);
}

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

void nrf24_enable(int fd, struct gpiod_line* ce){
    _gpio_high(ce);
    usleep(130);
}

void nrf24_disable(int fd, struct gpiod_line* ce){
    _gpio_low(ce);
}

void nrf24_flush_rx(int fd, struct gpiod_line* ce){
    uint8_t tx_buffer[] = {W_REGISTER | FLUSH_RX};
	uint8_t rx_buffer[] = {0};
    _spi_transfer(fd, tx_buffer, rx_buffer, 1);
}

void nrf24_flush_tx(int fd, struct gpiod_line* ce){
    uint8_t tx_buffer[] = {W_REGISTER | FLUSH_TX};
	uint8_t rx_buffer[] = {0};
    _spi_transfer(fd, tx_buffer, rx_buffer, 1);
    usleep(130);
}


struct NRF24_STATUS_DATA nrf24_status(int fd, struct gpiod_line* ce) {
    struct NRF24_STATUS_DATA status_data = {0};  // Initialize status structure to zero
    uint8_t tx_data[3];  // Commands to send
    uint8_t rx_data[3];  // Data received from SPI

    // Prepare the commands to read STATUS, FIFO_STATUS, and OBSERVE_TX
    tx_data[0] = R_REGISTER | FIFO_STATUS;
    tx_data[1] = R_REGISTER | OBSERVE_TX;
    tx_data[2] = RF24_NOP;  // Send NOP command to just read status

    // Transfer data over SPI and get the status, FIFO status, and observe TX
    _spi_transfer(fd, tx_data, rx_data, 3);

    // Parse the STATUS register (rx_data[0])
    uint8_t status = rx_data[0];
    status_data.STATUS_BYTE = status;
    status_data.RX_DR = (status >> 6) & 1;
    status_data.TX_DS = (status >> 5) & 1;
    status_data.MAX_RT = (status >> 4) & 1;
    status_data.RX_P_NO = (status >> 1) & 0b111;  // 3 bits for RX_P_NO
    status_data.TX_FULL = status & 1;

    // Parse the FIFO_STATUS register (rx_data[1])
    uint8_t fifo_status = rx_data[1];
    status_data.TX_FULL = (fifo_status >> 5) & 1;
    status_data.TX_EMPTY = (fifo_status >> 4) & 1;
    status_data.RX_FULL = (fifo_status >> 1) & 1;
    status_data.RX_EMPTY = fifo_status & 1;

    // Parse the OBSERVE_TX register (rx_data[2])
    uint8_t observe_tx = rx_data[2];
    status_data.ARC_CNT = observe_tx & 0x0F;        // ARC_CNT is the lower 4 bits
    status_data.PLOS_CNT = (observe_tx >> 4) & 0x0F; // PLOS_CNT is the upper 4 bits

    return status_data;
}

void init(int fd, struct gpiod_line* ce) {
    uint8_t tx_buffer[33] = {RF24_NOP};
	uint8_t rx_buffer[33] = {0};

    nrf24_disable(fd, ce);
    nrf24_enable(fd, ce);

	tx_buffer[0] = W_REGISTER | NRF_CONFIG;
    tx_buffer[1] = MASK_RX_DR + MASK_TX_DS + MASK_MAX_RT + EN_CRC + CRCO + PWR_UP + PRIM_RX;
    _spi_transfer(fd, tx_buffer, rx_buffer, 2);
	usleep(1500);

    tx_buffer[0] = W_REGISTER | EN_AA;
    tx_buffer[1] = ENAA_P5 + ENAA_P4 + ENAA_P3 + ENAA_P2 + ENAA_P1 + ENAA_P0;
    _spi_transfer(fd, tx_buffer, rx_buffer, 2);

    tx_buffer[0] = W_REGISTER | EN_RXADDR;
    tx_buffer[1] = ERX_P5 + ERX_P4 + ERX_P3 + ERX_P2 + ERX_P1 + ERX_P0;
    _spi_transfer(fd, tx_buffer, rx_buffer, 2);

    tx_buffer[0] = W_REGISTER | SETUP_AW;
    tx_buffer[1] = AW_3_BYTES + AW_4_BYTES*2 + AW_5_BYTES*3;
    _spi_transfer(fd, tx_buffer, rx_buffer, 2);

    tx_buffer[0] = W_REGISTER | SETUP_RETR;
    tx_buffer[1] = ARD | ARC;
    _spi_transfer(fd, tx_buffer, rx_buffer, 2);

    tx_buffer[0] = W_REGISTER | RF_CH;
    tx_buffer[1] = CHANNEL;
    _spi_transfer(fd, tx_buffer, rx_buffer, 2);

    tx_buffer[0] = W_REGISTER | RF_SETUP;
    tx_buffer[1] = RF_DR|RF_PWR|LNA;
    _spi_transfer(fd, tx_buffer, rx_buffer, 2);

    tx_buffer[0] = W_REGISTER | RX_ADDR_P0;
	for(int i = 1; i < 1+ADDRESS_WIDTH; i++){
		tx_buffer[i] = RX_ADDR_P0_BUFFER[i-1];
        printf("0x%x RX_ADDR_P0_BUFFER[i-1]\n", RX_ADDR_P0_BUFFER[i-1]);
	}// set rx addr to 0x...
    _spi_transfer(fd, tx_buffer, rx_buffer, ADDRESS_WIDTH+1);

    // read addr to confirm SPI working
    tx_buffer[0] = R_REGISTER | RX_ADDR_P0;
    for(int i = 1; i < 1+ADDRESS_WIDTH; i++){
		tx_buffer[i] = RF24_NOP;
	}
    _spi_transfer(fd, tx_buffer, rx_buffer, ADDRESS_WIDTH+1);
    for(int i = 1; i < 1+ADDRESS_WIDTH; i++){
        if(rx_buffer[i] != RX_ADDR_P0_BUFFER[i-1]){
            printf("i:%d, buffer: 0x%x, config: 0x%x. RX_ADDR_P0 incorrect.\n", i, rx_buffer[i], RX_ADDR_P0_BUFFER[i-1]);
        }
	}

    tx_buffer[0] = W_REGISTER | TX_ADDR;
	for(int i = 1; i < 7; i++){
		tx_buffer[i] = RX_ADDR_P0_BUFFER[i-1];
	}// set tx addr to 0x...
    _spi_transfer(fd, tx_buffer, rx_buffer, ADDRESS_WIDTH+1);

    tx_buffer[0] = W_REGISTER | RX_PW_P0;
    tx_buffer[1] = P0_PACKET_SIZE;
    _spi_transfer(fd, tx_buffer, rx_buffer, 2);

    nrf24_flush_tx(fd, ce);
    nrf24_flush_rx(fd, ce);


    struct NRF24_STATUS_DATA status = nrf24_status(fd, ce);
    print_status_data(status);
}

void nrf24_tx_mode(int fd, struct gpiod_line* ce){
    nrf24_disable(fd, ce);
    uint8_t tx_buffer[2] = {R_REGISTER | NRF_CONFIG, RF24_NOP};
    uint8_t rx_buffer[2];
    uint8_t config = rx_buffer[1];
    config = config & 1 ? config -1 : config;
    tx_buffer[0] = W_REGISTER | NRF_CONFIG;
    tx_buffer[1] = RF24_NOP;
    _spi_transfer(fd, tx_buffer, rx_buffer, 2);
    nrf24_enable(fd, ce);
}

void nrf24_rx_mode(int fd, struct gpiod_line* ce){
    nrf24_disable(fd, ce);
    uint8_t tx_buffer[2] = {R_REGISTER | NRF_CONFIG, RF24_NOP};
    uint8_t rx_buffer[2];
    uint8_t config = rx_buffer[1];
    config |= 1;
    tx_buffer[0] = W_REGISTER | NRF_CONFIG;
    tx_buffer[1] = RF24_NOP;
    _spi_transfer(fd, tx_buffer, rx_buffer, 2);
    nrf24_enable(fd, ce);
}

bool nrf24_tx(int fd, struct gpiod_line* ce, char* string, int string_length){
    uint8_t tx_buffer[33] = {RF24_NOP};
	uint8_t rx_buffer[33] = {0};
    tx_buffer[0] = W_TX_PAYLOAD;
    // clamp and write data to send
    string_length = string_length < P0_PACKET_SIZE+1 ? string_length : P0_PACKET_SIZE+1;
    for(int i = 0; i < string_length; i++){
		tx_buffer[i+1] = string[i];
	}
    // pad the remainder of the packet
    for(int i = string_length; i < 33; i++){
		tx_buffer[i] = 0;
	}
    nrf24_flush_tx(fd, ce);
    nrf24_tx_mode(fd, ce);

    // queue packet for transmission
    _spi_transfer(fd, tx_buffer, rx_buffer, 33);

    // wait for ACK response
    struct NRF24_STATUS_DATA status;
    bool success = false;
    while(1){
        status = nrf24_status(fd, ce);
        if(status.TX_DS){
            success = true;
            break;
        }
        if(status.MAX_RT){
            success = false;
            break;
        }
        usleep(500);
    }

    // write STATUS to reset IRQ bits
    tx_buffer[0] = W_REGISTER | STATUS;
    tx_buffer[1] = status.STATUS_BYTE;
    _spi_transfer(fd, tx_buffer, rx_buffer, 2);

    nrf24_rx_mode(fd, ce);
    return success;
}

void nrf24_rx(int fd, struct gpiod_line* ce, uint8_t* rx_buffer){
    uint8_t tx_buffer[33] = {RF24_NOP};
    tx_buffer[0] = R_RX_PAYLOAD;
    _spi_transfer(fd, tx_buffer, rx_buffer, P0_PACKET_SIZE+1);
}


void nrf24_read(int fd, struct gpiod_line* ce, uint8_t* rx_buffer){
    uint8_t tx_buffer[33] = {RF24_NOP};
    struct NRF24_STATUS_DATA status;
    while(1){
        status = nrf24_status(fd, ce);
        if(status.RX_P_NO < 7){
            // write STATUS to reset IRQ bits
            tx_buffer[0] = W_REGISTER | STATUS;
            tx_buffer[1] = status.STATUS_BYTE;
            _spi_transfer(fd, tx_buffer, rx_buffer, 2);

            nrf24_rx(fd, ce, rx_buffer);
            usleep(100);
            return;
        }
        usleep(5000);
    }
}


// TODO bind transceiveers (master brooadcaastts to neggottiaatte ch+p0 addr)

// TODO reg 0x09 CD to check RSS/reception to negotiate channel changes

int main() {
    struct gpiod_chip* chip;
    struct gpiod_line* ce;
    init_gpio(&chip, &ce);
    int fd; // SPI file descriptor
    init_spi(&fd);
	init(fd, ce);

    uint8_t rx_buffer[33] = {0};
    while (1) { 
        nrf24_read(fd, ce, rx_buffer);
    }

    if (ce)
        gpiod_line_release(ce);
    if (chip)
        gpiod_chip_close(chip);
    close(fd);
    return EXIT_SUCCESS;
}