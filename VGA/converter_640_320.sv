module converter_640_320 (
    input logic [9:0] vga_x,
    input logic [9:0] vga_y,
    output logic [9:0] fb_x,
    output logic [9:0] fb_y
);

always_comb begin
    fb_x = vga_x >> 1;
    fb_y = vga_y >> 1;
end

endmodule
