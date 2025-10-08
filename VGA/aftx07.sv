`include "core_interrupt_if.vh"
`include "aftx07_macros.vh"
`include "aftx07_mmap.vh"

module aftx07 #(
    parameter logic [31:0] RESET_PC = 32'h400,
    parameter int GPIO_PINS_PER_PORT = 8,
    parameter int BOOT_PINS = 3,
    parameter int PWM_CHANNELS = 2,
    parameter int IO_MUX_NUM_PINS = 32, // max:32
    parameter int IO_MUX_NUM_FUNC = 2, // max:4
    parameter int IO_MUX_NUM_BITS = 2, // max:2 origina:1
    parameter int IO_MUX_NUM_REGS = 2, // max:2
    parameter int TIMER_CC_CHANNELS = 8,
    parameter int NUM_CHIP = 2,
    parameter int ADDR_WIDTH = 18,
    parameter int DATA_WIDTH = 16
)(
    input CLK,
    input nRST,
    input uart_rx,
    output uart_tx,
    input [BOOT_PINS-1 : 0] boot_in,
    output logic [BOOT_PINS-1 : 0] boot_out,
    output logic [BOOT_PINS-1 : 0] boot_oe,
    input logic [IO_MUX_NUM_PINS-1:0] io_mux_to_module_iopad,
    output logic [IO_MUX_NUM_PINS-1:0] io_mux_from_module_ff,
    output logic [IO_MUX_NUM_PINS-1:0] io_mux_output_en_ff,
    // external SRAM ICs connections
    output logic n_OE [NUM_CHIP-1:0],
    output logic n_CE [NUM_CHIP-1:0],
    output logic n_WE [NUM_CHIP-1:0],
    output logic n_LB [NUM_CHIP-1:0],
    output logic n_UB [NUM_CHIP-1:0],
    output logic [ADDR_WIDTH-1:0] addr,
    output logic iopad_n_oe, //share one n_oe signal for all io pads for data pins
    output logic [DATA_WIDTH-1:0] wdata,
    input logic [DATA_WIDTH-1:0] rdata

    // VGA Signals
    input logic CLOCK_50,
    // input logic TD_RESET_N,
    output logic VGA_CLK,
    output logic [7:0] VGA_R,
    output logic [7:0] VGA_G,
    output logic [7:0] VGA_B,
    output logic VGA_SYNC_N,
    output logic VGA_BLANK_N,
    output logic VGA_HS,
    output logic VGA_VS
);

    // Enumerate interrupts -- TODO: Better way?
    localparam int GPIO0_IRQ0 = 0;
    localparam int GPIO0_IRQ1 = 1;
    localparam int GPIO0_IRQ2 = 2;
    localparam int GPIO0_IRQ3 = 3;
    localparam int GPIO0_IRQ4 = 4;
    localparam int GPIO0_IRQ5 = 5;
    localparam int GPIO0_IRQ6 = 6;
    localparam int GPIO0_IRQ7 = 7;

    localparam int BITS_WIDTH = 32;
    localparam int NUM_INTERRUPTS = 64;
    localparam int NUM_GPIO_PORTS = 4;


    genvar i, j, k;
    logic halt, wfi;
    logic uart_core_reset;

    // DMA control signals
    logic drq, daq, dma_interrupt;

    // PWM output signal
    logic [PWM_CHANNELS-1:0] pwm_out;

    // Timer signals
    logic [TIMER_CC_CHANNELS-1:0] timer_in;
    logic [TIMER_CC_CHANNELS-1:0] timer_out;
    logic [TIMER_CC_CHANNELS-1:0] timer_oe;

    /****************
    * Bus Interfaces
    *****************/
    // GPIO Pins
    bus_protocol_if gpio_protifs[NUM_GPIO_PORTS]();
    bus_protocol_if gpioboot_protif();

    bus_protocol_if memory_protif();
    bus_protocol_if clint_protif();
    bus_protocol_if uart_protif();
    bus_protocol_if plic_protif();
    bus_protocol_if printer_protif();
    bus_protocol_if pwm0_protif();
    bus_protocol_if spi_protif();
    bus_protocol_if mt_protif();
    bus_protocol_if ffram_protif();
    bus_protocol_if timer0_protif();
    bus_protocol_if rom_protif();
    ahb_if dma_protif(); // DMA is an outlier not using bus_protocol_if
    bus_protocol_if crc_protif();  // CRC AHB peripheral bus_protocol_if 
    bus_protocol_if vga_protif(); // VGA


    /***********************
    * Peripheral Interfaces
    ************************/
    gpio_if #(.NUM_PINS(GPIO_PINS_PER_PORT)) gpioifs[NUM_GPIO_PORTS]();
    gpio_if #(.NUM_PINS(BOOT_PINS)) gpiobootif();

    spi_if spiif();
    timer_if timerif();

    core_interrupt_if irqif();
    plic_if #(.NUM_INTERRUPTS(NUM_INTERRUPTS)) plicif(); //Originally 8 interrupts
    clint_if clif();


    /*****************************
    * Bus Interconnect Interfaces
    ******************************/
    ahb_if managers[2](CLK, nRST);
    ahb_if ahb_peripherals[AHB_NSUBORDINATES](
        .HCLK(CLK),
        .HRESETn(nRST)
    );
    ahb_if muxed_ahb_if(CLK, nRST);

    apb_if apb_requester_if(CLK, nRST);
    apb_if apb_peripherals[APB_NCOMPLETERS](CLK, nRST);


    /***********************
    * SoC IP Instantiations
    ************************/
    // AHB Managers
    RISCVBusiness #(
        .RESET_PC(RESET_PC)
    ) RISCV_CORE (
        .CLK,
        .nRST,
        .halt,
        .wfi,
        .ahb_manager(managers[0]),
        .interrupt_if(irqif),
        .mtime(clif.mtime)
    );

    dma_controller DMA(
        .clk(CLK),
        .n_rst(nRST),
        .drq(drq),
        .daq(daq),
        .subordinate(ahb_peripherals[DMA_AHB_IDX]),
        .manager(managers[1]),
        .dma_interrupt(dma_interrupt)
    );

    // Bus mux + interconnect
    ahb_mux #(
        .NMANAGERS(2)
    ) AHB_MUX(
        .HCLK(CLK),
        .HRESETn(nRST),
        .m_in(managers),
        .m_out(muxed_ahb_if)
    );

    ahb_simple_interconnect #(
        .NSUBORDINATES(AHB_NSUBORDINATES),
        .AHB_MAP(AHB_MAP)
    ) AHB_CXN(
        .manager(muxed_ahb_if),
        .subordinates(ahb_peripherals)
    );

    // AHB Peripherals
    ahb2apb #(
        .NWORDS(APB_NCOMPLETERS * 4096), // Allocate 4k per peripheral, may not all be used
        .BASE_ADDR(AHB_MAP[APB_BRIDGE_IDX])
    ) BRIDGE(
        .ahb_if(ahb_peripherals[APB_BRIDGE_IDX]),
        .apbif(apb_requester_if)
    );

    apb_interconnect #(
        .NCOMPLETERS(APB_NCOMPLETERS),
        .COMPLETER_ADDR_BITS(16),
        .BUS_REGION_BEGIN(32'h0),
        .APB_MAP(APB_MAP)
    ) APB_CXN(
        .requester(apb_requester_if),
        .completers(apb_peripherals)
    );

    rom BOOTROM (
        .busif(rom_protif)
    );

    // CRC peripheral
    crc_subordinate CRC0 (
        .CLK  (CLK),
        .nRST (nRST),
        .busif(crc_protif)  
        );

    sram_controller #(
        .NUM_CHIP(NUM_CHIP),
        .ADDR_WIDTH(ADDR_WIDTH), // for the memory chip
        .DATA_WIDTH(DATA_WIDTH)
    ) SRAM_controller (
        .CLK(CLK),
        .n_RST(nRST),
        .prif(memory_protif),
        .hintif(memory_protif),
        .n_OE(n_OE),
        .n_CE(n_CE),
        .n_WE(n_WE),
        .n_LB(n_LB),
        .n_UB(n_UB),
        .iopad_n_oe(iopad_n_oe),
        .addr(addr),
        .wdata(wdata),
        .rdata_in(rdata)
    );


    ff_ram #(
        .NBYTES(FF_RAM_SIZE),
        .LATENCY(4)  // only used for FPGA on-board memory
    ) FFRAM (
        .CLK,
        .busif(ffram_protif)
    );

    // VGA Subordinate
    top_vga VGAAHB(
        .clk(CLOCK_50),
        .n_rst(nRST),
        .vga_clk(VGA_CLK),
        .vga_r(VGA_R),
        .vga_g(VGA_G),
        .vga_b(VGA_B),
        .vga_sync_n(VGA_SYNC_N),
        .vga_blank_n(VGA_BLANK_N),
        .vga_hs(VGA_HS),
        .vga_vs(VGA_VS),
        .busif(vga_protif)
    );

    /*****************
    * APB Peripherals
    ******************/
    // GPIO
    gpio #(
        .NUM_PINS(GPIO_PINS_PER_PORT)
    ) GPIO0 (
        .CLK,
        .nRST,
        .busif(gpio_protifs[0]),
        .gpioif(gpioifs[0])
    );
    
    gpio #(
        .NUM_PINS(GPIO_PINS_PER_PORT)
    ) GPIO1 (
        .CLK,
        .nRST,
        .busif(gpio_protifs[1]),
        .gpioif(gpioifs[1])
    );

    gpio #(
        .NUM_PINS(GPIO_PINS_PER_PORT)
    ) GPIO2 (
        .CLK,
        .nRST,
        .busif(gpio_protifs[2]),
        .gpioif(gpioifs[2])
    );
    
    gpio #(
        .NUM_PINS(GPIO_PINS_PER_PORT)
    ) GPIO3 (
        .CLK,
        .nRST,
        .busif(gpio_protifs[3]),
        .gpioif(gpioifs[3])
    );

    // Extra GPIO for boot pins
    gpio #(
        .NUM_PINS(BOOT_PINS)
    ) GPIOBOOT (
        .CLK,
        .nRST,
        .busif(gpioboot_protif),
        .gpioif(gpiobootif)
    );

    // Timers
    advanced_timer #(
      .CHANNELS(TIMER_CC_CHANNELS),
      .BITS_WIDTH(BITS_WIDTH)
    ) TIMER0 (
      .clk(CLK),
      .n_rst(nRST),
      .busif(timer0_protif),
      .timerif(timerif)
    );

    pwm_wrapper #(
        .NUM_CHANNELS(PWM_CHANNELS)
    ) PWM0 (
        .CLK,
        .nRST,
        .busif(pwm0_protif),
        .pwm_out
    );

    spi_wrapper SPI0 (
        .CLK,
        .nRST,
        .busif(spi_protif),
        .spiif
    );

    clint CLINT(
        .CLK,
        .nRST,
        .busif(clint_protif),
        .clif
    );

    AHBUart UART (
        .clk(CLK),
        .nReset(nRST),
        .rx(uart_rx),
        .tx(uart_tx),
        .bp(uart_protif)
    );

    plic #(.NUM_INTERRUPTS(NUM_INTERRUPTS)) PLIC(
        .CLK,
        .nRST,
        .busif(plic_protif),
        .plicif
    );

    mt_wrapper PRNG (
        .clk(CLK),
        .n_rst(nRST),
        .busif(mt_protif)
    );

    logic [IO_MUX_NUM_PINS * IO_MUX_NUM_FUNC - 1:0] from_module;
    logic [IO_MUX_NUM_PINS * IO_MUX_NUM_FUNC - 1:0] output_enable;
    logic [IO_MUX_NUM_PINS * IO_MUX_NUM_FUNC - 1:0] to_module;

    // TEMP SIGNALS
    logic [19:0] dummy;


    // IO MUX FUNCTIONS
    logic [(GPIO_PINS_PER_PORT*NUM_GPIO_PORTS)-1 : 0] gpio_in;
    logic [(GPIO_PINS_PER_PORT*NUM_GPIO_PORTS)-1 : 0] gpio_out;
    logic [(GPIO_PINS_PER_PORT*NUM_GPIO_PORTS)-1 : 0] gpio_oe;
    logic SPI_MISO_IN, SPI_MOSI_IN, SPI_SCK_IN, SPI_SS_IN;
    logic SPI_MISO_OUT, SPI_MOSI_OUT, SPI_SCK_OUT, SPI_SS_OUT, SPI_MODE_OUT;

    // pin, to_module, from_module, output_enable
    `DEFINE_PIN(0, {dummy[16], gpio_in[0]}, {1'b0, gpio_out[0]}, {1'b0, gpio_oe[0]})
    `DEFINE_PIN(1, {dummy[17], gpio_in[1]}, {1'b0, gpio_out[1]}, {1'b0, gpio_oe[1]})

    //pin 2 - pin 3 for i2c and gpio2-3
    `DEFINE_PIN(2, {dummy[18], gpio_in[2]}, {1'b0, gpio_out[2]}, {1'b0, gpio_oe[2]})
    `DEFINE_PIN(3, {dummy[19], gpio_in[3]}, {1'b0, gpio_out[3]}, {1'b0, gpio_oe[3]})

    // pin 4 - pin 7 for spi0 and gpio4-7
    `DEFINE_PIN(4, {SPI_SS_IN, gpio_in[4]}, {SPI_SS_OUT, gpio_out[4]}, {SPI_MODE_OUT, gpio_oe[4]})
    `DEFINE_PIN(5, {SPI_SCK_IN, gpio_in[5]}, {SPI_SCK_OUT, gpio_out[5]}, {SPI_MODE_OUT, gpio_oe[5]})
    `DEFINE_PIN(6, {SPI_MOSI_IN, gpio_in[6]}, {SPI_MOSI_OUT, gpio_out[6]}, {SPI_MODE_OUT, gpio_oe[6]})
    `DEFINE_PIN(7, {SPI_MISO_IN, gpio_in[7]}, {SPI_MISO_OUT, gpio_out[7]}, {~SPI_MODE_OUT, gpio_oe[7]})

    //pin 8 - pin 9 for pwm0 and gpio8-9
    `DEFINE_PIN(8, {dummy[0], gpio_in[8]}, {pwm_out[0], gpio_out[8]}, {1'b1, gpio_oe[8]})
    `DEFINE_PIN(9, {dummy[1], gpio_in[9]}, {pwm_out[1], gpio_out[9]}, {1'b1, gpio_oe[9]})

    //pin 10 - pin 15 gpio10-15
    `DEFINE_PIN(10, {dummy[2], gpio_in[10]}, {1'b0, gpio_out[10]}, {1'b0, gpio_oe[10]})
    `DEFINE_PIN(11, {dummy[3], gpio_in[11]}, {1'b0, gpio_out[11]}, {1'b0, gpio_oe[11]})
    `DEFINE_PIN(12, {dummy[4], gpio_in[12]}, {1'b0, gpio_out[12]}, {1'b0, gpio_oe[12]})
    `DEFINE_PIN(13, {dummy[5], gpio_in[13]}, {1'b0, gpio_out[13]}, {1'b0, gpio_oe[13]})
    `DEFINE_PIN(14, {dummy[6], gpio_in[14]}, {1'b0, gpio_out[14]}, {1'b0, gpio_oe[14]})
    `DEFINE_PIN(15, {dummy[7], gpio_in[15]}, {1'b0, gpio_out[15]}, {1'b0, gpio_oe[15]})

    //pin 16 - pin 23 for tim0 and gpio16-23
    `DEFINE_PIN(16, {timer_in[0], gpio_in[16]}, {timer_out[0], gpio_out[16]}, {timer_oe[0], gpio_oe[16]})
    `DEFINE_PIN(17, {timer_in[1], gpio_in[17]}, {timer_out[1], gpio_out[17]}, {timer_oe[1], gpio_oe[17]})
    `DEFINE_PIN(18, {timer_in[2], gpio_in[18]}, {timer_out[2], gpio_out[18]}, {timer_oe[2], gpio_oe[18]})
    `DEFINE_PIN(19, {timer_in[3], gpio_in[19]}, {timer_out[3], gpio_out[19]}, {timer_oe[3], gpio_oe[19]})
    `DEFINE_PIN(20, {timer_in[4], gpio_in[20]}, {timer_out[4], gpio_out[20]}, {timer_oe[4], gpio_oe[20]})
    `DEFINE_PIN(21, {timer_in[5], gpio_in[21]}, {timer_out[5], gpio_out[21]}, {timer_oe[5], gpio_oe[21]})
    `DEFINE_PIN(22, {timer_in[6], gpio_in[22]}, {timer_out[6], gpio_out[22]}, {timer_oe[6], gpio_oe[22]})
    `DEFINE_PIN(23, {timer_in[7], gpio_in[23]}, {timer_out[7], gpio_out[23]}, {timer_oe[7], gpio_oe[23]})

    //pin 24 - pin 31 for gpio24-31
    `DEFINE_PIN(24, {dummy[8], gpio_in[24]}, {1'b0, gpio_out[24]}, {1'b0, gpio_oe[24]})
    `DEFINE_PIN(25, {dummy[9], gpio_in[25]}, {1'b0, gpio_out[25]}, {1'b0, gpio_oe[25]})
    `DEFINE_PIN(26, {dummy[10], gpio_in[26]}, {1'b0, gpio_out[26]}, {1'b0, gpio_oe[26]})
    `DEFINE_PIN(27, {dummy[11], gpio_in[27]}, {1'b0, gpio_out[27]}, {1'b0, gpio_oe[27]})
    `DEFINE_PIN(28, {dummy[12], gpio_in[28]}, {1'b0, gpio_out[28]}, {1'b0, gpio_oe[28]})
    `DEFINE_PIN(29, {dummy[13], gpio_in[29]}, {1'b0, gpio_out[29]}, {1'b0, gpio_oe[29]})
    `DEFINE_PIN(30, {dummy[14], gpio_in[30]}, {1'b0, gpio_out[30]}, {1'b0, gpio_oe[30]})
    `DEFINE_PIN(31, {dummy[15], gpio_in[31]}, {1'b0, gpio_out[31]}, {1'b0, gpio_oe[31]})

    top_level_digital_mux_wrapper #(
      .NUM_PINS(IO_MUX_NUM_PINS),
      .NUM_FUNC(IO_MUX_NUM_FUNC),
      .NUM_BITS(IO_MUX_NUM_BITS),
      .NUM_REGS(IO_MUX_NUM_REGS)
    ) digital_io_mux (
      .from_module_ff(io_mux_from_module_ff),
      .output_en_ff(io_mux_output_en_ff),
      .to_module_iopad(io_mux_to_module_iopad),
      .from_module(from_module),
      .output_enable(output_enable),
      .to_module(to_module),
      .apbif(apb_peripherals[DIGITAL_IO_MUX_IDX])
    );



`ifndef SYNTHESIS
    mmio_printer PRINT(
        .CLK,
        .busif(printer_protif)
    );
    
    `ADD_AHB(mmio_printer, PRINTER_AHB_IDX, 1, printer_protif)
`endif
    

    `ADD_AHB(rom, ROM_AHB_IDX, ROM_N_WORDS, rom_protif)
    `ADD_AHB(ff_ram, MEMORY_AHB_IDX, FF_RAM_N_WORDS, ffram_protif)
    `ADD_AHB(memory_controller, SRAM_AHB_IDX, SRAM_N_WORDS, memory_protif)
    
    `ADD_AHB(vga, VGA_AHB_IDX, 16, vga_protif) // vga

    `ADD_APB(gpio0, GPIO0_APB_IDX, 8, gpio_protifs[0])
    `ADD_APB(gpio1, GPIO1_APB_IDX, 8, gpio_protifs[1])
    `ADD_APB(gpio2, GPIO2_APB_IDX, 8, gpio_protifs[2])
    `ADD_APB(gpio3, GPIO3_APB_IDX, 8, gpio_protifs[3])
    `ADD_APB(gpioboot, GPIOBOOT_APB_IDX, 8, gpioboot_protif)

    `ADD_APB(advanced_timer, TIMER_APB_IDX, (2 * TIMER_CC_CHANNELS) + 4, timer0_protif)
    `ADD_APB(pwm, PWM_APB_IDX, 6, pwm0_protif)
    `ADD_APB(spi, SPI_APB_IDX, 8, spi_protif) // The third argument?

    `ADD_AHB(clint, CLINT_AHB_IDX, 'h8000 >> 2, clint_protif)
    `ADD_AHB(uart, UART_AHB_IDX, 4, uart_protif)
    `ADD_AHB(plic, PLIC_AHB_IDX, 'h4000000 >> 2, plic_protif) // statically allocate 0x400000 address for PLIC to accommodate its max size
    `ADD_AHB(mt, MT_AHB_IDX, 2, mt_protif)
    `ADD_AHB(crc, CRC_AHB_IDX, 1024, crc_protif) // 0x9000_3000 (0x1000 / 4 = 1024 for word counts)

    // Assign interrupt mapping
    // GPIO
    generate
        for(j = 0; j < NUM_GPIO_PORTS; j++) begin : g_gpio_port_irq
            for(i = 0; i < GPIO_PINS_PER_PORT; i++) begin : g_gpio_irq
                `DEFINE_INTERRUPT((j*GPIO_PINS_PER_PORT + i), gpioifs[j].irq[i])
            end
        end
    endgenerate

    //SPI
    generate
        for (i = GPIO_PINS_PER_PORT*NUM_GPIO_PORTS; i < GPIO_PINS_PER_PORT*NUM_GPIO_PORTS + 6; i++) begin : g_spi_irq
            `DEFINE_INTERRUPT(i, spiif.interrupts[i-GPIO_PINS_PER_PORT*NUM_GPIO_PORTS])
        end
    endgenerate

    // DMA
    `DEFINE_INTERRUPT(GPIO_PINS_PER_PORT*NUM_GPIO_PORTS + 6, dma_interrupt)

    // TIMER
    generate
        for(i = GPIO_PINS_PER_PORT*NUM_GPIO_PORTS + 7; i < TIMER_CC_CHANNELS + GPIO_PINS_PER_PORT*NUM_GPIO_PORTS + 7; i++) begin : g_timer_irq
            `DEFINE_INTERRUPT(i, timerif.t_irq[i - (GPIO_PINS_PER_PORT*NUM_GPIO_PORTS + 7)])
        end
    endgenerate
    `DEFINE_INTERRUPT(GPIO_PINS_PER_PORT*NUM_GPIO_PORTS + TIMER_CC_CHANNELS + 7, timerif.tc_irq)

    // Input/Output
    generate
        for(i = 0; i < NUM_GPIO_PORTS; i++) begin : g_gpio_packing
            assign gpioifs[i].in_data = gpio_in[i*8 +: 8];
            assign gpio_out[i*GPIO_PINS_PER_PORT +: GPIO_PINS_PER_PORT] = gpioifs[i].out_data;
            assign gpio_oe[i*GPIO_PINS_PER_PORT +: GPIO_PINS_PER_PORT] = gpioifs[i].oe_data;
        end
    endgenerate

    assign gpiobootif.in_data = boot_in;
    assign boot_out = gpiobootif.out_data;
    assign boot_oe = gpiobootif.oe_data;

    assign timerif.t_in = timer_in;
    assign timer_out = timerif.t_out;
    assign timer_oe = timerif.t_oe;

    assign irqif.soft_int = clif.soft_int;
    assign irqif.soft_int_clear = clif.clear_soft_int;
    assign irqif.timer_int = clif.timer_int;
    assign irqif.timer_int_clear = clif.clear_timer_int;
    assign irqif.ext_int = plicif.interrupt_service_request;
    assign irqif.ext_int_clear = ~plicif.interrupt_service_request;

    // SPI output signals
    assign SPI_MODE_OUT = spiif.mode;
    assign SPI_MISO_OUT = spiif.MISO_OUT;
    assign SPI_MOSI_OUT = spiif.MOSI_OUT;
    assign SPI_SCK_OUT = spiif.SCK_OUT;
    assign SPI_SS_OUT = spiif.SS_OUT;

    // SPI input signals
    assign spiif.MISO_IN = SPI_MISO_IN;
    assign spiif.MOSI_IN = SPI_MOSI_IN;
    assign spiif.SCK_IN = SPI_SCK_IN;
    assign spiif.SS_IN = SPI_SS_IN;


`ifndef SYNTHESIS
    always_ff @(posedge CLK) begin
        if(halt) begin
            $finish();
        end
    end


/*    bind stage3_mem_stage cpu_tracker cpu_track1 (
        .CLK(CLK),
        .wb_stall(wb_stall),
        .instr(ex_mem_if.ex_mem_reg.instr),
        .pc(ex_mem_if.ex_mem_reg.pc),
        .opcode(rv32i_types_pkg::opcode_t'(ex_mem_if.ex_mem_reg.instr[6:0])),
        .funct3(funct3),
        .funct12(funct12),
        .rs1(ex_mem_if.ex_mem_reg.instr[19:15]),
        .rs2(ex_mem_if.ex_mem_reg.instr[24:20]),
        .rd(ex_mem_if.ex_mem_reg.rd_m),
        .imm_S(ex_mem_if.ex_mem_reg.tracker_signals.imm_S), // TODO: Extract constants. Maybe we could pass these in the pipeline and they'd be removed by synthesis?
        .imm_I(ex_mem_if.ex_mem_reg.tracker_signals.imm_I),
        .imm_U(ex_mem_if.ex_mem_reg.tracker_signals.imm_U),
        .imm_UJ(ex_mem_if.ex_mem_reg.tracker_signals.imm_UJ),
        .imm_SB(ex_mem_if.ex_mem_reg.tracker_signals.imm_SB),
        .instr_30(instr_30)
    );*/
`endif

endmodule