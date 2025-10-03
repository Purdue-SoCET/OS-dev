#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "pal.h"

#define BAUD_CYCLES 2604
#define MEM_BASE ((volatile uint8_t*)0x80000000u)
#define MEM_SIZE (10 * 1024)   

// SLIP FRAMING CONSTANT
#define END 0xC0
#define ESC 0xDB
#define ESC_END 0xDC
#define ESC_ESC 0xDD

#define TYPE_META 0x01
#define TYPE_DATA 0x02
#define MAX_FRAME (10 * 1024)

// transfer_info struct
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

// SLIP State
typedef enum { 
    ST_IDLE = 0, 
    ST_IN, 
    ST_ESC 
} slip_state_t;

// Function Declaration
static inline uint16_t rd16(const uint8_t *p);
static inline uint32_t rd32(const uint8_t *p);
static void uart_init(void);
static void uart_rx(void);
static void split_byte_stream(uint8_t byte);
static void handle_frame(uint8_t *buf, uint32_t frame_num);
static void handle_meta(const uint8_t *buf, uint32_t frame_num);
static void handle_data(const uint8_t *buf, uint32_t frame_num_in);

static transfer_info_t transfer_info;
static UARTRegBlk *const uart = (UARTRegBlk *)UART_BASE;
static uint8_t frame_buf[MAX_FRAME];
static uint32_t frame_num = 0;
static uint8_t data_buf[MEM_SIZE];
static slip_state_t state = ST_IDLE;

static inline uint16_t rd16(const uint8_t *p);
static inline uint32_t rd32(const uint8_t *p);
static void handle_frame(uint8_t *buf, uint32_t frame_num);
static void handle_meta (const uint8_t *buf, uint32_t frame_num);
static void handle_data (const uint8_t *buf, uint32_t frame_num);

static void __init_uart(void) __attribute__((constructor));
static void __init_uart(void)
{
    uart->rxstate = (BAUD_CYCLES / 16) << 16;
    uart->txstate = BAUD_CYCLES << 16;
}

// =========================== FUNCTIONS =============================
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
    //printf("uart_rx starts here!\n");
    if ((uart->rxstate) & 0x1) {
        uint32_t rxdata = uart -> rxdata;
        uint8_t fifoCount = rxdata >> 24;
        // if (fifoCount == 0) fifoCount = 1;   
        // if (fifoCount > 3)  fifoCount = 3;
        for (uint8_t i = 0; i < fifoCount; i++) {
            uint8_t b = rxdata & 0xFF;
            split_byte_stream(b);
            rxdata >> 9;
        }
    }
    else if (uart->rxstate & 0x2) {
        //
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
        printf("TYPE_META\n");
	handle_meta(buf, frame_num); 
    } 
    else if (type == TYPE_DATA) {
	printf("TYPE_DATA\n");
        handle_data(buf, frame_num); 
    }
    else {
        // other type -> no need to handle so far
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
    //if (13u + (uint32_t)fname_len > frame_num) return;  // check the range
    //if (total_size > MEM_SIZE) return;                       // preventing the size getting bigger than the RAM size
    //if (fname_len > 255) fname_len = 255;
    
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
    printf("Before checking active\n");
    if (transfer_info.active == 0) return;
    printf("Before checking min data\n");
    if (frame_num_in < MIN_DATA) return;

    uint32_t file_id     = rd32(buf + 1);
    uint32_t seq         = rd32(buf + 5);
    printf("seq : %d\n", seq);
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
        printf("Success!\n");
	//memcpy((void*)MEM_BASE, data_buf, transfer_info.total);
        transfer_info.active = 0;
        // FatFs, SD card
    }
}
// =========================== Main =============================


int main(void) {
    printf("program starts here!\n");
    for (;;) {
        uart_rx();
    }


}
