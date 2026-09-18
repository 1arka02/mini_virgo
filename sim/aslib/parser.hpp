#pragma once

#include <span>
#include "instructions.hpp"
#include "token.hpp"
#include "error.hpp"
#include <expected>

namespace as {

namespace parser {

struct WarpsDirective {
    std::uint32_t number;
};

struct BlocksDirective {
    std::uint32_t number;
};

// FIXME: This is hacky since we can't access the column number just from the token type,
//        technically we should be getting a full Token struct here
using ImmediateOrLabelref = std::variant<token::Immediate, token::LabelRef>;

auto to_string(const ImmediateOrLabelref &imm) -> std::string;

struct ItypeOperands {
    sim::Register rd;
    sim::Register rs1;
    token::Immediate imm12;
};

struct RtypeOperands {
    sim::Register rd;
    sim::Register rs1;
    sim::Register rs2;
};

struct StypeOperands {
    sim::Register rs1;
    sim::Register rs2;
    token::Immediate imm12;
};

struct UtypeOperands {
    sim::Register rd;
    token::Immediate imm20;
};

struct JtypeOperands {
    sim::Register rd;
    token::Immediate imm20;
};

struct JalrOperands {
    sim::Register rd;
    sim::Register rs1;
    ImmediateOrLabelref immediate_or_label_ref;
};

/*using Operands = std::variant<Rtype, Itype, Load, Store, Branch, Jump, Utype, Sx>;*/
using Operands = std::variant<ItypeOperands, RtypeOperands, StypeOperands, UtypeOperands, JtypeOperands, JalrOperands>;

constexpr auto is_itype_arithmetic(sim::MnemonicName name) -> bool {
    return name == sim::MnemonicName::ADDI || name == sim::MnemonicName::SLTI || name == sim::MnemonicName::XORI ||
           name == sim::MnemonicName::ORI || name == sim::MnemonicName::ANDI || name == sim::MnemonicName::SLLI ||
           name == sim::MnemonicName::SRLI || name == sim::MnemonicName::SRAI || name == sim::MnemonicName::SX_SLTI;
}

constexpr auto is_rtype(sim::MnemonicName name) -> bool {
    return name == sim::MnemonicName::ADD || name == sim::MnemonicName::SUB || name == sim::MnemonicName::SLL ||
           name == sim::MnemonicName::SLT || name == sim::MnemonicName::XOR || name == sim::MnemonicName::SRL ||
           name == sim::MnemonicName::SRA || name == sim::MnemonicName::OR || name == sim::MnemonicName::AND ||
           name == sim::MnemonicName::SX_SLT;
}

constexpr auto is_load_type(sim::MnemonicName name) -> bool {
    return name == sim::MnemonicName::LB || name == sim::MnemonicName::LH || name == sim::MnemonicName::LW;
}

constexpr auto is_store_type(sim::MnemonicName name) -> bool {
    return name == sim::MnemonicName::SB || name == sim::MnemonicName::SH || name == sim::MnemonicName::SW;
}

constexpr auto is_utype(sim::MnemonicName name) -> bool {
    return name == sim::MnemonicName::LUI || name == sim::MnemonicName::AUIPC;
}

struct Instruction {
    std::optional<as::token::Label> label;
    sim::Mnemonic mnemonic;
    Operands operands;

    [[nodiscard]] auto to_str() const -> std::string {
          std::string result;
          if (label.has_value()) {
              result += std::format("{}: ", label->name);
          }
          result += mnemonic.to_str() + " ";
          std::visit(
              overloaded{
                  [&](const parser::ItypeOperands &operands) {
                      if (is_load_type(mnemonic.get_name())) {
                          result += operands.rd.to_str() + ", " + std::to_string(operands.imm12.value) + "(" +
                                    operands.rs1.to_str() + ")";
                      } else {
                          result += operands.rd.to_str() + ", " + operands.rs1.to_str() + ", " +
                                    to_string(operands.imm12);
                      }
                  },
                  [&result](const parser::RtypeOperands &operands) {
                      result += operands.rd.to_str() + ", " + operands.rs1.to_str() + ", " +
                                operands.rs2.to_str();
                  },
                  [&result](const parser::StypeOperands &operands) {
                      result += operands.rs2.to_str() + ", " + std::to_string(operands.imm12.value) + "(" +
                                operands.rs1.to_str() + ")";
                  },
                  [&result](const parser::UtypeOperands &operands) {
                      result += operands.rd.to_str() + ", " + std::to_string(operands.imm20.value);
                  },
                  /*[&result](const parser::BtypeOperands &operands) {*/
                  /*    result += operands.rs1.to_str() + ", " + operands.rs2.to_str() + ", " +*/
                  /*              std::to_string(operands.imm12);*/
                  /*},*/
                  [&result](const parser::JtypeOperands &operands) {
                      result += operands.rd.to_str() + ", " + std::to_string(operands.imm20.value);
                  },
                  [&result](const parser::JalrOperands &operands) {
                      result += operands.rd.to_str() + ", ";
                      if (std::holds_alternative<token::LabelRef>(operands.immediate_or_label_ref)) {
                          result += std::get<token::LabelRef>(operands.immediate_or_label_ref).label_name;
                          return;
                      }
                      const auto& immediate = std::get<token::Immediate>(operands.immediate_or_label_ref);
                      result += std::to_string(immediate.value) + "(" + operands.rs1.to_str() + ")";
                  },

                  /*[&result](const parser::UxtypeOperands &operands) {*/
                  /*    result += operands.rd.to_str() + ", " + std::to_string(operands.imm20);*/
                  /*},*/
              },
              operands);
          return result;
    }
};

struct JustLabel{
    as::token::Label label;
};

using Line = std::variant<JustLabel, WarpsDirective, BlocksDirective, Instruction>;
auto line_to_str(const Line &line) -> std::string;

struct Program {
    std::uint32_t blocks{};
    std::uint32_t warps{};
    std::vector<Instruction> instructions;
    std::unordered_map<std::string_view, std::uint32_t> label_mappings;
};

}

// The parsing function return nulloptr if an error has occured and push the error to
// the errors vector, otherwise they return the parsed value
class Parser {
  public:
    using Result = parser::Line;
    using Error = sim::Error;

    Parser() = delete;
    explicit Parser(const std::span<as::Token> &tokens) : tokens(tokens) {}

    auto chop() -> std::optional<as::Token>;
    [[nodiscard]] auto peek() const -> const as::Token *;

    template <typename ...T>
    auto expect() -> std::optional<as::Token> {
        auto next = chop();
        if(!next.has_value()) {
            throw_unexpected_eos(as::token::token_type_to_str<T...>());
            return std::nullopt;
        }

        if(!next->is_of_type<T...>()) {
            throw_unexpected_token(as::token::token_type_to_str<T...>(), *next);
            return std::nullopt;
        }

        return next;
    }

    auto parse_line() -> std::optional<Result>;
    auto parse_directive() -> std::optional<Result>;
    auto parse_instruction() -> std::optional<Result>;

    auto parse_rtype_instruction(const sim::Mnemonic& mnemonic) -> std::optional<parser::Instruction>;
    auto parse_itype_arithemtic_instruction(const sim::Mnemonic &mnemonic) -> std::optional<parser::Instruction>;
    auto parse_load_instruction(const sim::Mnemonic &mnemonic) -> std::optional<parser::Instruction>;
    auto parse_store_instruction(const sim::Mnemonic &mnemonic) -> std::optional<parser::Instruction>;
    auto parse_branch_instruction(const sim::Mnemonic &mnemonic) -> std::optional<parser::Instruction>;
    auto parse_utype_instruction(const sim::Mnemonic &mnemonic) -> std::optional<parser::Instruction>;
    auto parse_jal_instruction(const sim::Mnemonic& mnemonic) -> std::optional<parser::Instruction>;
    auto parse_jalr_instruction(const sim::Mnemonic& mnemonic) -> std::optional<parser::Instruction>;

    void push_err(Error &&err);
    void push_err(std::string &&message, unsigned column);
    void throw_unexpected_token(std::string &&expected, const Token &unexpected);
    void throw_unexpected_eos(std::string &&expected);
    auto get_errors() -> std::span<Error> { return errors; }
    auto consume_errors() -> std::vector<Error> { return std::move(errors); }

  private:

    auto check_register_correct_type(const Token &reg_token, bool should_be_scalar) -> bool;
    std::span<Token> tokens;
    std::vector<sim::Error> errors;
};

auto parse_line(std::span<Token> tokens) -> std::expected<Parser::Result, std::vector<Parser::Error>>;
auto parse_program(const std::span<const std::string> lines) -> std::expected<as::parser::Program, std::vector<sim::Error>>;
} // namespace as
