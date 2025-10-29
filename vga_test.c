#include <stdint.h>
<<<<<<< HEAD
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "FatFs/source/pal.h"
#include "FatFs/source/ff.h"

static volatile uint32_t * const vga_fb = (volatile uint32_t *)0xD0000000;

#define IMG_WIDTH  48 // 86 for gray
#define IMG_HEIGHT 48 // 86 for gray
#define MEM_SIZE (10 * 1024)
#define HEADER_SIZE 138
#define SEC_SIZE 512

void display_color_image (char *filename) {
    // Declaration
    FRESULT res;
    FATFS fs;
    FIL fil;
    UINT br;

    static uint8_t img_buf[MEM_SIZE]; // data buffer
    uint8_t header[HEADER_SIZE]; // header buffer
    uint32_t total_bytes = 0; // image size

    // Mount SD card
    res = f_mount(&fs, "", 0);
    if (res) {
        f_mount(0, "", 0);
    }

    // Open image file
    res = f_open(&fil, filename, FA_READ);
    if (res) {
	    f_close(&fil);
	    f_mount(0, "", 0);
    }

    // Read header
    f_read(&fil, header, HEADER_SIZE, &br);

    // Read data
    while (total_bytes + SEC_SIZE <= MEM_SIZE) {
        res = f_read(&fil, img_buf + total_bytes, SEC_SIZE, &br);
        if (res != FR_OK || br == 0) break;
        total_bytes += br;
        if (br < SEC_SIZE) break;
    }
    f_close(&fil);
    f_mount(0, "", 0);

    // Send data from the bottom
    uint32_t row_size = ((IMG_WIDTH * 3 + 3) / 4) * 4;
    uint32_t count = 0;

    for (int row = IMG_HEIGHT - 1; row >= 0; row--) {  // bottom -> top
        uint8_t *row_ptr = img_buf + row * row_size; // move pointer to the end
        for (int col = 0; col < IMG_WIDTH; col++) {
            uint8_t B = row_ptr[col * 3 + 0];
            uint8_t G = row_ptr[col * 3 + 1];
            uint8_t R = row_ptr[col * 3 + 2];
            uint32_t rgb = (R << 16) | (G << 8) | B;
            vga_fb[count++] = rgb;
        }
    }
}

void draw_rectangle () {
    for(volatile int i = 0; i < (IMG_WIDTH*IMG_HEIGHT); i++) {
        vga_fb[i] = 0x00FF00;
    }
}

int main () {
<<<<<<< HEAD
    //draw_rectangle();
    //display_gray_image();

    while(1) {
        display_color_image("mario.bmp");
        for (volatile int i = 0; i < 0x800000; i++);
        display_color_image("lena.bmp");
        for (volatile int i = 0; i < 0x800000; i++);
        display_color_image("purdue.bmp");
        for (volatile int i = 0; i < 0x800000; i++);
        //display_color_image("purdue_front.bmp");
        //for (volatile int i = 0; i < 0x800000; i++);
        //display_color_image("ece.bmp");
        //for (volatile int i = 0; i < 0x800000; i++);
        display_color_image("snail.bmp");
        for (volatile int i = 0; i < 0x800000; i++);
    };
    return 0;
}

