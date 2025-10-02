#ifndef PALH_H_
#define PALH_H_

#include <stdint.h>

// IO definitions
#define __I  volatile const // Defines 'read-only' permissions
#define __O  volatile       // Defines 'write-only' permissions
#define __IO volatile       // Defines 'read/write' permissions

// Possible logic levels settings
// enum LogicLevel {
//     Low,
//     High
// };

// Peripheral memory map
#define ROM_BASE            ((uint32_t)0x00000000)
#define SELFTEST_BASE       ((uint32_t)0x00000200)
#define FF_RAM_BASE         ((uint32_t)0x00008400)
#define TOPRAM_BASE         ((uint32_t)0x00008000)
#define SRAM_BASE           ((uint32_t)0x00020200)
#define GPIO0_BASE          ((uint32_t)0x80000000)
#define GPIO1_BASE          ((uint32_t)0x80000100)
#define GPIO2_BASE          ((uint32_t)0x80000200)
#define GPIO3_BASE          ((uint32_t)0x80000300)
#define PWM_BASE            ((uint32_t)0x80001000)
#define TIMER_BASE          ((uint32_t)0x80002000)
#define SPI_BASE            ((uint32_t)0x80004000)
#define IO_MUX_BASE         ((uint32_t)0x80005000)
#define CLINT_BASE          ((uint32_t)0x90010000)
#define DMA_BASE            ((uint32_t)0x90001000)
#define UART_BASE           ((uint32_t)0x90002000)
#define PLIC_BASE           ((uint32_t)0xA0000000)

// IO Mux alternate pin functions
#define IOM_F0_UART_TX  (1<<0)
#define IOM_F0_UART_RX  (1<<2)
#define IOM_F0_I2C_SCL  (1<<4)
#define IOM_F0_I2C_SDA  (1<<6)
#define IOM_F0_SPI_SS   (1<<8)
#define IOM_F0_SPI_SCK  (1<<10)
#define IOM_F0_SPI_MOSI (1<<12)
#define IOM_F0_SPI_MISO (1<<14)
#define IOM_F0_PWM0_0   (1<<16)
#define IOM_F0_PWM0_1   (1<<18)
#define IOM_F1_TIM0_CC0 (1<<0)
#define IOM_F1_TIM0_CC1 (1<<2)
#define IOM_F1_TIM0_CC2 (1<<4)
#define IOM_F1_TIM0_CC3 (1<<6)
#define IOM_F1_TIM0_CC4 (1<<8)
#define IOM_F1_TIM0_CC5 (1<<10)
#define IOM_F1_TIM0_CC6 (1<<12)
#define IOM_F1_TIM0_CC7 (1<<14)

// PWM constants
#define PWM_CTRL_EN    (1<<0)
#define PWM_CTRL_POL   (1<<1)
#define PWM_CTRL_ALIGN (1<<2)

// SPI constants
// Control register fields
#define SPI_CR_TX_START  (1<<31)
#define SPI_CR_MODE      (1<<30)
#define SPI_CR_CLK_POL   (1<<29)
#define SPI_CR_CLK_PHASE (1<<28)
#define SPI_CR_SSOE      (1<<27)
#define SPI_CR_BIT_ORDER (1<<26)
#define SPI_CR_MODF_EN   (1<<25)
#define SPI_CR_RX_WM     (0x1f<<20)
#define SPI_CR_TX_WM     (0x1f<<15)
#define SPI_CR_TC_IE     (1<<14)
#define SPI_CR_RXWM_IE   (1<<13)
#define SPI_CR_TXWM_IE   (1<<12)
#define SPI_CR_RXF_IE    (1<<11)
#define SPI_CR_TXE_IE    (1<<10)
#define SPI_CR_MODF_IE   (1<<9)
// Status register fields
#define SPI_SR_COMPLETE (1<<31)
#define SPI_SR_RXWM_HIT (1<<30)
#define SPI_SR_TXWM_HIT (1<<29)
#define SPI_SR_RXF      (1<<28)
#define SPI_SR_TXE      (1<<27)
#define SPI_SR_MODF     (1<<26)
#define SPI_SR_RX_CNT   (0x1f<<21)
#define SPI_SR_TX_CNT   (0x1f<<16)

// Timer constants
#define TIM_N_CHANNELS     8
// Control register fields
#define TIM_TCR_IRQ_EN     (1<<1)
#define TIM_TCR_EN         (1<<7)
// Prescale maximum
#define TIM_TPSC_MAX       0xffffffff
// Auto-reload maximum
#define TIM_TARR_MAX       0xffffffff
// CC channel control register fields
#define TIM_TCCMR_EN       (1<<0)
#define TIM_TCCMR_RISING   (0<<1)
#define TIM_TCCMR_FALLING  (1<<1)
#define TIM_TCCMR_BOTH     (3<<1)
#define TIM_TCCMR_OUTPUT   (0<<3)
#define TIM_TCCMR_TI1_IN   (1<<3)
#define TIM_TCCMR_TI2_IN   (2<<3)
#define TIM_TCCMR_FROZEN   (0<<5)
#define TIM_TCCMR_MATCH_HI (1<<5)
#define TIM_TCCMR_MATCH_LO (2<<5)
#define TIM_TCCMR_TOGGLE   (3<<5)
#define TIM_TCCMR_FORCE_LO (4<<5)
#define TIM_TCCMR_FORCE_HI (5<<5)
#define TIM_TCCMR_PWM1     (6<<5)
#define TIM_TCCMR_PWM2     (7<<5)
#define TIM_TCCMR_IRQ_EN   (1<<8)

// DMA constants
// Control register fields
#define DMA_CR_EN      (1<<0)
#define DMA_CR_TCIE    (1<<1)
#define DMA_CR_HTIE    (1<<2)
#define DMA_CR_SRC     (1<<4)
#define DMA_CR_DST     (1<<5)
#define DMA_CR_PSIZE_0 (1<<6)
#define DMA_CR_PSIZE_1 (1<<7)
#define DMA_CR_PSIZE   (0x3<<6)
#define DMA_CR_TE      (1<<8)
#define DMA_CR_CIRC    (1<<10)
#define DMA_CR_IDST    (1<<11)
#define DMA_CR_ISRC    (1<<12)
// Status register fields
#define DMA_SR_COMPLETE (1<<0)
#define DMA_SR_ERROR    (1<<1)

#define N_INTS 64 // make sure this value matches the NUM_INTERRUPTS param in aftx07.sv

// IRQ mappings
enum IRQMap {
    Dflt  = 0,
    Exc   = 1,
    MSoft = 2,
    MTim  = 3,
    MExt  = 4
};

// GPIO register block
typedef struct {
    __IO uint8_t data;
    uint8_t reserved0;
    uint16_t reserved1;
    __IO uint32_t ddr;
    __IO uint32_t ier;
    __IO uint32_t per;
    __IO uint32_t ner;
    __IO uint32_t icr;
    __IO uint32_t isr;
} GPIORegBlk;

// Availale GPIO pins
enum GPIOPin {
    Pin0 = 1 << 0,
    Pin1 = 1 << 1,
    Pin2 = 1 << 2,
    Pin3 = 1 << 3,
    Pin4 = 1 << 4,
    Pin5 = 1 << 5,
    Pin6 = 1 << 6,
    Pin7 = 1 << 7
};

// SPI register block
typedef struct {
    __IO uint32_t cr0;
    __IO uint32_t blr0;
    __IO uint32_t brr0;
    __O uint8_t txdr0;
    uint8_t reserved0;
    uint16_t reserved1;
    __I uint8_t rxdr0;
    uint8_t reserved2;
    uint16_t reserved3;
    __IO uint32_t sr0;
} SPIRegBlk;

// PWM register block
typedef struct {
    __O uint32_t per0;
    __O uint32_t duty0;
    __O uint8_t ctrl0;
    uint8_t reserved0;
    uint16_t reserved1;
    __O uint32_t per1;
    __O uint32_t duty1;
    __O uint8_t ctrl1;
    uint8_t reserved2;
    uint16_t reserved3;
/*  only 2 PWM channels currently
    __O uint32_t per2;
    __O uint32_t duty2;
    __O uint8_t ctrl2;
    uint8_t reserved4;
    uint16_t reserved5;
    __O uint32_t per3;
    __O uint32_t duty3;
    __O uint8_t ctrl3;
    uint8_t reserved6;
    uint16_t reserved7;
    __O uint32_t per4;
    __O uint32_t duty4;
    __O uint8_t ctrl4;
    uint8_t reserved8;
    uint16_t reserved9;
    __O uint32_t per5;
    __O uint32_t duty5;
    __O uint8_t ctrl5;
    uint8_t reserved10;
    uint16_t reserved11;
    __O uint32_t per6;
    __O uint32_t duty6;
    __O uint8_t ctrl6;
    uint8_t reserved12;
    uint16_t reserved13;
    __O uint32_t per7;
    __O uint32_t duty7;
    __O uint8_t ctrl7;
*/
} PWMRegBlk;

// Timer register block
typedef struct {
    __IO uint32_t tcnt;
    __IO uint32_t tcr;
    __IO uint32_t tpsc;
    __IO uint32_t tarr;
    __IO uint32_t tccmr[TIM_N_CHANNELS];
    __IO uint32_t tccr[TIM_N_CHANNELS];
} TimerRegBlk;

// UART register block
typedef struct {
    __IO uint32_t rxstate;
    __I uint32_t rxdata;
    __IO uint32_t txstate;
    __IO uint32_t txdata;
} UARTRegBlk;

// Digital IO Mux register block
typedef struct {
    __IO uint32_t fsel0;  // pin0-15
    __IO uint32_t fsel1;  // pin16-31
} IOMuxRegBlk;

// CLINT register block
typedef struct {
    __IO uint32_t msip[4095];
    uint32_t reserved0;
    struct {
        __IO uint32_t l;
        __IO uint32_t h;
    } mtimecmp[4095];
    struct {
        __IO uint32_t l;
        __IO uint32_t h;
    } mtime;
} CLINTRegBlk;

#define MAX_CTX (15872)
#define MAX_SOURCES (1024)

// The monster
typedef struct {
    __IO uint32_t priority[MAX_SOURCES]; // WARNING: sources[0] is reserved!
    __IO uint32_t pending[MAX_SOURCES / 32];
    uint32_t reserved0[(0x2000 - 0x107C - 4) / 4];
    __IO uint32_t enable[MAX_SOURCES * MAX_CTX / 32];
    uint32_t reserved1[(0x200000 - 0x1F1FFC - 4) / 4];
    struct {
        __IO uint32_t threshold;
        __IO uint32_t claim_complete;
        uint32_t reserved2[(0xFFC - 4) / 4];
    } cfg[MAX_CTX];
} PLICRegBlk;

#undef MAX_CTX
#undef MAX_SOURCES

// PLIC Register block
// The address space here is so huge and somewhat irregular, that the
// struct method will not work well. Instead, we'll have to provide macros to access PLIC registers
// Note: The returned address is ONLY legal if interrupt_num is < N_INTS
#define PLIC_PRIORITY(base, interrupt_num) \
    (__IO uint32_t *)((uint32_t)(base) + (interrupt_num)*4)

// Careful! This is a pointer to a register containing enables for 32 interrupts,
// whereas the rest of these give pointers to per-interrupt registers!
#define PLIC_ENABLE(base, interrupt_num, context_num) \
    (__IO uint32_t *)((uint32_t)(base) + 0x2000 + 0x80*(context_num) + (interrupt_num)/32)

#define PLIC_PRIORITY_THRESHOLD(base, context_num) \
    (__IO uint32_t *)((uint32_t)(base) + 0x200000 + 0x1000*(context_num))

#define PLIC_CLAIM_COMPLETE(base, context_num) \
    (__IO uint32_t *)((uint32_t)(base) + 0x200004 + 0x1000*(context_num))

// DMA register block
typedef struct {
    uint32_t reserved;
    __IO uint32_t sar;
    __IO uint32_t dar;
    __IO uint32_t tsr;
    __IO uint32_t cr;
    __I uint32_t sr;
} DMARegBlk;

#endif /* PAL_H_ */
