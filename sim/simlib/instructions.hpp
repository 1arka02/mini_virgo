#pragma once
#include "verilated.h"
#include "error.hpp"
#include <array>
#include <algorithm>
#include <bitset>

namespace sim {

// Opcodes

// if a value here is 6 bits, then the 7th leftmost bit defines whether it's a scalar or vector instruction
// 1 for scalar, 0 for vector
enum class Opcode : IData {
    LUI      =   0b110111,         // Used by LUI (U-type)
    AUIPC    =   0b010111,         // Used by AUIPC
    ITYPE    =   0b010011,         // Used by ALU I-type instructions (ADDI, SLTI, XORI, ORI, ANDI, SLLI, SRLI, SRAI)
    RTYPE    =   0b110011,         // Used by all R-type instructions (ADD, SUB, SLL, SLT, XOR, SRL, SRA)
    LOAD     =   0b000011,         // Used by load instructions (LB, LH, LW)
    STYPE    =   0b100011,         // Used by store instructions (SB, SH, SW)
    // Jumps and branches can only be scalar instructions
    JTYPE    =  0b1101111,         // Used by JAL (J-type)
    JALR     =  0b1100111,         // Used by JALR (I-type)
    BTYPE    =  0b1100011,         // Used by branch instructions (BEQ, BNE, BLT, BGE)
    // Custom opcodes (also only scalar)
    HALT     =  0b1111111,         // Used by HALT
    SX_SLT   =  0b1111110,         // Used by SX_SLT
    SX_SLTI  =  0b1111101          // Used by SX_SLTI
};

constexpr auto opcodes = std::array{
    Opcode::LUI,
    Opcode::AUIPC,
    Opcode::ITYPE,
    Opcode::RTYPE,
    Opcode::LOAD,
    Opcode::STYPE,
    Opcode::JTYPE,
    Opcode::JALR,
    Opcode::BTYPE,
    Opcode::HALT,
    Opcode::SX_SLT,
    Opcode::SX_SLTI
};

constexpr auto is_of_type(IData opcode, Opcode type) -> bool {
    // The 6 bit opcodes
    if (type == Opcode::LUI || type == Opcode::AUIPC || type == Opcode::ITYPE
                            || type == Opcode::RTYPE || type == Opcode::LOAD || type == Opcode::STYPE) {
        return (opcode & (IData)0b111111) == (IData)type;
    }

    // The 7 bit opcodes
    return opcode == (IData)type;
}

constexpr auto is_scalar(IData opcode) -> bool {
    return (opcode & ((IData)1 << 6u)) != 0u;
}

constexpr auto is_vector(IData opcode) -> bool {
    return !is_scalar(opcode);
}

constexpr auto to_scalar(IData opcode) -> IData {
    return opcode | ((IData)1 << 6u);
}

// This returns IData rather than Opcode because it can return a scalar version of the opcode
// if the input string is prefixed with "s."
constexpr auto str_to_opcode(std::string_view str) -> std::optional<IData> {
    auto is_scalar = str.starts_with("s.");
    if (is_scalar) {
        str.remove_prefix(2);
    }

    IData opcode{} ;

    if (str == "lui") {
        opcode = (IData)Opcode::LUI;
    } else if (str == "auipc") {
        opcode = (IData)Opcode::AUIPC;
    } else if (str == "addi") {
        opcode = (IData)Opcode::ITYPE;
    } else if (str == "add") {
        opcode = (IData)Opcode::RTYPE;
    } else if (str == "sub") {
        opcode = (IData)Opcode::RTYPE;
    } else if (str == "slli") {
        opcode = (IData)Opcode::ITYPE;
    } else if (str == "slti") {
        opcode = (IData)Opcode::ITYPE;
    } else if (str == "sltiu") {
        opcode = (IData)Opcode::ITYPE;
    } else if (str == "xori") {
        opcode = (IData)Opcode::ITYPE;
    } else if (str == "srli") {
        opcode = (IData)Opcode::ITYPE;
    } else if (str == "srai") {
        opcode = (IData)Opcode::ITYPE;
    } else if (str == "ori") {
        opcode = (IData)Opcode::ITYPE;
    } else if (str == "andi") {
        opcode = (IData)Opcode::ITYPE;
    } else if (str == "lb") {
        opcode = (IData)Opcode::LOAD;
    } else if (str == "lh") {
        opcode = (IData)Opcode::LOAD;
    } else if (str == "lw") {
        opcode = (IData)Opcode::LOAD;
    } else if (str == "sb") {
        opcode = (IData)Opcode::STYPE;
    } else if (str == "sh") {
        opcode = (IData)Opcode::STYPE;
    } else if (str == "sw") {
        opcode = (IData)Opcode::STYPE;
    } else if (str == "beq") {
        opcode = (IData)Opcode::BTYPE;
    } else if (str == "bne") {
        opcode = (IData)Opcode::BTYPE;
    } else if (str == "blt") {
        opcode = (IData)Opcode::BTYPE;
    } else if (str == "bge") {
        opcode = (IData)Opcode::BTYPE;
    } else if (str == "jal") {
        opcode = (IData)Opcode::JTYPE;
    } else if (str == "jalr") {
        opcode = (IData)Opcode::JALR;
    } else if (str == "halt") {
        opcode = (IData)Opcode::HALT;
    } else if (str == "sx.slt") {
        opcode = (IData)Opcode::SX_SLT;
    } else if (str == "sx.slti") {
        opcode = (IData)Opcode::SX_SLTI;
    } else {
        return std::nullopt;
    }

    if (is_scalar) {
        opcode = to_scalar(opcode);
    }

    return opcode;
}

constexpr auto opcode_to_str(IData opcode) -> std::string_view {
    switch (opcode) {
    case (IData)Opcode::LUI:
        return is_scalar(opcode) ? "s.lui" : "lui";
    case (IData)Opcode::AUIPC:
        return is_scalar(opcode) ? "s.auipc" : "auipc";
    case (IData)Opcode::ITYPE:
        return is_scalar(opcode) ? "s.<itype>" : "<itype>";
    case (IData)Opcode::RTYPE:
        return is_scalar(opcode) ? "s.<rtype>" : "<rtype>";
    case (IData)Opcode::LOAD:
        return is_scalar(opcode) ? "s.<load>" : "<load>";
    case (IData)Opcode::STYPE:
        return is_scalar(opcode) ? "s.<store>" : "<store>";
    case (IData)Opcode::JTYPE:
        return "jal";
    case (IData)Opcode::JALR:
        return "jalr";
    case (IData)Opcode::BTYPE:
        return "beq";
    case (IData)Opcode::HALT:
        return "halt";
    case (IData)Opcode::SX_SLT:
        return "sx.slt";
    case (IData)Opcode::SX_SLTI:
        return "sx.slti";
    default:
        return "unknown";
    }
}




// Funct3
enum class Funct3 : IData {
// I-type
    ADDI            = 0b000,
    SLTI            = 0b010,
    XORI            = 0b100,
    ORI             = 0b110,
    ANDI            = 0b111,
    SLLI            = 0b001,
    SRLI            = 0b101,
    SRAI            = 0b101,
// R-type
    ADD             = 0b000,
    SUB             = 0b000,
    SLL             = 0b001,
    SLT             = 0b010,
    XOR             = 0b100,
    SRL             = 0b101,
    SRA             = 0b101,
    OR              = 0b110,
    AND             = 0b111,
// Load
    LB              = 0b000,
    LH              = 0b001,
    LW              = 0b010,
// Store
    SB              = 0b000,
    SH              = 0b001,
    SW              = 0b010,
// JALR
    JALR            = 0b000,
// B-type
    BEQ             = 0b000,
    BNE             = 0b001,
    BLT             = 0b100,
    BGE             = 0b101,
};

constexpr auto funct3s = std::array{
    Funct3::ADDI,
    Funct3::SLTI,
    Funct3::XORI,
    Funct3::ORI,
    Funct3::ANDI,
    Funct3::SLLI,
    Funct3::SRLI,
    Funct3::SRAI,
    Funct3::ADD,
    Funct3::SUB,
    Funct3::SLL,
    Funct3::SLT,
    Funct3::XOR,
    Funct3::SRL,
    Funct3::SRA,
    Funct3::OR,
    Funct3::AND,
    Funct3::LB,
    Funct3::LH,
    Funct3::LW,
    Funct3::SB,
    Funct3::SH,
    Funct3::SW,
    Funct3::JALR,
    Funct3::BEQ,
    Funct3::BNE,
    Funct3::BLT,
    Funct3::BGE
};

// Funct7
enum class Funct7 : IData {
// I-type
    SLLI            = 0b0000000,
    SRLI            = 0b0000000,
    SRAI            = 0b0100000,
// R-type
    ADD             = 0b0000000,
    SUB             = 0b0100000,
    SLL             = 0b0000000,
    SLT             = 0b0000000,
    XOR             = 0b0000000,
    SRL             = 0b0000000,
    SRA             = 0b0100000,
    OR              = 0b0000000,
    AND             = 0b0000000,
};

constexpr auto funct7s = std::array{
    Funct7::SLLI,
    Funct7::SRLI,
    Funct7::SRAI,
    Funct7::ADD,
    Funct7::SUB,
    Funct7::SLL,
    Funct7::SLT,
    Funct7::XOR,
    Funct7::SRL,
    Funct7::SRA,
    Funct7::OR,
    Funct7::AND
};

/*
x0-x31 -> VECTOR
s0-s31 -> SCALAR
*/
enum class RegisterType {
    VECTOR,
    SCALAR,
};

struct Register {
    IData register_number;
    RegisterType type;

    [[nodiscard]] constexpr auto bits() const -> IData {
        return register_number;
    }
 
    auto operator==(const Register &other) const -> bool {
        return register_number == other.register_number && type == other.type;
    }
    auto operator!=(const Register &other) const -> bool = default;

    [[nodiscard]] auto is_scalar() const -> bool {
        return type == RegisterType::SCALAR;
    }

    [[nodiscard]] auto is_vector() const -> bool {
        return type == RegisterType::VECTOR;
    }

    [[nodiscard]] auto to_str() const -> std::string {
        switch (type) {
        case RegisterType::VECTOR:
            return std::format("x{}", register_number);
        case RegisterType::SCALAR:
            return std::format("s{}", register_number);
        }
        return "unknown";
    }

    [[nodiscard]] auto validate() const -> bool {
        return register_number < 32;
    }
};

// Helper function for validating register numbers
inline void validate_register(Register reg) {
    sim::assert_or_err( reg.validate(), Error(std::format("Invalid register: '{}'.", reg.to_str())));
}

// Helper function for validating instruction IDs
// used for validating opcodes, funct3s, and funct7s
// possible_ids should be a std::array<IData, N>
constexpr void validate_instr_id(const std::string_view id_name, const auto id, const auto &possible_ids) {
    const auto bit_string = std::bitset<7>((IData)id).to_string();
    const auto *const found = std::find(possible_ids.begin(), possible_ids.end(), id);
    assert_or_err(found != possible_ids.end(), Error(std::format("Unknown {}: '0b{}'", id_name, bit_string)));
}

struct InstructionBits {
    IData bits{};

    InstructionBits() = default;
    InstructionBits(IData bits) : bits(bits) {}

    operator IData() {
        return bits;
    }

    constexpr auto set_opcode(Opcode opcode) -> InstructionBits& {
        validate_instr_id("opcode", opcode, opcodes);
        bits |= (IData)opcode;
        return *this;
    }

    constexpr auto set_rd(Register rd) -> InstructionBits& {
        validate_register(rd);
        bits |= rd.bits() << 7u;
        return *this;
    }

    constexpr auto set_funct3(Funct3 funct3) -> InstructionBits& {
        validate_instr_id("funct3", funct3, funct3s);
        bits |= (IData)funct3 << 12u;
        return *this;
    }

    constexpr auto set_rs1(Register rs1) -> InstructionBits& {
        validate_register(rs1);
        bits |= rs1.bits() << 15u;
        return *this;
    }

    constexpr auto set_rs2(Register rs2) -> InstructionBits& {
        validate_register(rs2);
        bits |= rs2.bits() << 20u;
        return *this;
    }

    inline auto set_imm12(IData imm) -> InstructionBits& {
        assert_or_err(imm < 4096, Error(std::format("Invalid immediate: '{}', expected 12-bit.", imm)));
        bits |= imm << 20u;
        return *this;
    }

    constexpr auto set_funct7(Funct7 funct7) -> InstructionBits& {
        validate_instr_id("funct7", funct7, funct7s);
        bits |= (IData)funct7 << 25u;
        return *this;
    }

    inline  auto set_imm20(IData imm20) -> InstructionBits& {
        assert_or_err(imm20 < 1048576, Error(std::format("Invalid immediate: '{}', expected 20-bit.", imm20)));
        bits |= imm20 << 12u;
        return *this;
    }

    inline  auto set_imm21(IData imm21) -> InstructionBits& {
        // FIXME: Need to double check whether this is correct
        assert_or_err(imm21 < 2097152, Error(std::format("Invalid immediate: '{}', expected 21-bit.", imm21)));

        auto imm_j = std::bitset<32>{};
        auto imm = std::bitset<21>(imm21);

        imm_j[31] = imm[20];
        for(auto i = 21u; i <= 30u; i++) {
            imm_j[i] = imm[i - 20];
        }
        imm_j[20] = imm[11];
        for(auto i = 12u; i <= 19u; i++) {
            imm_j[i] = imm[i];
        }
        imm_j[0] = 0;

        bits |= imm_j.to_ulong();
        return *this;
    }

    constexpr auto make_scalar() -> InstructionBits& {
        bits |= (IData)1 << 6u;
        return *this;
    }

    constexpr auto make_vector() -> InstructionBits& {
        bits &= ~((IData)1 << 6u);
        return *this;
    }
};

namespace instructions {

// Helper functions for creating instructions
constexpr auto create_utype_instruction(Opcode opcode, Register rd, IData imm20) -> InstructionBits {
    return InstructionBits().set_opcode(opcode).set_rd(rd).set_imm20(imm20);
}
constexpr auto create_itype_instruction(Opcode opcode, Funct3 funct3, Register rd, Register rs1, IData imm12) -> InstructionBits {
    return InstructionBits().set_opcode(opcode).set_funct3(funct3).set_rd(rd).set_rs1(rs1).set_imm12(imm12);
}
inline  auto create_itype_shift_instruction(Opcode opcode, Funct3 funct3, Funct7 funct7, Register rd, Register rs1, IData imm12) -> InstructionBits {
    assert_or_err(imm12 < 32, Error(std::format("Invalid immediate: '{}', expected 5-bit immediate in shift instruction.", imm12)));
    return InstructionBits().set_opcode(opcode).set_funct3(funct3).set_funct7(funct7).set_rd(rd).set_rs1(rs1).set_imm12(imm12);
}
constexpr auto create_rtype_instruction(Opcode opcode, Funct3 funct3, Funct7 funct7, Register rd, Register rs1, Register rs2) -> InstructionBits {
    return InstructionBits().set_opcode(opcode).set_funct3(funct3).set_funct7(funct7).set_rd(rd).set_rs1(rs1).set_rs2(rs2);
}
constexpr auto create_jtype_instruction(Opcode opcode, Register rd, IData imm21) -> InstructionBits {
    return InstructionBits().set_opcode(opcode).set_rd(rd).set_imm21(imm21);
}
constexpr auto create_btype_instruction(Opcode opcode, Funct3 funct3, Register rs1, Register rs2, IData imm12) -> InstructionBits {
    // TODO: Implement this
    return InstructionBits{};
    /*return Instruction().set_opcode(opcode).set_funct3(funct3).set_rs1(rs1).set_rs2(rs2);*/
}
constexpr auto create_stype_instruction(Opcode opcode, Funct3 funct3, Register rs1, Register rs2, IData imm12) -> InstructionBits {
    return InstructionBits().set_opcode(opcode).set_funct3(funct3).set_rs1(rs1).set_rs2(rs2).set_imm12(imm12);
}

// Instruction constructors

// U-type
constexpr auto lui(Register rd, IData imm20) -> InstructionBits {
    return create_utype_instruction(Opcode::LUI, rd, imm20);
}
constexpr auto auipc(Register rd, IData imm20) -> InstructionBits {
    return create_utype_instruction(Opcode::AUIPC, rd, imm20);
}

// I-type
constexpr auto addi(Register rd, Register rs1, IData imm12) -> InstructionBits {
    return create_itype_instruction(Opcode::ITYPE, Funct3::ADDI, rd, rs1, imm12);
}
constexpr auto slti(Register rd, Register rs1, IData imm12) -> InstructionBits {
    return create_itype_instruction(Opcode::ITYPE, Funct3::SLTI, rd, rs1, imm12);
}
constexpr auto xori(Register rd, Register rs1, IData imm) -> InstructionBits {
    return create_itype_instruction(Opcode::ITYPE, Funct3::XORI,  rd, rs1, imm);
}
constexpr auto ori(Register rd, Register rs1, IData imm) -> InstructionBits {
    return create_itype_instruction(Opcode::ITYPE, Funct3::ORI, rd, rs1, imm);
}
constexpr auto andi(Register rd, Register rs1, IData imm) -> InstructionBits {
    return create_itype_instruction(Opcode::ITYPE, Funct3::ANDI, rd, rs1, imm);
}
constexpr auto slli(Register rd, Register rs1, IData imm) -> InstructionBits {
    return create_itype_shift_instruction(Opcode::ITYPE, Funct3::SLLI, Funct7::SLLI, rd, rs1, imm);
}
constexpr auto srli(Register rd, Register rs1, IData imm) -> InstructionBits {
    return create_itype_shift_instruction(Opcode::ITYPE, Funct3::SRLI, Funct7::SRLI, rd, rs1, imm);
}
constexpr auto srai(Register rd, Register rs1, IData imm) -> InstructionBits {
    return create_itype_shift_instruction(Opcode::ITYPE, Funct3::SRAI, Funct7::SRAI, rd, rs1, imm);
}

// R-type
constexpr auto add(Register rd, Register rs1, Register rs2) -> InstructionBits {
    return create_rtype_instruction(Opcode::RTYPE, Funct3::ADD, Funct7::ADD, rd, rs1, rs2);
}
constexpr auto sub(Register rd, Register rs1, Register rs2) -> InstructionBits {
    return create_rtype_instruction(Opcode::RTYPE, Funct3::SUB, Funct7::SUB, rd, rs1, rs2);
}
constexpr auto sll(Register rd, Register rs1, Register rs2) -> InstructionBits {
    return create_rtype_instruction(Opcode::RTYPE, Funct3::SLL, Funct7::SLL, rd, rs1, rs2);
}
constexpr auto slt(Register rd, Register rs1, Register rs2) -> InstructionBits {
    return create_rtype_instruction(Opcode::RTYPE, Funct3::SLT, Funct7::SLT, rd, rs1, rs2);
}
constexpr auto xor_(Register rd, Register rs1, Register rs2) -> InstructionBits {
    return create_rtype_instruction(Opcode::RTYPE, Funct3::XOR, Funct7::XOR, rd, rs1, rs2);
}
constexpr auto srl(Register rd, Register rs1, Register rs2) -> InstructionBits {
    return create_rtype_instruction(Opcode::RTYPE, Funct3::SRL, Funct7::SRL, rd, rs1, rs2);
}
constexpr auto sra(Register rd, Register rs1, Register rs2) -> InstructionBits {
    return create_rtype_instruction(Opcode::RTYPE, Funct3::SRA, Funct7::SRA, rd, rs1, rs2);
}
constexpr auto or_(Register rd, Register rs1, Register rs2) -> InstructionBits {
    return create_rtype_instruction(Opcode::RTYPE, Funct3::OR, Funct7::OR, rd, rs1, rs2);
}
constexpr auto and_(Register rd, Register rs1, Register rs2) -> InstructionBits {
    return create_rtype_instruction(Opcode::RTYPE, Funct3::AND, Funct7::AND, rd, rs1, rs2);
}

// Load
constexpr auto lb(Register rd, Register rs1, IData imm12) -> InstructionBits {
    return create_itype_instruction(Opcode::LOAD, Funct3::LB, rd, rs1, imm12);
}
constexpr auto lh(Register rd, Register rs1, IData imm12) -> InstructionBits {
    return create_itype_instruction(Opcode::LOAD, Funct3::LH, rd, rs1, imm12);
}
constexpr auto lw(Register rd, Register rs1, IData imm12) -> InstructionBits {
    return create_itype_instruction(Opcode::LOAD, Funct3::LW, rd, rs1, imm12);
}

// Store
constexpr auto sb(Register rs1, Register rs2, IData imm12) -> InstructionBits {
    return create_stype_instruction(Opcode::STYPE, Funct3::SB, rs1, rs2, imm12);
}
constexpr auto sh(Register rs1, Register rs2, IData imm12) -> InstructionBits {
    return create_stype_instruction(Opcode::STYPE, Funct3::SH, rs1, rs2, imm12);
}
constexpr auto sw(Register rs1, Register rs2, IData imm12) -> InstructionBits {
    return create_stype_instruction(Opcode::STYPE, Funct3::SW, rs1, rs2, imm12);
}

// J-type
constexpr auto jal(Register rd, IData imm21) -> InstructionBits {
    return create_jtype_instruction(Opcode::JTYPE, rd, imm21);
}

// JALR
constexpr auto jalr(Register rd, Register rs1, IData imm12) -> InstructionBits {
    return create_itype_instruction(Opcode::JALR, Funct3::JALR, rd, rs1, imm12);
}

// B-type
constexpr auto beq(Register rs1, Register rs2, IData imm12) -> InstructionBits {
    return create_btype_instruction(Opcode::BTYPE, Funct3::BEQ, rs1, rs2, imm12);
}
constexpr auto bne(Register rs1, Register rs2, IData imm12) -> InstructionBits {
    return create_btype_instruction(Opcode::BTYPE, Funct3::BNE, rs1, rs2, imm12);
}
constexpr auto blt(Register rs1, Register rs2, IData imm12) -> InstructionBits {
    return create_btype_instruction(Opcode::BTYPE, Funct3::BLT, rs1, rs2, imm12);
}
constexpr auto bge(Register rs1, Register rs2, IData imm12) -> InstructionBits {
    return create_btype_instruction(Opcode::BTYPE, Funct3::BGE, rs1, rs2, imm12);
}

// Custom opcodes
constexpr auto halt() -> InstructionBits {
    return InstructionBits().set_opcode(Opcode::HALT);
}
constexpr auto sx_slt(Register rd, Register rs1, Register rs2) -> InstructionBits {
    return create_rtype_instruction(Opcode::SX_SLT, Funct3::SLT, Funct7::SLT, rd, rs1, rs2);
}
constexpr auto sx_slti(Register rd, Register rs1, IData imm12) -> InstructionBits {
    return create_itype_instruction(Opcode::SX_SLTI, Funct3::SLTI, rd, rs1, imm12);
}
}

struct InstructionDeterminant {
    Opcode opcode;
    Funct3 funct3;
    Funct7 funct7;
};

enum class MnemonicName {
    // U-type
    LUI,
    AUIPC,
    // I-type arithmetic
    ADDI,
    SLTI,
    XORI,
    ORI,
    ANDI,
    SLLI,
    SRLI,
    SRAI,
    // R-type
    ADD,
    SUB,
    SLL,
    SLT,
    XOR,
    SRL,
    SRA,
    OR,
    AND,
    // Load
    LB,
    LH,
    LW,
    // Store
    SB,
    SH,
    SW,
    // J-type
    JAL,
    // I-type jumps
    JALR,
    // B-type
    BEQ,
    BNE,
    BLT,
    BGE,
    // Halt
    HALT,
    // SX-type
    SX_SLT,
    SX_SLTI
};

constexpr auto str_to_mnemonic_name(const std::string_view name) -> std::optional<MnemonicName> {
    if (name == "lui") {
        return MnemonicName::LUI;
    } else if (name == "auipc") {
        return MnemonicName::AUIPC;
    } else if (name == "addi") {
        return MnemonicName::ADDI;
    } else if (name == "slti") {
        return MnemonicName::SLTI;
    } else if (name == "xori") {
        return MnemonicName::XORI;
    } else if (name == "ori") {
        return MnemonicName::ORI;
    } else if (name == "andi") {
        return MnemonicName::ANDI;
    } else if (name == "slli") {
        return MnemonicName::SLLI;
    } else if (name == "srli") {
        return MnemonicName::SRLI;
    } else if (name == "srai") {
        return MnemonicName::SRAI;
    } else if (name == "add") {
        return MnemonicName::ADD;
    } else if (name == "sub") {
        return MnemonicName::SUB;
    } else if (name == "sll") {
        return MnemonicName::SLL;
    } else if (name == "slt") {
        return MnemonicName::SLT;
    } else if (name == "xor") {
        return MnemonicName::XOR;
    } else if (name == "srl") {
        return MnemonicName::SRL;
    } else if (name == "sra") {
        return MnemonicName::SRA;
    } else if (name == "or") {
        return MnemonicName::OR;
    } else if (name == "and") {
        return MnemonicName::AND;
    } else if (name == "lb") {
        return MnemonicName::LB;
    } else if (name == "lh") {
        return MnemonicName::LH;
    } else if (name == "lw") {
        return MnemonicName::LW;
    } else if (name == "sb") {
        return MnemonicName::SB;
    } else if (name == "sh") {
        return MnemonicName::SH;
    } else if (name == "sw") {
        return MnemonicName::SW;
    } else if (name == "jal") {
        return MnemonicName::JAL;
    } else if (name == "jalr") {
        return MnemonicName::JALR;
    } else if (name == "beq") {
        return MnemonicName::BEQ;
    } else if (name == "bne") {
        return MnemonicName::BNE;
    } else if (name == "blt") {
        return MnemonicName::BLT;
    } else if (name == "bge") {
        return MnemonicName::BGE;
    } else if (name == "halt") {
        return MnemonicName::HALT;
    } else if (name == "sx.slt") {
        return MnemonicName::SX_SLT;
    } else if (name == "sx.slti") {
        return MnemonicName::SX_SLTI;
    }

    return std::nullopt;
}

constexpr auto to_string(const MnemonicName name) -> std::string_view {
    switch (name) {
    case MnemonicName::LUI:
        return "lui";
    case MnemonicName::AUIPC:
        return "auipc";
    case MnemonicName::ADDI:
        return "addi";
    case MnemonicName::SLTI:
        return "slti";
    case MnemonicName::XORI:
        return "xori";
    case MnemonicName::ORI:
        return "ori";
    case MnemonicName::ANDI:
        return "andi";
    case MnemonicName::SLLI:
        return "slli";
    case MnemonicName::SRLI:
        return "srli";
    case MnemonicName::SRAI:
        return "srai";
    case MnemonicName::ADD:
        return "add";
    case MnemonicName::SUB:
        return "sub";
    case MnemonicName::SLL:
        return "sll";
    case MnemonicName::SLT:
        return "slt";
    case MnemonicName::XOR:
        return "xor";
    case MnemonicName::SRL:
        return "srl";
    case MnemonicName::SRA:
        return "sra";
    case MnemonicName::OR:
        return "or";
    case MnemonicName::AND:
        return "and";
    case MnemonicName::LB:
        return "lb";
    case MnemonicName::LH:
        return "lh";
    case MnemonicName::LW:
        return "lw";
    case MnemonicName::SB:
        return "sb";
    case MnemonicName::SH:
        return "sh";
    case MnemonicName::SW:
        return "sw";
    case MnemonicName::JAL:
        return "jal";
    case MnemonicName::JALR:
        return "jalr";
    case MnemonicName::BEQ:
        return "beq";
    case MnemonicName::BNE:
        return "bne";
    case MnemonicName::BLT:
        return "blt";
    case MnemonicName::BGE:
        return "bge";
    case MnemonicName::HALT:
        return "halt";
    case MnemonicName::SX_SLT:
        return "sx.slt";
    case MnemonicName::SX_SLTI:
        return "sx.slti";
    }

    return "unknown";
}

constexpr auto mnemonic_name_to_opcode(MnemonicName name) -> Opcode {
    switch (name) {
    case MnemonicName::LUI:
        return Opcode::LUI;
    case MnemonicName::AUIPC:
        return Opcode::AUIPC;
    case MnemonicName::ADDI:
        return Opcode::ITYPE;
    case MnemonicName::SLTI:
        return Opcode::ITYPE;
    case MnemonicName::XORI:
        return Opcode::ITYPE;
    case MnemonicName::ORI:
        return Opcode::ITYPE;
    case MnemonicName::ANDI:
        return Opcode::ITYPE;
    case MnemonicName::SLLI:
        return Opcode::ITYPE;
    case MnemonicName::SRLI:
        return Opcode::ITYPE;
    case MnemonicName::SRAI:
        return Opcode::ITYPE;
    case MnemonicName::ADD:
        return Opcode::RTYPE;
    case MnemonicName::SUB:
        return Opcode::RTYPE;
    case MnemonicName::SLL:
        return Opcode::RTYPE;
    case MnemonicName::SLT:
        return Opcode::RTYPE;
    case MnemonicName::XOR:
        return Opcode::RTYPE;
    case MnemonicName::SRL:
        return Opcode::RTYPE;
    case MnemonicName::SRA:
        return Opcode::RTYPE;
    case MnemonicName::OR:
        return Opcode::RTYPE;
    case MnemonicName::AND:
        return Opcode::RTYPE;
    case MnemonicName::LB:
        return Opcode::LOAD;
    case MnemonicName::LH:
        return Opcode::LOAD;
    case MnemonicName::LW:
        return Opcode::LOAD;
    case MnemonicName::SB:
        return Opcode::STYPE;
    case MnemonicName::SH:
        return Opcode::STYPE;
    case MnemonicName::SW:
        return Opcode::STYPE;
    case MnemonicName::JAL:
        return Opcode::JTYPE;
    case MnemonicName::JALR:
        return Opcode::JALR;
    case MnemonicName::BEQ:
        return Opcode::BTYPE;
    case MnemonicName::BNE:
        return Opcode::BTYPE;
    case MnemonicName::BLT:
        return Opcode::BTYPE;
    case MnemonicName::BGE:
        return Opcode::BTYPE;
    case MnemonicName::HALT:
        return Opcode::HALT;
    case MnemonicName::SX_SLT:
        return Opcode::SX_SLT;
    case MnemonicName::SX_SLTI:
        return Opcode::SX_SLTI;
    }
}

struct Mnemonic {
    Mnemonic(MnemonicName name, bool is_scalar) : name(name), has_s_prefix(is_scalar) {}

    [[nodiscard]] auto get_name() const -> MnemonicName {
        return name;
    }

    [[nodiscard]] auto to_str() const -> std::string {
        return std::format("{}{}", has_s_prefix ? "s." : "", to_string(name));
    }

    [[nodiscard]] auto to_opcode() const -> IData {
        auto opcode = (IData)mnemonic_name_to_opcode(name);
        if (has_s_prefix) {
            opcode |= (IData)1 << 6u;
        }
        return opcode;
    }

    [[nodiscard]] auto is_vector_scalar() const -> bool {
        return name == MnemonicName::SX_SLT || name == MnemonicName::SX_SLTI;
    }

    [[nodiscard]] auto is_branch() const -> bool {
        return name == MnemonicName::BEQ || name == MnemonicName::BNE || name == MnemonicName::BLT || name == MnemonicName::BGE;
    }

    [[nodiscard]] auto is_jump() const -> bool {
        return name == MnemonicName::JAL || name == MnemonicName::JALR;
    }

    // in practice, that is equivalent to MSB of the opcode being 1
    [[nodiscard]] auto is_scalar() const -> bool {
        return has_s_prefix || is_vector_scalar() || is_branch() || is_jump();
    }

    auto operator==(const Mnemonic &other) const -> bool {
        return name == other.name && is_scalar() == other.is_scalar();
    }

    auto operator!=(const Mnemonic &other) const -> bool = default;
private:
    MnemonicName name;
    bool has_s_prefix;
};

inline  auto str_to_mnemonic(std::string_view str) -> std::optional<Mnemonic> {
    auto is_scalar = str.starts_with("s.");
    if (is_scalar) {
        str.remove_prefix(2);
    }

    auto name = str_to_mnemonic_name(str);
    if (!name) {
        return std::nullopt;
    }

    return Mnemonic(*name, is_scalar);
}

constexpr auto name_to_determinant(MnemonicName name) -> InstructionDeterminant {
    switch (name) {
        // U-type
        case MnemonicName::LUI:
            return {Opcode::LUI, {}, {}};
        case MnemonicName::AUIPC:
            return {Opcode::AUIPC, {}, {}};
        // I-type arithmetic
        case MnemonicName::ADDI:
            return {Opcode::ITYPE, Funct3::ADDI, {}};
        case MnemonicName::SLTI:
            return {Opcode::ITYPE, Funct3::SLTI, {}};
        case MnemonicName::XORI:
            return {Opcode::ITYPE, Funct3::XORI, {}};
        case MnemonicName::ORI:
            return {Opcode::ITYPE, Funct3::ORI, {}};
        case MnemonicName::ANDI:
            return {Opcode::ITYPE, Funct3::ANDI, {}};
        case MnemonicName::SLLI:
            return {Opcode::ITYPE, Funct3::SLLI, Funct7::SLLI};
        case MnemonicName::SRLI:
            return {Opcode::ITYPE, Funct3::SRLI, Funct7::SRLI};
        case MnemonicName::SRAI:
            return {Opcode::ITYPE, Funct3::SRAI, Funct7::SRAI};
        // R-type
        case MnemonicName::ADD:
            return {Opcode::RTYPE, Funct3::ADD, Funct7::ADD};
        case MnemonicName::SUB:
            return {Opcode::RTYPE, Funct3::SUB, Funct7::SUB};
        case MnemonicName::SLL:
            return {Opcode::RTYPE, Funct3::SLL, Funct7::SLL};
        case MnemonicName::SLT:
            return {Opcode::RTYPE, Funct3::SLT, Funct7::SLT};
        case MnemonicName::XOR:
            return {Opcode::RTYPE, Funct3::XOR, Funct7::XOR};
        case MnemonicName::SRL:
            return {Opcode::RTYPE, Funct3::SRL, Funct7::SRL};
        case MnemonicName::SRA:
            return {Opcode::RTYPE, Funct3::SRA, Funct7::SRA};
        case MnemonicName::OR:
            return {Opcode::RTYPE, Funct3::OR, Funct7::OR};
        case MnemonicName::AND:
            return {Opcode::RTYPE, Funct3::AND, Funct7::AND};
        // Load
        case MnemonicName::LB:
            return {Opcode::LOAD, Funct3::LB, {}};
        case MnemonicName::LH:
            return {Opcode::LOAD, Funct3::LH, {}};
        case MnemonicName::LW:
            return {Opcode::LOAD, Funct3::LW, {}};
        // Store
        case MnemonicName::SB:
            return {Opcode::STYPE, Funct3::SB, {}};
        case MnemonicName::SH:
            return {Opcode::STYPE, Funct3::SH, {}};
        case MnemonicName::SW:
            return {Opcode::STYPE, Funct3::SW, {}};
        // J-type
        case MnemonicName::JAL:
            return {Opcode::JTYPE, {}, {}};
        // I-type jumps
        case MnemonicName::JALR:
            return {Opcode::JALR, Funct3::JALR, {}};
        // B-type
        case MnemonicName::BEQ:
            return {Opcode::BTYPE, Funct3::BEQ, {}};
        case MnemonicName::BNE:
            return {Opcode::BTYPE, Funct3::BNE, {}};
        case MnemonicName::BLT:
            return {Opcode::BTYPE, Funct3::BLT, {}};
        case MnemonicName::BGE:
            return {Opcode::BTYPE, Funct3::BGE, {}};
        // Halt
        case MnemonicName::HALT:
            return {Opcode::HALT, {}, {}};
        // SX-type
        case MnemonicName::SX_SLT:
            return {Opcode::SX_SLT, Funct3::SLT, Funct7::SLT};
        case MnemonicName::SX_SLTI:
            return {Opcode::SX_SLTI, Funct3::SLTI, {}};
    }

    std::unreachable();
}

}

// s suffix for scalar registers
inline sim::Register operator ""_s(unsigned long long reg) {
    return sim::Register{.register_number = (IData)reg, .type = sim::RegisterType::SCALAR};
}

// x suffix for vector registers
inline sim::Register operator ""_x(unsigned long long reg) {
    return sim::Register{.register_number = (IData)reg, .type = sim::RegisterType::VECTOR};
}
