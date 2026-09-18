`default_nettype none
`timescale 1ns/1ns

`include "common.sv"

// Virgo Cluster Synchronizer
// Coordinates synchronization and barrier operations between SIMT sub-cores
// and the disaggregated Cluster Matrix Unit.
module cluster_sync #(
    parameter int CORES_PER_CLUSTER = 2,
    parameter int WARPS_PER_CORE = 2
) (
    input wire clk,
    input wire reset,

    // Matrix Unit status
    input wire mu_busy,
    input wire mu_done,

    // Cores done/active status
    input wire [CORES_PER_CLUSTER-1:0] core_done,

    // Synchronization outputs
    output logic cluster_all_cores_done,
    output logic cluster_matrix_ready
);

    assign cluster_all_cores_done = &core_done;
    assign cluster_matrix_ready   = !mu_busy;

endmodule
