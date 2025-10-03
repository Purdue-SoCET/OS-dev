#include "source/ff.h"
#include <stdio.h>
#include "source/spi_sd.h"
#include "source/sdio.h"
#include <stdlib.h>
#include <string.h>
#include "source/pal.h"

extern unsigned char test_png[];
extern unsigned int test_png_len;

#define BAUD_CYCLES 2604
static UARTRegBlk *uart = (UARTRegBlk *) UART_BASE;
static void __init_uart(void) __attribute__((constructor));
static void __init_uart(void)
{
    uart->rxstate = (BAUD_CYCLES / 16) << 16;
    uart->txstate = BAUD_CYCLES << 16;
}

void write_img();

int main() {
	printf("Start storing image in SD card!\n");
	write_img();
}

void write_img() {
	FATFS fs;
        FIL dst;
	FRESULT res;
	BYTE work[FF_MAX_SS];
	UINT bw;
	UINT bytes_left = test_png_len;
	UINT offset = 0;

        /* Format the default drive with default parameters */
        //res = f_mkfs("", 0, work, sizeof work);
	//printf("f_mkfs here!\n");
        //if (res) {
        //    printf("f_mkfs failed with %d\n", res);
        //}

	// Mount
	res = f_mount(&fs, "", 0);
	printf("f_mount here!\n");
	if (res) {
		printf("f_mount failed with %d\n", res);
	}

	// Create image file on SD card
	res = f_open(&dst, "from_pc.bmp", FA_CREATE_ALWAYS | FA_WRITE);
	printf("f_open here!\n");
	if (res) {
		f_close(&dst);
		f_mount(0, "", 0);
		printf("f_open for dst failed with %d\n", res);
	}

	// Write image data
	while (bytes_left > 0) {
		printf("Try writing data!\n");
		UINT chunk_size = (bytes_left > 512) ? 512 : bytes_left;
		
		res = f_write(&dst, &test_png[offset], chunk_size, &bw);
		if (res != FR_OK || bw != chunk_size) {
                        printf("f_write failed with %d\n", res);
                        break;
                }

                offset += chunk_size;
		bytes_left -= chunk_size;
	}

	// Close file
	f_close(&dst);
	f_mount(0, "", 0);
	printf("Finish storing image in SD card!\n");
}
