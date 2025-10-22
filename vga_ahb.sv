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

// Declareation
logic clk_25;
logic fb_wen;
logic [DATA_WIDTH-1:0] fb_wdata;
logic [ADDR_WIDTH-1:0] fb_waddr;
logic [9:0] vga_x, vga_y;
logic [9:0] fb_x, fb_y;
logic [23:0] rdata; 
logic [ADDR_WIDTH-1:0] raddr;
logic [DATA_WIDTH-1:0] mem [MEM_DEPTH-1:0];

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
    .clk_25(vga_clk),
    .n_rst(n_rst),
    .hsync(vga_hs),
    .vsync(vga_vs),
    .video_on(vga_blank_n),
    .synch(vga_sync_n),
    .x_coordinate(vga_x),
    .y_coordinate(vga_y)
);

always_ff @(posedge ahb_clk) begin
    if(fb_wen) mem[fb_waddr] <= fb_wdata;
end

always_comb begin
    // Temp Variables
    logic [11:0] pixel_index;
    logic [31:0] word_data;
    logic [7:0] gray_pixel_value;

    fb_x = 10'b0;
    fb_y = 10'b0;
    raddr = '0;
    rdata = '0; // Black Pixel

    if(vga_x >= X_START && vga_x < X_END) begin
        if(vga_y >= Y_START && vga_y < Y_END) begin
            fb_x = vga_x - X_START;
            fb_y = vga_y - Y_START;

            pixel_index = (fb_y << 6) + fb_x; // (fb_y * 64) + fb_x
            raddr = pixel_index >> 2;
            word_data = mem[raddr];

            case(pixel_index[1:0])
                2'd0: gray_pixel_value = word_data[7:0];
                2'd1: gray_pixel_value = word_data[15:8];
                2'd2: gray_pixel_value = word_data[23:16];
                2'd3: gray_pixel_value = word_data[31:24];
                default: gray_pixel_value = 8'h00;
            endcase

            rdata = {gray_pixel_value, gray_pixel_value, gray_pixel_value};
        end
    end
end

always_comb begin
    vga_r = 8'b0;
    vga_g = 8'b0;
    vga_b = 8'b0;

    if(vga_blank_n) begin
        vga_r = rdata[23:16];
        vga_g = rdata[15:8];
        vga_b = rdata[7:0];
    end
end
endmodule
