`default_nettype none
`timescale 1ns/1ns

`include "common.sv"

// Virgo Cluster Interconnect
// Demultiplexes memory requests from SIMT sub-cores:
// - Global Memory (< 0x8000_0000): routed out to global memory controller
// - Shared Memory (0x8000_0000 - 0x8000_0FFF): routed to local cluster shared memory
// - MMIO (0x8001_0000 - 0x8001_00FF): routed to local cluster matrix controller
module cluster_interconnect #(
    parameter int NUM_LSUS = 33, // Number of LSUs across cores in this cluster
    parameter int ADDR_WIDTH = 32,
    parameter int DATA_WIDTH = 32
) (
    input wire clk,
    input wire reset,

    // Core LSUs Interface
    input wire  [NUM_LSUS-1:0]   core_read_valid,
    input wire  [ADDR_WIDTH-1:0] core_read_address [NUM_LSUS],
    output logic [NUM_LSUS-1:0]  core_read_ready,
    output logic [DATA_WIDTH-1:0] core_read_data [NUM_LSUS],

    input wire  [NUM_LSUS-1:0]   core_write_valid,
    input wire  [ADDR_WIDTH-1:0] core_write_address [NUM_LSUS],
    input wire  [DATA_WIDTH-1:0] core_write_data [NUM_LSUS],
    output logic [NUM_LSUS-1:0]  core_write_ready,

    // External Global Memory Interface (to top-level mem_controller)
    output logic [NUM_LSUS-1:0]  ext_read_valid,
    output logic [ADDR_WIDTH-1:0] ext_read_address [NUM_LSUS],
    input wire  [NUM_LSUS-1:0]   ext_read_ready,
    input wire  [DATA_WIDTH-1:0] ext_read_data [NUM_LSUS],

    output logic [NUM_LSUS-1:0]  ext_write_valid,
    output logic [ADDR_WIDTH-1:0] ext_write_address [NUM_LSUS],
    output logic [DATA_WIDTH-1:0] ext_write_data [NUM_LSUS],
    input wire  [NUM_LSUS-1:0]   ext_write_ready,

    // Cluster Shared Memory Interface (Port A)
    output logic                  sm_read_valid,
    output logic [ADDR_WIDTH-1:0] sm_read_address,
    input wire                    sm_read_ready,
    input wire  [DATA_WIDTH-1:0]  sm_read_data,

    output logic                  sm_write_valid,
    output logic [ADDR_WIDTH-1:0] sm_write_address,
    output logic [DATA_WIDTH-1:0] sm_write_data,
    input wire                    sm_write_ready,

    // Cluster Matrix MMIO Interface
    output logic                  mmio_read_valid,
    output logic [ADDR_WIDTH-1:0] mmio_read_address,
    input wire                    mmio_read_ready,
    input wire  [DATA_WIDTH-1:0]  mmio_read_data,

    output logic                  mmio_write_valid,
    output logic [ADDR_WIDTH-1:0] mmio_write_address,
    output logic [DATA_WIDTH-1:0] mmio_write_data,
    input wire                    mmio_write_ready
);

    // Address decode helper functions
    function automatic logic is_shared_mem(input [ADDR_WIDTH-1:0] addr);
        return (addr >= `VIRGO_SHARED_MEM_BASE && addr < `VIRGO_SHARED_MEM_LIMIT);
    endfunction

    function automatic logic is_mmio(input [ADDR_WIDTH-1:0] addr);
        return (addr >= `VIRGO_MMIO_BASE && addr < `VIRGO_MMIO_LIMIT);
    endfunction

    // -------------------------------------------------------------
    // 1. Global Memory Pass-through
    // -------------------------------------------------------------
    genvar i;
    generate
        for (i = 0; i < NUM_LSUS; i = i + 1) begin : g_ext_mem
            wire req_is_sm_r   = is_shared_mem(core_read_address[i]);
            wire req_is_mmio_r = is_mmio(core_read_address[i]);
            wire req_is_glob_r = !(req_is_sm_r || req_is_mmio_r);

            wire req_is_sm_w   = is_shared_mem(core_write_address[i]);
            wire req_is_mmio_w = is_mmio(core_write_address[i]);
            wire req_is_glob_w = !(req_is_sm_w || req_is_mmio_w);

            assign ext_read_valid[i]   = core_read_valid[i] && req_is_glob_r;
            assign ext_read_address[i] = core_read_address[i];

            assign ext_write_valid[i]   = core_write_valid[i] && req_is_glob_w;
            assign ext_write_address[i] = core_write_address[i];
            assign ext_write_data[i]    = core_write_data[i];
        end
    endgenerate

    // -------------------------------------------------------------
    // 2. Shared Memory & MMIO Arbitration
    // -------------------------------------------------------------
    // Arbitration state machines for internal cluster accesses
    typedef enum logic [1:0] {
        ARB_IDLE,
        ARB_SERVING_READ,
        ARB_SERVING_WRITE
    } arb_state_t;

    arb_state_t sm_arb_state;
    logic [$clog2(NUM_LSUS)-1:0] sm_consumer;

    arb_state_t mmio_arb_state;
    logic [$clog2(NUM_LSUS)-1:0] mmio_consumer;

    // Shared Memory arbiter
    always_ff @(posedge clk) begin
        if (reset) begin
            sm_arb_state    <= ARB_IDLE;
            sm_consumer     <= '0;
            sm_read_valid   <= 1'b0;
            sm_read_address <= '0;
            sm_write_valid  <= 1'b0;
            sm_write_address<= '0;
            sm_write_data   <= '0;
        end else begin
            case (sm_arb_state)
                ARB_IDLE: begin
                    sm_read_valid  <= 1'b0;
                    sm_write_valid <= 1'b0;

                    // Scan for write requests first, then read
                    for (int j = 0; j < NUM_LSUS; j++) begin
                        if (core_write_valid[j] && is_shared_mem(core_write_address[j])) begin
                            sm_consumer      <= j[$clog2(NUM_LSUS)-1:0];
                            sm_write_valid   <= 1'b1;
                            sm_write_address <= core_write_address[j];
                            sm_write_data    <= core_write_data[j];
                            sm_arb_state     <= ARB_SERVING_WRITE;
                            break;
                        end else if (core_read_valid[j] && is_shared_mem(core_read_address[j])) begin
                            sm_consumer      <= j[$clog2(NUM_LSUS)-1:0];
                            sm_read_valid    <= 1'b1;
                            sm_read_address  <= core_read_address[j];
                            sm_arb_state     <= ARB_SERVING_READ;
                            break;
                        end
                    end
                end

                ARB_SERVING_READ: begin
                    if (sm_read_ready) begin
                        sm_read_valid <= 1'b0;
                        sm_arb_state  <= ARB_IDLE;
                    end
                end

                ARB_SERVING_WRITE: begin
                    if (sm_write_ready) begin
                        sm_write_valid <= 1'b0;
                        sm_arb_state   <= ARB_IDLE;
                    end
                end

                default: sm_arb_state <= ARB_IDLE;
            endcase
        end
    end

    // MMIO arbiter
    always_ff @(posedge clk) begin
        if (reset) begin
            mmio_arb_state    <= ARB_IDLE;
            mmio_consumer     <= '0;
            mmio_read_valid   <= 1'b0;
            mmio_read_address <= '0;
            mmio_write_valid  <= 1'b0;
            mmio_write_address<= '0;
            mmio_write_data   <= '0;
        end else begin
            case (mmio_arb_state)
                ARB_IDLE: begin
                    mmio_read_valid  <= 1'b0;
                    mmio_write_valid <= 1'b0;

                    for (int j = 0; j < NUM_LSUS; j++) begin
                        if (core_write_valid[j] && is_mmio(core_write_address[j])) begin
                            mmio_consumer      <= j[$clog2(NUM_LSUS)-1:0];
                            mmio_write_valid   <= 1'b1;
                            mmio_write_address <= core_write_address[j];
                            mmio_write_data    <= core_write_data[j];
                            mmio_arb_state     <= ARB_SERVING_WRITE;
                            break;
                        end else if (core_read_valid[j] && is_mmio(core_read_address[j])) begin
                            mmio_consumer      <= j[$clog2(NUM_LSUS)-1:0];
                            mmio_read_valid    <= 1'b1;
                            mmio_read_address  <= core_read_address[j];
                            mmio_arb_state     <= ARB_SERVING_READ;
                            break;
                        end
                    end
                end

                ARB_SERVING_READ: begin
                    if (mmio_read_ready) begin
                        mmio_read_valid <= 1'b0;
                        mmio_arb_state  <= ARB_IDLE;
                    end
                end

                ARB_SERVING_WRITE: begin
                    if (mmio_write_ready) begin
                        mmio_write_valid <= 1'b0;
                        mmio_arb_state   <= ARB_IDLE;
                    end
                end

                default: mmio_arb_state <= ARB_IDLE;
            endcase
        end
    end

    // -------------------------------------------------------------
    // 3. Demultiplex Ready & Data Back to Cores
    // -------------------------------------------------------------
    always_comb begin
        for (int k = 0; k < NUM_LSUS; k++) begin
            if (is_shared_mem(core_read_address[k])) begin
                core_read_ready[k] = (sm_arb_state == ARB_SERVING_READ && sm_consumer == k[$clog2(NUM_LSUS)-1:0]) ? sm_read_ready : 1'b0;
                core_read_data[k]  = sm_read_data;
            end else if (is_mmio(core_read_address[k])) begin
                core_read_ready[k] = (mmio_arb_state == ARB_SERVING_READ && mmio_consumer == k[$clog2(NUM_LSUS)-1:0]) ? mmio_read_ready : 1'b0;
                core_read_data[k]  = mmio_read_data;
            end else begin
                core_read_ready[k] = ext_read_ready[k];
                core_read_data[k]  = ext_read_data[k];
            end

            if (is_shared_mem(core_write_address[k])) begin
                core_write_ready[k] = (sm_arb_state == ARB_SERVING_WRITE && sm_consumer == k[$clog2(NUM_LSUS)-1:0]) ? sm_write_ready : 1'b0;
            end else if (is_mmio(core_write_address[k])) begin
                core_write_ready[k] = (mmio_arb_state == ARB_SERVING_WRITE && mmio_consumer == k[$clog2(NUM_LSUS)-1:0]) ? mmio_write_ready : 1'b0;
            end else begin
                core_write_ready[k] = ext_write_ready[k];
            end
        end
    end

endmodule
