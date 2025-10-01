`timescale 1ns / 10ps

module crc #(
    parameter DATA_BYTES = 8,    // default width of input data
    parameter CRC_WIDTH = 32     // default width of CRC polynomial
)(
    input  logic clk,                    // Clock signal
    input  logic n_rst,                  // Active-low reset
    input  logic [(DATA_BYTES*8)-1:0] data_in, // Input data
    input  logic [CRC_WIDTH-1:0] crc_in, // Initial value of CRC
    input  logic [CRC_WIDTH-1:0] polynomial, //user chosen polynomial
    input  logic enable,                   // enable signal
    input  logic reflect_in, // 입력 바이트 단위로 MSB↔LSB 비트 반전 후 CRC 연산에 사용.
    input  logic reflect_out, // 최종 CRC 값에 적용해서 전체 비트 순서를 반전.
    input  logic [CRC_WIDTH-1:0] final_xor_val, // 최종 단계에서 한 번 XOR-out 처리.
    output logic [CRC_WIDTH-1:0] crc_out  // 최종 계산 결과 레지스터 값
);

    logic [CRC_WIDTH-1:0] next_crc;
    logic [CRC_WIDTH-1:0] crc_temp;
    logic [(DATA_BYTES*8)-1:0] input_data;
    logic [7:0] data_byte;
    integer i, j;

    function automatic logic [7:0] reverse_data_byte(logic [7:0] in_data);
        integer k;
        logic [7:0] reverse_data;

        for (k = 0; k < 8; k = k + 1) begin
            reverse_data[k] = in_data[7-k];
        end
        return reverse_data;
    endfunction
   
    function automatic logic [CRC_WIDTH-1:0] reverse_bits_crc(logic [CRC_WIDTH-1:0] in_data);
        integer k;
        logic [CRC_WIDTH-1:0] reverse_crc;

        for (k = 0; k < CRC_WIDTH; k = k + 1) begin
            reverse_crc[k] = in_data[CRC_WIDTH-1-k];
        end
        return reverse_crc;
    endfunction

    always_ff @(posedge clk or negedge n_rst) begin
        if (~n_rst) begin
            crc_out <= {CRC_WIDTH{1'b1}};
        end else begin
            crc_out <= crc_temp;
        end
    end

    always_comb begin
        data_byte = 0; 
        crc_temp = 0; 
        next_crc = crc_in;
       
        input_data = data_in;
        // if (reflect_in) begin
        //     input_data = reverse_bits_data(data_in);
        // end
           
        if (enable) begin
            for (j = DATA_BYTES - 1; j >= 0; j = j - 1) begin
                // data_byte = input_data[j*8 +: 8];
                if(reflect_in) begin
                    data_byte = reverse_data_byte(input_data[j*8 +: 8]);
                end else begin
                    data_byte = input_data[j*8 +: 8];
                end
                
                
                next_crc = next_crc ^ ({data_byte,{(CRC_WIDTH-8){1'b0}}}); //next_crc = next_crc ^ (data_byte << (CRC_WIDTH - 8));
                for (i = 0; i < 8; i = i + 1) begin
                    if (next_crc[CRC_WIDTH-1]) begin
                        next_crc = (next_crc << 1) ^ polynomial;
                    end else begin
                        next_crc = next_crc << 1;
                    end
                end
               
            end
        end
           
        // Final XOR
        crc_temp = next_crc ^ final_xor_val;
           
        // Reflect output if required
        if (reflect_out) begin
            crc_temp = reverse_bits_crc(crc_temp);
        end
   
    end

endmodule