#ifndef NRF24_DEFINES_H
#define NRF24_DEFINES_H

#include <stdint.h>

#define GPIO_CHIP_NAME "gpiochip4" // gpiochip0 or gpiochip4 on rpi5
#define GPIO_OFFSET_CE 25 // Chip Enable Activates RX or TX mode

#define SPI_DEVICE "/dev/spidev0.1" // spi0 (spidev0.0) or spi1 (spidev0.1)
#define SPI_HZ 8000000

// Register Addresses
#define R_REGISTER    0x00
#define W_REGISTER    0x20

#define NRF_CONFIG    0x00
#define MASK_RX_DR    (1 << 6) // Bit 6
#define MASK_TX_DS    (1 << 5) // Bit 5
#define MASK_MAX_RT   (1 << 4) // Bit 4
#define EN_CRC        (1 << 3) // Bit 3
#define CRCO          (1 << 2) // Bit 2
#define PWR_UP        (1 << 1) // Bit 1
#define PRIM_RX       (1 << 0) // Bit 0

#define EN_AA         0x01
#define ENAA_P5       (0 << 5)
#define ENAA_P4       (0 << 4)
#define ENAA_P3       (0 << 3)
#define ENAA_P2       (0 << 2)
#define ENAA_P1       (0 << 1)
#define ENAA_P0       (1 << 0)

#define EN_RXADDR     0x02
#define ERX_P5        (0 << 5)
#define ERX_P4        (0 << 4)
#define ERX_P3        (0 << 3)
#define ERX_P2        (0 << 2)
#define ERX_P1        (0 << 1)
#define ERX_P0        (1 << 0)

#define SETUP_AW      0x03
#define AW_3_BYTES    1
#define AW_4_BYTES    0
#define AW_5_BYTES    0

#define SETUP_RETR    0x04
#define ARD           (2 << 4)  // Auto Retransmit Delay: 0-15 << 4
#define ARC           15        // Auto Retransmit Count: 0-15

#define RF_CH         0x05
#define CHANNEL       2         // RF Channel (0-63)

#define RF_SETUP      0x06
#define RF_DR         (1 << 3)          // Data Rate (0(1mbps) or 1(2mbps))
#define RF_PWR        (0b11 << 1)       // Power Amplifier Level (0-3)
#define LNA           (1 << 0)          // Low Noise Amplifier (0-1)

#define STATUS        0x07
#define OBSERVE_TX    0x08

// Address Registers
#define RX_ADDR_P0    0x0A
#define RX_ADDR_P1    0x0B
#define RX_ADDR_P2    0x0C
#define RX_ADDR_P3    0x0D
#define RX_ADDR_P4    0x0E
#define RX_ADDR_P5    0x0F
#define TX_ADDR       0x10

// Define Address Buffers based on Address Width (AW)
#if AW_3_BYTES
    #define ADDRESS_WIDTH 3
#elif AW_4_BYTES
    #define ADDRESS_WIDTH 4
#elif AW_5_BYTES
    #define ADDRESS_WIDTH 5
#else
    #define ADDRESS_WIDTH 3 // Default to 3 bytes if not defined
#endif

const uint8_t RX_ADDR_P0_BUFFER[ADDRESS_WIDTH] = {0xDD, 0xAA, 0x33};
const uint8_t RX_ADDR_P1_BUFFER[ADDRESS_WIDTH] = {0xA5, 0xA5, 0xA5};
const uint8_t RX_ADDR_P2_BUFFER[ADDRESS_WIDTH] = {0xB6, 0xB6, 0xB6};
const uint8_t RX_ADDR_P3_BUFFER[ADDRESS_WIDTH] = {0xC7, 0xC7, 0xC7};
const uint8_t RX_ADDR_P4_BUFFER[ADDRESS_WIDTH] = {0xD8, 0xD8, 0xD8};
const uint8_t RX_ADDR_P5_BUFFER[ADDRESS_WIDTH] = {0xE9, 0xE9, 0xE9};
const uint8_t TX_ADDR_BUFFER[ADDRESS_WIDTH]     = {0xDD, 0xAA, 0x33}; // Example TX address

// Payload Width Registers
#define RX_PW_P0      0x11
#define P0_PACKET_SIZE  32
#define RX_PW_P1      0x12
#define P1_PACKET_SIZE  P0_PACKET_SIZE
#define RX_PW_P2      0x13
#define P2_PACKET_SIZE  P0_PACKET_SIZE
#define RX_PW_P3      0x14
#define P3_PACKET_SIZE  P0_PACKET_SIZE
#define RX_PW_P4      0x15
#define P4_PACKET_SIZE  P0_PACKET_SIZE
#define RX_PW_P5      0x16
#define P5_PACKET_SIZE  P0_PACKET_SIZE

// Instruction Mnemonics
#define W_TX_PAYLOAD  0xA0
#define R_RX_PAYLOAD  0x61
#define RF24_NOP      0xFF
#define FLUSH_TX      0xE1
#define FLUSH_RX      0xE2

#define FIFO_STATUS   0x17

#endif // NRF24_DEFINES_H
