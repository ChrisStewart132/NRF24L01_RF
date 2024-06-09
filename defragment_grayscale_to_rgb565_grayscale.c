/**
 * Author: Christopher Stewart (Christopher.ray.stewart@gmail.com)
 * Date: 08062024
 * Description: program to fragment an incoming stream of 128x160 grayscale to 32 byte packets
 * 
 * gcc -o defragment_grayscale_to_rgb565_grayscale defragment_grayscale_to_rgb565_grayscale.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <stdbool.h>

#define WIDTH 128
#define HEIGHT 160

int main(int argc, char** argv) {
    uint16_t rgb565[HEIGHT][WIDTH]={{0xf000}};// red
    
    // 32 byte packet
    uint8_t fragment[32];

    int packets_read = 0;
    while(1){
        // read fragment
        read(0, fragment, sizeof(fragment));

        packets_read++;

        // buffer fragment
        // current x,y coordinates
        uint8_t x = fragment[0];
        uint8_t y = fragment[1];
        for(int i = 2; i < 32; i++){
            if(x >= WIDTH){
                x=0;
                y++;
            }
            uint8_t gray = fragment[i];
            uint8_t r = gray/8;//0-31
            uint8_t g = gray/4;//0-63
            uint8_t b = gray/8;//0-31
            rgb565[y][x++] = (r << 11) | (g << 5) | b;
        }        

        bool send_buffer = packets_read >= 682;// 682 packets per frame
        if(send_buffer){
            packets_read = 0;
            write(1, rgb565, sizeof(rgb565));
            usleep(1000); 
        }
        usleep(130);
    }
	return EXIT_SUCCESS;
}