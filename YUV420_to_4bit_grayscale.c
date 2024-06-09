/**
 * Author: Christopher Stewart (Christopher.ray.stewart@gmail.com)
 * Date: 10062024
 * Description: program to convert an incoming stream of 128x160 yuv420 images to 4bit grayscale
 * 
 * gcc -o YUV420_to_4bit_grayscale YUV420_to_4bit_grayscale.c
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
    sleep(1);
    while (read(0, y, sizeof(y)) > 0) {
        read(0, u, sizeof(u));
        read(0, v, sizeof(v));
        // convert YUV420 (12bit per pixel) to 4bit grayscale (4bit per pixel)
        for (int i = 0; i < HEIGHT; i++) {
            for (int j = 0; j < WIDTH; j ++) {
                uint8_t gray = y[i][j]/16;// convert from 8bit (0-255) to 4bit gray (0-15)
                if(j&1){
                    grayscale_4bit[i][j/2] |= gray;
                }else{
                    grayscale_4bit[i][j/2] = (gray << 4);
                }  
            }
        }
        write(1, grayscale_4bit, sizeof(grayscale_4bit));
    }
    return EXIT_SUCCESS;
}