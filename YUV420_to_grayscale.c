/**
 * Author: Christopher Stewart (Christopher.ray.stewart@gmail.com)
 * Date: 08062024
 * Description: program to convert an incoming stream of 128x160 yuv420 images to grayscale
 * 
 * gcc -o YUV420_to_grayscale YUV420_to_grayscale.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#define WIDTH 128
#define HEIGHT 160

int main(int argc, char** argv) {
    // input
    uint8_t y[HEIGHT][WIDTH];
    uint8_t u[HEIGHT/2][WIDTH/2];
    uint8_t v[HEIGHT/2][WIDTH/2];

    while(1){
        read(0, y, sizeof(y));
        read(0, u, sizeof(u));
        read(0, v, sizeof(v));

        write(1, y, sizeof(y));
        usleep(1000000/30);
    }
	return EXIT_SUCCESS;
}