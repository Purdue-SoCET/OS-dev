#include <stdint.h>
#include "format.h"

#define CRC_BASE ((uintptr_t)0x90003000u)

// Register offsets
#define CRC_CTRL 0x00  // [0]=EN, [1]=INIT (pulse), [2]=FINALIZE (pulse)
#define CRC_DATA 0x04  // write-only input data
#define CRC_RES  0x08  // final CRC result

static inline void mmio_write32(uintptr_t addr, uint32_t data) {
    *(volatile uint32_t *)addr = data;
}

static inline uint32_t mmio_read32(uintptr_t addr) {
    return *(volatile uint32_t *)addr;
}

static void print_string(const char *s) {
    print("%s", s);
}

static void print_hex32(uint32_t v) {
    print("%x", v);
}

// Start a new CRC session: enable hardware + send INIT pulse
static void crc_start_session(void) {
    uint32_t ctrl = 0;
    ctrl |= (1u << 0);  // EN = 1
    ctrl |= (1u << 1);  // INIT pulse = 1
    mmio_write32(CRC_BASE + CRC_CTRL, ctrl);

    // Clear INIT bit while keeping EN active
    ctrl &= ~(1u << 1);
    mmio_write32(CRC_BASE + CRC_CTRL, ctrl);
}

// Send one 32-bit word into the CRC hardware
static void crc_push_word(uint32_t w) {
    mmio_write32(CRC_BASE + CRC_DATA, w);
}

// Finalize the CRC computation and read the result
static uint32_t crc_finalize_and_read(void) {
    uint32_t ctrl = mmio_read32(CRC_BASE + CRC_CTRL);

    ctrl |=  (1u << 2);  // FINALIZE = 1 pulse
    mmio_write32(CRC_BASE + CRC_CTRL, ctrl);

    ctrl &= ~(1u << 2);  // FINALIZE = 0
    mmio_write32(CRC_BASE + CRC_CTRL, ctrl);

    return mmio_read32(CRC_BASE + CRC_RES);
}

int main(void)
{
    const uint64_t MSG = 0x766970736F636574ull; // ASCII for "vipsocet"
    const uint32_t EXP = 0x36CE4FECu;           // Expected CRC result

    print_string("=== CRC HW test ===\n");

    // Begin CRC session
    crc_start_session();

    // Split 64-bit message into two 32-bit words
    uint32_t high = (uint32_t)((MSG >> 32) & 0xFFFFFFFFu);
    uint32_t low  = (uint32_t)( MSG        & 0xFFFFFFFFu);

    print_string("PUSH high: ");  // exp : 76697073
    print_hex32(high); 
    print_string("\n");
    crc_push_word(high);

    print_string("PUSH low : "); // exp : 6f636574
    print_hex32(low);  
    print_string("\n");
    crc_push_word(low);

    // Finalize CRC and check the result
    uint32_t hw_crc = crc_finalize_and_read();
    print_string("HW CRC result: "); // exp : 36ce4fec
    print_hex32(hw_crc);
    print_string("\n");

    if (hw_crc == EXP) {
        print_string("[PASS] CRC matches expected 0x36CE4FEC\n"); // exp : 0x36CE4FEC
    } else {
        print_string("[FAIL] Expected 0x36CE4FEC, got ");
        print_hex32(hw_crc);
        print_string("\n");
    }

    print_string("=== CRC test done ===\n");

    while (1) { /* spin */ }
    return 0;
}
