/**
 * Author: Christopher Stewart (Christopher.ray.stewart@gmail.com)
 * Date: 10062024
 * Description: program to defragment an incoming stream 32 byte packets to 128x160 4bit grayscale
 * 
 * gcc -o defragment_4bit_grayscale defragment_4bit_grayscale.c
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
    uint8_t grayscale_4bit[HEIGHT][WIDTH/2] = {{0}};
    
    // 32 byte packet
    uint8_t fragment[32];

    int packets_read = 0;
    while(read(0, fragment, sizeof(fragment)) > 0){
        packets_read++;

        // current 4bit x,y coordinates
        uint8_t x = fragment[0];
        uint8_t y = fragment[1];
        for(int i = 0; i < 30; i++){
            if(x >= WIDTH/2){
                x = 0;
                y++;
            }
            grayscale_4bit[y][x++] = fragment[i+2];
        }        

        bool send_buffer = packets_read >= 341;// 341 packets per frame    
        if(send_buffer){
            packets_read = 0;
            write(1, grayscale_4bit, sizeof(grayscale_4bit));
            memset(grayscale_4bit, 0, sizeof(grayscale_4bit));
        }
    }
	return EXIT_SUCCESS;
}