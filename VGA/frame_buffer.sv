module frame_buffer #(
    parameter int ADDR_WIDTH = 32,
    parameter int DATA_WIDTH = 32,
    parameter int FRAME_WIDTH = 320,
    parameter int FRAME_HEIGHT = 240,
    parameter int MEM_DEPTH = 153600
)(
    input logic clk,
    input logic n_rst,

    // Subordinate
    input logic wen,
    input logic [ADDR_WIDTH-1:0] waddr,
    input logic [DATA_WIDTH-1:0] wdata,

    // VGA Controller
    input logic ren,
    input logic [9:0] x_coordinate,
    input logic [9:0] y_coordinate,
    output logic [23:0] rdata
)
    logic [DATA_WIDTH-1:0] mem [MEM_DEPTH-1:0]; // Memory Storage
    logic [ADDR_WIDTH-1:0] raddr;
    assign raddr = y_coordinate * FRAME_WIDTH + x_coordinate;

    // Writing
    always_ff @(posedge clk, negedge n_rst) begin
        if(wen) mem[waddr] <= wdata;
    end

    // Reading
    assign rdata == mem[raddr][23:0];
endmodule