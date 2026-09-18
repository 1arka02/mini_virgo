`default_nettype none
`timescale 1ns/1ns

`include "common.sv"

// Virgo Cluster (Streaming Multiprocessor)
// Integrates:
// - CORES_PER_CLUSTER SIMT sub-cores (compute_core)
// - Dedicated Cluster-Level Matrix Unit (matrix_unit)
// - Cluster Shared Memory (shared_memory)
// - MMIO Matrix Controller (matrix_controller)
// - Cluster Interconnect (cluster_interconnect)
// - Cluster Synchronizer (cluster_sync)
module virgo_cluster #(
    parameter int CLUSTER_ID = 0,
    parameter int CORES_PER_CLUSTER = 2,
    parameter int WARPS_PER_CORE = 2,
    parameter int THREADS_PER_WARP = 32,
    parameter int SHARED_MEM_WORDS = 1024
) (
    input wire clk,
    input wire reset,

    // Cluster Execution Controls (per-core signals from Dispatcher)
    input wire  [CORES_PER_CLUSTER-1:0] core_start,
    input wire  [CORES_PER_CLUSTER-1:0] core_reset,
    input data_t                        core_block_id [CORES_PER_CLUSTER],
    output wire [CORES_PER_CLUSTER-1:0] core_done,

    input kernel_config_t kernel_config,

    // Instruction Memory Channels (to Program Memory Controller)
    output wire  [TOTAL_FETCHERS-1:0] instruction_mem_read_valid,
    output instruction_memory_address_t instruction_mem_read_address [TOTAL_FETCHERS],
    input wire   [TOTAL_FETCHERS-1:0] instruction_mem_read_ready,
    input instruction_t               instruction_mem_read_data [TOTAL_FETCHERS],

    // Global Data Memory Channels (to Data Memory Controller)
    output wire  [TOTAL_LSUS-1:0] data_mem_read_valid,
    output data_memory_address_t  data_mem_read_address [TOTAL_LSUS],
    input wire   [TOTAL_LSUS-1:0] data_mem_read_ready,
    input data_t                  data_mem_read_data [TOTAL_LSUS],

    output wire  [TOTAL_LSUS-1:0] data_mem_write_valid,
    output data_memory_address_t  data_mem_write_address [TOTAL_LSUS],
    output data_t                 data_mem_write_data [TOTAL_LSUS],
    input wire   [TOTAL_LSUS-1:0] data_mem_write_ready
);

    localparam int NUM_LSUS_PER_CORE = THREADS_PER_WARP + 1;
    localparam int TOTAL_LSUS = CORES_PER_CLUSTER * NUM_LSUS_PER_CORE;
    localparam int TOTAL_FETCHERS = CORES_PER_CLUSTER * WARPS_PER_CORE;

    // Internal LSU signals from SIMT cores to Cluster Interconnect
    wire  [TOTAL_LSUS-1:0]    int_lsu_read_valid;
    wire  [TOTAL_LSUS-1:0]    int_lsu_read_ready;
    wire  [TOTAL_LSUS-1:0]    int_lsu_write_valid;
    wire  [TOTAL_LSUS-1:0]    int_lsu_write_ready;
    data_memory_address_t     int_lsu_read_address [TOTAL_LSUS];
    data_memory_address_t     int_lsu_write_address [TOTAL_LSUS];
    data_t                    int_lsu_read_data [TOTAL_LSUS];
    data_t                    int_lsu_write_data [TOTAL_LSUS];

    // Shared Memory Port A (Interconnect)
    wire                      sm_a_read_valid;
    data_memory_address_t     sm_a_read_address;
    wire                      sm_a_read_ready;
    data_t                    sm_a_read_data;

    wire                      sm_a_write_valid;
    data_memory_address_t     sm_a_write_address;
    data_t                    sm_a_write_data;
    wire                      sm_a_write_ready;

    // Shared Memory Port B (Matrix Unit)
    wire                      sm_b_read_valid;
    data_memory_address_t     sm_b_read_address;
    wire                      sm_b_read_ready;
    data_t                    sm_b_read_data;

    wire                      sm_b_write_valid;
    data_memory_address_t     sm_b_write_address;
    data_t                    sm_b_write_data;
    wire                      sm_b_write_ready;

    // MMIO Interface (Interconnect to Matrix Controller)
    wire                      mmio_read_valid;
    data_memory_address_t     mmio_read_address;
    wire                      mmio_read_ready;
    data_t                    mmio_read_data;

    wire                      mmio_write_valid;
    data_memory_address_t     mmio_write_address;
    data_t                    mmio_write_data;
    wire                      mmio_write_ready;

    // Matrix Unit Control & Status Signals
    wire                      mu_start;
    wire                      mu_accumulate;
    wire                      mu_clear_acc;
    wire [3:0]                mu_dim_m;
    wire [3:0]                mu_dim_n;
    wire [3:0]                mu_dim_k;
    data_memory_address_t     mu_addr_a;
    data_memory_address_t     mu_addr_b;
    data_memory_address_t     mu_addr_c;

    wire                      mu_busy;
    wire                      mu_done;
    data_t                    mu_cycle_count;

    // Synchronizer signals
    wire                      cluster_all_done;
    wire                      cluster_matrix_ready;

    // -------------------------------------------------------------
    // 1. SIMT Sub-Cores Instantiation
    // -------------------------------------------------------------
    genvar c;
    generate
        for (c = 0; c < CORES_PER_CLUSTER; c = c + 1) begin : g_cores
            localparam int core_lsu_offset = c * NUM_LSUS_PER_CORE;
            localparam int core_fetcher_offset = c * WARPS_PER_CORE;

            compute_core #(
                .WARPS_PER_CORE(WARPS_PER_CORE),
                .THREADS_PER_WARP(THREADS_PER_WARP)
            ) core_inst (
                .clk(clk),
                .reset(core_reset[c]),

                .start(core_start[c]),
                .done(core_done[c]),

                .block_id(core_block_id[c]),
                .kernel_config(kernel_config),

                // Instruction memory connections
                .instruction_mem_read_valid(instruction_mem_read_valid[core_fetcher_offset +: WARPS_PER_CORE]),
                .instruction_mem_read_address(instruction_mem_read_address[core_fetcher_offset +: WARPS_PER_CORE]),
                .instruction_mem_read_ready(instruction_mem_read_ready[core_fetcher_offset +: WARPS_PER_CORE]),
                .instruction_mem_read_data(instruction_mem_read_data[core_fetcher_offset +: WARPS_PER_CORE]),

                // Internal LSU connections to cluster interconnect
                .data_mem_read_valid(int_lsu_read_valid[core_lsu_offset +: NUM_LSUS_PER_CORE]),
                .data_mem_read_address(int_lsu_read_address[core_lsu_offset +: NUM_LSUS_PER_CORE]),
                .data_mem_read_ready(int_lsu_read_ready[core_lsu_offset +: NUM_LSUS_PER_CORE]),
                .data_mem_read_data(int_lsu_read_data[core_lsu_offset +: NUM_LSUS_PER_CORE]),

                .data_mem_write_valid(int_lsu_write_valid[core_lsu_offset +: NUM_LSUS_PER_CORE]),
                .data_mem_write_address(int_lsu_write_address[core_lsu_offset +: NUM_LSUS_PER_CORE]),
                .data_mem_write_data(int_lsu_write_data[core_lsu_offset +: NUM_LSUS_PER_CORE]),
                .data_mem_write_ready(int_lsu_write_ready[core_lsu_offset +: NUM_LSUS_PER_CORE])
            );
        end
    endgenerate

    // -------------------------------------------------------------
    // 2. Cluster Interconnect
    // -------------------------------------------------------------
    cluster_interconnect #(
        .NUM_LSUS(TOTAL_LSUS),
        .ADDR_WIDTH(`DATA_MEMORY_ADDRESS_WIDTH),
        .DATA_WIDTH(`DATA_WIDTH)
    ) interconnect_inst (
        .clk(clk),
        .reset(reset),

        // Core side
        .core_read_valid(int_lsu_read_valid),
        .core_read_address(int_lsu_read_address),
        .core_read_ready(int_lsu_read_ready),
        .core_read_data(int_lsu_read_data),

        .core_write_valid(int_lsu_write_valid),
        .core_write_address(int_lsu_write_address),
        .core_write_data(int_lsu_write_data),
        .core_write_ready(int_lsu_write_ready),

        // External global memory side
        .ext_read_valid(data_mem_read_valid),
        .ext_read_address(data_mem_read_address),
        .ext_read_ready(data_mem_read_ready),
        .ext_read_data(data_mem_read_data),

        .ext_write_valid(data_mem_write_valid),
        .ext_write_address(data_mem_write_address),
        .ext_write_data(data_mem_write_data),
        .ext_write_ready(data_mem_write_ready),

        // Shared memory Port A
        .sm_read_valid(sm_a_read_valid),
        .sm_read_address(sm_a_read_address),
        .sm_read_ready(sm_a_read_ready),
        .sm_read_data(sm_a_read_data),

        .sm_write_valid(sm_a_write_valid),
        .sm_write_address(sm_a_write_address),
        .sm_write_data(sm_a_write_data),
        .sm_write_ready(sm_a_write_ready),

        // MMIO side
        .mmio_read_valid(mmio_read_valid),
        .mmio_read_address(mmio_read_address),
        .mmio_read_ready(mmio_read_ready),
        .mmio_read_data(mmio_read_data),

        .mmio_write_valid(mmio_write_valid),
        .mmio_write_address(mmio_write_address),
        .mmio_write_data(mmio_write_data),
        .mmio_write_ready(mmio_write_ready)
    );

    // -------------------------------------------------------------
    // 3. Cluster Shared Memory
    // -------------------------------------------------------------
    shared_memory #(
        .MEM_WORDS(SHARED_MEM_WORDS),
        .ADDR_WIDTH(`DATA_MEMORY_ADDRESS_WIDTH),
        .DATA_WIDTH(`DATA_WIDTH)
    ) shared_mem_inst (
        .clk(clk),
        .reset(reset),

        // Port A: Cores via Interconnect
        .core_read_valid(sm_a_read_valid),
        .core_read_address(sm_a_read_address),
        .core_read_ready(sm_a_read_ready),
        .core_read_data(sm_a_read_data),

        .core_write_valid(sm_a_write_valid),
        .core_write_address(sm_a_write_address),
        .core_write_data(sm_a_write_data),
        .core_write_ready(sm_a_write_ready),

        // Port B: Dedicated Matrix Unit
        .mu_read_valid(sm_b_read_valid),
        .mu_read_address(sm_b_read_address),
        .mu_read_ready(sm_b_read_ready),
        .mu_read_data(sm_b_read_data),

        .mu_write_valid(sm_b_write_valid),
        .mu_write_address(sm_b_write_address),
        .mu_write_data(sm_b_write_data),
        .mu_write_ready(sm_b_write_ready)
    );

    // -------------------------------------------------------------
    // 4. Asynchronous MMIO Matrix Controller
    // -------------------------------------------------------------
    matrix_controller #(
        .ADDR_WIDTH(`DATA_MEMORY_ADDRESS_WIDTH),
        .DATA_WIDTH(`DATA_WIDTH)
    ) matrix_ctrl_inst (
        .clk(clk),
        .reset(reset),

        .mmio_read_valid(mmio_read_valid),
        .mmio_read_address(mmio_read_address),
        .mmio_read_ready(mmio_read_ready),
        .mmio_read_data(mmio_read_data),

        .mmio_write_valid(mmio_write_valid),
        .mmio_write_address(mmio_write_address),
        .mmio_write_data(mmio_write_data),
        .mmio_write_ready(mmio_write_ready),

        .mu_start(mu_start),
        .mu_accumulate(mu_accumulate),
        .mu_clear_acc(mu_clear_acc),
        .mu_dim_m(mu_dim_m),
        .mu_dim_n(mu_dim_n),
        .mu_dim_k(mu_dim_k),
        .mu_addr_a(mu_addr_a),
        .mu_addr_b(mu_addr_b),
        .mu_addr_c(mu_addr_c),

        .mu_busy(mu_busy),
        .mu_done(mu_done),
        .mu_cycle_count(mu_cycle_count)
    );

    // -------------------------------------------------------------
    // 5. Dedicated Cluster Matrix Unit (Systolic GEMM Tile Engine)
    // -------------------------------------------------------------
    matrix_unit #(
        .MAX_DIM(8),
        .ADDR_WIDTH(`DATA_MEMORY_ADDRESS_WIDTH),
        .DATA_WIDTH(`DATA_WIDTH)
    ) matrix_unit_inst (
        .clk(clk),
        .reset(reset),

        .start(mu_start),
        .accumulate(mu_accumulate),
        .clear_acc(mu_clear_acc),
        .dim_m(mu_dim_m),
        .dim_n(mu_dim_n),
        .dim_k(mu_dim_k),
        .addr_a(mu_addr_a),
        .addr_b(mu_addr_b),
        .addr_c(mu_addr_c),

        .busy(mu_busy),
        .done(mu_done),
        .cycle_count(mu_cycle_count),

        // Direct Shared Memory interface (Port B)
        .mem_read_valid(sm_b_read_valid),
        .mem_read_address(sm_b_read_address),
        .mem_read_ready(sm_b_read_ready),
        .mem_read_data(sm_b_read_data),

        .mem_write_valid(sm_b_write_valid),
        .mem_write_address(sm_b_write_address),
        .mem_write_data(sm_b_write_data),
        .mem_write_ready(sm_b_write_ready)
    );

    // -------------------------------------------------------------
    // 6. Cluster Synchronizer
    // -------------------------------------------------------------
    cluster_sync #(
        .CORES_PER_CLUSTER(CORES_PER_CLUSTER),
        .WARPS_PER_CORE(WARPS_PER_CORE)
    ) cluster_sync_inst (
        .clk(clk),
        .reset(reset),

        .mu_busy(mu_busy),
        .mu_done(mu_done),
        .core_done(core_done),

        .cluster_all_cores_done(cluster_all_done),
        .cluster_matrix_ready(cluster_matrix_ready)
    );

endmodule
