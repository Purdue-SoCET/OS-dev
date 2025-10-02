#include <stdio.h>
#include <stdint.h>
#include "source/pal.h"

#ifdef UART_STDIO
#define BAUD_CYCLES 2604

static UARTRegBlk *uart = (UARTRegBlk *) UART_BASE;
static GPIORegBlk *gpio = (GPIORegBlk *) GPIO0_BASE;

static int __putc(char c, FILE *file)
{
    (void)file;
    uart->txdata = c;
    while (!(uart->txstate & 0x1)); // Wait for transmission complete
    return c;
}

static int __getc(FILE *file)
{
    (void)file;
    while (!(uart->rxstate & 0x1)); // Wait for byte to be received
    if (uart->rxstate & 0x2) { // Check for error
        return EOF;
    }
    return uart->rxdata & 0xFF;
}

static void __init_uart(void) __attribute__((constructor));
static void __init_uart(void)
{
    // gpio->ddr = 0xFFFF;
    // gpio->data = 0x0;
    
    uart->rxstate = (BAUD_CYCLES / 16) << 16;
    uart->txstate = BAUD_CYCLES << 16;
}

#elif defined(OLED_STDIO)

static GPIORegBlk *gpio_PRINT = (GPIORegBlk *) GPIO0_BASE;
static char print_buf[32];
static int buf_pos = 0;

static void wait(int t) {
    for (int i = 0; i < t; i++);
}

static void write_bit(uint32_t val) {
    uint32_t temp = gpio_PRINT->data;
    if (val) {
        gpio_PRINT->data = (temp & ~0x4) | 0x2;
        wait(0x10);
        gpio_PRINT->data = (temp | 0x4) | 0x2;
        wait(0x10);
        gpio_PRINT->data = (temp & ~0x4) | 0x2;
    } else {
        gpio_PRINT->data = (temp & ~0x4) & ~0x2;
        wait(0x10);
        gpio_PRINT->data = (temp | 0x4) & ~0x2;
        wait(0x10);
        gpio_PRINT->data = (temp & ~0x4) & ~0x2;
    }
}

static void oled_cmd(uint32_t data) {
    gpio_PRINT->data &= ~(0x8);  // SS low
    for (int i = 9; i >= 0; i--) {
        write_bit((data >> i) & 0x1);
    }
    gpio_PRINT->data |= 0x8;  // SS high
    wait(0x400);
}

static void oled_data(uint32_t data) {
    oled_cmd(data | 0x200);
}

static void init_oled(void) {
    gpio_PRINT->ddr &= 0xf1;
    gpio_PRINT->ddr |= 0x0e;
    gpio_PRINT->data |= 0x8;

    wait(0x10000);
    oled_cmd(0x38);
    oled_cmd(0x08);
    oled_cmd(0x01);
    wait(0x10000);
    oled_cmd(0x06);
    oled_cmd(0x02);
    oled_cmd(0x0c);
}

static void display_buffer(void) {
    init_oled();
    
    // First line
    oled_cmd(0x02);
    for(int i = 0; i < 16 && i < buf_pos; i++) {
        oled_data(print_buf[i]);
    }
    
    // Second line
    oled_cmd(0xc0);
    for(int i = 16; i < 32 && i < buf_pos; i++) {
        oled_data(print_buf[i]);
    }
    
    wait(0x101000);
}

static int __putc(char c, FILE *file)
{
    (void)file;
    if (c == '\n' || buf_pos >= 31) {
        print_buf[buf_pos] = '\0';
        display_buffer();
        buf_pos = 0;
    } else {
        print_buf[buf_pos++] = c;
    }
    return c;
}

static int __getc(FILE *file)
{
    (void)file;
    return EOF;
}

#else

volatile char *MAGIC_ADDR = (volatile char *)0xB0000000;

static int __putc(char c, FILE *file)
{
    (void)file;
    *MAGIC_ADDR = c;
    return c;
}

static int __getc(FILE *file)
{
    (void)file;
    return EOF;
}

#endif

static FILE __stdio = FDEV_SETUP_STREAM(__putc, __getc, NULL, _FDEV_SETUP_RW);
FILE *const stdin = &__stdio;
__strong_reference(stdin, stdout);
__strong_reference(stdin, stderr);
