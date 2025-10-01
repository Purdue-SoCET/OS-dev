#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "uart_read_byte_nonblock.h"

// SLIP and Protocol Constant
#define END      0xC0
#define ESC      0xDB
#define ESC_END  0xDC
#define ESC_ESC  0xDD

#define TYPE_META 0x01
#define TYPE_DATA 0x02
#define TYPE_ACK  0x10
#define MAX_FRAME (8u * 1024u)

// base address: 0x80000000u, size: 64MB

#define MEM_BASE ((volatile uint8_t*)0x80000000u)
#define MEM_SIZE (64u * 1024u * 1024u)  // 64MB 예시

static uint8_t frame_buf[MAX_FRAME];
static uint32_t frame_num = 0;

// function declarations
static void handle_frame(uint8_t *buf, uint32_t frame_num);
static void handle_meta(const uint8_t *buf, uint32_t frame_num);
static void handle_data(const uint8_t *buf, uint32_t frame_num);

/* ===== SLIP receive state ===== */
typedef enum { 
    ST_IDLE = 0, 
    ST_IN, 
    ST_ESC 
} slip_state_t;



/* ===== META struct ===== */
typedef struct {
    uint32_t file_id;
    uint32_t total;
    uint32_t received;
    uint16_t chunk;
    uint32_t expect_seq;
    volatile uint8_t *write_base;   // RAM start address
    volatile uint8_t *write_ptr;    // current writing pointer
    bool active;
    uint8_t  fname_len;
    char     fname[256];
} transfer_info_t;

static slip_state_t state = ST_IDLE;
static transfer_info_t transfer_info;

// little endians, 16bits & 32 bits
static inline uint16_t rd16(const uint8_t *p){
    uint16_t output = (uint16_t)p[0] | ((uint16_t)p[1] << 8);
    return output;
}
static inline uint32_t rd32(const uint8_t *p){
    uint32_t output = (uint32_t)p[0]
                    | ((uint32_t)p[1] << 8)
                    | ((uint32_t)p[2] << 16)
                    | ((uint32_t)p[3] << 24);
    return output;
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
            if (state == ST_IN) {
                state = ST_ESC;
            }
            return;
        
        default:
            if (state == ST_ESC) {
                if (byte == ESC_END)
                    byte = END;
                else if (byte == ESC_ESC) 
                    byte = ESC;
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

static void handle_meta(const uint8_t *buf, uint32_t frame_num){
    const uint32_t MIN_META = 13u; 
    if (frame_num < MIN_META) return;
    uint32_t file_id    = rd32(buf + 2);
    uint32_t total      = rd32(buf + 6);
    uint16_t chunk_size = rd16(buf + 10);
    uint8_t  fname_len  = buf[12];

    if (13u + (uint32_t)fname_len > frame_num) return;  // check the range
    if (total > MEM_SIZE) return;                       // preventing the size getting bigger than the RAM size
    if (fname_len > 255) fname_len = 255;

    // reset all transfer_into struct to be ready for the DATA session
    memset(&transfer_info, 0, sizeof(transfer_info));
    transfer_info.file_id     = file_id;
    transfer_info.total       = total;
    transfer_info.chunk       = chunk_size;
    transfer_info.received    = 0;
    transfer_info.expect_seq  = 0;

    transfer_info.write_base  = MEM_BASE;             
    transfer_info.write_ptr   = MEM_BASE;

    transfer_info.fname_len   = fname_len;
    if (fname_len) {
        memcpy(transfer_info.fname, (const char *)(buf + 13), fname_len);
    }
    transfer_info.fname[fname_len] = '\0';

    transfer_info.active = true;
}

static void handle_data(const uint8_t *buf, uint32_t frame_num){
    const uint32_t MIN_DATA = 11u; // size of DATA header = 11B
    if (!transfer_info.active) return;
    if (frame_num < MIN_DATA) return;

    // header parsing
    uint32_t file_id = rd32(buf + 1);
    uint32_t seq     = rd32(buf + 5);
    uint16_t payload_len  = rd16(buf + 9);

    // 2) length check
    if (11u + (uint32_t)payload_len > frame_num) return;  // payload length > frame -> false
    const uint8_t *payload = buf + 11; // point to the image data

    // 3) order check before append the data to the RAM
    if (file_id != transfer_info.file_id) return;    
    if (seq     != transfer_info.expect_seq) return;

    // 4) RAM range valid check
    if (transfer_info.received + payload_len > transfer_info.total) return;
    uint32_t used = (uint32_t)(transfer_info.write_ptr - transfer_info.write_base);
    if (used + payload_len > MEM_SIZE) return; 

    // 5) append payload to the RAM
    memcpy((void*)transfer_info.write_ptr, payload, payload_len);
    transfer_info.write_ptr  += payload_len;
    transfer_info.received   += payload_len;
    transfer_info.expect_seq = seq + 1;

    // 6) Done, transfer_info.total from META
    if (transfer_info.received == transfer_info.total) {
        transfer_info.active = false;
        // fatfs, SD card
    }
}

// main task for receiving slip frames
void x07_receiver_task(void) {
    for (;;) {
        uint8_t b;
        while(uart_read_byte_nonblock(&b)) {
            split_byte_stream(b);
        }
    }
}

int main(void){
    x07_receiver_task();
    return 0;
}

