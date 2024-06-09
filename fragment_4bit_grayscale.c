/**
 * Author: Christopher Stewart (Christopher.ray.stewart@gmail.com)
 * Date: 10062024
 * Description: program to fragment an incoming stream of 128x160 4bit grayscale to 32 byte packets
 * 
 * gcc -o fragment_4bit_grayscale fragment_4bit_grayscale.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#define WIDTH 128
#define HEIGHT 160

int main(int argc, char** argv) {
    uint8_t grayscale_4bit[HEIGHT][WIDTH/2];

    // 32 byte packets
    uint8_t fragment_buffer[32*341];// 341 packets per frame
    while(read(0, grayscale_4bit, sizeof(grayscale_4bit)) > 0){
        // current x,y coordinates
        uint8_t x = 0;
        uint8_t y = 0;
        int index = 0;
        for(int j = 0; j < 341; j++){
            fragment_buffer[index++] = x;// 2 byte grid index header
            fragment_buffer[index++] = y;
            for(int i = 2; i < 32; i++){// 30 Byte pixel payload
                if(x >= WIDTH/2){
                    x = 0;
                    y++;
                }
                fragment_buffer[index++] = grayscale_4bit[y][x++];
            }
        }
        write(1, fragment_buffer, sizeof(fragment_buffer));
    }
	return EXIT_SUCCESS;
}