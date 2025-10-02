#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fatfs-aft/source/ff.h"
#include "fatfs-aft/source/ffconf.h"
#include "fatfs-aft/source/pal.h"

#define PACKET_SIZE 1024
#define MAX_PACKETS 8
#define TOTAL_SIZE (PACKET_SIZE*MAX_PACKETS)
#define BAUD_CYCLES 2604

static UARTRegBlk *uart = (UARTRegBlk *) UART_BASE;
uint8_t buff[TOTAL_SIZE];
int buff_pos = 0;

// Maximum bytes to send is 512, so send it separately!
void send_img_to_SD (uint8_t buff, UINT buff_size, const char *file_name) {
        FATFS fs;
        FIL dst;
        FRESULT res;
        UINT bw;

        /* Format the default drive with default parameters */
        res = f_mkfs("", 0, work, sizeof work);
        if (res) {
            printf("f_mkfs failed with %d\n", res);
        }

        // Mount
        res = f_mount(&fs, "", 0);
        if (res) {
                printf("f_mount failed with %d\n", res);
        }

        // Create image file in SD card
        res = f_open(&dst, file_name, FA_CREATE_ALWAYS | FA_WRITE);
        if (res) {
                f_close(&dst);
                f_mount(0, "", 0);
                printf("f_open failed with %d\n", res);
        }

        // Write image data
	res = f_write(&dst, buff, buff_size, &bw);
	if (res != FR_OK || bw < br) {
		printf("f_write failed with %d\n", res);
	}
        
        f_close(&dst);
        f_mount(0, "", 0);
        printf("Finish storing image in SD!\n");
}


int main (void) {
	// receive_packet()
}
