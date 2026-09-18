`default_nettype none
`timescale 1ns/1ns

`include "common.sv"

module fetcher (
    input wire clk,
    input wire reset,

    input warp_state_t warp_state,
    input instruction_memory_address_t pc,

    // Instruction Memory
    input logic instruction_mem_read_ready,
    input instruction_t instruction_mem_read_data,
    output logic instruction_mem_read_valid,
    output instruction_memory_address_t instruction_mem_read_address,

    // Fetcher output
    output fetcher_state_t fetcher_state,
    output instruction_t instruction
);

always @(posedge clk) begin
    if (reset) begin
        fetcher_state <= FETCHER_IDLE;
        instruction_mem_read_valid <= 0;
        instruction_mem_read_address <= 0;
        instruction <= {`INSTRUCTION_WIDTH{1'b0}};
    end else begin
        case (fetcher_state)
            FETCHER_IDLE: begin
                if (warp_state == WARP_FETCH) begin
                    fetcher_state <= FETCHER_FETCHING;
                    instruction_mem_read_valid <= 1;
                    instruction_mem_read_address <= pc;
                end
            end
            FETCHER_FETCHING: begin
                if (instruction_mem_read_ready) begin
                    fetcher_state <= FETCHER_DONE;
                    instruction_mem_read_valid <= 0;
                    instruction <= instruction_mem_read_data;
                end
            end
            FETCHER_DONE: begin
                if (warp_state == WARP_DECODE) begin
                    fetcher_state <= FETCHER_IDLE;
                end
            end
            default: begin
                $error("Invalid fetcher state");
            end
        endcase
    end
end

endmodule
