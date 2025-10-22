module vga_ahb #(
    parameter int ADDR_WIDTH = 32,
    parameter int DATA_WIDTH = 32
)(
    input logic ahb_clk,
    input logic clk_50, // 50 MHz
    input logic n_rst,
    output logic vga_clk, // 25 MHz
    output logic [7:0] vga_r,
    output logic [7:0] vga_g,
    output logic [7:0] vga_b,
    output logic vga_sync_n,
    output logic vga_blank_n,
    output logic vga_hs,
    output logic vga_vs,
    bus_protocol_if.peripheral_vital busif
);

localparam VGA_WIDTH = 640;
localparam VGA_HEIGHT = 480;
localparam IMG_WIDTH = 64;
localparam IMG_HEIGHT = 64;
localparam X_START = (VGA_WIDTH - IMG_WIDTH) / 2;
localparam X_END = X_START + IMG_WIDTH;
localparam Y_START = (VGA_HEIGHT - IMG_HEIGHT) / 2;
localparam Y_END = Y_START + IMG_HEIGHT;
localparam MEM_DEPTH = (IMG_WIDTH * IMG_HEIGHT) / 4;
localparam FB_BASE_ADDR = 32'hD0000000;
localparam FB_END_ADDR = FB_BASE_ADDR + MEM_DEPTH * 4;

// Declareation
logic clk_25;
logic [DATA_WIDTH-1:0] mem [MEM_DEPTH-1:0];

// AHB Signals
logic ahb_wen;
logic [DATA_WIDTH-1:0] ahb_wdata;
logic [ADDR_WIDTH-1:0] ahb_waddr; // Address in FB
logic [ADDR_WIDTH-1:0] ahb_addr; // Raw Address from AFTx07 core

logic addr_match;
logic vga_hs_raw, vga_vs_raw, vga_blank_n_raw, vga_sync_n_raw;
logic [9:0] vga_x, vga_y;

// 50 MHz -> 25 MHz
assign vga_clk = clk_25;
always_ff @(posedge clk_50, negedge n_rst) begin
    if(!n_rst) clk_25 <= 1'b0;
    else clk_25 <= ~clk_25;
end

always_ff @(posedge ahb_clk, negedge n_rst) begin
    if(!n_rst) begin
        ahb_wen <= 1'b0;
        ahb_wdata <= '0;
        ahb_addr <= '0;
    end else begin
        ahb_wen <= busif.wen;
        ahb_wdata <= busif.wdata;
        ahb_addr <= busif.addr;
    end
end

assign addr_match = (ahb_addr >= FB_BASE_ADDR) && (ahb_addr < FB_END_ADDR);
always_ff @(posedge ahb_clk) begin
    if(ahb_wen && addr_match) begin
        mem[(ahb_addr - FB_BASE_ADDR) >> 2] <= ahb_wdata;
    end
end

// AHB-BUS Communication
always_comb begin
    busif.request_stall = (busif.wen || busif.ren) &&
                            (busif.addr >= FB_BASE_ADDR) && (busif.addr < FB_END_ADDR);
    busif.error = 1'b0;
    busif.rdata = '0;
end

// VGA controller
vga_controller VGA_CNT (
    .clk_25(vga_clk),
    .n_rst(n_rst),
    .hsync(vga_hs_raw),
    .vsync(vga_vs_raw),
    .video_on(vga_blank_n_raw),
    .synch(vga_sync_n_raw),
    .x_coordinate(vga_x),
    .y_coordinate(vga_y)
);

logic [9:0] fb_x, fb_y;
logic [11:0] pixel_index;
logic [ADDR_WIDTH-1:0] vga_raddr;
logic [DATA_WIDTH-1:0] mem_rdata; 

always_comb begin
    fb_x = 10'b0;
    fb_y = 10'b0;
    pixel_index = 12'b0;
    vga_raddr = '0; // Black Pixel (default)

    if(vga_x >= X_START && vga_x < X_END && vga_y >= Y_START && vga_y < Y_END) begin
        fb_x = vga_x - X_START; // x-location in 64 x 64 region
        fb_y = vga_y - Y_START; // y-location in 64 x 64 region
        pixel_index = (fb_y << 6) + fb_x; // order of the pixel in memory
        vga_raddr = pixel_index >> 2; // order of pixel in framebuffer
    end
end

logic [11:0] pixel_index_q1;
logic vga_hs_q1, vga_vs_q1, vga_blank_n_q1, vga_sync_n_q1;
always_ff @(posedge vga_clk, negedge n_rst) begin
    if(!n_rst) begin
        pixel_index_q1 <= '0;
        vga_hs_q1 <= 1'b0;
        vga_vs_q1 <= 1'b0;
        vga_blank_n_q1 <= 1'b0;
        vga_sync_n_q1 <= 1'b0;
    end else begin
        pixel_index_q1 <= pixel_index;
        vga_hs_q1 <= vga_hs_raw;
        vga_vs_q1 <= vga_vs_raw;
        vga_blank_n_q1 <= vga_blank_n_raw;
        vga_sync_n_q1 <= vga_sync_n_raw;
    end
end

always_ff @(posedge vga_clk) begin
    mem_rdata <= mem[vga_raddr];
end

logic [23:0] rdata_q2;
logic vga_hs_q2, vga_vs_q2, vga_blank_n_q2, vga_sync_n_q2;
logic [7:0] gray_pixel_value;

always_ff @(posedge vga_clk, negedge n_rst) begin
    if (!n_rst) begin
        rdata_q2 <= '0;
        vga_hs_q2 <= 1'b0;
        vga_vs_q2 <= 1'b0;
        vga_blank_n_q2 <= 1'b0;
        vga_sync_n_q2 <= 1'b0;
    end else begin

        case(pixel_index_q1[1:0])
            2'd0: gray_pixel_value = mem_rdata[7:0];
            2'd1: gray_pixel_value = mem_rdata[15:8];
            2'd2: gray_pixel_value = mem_rdata[23:16];
            2'd3: gray_pixel_value = mem_rdata[31:24];
            default: gray_pixel_value = 8'h00;
        endcase
        
        rdata_q2 <= {gray_pixel_value, gray_pixel_value, gray_pixel_value};

        vga_hs_q2 <= vga_hs_q1;
        vga_vs_q2 <= vga_vs_q1;
        vga_blank_n_q2 <= vga_blank_n_q1;
        vga_sync_n_q2 <= vga_sync_n_q1;
    end
end

always_comb begin
    vga_r = 8'b0;
    vga_g = 8'b0;
    vga_b = 8'b0;

    if(vga_blank_n_q2) begin
        vga_r = rdata_q2[23:16];
        vga_g = rdata_q2[15:8];
        vga_b = rdata_q2[7:0];
    end
end

assign vga_hs = vga_hs_q2;
assign vga_vs = vga_vs_q2;
assign vga_blank_n = vga_blank_n_q2;
assign vga_sync_n = vga_sync_n_q2;

endmodule
