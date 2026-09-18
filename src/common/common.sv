// common.sv
`ifndef COMMON_SV
`define COMMON_SV

// Global Macros
`define DATA_WIDTH 32
`define INSTRUCTION_WIDTH 32
`define DATA_MEMORY_ADDRESS_WIDTH 32
`define INSTRUCTION_MEMORY_ADDRESS_WIDTH 32

// Type Definitions
typedef logic [`DATA_WIDTH-1:0] data_t;
typedef logic [`INSTRUCTION_WIDTH-1:0] instruction_t;
typedef logic [`DATA_MEMORY_ADDRESS_WIDTH-1:0] data_memory_address_t;
typedef logic [`INSTRUCTION_MEMORY_ADDRESS_WIDTH-1:0] instruction_memory_address_t;

typedef struct packed {
    instruction_memory_address_t base_instructions_address;
    data_memory_address_t base_data_address; // This is where kernel function arguments are stored
    data_t num_blocks;
    data_t num_warps_per_block;
} kernel_config_t;

// RISC-V Definitions
`define OPCODE_WIDTH 7
`define FUNCT3_WIDTH 3
`define FUNCT7_WIDTH 7

// Halt Instruction Opcode
`define OPCODE_HALT     7'b1111111

// Vector-Scalar Instruction Opcodes (SX_SLTI and SX_SLT)
// SX_SLTI sets one bit of a scalar register based on thread's comparison result
`define OPCODE_SX_SLT   7'b1111110        // SX_SLT rd, rs1, rs2 <=> rd[id] = rs1 < rs2 ? 1 : 0
`define OPCODE_SX_SLTI  7'b1111101        // SX_SLTI rd, rs1, imm <=> rd[id] = rs1 < imm ? 1 : 0

// Instruction Opcodes
// The entire opcode is 7 bits, the most significant bit decides whether the instruction is vector or scalar
`define OPCODE_R        6'b110011         // Used by all R-type instructions (ADD, SUB, SLL, SLT, XOR, SRL, SRA)
`define OPCODE_I        6'b010011         // Used by ALU I-type instructions (ADDI, SLTI, XORI, ORI, ANDI, SLLI, SRLI, SRAI)
`define OPCODE_S        6'b100011         // Used by store instructions (SB, SH, SW)
`define OPCODE_U        6'b110111         // Used by LUI
`define OPCODE_LOAD     6'b000011         // Used by load instructions (LB, LH, LW)
`define OPCODE_AUIPC    6'b010111         // Used by AUIPC

// Those instructions can only be used by scalar instructions
`define OPCODE_B        7'b1100011        // Used by branch instructions (BEQ, BNE, BLT, BGE)
`define OPCODE_J        7'b1101111        // Used by JAL but it is not a vector instruction, jumps are scalar
`define OPCODE_JALR     7'b1100111        // Used by JALR but it is not a vector instruction, jumps are scalar

typedef logic [`OPCODE_WIDTH-1:0] opcode_t;
typedef logic [`FUNCT3_WIDTH-1:0] funct3_t;
typedef logic [`FUNCT7_WIDTH-1:0] funct7_t;
typedef logic [11:0] imm12_t;

// alu instructions enum
typedef enum logic [4:0] {
    // immediate instructions
    ADDI,
    SLTI,
    XORI,
    ORI,
    ANDI,
    SLLI,
    SRLI,
    SRAI,

    // register instructions
    ADD,
    SUB,
    SLL,
    SLT,
    XOR,
    SRL,
    SRA,
    OR,
    AND,

    // compare instructions
    BEQ,
    BNE,
    BLT,
    BGE,

    // jump instructions
    JAL,
    JALR
} alu_instruction_t;

// warp state enum
typedef enum logic [2:0] {
    WARP_IDLE,
    WARP_FETCH,
    WARP_DECODE,
    WARP_REQUEST,
    WARP_WAIT,
    WARP_EXECUTE,
    WARP_UPDATE,
    WARP_DONE
} warp_state_t;

// fetcher state enum
typedef enum logic [1:0] {
    FETCHER_IDLE,
    FETCHER_FETCHING,
    FETCHER_DONE
} fetcher_state_t;

// lsu state enum
typedef enum logic [1:0] {
    LSU_IDLE,
    LSU_REQUESTING,
    LSU_WAITING,
    LSU_DONE
} lsu_state_t;

// reg input mux
typedef enum logic [2:0] {
    ALU_OUT,
    LSU_OUT,
    IMMEDIATE,
    PC_PLUS_1,
    VECTOR_TO_SCALAR
} reg_input_mux_t;

// sign extend function
function automatic data_t sign_extend(imm12_t imm12);
    data_t signed_imm12;
    if (imm12[11]) begin
        signed_imm12 = {{20{1'b1}}, imm12};
    end else begin
        signed_imm12 = {{20{1'b0}}, imm12};
    end
    return signed_imm12;
endfunction

// sign extend function for 13-bit immediate values
function automatic data_t sign_extend_13(logic[12:0] imm13);
    data_t signed_imm13;
    if (imm13[12]) begin
        signed_imm13 = {{19{1'b1}}, imm13};
    end else begin
        signed_imm13 = {{19{1'b0}}, imm13};
    end
    return signed_imm13;
endfunction

// sign extend function for 21-bit immediate values
function automatic data_t sign_extend_21(logic[20:0] imm21);
    data_t signed_imm21;
    if (imm21[20]) begin
        signed_imm21 = {{11{1'b1}}, imm21};
    end else begin
        signed_imm21 = {{11{1'b0}}, imm21};
    end
    return signed_imm21;
endfunction

// Virgo Microarchitecture Constants
`define VIRGO_SHARED_MEM_BASE   32'h8000_0000
`define VIRGO_SHARED_MEM_LIMIT  32'h8000_1000  // 4KB shared memory (1024 32-bit words)
`define VIRGO_MMIO_BASE         32'h8001_0000
`define VIRGO_MMIO_LIMIT        32'h8001_0100

// Virgo Matrix Unit MMIO Registers
`define VIRGO_REG_CTRL          32'h8001_0000  // [0]=START, [1]=ACCUMULATE, [2]=CLEAR_ACC
`define VIRGO_REG_STATUS        32'h8001_0004  // [0]=BUSY, [1]=DONE
`define VIRGO_REG_DIM_M         32'h8001_0008  // M dimension (tile rows)
`define VIRGO_REG_DIM_N         32'h8001_000C  // N dimension (tile cols)
`define VIRGO_REG_DIM_K         32'h8001_0010  // K dimension (inner product)
`define VIRGO_REG_ADDR_A        32'h8001_0014  // Shared memory word offset for matrix A
`define VIRGO_REG_ADDR_B        32'h8001_0018  // Shared memory word offset for matrix B
`define VIRGO_REG_ADDR_C        32'h8001_001C  // Shared memory word offset for matrix C
`define VIRGO_REG_CYCLES        32'h8001_0020  // Matrix unit cycle counter
`define VIRGO_REG_SYNC          32'h8001_0024  // Hardware barrier/sync: holds ready low while busy

`endif // COMMON_SV
