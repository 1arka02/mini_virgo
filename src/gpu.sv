`default_nettype none
`timescale 1ns/1ns

`include "common.sv"

module gpu #(
    parameter int DATA_MEM_NUM_CHANNELS /*verilator public*/ = 8,        // Number of concurrent channels for sending requests to data memory
    parameter int INSTRUCTION_MEM_NUM_CHANNELS /*verilator public*/ = 8, // Number of concurrent channels for sending requests to data memory
    parameter int NUM_CLUSTERS /*verilator public*/ = 1,                 // Number of Virgo clusters (SMs)
    parameter int CORES_PER_CLUSTER /*verilator public*/ = 2,            // Number of SIMT sub-cores per cluster
    parameter int NUM_CORES /*verilator public*/ = NUM_CLUSTERS * CORES_PER_CLUSTER, // Total number of cores
    parameter int WARPS_PER_CORE /*verilator public*/ = 2,               // Number of warps in each core
    parameter int THREADS_PER_WARP /*verilator public*/ = 32,            // Number of threads per warp (max 32)
    parameter int SHARED_MEM_WORDS /*verilator public*/ = 1024           // Cluster shared memory words (4KB)
) (
    input wire clk,
    input wire reset,

    input wire execution_start,
    output wire execution_done,

    // kernel configuration
    input kernel_config_t kernel_config,

    // Program Memory
    output wire [INSTRUCTION_MEM_NUM_CHANNELS-1:0] instruction_mem_read_valid,
    output instruction_memory_address_t instruction_mem_read_address [INSTRUCTION_MEM_NUM_CHANNELS],
    input wire [INSTRUCTION_MEM_NUM_CHANNELS-1:0] instruction_mem_read_ready,
    input instruction_t instruction_mem_read_data [INSTRUCTION_MEM_NUM_CHANNELS],

    // Data Memory
    output wire [DATA_MEM_NUM_CHANNELS-1:0] data_mem_read_valid,
    output data_memory_address_t data_mem_read_address [DATA_MEM_NUM_CHANNELS],
    input wire [DATA_MEM_NUM_CHANNELS-1:0] data_mem_read_ready,
    input data_memory_address_t data_mem_read_data [DATA_MEM_NUM_CHANNELS],
    output wire [DATA_MEM_NUM_CHANNELS-1:0] data_mem_write_valid,
    output data_memory_address_t data_mem_write_address [DATA_MEM_NUM_CHANNELS],
    output data_t data_mem_write_data [DATA_MEM_NUM_CHANNELS],
    input wire [DATA_MEM_NUM_CHANNELS-1:0] data_mem_write_ready
);

kernel_config_t kernel_config_reg;
logic start_execution; // EDA: Unimportant hack used because of EDA tooling

// save kernel config on execution start to avoid losing data when the kernel is running
always @(posedge clk) begin
    if (reset) begin
        start_execution <= 0;
    end else if (execution_start && !start_execution) begin
        start_execution <= 1;
        kernel_config_reg <= kernel_config;
        $display("GPU: Kernel configuration:");
        $display("     - Base instruction address: %h", kernel_config.base_instructions_address);
        $display("     - Base data address: %h", kernel_config.base_data_address);
        $display("     - Num %d blocks", kernel_config.num_blocks);
        $display("     - Number of warps per block: %d", kernel_config.num_warps_per_block);
    end
end

logic [NUM_CORES-1:0] core_done;
logic [NUM_CORES-1:0] core_start;
logic [NUM_CORES-1:0] core_reset;
data_t core_block_id [NUM_CORES];

// LSU <> Data Memory Controller Channels
localparam int NUM_LSUS_PER_CORE = THREADS_PER_WARP + 1;
localparam int NUM_LSUS = NUM_CORES * NUM_LSUS_PER_CORE;
typedef logic [NUM_LSUS-1:0] lsu_size_t;
lsu_size_t lsu_read_valid;
lsu_size_t lsu_read_ready;
lsu_size_t lsu_write_valid;
lsu_size_t lsu_write_ready;
data_memory_address_t lsu_read_address [NUM_LSUS];
data_memory_address_t lsu_write_address [NUM_LSUS];
data_t lsu_read_data [NUM_LSUS];
data_t lsu_write_data [NUM_LSUS];

// Fetcher <> Program Memory Controller Channels
localparam NUM_FETCHERS = NUM_CORES * WARPS_PER_CORE;
typedef logic [NUM_FETCHERS-1:0] fetcher_size_t;
fetcher_size_t fetcher_read_valid;
instruction_memory_address_t fetcher_read_address [NUM_FETCHERS];
fetcher_size_t fetcher_read_ready;
instruction_t fetcher_read_data [NUM_FETCHERS];

dispatcher #(
    .NUM_CORES(NUM_CORES)
    ) dispatcher_inst (
    .clk(clk),
    .reset(reset),
    .start(start_execution),

    .kernel_config(kernel_config_reg),

    .core_done(core_done),
    .core_start(core_start),
    .core_reset(core_reset),
    .core_block_id(core_block_id),

    .done(execution_done)
);

// Data Memory Controller
mem_controller #(
    .DATA_WIDTH(`DATA_WIDTH),
    .ADDRESS_WIDTH(`DATA_MEMORY_ADDRESS_WIDTH),
    .NUM_CONSUMERS(NUM_LSUS),
    .NUM_CHANNELS(DATA_MEM_NUM_CHANNELS)
    ) data_memory_controller (
        .clk(clk),
        .reset(reset),

        .consumer_read_valid(lsu_read_valid),
        .consumer_read_address(lsu_read_address),
        .consumer_read_ready(lsu_read_ready),
        .consumer_read_data(lsu_read_data),

        .consumer_write_valid(lsu_write_valid),
        .consumer_write_address(lsu_write_address),
        .consumer_write_data(lsu_write_data),
        .consumer_write_ready(lsu_write_ready),

        .mem_read_valid(data_mem_read_valid),
        .mem_read_address(data_mem_read_address),
        .mem_read_ready(data_mem_read_ready),
        .mem_read_data(data_mem_read_data),

        .mem_write_valid(data_mem_write_valid),
        .mem_write_address(data_mem_write_address),
        .mem_write_data(data_mem_write_data),
        .mem_write_ready(data_mem_write_ready)
    );



// Instruction Memory Controller

// Disconnected write wires

logic [NUM_FETCHERS-1:0] d_consumer_write_valid;
instruction_memory_address_t d_consumer_write_address [NUM_FETCHERS];
instruction_t d_consumer_write_data [NUM_FETCHERS];
logic [NUM_FETCHERS-1:0] d_consumer_write_ready;

logic [INSTRUCTION_MEM_NUM_CHANNELS-1:0] d_mem_write_valid;
logic [`INSTRUCTION_MEMORY_ADDRESS_WIDTH-1:0] d_mem_write_address [INSTRUCTION_MEM_NUM_CHANNELS];
logic [`INSTRUCTION_WIDTH-1:0] d_mem_write_data [INSTRUCTION_MEM_NUM_CHANNELS];

mem_controller #(
    .DATA_WIDTH(`INSTRUCTION_WIDTH),
    .ADDRESS_WIDTH(`INSTRUCTION_MEMORY_ADDRESS_WIDTH),
    .NUM_CONSUMERS(NUM_FETCHERS),
    .NUM_CHANNELS(INSTRUCTION_MEM_NUM_CHANNELS),
    .WRITE_ENABLE(0)
) program_memory_controller (
    .clk(clk),
    .reset(reset),

    .consumer_read_valid(fetcher_read_valid),
    .consumer_read_address(fetcher_read_address),
    .consumer_read_ready(fetcher_read_ready),
    .consumer_read_data(fetcher_read_data),

    .consumer_write_valid(d_consumer_write_valid),
    .consumer_write_address(d_consumer_write_address),
    .consumer_write_data(d_consumer_write_data),
    .consumer_write_ready(d_consumer_write_ready),

    .mem_read_valid(instruction_mem_read_valid),
    .mem_read_address(instruction_mem_read_address),
    .mem_read_ready(instruction_mem_read_ready),
    .mem_read_data(instruction_mem_read_data),

    .mem_write_valid(d_mem_write_valid),
    .mem_write_address(d_mem_write_address),
    .mem_write_data(d_mem_write_data),
    .mem_write_ready(0)
);

initial begin
    $display("Hello, World!");
end

// Virgo Clusters (Streaming Multiprocessors)
generate
    for (genvar cl = 0; cl < NUM_CLUSTERS; cl = cl + 1) begin : g_clusters
        localparam int CLUSTER_CORES = CORES_PER_CLUSTER;
        localparam int CLUSTER_NUM_LSUS = CLUSTER_CORES * NUM_LSUS_PER_CORE;
        localparam int CLUSTER_NUM_FETCHERS = CLUSTER_CORES * WARPS_PER_CORE;
        localparam int core_idx_base = cl * CLUSTER_CORES;
        localparam int lsu_base = cl * CLUSTER_NUM_LSUS;
        localparam int fetcher_base = cl * CLUSTER_NUM_FETCHERS;

        logic [CLUSTER_NUM_LSUS-1:0] cluster_lsu_read_valid;
        data_memory_address_t cluster_lsu_read_address [CLUSTER_NUM_LSUS];
        logic [CLUSTER_NUM_LSUS-1:0] cluster_lsu_read_ready;
        data_t cluster_lsu_read_data [CLUSTER_NUM_LSUS];
        logic [CLUSTER_NUM_LSUS-1:0] cluster_lsu_write_valid;
        data_memory_address_t cluster_lsu_write_address [CLUSTER_NUM_LSUS];
        data_t cluster_lsu_write_data [CLUSTER_NUM_LSUS];
        logic [CLUSTER_NUM_LSUS-1:0] cluster_lsu_write_ready;

        // Pass through signals between cluster LSUs and top-level data memory controller
        genvar j;
        for (j = 0; j < CLUSTER_NUM_LSUS; j = j + 1) begin : g_lsu_connect
            localparam lsu_index = lsu_base + j;
            always @(posedge clk) begin
                lsu_read_valid[lsu_index] <= cluster_lsu_read_valid[j];
                lsu_read_address[lsu_index] <= cluster_lsu_read_address[j];

                lsu_write_valid[lsu_index] <= cluster_lsu_write_valid[j];
                lsu_write_address[lsu_index] <= cluster_lsu_write_address[j];
                lsu_write_data[lsu_index] <= cluster_lsu_write_data[j];

                cluster_lsu_read_ready[j] <= lsu_read_ready[lsu_index];
                cluster_lsu_read_data[j] <= lsu_read_data[lsu_index];
                cluster_lsu_write_ready[j] <= lsu_write_ready[lsu_index];
            end
        end

        // Unpacked array wiring for block IDs
        data_t cluster_core_block_id [CLUSTER_CORES];
        genvar k;
        for (k = 0; k < CLUSTER_CORES; k = k + 1) begin : g_block_ids
            assign cluster_core_block_id[k] = core_block_id[core_idx_base + k];
        end

        // Unpacked array wiring for fetcher addresses and data
        instruction_memory_address_t cluster_fetch_addr [CLUSTER_NUM_FETCHERS];
        instruction_t cluster_fetch_data [CLUSTER_NUM_FETCHERS];
        for (k = 0; k < CLUSTER_NUM_FETCHERS; k = k + 1) begin : g_fetch_arrays
            assign fetcher_read_address[fetcher_base + k] = cluster_fetch_addr[k];
            assign cluster_fetch_data[k] = fetcher_read_data[fetcher_base + k];
        end

        virgo_cluster #(
            .CLUSTER_ID(cl),
            .CORES_PER_CLUSTER(CORES_PER_CLUSTER),
            .WARPS_PER_CORE(WARPS_PER_CORE),
            .THREADS_PER_WARP(THREADS_PER_WARP),
            .SHARED_MEM_WORDS(SHARED_MEM_WORDS)
        ) cluster_instance (
            .clk(clk),
            .reset(core_reset[core_idx_base]), // Reset cluster with core reset

            .core_start(core_start[core_idx_base +: CLUSTER_CORES]),
            .core_reset(core_reset[core_idx_base +: CLUSTER_CORES]),
            .core_block_id(cluster_core_block_id),
            .core_done(core_done[core_idx_base +: CLUSTER_CORES]),

            .kernel_config(kernel_config_reg),

            .instruction_mem_read_valid(fetcher_read_valid[fetcher_base +: CLUSTER_NUM_FETCHERS]),
            .instruction_mem_read_address(cluster_fetch_addr),
            .instruction_mem_read_ready(fetcher_read_ready[fetcher_base +: CLUSTER_NUM_FETCHERS]),
            .instruction_mem_read_data(cluster_fetch_data),

            .data_mem_read_valid(cluster_lsu_read_valid),
            .data_mem_read_address(cluster_lsu_read_address),
            .data_mem_read_ready(cluster_lsu_read_ready),
            .data_mem_read_data(cluster_lsu_read_data),
            .data_mem_write_valid(cluster_lsu_write_valid),
            .data_mem_write_address(cluster_lsu_write_address),
            .data_mem_write_data(cluster_lsu_write_data),
            .data_mem_write_ready(cluster_lsu_write_ready)
        );
    end
endgenerate


endmodule
