/**
 * Author: Christopher Stewart (Christopher.ray.stewart@gmail.com)
 * Date: 22062024
 * Description: program to read an incoming stream of 128x160 yuv420 images from a file and stdout at 30fps
 * 
 * gcc -o YUV420_Stream_From_File YUV420_Stream_From_File.c
 * 
 * ./file.yuv420 | YUV420_Stream_From_File | ./YUV420_to_RGB565 | ./ST7735S_LCD_stdin_stream
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#define WIDTH 128
#define HEIGHT 160

int main(int argc, char** argv) {
    uint8_t y[HEIGHT][WIDTH];
    uint8_t u[HEIGHT/2][WIDTH/2];
    uint8_t v[HEIGHT/2][WIDTH/2];
    uint8_t grayscale_4bit[HEIGHT][WIDTH/2];
    int bytes_read;
    while (1) {
        usleep(1000000/30);// 30fps
        for(int i = 0; i < HEIGHT; i++){
            bytes_read = read(0, y[i], sizeof(uint8_t)*WIDTH);
        }
        if(bytes_read <= 0)
            break;
        for(int i = 0; i < HEIGHT/2; i++){
            read(0, u[i], sizeof(uint8_t)*WIDTH/2);
        }
        for(int i = 0; i < HEIGHT/2; i++){
            read(0, v[i], sizeof(uint8_t)*WIDTH/2);
        }

        write(1, y, sizeof(y));
        write(1, u, sizeof(u));
        write(1, v, sizeof(v));
    }
    return EXIT_SUCCESS;
}