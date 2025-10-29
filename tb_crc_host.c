#include <stdint.h>
#include <stdio.h>

// Bit reflection helper (e.g. reflect 0b101100.. into reverse bit order)
static uint32_t reflect_bits(uint32_t v, int width) {
    uint32_t r = 0;
    for (int i = 0; i < width; i++) {
        if (v & (1u << i)) {
            r |= (1u << (width - 1 - i));
        }
    }
    return r;
}

// Software reference model of your CRC unit.
static uint32_t crc_sw_model(uint64_t data_in,
                             uint32_t polynomial,
                             uint32_t seed,
                             uint32_t final_xor_val,
                             int reflect_in,
                             int reflect_out)
{
    uint32_t crc = seed;

    // Break 64-bit data into 8 bytes, LSB-first order
    for (int byte_i = 0; byte_i < 8; byte_i++) {
        uint8_t b = (data_in >> (8 * byte_i)) & 0xFFu;

        // reflect each input byte if reflect_in is set
        if (reflect_in) {
            uint8_t rb = 0;
            for (int k = 0; k < 8; k++) {
                if (b & (1u << k)) {
                    rb |= 1u << (7 - k);
                }
            }
            b = rb;
        }

        // XOR this byte into the top 8 bits of the CRC register
        crc ^= ((uint32_t)b) << 24;

        // Advance 8 cycles through the polynomial
        for (int k = 0; k < 8; k++) {
            if (crc & 0x80000000u) {
                crc = (crc << 1) ^ polynomial;
            } else {
                crc = (crc << 1);
            }
        }
    }

    // reflect final 32-bit CRC if reflect_out requested
    if (reflect_out) {
        crc = reflect_bits(crc, 32);
    }

    // Apply final XOR if nonzero final_xor_val
    if (final_xor_val != 0x00000000u) {
        crc ^= final_xor_val;
    }

    return crc;
}

// one test run: mimic set_input(...) in tb_crc.sv
static void run_case(const char *testname,
                     uint64_t data_in,
                     uint32_t polynomial,
                     uint32_t crc_in,
                     uint32_t final_xor_val,
                     int reflect_in,
                     int reflect_out,
                     uint32_t exp_crc_out)
{
    uint32_t got = crc_sw_model(data_in,
                                polynomial,
                                crc_in,
                                final_xor_val,
                                reflect_in,
                                reflect_out);

    printf("%s\n", testname);
    printf("  expected: 0x%08X\n", exp_crc_out);
    printf("       got: 0x%08X  %s\n\n",
           got,
           (got == exp_crc_out ? "[OK]" : "[MISMATCH]"));
}

int main(void)
{

    run_case(
        "CRC-32: with 8 bytes data, no reflect",
        0x766970736F636574ull,
        0x04C11DB7u,
        0x00000000u,
        0x00000000u,
        0, 
        0, 
        0x36CE4FECu
    );

    run_case(
        "CRC-32: reflect in, no reflect out",
        0x766970736F636574ull,
        0x04C11DB7u,
        0x00000000u,
        0x00000000u,
        1,
        0,
        0x287DEEE5u
    );

    run_case(
        "CRC-32: reflect in, reflect out",
        0x766970736F636574ull,
        0x04C11DB7u,
        0x00000000u,
        0x00000000u,
        1,
        1,
        0xA777BE14u
    );

    run_case(
        "CRC-32: reflect in, init=0xFFFF_FFFF, no reflect out",
        0x766970736F636574ull,
        0x04C11DB7u,
        0xFFFFFFFFu,
        0x00000000u,
        1,
        0,
        0x417955BCu
    );

    run_case(
        "CRC-32: no reflect in, final XOR 0xFFFF_FFFF, reflect out",
        0x766970736F636574ull,
        0x04C11DB7u,
        0x00000000u,
        0xFFFFFFFFu,
        0,
        1,
        0xC80D8C93u
    );

    run_case(
        "Changed input and poly",
        0x4637234586740986ull,
        0x06782345u,
        0x00000000u,
        0xFFFFFFFFu,
        0,
        0,
        0xEF26C3B7u
    );

    return 0;
}
