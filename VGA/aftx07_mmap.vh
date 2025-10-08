`ifndef AFTX07_MMAP
`define AFTX07_MMAP
    
    localparam int K = 1024;
    localparam int M = K * K;
    localparam int SRAM_SIZE = 2*M;
    localparam int SRAM_N_WORDS = SRAM_SIZE / 4;
    localparam int FF_RAM_SIZE = 64*K;
    localparam int FF_RAM_N_WORDS = FF_RAM_SIZE / 4;

    // SRAM controller config
    localparam int NREG = SRAM_SIZE / 4;

    localparam int ROM_N_WORDS = 4*K / 4;

    `ifndef SYNTHESIS

        localparam int ROM_AHB_IDX      = 0;
        localparam int MEMORY_AHB_IDX   = 1;
        localparam int SRAM_AHB_IDX     = 2;
        localparam int APB_BRIDGE_IDX   = 3;
        localparam int DMA_AHB_IDX      = 4;
        localparam int UART_AHB_IDX     = 5;
        localparam int CLINT_AHB_IDX    = 6;
        localparam int PLIC_AHB_IDX     = 7;
        localparam int PRINTER_AHB_IDX  = 8;
        localparam int MT_AHB_IDX       = 9;
        localparam int VGA_AHB_IDX      = 10;
        localparam int AHB_NSUBORDINATES = 11; // originally was 10

        localparam logic [31:0] AHB_MAP [AHB_NSUBORDINATES] = '{
            32'h400,
            32'h8400,
            32'h20000,
            32'h80000000,
            32'h90001000,
            32'h90002000,
            32'h90010000,
            32'hA0000000,
            32'hB0000000,
            32'hC0000000,
            32'hD0000000
        };

        localparam int GPIO0_APB_IDX = 0;
        localparam int GPIO1_APB_IDX = 1;
        localparam int GPIO2_APB_IDX = 2;
        localparam int GPIO3_APB_IDX = 3;
        localparam int PWM_APB_IDX = 4;
        localparam int TIMER_APB_IDX = 5;
        localparam int SPI_APB_IDX = 6;
        localparam int DIGITAL_IO_MUX_IDX = 7;
        localparam int GPIOBOOT_APB_IDX = 8;
        localparam int APB_NCOMPLETERS= 9;

        localparam logic [31:0] APB_MAP[APB_NCOMPLETERS] = '{
            32'h0, // gpio start
            32'h100,
            32'h200,
            32'h300,
            32'h1000,
            32'h2000,
            32'h4000,
            32'h5000, //io mux offset
            32'h6000
        };

    `else

        localparam int ROM_AHB_IDX       = 0;
        localparam int MEMORY_AHB_IDX    = 1;
        localparam int SRAM_AHB_IDX      = 2;
        localparam int APB_BRIDGE_IDX    = 3;
        localparam int DMA_AHB_IDX       = 4;
        localparam int UART_AHB_IDX      = 5;
        localparam int CLINT_AHB_IDX     = 6;
        localparam int PLIC_AHB_IDX      = 7;
        localparam int MT_AHB_IDX        = 8;
        localparam int VGA_AHB_IDX       = 9;
        localparam int AHB_NSUBORDINATES = 10; // originally was 9

        localparam logic [31:0] AHB_MAP [AHB_NSUBORDINATES] = '{
            32'h400,
            32'h8400,
            32'h20000,
            32'h80000000,
            32'h90001000,
            32'h90002000,
            32'h90010000,
            32'hA0000000,
            32'hC0000000,
            32'hD0000000 //vga 
        };
        
        localparam int GPIO0_APB_IDX = 0;
        localparam int GPIO1_APB_IDX = 1;
        localparam int GPIO2_APB_IDX = 2;
        localparam int GPIO3_APB_IDX = 3;
        localparam int PWM_APB_IDX = 4;
        localparam int TIMER_APB_IDX = 5;
        localparam int SPI_APB_IDX = 6;
        localparam int DIGITAL_IO_MUX_IDX = 7;
        localparam int GPIOBOOT_APB_IDX = 8;
        localparam int APB_NCOMPLETERS= 9;

        localparam logic [31:0] APB_MAP[APB_NCOMPLETERS] = '{
            32'h0, // gpio start
            32'h100,
            32'h200,
            32'h300, // gpio end
            32'h1000,
            32'h2000,
            32'h4000,
            32'h5000, //io mux offset
            32'h6000
        };

    `endif

`endif