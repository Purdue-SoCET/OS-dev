module top_vga (
    input logic clk,
    input logic n_rst,

    output logic vga_clk,
    output logic [7:0] vga_r,
    output logic [7:0] vga_g,
    output logic [7:0] vga_b,

    output logic vga_sync_n,
    output logic vga_blank_n,

    output logic vga_hs,
    output logic vga_vs,

    bus_protocol_if.peripheral_vital busif
);

    //======================================================
    // VGA Clock Generation (25 MHz)
    //======================================================
    clk25 CLK_DIV (
        .clk_50(clk),
        .n_rst(n_rst),
        .clk_12_5(vga_clk)
    );

    //======================================================
    // VGA Subordinate — Write Path (Bus → Frame Buffer)
    //======================================================
    logic fb_wen;
    logic [DATA_WIDTH-1:0] fb_wdata;
    logic [ADDR_WIDTH-1:0] fb_waddr;
    vga_subordinate VGA_SUB (
        .clk(clk),
        .n_rst(n_rst),
        .fb_wen(fb_wen),
        .fb_wdata(fb_wdata),
        .fb_waddr(fb_waddr),
        .busif(busif)
    );

    //======================================================
    // VGA Controller — Pixel Coordinate Generation
    //======================================================
    logic [9:0] vga_x, vga_y;
    vga_controller VGA_CNT (
        .clk(clk),
        .clk_25(vga_clk),
        .n_rst(n_rst),
        .hsync(vga_hs),
        .vsync(vga_vs),
        .video_on(vga_blank_n),
        .synch(vga_sync_n),
        .x_coordinate(vga_x),
        .y_coordinate(vga_y)
    );

    // =============================
    //  Resolution Converter (640x480 → 320x240)
    // =============================
    logic [9:0] fb_x, fb_y;
    converter_640_320 CVT (
        .vga_x(vga_x),
        .vga_y(vga_y),
        .fb_x(fb_x),
        .fb_y(fb_y)
    );

    //======================================================
    // Frame Buffer — Dual-Port Memory (write/read domains)
    //======================================================
    logic [23:0] pixel_data;
    frame_buffer FB (
        .clk(clk),
        .n_rst(n_rst),
        .wen(fb_wen),
        .waddr(fb_waddr),
        .wdata(fb_wdata),
        .ren(vga_blank_n),
        .x_coordinate(fb_x),
        .y_coordinate(fb_y),
        .rdata(pixel_data) // combinational
    );

    always_comb begin
        vga_r = pixel_data[23:16];
        vga_g = pixel_data[15:8];
        vga_b = pixel_data[7:0];
    end

endmodule
