#include <stdint.h>
#include <inttypes.h>

// CRC Subordinate Base Address
#define CRC_BASE 0x90003000u   // Base address for the CRC peripheral

// Register Offsets (Word-Aligned)
#define CRC_CTRL 0x00  // Control Register: [0]=EN [1]=INIT [2]=FINALIZE [3]=REF_IN [4]=REF_OUT [5]=XOR_EN
#define CRC_POLY 0x04  // Polynomial register
#define CRC_INIT 0x08  // Initial seed register
#define CRC_XORO 0x0C  // Final XOR constant
#define CRC_DATA 0x10  // Data input register (write-only)
#define CRC_LEN  0x14  // Processed byte counter
#define CRC_STAT 0x18  // Status: [0]=BUSY, [1]=DONE
#define CRC_RES  0x1C  // Final result register

// MMIO helpers
// These helper functions directly write/read to hardware registers
static inline void mmio_write32(uintptr_t addr, uint32_t data) {
  *(volatile uint32_t *)addr = data;  // Write 32-bit data to given address
}

static inline uint32_t mmio_read32(uint32_t addr) {
  return *(volatile uint32_t *)addr;  // Read 32-bit value from given address
}

// Debug print placeholder (for UART or MMIO printer if available)
static inline void dbg_hex32(uint32_t v) {
  (void)v; // Currently unused – connect to UART or printer for debugging
}

// CRC Control Helper Functions
// Set up CRC configuration registers (polynomial, seed, XOR constant, control bits)
static void crc_set_config(uint32_t poly, uint32_t init_seed, uint32_t xorout, int en, int ref_in, int ref_out, int xor_en)
{
  // Write polynomial, initial seed, and XOR constant
  mmio_write32(CRC_BASE + CRC_POLY, poly);
  mmio_write32(CRC_BASE + CRC_INIT, init_seed);
  mmio_write32(CRC_BASE + CRC_XORO, xorout);

  // Construct CTRL register bits
  uint32_t ctrl = 0;
  if (en) ctrl |= (1u << 0);  // Enable bit
  if (ref_in) ctrl |= (1u << 3);  // Reflect input data
  if (ref_out) ctrl |= (1u << 4);  // Reflect output result
  if (xor_en) ctrl |= (1u << 5);  // Enable final XOR stage
  mmio_write32(CRC_BASE + CRC_CTRL, ctrl);
}

// Generate a single INIT pulse to start a CRC session
static void crc_init_pulse(void)
{
  uint32_t ctrl = mmio_read32(CRC_BASE + CRC_CTRL);
  ctrl |=  (1u << 1);                      // Set INIT=1 (start new session)
  mmio_write32(CRC_BASE + CRC_CTRL, ctrl);
  ctrl &= ~(1u << 1);                      // Clear INIT=0 (optional, ends pulse)
  mmio_write32(CRC_BASE + CRC_CTRL, ctrl);
}

// Write one 32-bit data word to the CRC engine (folded LSB-first internally)
static void crc_push_word(uint32_t w)
{
  mmio_write32(CRC_BASE + CRC_DATA, w);  // Feed one word to DATA register
}

// Trigger FINALIZE pulse and wait until CRC computation completes
static uint32_t crc_finalize_and_read(void)
{
  // Set FINALIZE=1 to compute the final CRC
  uint32_t ctrl = mmio_read32(CRC_BASE + CRC_CTRL);
  ctrl |=  (1u << 2);
  mmio_write32(CRC_BASE + CRC_CTRL, ctrl);
  ctrl &= ~(1u << 2); // Clear pulse (optional)
  mmio_write32(CRC_BASE + CRC_CTRL, ctrl);

  // Poll BUSY flag until computation finishes
  while (mmio_read32(CRC_BASE + CRC_STAT) & 0x1u) {
    /* Wait while BUSY=1 */
  }

  // Read and return final CRC result
  return mmio_read32(CRC_BASE + CRC_RES);
}

// Run One Test Case (Single CRC Calculation)
// msg64: 8-byte message (two 32-bit writes)
// poly, seed, xorout: CRC parameters
// ref_in/ref_out/xor_en: control bits for reflection and XOR
static uint32_t crc_run_case(uint64_t msg64,
                             uint32_t poly,
                             uint32_t seed,
                             uint32_t xorout,
                             int ref_in, int ref_out, int xor_en)
{
  // 1. Configure CRC engine and start new session
  crc_set_config(poly, seed, xorout, /*en=*/1, ref_in, ref_out, xor_en);
  crc_init_pulse();

// 2. Feed two 32-bit words (upper word first, lower word second)
crc_push_word((uint32_t)((msg64 >> 32) & 0xFFFFFFFFu)); 
crc_push_word((uint32_t)(msg64 & 0xFFFFFFFFu));         


  // 3. Finalize CRC computation and return result
  return crc_finalize_and_read();
}

// Main Function (Smoke Test)
// Executes six predefined test cases and prints results
int main(void)
{
  // Example 8-byte message: ASCII "vipsocet" → 0x766970736F636574
  const uint64_t MSG = 0x766970736F636574ull;

  // Case 1: Standard CRC-32, no reflection, seed=0, xorout=0 → expected 0x36CE4FEC
  uint32_t g1 = crc_run_case(MSG, 0x04C11DB7u, 0x00000000u, 0x00000000u,
                             0, 0, 0);
  dbg_hex32(g1);

  // Case 2: Reflect input only → expected 0x287DEEE5
  uint32_t g2 = crc_run_case(MSG, 0x04C11DB7u, 0x00000000u, 0x00000000u,
                             1, 0, 0);
  dbg_hex32(g2);

  // Case 3: Reflect both input and output → expected 0xA777BE14
  uint32_t g3 = crc_run_case(MSG, 0x04C11DB7u, 0x00000000u, 0x00000000u,
                             1, 1, 0);
  dbg_hex32(g3);

  // Case 4: Seed = 0xFFFFFFFF, reflect input only → expected 0x417955BC
  uint32_t g4 = crc_run_case(MSG, 0x04C11DB7u, 0xFFFFFFFFu, 0x00000000u,
                             1, 0, 0);
  dbg_hex32(g4);

  // Case 5: Reflect output and apply final XOR → expected 0xC80D8C93
  uint32_t g5 = crc_run_case(MSG, 0x04C11DB7u, 0x00000000u, 0xFFFFFFFFu,
                             0, 1, 1);
  dbg_hex32(g5);

  // Case 6: Custom polynomial and message → expected 0xEF26C3B7
  uint32_t g6 = crc_run_case(0x4637234586740986ull,
                             0x06782345u, 0x00000000u, 0xFFFFFFFFu,
                             0, 0, 1);
  dbg_hex32(g6);

  // Infinite loop to keep simulation running (so ModelSim can capture waveform)
  for (;;) { }

  return 0;
}
