#include "parser.hpp"
#include "instructions.hpp"
#include "lexer.hpp"
#include "token.hpp"

#define EXPECT_OR_RETURN(opt, ...) \
    auto opt = expect<__VA_ARGS__>();     \
    if (!opt.has_value()) {        \
        return std::nullopt;       \
    }

namespace as {

namespace parser {
auto to_string(const ImmediateOrLabelref &imm) -> std::string {
    return std::visit(overloaded{
                          [](const token::Immediate &imm) -> std::string { return std::to_string(imm.value); },
                          [](const token::LabelRef &label) -> std::string { return std::string{label.label_name}; },
                      },
                      imm);
}

auto line_to_str(const parser::Line &line) -> std::string {
    return std::visit(overloaded{
                          [](const parser::JustLabel &label) { return std::format("{}:", label.label.name); },
                          [](const parser::BlocksDirective &blocks) { return std::format(".blocks {}", blocks.number); },
                          [](const parser::WarpsDirective &warps) { return std::format(".warps {}", warps.number); },
                          [](const parser::Instruction &instruction) {
                              return instruction.to_str();
                          },
                      },
                      line);
}

}

#define CHECK_REG(reg, should_be_scalar) \
    if (!check_register_correct_type(reg, should_be_scalar)) { \
        return std::nullopt; \
    }

auto Parser::check_register_correct_type(const Token &reg_token, bool should_be_scalar) -> bool {
    const auto &reg = reg_token.as<token::Register>();
    if (reg.register_data.is_scalar() != should_be_scalar) {
        push_err(std::format("Register '{}' should be {}", reg.register_data.to_str(), should_be_scalar ? "scalar" : "vector"), reg_token.col);
        return false;
    }

    return true;
}

auto Parser::chop() -> std::optional<Token> {
    if (tokens.empty()) {
        return std::nullopt;
    }

    auto token = tokens.front();
    tokens = tokens.subspan(1);

    return token;
}

[[nodiscard]] auto Parser::peek() const -> const Token * {
    if (tokens.empty()) {
        return nullptr;
    }

    return &tokens.front();
}

void Parser::push_err(Error &&err) { errors.push_back(std::move(err)); }

void Parser::push_err(std::string &&message, unsigned column) {
    errors.emplace_back(std::move(message), column);
}

void Parser::throw_unexpected_token(std::string &&expected, const Token &unexpected) {
    push_err(std::format("Unexpected token: Expected {}, instead found {}", std::move(expected), unexpected.to_str()), unexpected.col);
}

void Parser::throw_unexpected_eos(std::string &&expected) {
    push_err(std::format("Unexpected end of stream: Expected {}", std::move(expected)), 0);
}

auto Parser::parse_instruction() -> std::optional<Result> {
    auto mnemonic_token = *chop();
    auto mnemonic = mnemonic_token.as<token::Mnemonic>().mnemonic;

    // HALT
    if (mnemonic.get_name() == sim::MnemonicName::HALT) {
        return parser::Instruction{.mnemonic = mnemonic};
    }

    // ADDI, SLTI, XORI, ORI, ANDI, SLLI, SRLI, SRAI, SX_SLTI
    if (parser::is_itype_arithmetic(mnemonic.get_name())) {
        return parse_itype_arithemtic_instruction(mnemonic);
    }

    // ADD, SUB, SLL, SLT, XOR, SRL, SRA, OR, AND, SX_SLT
    if (parser::is_rtype(mnemonic.get_name())) {
        return parse_rtype_instruction(mnemonic);
    }

    // LB, LH, LW
    if (parser::is_load_type(mnemonic.get_name())) {
        return parse_load_instruction(mnemonic);
    }

    // SB, SH, SW
    if (parser::is_store_type(mnemonic.get_name())) {
        return parse_store_instruction(mnemonic);
    }

    // LUI, AUIPC
    if (parser::is_utype(mnemonic.get_name())) {
        return parse_utype_instruction(mnemonic);
    }

    if (mnemonic.get_name() == sim::MnemonicName::JALR) {
        return parse_jalr_instruction(mnemonic);
    }

    if (mnemonic.get_name() == sim::MnemonicName::JAL) {
        return parse_jal_instruction(mnemonic);
    }

    push_err(std::format("Unknown mnemonic: '{}'", mnemonic.to_str()), mnemonic_token.col);
    return std::nullopt;
}

auto Parser::parse_utype_instruction(const sim::Mnemonic& mnemonic) -> std::optional<parser::Instruction> {
    EXPECT_OR_RETURN(rd, token::Register);
    EXPECT_OR_RETURN(comma, token::Comma);
    EXPECT_OR_RETURN(imm20, token::Immediate);

    CHECK_REG(*rd, mnemonic.is_scalar());

    auto instruction = parser::Instruction{
        .label = {},
        .mnemonic = mnemonic,
        .operands = parser::UtypeOperands{
            .rd = rd->as<token::Register>().register_data,
            .imm20 = imm20->as<token::Immediate>(),
        },
    };

    return instruction;
}

// <opcode> <rd>, <rs1>, <imm12>
auto Parser::parse_itype_arithemtic_instruction(const sim::Mnemonic &mnemonic) -> std::optional<parser::Instruction> {
    EXPECT_OR_RETURN(rd, token::Register);
    EXPECT_OR_RETURN(comma1, token::Comma);
    EXPECT_OR_RETURN(rs1, token::Register);
    EXPECT_OR_RETURN(comma2, token::Comma);
    EXPECT_OR_RETURN(imm12, token::Immediate);

    // if it's a vector scalar instuction, the destination register should be scalar but the source registers should be vector
    if (mnemonic.get_name() == sim::MnemonicName::SX_SLTI) {
        CHECK_REG(*rd, true);
        CHECK_REG(*rs1, false);
    } else {
        CHECK_REG(*rd, mnemonic.is_scalar());
        CHECK_REG(*rs1, mnemonic.is_scalar());
    }

    auto instruction = parser::Instruction{
        .label = {},
        .mnemonic = mnemonic,
        .operands = parser::ItypeOperands{
            .rd = rd->as<token::Register>().register_data,
            .rs1 = rs1->as<token::Register>().register_data,
            .imm12 = imm12->as<token::Immediate>(),
        },
    };

    return instruction;
}

auto Parser::parse_rtype_instruction(const sim::Mnemonic& mnemonic) -> std::optional<parser::Instruction> {
    EXPECT_OR_RETURN(rd, token::Register);
    EXPECT_OR_RETURN(comma1, token::Comma);
    EXPECT_OR_RETURN(rs1, token::Register);
    EXPECT_OR_RETURN(comma2, token::Comma);
    EXPECT_OR_RETURN(rs2, token::Register);

    // if it's a vector scalar instuction, the destination register should be scalar but the source registers should be vector
    if (mnemonic.get_name() == sim::MnemonicName::SX_SLT) {
        CHECK_REG(*rd, true);
        CHECK_REG(*rs1, false);
        CHECK_REG(*rs2, false);
    } else {
        CHECK_REG(*rd, mnemonic.is_scalar());
        CHECK_REG(*rs1, mnemonic.is_scalar());
        CHECK_REG(*rs2, mnemonic.is_scalar());
    }

    auto instruction = parser::Instruction{
        .label = {},
        .mnemonic = mnemonic,
        .operands = parser::RtypeOperands{
            .rd = rd->as<token::Register>().register_data,
            .rs1 = rs1->as<token::Register>().register_data,
            .rs2 = rs2->as<token::Register>().register_data,
        },
    };

    return instruction;
}

auto Parser::parse_load_instruction(const sim::Mnemonic& mnemonic) -> std::optional<parser::Instruction> {
    EXPECT_OR_RETURN(rd, token::Register);
    EXPECT_OR_RETURN(comma1, token::Comma);
    EXPECT_OR_RETURN(offset, token::Immediate);
    EXPECT_OR_RETURN(lparen, token::Lparen);
    EXPECT_OR_RETURN(rs1, token::Register);
    EXPECT_OR_RETURN(rparen, token::Rparen);

    CHECK_REG(*rd, mnemonic.is_scalar());
    CHECK_REG(*rs1, mnemonic.is_scalar());

    auto instruction = parser::Instruction{
        .label = {},
        .mnemonic = mnemonic,
        .operands = parser::ItypeOperands{
            .rd = rd->as<token::Register>().register_data,
            .rs1 = rs1->as<token::Register>().register_data,
            .imm12 = offset->as<token::Immediate>(),
        },
    };

    return instruction;
}


auto Parser::parse_store_instruction(const sim::Mnemonic& mnemonic) -> std::optional<parser::Instruction> {
    EXPECT_OR_RETURN(rs2, token::Register);
    EXPECT_OR_RETURN(comma1, token::Comma);
    EXPECT_OR_RETURN(offset, token::Immediate);
    EXPECT_OR_RETURN(lparen, token::Lparen);
    EXPECT_OR_RETURN(rs1, token::Register);
    EXPECT_OR_RETURN(rparen, token::Rparen);

    CHECK_REG(*rs1, mnemonic.is_scalar());
    CHECK_REG(*rs2, mnemonic.is_scalar());

    auto instruction = parser::Instruction{
        .label = {},
        .mnemonic = mnemonic,
        .operands = parser::StypeOperands{
            .rs1 = rs1->as<token::Register>().register_data,
            .rs2 = rs2->as<token::Register>().register_data,
            .imm12 = offset->as<token::Immediate>(),
        },
    };

    return instruction;
}

auto Parser::parse_jal_instruction(const sim::Mnemonic& mnemonic) -> std::optional<parser::Instruction> {
    EXPECT_OR_RETURN(rd, token::Register);
    EXPECT_OR_RETURN(comma, token::Comma);
    EXPECT_OR_RETURN(immediate, token::Immediate);

    return parser::Instruction {
        .label = {},
        .mnemonic = mnemonic,
        .operands = parser::JtypeOperands {
            .rd = rd->as<token::Register>().register_data,
            .imm20 = immediate->as<token::Immediate>()
        }
    };
}

// JALR instruction is JALR rd, rs1, offset <=> rd = PC + 1, PC = rs1 + offset
// Therefore we can use it to jump to labels if rs1 = r0
// That's why there are two syntaxes:
// JALR rd, offset(rs1)
// JALR rd, labelref
auto Parser::parse_jalr_instruction(const sim::Mnemonic& mnemonic) -> std::optional<parser::Instruction> {
    EXPECT_OR_RETURN(rd, token::Register);
    EXPECT_OR_RETURN(comma1, token::Comma);
    EXPECT_OR_RETURN(next, token::LabelRef, token::Immediate);

    if (next->is_of_type<token::LabelRef>()) {
        return parser::Instruction {
            .label = {},
            .mnemonic = mnemonic,
            .operands = parser::JalrOperands {
                .rd = rd->as<token::Register>().register_data,
                .rs1 = 0_x,
                .immediate_or_label_ref = next->as<token::LabelRef>()
            }
        };
    }

    EXPECT_OR_RETURN(lparen, token::Lparen);
    EXPECT_OR_RETURN(rs1, token::Register);
    EXPECT_OR_RETURN(rparen, token::Rparen);

    return parser::Instruction {
        .label = {},
        .mnemonic = mnemonic,
        .operands = parser::JalrOperands {
            .rd = rd->as<token::Register>().register_data,
            .rs1 = rs1->as<token::Register>().register_data,
            .immediate_or_label_ref = next->as<token::Immediate>()
        }
    };
}


auto Parser::parse_directive() -> std::optional<Result> {
    auto token = chop();
    if (!token) {
        return std::nullopt;
    }

    auto number = expect<token::Immediate>();
    if(!number.has_value()) {
        return std::nullopt;
    }

    const auto value = number->as<token::Immediate>().value;
    if (value < 1) {
        push_err(std::format("Invalid number of {}: '{}'", token->to_str(), value), number->col);
        return std::nullopt;
    }

    // The line should end here, otherwise it's an error
    if (peek() != nullptr) {
        throw_unexpected_token("End of line", *peek());
        return std::nullopt;
    }

    if (token->is_of_type<token::BlocksDirective>()) {
        return parser::BlocksDirective{.number = static_cast<std::uint32_t>(value)};
    } else {
        return parser::WarpsDirective{.number = static_cast<std::uint32_t>(value)};
    }

    return std::nullopt;
}

auto Parser::parse_line() -> std::optional<Result> {
    if (tokens.empty()) {
        return std::nullopt;
    }

    auto token = *peek();

    if (token.is_of_type<token::BlocksDirective>() || token.is_of_type<token::WarpsDirective>()) {
        return parse_directive();
    }

    std::optional<as::token::Label> label = std::nullopt;

    if (token.is_of_type<token::Label>()) {
        label = token.as<token::Label>();
        chop();

        if (tokens.empty()) {
            return parser::JustLabel{.label = token.as<token::Label>()};
        }

        token = *peek();
    }

    if (token.is_of_type<token::Mnemonic>()) {
        auto instruction = parse_instruction();

        if(!instruction.has_value()) {
            return std::nullopt;
        }

        std::get<parser::Instruction>(instruction.value()).label = label;

        if (tokens.empty()) {
            return instruction;
        }

        push_err(std::format("Unexpected token: Expected end of line, instead found '{}'", peek()->to_str()), token.col);
        return std::nullopt;
    }

    push_err(std::format("Unexpected token: Expected mnemonic or directive, instead found '{}'", token.to_str()), token.col);
    return std::nullopt;
}

auto parse_line(std::span<Token> tokens) -> std::expected<Parser::Result, std::vector<Parser::Error>> { 
    Parser parser{tokens};
    auto result = parser.parse_line();
    if (!result.has_value()) {
        return std::unexpected(parser.consume_errors());
    }

    return result.value();
}

auto parse_program(const std::span<const std::string> lines) -> std::expected<as::parser::Program, std::vector<sim::Error>> {
    auto program = as::parser::Program{};
    auto errors = std::vector<sim::Error>{};
 
    std::optional<std::uint32_t> block_count{};
    std::optional<std::uint32_t> warp_count{};

    auto line_nr = 0u;
    auto instr_count = 0u;

    auto add_label = [&](const std::string_view label_name) {
        if (program.label_mappings.contains(label_name)) {
            auto previous_declaration_line = program.label_mappings[label_name];
            errors.emplace_back(std::format("Duplicate label declaration on line {}", line_nr), 0, line_nr);
        } else {
            program.label_mappings[label_name] = instr_count;
        }
    };


    for(const auto& line : lines) {
        line_nr++;

        // Tokenize
        auto [tokens, lexer_errors] = as::collect_tokens(line);
        if (!lexer_errors.empty()) {
            for (auto error : lexer_errors) {
                errors.push_back(error.with_line(line_nr));
            }
        }

        // Skip empty lines
        if (tokens.empty()) {
            continue;
        }

        const auto output = as::parse_line(tokens);
        if(!output.has_value()) {
            for (auto err : output.error()) {
                errors.push_back(err.with_line(line_nr));
            }
            continue;
        }

        const auto& val = output.value();
        std::visit(as::overloaded{
                [&](const as::parser::JustLabel& label) {
                    add_label(label.label.name);
                },
                [&](const as::parser::Instruction& instr) {
                    program.instructions.push_back(instr);
                    if (instr.label.has_value()) {
                        add_label(instr.label->name);
                    }
                    instr_count++;
                },
                [&](const as::parser::BlocksDirective& block) {
                    if(block_count.has_value()) {
                        errors.emplace_back("Duplicate blocks directive", 0, line_nr);
                    }
                    block_count = block.number;
                },
                [&](const as::parser::WarpsDirective& warp) {
                    if(warp_count.has_value()) {
                        errors.emplace_back("Duplicate warps directive", 0, line_nr);
                    }
                    warp_count = warp.number;
                },
        }, val);
    }

    program.blocks = block_count.value_or(1);
    program.warps = warp_count.value_or(1);

    if (!errors.empty()) {
        return std::unexpected{errors};
    }

    return program;
}

}
