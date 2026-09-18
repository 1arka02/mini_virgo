`default_nettype none
`timescale 1ns/1ns

// LOAD-STORE UNIT
// > Handles asynchronous memory load and store operations and waits for response
// > Each thread in each core has it's own LSU
// > LDR, STR instructions are executed here
module lsu (
    input wire clk,
    input wire reset,
    input wire enable, // If current block has less threads then block size, some LSUs will be inactive

    // State
    input warp_state_t warp_state,

    // Memory Control Signals
    input reg decoded_mem_read_enable,
    input reg decoded_mem_write_enable,

    // Registers
    input data_t rs1,
    input data_t rs2,
    input data_t imm,

    // Data Memory
    output logic mem_read_valid,
    output data_memory_address_t mem_read_address,
    input logic mem_read_ready,
    input data_t mem_read_data,
    output logic mem_write_valid,
    output data_memory_address_t mem_write_address,
    output data_t mem_write_data,
    input logic mem_write_ready,

    // LSU Outputs
    output lsu_state_t lsu_state,
    output data_t lsu_out
);

data_t offset_address;
assign offset_address = rs1 + imm;

always @(posedge clk) begin
    if (reset) begin
        lsu_state <= LSU_IDLE;
        lsu_out <= 0;
        mem_read_valid <= 0;
        mem_read_address <= 0;
        mem_write_valid <= 0;
        mem_write_address <= 0;
        mem_write_data <= 0;
    end else if (enable) begin
        // If memory read enable is triggered (LDR instruction)
        if (decoded_mem_read_enable) begin 
            case (lsu_state)
                LSU_IDLE: begin
                    // Only read when warp_state = REQUEST
                    if (warp_state == WARP_REQUEST) begin 
                        lsu_state <= LSU_REQUESTING;
                    end
                end
                LSU_REQUESTING: begin 
                    mem_read_valid <= 1;
                    mem_read_address <= offset_address;
                    lsu_state <= LSU_WAITING;
                end
                LSU_WAITING: begin
                    if (mem_read_ready == 1) begin
                        //$display("LSU: Reading %d from memory address %d", mem_read_data, rs1);
                        mem_read_valid <= 0;
                        lsu_out <= mem_read_data;
                        lsu_state <= LSU_DONE;
                    end
                end
                LSU_DONE: begin 
                    // Reset when warp_state = UPDATE
                    if (warp_state == WARP_UPDATE) begin 
                        lsu_state <= LSU_IDLE;
                    end
                end
            endcase
        end

        // If memory write enable is triggered (STR instruction)
        if (decoded_mem_write_enable) begin 
            case (lsu_state)
                LSU_IDLE: begin
                    // Only read when warp_state = REQUEST
                    if (warp_state == WARP_REQUEST) begin 
                        lsu_state <= LSU_REQUESTING;
                    end
                end
                LSU_REQUESTING: begin 
                    $display("LSU: Writing %d to memory address %d", rs2, rs1);
                    mem_write_valid <= 1;
                    mem_write_address <= offset_address;
                    mem_write_data <= rs2;
                    lsu_state <= LSU_WAITING;
                end
                LSU_WAITING: begin
                    if (mem_write_ready) begin
                        mem_write_valid <= 0;
                        lsu_state <= LSU_DONE;
                    end
                end
                LSU_DONE: begin 
                    // Reset when warp_state = UPDATE
                    if (warp_state == WARP_UPDATE) begin 
                        lsu_state <= LSU_IDLE;
                    end
                end
            endcase
        end
    end
end
endmodule
