`default_nettype none
`timescale 1ns/1ns

`include "common.sv"

module alu (
    input wire clk,
    input wire reset,
    input wire enable,

    input instruction_memory_address_t pc,
    input data_t imm,
    input data_t rs1,
    input data_t rs2,
    input alu_instruction_t instruction,

    output data_t alu_out
);

always @(posedge clk) begin
    if (reset) begin
        alu_out <= 0;
    end else if (enable) begin
        case (instruction)
            ADDI: begin
                alu_out <= rs1 + imm;
            end
            SLTI: begin
                alu_out <= (rs1 < imm) ? 1 : 0;
            end
            XORI: begin
                alu_out <= rs1 ^ imm;
            end
            ORI: begin
                alu_out <= rs1 | imm;
            end
            ANDI: begin
                alu_out <= rs1 & imm;
            end
            SLLI: begin
                alu_out <= rs1 << imm;
            end
            SRLI: begin
                alu_out <= rs1 >> imm;
            end
            SRAI: begin
                alu_out <= rs1 >>> imm;
            end
            ADD: begin
                alu_out <= rs1 + rs2;
            end
            SUB: begin
                alu_out <= rs1 - rs2;
            end
            SLL: begin
                alu_out <= rs1 << rs2;
            end
            SLT: begin
                alu_out <= (rs1 < rs2) ? 1 : 0;
            end
            XOR: begin
                alu_out <= rs1 ^ rs2;
            end
            SRL: begin
                alu_out <= rs1 >> rs2;
            end
            SRA: begin
                alu_out <= rs1 >>> rs2;
            end
            OR: begin
                alu_out <= rs1 | rs2;
            end
            AND: begin
                alu_out <= rs1 & rs2;
            end
            BEQ: begin
                alu_out <= (rs1 == rs2) ? 1 : 0;
            end
            BNE: begin
                alu_out <= (rs1 != rs2) ? 1 : 0;
            end
            BLT: begin
                alu_out <= (rs1 < rs2) ? 1 : 0;
            end
            BGE: begin
                alu_out <= (rs1 >= rs2) ? 1 : 0;
            end
            JAL: begin
                alu_out <= pc + imm;
            end
            JALR: begin
                alu_out <= rs1 + imm;
            end
            default: begin
                $error("Invalid ALU instruction %0d", instruction);
                alu_out <= 0;
            end
        endcase
    end
end

endmodule
