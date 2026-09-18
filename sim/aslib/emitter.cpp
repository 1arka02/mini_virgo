#include "emitter.hpp"
#include "instructions.hpp"

namespace as {

auto translate_to_binary(const as::parser::Program& program) -> std::vector<sim::InstructionBits> {
    const auto len = program.instructions.size();
    auto machine_code = std::vector<sim::InstructionBits>(len);

    for(auto i = 0u; i < len; i++) {
        const auto instruction = program.instructions[i];
        auto instruction_bits = sim::InstructionBits{};

        const auto [opcode, funct3, funct7] = name_to_determinant(instruction.mnemonic.get_name());

        std::visit(as::overloaded{
            [&](const as::parser::ItypeOperands &operands) {
                instruction_bits = sim::instructions::create_itype_instruction(opcode, funct3, operands.rd, operands.rs1, (IData)operands.imm12.value);
            },
                [&](const as::parser::RtypeOperands &operands) {
                instruction_bits = sim::instructions::create_rtype_instruction(opcode, funct3, funct7, operands.rd, operands.rs1, operands.rs2);
            },
                [&](const as::parser::StypeOperands &operands) {
                instruction_bits = sim::instructions::create_stype_instruction(opcode, funct3, operands.rs1, operands.rs2, (IData)operands.imm12.value);
            },
                [&](const as::parser::UtypeOperands &operands) {
                instruction_bits = sim::instructions::create_utype_instruction(opcode, operands.rd, (IData)operands.imm20.value);
            },
                [&](const as::parser::JtypeOperands &operands) {
                // FIXME: The JtypeOperands might be broken. The create_jtype_instruction requires imm21 but we provide imm20
                instruction_bits = sim::instructions::create_jtype_instruction(opcode, operands.rd, (IData)operands.imm20.value);
            },
                [&](const as::parser::JalrOperands &operands) {
                if (std::holds_alternative<token::LabelRef>(operands.immediate_or_label_ref)) {
                    const auto &label_token = std::get<token::LabelRef>(operands.immediate_or_label_ref);
                    if (!program.label_mappings.contains(label_token.label_name)) {
                        // TODO: Error here
                    }
                    instruction_bits = sim::instructions::jalr(operands.rd, 0_x, program.label_mappings.at(label_token.label_name));
                } else {
                    const auto &immediate = std::get<token::Immediate>(operands.immediate_or_label_ref);
                    instruction_bits = sim::instructions::jalr(operands.rd, operands.rs1, immediate.value);
                }
            }

        }, program.instructions[i].operands);

        machine_code[i] = instruction_bits;
    }

    return machine_code;
}

}
