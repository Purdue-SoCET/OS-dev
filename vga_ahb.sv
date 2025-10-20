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
localparam VGA_WIDTH = 320;
localparam VGA_HEIGHT = 240;
localparam MEM_DEPTH = VGA_WIDTH * VGA_HEIGHT * 2;
localparam FB_BASE_ADDR = 32'hD0000000;

// Declareation
logic clk_25;
logic fb_wen;
logic [DATA_WIDTH-1:0] fb_wdata;
logic [ADDR_WIDTH-1:0] fb_waddr;
logic [9:0] vga_x, vga_y;
logic [9:0] fb_x, fb_y;
logic [23:0] pixel_data;
logic [ADDR_WIDTH-1:0] waddr;
logic [DATA_WIDTH-1:0] wdata;
logic [23:0] rdata; 
logic [DATA_WIDTH-1:0] mem [MEM_DEPTH-1:0];
logic [ADDR_WIDTH-1:0] raddr;

// 50 MHz -> 25 MHz
assign vga_clk = clk_25;
always_ff @(posedge clk_50, negedge n_rst) begin
    if(!n_rst) clk_25 <= 1'b0;
    else clk_25 <= ~clk_25;
end

// AHB-BUS Communication
always_comb begin
    busif.request_stall = busif.wen || busif.ren;
    busif.error = 1'b0;
    busif.rdata = '0;
end
always_ff @(posedge ahb_clk, negedge n_rst) begin
    if(!n_rst) begin
        fb_wen <= 1'b0;
        fb_wdata <= '0;
        fb_waddr <= '0;
    end else begin
        fb_wen <= busif.wen;
        fb_wdata <= busif.wdata;
        fb_waddr <= busif.addr - FB_BASE_ADDR;
    end
end

// VGA controller
vga_controller VGA_CNT (
    .clk(ahb_clk),
    .clk_25(vga_clk),
    .n_rst(n_rst),
    .hsync(vga_hs),
    .vsync(vga_vs),
    .video_on(vga_blank_n),
    .synch(vga_sync_n),
    .x_coordinate(vga_x),
    .y_coordinate(vga_y)
);

// Converting Resolution
always_comb begin
    fb_x = vga_x >> 1;
    fb_y = vga_y >> 1;
end

// Frame Buffer Logic
always_comb begin
    waddr = fb_waddr;
    wdata = fb_wdata;
    raddr = fb_y * VGA_WIDTH + fb_x;
    rdata = (vga_blank_n) ?  mem[raddr][23:0] : 24'b0;
end
always_ff @(posedge ahb_clk) begin
    if(fb_wen) mem[waddr] <= fb_wdata[23:0];
end

always_comb begin
    vga_r = rdata[23:16];
    vga_g = rdata[15:8];
    vga_b = rdata[7:0];
end

endmodule
