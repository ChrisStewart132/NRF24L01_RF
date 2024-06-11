/**
 * Author: Christopher Stewart (Christopher.ray.stewart@gmail.com)
 * Date: 11062024
 * Description: program to display a 128x160 16bit rgb (565) stream from stdin on a ST7735S 1.8" 128x160 LCD
 * 
 * gcc -o ST7735S_LCD_stdin_stream ST7735S_LCD_stdin_stream.c -lgpiod
 * 
 * grayscale
 * rpicam-vid -t 0 -n --framerate 30 --width 128 --height 160 --codec yuv420 -o - | ./YUV420_to_RGB565_grayscale | ./ST7735S_LCD_stdin_stream
 * 
 * rgb565
 * rpicam-vid -t 0 -n --framerate 30 --width 128 --height 160 --codec yuv420 -o - | ./YUV420_to_RGB565 | ./ST7735S_LCD_stdin_stream
 * 
 * 4bit grayscale
 * rpicam-vid -t 0 -n --framerate 24 --width 128 --height 160 --codec yuv420 -o - | ./YUV420_to_4bit_grayscale | ./grayscale_4bit_to_16bit_RGB565 | ./ST7735S_LCD_stdin_stream
 * 
 * 2bit grayscale
 * rpicam-vid -t 0 -n --framerate 24 --width 128 --height 160 --codec yuv420 -o - | ./YUV420_to_2bit_grayscale | ./grayscale_2bit_to_16bit_RGB565 | ./ST7735S_LCD_stdin_stream
 * 
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

#define WIDTH 128
#define HEIGHT 160

#define GPIO_CHIP_NAME "gpiochip0"
#define GPIO_OFFSET_DC 26 	// DATA(high)/COMMAND(low) aka WRX
#define GPIO_OFFSET_RST 16 	// RESET active low

/**
 * Note to use SPI1 on rpi you must edit boot/config.txt and add (then reboot): dtoverlay=spi1-2cs
 * this makes GPIO 18,17 chip selects/enables (CE0, CE1) for SPI1
*/
#define SPI_DEVICE "/dev/spidev1.0"
#define SPI_HZ 16000000
#define MAX_SPI_TRANSFER_SIZE 4096 // Define a maximum transfer size that your SPI hardware supports
/**
 * SCE
 * MOSI
 * CLK
 */

/**
 * commands
*/
#define NOP 0x00
#define SWRESET 0x01	// loads default settings
#define SLPOUT 0x11		// Sleep Out & Booster On
#define NORON 0x13		// normal display mode on
#define DISPON 0x29		// default display is off...

#define GAMSET 0x26		
#define COLMOD 0x3a

#define CASET 0x2a	// cursor xs-xe
#define RASET 0x2b	// cursor ys-ye
#define RAMWR 0x2c	// continuously write to fill xs,ys - xe,ye


#define INVON 0x20
#define INVOFF 0x21


#define RGBSET 0x2d	// rgb order
#define SCRLAR 0x33
#define MADCTL 0x36 // lcd orientation settings
#define VSCSAD 0x37 

         
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

void init_gpio(struct gpiod_chip** chip, struct gpiod_line** dc, struct gpiod_line** rst) {
    *chip = gpiod_chip_open_by_name(GPIO_CHIP_NAME);
    if (!*chip) {
        perror("gpiod_chip_open_by_name");
        exit(EXIT_FAILURE);
    }

    *rst = gpiod_chip_get_line(*chip, GPIO_OFFSET_RST);
    if (!*rst) {
        perror("gpiod_chip_get_line");
        gpiod_chip_close(*chip);
        exit(EXIT_FAILURE);
    }
    if (gpiod_line_request_output(*rst, "rst", 0) < 0) {
        perror("gpiod_line_request_output");
        gpiod_chip_close(*chip);
        exit(EXIT_FAILURE);
    }

	*dc = gpiod_chip_get_line(*chip, GPIO_OFFSET_DC);
    if (!*dc) {
        perror("gpiod_chip_get_line");
        gpiod_chip_close(*chip);
        exit(EXIT_FAILURE);
    }
    if (gpiod_line_request_output(*dc, "dc", 0) < 0) {
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

void command(int fd, struct gpiod_line* dc, uint8_t cmd){
	uint8_t rx;
	_gpio_low(dc);
    _spi_transfer(fd, &cmd, &rx, 1);
    usleep(120000); // 120ms delay
}

void _command(int fd, struct gpiod_line* dc, uint8_t cmd){
	uint8_t rx;
	_gpio_low(dc);
    _spi_transfer(fd, &cmd, &rx, 1);
}

void init(int fd, struct gpiod_line* dc, struct gpiod_line* rst){
	/**
	 * 9.14 Power Level Definition
		* 6. Power Off Mode In this mode, both VDD and VDDI are removed.
		* 5. Sleep In Mode In this mode, the DC: DC converter, internal oscillator and panel driver circuit are stopped. Only the MCU interface and memory works with VDDI power supply. Contents of the memory are safe.
		* 1. Normal Mode On (full display), Idle Mode Off, Sleep Out. In this mode, the display is able to show maximum 262,144 colors.
	 * 
	 * 
	 * 
	 * Power on (HW reset, SW reset): Sleep in, Normal display mode on, idle mode off
	 * 
	 * CMD SLPOUT: Sleep out, Normal display mode on, idle mode off (maximum power usage, full screen, and full color mode)
	 * 
	 * 
	*/
	uint8_t tx_buffer[33] = {NOP};
	uint8_t rx_buffer[33] = {0};
	
	// Hardware reset
	_gpio_low(rst);
	usleep(100000); // 100ms delay
	_gpio_high(rst);
	usleep(100000); // 100ms delay

	// Software reset
	command(fd, dc, SWRESET);

	// Exit sleep mode
	command(fd, dc, SLPOUT);

	// Display on
	command(fd, dc, DISPON);

	// Normal display mode on
	command(fd, dc, NORON);

	
	// Gamma set
	command(fd, dc, GAMSET);
	_gpio_high(dc);
	tx_buffer[0] = 0x2;
	_spi_transfer(fd, tx_buffer, rx_buffer, 1);

	// Interface Pixel Format
	command(fd, dc, COLMOD);
	_gpio_high(dc);
	tx_buffer[0] = 0x05;// 16-bit / pixel
	_spi_transfer(fd, tx_buffer, rx_buffer, 1);

	// set col range 
	command(fd, dc, CASET);
	_gpio_high(dc);
	tx_buffer[0] = 0;		// start short (2 bytes)
	tx_buffer[1] = 0;
	tx_buffer[2] = 0;		// end short
	tx_buffer[3] = WIDTH;
	_spi_transfer(fd, tx_buffer, rx_buffer, 4);

	// set row range 
	command(fd, dc, RASET);
	_gpio_high(dc);
	tx_buffer[0] = 0;		// start short (2 bytes)
	tx_buffer[1] = 0;
	tx_buffer[2] = 0;		// end short
	tx_buffer[3] = HEIGHT;
	_spi_transfer(fd, tx_buffer, rx_buffer, 4);
}

void display_invert(int fd, struct gpiod_line* dc){
	static bool inverted = false;
	uint8_t cmd;
	if(inverted){
		cmd = INVOFF;
	}else{
		cmd = INVON;
	}
	command(fd, dc, cmd);
	inverted = ! inverted;
}



void display_buffer(int fd, struct gpiod_line* dc, uint16_t buffer[160][128]){
	uint8_t tx_buffer[4] = {NOP};
	uint8_t frame_buffer[WIDTH * HEIGHT * 2];// Frame buffer to hold the entire frame in RGB565 format
	uint8_t rx_buffer[WIDTH * HEIGHT * 2] = {0}; // Dummy receive buffer
	
	// write to ram
	_command(fd, dc, RAMWR);
	_gpio_high(dc);
	int index = 0;
	for(int i = 0; i < HEIGHT; i++){
		for(int j = 0; j < WIDTH; j++){
			frame_buffer[index++] = (uint8_t)(buffer[i][j] >> 8);    // High byte of 16-bit color
            frame_buffer[index++] = (uint8_t)(buffer[i][j] & 0xFF); // Low byte of 16-bit color
		}
	}
	// Transfer the frame buffer in chunks
    int total_bytes = sizeof(frame_buffer);
    int offset = 0;
    while (offset < total_bytes) {
        int chunk_size = total_bytes - offset;
        if (chunk_size > MAX_SPI_TRANSFER_SIZE) {
            chunk_size = MAX_SPI_TRANSFER_SIZE;
        }
        _spi_transfer(fd, frame_buffer + offset, rx_buffer, chunk_size);
        offset += chunk_size;
    }
}

int main() {
	struct gpiod_chip* chip;
	struct gpiod_line* dc;
	struct gpiod_line* rst;

	init_gpio(&chip, &dc, &rst);
	
	int fd;// SPI file
	init_spi(&fd);

	init(fd, dc, rst);
	//display_invert(fd, dc);
	sleep(1);
	uint16_t buffer[160][128] = {{0}};
	while(1){
		for(int i = 0; i < 160; i++){
			read(0, buffer[i], sizeof(uint16_t)*128);
		}
		//read(0, buffer, sizeof(buffer));
		display_buffer(fd, dc, buffer);
		usleep(1000);
	}

	if(dc)
		gpiod_line_release(dc);
	if(rst)
		gpiod_line_release(rst);
	if(chip)
		gpiod_chip_close(chip);

	close(fd);
	return EXIT_SUCCESS;
}