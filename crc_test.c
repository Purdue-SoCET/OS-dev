#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>

//  USE_UART_PRINT = 0 → 표준 출력 (printf 사용)
//  USE_UART_PRINT = 1 → UART MMIO로 직접 출력
#ifndef USE_UART_PRINT
#define USE_UART_PRINT 0
#endif

#if USE_UART_PRINT
#ifndef UART_BASE
#define UART_BASE ((uintptr_t)0x9000C000u)
#endif
#define UART_TXDATA   0x00u  // TX data register
#define UART_TXFULL   (1u << 31) // TX buffer full flag

static inline void uart_write_ch(char c) {
  volatile uint32_t *tx = (volatile uint32_t*)(UART_BASE + UART_TXDATA);
  while ((*tx) & UART_TXFULL) { /* wait until not full */ }
  *tx = (uint32_t)(uint8_t)c;
}

// UART print helpers (string, newline, 32-bit hex)
static void uprint_str(const char* s) { while (*s) uart_write_ch(*s++); }
static void uprint_nl(void) { uart_write_ch('\n'); }
static void uprint_hex32(uint32_t v) {
  static const char* hex = "0123456789ABCDEF";
  for (int i = 7; i >= 0; --i)
    uart_write_ch(hex[(v >> (i*4)) & 0xF]);
}
#endif

// CRC MMIO mmap
#define CRC_BASE ((uintptr_t)0x90003000u) // CRC Base address
#define CRC_CTRL 0x00  // Control Register: [0]=EN [1]=INIT [2]=FINALIZE [3]=REF_IN [4]=REF_OUT [5]=XOR_EN
#define CRC_POLY 0x04  // Polynomial (다항식)
#define CRC_INIT 0x08  // Initial seed
#define CRC_XORO 0x0C  // Final XOR value
#define CRC_DATA 0x10  // Data input register (write-only)
#define CRC_LEN  0x14  // Processed byte counter (optional)
#define CRC_STAT 0x18  // Status: [0]=BUSY, [1]=DONE
#define CRC_RES  0x1C  // Result (final CRC output)

// MMIO Helper
static inline void mmio_write32(uintptr_t addr, uint32_t data) {
  *(volatile uint32_t *)addr = data;
}
static inline uint32_t mmio_read32(uintptr_t addr) {
  return *(volatile uint32_t *)(addr);
}

// Print Helper
static void print_case(const char* label, uint32_t got, uint32_t exp) {
#if USE_UART_PRINT
  uprint_str(label); uprint_str(": got=0x"); uprint_hex32(got);
  uprint_str("  exp=0x"); uprint_hex32(exp);
  uprint_str("  "); uprint_str((got==exp) ? "OK" : "MISMATCH"); uprint_nl();
#else
  printf("%s: got=0x%08" PRIX32 "  exp=0x%08" PRIX32 "  %s\n",
         label, got, exp, (got==exp) ? "OK" : "MISMATCH");
#endif
}

// Output helper for overall
static void print_summary(int failures) {
#if USE_UART_PRINT
  uprint_str("\nSummary: ");
  uprint_str(failures ? "FAIL" : "PASS");
  uprint_str(" (");
  
  char buf[12]; int i=0;
  if (failures==0) buf[i++]='0';
  else {
    int f=failures; char tmp[12]; int j=0;
    while (f>0){ tmp[j++]='0'+(f%10); f/=10; }
    while (j>0) buf[i++]=tmp[--j];
  }
  buf[i]=0;
  uprint_str(buf);
  uprint_str(" failing case"); if (failures!=1) uprint_str("s");
  uprint_str(")\n");
#else
  printf("\nSummary: %s (%d failing case%s)\n",
         failures ? "FAIL" : "PASS", failures, (failures==1)?"":"s");
#endif
}

// CRC CTRL 
static void crc_set_config(uint32_t poly, uint32_t init_seed, uint32_t xorout,
                           int en, int ref_in, int ref_out, int xor_en)
{
  mmio_write32(CRC_BASE + CRC_POLY, poly);
  mmio_write32(CRC_BASE + CRC_INIT, init_seed);
  mmio_write32(CRC_BASE + CRC_XORO, xorout);

  uint32_t ctrl = 0;
  if (en)      ctrl |= (1u << 0);
  if (ref_in)  ctrl |= (1u << 3);
  if (ref_out) ctrl |= (1u << 4);
  if (xor_en)  ctrl |= (1u << 5);
  mmio_write32(CRC_BASE + CRC_CTRL, ctrl);
}

// CRC INIT 
static void crc_init_pulse(void) {
  uint32_t ctrl = mmio_read32(CRC_BASE + CRC_CTRL);
  ctrl |=  (1u << 1);
  mmio_write32(CRC_BASE + CRC_CTRL, ctrl);
  ctrl &= ~(1u << 1);
  mmio_write32(CRC_BASE + CRC_CTRL, ctrl);
}

// 
static void crc_push_word(uint32_t w) {
  mmio_write32(CRC_BASE + CRC_DATA, w);
}

// Finalize CRC computation and return result
static uint32_t crc_finalize_and_read(void) {
  uint32_t ctrl = mmio_read32(CRC_BASE + CRC_CTRL);
  ctrl |=  (1u << 2);  // FINALIZE=1
  mmio_write32(CRC_BASE + CRC_CTRL, ctrl);
  ctrl &= ~(1u << 2);  // FINALIZE=0
  mmio_write32(CRC_BASE + CRC_CTRL, ctrl);

  // Wait until BUSY=0
  while (mmio_read32(CRC_BASE + CRC_STAT) & 0x1u) { /* wait */ }

  // Return CRC result
  return mmio_read32(CRC_BASE + CRC_RES);
}

// Run one complete CRC test case
static uint32_t crc_run_case(uint64_t msg64,
                             uint32_t poly,
                             uint32_t seed,
                             uint32_t xorout,
                             int ref_in, int ref_out, int xor_en)
{
  // 1. Configure and initialize CRC engine
  crc_set_config(poly, seed, xorout, /*en=*/1, ref_in, ref_out, xor_en);
  crc_init_pulse();

  // 2. Push 8-byte message
  crc_push_word((uint32_t)((msg64 >> 32) & 0xFFFFFFFFu));
  crc_push_word((uint32_t)( msg64        & 0xFFFFFFFFu));

  // 3. Finalize computation and return result
  return crc_finalize_and_read();
}

// Main 
int main(void)
{
  // msg example ("vipsocet")
  const uint64_t MSG = 0x766970736F636574ull;

  // Case 1
  uint32_t g1 = crc_run_case(MSG, 0x04C11DB7u, 0x00000000u, 0x00000000u, 0, 0, 0);
  print_case("Case 1: no reflect", g1, 0x36CE4FECu);

  // Case 2 
  uint32_t g2 = crc_run_case(MSG, 0x04C11DB7u, 0x00000000u, 0x00000000u, 1, 0, 0);
  print_case("Case 2: reflect in", g2, 0x287DEEE5u);

  // Case 3 
  uint32_t g3 = crc_run_case(MSG, 0x04C11DB7u, 0x00000000u, 0x00000000u, 1, 1, 0);
  print_case("Case 3: reflect in/out", g3, 0xA777BE14u);

  // Case 4 
  uint32_t g4 = crc_run_case(MSG, 0x04C11DB7u, 0xFFFFFFFFu, 0x00000000u, 1, 0, 0);
  print_case("Case 4: reflect in, seed=0xFFFFFFFF", g4, 0x417955BCu);

  // Case 5 
  uint32_t g5 = crc_run_case(MSG, 0x04C11DB7u, 0x00000000u, 0xFFFFFFFFu, 0, 1, 1);
  print_case("Case 5: reflect out + final XOR", g5, 0xC80D8C93u);

  // Case 6 
  uint32_t g6 = crc_run_case(0x4637234586740986ull, 0x06782345u, 0x00000000u, 0xFFFFFFFFu, 0, 0, 1);
  print_case("Case 6: custom poly/msg", g6, 0xEF26C3B7u);

  // Summary 
  int failures = (g1!=0x36CE4FECu) + (g2!=0x287DEEE5u) + (g3!=0xA777BE14u) +
                 (g4!=0x417955BCu) + (g5!=0xC80D8C93u) + (g6!=0xEF26C3B7u);
  print_summary(failures);

#if !USE_UART_PRINT
  fflush(stdout); // printf buffer
#endif

//  // Infinite loop to keep simulation running
//   // #define KEEP_ALIVE 1
// #ifdef KEEP_ALIVE
//   for(;;) { /* spin */ }
// #endif

//   return 0;
}
