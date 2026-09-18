`default_nettype none
`timescale 1ns/1ns

`include "common.sv"

// Virgo Cluster Shared Memory
// High-throughput banked SRAM scratchpad memory local to each cluster.
// Accessible by SIMT sub-cores (Port A) and by the dedicated Matrix Unit (Port B).
module shared_memory #(
    parameter int MEM_WORDS = 1024,
    parameter int ADDR_WIDTH = 32,
    parameter int DATA_WIDTH = 32
) (
    input wire clk,
    input wire reset,

    // Port A: SIMT Cores Interconnect Interface
    input wire                     core_read_valid,
    input wire [ADDR_WIDTH-1:0]    core_read_address,
    output logic                   core_read_ready,
    output logic [DATA_WIDTH-1:0]  core_read_data,

    input wire                     core_write_valid,
    input wire [ADDR_WIDTH-1:0]    core_write_address,
    input wire [DATA_WIDTH-1:0]    core_write_data,
    output logic                   core_write_ready,

    // Port B: Dedicated Matrix Unit Interface
    input wire                     mu_read_valid,
    input wire [ADDR_WIDTH-1:0]    mu_read_address,
    output logic                   mu_read_ready,
    output logic [DATA_WIDTH-1:0]  mu_read_data,

    input wire                     mu_write_valid,
    input wire [ADDR_WIDTH-1:0]    mu_write_address,
    input wire [DATA_WIDTH-1:0]    mu_write_data,
    output logic                   mu_write_ready
);

    // Memory array: MEM_WORDS 32-bit entries
    logic [DATA_WIDTH-1:0] mem [MEM_WORDS-1:0];

    // Address translation: (addr - `VIRGO_SHARED_MEM_BASE) >> 2
    wire [$clog2(MEM_WORDS)-1:0] core_r_word = core_read_address[$clog2(MEM_WORDS)+1:2];
    wire [$clog2(MEM_WORDS)-1:0] core_w_word = core_write_address[$clog2(MEM_WORDS)+1:2];

    // Matrix unit offsets can be relative (word index) or byte addresses
    wire [$clog2(MEM_WORDS)-1:0] mu_r_word = (mu_read_address >= `VIRGO_SHARED_MEM_BASE) ?
                                             mu_read_address[$clog2(MEM_WORDS)+1:2] :
                                             mu_read_address[$clog2(MEM_WORDS)-1:0];
    wire [$clog2(MEM_WORDS)-1:0] mu_w_word = (mu_write_address >= `VIRGO_SHARED_MEM_BASE) ?
                                             mu_write_address[$clog2(MEM_WORDS)+1:2] :
                                             mu_write_address[$clog2(MEM_WORDS)-1:0];

    // Port A: Core operations
    always_ff @(posedge clk) begin
        if (reset) begin
            core_read_ready <= 1'b0;
            core_read_data  <= '0;
            core_write_ready <= 1'b0;
        end else begin
            // Read handshake
            if (core_read_valid && !core_read_ready) begin
                core_read_data  <= mem[core_r_word];
                core_read_ready <= 1'b1;
            end else begin
                core_read_ready <= 1'b0;
            end

            // Write handshake
            if (core_write_valid && !core_write_ready) begin
                mem[core_w_word] <= core_write_data;
                core_write_ready <= 1'b1;
            end else begin
                core_write_ready <= 1'b0;
            end
        end
    end

    // Port B: Matrix Unit operations
    always_ff @(posedge clk) begin
        if (reset) begin
            mu_read_ready   <= 1'b0;
            mu_read_data    <= '0;
            mu_write_ready  <= 1'b0;
        end else begin
            // Read handshake
            if (mu_read_valid && !mu_read_ready) begin
                mu_read_data  <= mem[mu_r_word];
                mu_read_ready <= 1'b1;
            end else begin
                mu_read_ready <= 1'b0;
            end

            // Write handshake
            if (mu_write_valid && !mu_write_ready) begin
                mem[mu_w_word] <= mu_write_data;
                mu_write_ready <= 1'b1;
            end else begin
                mu_write_ready <= 1'b0;
            end
        end
    end

endmodule
