#include <stdint.h>
#include <stdio.h>

// reflect N-bit value (used for reflect_in / reflect_out behavior)
static uint32_t reflect_bits(uint32_t v, int width) {
    uint32_t r = 0;
    for (int i = 0; i < width; i++) {
        if (v & (1u << i)) {
            r |= 1u << (width - 1 - i);
        }
    }
    return r;
}

static uint32_t crc_sw_model_lsb(uint64_t data_in,
                                 uint32_t polynomial,
                                 uint32_t seed,
                                 uint32_t final_xor,
                                 int ref_in,
                                 int ref_out,
                                 int xor_en)
{
    uint32_t crc = seed;

    // process 8 bytes, still low byte first (like hw feeding low word then high word)
    for (int byte_i = 0; byte_i < 8; byte_i++) {
        uint8_t b = (data_in >> (8 * byte_i)) & 0xFFu;

        // reflect input byte if reflect_in = 1
        if (ref_in) {
            uint8_t rb = 0;
            for (int k = 0; k < 8; k++) {
                if (b & (1u << k)) {
                    rb |= 1u << (7 - k);
                }
            }
            b = rb;
        }

        // now push this byte bit-by-bit, LSB first
        for (int bit = 0; bit < 8; bit++) {
            uint32_t bit_in = (b >> bit) & 1u;   // next incoming bit

            // XOR incoming bit into LSB of crc
            uint32_t lsb = (crc ^ bit_in) & 1u;

            crc >>= 1;  // shift right

            if (lsb) {
                crc ^= polynomial;
            }
        }
    }

    // reflect_out: reflect final CRC before XOR stage, if enabled
    if (ref_out) {
        crc = reflect_bits(crc, 32);
    }

    // final XOR stage if xor_en
    if (xor_en) {
        crc ^= final_xor;
    }

    return crc;
}

static void run_case(const char *label,
                     uint64_t data_in,
                     uint32_t polynomial,
                     uint32_t crc_in,
                     uint32_t final_xor_val,
                     int reflect_in,
                     int reflect_out,
                     int xor_en,
                     uint32_t exp_crc_out)
{
    uint32_t got = crc_sw_model_lsb(data_in,
                                    polynomial,
                                    crc_in,
                                    final_xor_val,
                                    reflect_in,
                                    reflect_out,
                                    xor_en);

    printf("%s\n", label);
    printf("  expected: 0x%08X\n", exp_crc_out);
    printf("       got: 0x%08X  %s\n\n",
           got,
           (got == exp_crc_out ? "[OK]" : "[MISMATCH]"));
}

int main(void)
{
    // from tb_crc.sv
    // case 1:
    // expected=0x36CE4FEC
    run_case(
        "CRC-32: with 8 bytes data, no reflect",
        0x766970736F636574ull,
        0x04C11DB7u,
        0x00000000u,
        0x00000000u,
        0,
        0,
        0, // xor_en? final_xor=0 anyway
        0x36CE4FECu
    );

    // case 2:
    // reflect_in=1, reflect_out=0
    // expected=0x287DEEE5
    run_case(
        "CRC-32: reflect in, no reflect out",
        0x766970736F636574ull,
        0x04C11DB7u,
        0x00000000u,
        0x00000000u,
        1,
        0,
        0,
        0x287DEEE5u
    );

    // case 3:
    // reflect_in=1, reflect_out=1
    // expected=0xA777BE14
    run_case(
        "CRC-32: reflect in, reflect out",
        0x766970736F636574ull,
        0x04C11DB7u,
        0x00000000u,
        0x00000000u,
        1,
        1,
        0,
        0xA777BE14u
    );

    // case 4:
    // reflect_in=1, reflect_out=0, crc_in=0xFFFF_FFFF
    // expected=0x417955BC
    run_case(
        "CRC-32: reflect in, init=0xFFFF_FFFF, no reflect out",
        0x766970736F636574ull,
        0x04C11DB7u,
        0xFFFFFFFFu,
        0x00000000u,
        1,
        0,
        0,
        0x417955BCu
    );

    // case 5:
    // reflect_in=0, reflect_out=1,
    // final_xor_val=0xFFFF_FFFF,
    // expected=0xC80D8C93
    run_case(
        "CRC-32: no reflect in, final XOR=0xFFFF_FFFF, reflect out",
        0x766970736F636574ull,
        0x04C11DB7u,
        0x00000000u,
        0xFFFFFFFFu,
        0,
        1,
        1, // xor_en yes here
        0xC80D8C93u
    );

    // case 6:
    // reflect_in=0, reflect_out=0
    // expected=0xEF26C3B7
    run_case(
        "Changed input and poly",
        0x4637234586740986ull,
        0x06782345u,
        0x00000000u,
        0xFFFFFFFFu,
        0,
        0,
        1, // xor_en yes
        0xEF26C3B7u
    );

    return 0;
}
