`default_nettype none
`timescale 1ns/1ns

`include "common.sv"

module dispatcher #(
    parameter int NUM_CORES                 // Number of cores to include in this GPU
) (
    input wire clk,
    input wire reset,
    input wire start,

    // Kernel Metadata
    input kernel_config_t kernel_config,

    // Core States
    input reg [NUM_CORES-1:0] core_done,
    output reg [NUM_CORES-1:0] core_start,
    output reg [NUM_CORES-1:0] core_reset,
    output data_t core_block_id [NUM_CORES],

    // Kernel Execution
    output reg done
);

wire data_t total_blocks = kernel_config.num_blocks;

data_t blocks_done;
data_t blocks_dispatched; // How many blocks have been sent to cores?

logic start_execution; // EDA: Unimportant hack used because of EDA tooling

always @(posedge clk) begin
    if (reset) begin
        done <= 0;
        blocks_dispatched = 0;
        blocks_done <= 0;
        start_execution <= 0;

        for (int i = 0; i < NUM_CORES; i++) begin
            core_start[i] <= 0;
            core_reset[i] <= 1;
            core_block_id[i] <= 0;
        end
    end else if (start) begin
        // EDA: Indirect way to get @(posedge start) without driving from 2 different clocks
        if (!start_execution) begin
            $display("Dispatcher: Start execution of %0d block(s)", total_blocks);
            start_execution <= 1;
            for (int i = 0; i < NUM_CORES; i++) begin
                core_reset[i] <= 1;
            end
        end

        // If the last block has finished processing, mark this kernel as done executing
        if (blocks_done == total_blocks) begin
            $display("Dispatcher: Done execution");
            done <= 1;
        end

        for (int i = 0; i < NUM_CORES; i++) begin
            if (core_reset[i]) begin
                core_reset[i] <= 0;

                // If this core was just reset, check if there are more blocks to be dispatched
                if (blocks_dispatched < total_blocks) begin
                    $display("Dispatcher: Dispatching block %d to core %d", blocks_dispatched, i);
                    core_start[i] <= 1;
                    core_block_id[i] <= blocks_dispatched;

                    blocks_dispatched = blocks_dispatched + 1;
                end
            end
        end

        for (int i = 0; i < NUM_CORES; i++) begin
            if (core_start[i] && core_done[i]) begin
                // If a core just finished executing it's current block, reset it
                $display("Dispatcher: Core %d finished block %d", i, core_block_id[i]);
                core_reset[i] <= 1;
                core_start[i] <= 0;
                blocks_done <= blocks_done + 1;
            end
        end
    end
end
endmodule
