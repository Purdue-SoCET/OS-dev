module clk25 (
    input logic clk,
    input logic n_rst,
    output logic clk_25
);

always_ff @(posedge clk, negedge n_rst) begin
    if(!n_rst) begin
        clk_25 <= 0;
    end
    else begin
        clk_25 <= ~clk_25;
    end
end
endmodule