#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "FatFs/source/pal.h"
#include "FatFs/source/ff.h"

static volatile uint32_t * const vga_fb = (volatile uint32_t *)0xD0000000;

// ======================================================================
// Define constants
// ======================================================================
// UART
#define BAUD_CYCLES 2604
#define ACK         0
#define DONE        1
#define BAD         2
// SLIP
#define END         0xC0
#define ESC         0xDB
#define ESC_END     0xDC
#define ESC_ESC     0xDD
#define TYPE_DUMMY  0x7F
#define TYPE_META   0x01
#define TYPE_DATA   0x02
#define MAX_FRAME   (10 * 1024)
#define MEM_SIZE    (10 * 1024)
// FatFs
#define SEC_SIZE    512
// Image (Color)
#define BMP_HEADER  54
#define IMG_WIDTH   640
#define IMG_HEIGHT  480

// ======================================================================
// Define structures
// ======================================================================
typedef struct {
    uint32_t file_id;
    uint32_t total;        // total file size (from META)
    uint32_t received;     // total receieved byte in data_buf
    uint16_t chunk;        // chunk size
    uint32_t expect_seq;   // next seq of data
    uint32_t active;       // receive active signal
    uint8_t  fname_len;
    char     fname[256];
} transfer_info_t;

typedef enum { 
    ST_IDLE = 0, 
    ST_IN, 
    ST_ESC 
} slip_state_t;

// ======================================================================
// UART Initialization
// ======================================================================
static UARTRegBlk *const uart = (UARTRegBlk *)UART_BASE;
static void __init_uart(void) __attribute__((constructor));
static void __init_uart(void)
{
    uart->rxstate = (BAUD_CYCLES / 16) << 16;
    uart->txstate = BAUD_CYCLES << 16;
}

// ======================================================================
// Define global variables
// ======================================================================
static transfer_info_t   transfer_info;
static slip_state_t      state = ST_IDLE;
static uint8_t           frame_buf[MAX_FRAME];
static uint32_t          frame_num = 0;
static uint8_t           data_buf[MEM_SIZE];
static uint32_t          count_photo = 0;
static uint32_t          photo_offset = 0;
static uint32_t          dummy_flag = 0;
static uint32_t          image_done = 0;
static FATFS             fs;
static FIL               fil;
static int               file_opened = 0;
static char              filename[32];
static uint8_t           write_buf[SEC_SIZE];
static uint32_t          write_bytes = 0;

// ======================================================================
// Declare functions
// ======================================================================
static inline uint16_t   rd16(const uint8_t *p);
static inline uint32_t   rd32(const uint8_t *p);
static void              uart_rx(void);
static void              split_byte_stream(uint8_t byte);
static void              handle_frame(uint8_t *buf, uint32_t frame_num);
static void              handle_meta(const uint8_t *buf, uint32_t frame_num);
static void              handle_data(const uint8_t *buf, uint32_t frame_num_in);
static inline uint16_t   rd16(const uint8_t *p);
static inline uint32_t   rd32(const uint8_t *p);
static void              handle_frame(uint8_t *buf, uint32_t frame_num);
static void              handle_meta (const uint8_t *buf, uint32_t frame_num);
static void              handle_data (const uint8_t *buf, uint32_t frame_num);
static void              display_rgb565_image (char filename[32]);
static void              search_next_image();
static void              send_ack(int TYPE);

// ======================================================================
// Functions
// ======================================================================
static void send_ack(int TYPE) {
    if (TYPE == ACK) {
        uart->txdata = 'A';
        while (!(uart->txstate & 0x1));
        uart->txdata = 'C';
        while (!(uart->txstate & 0x1));
        uart->txdata = 'K';
        while (!(uart->txstate & 0x1));
    }
    else if (TYPE == DONE) {
        uart->txdata = 'E';
        while (!(uart->txstate & 0x1));
        uart->txdata = 'N';
        while (!(uart->txstate & 0x1));
        uart->txdata = 'D';
        while (!(uart->txstate & 0x1));
    } else {
        uart->txdata = 'B';
        while (!(uart->txstate & 0x1));
        uart->txdata = 'A';
        while (!(uart->txstate & 0x1));
        uart->txdata = 'D';
        while (!(uart->txstate & 0x1));
    }
}

static void display_rgb565_image(char *filename) {
    FRESULT res;
    FATFS fs;
    FIL fil;
    UINT br;

    static uint8_t sec_buf[SEC_SIZE];
    static uint8_t row_buf[((IMG_WIDTH * 2 + 3) & ~3)];

    res = f_mount(&fs, "", 0);
    if (res) return;

    res = f_open(&fil, filename, FA_READ);
    if (res) {
        f_mount(0, "", 0);
        return;
    }

    uint8_t header[BMP_HEADER];
    f_read(&fil, header, BMP_HEADER, &br);

    uint32_t pixel_offset =
        (uint32_t)header[10]        |
        ((uint32_t)header[11] << 8) |
        ((uint32_t)header[12] << 16)|
        ((uint32_t)header[13] << 24);

    f_lseek(&fil, pixel_offset);

    uint32_t row_size = ((IMG_WIDTH * 2 + 3) & ~3);
    uint32_t row_bytes = 0;
    uint32_t bmp_row = 0;

    while (bmp_row < IMG_HEIGHT) {
        f_read(&fil, sec_buf, 512, &br);
        if (br == 0) break;

        uint32_t p = 0;

        while (p < br && bmp_row < IMG_HEIGHT)
        {
            uint32_t need = row_size - row_bytes;
            uint32_t remain = br - p;
            uint32_t take = (remain < need) ? remain : need;

            memcpy(row_buf + row_bytes, sec_buf + p, take);

            row_bytes += take;
            p += take;

            if (row_bytes == row_size) {
                uint32_t fb_row = IMG_HEIGHT - 1 - bmp_row;
                uint32_t base = fb_row * IMG_WIDTH;

                for (int col = 0; col < IMG_WIDTH; col++) {
                    uint16_t pixel565 = (row_buf[col * 2 + 1] << 8) | row_buf[col * 2 + 0];
                    vga_fb[base + col] = pixel565;
                }

                bmp_row++;
                row_bytes = 0;
            }
        }
    }

    f_close(&fil);
    f_mount(0, "", 0);
}

// ENDIAN LOADER
static inline uint16_t rd16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static inline uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static void uart_rx(void) {
    if ((uart->rxstate) & 0x1) {
        uint32_t rxdata = uart -> rxdata;
        uint8_t fifoCount = rxdata >> 24;
        if (fifoCount == 0) fifoCount = 1;   
        if (fifoCount > 3)  fifoCount = 3;
        for (uint8_t i = 0; i < fifoCount; i++) {
            uint8_t b = rxdata & 0xFF;
            split_byte_stream(b);
            rxdata >> 8;
        }
    }
}

static void split_byte_stream(uint8_t byte){
    switch(byte) {
    case END:
        if (state == ST_IN && frame_num > 0) {
            handle_frame(frame_buf, frame_num);
        }
        frame_num = 0;
        state = ST_IN;
        return;

    case ESC:
        if (state == ST_IN) state = ST_ESC;
        return;

    default:
        if (state == ST_ESC) {
            if (byte == ESC_END)      byte = END;
            else if (byte == ESC_ESC) byte = ESC;
            state = ST_IN;
        }
        if (state == ST_IN) {
            if (frame_num < MAX_FRAME) {
                frame_buf[frame_num++] = byte;
            } else {
                frame_num = 0;
                state = ST_IDLE;
            }
        }
        return;
    }
}

static void handle_frame(uint8_t *buf, uint32_t frame_num){
    uint8_t type = buf[0];
    if (!frame_num) return;
    if (type == TYPE_META) {
        handle_meta(buf, frame_num); 
    } 
    else if (type == TYPE_DATA) {
        handle_data(buf, frame_num); 
    }
}

// META Frame Handler
// Format: <BBIIHB + fname>
// [0]=0x01, [1]=ver, [2..5]=file_id, [6..9]=total, [10..11]=chunk, [12]=fname_len, [13..]=fname
static void handle_meta(const uint8_t *buf, uint32_t frame_num) {
    const uint32_t MIN_META = 13u; 
    if (frame_num < MIN_META) return;
    uint32_t file_id = rd32(buf + 2);
    uint32_t total_size = rd32(buf + 6);
    uint16_t chunk_size = rd16(buf + 10);
    uint8_t  fname_len  = buf[12];
    if (13u + (uint32_t)fname_len > frame_num) return;  // check the range
    if (fname_len > 255) fname_len = 255;
    
    // init receive session
    memset(&transfer_info, 0, sizeof(transfer_info));
    transfer_info.file_id = file_id;
    transfer_info.total = total_size;
    transfer_info.chunk = chunk_size;
    transfer_info.received = 0;
    transfer_info.expect_seq = 0;
    transfer_info.active = 1;

    if (fname_len) {
        memcpy(transfer_info.fname, (const char*)(buf + 13), fname_len);
    }
    transfer_info.fname[fname_len] = '\0';

    write_bytes = 0;

    // Send the data to SD
    snprintf(filename, sizeof(filename), "image%d.bmp", count_photo);

    // Mount
    FRESULT res;
    res = f_mount(&fs, "", 0);
    if (res) {
        file_opened = 0;
        transfer_info.active = 0;
        printf("f_mount failed with %d\n", res);
    }

    // Create file in SD
    res = f_open(&fil, filename, FA_CREATE_ALWAYS | FA_WRITE);

    if (res) {
        f_close(&fil);
        f_mount(0, "", 0);
        file_opened = 0;
        transfer_info.active = 0;
        printf("f_open for dst failed with %d\n", res);
    }

    file_opened = 1;
    send_ack(ACK);
    uart_rx();
}

// DATA Frame Handler
// Format: <BIIH + payload>
// [0]=0x02, [1..4]=file_id, [5..8]=seq, [9..10]=payload_len, [11..]=payload
static void handle_data(const uint8_t *buf, uint32_t frame_num_in){
    const uint32_t MIN_DATA = 11u;
    if (transfer_info.active == 0) return;
    if (frame_num_in < MIN_DATA) {
        send_ack(BAD);
        uart_rx();
    }

    uint32_t file_id     = rd32(buf + 1);
    uint32_t seq         = rd32(buf + 5);
    uint16_t payload_len = rd16(buf + 9);

    if (11u + (uint32_t)payload_len > frame_num_in) {
        send_ack(BAD);
        uart_rx();
    } // length check
    
    const uint8_t *payload = buf + 11;

    if (file_id != transfer_info.file_id) {
        send_ack(BAD);
        uart_rx();
    }
    if (seq != transfer_info.expect_seq) {
        send_ack(BAD);
        uart_rx();
    }

    if (transfer_info.received + payload_len > transfer_info.total) {
        send_ack(BAD);
        uart_rx();
    }

    if (file_opened == 0) return;

    // Write data to SD
    UINT bw;
    FRESULT res;

    uint32_t pos = 0;
    while (pos < payload_len) {
        uint32_t space = SEC_SIZE - write_bytes;
        uint32_t copy = payload_len - pos;
        if (copy > space) copy = space;

        memcpy(write_buf + write_bytes, payload + pos, copy);
        write_bytes += copy;
        pos += copy;

        if (write_bytes == SEC_SIZE) {
            res = f_write(&fil, write_buf, SEC_SIZE, &bw);
            if (res != FR_OK || bw != SEC_SIZE) {
                f_close(&fil);
                f_mount(0, "", 0);
                file_opened = 0;
                transfer_info.active = 0;
                printf("f_write failed with %d\n", res);
                return;
            }
            write_bytes = 0;
        }
    }

    transfer_info.received   += payload_len;
    transfer_info.expect_seq  = seq + 1;

    if (transfer_info.received == transfer_info.total) {
        if (write_bytes > 0) {
            res = f_write(&fil, write_buf, write_bytes, &bw);
            if (res != FR_OK || bw != write_bytes) {
                f_close(&fil);
                f_mount(0, "", 0);
                file_opened = 0;
                transfer_info.active = 0;
                printf("f_write failed with %d\n", res);
                return;
            }
            write_bytes = 0;
        }
        send_ack(DONE);
    } else {
        send_ack(ACK);
        uart_rx();
    }

    if (transfer_info.received == transfer_info.total) {
        printf("Received image from PC!\n");
        f_close(&fil);
        f_mount(0, "", 0);
        file_opened = 0;
        transfer_info.active = 0;
        display_rgb565_image(filename);
        photo_offset = count_photo;
        count_photo++;
        search_next_image();
    }
}

static void search_next_image() {
    while (1) {
        uart_rx();
        if (photo_offset < count_photo) {
            char next_filename[32];
            snprintf(next_filename, sizeof(next_filename), "image%d.bmp", photo_offset);
            display_rgb565_image(next_filename);
        }
        for (volatile int i = 0; i < 0xF00000; i++) {
            uart_rx();
        }
        photo_offset++;
        if (photo_offset == count_photo) {
            photo_offset = 0;
        }
    }
}

int main(void) {
    search_next_image();
}