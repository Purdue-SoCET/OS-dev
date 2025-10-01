#include <stdint.h>
#include "pal.h"   

#define BAUD_CYCLES 2604

// define the function
int uart_read_byte_nonblock(uint8_t *out);

static UARTRegBlk *const uart = (UARTRegBlk *)UART_BASE;

static uint8_t rx_sw_buf[3];
static uint8_t rx_sw_cnt = 0;  // remaining byte count
static uint8_t rx_sw_idx = 0;  // next index

// set the baud rate
static inline void uart_init_for_rx(void) {
    uart->rxstate = (BAUD_CYCLES / 16) << 16;  // RX
    uart->txstate = BAUD_CYCLES << 16;         // TX
}

/* non blocking uart byte receive
 * success -> return 1, store
 * fail -> return 0
 */
int uart_read_byte_nonblock(uint8_t *out) {
    if (rx_sw_cnt) {
        *out = rx_sw_buf[rx_sw_idx++];
        rx_sw_cnt--;
        return 1;
    }

    // rxstate bit0 == avail
    uint32_t state = uart->rxstate;
    if ((state & 0x1u) == 0) {
        return 0; // no byte to read
    }

    // read rxdata
    uint32_t data = uart->rxdata;
    uint8_t fifoCount = (uint8_t)(data >> 24);
    if (fifoCount == 0) fifoCount = 1; // 하드웨어 규칙: 0은 1로 취급
    if (fifoCount > 3) fifoCount = 3;  // 최대 3바이트 보호

    // store rightmost, LSB
    rx_sw_buf[0] = (uint8_t)( data        & 0xFFu );
    rx_sw_buf[1] = (uint8_t)((data >> 8)  & 0xFFu );
    rx_sw_buf[2] = (uint8_t)((data >> 16) & 0xFFu );

    rx_sw_cnt = fifoCount;
    rx_sw_idx = 0;

    // return a byte
    *out = rx_sw_buf[rx_sw_idx++];
    rx_sw_cnt--;
    return 1;
}