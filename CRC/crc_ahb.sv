`timescale 1ns/1ps

module crc_subordinate #(
    parameter int ADDR_WIDTH = 32,
    parameter int DATA_WIDTH = 32,
    parameter logic [ADDR_WIDTH-1:0] BASE_ADDR = 32'h9000_3000
)(
    input  logic CLK,
    input  logic nRST,
    bus_protocol_if.peripheral_vital busif
);
    // Register map (word index = addr[5:2])
    //   0x00 → CTRL
    //   0x04 → DATA (write-only)
    //   0x08 → RES  (final CRC value)
    typedef enum logic [3:0] {
        A_CTRL = 4'h0,  // 0x00
        A_DATA = 4'h1,  // 0x04
        A_RES  = 4'h2   // 0x08
    } addr_e;

    addr_e addr_dec;
    always_comb addr_dec = addr_e'(busif.addr[5:2]);

    // Access flags
    wire do_write = busif.wen;
    wire do_read  = busif.ren;

    // Internal registers
    logic [31:0] r_ctrl; // Only keeps EN bit (bit0)
    logic [31:0] r_result; // Final CRC output
    logic        r_busy; // Internal busy flag (CRC session active)

    logic [31:0] crc_state; // Running CRC state (before XOR/reflection)
    logic [31:0] next_crc_state;

    logic [31:0] next_r_ctrl;
    logic [31:0] next_r_result;
    logic        next_busy;
    logic [31:0] next_rdata;

    // DATA write → triggers CRC core
    logic        data_fire;
    logic [31:0] data_word;
    logic        core_fire_d;    // Delayed version of data_fire

    // IEEE CRC-32 parameters
    localparam logic [31:0] CRC_POLY   = 32'h04C11DB7;
    localparam logic [31:0] CRC_INIT   = 32'hFFFF_FFFF;
    localparam logic [31:0] CRC_XOROUT = 32'hFFFF_FFFF;
    localparam logic        CRC_REFIN  = 1'b1;
    localparam logic        CRC_REFOUT = 1'b1;

    // Bit-reverse function for final output
    function automatic logic [31:0] reverse32(input logic [31:0] x);
        logic [31:0] r;
        integer i;
        begin
            for (i = 0; i < 32; i++) r[i] = x[31-i];
            return r;
        end
    endfunction

    // CRC core instance (streaming mode)
    logic [31:0] crc_out_wire;

    crc #(
        .DATA_BYTES(4),
        .CRC_WIDTH (32)
    ) u_crc (
        .clk          (CLK),
        .n_rst        (nRST),
        .data_in      (data_word), // 32-bit data word
        .crc_in       (crc_state),
        .polynomial   (CRC_POLY),
        .enable       (data_fire),
        .reflect_in   (CRC_REFIN),
        .reflect_out  (1'b0), // reflection handled in wrapper
        .final_xor_val(32'h0),
        .crc_out      (crc_out_wire)
    );

    // Combinational next-state logic
    always_comb begin
        // Default: hold current values
        next_r_ctrl    = r_ctrl;
        next_r_result  = r_result;
        next_busy      = r_busy;
        next_rdata     = '0;

        data_fire      = 1'b0;
        data_word      = '0;
        next_crc_state = crc_state;

        // Apply CRC output one cycle after data_fire
        if (core_fire_d)
            next_crc_state = crc_out_wire;

        // WRITE path
        if (do_write) begin
            unique case (addr_dec)

                // CTRL register
                A_CTRL: begin
                    // Keep EN bit persistent
                    next_r_ctrl[0] = busif.wdata[0];

                    // INIT pulse (bit1)
                    if (busif.wdata[1] && busif.wdata[0]) begin
                        next_crc_state = CRC_INIT;
                        next_busy      = 1'b1;
                        next_r_result  = 32'h0;
                    end

                    // FINALIZE pulse (bit2)
                    if (busif.wdata[2] && r_busy) begin
                        logic [31:0] outv;
                        outv = crc_state ^ CRC_XOROUT;
                        if (CRC_REFOUT)
                            outv = reverse32(outv);

                        next_r_result = outv;
                        next_busy     = 1'b0;
                    end
                end

                // DATA write (32-bit word)
                A_DATA: begin
                    // Only accept data if EN=1 and session active
                    if (r_ctrl[0] && r_busy) begin
                        data_fire = 1'b1;
                        data_word = busif.wdata;
                    end
                end

                default: ;
            endcase
        end

        // READ path
        if (do_read) begin
            unique case (addr_dec)
                A_CTRL: next_rdata = r_ctrl;
                A_DATA: next_rdata = 32'h0;      // write-only
                A_RES : next_rdata = r_result;   // final CRC output
                default: next_rdata = '0;
            endcase
        end
    end

    // Sequential logic
    always_ff @(posedge CLK or negedge nRST) begin
        if (!nRST) begin
            r_ctrl      <= 32'h0;
            r_result    <= 32'h0;
            r_busy      <= 1'b0;

            crc_state   <= CRC_INIT;
            core_fire_d <= 1'b0;

            busif.rdata <= '0;
            busif.error <= 1'b0;
        end else begin
            r_ctrl      <= next_r_ctrl;
            r_result    <= next_r_result;
            r_busy      <= next_busy;

            crc_state   <= next_crc_state;
            core_fire_d <= data_fire;

            busif.rdata <= next_rdata;
            busif.error <= 1'b0;
        end
    end

    // Always stall for 1 cycle per transaction
    always_comb begin
        busif.request_stall = (busif.wen || busif.ren);
    end

endmodule
