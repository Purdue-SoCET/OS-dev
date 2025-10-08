// - bus_protocol_if 에 붙는 CRC AHB subordinate
// - 레지스터 맵은 32바이트 (0x00 ~ 0x1C) 고정
// - 내부에서 기존 crc.sv 모듈을 4바이트 fold 엔진으로 사용
// CTRL로 세션 시작(INIT) → DATA에 32비트씩 쓰면 내부 crc.sv가 4바이트 단위로 누적 → FINALIZE로 XOR/REF_OUT 후 RESULT에 결과 래치.
// STAT의 BUSY/DONE으로 진행상태 확인, LEN은 바이트 카운트(+4/word).
`timescale 1ns/1ps

module crc_subordinate #(
    parameter int ADDR_WIDTH = 32,
    parameter int DATA_WIDTH = 32,
    parameter logic [ADDR_WIDTH-1:0] BASE_ADDR = 32'h9000_3000 // Matches AHB_MAP[CRC_AHB_IDX] in aftx07_mmap.vh

)(
    input logic CLK,
    input logic nRST,
    bus_protocol_if.peripheral_vital busif
);

    // Register map (32-bit word aligned)
    // 0x00 CTRL   : [0]EN [1]INIT(pulse) [2]FINALIZE(pulse) [3]REF_IN [4]REF_OUT [5]XOR_EN
    // 0x04 POLY   : CRC polynomial
    // 0x08 INIT   : seed
    // 0x0C XOROUT : final xor constant
    // 0x10 DATA   : write-only, 32-bit stream (bytes LSB-first)
    // 0x14 LENGTH : byte counter (+4 per DATA write; SW-writable to clear)
    // 0x18 STATUS : [0]BUSY [1]DONE
    // 0x1C RESULT : final result

    typedef enum logic [3:0] {
        A_CTRL = 4'h0, A_POLY = 4'h1, A_INIT = 4'h2, A_XORO = 4'h3,
        A_DATA = 4'h4, A_LEN  = 4'h5, A_STAT = 4'h6, A_RES  = 4'h7
    } addr_e;

    // Decode offset word index (0x00~0x1C → addr[5:2])
    addr_e addr_dec;
    always_comb addr_dec = addr_e'(busif.addr[5:2]);

    // Simple request flags (동일 사이클 ren/wen 동시 발생 안 한다고 가정)
    wire do_write = busif.wen;
    wire do_read  = busif.ren;

    // Hardware registers
    logic [31:0] r_ctrl;  // 0x00 제어 비트 저장
    logic [31:0] r_poly; // 0x04 crc poly value
    logic [31:0] r_init; // 0x08 initial seed value (0xFFFF_FFFF)
    logic [31:0] r_xorout; // 0x0C
    logic [31:0] r_len; // 0x14 처리된 데이터의 누적 길이 (DATA write 할 때마다 +4)
    logic [31:0] r_result; //0x1C 
    logic r_busy; // 현재 연산 중인가
    logic r_done; // 연산 완료인가 

    // Persistent cfg bits
    wire cfg_en = r_ctrl[0]; // CRC 모듈이 동작 가능한 상태인지 제어 (0이면 CRC 무시)
    wire cfg_ref_in = r_ctrl[3]; // 입력 데이터를 비트 반전해서 처리할지말지 
    wire cfg_ref_out = r_ctrl[4]; // 최종 결과를 비트 반전시킬지 여부
    wire cfg_xor_en = r_ctrl[5]; // 최종 단계에서 XOR 상수를 적용할지 여부

    // Next-state
    logic [31:0] next_r_ctrl, next_r_poly, next_r_init, next_r_xorout, next_r_len, next_r_result;
    logic next_busy, next_done;
    logic [31:0] next_rdata;

    // Streaming CRC state (중간 누적 상태)
    logic [31:0] crc_state; // 현재 사이클에 저장되어 있는 누적 CRC value
    logic [31:0] next_crc_state; // 다음 사이클에 저장할 누적 value

    // DATA write → crc core를 1사이클 구동
    logic data_fire; // 1 cycle strobe
    logic [31:0] data_word; // 버스로부터 받은 32b word

    // Reverse 32-bit for reflect_out at finalize time
    function automatic logic [31:0] reverse32(input logic [31:0] x);
        logic [31:0] r; integer i;
        begin for (i=0;i<32;i++) r[i] = x[31-i]; return r; end
    endfunction

    // 기존 crc.sv를 4B(32bit) fold 엔진으로 사용
    // - DATA_BYTES = 4 고정: DATA write 1회 = 바이트 4개 fold 1회
    // - 스트리밍 중엔 reflect_out=0, final_xor=0으로 순수 상태만 갱신
    // - crc_in : 이전 상태(crc_state), enable(data_fire) 1사이클 입력
    // - crc_out : 다음 상태 후보(crc_out_wire), FF에서 잡아 crc_state 갱신
    // - FINALIZE 시점에만 REF_OUT/XOR_OUT 적용해서 RESULT 생성

    logic [31:0] crc_out_wire;
    crc #(
        .DATA_BYTES(4),
        .CRC_WIDTH (32)
    ) u_crc (
        .clk (CLK),
        .n_rst (nRST),
        .data_in (data_word), // 32b from bus (내부에서 LSB to MSB 바이트 순서로 fold)
        .crc_in (crc_state), // 직전 누적 상태(seed) 입력
        .polynomial (r_poly),  // 현재 선택된 다항식
        .enable (data_fire), // DATA write 시 1사이클만 펄스로 구동
        .reflect_in (cfg_ref_in), // 입력 바이트 리플렉션 여부
        .reflect_out (1'b0), // 스트리밍 중엔 미적용
        .final_xor_val(32'h0), // 스트리밍 중엔 미적용
        .crc_out (crc_out_wire) // 다음 상태 누적 후보
    );

    // Combinational next-state
    always_comb begin
        // Default hold
        next_r_ctrl = r_ctrl;
        next_r_poly = r_poly;
        next_r_init = r_init;
        next_r_xorout = r_xorout;
        next_r_len = r_len;
        next_r_result = r_result;
        next_busy = r_busy;
        next_done = r_done;
        next_rdata = '0;

        data_fire = 1'b0; // 코어 작동 안함
        data_word = '0;
        next_crc_state = crc_state;

        // Write path 
        if (do_write) begin
            unique case (addr_dec)
                A_CTRL: begin
                    // 지속 비트(EN/REF_IN/REF_OUT/XOR_EN) 갱신
                    next_r_ctrl[0] = busif.wdata[0]; // EN
                    next_r_ctrl[3] = busif.wdata[3]; // REF_IN
                    next_r_ctrl[4] = busif.wdata[4]; // REF_OUT
                    next_r_ctrl[5] = busif.wdata[5]; // XOR_EN

                    // INIT pulse 시작 
                    if (busif.wdata[1] && next_r_ctrl[0]) begin
                        next_crc_state = r_init;  // seed 로드
                        next_busy = 1'b1; // 계산 시작
                        next_done = 1'b0; // 이전 완료 플래그 클리어
                        next_r_len = '0;  // 길이 카운터 리셋
                    end
                    // FINALIZE pulse (only for the busy)
                    if (busif.wdata[2] && next_r_ctrl[0] && r_busy) begin
                        logic [31:0] outv;
                        outv = crc_state; // 누적 상태를 베이스
                        if (cfg_xor_en) outv ^= r_xorout; // XOR-out
                        if (cfg_ref_out) outv  = reverse32(outv); // reflect-out
                        next_r_result = outv; // 결과 레지스터 기록
                        next_done = 1'b1; // 완료 플래그 
                        next_busy = 1'b0; // no more data 
                    end
                end

                // 갱신 
                A_POLY: next_r_poly = busif.wdata;
                A_INIT: next_r_init = busif.wdata;
                A_XORO: next_r_xorout = busif.wdata;

                // 데이터 스트림: 4바이트씩 한 번 fold
                A_DATA: begin
                    // 4바이트 스트림 입력
                    if (cfg_en && r_busy) begin
                        data_fire  = 1'b1;  // crc core 1사이클 구동
                        data_word  = busif.wdata; // 내부 fold는 LSB-first
                        next_r_len = r_len + 32'd4; // 길이 +4 증가 
                    end
                end
                // 소프트웨어가 LENGTH를 임의로 클리어 가능
                A_LEN : next_r_len = busif.wdata; 
                default: ;
            endcase
        end

        // READ path 
        if (do_read) begin
            unique case (addr_dec)
                A_CTRL : next_rdata = r_ctrl;
                A_POLY : next_rdata = r_poly;
                A_INIT : next_rdata = r_init;
                A_XORO : next_rdata = r_xorout;
                A_DATA : next_rdata = 32'h0; // write-only
                A_LEN  : next_rdata = r_len;
                A_STAT : next_rdata = {29'b0, 1'b0, r_done, r_busy}; // [1]=DONE [0]=BUSY
                A_RES  : next_rdata = r_result;
                default: next_rdata = '0;
            endcase
        end
    end

    // Registers outputs
    always_ff @(posedge CLK or negedge nRST) begin
        if (!nRST) begin
            // CRC-32 preset
            r_ctrl <= 32'b0;
            r_poly <= 32'h04C11DB7;
            r_init <= 32'hFFFF_FFFF;
            r_xorout <= 32'hFFFF_FFFF;
            r_len <= 32'b0;

            r_result <= 32'b0;
            r_busy <= 1'b0;
            r_done <= 1'b0;

            crc_state  <= 32'hFFFF_FFFF; // reset

            busif.rdata <= '0;
            busif.error <= 1'b0;
        end else begin
            r_ctrl <= next_r_ctrl;
            r_poly <= next_r_poly;
            r_init <= next_r_init;
            r_xorout <= next_r_xorout;
            r_len <= next_r_len;

            r_result <= next_r_result;
            r_busy <= next_busy;
            r_done <= next_done;

            // DATA write 사이클 말미에 다음 상태 캡처
            // 이 클럭에서 data_fire=1 이면, 같은 엣지에 u_crc가 fold를 끝내고 crc_out_wire를 바로 이 FF에서 잡아서 다음 상태로 저장
            if (data_fire) begin
                crc_state <= crc_out_wire;
            end

            busif.rdata <= next_rdata;
            busif.error <= 1'b0;
        end
    end

    // 한 사이클 stall: (same as vga)
    always_comb begin
        busif.request_stall = (busif.wen || busif.ren);
    end

endmodule
