module vga_controller #(
    parameter HVID = 640,     // horizontal active video width pix clocks
    parameter HFP = 16,      // horizontal front porch pix clocks
    parameter HS = 96,      // horizontal hsync pulse width pix clocks
    parameter HBP = 48,      // horizontal back porch pix clocks
    parameter VVID = 480,     // vertical active video lines
    parameter VFP = 10,      // vertical front porch video lines
    parameter VS = 2,       // vertical vsync pulse width video lines
    parameter VBP = 29       // vertical back porch video lines
) (
    input logic clk,
    input logic clk_25,
    input logic n_rst,
    output logic hsync,
    output logic vsync,
    output logic video_on, // high when active display region
    output logic synch,
    output logic [9:0] x_coordinate,
    output logic [9:0] y_coordinate
);

localparam HC_MAX = HVID + HFP+HS+HBP;   // one more than the max horizontal count value
localparam VC_MAX = VVID + VFP+VS+VBP;   // one more than the max vertical count value

localparam HSYNC_BEGIN = HVID + HFP;         // first pix clock hsync should go on
localparam HSYNC_END = HSYNC_BEGIN+HS;   // first pix clock that hsync should go off

localparam VSYNC_BEGIN = VVID+VFP;
localparam VSYNC_END = VSYNC_BEGIN+VS;

logic next_hsync;
logic next_vsync;

logic [9:0] pixel_x;
logic [9:0] pixel_y;
logic [9:0] next_pixel_x;
logic [9:0] next_pixel_y;
logic next_video_on;

always_ff @(posedge clk, negedge n_rst) begin
    if(!n_rst) begin
        pixel_x <= '0;
        pixel_y <= '0;
        video_on <= 0;
        vsync <= 0;
        hsync <= 0;
    end
    else if(vga_clk) begin
        pixel_x <= next_pixel_x;
        pixel_y <= next_pixel_y;
        video_on <= next_video_on;
        vsync <= next_vsync;
        hsync <= next_hsync;
    end
end

always_comb begin
    synch = 1'b1;
    
    if(pixel_x >= HC_MAX - 1) next_pixel_x = 0;
    else next_pixel_x = pixel_x + 1;

    if (!next_pixel_x) next_pixel_y = (pixel_y >= VC_MAX-1) ? 0 : pixel_y + 1;
    else next_pixel_y = pixel_y;

    next_video_on = (next_pixel_x < HVID && next_pixel_y < VVID) ? 1 : 0;
    next_hsync = (next_pixel_x >= HSYNC_BEGIN && next_pixel_x < HSYNC_END) ? 1 : 0;
    next_vsync = (next_pixel_y >= VSYNC_BEGIN && next_pixel_y < VSYNC_END) ? 1 : 0;
end

assign x_coordinate = pixel_x;
assign y_coordinate = pixel_y;

endmodule