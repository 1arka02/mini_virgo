`default_nettype none
`timescale 1ns/1ns

`include "common.sv"

// Virgo Dedicated Cluster Matrix Unit
// Disaggregated from the SIMT cores and placed at the Cluster (SM) level.
// Performs matrix multiply-accumulate operations directly on Cluster Shared Memory.
// Bypasses SIMT core register files completely to maximize FLOPS scalability and energy efficiency.
module matrix_unit #(
    parameter int MAX_DIM = 8,
    parameter int ADDR_WIDTH = 32,
    parameter int DATA_WIDTH = 32
) (
    input wire clk,
    input wire reset,

    // Control Interface
    input wire                    start,
    input wire                    accumulate,
    input wire                    clear_acc,
    input wire [3:0]              dim_m,       // Tile M (rows of A & C)
    input wire [3:0]              dim_n,       // Tile N (cols of B & C)
    input wire [3:0]              dim_k,       // Tile K (inner dimension)
    input wire [ADDR_WIDTH-1:0]   addr_a,      // Shared memory offset for A
    input wire [ADDR_WIDTH-1:0]   addr_b,      // Shared memory offset for B
    input wire [ADDR_WIDTH-1:0]   addr_c,      // Shared memory offset for C
    input wire [1:0]              precision,   // 0: INT32, 1: INT8x4, 2: FP8x4

    output logic                  busy,
    output logic                  done,
    output logic [DATA_WIDTH-1:0] cycle_count,

    // Shared Memory Interface (Port B)
    output logic                  mem_read_valid,
    output logic [ADDR_WIDTH-1:0] mem_read_address,
    input wire                    mem_read_ready,
    input wire [DATA_WIDTH-1:0]   mem_read_data,

    output logic                  mem_write_valid,
    output logic [ADDR_WIDTH-1:0] mem_write_address,
    output logic [DATA_WIDTH-1:0] mem_write_data,
    input wire                    mem_write_ready
);

    typedef enum logic [2:0] {
        ST_IDLE,
        ST_READ_A,
        ST_WAIT_A,
        ST_READ_B,
        ST_WAIT_B,
        ST_WRITE_C,
        ST_WAIT_C,
        ST_DONE
    } state_t;

    state_t state;

    // Internal accumulator array for matrix C: [MAX_DIM][MAX_DIM]
    logic signed [DATA_WIDTH-1:0] acc [MAX_DIM][MAX_DIM];

    // Latched control signals
    logic [3:0] reg_m, reg_n, reg_k;
    logic [ADDR_WIDTH-1:0] reg_addr_a, reg_addr_b, reg_addr_c;
    logic reg_accumulate;
    logic [1:0] reg_precision;

    localparam int DIM_BITS = $clog2(MAX_DIM);

    // Loop indices
    logic [DIM_BITS-1:0] idx_i, idx_j, idx_k;

    // Temporary element registers
    logic signed [DATA_WIDTH-1:0] elem_a;
    logic signed [DATA_WIDTH-1:0] elem_b;

    // FP8 Datapath
    logic signed [DATA_WIDTH-1:0] fp8_mac_out;
    fp8_dp4 fp8_inst (
        .a_packed(elem_a),
        .b_packed(mem_read_data),
        .result(fp8_mac_out)
    );

    // Cycle profiling counter
    logic [DATA_WIDTH-1:0] cycles;

    wire [ADDR_WIDTH-1:0] offset_a = (ADDR_WIDTH'(idx_i) * ADDR_WIDTH'(reg_k) + ADDR_WIDTH'(idx_k)) << 2;
    wire [ADDR_WIDTH-1:0] offset_b = (ADDR_WIDTH'(idx_k) * ADDR_WIDTH'(reg_n) + ADDR_WIDTH'(idx_j)) << 2;
    wire [ADDR_WIDTH-1:0] offset_c = (ADDR_WIDTH'(idx_i) * ADDR_WIDTH'(reg_n) + ADDR_WIDTH'(idx_j)) << 2;

    always_ff @(posedge clk) begin
        if (reset) begin
            state           <= ST_IDLE;
            busy            <= 1'b0;
            done            <= 1'b0;
            cycle_count     <= '0;
            cycles          <= '0;
            mem_read_valid  <= 1'b0;
            mem_read_address<= '0;
            mem_write_valid <= 1'b0;
            mem_write_address <= '0;
            mem_write_data  <= '0;
            idx_i           <= '0;
            idx_j           <= '0;
            idx_k           <= '0;
            elem_a          <= '0;
            elem_b          <= '0;
            reg_m           <= '0;
            reg_n           <= '0;
            reg_k           <= '0;
            reg_addr_a      <= '0;
            reg_addr_b      <= '0;
            reg_addr_c      <= '0;
            reg_accumulate  <= 1'b0;
            reg_precision   <= '0;

            for (int r = 0; r < MAX_DIM; r++) begin
                for (int c = 0; c < MAX_DIM; c++) begin
                    acc[r][c] <= '0;
                end
            end
        end else begin
            if (busy) begin
                cycles <= cycles + 1;
            end

            case (state)
                ST_IDLE: begin
                    done <= 1'b0;
                    if (clear_acc) begin
                        for (int r = 0; r < MAX_DIM; r++) begin
                            for (int c = 0; c < MAX_DIM; c++) begin
                                acc[r][c] <= '0;
                            end
                        end
                    end

                    if (start) begin
                        $display("MU: START received, m=%0d n=%0d k=%0d addr_a=%h addr_b=%h addr_c=%h acc=%0d",
                                 dim_m, dim_n, dim_k, addr_a, addr_b, addr_c, accumulate);
                        busy           <= 1'b1;
                        cycles         <= '0;
                        reg_m          <= (dim_m == 0 || dim_m > MAX_DIM[3:0]) ? 4'd4 : dim_m;
                        reg_n          <= (dim_n == 0 || dim_n > MAX_DIM[3:0]) ? 4'd4 : dim_n;
                        reg_k          <= (dim_k == 0 || dim_k > MAX_DIM[3:0]) ? 4'd4 : dim_k;
                        reg_addr_a     <= addr_a;
                        reg_addr_b     <= addr_b;
                        reg_addr_c     <= addr_c;
                        reg_accumulate <= accumulate;
                        reg_precision  <= precision;

                        if (!accumulate && !clear_acc) begin
                            for (int r = 0; r < MAX_DIM; r++) begin
                                for (int c = 0; c < MAX_DIM; c++) begin
                                    acc[r][c] <= '0;
                                end
                            end
                        end

                        idx_i <= '0;
                        idx_j <= '0;
                        idx_k <= '0;
                        state <= ST_READ_A;
                    end
                end

                ST_READ_A: begin
                    mem_read_valid   <= 1'b1;
                    mem_read_address <= reg_addr_a + offset_a;
                    state            <= ST_WAIT_A;
                end

                ST_WAIT_A: begin
                    if (mem_read_ready) begin
                        mem_read_valid <= 1'b0;
                        elem_a         <= mem_read_data;
                        state          <= ST_READ_B;
                    end
                end

                ST_READ_B: begin
                    mem_read_valid   <= 1'b1;
                    mem_read_address <= reg_addr_b + offset_b;
                    state            <= ST_WAIT_B;
                end

                ST_WAIT_B: begin
                    if (mem_read_ready) begin
                        mem_read_valid <= 1'b0;
                        elem_b         <= mem_read_data;

                        // Systolic MAC Accumulation
                        if (reg_precision == 2'b01) begin
                            // INT8x4 (DP4A)
                            logic signed [7:0] a0, a1, a2, a3;
                            logic signed [7:0] b0, b1, b2, b3;
                            
                            {a3, a2, a1, a0} = elem_a;
                            {b3, b2, b1, b0} = mem_read_data;

                            acc[idx_i][idx_j] <= acc[idx_i][idx_j] + 
                                                 (a0 * b0) + (a1 * b1) + (a2 * b2) + (a3 * b3);
                        end else if (reg_precision == 2'b10) begin
                            // FP8x4 (E4M3)
                            acc[idx_i][idx_j] <= acc[idx_i][idx_j] + fp8_mac_out;
                        end else begin
                            // INT32 (Legacy)
                            acc[idx_i][idx_j] <= acc[idx_i][idx_j] + (elem_a * $signed(mem_read_data));
                        end

                        // Advance loop indices
                        begin
                            logic [31:0] k_step;
                            k_step = (reg_precision == 2'b00) ? 1 : 4;

                            if (32'(idx_k) + k_step < 32'(reg_k)) begin
                                idx_k <= idx_k + k_step[DIM_BITS-1:0];
                                state <= ST_READ_A;
                            end else begin
                                idx_k <= '0;
                            if (32'(idx_j) + 1 < 32'(reg_n)) begin
                                idx_j <= idx_j + 1'b1;
                                state <= ST_READ_A;
                            end else begin
                                idx_j <= '0;
                                if (32'(idx_i) + 1 < 32'(reg_m)) begin
                                    idx_i <= idx_i + 1'b1;
                                    state <= ST_READ_A;
                                end else begin
                                    // All MAC operations completed! Transition to writeback.
                                    idx_i <= '0;
                                    idx_j <= '0;
                                    state <= ST_WRITE_C;
                                end
                            end
                        end
                        end // end of begin block for k_step
                    end
                end

                ST_WRITE_C: begin
                    mem_write_valid   <= 1'b1;
                    mem_write_address <= reg_addr_c + offset_c;
                    mem_write_data    <= acc[idx_i][idx_j];
                    state             <= ST_WAIT_C;
                end

                ST_WAIT_C: begin
                    if (mem_write_ready) begin
                        mem_write_valid <= 1'b0;

                        if (32'(idx_j) + 1 < 32'(reg_n)) begin
                            idx_j <= idx_j + 1'b1;
                            state <= ST_WRITE_C;
                        end else begin
                            idx_j <= '0;
                            if (32'(idx_i) + 1 < 32'(reg_m)) begin
                                idx_i <= idx_i + 1'b1;
                                state <= ST_WRITE_C;
                            end else begin
                                // Writeback completed!
                                state <= ST_DONE;
                            end
                        end
                    end
                end

                ST_DONE: begin
                    $display("MU: DONE, cycles=%0d", cycles);
                    busy        <= 1'b0;
                    done        <= 1'b1;
                    cycle_count <= cycles;
                    state       <= ST_IDLE;
                end
            endcase
        end
    end

endmodule
