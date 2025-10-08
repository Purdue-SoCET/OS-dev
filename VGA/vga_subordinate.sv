module vga_subordinate #(
    parameter int ADDR_WIDTH = 32,
    parameter int DATA_WIDTH = 32
)(
    input logic clk, n_rst,
    output logic fb_wen,
    output logic [DATA_WIDTH-1:0] fb_wdata,
    output logic [ADDR_WIDTH-1:0] fb_waddr

    bus_protocol_if.peripheral_vital busif
);

    always_ff @(posedge clk, negedge n_rst) begin
        if(!n_rst) begin
            fb_wen <= 1'b0;
            fb_wdata <= '0;
            fb_waddr <= '0;
        end
        else begin
            fb_wen <= busif.wen;
            fb_wdata <= busif.wdata;
            fb_waddr <= busif.addr;
        end
    end

    always_comb begin
        busif.request_stall = busif.wen || busif.ren;
        busif.error = 1'b0;
        busif.rdata = '0;
    end

endmodule