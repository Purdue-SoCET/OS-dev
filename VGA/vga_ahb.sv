module vga_ahb #(
    parameter int ADDR_WIDTH = 32,
    parameter int DATA_WIDTH = 24 // 24=color | 8=gray
)(
    // Input
    input logic ahb_clk,
    input logic clk_50, // 50 MHz
    input logic n_rst,

    // Output
    output logic vga_clk, // 25 MHz
    output logic [7:0] vga_r,
    output logic [7:0] vga_g,
    output logic [7:0] vga_b,
    output logic vga_sync_n,
    output logic vga_blank_n,
    output logic vga_hs,
    output logic vga_vs,

    // Bus
    bus_protocol_if.peripheral_vital busif
);

// =====================================================
// Parameters
// =====================================================
localparam VGA_WIDTH     = 640;
localparam VGA_HEIGHT    = 480;

localparam SCALE         = 3;
localparam IMG_WIDTH     = 48; // 86 for grayscale
localparam IMG_HEIGHT    = 48; // 86 for grayscale

localparam X_START       = (VGA_WIDTH - IMG_WIDTH) / 2;
localparam X_END         = X_START + IMG_WIDTH - 1;
localparam Y_START       = (VGA_HEIGHT - IMG_HEIGHT) / 2;
localparam Y_END         = Y_START + IMG_HEIGHT - 1;

localparam NEW_X_START   = X_START - IMG_WIDTH;
localparam NEW_X_END     = X_END + IMG_WIDTH;
localparam NEW_Y_START   = Y_START - IMG_HEIGHT;
localparam NEW_Y_END     = Y_END + IMG_HEIGHT;

// =====================================================
// Internal Signals
// =====================================================
logic clk_25;
logic is_center;
logic [9:0] vga_x, vga_y;
logic [9:0] fb_x, fb_y;
logic [DATA_WIDTH-1:0] rdata; 
logic [DATA_WIDTH-1:0] mem [(IMG_WIDTH*IMG_HEIGHT)-1:0];
logic [ADDR_WIDTH-1:0] raddr;

// =====================================================
// VGA Controller
// =====================================================
vga_controller VGA_CNT (
    .clk_25(vga_clk),
    .n_rst(n_rst),
    .hsync(vga_hs),
    .vsync(vga_vs),
    .video_on(vga_blank_n),
    .synch(vga_sync_n),
    .x_coordinate(vga_x),
    .y_coordinate(vga_y)
);

// =====================================================
// Clock Divider (50MHz -> 25MHz)
// =====================================================
assign vga_clk = clk_25;
always_ff @(posedge clk_50, negedge n_rst) begin
    if (!n_rst) clk_25 <= 1'b0;
    else clk_25 <= ~clk_25;
end

// =====================================================
// AHB Bus Interface
// =====================================================
always_comb begin
    busif.request_stall = 1'b0;
    busif.error = 1'b0;
    busif.rdata = '0;
end

// =====================================================
// XY Coordinate
// =====================================================
always_comb begin
    fb_x = '0;
    fb_y = '0;
    is_center = 1'b0;

    if (vga_x >= NEW_X_START && vga_x <= NEW_X_END) begin
        if (vga_y >= NEW_Y_START && vga_y <= NEW_Y_END) begin
            fb_x = (vga_x - NEW_X_START) / SCALE;
            fb_y = (vga_y - NEW_Y_START) / SCALE;
            is_center = 1'b1;
        end
    end
end

// =====================================================
// Frame Buffer Logic
// =====================================================
always_ff @(posedge ahb_clk) begin
    if (busif.wen) mem[busif.addr >> 2] <= busif.wdata;
end

always_comb begin
    raddr = (fb_y * IMG_WIDTH) + fb_x;
    rdata = is_center ? mem[raddr] : '0;
end

// =====================================================
// RGB Output Logic
// =====================================================
// Gray
// always_comb begin
//     vga_r = rdata[7:6];
//     vga_g = rdata[5:2];
//     vga_b = rdata[1:0];
// end

// Color
always_comb begin
    vga_r = rdata[23:16];
    vga_g = rdata[15:8];
    vga_b = rdata[7:0];
end

endmodule