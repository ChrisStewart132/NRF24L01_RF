'''
    Author: Christopher Stewart (Christopher.ray.stewart@gmail.com)
    Date: 26092024
    Description: NRF24L01 rpi pico micropython program testing 2 nrf24's data rate with AA enabled
        transceives on CHANNEL @ RX_ADDR_P0_BUFFER(5 byte addr) with P0_PACKET_SIZE
'''
import sys
import utime
from machine import Pin, SPI, Timer
from micropython import const


########################################################################################
# 											config      							   #
########################################################################################

R_REGISTER=0x00
W_REGISTER=0x20

NRF_CONFIG = 0x00
MASK_RX_DR  =0
MASK_TX_DS  =0
MASK_MAX_RT =0
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
ENAA_P0      =1

EN_RXADDR   =0x02
ERX_P5      =0
ERX_P4      =0
ERX_P3      =0
ERX_P2      =0
ERX_P1      =0
ERX_P0      =1

SETUP_AW    =0x03
AW_3_BYTES   =0
AW_4_BYTES   =0
AW_5_BYTES   =1

SETUP_RETR = 0x04
ARD = 2 << 4			# 0-15
ARC = 15					# 0-15

RF_CH = 0x05
CHANNEL = 2			# 0-63 channels

RF_SETUP = 0x06
RF_DR = 1 << 3			# 0-1
RF_PWR = 0b11 << 1
LNA = 1

STATUS = 0x07
OBSERVE_TX = 0x08

# addr written lsb first
# addr 1-5 = [0x..]+RX_ADDR_P0_BUFFER[1:]
# RX_ADDR_P0 recvs ACK, (rx mode sets TX to P0)
# TX sets addr 0-5
RX_ADDR_P0      =0x0A
RX_ADDR_P0_BUFFER = [0xdd, 0xaa, 0x33]+[0xc4]*(2*AW_5_BYTES + 1*AW_4_BYTES)
RX_ADDR_P1      =0x0B
RX_ADDR_P1_BUFFER = [0xa5]*(5*AW_5_BYTES + 4*AW_4_BYTES + 3*AW_3_BYTES)
RX_ADDR_P2      =0x0C
RX_ADDR_P2_BUFFER = [0xb6]*(5*AW_5_BYTES + 4*AW_4_BYTES + 3*AW_3_BYTES)
RX_ADDR_P3      =0x0D
RX_ADDR_P3_BUFFER = [0xc7]*(5*AW_5_BYTES + 4*AW_4_BYTES + 3*AW_3_BYTES)
RX_ADDR_P4      =0x0E
RX_ADDR_P4_BUFFER = [0xd8]*(5*AW_5_BYTES + 4*AW_4_BYTES + 3*AW_3_BYTES)
RX_ADDR_P5      =0x0F
RX_ADDR_P5_BUFFER = [0xe9]*(5*AW_5_BYTES + 4*AW_4_BYTES + 3*AW_3_BYTES)

TX_ADDR     =0x10

RX_PW_P0        =0x11
P0_PACKET_SIZE  =32
RX_PW_P1        =0x12
P1_PACKET_SIZE  =P0_PACKET_SIZE
RX_PW_P2        =0x13
P2_PACKET_SIZE  =P0_PACKET_SIZE
RX_PW_P3        =0x14
P3_PACKET_SIZE  =P0_PACKET_SIZE
RX_PW_P4        =0x15
P4_PACKET_SIZE  =P0_PACKET_SIZE
RX_PW_P5        =0x16
P5_PACKET_SIZE  =P0_PACKET_SIZE



W_TX_PAYLOAD   =0xA0
R_RX_PAYLOAD   =0x61
RF24_NOP      =0xFF
FLUSH_TX      =0xE1
FLUSH_RX      =0xE2

FIFO_STATUS = 0x17

#if sys.platform != "rp2":  # Hardware SPI with explicit pin definitions
    #raise ValueError("Unsupported platform {}".format(sys.platform))


BAUDRATE = 8*10**6

spi0 = SPI(0, baudrate=BAUDRATE, sck=Pin(2), mosi=Pin(3), miso=Pin(4))
cfg0 = {"spi": spi0, "csn": Pin(1, mode=Pin.OUT, value=1) , "ce": Pin(0, mode=Pin.OUT, value=0)}

spi1 = SPI(1, baudrate=BAUDRATE, sck=Pin(10), mosi=Pin(11), miso=Pin(12))
cfg1 = {"spi": spi1, "csn": Pin(13, mode=Pin.OUT, value=1) , "ce": Pin(14, mode=Pin.OUT, value=0)}

########################################################################################
# 											driver       							   #
########################################################################################

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
        print("  Auto Retransmit Delay: {:d}us".format(((value>>4)&0xf)*250+250))
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

def spi_transfer(cfg, write_bytes):
    spi, csn = cfg["spi"], cfg["csn"]
    read_bytes = bytearray(len(write_bytes))
    csn.off()
    spi.write_readinto(write_bytes, read_bytes)
    csn.on()
    return read_bytes

def nrf24_enable(cfg):
    ce = cfg["ce"]
    ce.on()
    utime.sleep_us(130)
    
def nrf24_disable(cfg):
    ce = cfg["ce"]
    ce.off()
    
def nrf24_flush_tx(cfg):
    spi_transfer(cfg, bytes([FLUSH_TX]))\
    #test
    status = nrf24_status(cfg)
    assert status["TX_FULL"] == 0 and status["TX_EMPTY"] == 1, f"flush tx test {status}" 

def nrf24_flush_rx(cfg):
    spi_transfer(cfg, bytes([FLUSH_RX]))
    #test
    status = nrf24_status(cfg)
    assert status["RX_FULL"] == 0 and status["RX_EMPTY"] == 1, "flush rx test" 

def nrf24_status(cfg):
    status_dict = {}
    status, fifo_status, observe_tx = spi_transfer(cfg, bytes([R_REGISTER | FIFO_STATUS, R_REGISTER | OBSERVE_TX, RF24_NOP]))
    status_dict["STATUS"] = status
    status_dict["RX_DR"] = status>>6&1
    status_dict["TX_DS"] = status>>5&1
    status_dict["MAX_RT"] = status>>4&1    
    status_dict["RX_P_NO"] = status>>1&7
    status_dict["TX_FULL"] = status&1
    
    status_dict["TX_FULL"] = fifo_status>>5&1
    status_dict["TX_EMPTY"] = fifo_status>>4&1
    status_dict["RX_FULL"] = fifo_status>>1&1
    status_dict["RX_EMPTY"] = fifo_status&1
    
    status_dict["ARC_CNT"] = observe_tx&0xf
    status_dict["PLOS_CNT"] = (observe_tx>>4)&0xf

    assert status>>7==0, "STATUS 7th bit must be 0 (RESERVED)"
    return status_dict

def nrf24_init(cfg):
    nrf24_disable(cfg)
    nrf24_enable(cfg)
    
    spi_transfer(cfg, bytes([W_REGISTER | NRF_CONFIG, (MASK_RX_DR<<6) + (MASK_TX_DS<<5) + (MASK_MAX_RT<<4) + (EN_CRC<<3) + (CRCO<<2) + (PWR_UP<<1) + (PRIM_RX<<0)]))
    spi_transfer(cfg, bytes([W_REGISTER | EN_AA, (ENAA_P5<<5) + (ENAA_P4<<4) + (ENAA_P3<<3) + (ENAA_P2<<2) + (ENAA_P1<<1) + (ENAA_P0<<0)]))
    spi_transfer(cfg, bytes([W_REGISTER | EN_RXADDR, (ERX_P5<<5) + (ERX_P4<<4) + (ERX_P3<<3) + (ERX_P2<<2) + (ERX_P1<<1) + (ERX_P0<<0)]))
    spi_transfer(cfg, bytes([W_REGISTER | SETUP_AW, AW_3_BYTES + AW_4_BYTES*2 + AW_5_BYTES*3]))
    spi_transfer(cfg, bytes([W_REGISTER | SETUP_RETR, ARD | ARC]))
    spi_transfer(cfg, bytes([W_REGISTER | RF_CH, CHANNEL]))
    spi_transfer(cfg, bytes([W_REGISTER | RF_SETUP, RF_DR|RF_PWR|LNA]))
    spi_transfer(cfg, bytes([W_REGISTER | RX_ADDR_P0] + RX_ADDR_P0_BUFFER))
    spi_transfer(cfg, bytes([W_REGISTER | TX_ADDR] + RX_ADDR_P0_BUFFER))
    spi_transfer(cfg, bytes([W_REGISTER | RX_PW_P0, P0_PACKET_SIZE]))
    
    nrf24_flush_tx(cfg)
    nrf24_flush_rx(cfg)
    
    status = nrf24_status(cfg)
    spi_transfer(cfg, bytes([W_REGISTER | STATUS, status["STATUS"]]))
    status = nrf24_status(cfg)
    print("init", status)
    assert status["RX_P_NO"] == 7 and status["TX_DS"] == 0 and status["MAX_RT"] == 0, "init test"

def nrf24_tx_mode(cfg):
    nrf24_disable(cfg)
    config = spi_transfer(cfg, bytes([R_REGISTER | NRF_CONFIG, RF24_NOP]))[1]
    if config & 1:# primrx = 1?
        spi_transfer(cfg, bytes([W_REGISTER | NRF_CONFIG, config-1]))
    nrf24_enable(cfg)
    #test
    config = spi_transfer(cfg, bytes([R_REGISTER | NRF_CONFIG, RF24_NOP]))[1]
    assert config & 1 == 0, f"nrf24_tx_mode test {config}"

def nrf24_rx_mode(cfg):
    nrf24_disable(cfg)
    config = spi_transfer(cfg, bytes([R_REGISTER | NRF_CONFIG, RF24_NOP]))[1]
    spi_transfer(cfg, bytes([W_REGISTER | NRF_CONFIG, config | 1]))
    nrf24_enable(cfg)
    #test
    config = spi_transfer(cfg, bytes([R_REGISTER | NRF_CONFIG, RF24_NOP]))[1]
    assert config & 1 == 1, f"nrf24_rx_mode test {config}"
    
def nrf24_tx(cfg, write_bytes, AUTO_ACK=True):
    '''
    by default blocks until succesful ack or max retransmission reached
    '''
    assert len(write_bytes) == P0_PACKET_SIZE, f"tx len: {len(write_bytes)}, {write_bytes}"
    nrf24_flush_tx(cfg)
    nrf24_tx_mode(cfg)
    
    spi_transfer(cfg, bytes([W_TX_PAYLOAD]) + write_bytes)

    success = False
    while AUTO_ACK:
        status = nrf24_status(cfg)

        if status["TX_DS"]:
            spi_transfer(cfg, bytes([W_REGISTER | STATUS, status["STATUS"]]))
            #print('TX_DS')
            success = True
            break
        
        if status["MAX_RT"]:
            spi_transfer(cfg, bytes([W_REGISTER | STATUS, status["STATUS"]]))
            #print('MAX_RT')
            success = False
            break
        
        utime.sleep_us(500)
    
    nrf24_rx_mode(cfg)
    return success

def nrf24_rx(cfg):
    '''
    reads from rx payload, use read() for blocking listen 
    '''
    data = spi_transfer(cfg, bytes([R_RX_PAYLOAD] + [RF24_NOP]*P0_PACKET_SIZE))
    return data[1:]

def nrf24_read(cfg):
    '''
    blocking read of single packet
    '''
    while True:
        status = nrf24_status(cfg)
        #print(status)
        if status["RX_P_NO"] < 7:
            # reset rx dr (TODO check rx  empty then reset)
            spi_transfer(cfg, bytes([W_REGISTER | STATUS, status["STATUS"] & 0b10111111]))
            
            # read data from buffer
            data = nrf24_rx(cfg)
            utime.sleep_ms(1)# sleep to allow auto ack reply
            
            assert len(data) == P0_PACKET_SIZE, f"data rcv len: {len(data)}"
            return data

        utime.sleep_ms(1)# polling sleep



########################################################################################
# 											application 							   #
########################################################################################
def process_packet(cfg, data):
    assert type(data) == type(bytearray()) or type(data) == type(bytes()), f"process_packet invalid data type {type(data)}"
    data = bytes(data)

    if data.find(b'COMMAND') == 0:
        CMD_LENGTH = data[len(b'COMMAND'): len(b'COMMAND')+1][0]
        CMD_DATA = data[len(b'COMMAND')+1: len(b'COMMAND')+1+CMD_LENGTH]
        #print("CMD RECEIVED:", [str(hex(b)) for b in CMD_DATA], "W_REGISTER:", CMD_DATA[0] & W_REGISTER == W_REGISTER, "EN_RXADDR:", CMD_DATA[0] & EN_RXADDR == EN_RXADDR)
        spi_transfer(cfg, CMD_DATA)
    elif data.find(b'SITREP') == 0:
        print("sitrep msg recvd")
        nrf24_tx_mode(cfg1)
        data = b'SITREP'
        data += bytes([b'z']*(P0_PACKET_SIZE-len(data)))
        if not nrf24_tx(cfg, data):
            print('sitrep no ack')
        nrf24_rx_mode(cfg1)
    else:
        print("rcvd:", data)
        

def send_cmd(cfg, cmd):
    '''
    send cmds for slave to write
    '''
    assert len(cmd) + len(b'COMMAND') <= P0_PACKET_SIZE, "send cmd, cmd too long"
    data = b'COMMAND' + bytes([len(cmd)]) + cmd + bytes([RF24_NOP] * (P0_PACKET_SIZE-len(b'COMMAND')-len(cmd)-1))
    #print("sending:", data)
    res = nrf24_tx(cfg, data)
    #print("send cmd success:", res)
    return res

def send_sitrep(cfg):
    '''
    request and wait for
    '''
    data = b'SITREP'
    data += bytes([RF24_NOP]*(P0_PACKET_SIZE-len(data)))
    if nrf24_tx(cfg1, data):
        sitrep = nrf24_read(cfg1)
        return sitrep
    else:
        print('sitrep no ack')


def main():
    nrf24_init(cfg0)
    #nrf24_check(cfg0)

    nrf24_init(cfg1)
    #nrf24_check(cfg1)

    i,j=1,0
    t0 = utime.ticks_us()
    while True:
        # cfg1 sends cmd to cfg0
        enable_all_rx_addr = bytes([W_REGISTER | EN_RXADDR, (1<<5) + (1<<4) + (1<<3) + (1<<2) + (1<<1) + (ERX_P0<<0)])
        
        start = utime.ticks_us()
        res = send_cmd(cfg1, enable_all_rx_addr)
        if not res:
            j+=1
            continue
        end = utime.ticks_us()
        if i % 200 ==  0:
            seconds=(utime.ticks_us()-t0)/1000000
            data = i*32/1000
            success = (i/(i+j))*100
            print('    tx time:', end-start, f"success {success:.2f}%:", f"data: {data:.2f}kB", f"t: {seconds:.2f}s", f"rate: {data/seconds:.2f}kB/s")
        #send_sitrep(cfg1)

        # block read rx buffer
        data = nrf24_read(cfg0)

        # react to rcvd pkt
        process_packet(cfg0, data)
        
        i+=1

        
try:
    main()
except Exception as e:
    print(e)
finally:
    pass

s