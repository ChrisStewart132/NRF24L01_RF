/**
 * Author: Christopher Stewart (Christopher.ray.stewart@gmail.com)
 * Date: 10062024
 * Description: program to convert a stream of 128x160 4bit grayscale to 16bit rgb565
 * 
 * gcc -o grayscale_4bit_to_16bit_RGB565 grayscale_4bit_to_16bit_RGB565.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>

#define WIDTH 128
#define HEIGHT 160

int main(int argc, char** argv) {
    uint16_t rgb565[HEIGHT][WIDTH];
    uint8_t grayscale_4bit[HEIGHT][WIDTH/2];
    while(read(0, grayscale_4bit, sizeof(grayscale_4bit)) > 0){

        for(int i = 0; i < HEIGHT; i++){
            for(int j = 0; j < WIDTH; j++){
                uint8_t gray;
                if(j&1 == 0){
                    gray = (grayscale_4bit[i][j/2] >> 4)*16;
                }else{
                    gray = (grayscale_4bit[i][j/2] & 0x0f)*16;
                }
                uint8_t r = gray/8;//0-31
                uint8_t g = gray/4;//0-63
                uint8_t b = gray/8;//0-31
                rgb565[i][j] = (r << 11) | (g << 5) | b;
            }
        }        
        write(1, rgb565, sizeof(rgb565));
        usleep(1000);
    }
	return EXIT_SUCCESS;
}