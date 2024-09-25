'''
    Author: Christopher Stewart (Christopher.ray.stewart@gmail.com)
    Date: 23092024
    Description: NRF24L01 rpi pico micropython program
'''
import sys
import utime
from machine import Pin, SPI
from micropython import const


R_REGISTER=0x00
W_REGISTER=0x20

NRF_CONFIG = 0x00
MASK_RX_DR  =1
MASK_TX_DS  =1
MASK_MAX_RT =1
EN_CRC      =1
CRCO        =1
PWR_UP      =1
PRIM_RX     =1

EN_AA       =0x01
ENAA_P5      =0
ENAA_P4      =0
ENAA_P3      =0
ENAA_P2      =0
ENAA_P1      =0
ENAA_P0      =0

EN_RXADDR   =0x02
ERX_P5      =0
ERX_P4      =0
ERX_P3      =0
ERX_P2      =0
ERX_P1      =0
ERX_P0      =1

SETUP_AW    =0x03
AW_3_BYTES   =1
AW_4_BYTES   =0
AW_5_BYTES   =0

SETUP_RETR = 0x04
ARD = 0b1111 << 4
ARC = 0b0

RF_CH = 0x05
CHANNEL = 2

RF_SETUP = 0x06
RF_DR = 1 << 3
RF_PWR = 0b11 << 1
LNA = 0

RX_ADDR_P0      =0x0A
RX_ADDR_P0_BUFFER = [0xc4]*(5*AW_5_BYTES + 4*AW_4_BYTES + 3*AW_3_BYTES)

RX_PW_P0        =0x11
P0_PACKET_SIZE  =32

TX_ADDR     =0x10

W_TX_PAYLOAD   =0xA0
R_RX_PAYLOAD   =0x61
RF24_NOP      =0xFF
FLUSH_TX      =0xE1
FLUSH_RX      =0xE2

FIFO_STATUS = 0x17

if sys.platform != "rp2":  # Hardware SPI with explicit pin definitions
    raise ValueError("Unsupported platform {}".format(sys.platform))


BAUDRATE = 8*10**6

spi0 = SPI(0, baudrate=BAUDRATE, sck=Pin(2), mosi=Pin(3), miso=Pin(4))
cfg0 = {"spi": spi0, "csn": Pin(1, mode=Pin.OUT, value=1) , "ce": Pin(0, mode=Pin.OUT, value=0)}

spi1 = SPI(1, baudrate=BAUDRATE, sck=Pin(10), mosi=Pin(11), miso=Pin(12))
cfg1 = {"spi": spi1, "csn": Pin(13, mode=Pin.OUT, value=1) , "ce": Pin(14, mode=Pin.OUT, value=0)}

def spi_transfer(cfg, write_bytes):
    spi, csn = cfg["spi"], cfg["csn"]
    read_bytes = bytearray(len(write_bytes))
    #print(read_bytes, write_bytes)
    csn.off()
    spi.write_readinto(write_bytes, read_bytes)
    csn.on()
    return read_bytes
    
def flush(cfg):
    spi_transfer(cfg, bytes([RF24_NOP]))
                 
def nrf24_init(cfg):
    spi_transfer(cfg, bytes([W_REGISTER | NRF_CONFIG, (MASK_RX_DR<<6) + (MASK_TX_DS<<5) + (MASK_MAX_RT<<4) + (EN_CRC<<3) + (CRCO<<2) + (PWR_UP<<1) + (PRIM_RX<<0)]))
    utime.sleep_us(1500) # power up
    spi_transfer(cfg, bytes([W_REGISTER | EN_AA, (ENAA_P5<<5) + (ENAA_P4<<4) + (ENAA_P3<<3) + (ENAA_P2<<2) + (ENAA_P1<<1) + (ENAA_P0<<0)]))
    spi_transfer(cfg, bytes([W_REGISTER | EN_RXADDR, (ERX_P5<<5) + (ERX_P4<<4) + (ERX_P3<<3) + (ERX_P2<<2) + (ERX_P1<<1) + (ERX_P0<<0)]))
    spi_transfer(cfg, bytes([W_REGISTER | SETUP_AW, AW_3_BYTES + AW_4_BYTES*2 + AW_5_BYTES*3]))
    spi_transfer(cfg, bytes([W_REGISTER | SETUP_RETR, ARD | ARC]))
    spi_transfer(cfg, bytes([W_REGISTER | RF_CH, CHANNEL]))
    spi_transfer(cfg, bytes([W_REGISTER | RF_SETUP, RF_DR|RF_PWR|LNA]))
    spi_transfer(cfg, bytes([W_REGISTER | RX_ADDR_P0] + RX_ADDR_P0_BUFFER))
    spi_transfer(cfg, bytes([W_REGISTER | TX_ADDR] + RX_ADDR_P0_BUFFER))
    spi_transfer(cfg, bytes([W_REGISTER | RX_PW_P0, P0_PACKET_SIZE]))
    spi_transfer(cfg, bytes([W_REGISTER | NRF_CONFIG, (MASK_RX_DR<<6) + (MASK_TX_DS<<5) + (MASK_MAX_RT<<4) + (EN_CRC<<3) + (CRCO<<2) + (PWR_UP<<1) + (PRIM_RX<<0)]))
    flush(cfg)

def nrf24_enable(cfg):
    # must be enabled constantly to receive
    # can remain disabled and pulse enable/disable to transmit only
    ce = cfg["ce"]
    ce.on()
    utime.sleep_us(130)
    
def nrf24_disable(cfg):
    ce = cfg["ce"]
    ce.off()
    #utime.sleep_us(130)

def nrf24_tx_mode(cfg):
    config = spi_transfer(cfg, bytes([R_REGISTER | NRF_CONFIG, RF24_NOP]))[1]
    if config & 1:
        spi_transfer(cfg, bytes([W_REGISTER | NRF_CONFIG, config-1]))

def nrf24_rx_mode(cfg):
    config = spi_transfer(cfg, bytes([R_REGISTER | NRF_CONFIG, RF24_NOP]))[1]
    spi_transfer(cfg, bytes([W_REGISTER | NRF_CONFIG, config | 1]))
    
def nrf24_tx(cfg, write_bytes):
    assert len(write_bytes) == P0_PACKET_SIZE
    buffer = bytes([W_TX_PAYLOAD]) + write_bytes
    spi_transfer(cfg, buffer)
    
    STATUS = spi_transfer(cfg, bytes([R_REGISTER | FIFO_STATUS, RF24_NOP]))[1]
    TX_EMPTY = STATUS & 0x10
    if TX_EMPTY:
        print("tx empty while trying to send")
    # pulse to transmit
    nrf24_enable(cfg)
    nrf24_disable(cfg)

def nrf24_rx(cfg):
    STATUS = spi_transfer(cfg, bytes([R_REGISTER | FIFO_STATUS, RF24_NOP]))[1]
    #print(cfg)
    #print_register("FIFO_STATUS", STATUS)
    RX_EMPTY = STATUS & 1
    if not RX_EMPTY:
        data = spi_transfer(cfg, bytes([R_RX_PAYLOAD] + [RF24_NOP]*P0_PACKET_SIZE))
        print(data)
        return data
    #else:
        #print("RX_EMPTY")
    return None

def nrf24_check(cfg):
    register_addresses = [
        "CONFIG",
        "EN_AA",
        "EN_RXADDR",
        "SETUP_AW",
        "SETUP_RETR",
        "RF_CH",
        "RF_SETUP",
        "STATUS",
        "OBSERVE_TX",
        "CD",
        "RX_ADDR_P0",
        "RX_ADDR_P1",
        "RX_ADDR_P2",
        "RX_ADDR_P3",
        "RX_ADDR_P4",
        "RX_ADDR_P5",
        "TX_ADDR",
        "RX_PW_P0",
        "RX_PW_P1",
        "RX_PW_P2",
        "RX_PW_P3",
        "RX_PW_P4",
        "RX_PW_P5",
        "FIFO_STATUS"  
    ]
    print(cfg)
    for i, addr in enumerate(register_addresses):
        cmd = bytes([R_REGISTER | i, RF24_NOP])
        res = spi_transfer(cfg, cmd)
        print_register(addr, res[1])
    print("")

def print_register(reg_addr, value):
    if reg_addr == "CONFIG":
        print("CONFIG: 0x{:x}".format(value))
        print("  MASK_RX_DR (irq disabled): {}".format((value & (1 << 6)) != 0))
        print("  MASK_TX_DS (irq disabled): {}".format((value & (1 << 5)) != 0))
        print("  MASK_MAX_RT (irq disabled): {}".format((value & (1 << 4)) != 0))
        print("  EN_CRC: {}".format((value & (1 << 3)) != 0))
        print("  CRCO: 1+{:d} bytes".format((value & (1 << 2)) != 0))
        print("  PWR_UP: {}".format((value & (1 << 1)) != 0))
        print("  PRIM_RX: {}".format((value & (1 << 0)) != 0))
    elif reg_addr == "EN_AA":
        print("EN_AA: 0x{:x} (auto acknowledgement)".format(value))
        print("  ENAA_P5: {}".format((value & (1 << 5)) != 0))
        print("  ENAA_P4: {}".format((value & (1 << 4)) != 0))
        print("  ENAA_P3: {}".format((value & (1 << 3)) != 0))
        print("  ENAA_P2: {}".format((value & (1 << 2)) != 0))
        print("  ENAA_P1: {}".format((value & (1 << 1)) != 0))
        print("  ENAA_P0: {}".format((value & (1 << 0)) != 0))
    elif reg_addr == "EN_RXADDR":
        print("EN_RXADDR: 0x{:x}".format(value))
        print("  ERX_P5: {}".format((value & (1 << 5)) != 0))
        print("  ERX_P4: {}".format((value & (1 << 4)) != 0))
        print("  ERX_P3: {}".format((value & (1 << 3)) != 0))
        print("  ERX_P2: {}".format((value & (1 << 2)) != 0))
        print("  ERX_P1: {}".format((value & (1 << 1)) != 0))
        print("  ERX_P0: {}".format((value & (1 << 0)) != 0))
    elif reg_addr == "SETUP_AW":
        print("SETUP_AW: 0x{:x} (addr width)".format(value))
        print("  5_bytes: {}".format(value == 3))
        print("  4_bytes: {}".format(value == 2))
        print("  3_bytes: {}".format(value == 1))
    elif reg_addr == "SETUP_RETR":
        print("SETUP_RETR: 0x{:x}".format(value))
        print("  Auto Retransmit Delay: {:d}us".format((value & 0xf0)*250))
        print("  Auto Retransmit Count: {:d}".format(value & 0xf))
    elif reg_addr == "RF_CH":
        print("RF_CH: 0x{:x}".format(value))
        print("  RF_CH: {:d}".format(value & 0x3f))
    elif reg_addr == "RF_SETUP":
        print("RF_SETUP: 0x{:x}".format(value))
        print("  PLL_LOCK: {}".format((value & (1 << 4)) != 0))
        print("  Air Data Rate: 1 + {:d} mbps".format((value & (1 << 3)) != 0))
        print("  RF_PWR (0-3): {:d}".format(((value >> 1) & 0x3)))
        print("  Setup LNA gain: {}".format((value & (1 << 0)) != 0))
    elif reg_addr == "STATUS":
        print("STATUS: 0x{:x}".format(value))
        print("  Data Ready RX: {}".format((value & (1 << 6)) != 0))
        print("  TX Data Sent : {}".format((value & (1 << 5)) != 0))
        print("  MAX_RT (must be 0 to ): {}".format((value & (1 << 4)) != 0))
        print("  RX Data Pipe Ready Number (7==empty): {:d}".format(((value >> 1) & 0x7)))
        print("  TX_FULL: {}".format((value & (1 << 0)) != 0))
    elif reg_addr == "OBSERVE_TX":
        print("OBSERVE_TX: 0x{:x}".format(value))
        print("  PLOS_CNT: {:d}".format(value & 0xf0))
        print("  ARC_CNT: {:d}".format(value & 0xf))
    elif reg_addr == "CD":
        print("CD")
        print("  Reserved.")
    elif reg_addr == "RX_ADDR_P0":
        print("RX_ADDR_P0")
        print("  addr: 0x{:x}".format(value))
    elif reg_addr == "RX_ADDR_P1":
        print("RX_ADDR_P1")
        print("  addr: 0x{:x}".format(value))
    elif reg_addr == "RX_ADDR_P2":
        print("RX_ADDR_P2")
        print("  addr: 0x{:x}".format(value))
    elif reg_addr == "RX_ADDR_P3":
        print("RX_ADDR_P3")
        print("  addr: 0x{:x}".format(value))
    elif reg_addr == "RX_ADDR_P4":
        print("RX_ADDR_P4")
        print("  addr: 0x{:x}".format(value))
    elif reg_addr == "RX_ADDR_P5":
        print("RX_ADDR_P5")
        print("  addr: 0x{:x}".format(value))
    elif reg_addr == "TX_ADDR":
        print("TX_ADDR")
        print("  addr: 0x{:x}".format(value))
    elif reg_addr == "RX_PW_P0":
        print("RX_PW_P0")
        print("  payload width: {:d}".format(value & 0x3f))
    elif reg_addr == "RX_PW_P1":
        print("RX_PW_P1")
        print("  payload width: {:d}".format(value & 0x3f))
    elif reg_addr == "RX_PW_P2":
        print("RX_PW_P2")
        print("  payload width: {:d}".format(value & 0x3f))
    elif reg_addr == "RX_PW_P3":
        print("RX_PW_P3")
        print("  payload width: {:d}".format(value & 0x3f))
    elif reg_addr == "RX_PW_P4":
        print("RX_PW_P4")
        print("  payload width: {:d}".format(value & 0x3f))
    elif reg_addr == "RX_PW_P5":
        print("RX_PW_P5")
        print("  payload width: {:d}".format(value & 0x3f))
    elif reg_addr == "FIFO_STATUS":
        print("FIFO_STATUS: 0x{:x}".format(value))
        print("  TX_REUSE: {}".format((value & (1 << 6)) != 0))
        print("  TX_FULL: {}".format((value & (1 << 5)) != 0))
        print("  TX_EMPTY: {}".format((value & (1 << 4)) != 0))
        print("  RX_FULL: {}".format((value & (1 << 1)) != 0))
        print("  RX_EMPTY: {}".format((value & (1 << 0)) != 0))
    else:
        print("Unknown register: {}".format(reg_addr))

    
def main():
    nrf24_init(cfg0)
    nrf24_rx_mode(cfg0)
    #nrf24_check(cfg0)
    nrf24_enable(cfg0)
    #return
    
    nrf24_init(cfg1)
    nrf24_tx_mode(cfg1)
    #nrf24_check(cfg1)   
    #return
    
    for i in range(16):
        data = bytes([i for j in range(P0_PACKET_SIZE)])
        print("sending")
        nrf24_tx(cfg1, data)
        
        utime.sleep_ms(100)
        data = nrf24_rx(cfg0)

                
        
main()

