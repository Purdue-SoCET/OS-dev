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
#define BMP_HEADER  138
#define IMG_WIDTH   48 // 86 for gray
#define IMG_HEIGHT  48 // 86 for gray

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
static void              send_photo_to_SD();
static void              display_color_image (char filename[32]);
static void              search_next_image();

// ======================================================================
// Functions
// ======================================================================
void display_color_image (char filename[32]) {
    // Declaration
    FRESULT res;
    FATFS fs;
    FIL fil;
    UINT br;

    static uint8_t img_buf[MEM_SIZE]; // data buffer
    uint8_t header[BMP_HEADER]; // header buffer
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
    f_read(&fil, header, BMP_HEADER, &br);

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

static void send_photo_to_SD() {
    // Name file
    count_photo++;
    photo_offset = count_photo;
    char filename[32];
    snprintf(filename, sizeof(filename), "image%lu.bmp", (unsigned long)count_photo);

    // Declare variables	
    FATFS fs;
    FIL fil;
    FRESULT res;
    UINT bw;
    UINT bytes_left = transfer_info.received;
    UINT offset = 0;

    // Mount
    res = f_mount(&fs, "", 0);
    //printf("Start mounting SD card...\n");
    if (res) {
        printf("f_mount failed with %d\n", res);
    }

    // Create file in SD
    res = f_open(&fil, filename, FA_CREATE_ALWAYS | FA_WRITE);
    //printf("Creating file in SD card...\n");
    if (res) {
        f_close(&fil);
        f_mount(0, "", 0);
        printf("f_open for dst failed with %d\n", res);
    }

    // Write photo data
    while (bytes_left > 0) {
	    UINT chunk_size = (bytes_left > SEC_SIZE) ? SEC_SIZE : bytes_left;
		
	    res = f_write(&fil, &data_buf[offset], chunk_size, &bw);
	    if (res != FR_OK || bw != chunk_size) {
	        printf("f_write failed with %d\n", res);
            break;
	}

	    offset += chunk_size;
	    bytes_left -= chunk_size;
    }

    // Close file
    f_close(&fil);
    f_mount(0, "", 0);
    //printf("Complete sending photo to SD!\n");

    // Display on the monitor
    //printf("Displaying on the monitor...\n");
    display_color_image(filename);
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
    //if (total_size > MEM_SIZE) return; // preventing the size getting bigger than the RAM size
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
}

// DATA Frame Handler
// Format: <BIIH + payload>
// [0]=0x02, [1..4]=file_id, [5..8]=seq, [9..10]=payload_len, [11..]=payload
static void handle_data(const uint8_t *buf, uint32_t frame_num_in){
    const uint32_t MIN_DATA = 11u;
    if (transfer_info.active == 0) return;
    if (frame_num_in < MIN_DATA) return;

    uint32_t file_id     = rd32(buf + 1);
    uint32_t seq         = rd32(buf + 5);
    uint16_t payload_len = rd16(buf + 9);

    if (11u + (uint32_t)payload_len > frame_num_in) return; // length check
    const uint8_t *payload = buf + 11;

    if (file_id != transfer_info.file_id) return;
    if (seq     != transfer_info.expect_seq) return;

    if (transfer_info.received + payload_len > transfer_info.total) return;
    if (transfer_info.received + payload_len > MEM_SIZE) return;

    // append each DATA frame to the data_buf
    memcpy(&data_buf[transfer_info.received], payload, payload_len);
    transfer_info.received   += payload_len;
    transfer_info.expect_seq  = seq + 1;

    // once appending to the data buf is done, copy it to the RAM
    if (transfer_info.received == transfer_info.total) {
        printf("Received image from PC!\n");
	    printf("Start sending it to SD card...\n");
        transfer_info.active = 0;
        send_photo_to_SD();
    }
}

static void search_next_image() {
    while (1) {
        uart_rx();
        if (photo_offset <= count_photo) {
            char filename[32];
            snprintf(filename, sizeof(filename), "image%lu.bmp", (unsigned long)photo_offset);
            display_color_image(filename);
        }
        for (volatile int i = 0; i < 0xF00000; i++) {
            uart_rx();
        }
        photo_offset++;
        if (photo_offset > count_photo) {
            photo_offset = 1;
        }
    }

    while (dummy_flag) {
        uart_rx();
        if (image_done) {
            printf("Start sending it to SD card...\n");
            send_photo_to_SD();
            image_done = 0;
            dummy_flag = 0;
        }
    }
}

// ======================================================================
// Functions
// ======================================================================
int main(void) {
    printf("Start receiving photo...\n");
    search_next_image();
}
