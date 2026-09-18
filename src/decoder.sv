`default_nettype none
`timescale 1ns/1ns

`include "common.sv"

module decoder(
    input wire clk,
    input wire reset,
    input warp_state_t warp_state,

    input instruction_t instruction,

    output reg decoded_reg_write_enable,
    output reg decoded_mem_write_enable,
    output reg decoded_mem_read_enable,
    output reg decoded_branch,
    output reg_input_mux_t decoded_reg_input_mux,
    output reg decoded_scalar_instruction,
    output data_t decoded_immediate,
    output reg [4:0] decoded_rd_address,
    output reg [4:0] decoded_rs1_address,
    output reg [4:0] decoded_rs2_address,
    output alu_instruction_t decoded_alu_instruction,

    output reg decoded_halt
);
    // Extract fields from instruction
    wire         scalar = instruction[6];
    wire [5:0]   inst   = instruction[5:0];
    wire [6:0]   opcode = instruction[6:0];
    wire [4:0]   rd     = instruction[11:7];
    wire [2:0]   funct3 = instruction[14:12];
    wire [4:0]   rs1    = instruction[19:15];
    wire [4:0]   rs2    = instruction[24:20];
    wire [6:0]   funct7 = instruction[31:25];
    wire [11:0]  imm_i  = instruction[31:20];
    wire [11:0]  imm_s  = {instruction[31:25], instruction[11:7]};
    wire [12:0]  imm_b  = {instruction[31], instruction[7], instruction[30:25], instruction[11:8], 1'b0};
    wire [31:12] imm_u  = instruction[31:12];
    wire [20:0]  imm_j  = {instruction[31], instruction[19:12], instruction[20], instruction[30:21], 1'b0};

    always @(posedge clk) begin
        if (reset) begin
            // Set outputs to default values
            decoded_reg_write_enable <= 0;
            decoded_mem_write_enable <= 0;
            decoded_mem_read_enable <= 0;
            decoded_branch <= 0;
            decoded_reg_input_mux <= ALU_OUT;
            decoded_immediate <= {`DATA_WIDTH{1'b0}};
            decoded_rd_address <= 5'b0;
            decoded_rs1_address <= 5'b0;
            decoded_rs2_address <= 5'b0;
            decoded_alu_instruction <= ADDI;
            decoded_halt <= 0;
            decoded_scalar_instruction <= 0;
        end else if (warp_state == WARP_DECODE) begin
            // Default assignments for new decode
            decoded_reg_write_enable <= 0;
            decoded_reg_input_mux <= ALU_OUT;
            decoded_immediate <= {`DATA_WIDTH{1'b0}};
            decoded_rd_address <= 5'b0;
            decoded_rs1_address <= 5'b0;
            decoded_rs2_address <= 5'b0;
            decoded_alu_instruction <= ADDI;
            decoded_mem_read_enable <= 0;
            decoded_mem_write_enable <= 0;
            decoded_branch <= 0;
            decoded_halt <= 0;
            decoded_scalar_instruction <= scalar;

            if (opcode == `OPCODE_HALT) begin
                decoded_halt <= 1;
            end else if (opcode == `OPCODE_SX_SLT) begin
                decoded_scalar_instruction <= 0; // This is a vector-scalar instruction
                decoded_rd_address <= rd;
                decoded_rs1_address <= rs1;
                decoded_rs2_address <= rs2;
                decoded_reg_write_enable <= 1;
                decoded_reg_input_mux <= VECTOR_TO_SCALAR;
                decoded_alu_instruction <= SLT;
            end else if (opcode == `OPCODE_SX_SLTI) begin
                decoded_scalar_instruction <= 0; // This is a vector-scalar instruction
                decoded_rd_address <= rd;
                decoded_rs1_address <= rs1;
                decoded_immediate <= sign_extend(imm_i);
                decoded_reg_write_enable <= 1;
                decoded_reg_input_mux <= VECTOR_TO_SCALAR;
                decoded_alu_instruction <= SLTI;                // not implemented yet
        end else if (opcode == `OPCODE_B) begin
                // Branch instructions (e.g., BEQ, BNE)
                decoded_rs1_address <= rs1;
                decoded_rs2_address <= rs2;
                decoded_immediate <= sign_extend_13(imm_b);
                decoded_branch <= 1;

                unique case (funct3)
                    3'b000: decoded_alu_instruction <= BEQ;
                    3'b001: decoded_alu_instruction <= BNE;
                    3'b100: decoded_alu_instruction <= BLT;
                    3'b101: decoded_alu_instruction <= BGE;
                    default: $error("Invalid B-type instruction with funct3 %b", funct3);
                endcase
            end else if (opcode == `OPCODE_J) begin // JAL
                decoded_rd_address <= rd;
                decoded_reg_write_enable <= 1;
                decoded_reg_input_mux <= PC_PLUS_1;
                decoded_scalar_instruction <= 1;
                decoded_alu_instruction <= JAL;
                $display("Decoding instruction 0b%32b", instruction);
                decoded_immediate <= sign_extend_21(imm_j);
            end else if (opcode == `OPCODE_JALR) begin
                // JALR instruction decoding
                decoded_rd_address <= rd;
                decoded_rs1_address <= rs1;
                decoded_reg_write_enable <= 1;
                decoded_reg_input_mux <= PC_PLUS_1;
                decoded_scalar_instruction <= 1;
                decoded_alu_instruction <= JALR;
                decoded_immediate <= sign_extend(imm_i);
            end else begin
                unique case (inst)
                    `OPCODE_R: begin
                        // Vector R-type instructions
                        decoded_rd_address <= rd;
                        decoded_rs1_address <= rs1;
                        decoded_rs2_address <= rs2;
                        decoded_reg_write_enable <= 1;
                        decoded_reg_input_mux <= ALU_OUT;

                        // Determine the ALU instruction
                        unique case (funct3)
                            3'b000: begin
                                if (funct7 == 7'b0000000)
                                    decoded_alu_instruction <= ADD;
                                else if (funct7 == 7'b0100000)
                                    decoded_alu_instruction <= SUB;
                                else
                                    $error("Invalid R-type instruction with funct7 %b", funct7);
                            end
                            3'b001: decoded_alu_instruction <= SLL;
                            3'b010: decoded_alu_instruction <= SLT;
                            3'b100: decoded_alu_instruction <= XOR;
                            3'b101: begin
                                if (funct7 == 7'b0000000)
                                    decoded_alu_instruction <= SRL;
                                else if (funct7 == 7'b0100000)
                                    decoded_alu_instruction <= SRA;
                                else
                                    $error("Invalid R-type instruction with funct7 %b", funct7);
                            end
                            3'b110: decoded_alu_instruction <= OR;
                            3'b111: decoded_alu_instruction <= AND;
                            default: $error("Invalid R-type instruction with funct3 %b", funct3);
                        endcase
                    end
                    `OPCODE_I: begin
                        // Vector I-type instructions
                        decoded_rd_address <= rd;
                        decoded_rs1_address <= rs1;
                        decoded_reg_write_enable <= 1;
                        decoded_reg_input_mux <= ALU_OUT;
                        decoded_immediate <= sign_extend(imm_i);

                        unique case (funct3)
                            3'b000: decoded_alu_instruction <= ADDI;
                            3'b010: decoded_alu_instruction <= SLTI;
                            3'b100: decoded_alu_instruction <= XORI;
                            3'b110: decoded_alu_instruction <= ORI;
                            3'b111: decoded_alu_instruction <= ANDI;
                            3'b001: decoded_alu_instruction <= SLLI;
                            3'b101: begin
                                if (funct7 == 7'b0000000)
                                    decoded_alu_instruction <= SRLI;
                                else if (funct7 == 7'b0100000)
                                    decoded_alu_instruction <= SRAI;
                                else
                                    $error("Invalid I-type shift instruction with funct7 %b", funct7);
                            end
                            default: $error("Invalid I-type instruction with funct3 %b", funct3);
                        endcase
                    end
                    `OPCODE_LOAD: begin
                        // Load instructions (e.g., LW)
                        decoded_rd_address <= rd;
                        decoded_rs1_address <= rs1;
                        decoded_reg_write_enable <= 1;
                        decoded_reg_input_mux <= LSU_OUT;
                        decoded_immediate <= sign_extend(imm_i);
                        decoded_mem_read_enable <= 1;
                        decoded_alu_instruction <= ADDI; // For computing effective address
                    end
                    `OPCODE_S: begin
                        // Store instructions (e.g., SW)
                        decoded_rs1_address <= rs1;
                        decoded_rs2_address <= rs2;
                        decoded_immediate <= sign_extend(imm_s);
                        decoded_mem_write_enable <= 1;
                        decoded_alu_instruction <= ADDI; // For computing effective address
                    end
                    `OPCODE_U: begin
                        // LUI instruction
                        decoded_rd_address <= rd;
                        decoded_immediate <= {imm_u, 12'b0}; // Immediate value shifted left 12 bits
                        decoded_reg_write_enable <= 1;
                        decoded_reg_input_mux <= IMMEDIATE;
                    end
                    `OPCODE_AUIPC: begin
                        // AUIPC instruction
                        decoded_rd_address <= rd;
                        decoded_immediate <= {imm_u, 12'b0}; // Immediate value shifted left 12 bits
                        decoded_reg_write_enable <= 1;
                        decoded_alu_instruction <= ADD; // Will be used with PC in execution stage
                    end
                    default: begin
                        // No operation or unrecognized opcode
                    end
                endcase
            end
        end
    end
endmodule
