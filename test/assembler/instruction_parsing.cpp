#include "instructions.hpp"
#include "lexer.hpp"
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include <array>

TEST_CASE("Lexing instructions") {
    std::string_view input;

    const auto instructions = std::unordered_map<std::string_view, sim::MnemonicName>{
        {"lui", sim::MnemonicName::LUI},
        {"auipc", sim::MnemonicName::AUIPC},
        {"addi", sim::MnemonicName::ADDI},
        {"slti", sim::MnemonicName::SLTI},
        {"xori", sim::MnemonicName::XORI},
        {"ori", sim::MnemonicName::ORI},
        {"andi", sim::MnemonicName::ANDI},
        {"slli", sim::MnemonicName::SLLI},
        {"srli", sim::MnemonicName::SRLI},
        {"srai", sim::MnemonicName::SRAI},
        {"add", sim::MnemonicName::ADD},
        {"sub", sim::MnemonicName::SUB},
        {"sll", sim::MnemonicName::SLL},
        {"slt", sim::MnemonicName::SLT},
        {"xor", sim::MnemonicName::XOR},
        {"srl", sim::MnemonicName::SRL},
        {"sra", sim::MnemonicName::SRA},
        {"or", sim::MnemonicName::OR},
        {"and", sim::MnemonicName::AND},
        {"lb", sim::MnemonicName::LB},
        {"lh", sim::MnemonicName::LH},
        {"lw", sim::MnemonicName::LW},
        {"sb", sim::MnemonicName::SB},
        {"sh", sim::MnemonicName::SH},
        {"sw", sim::MnemonicName::SW},
        {"jal", sim::MnemonicName::JAL},
        {"jalr", sim::MnemonicName::JALR},
        {"beq", sim::MnemonicName::BEQ},
        {"bne", sim::MnemonicName::BNE},
        {"blt", sim::MnemonicName::BLT},
        {"bge", sim::MnemonicName::BGE},
        {"halt", sim::MnemonicName::HALT},
        {"sx.slt", sim::MnemonicName::SX_SLT},
        {"sx.slti", sim::MnemonicName::SX_SLTI}
    };

    for (const auto &[instr, mnemonic_name] : instructions) {
        SUBCASE(std::format("Instruction: {}", instr).data()) {
            input = instr;
            const auto [tokens, errors] = as::collect_tokens(input);

            REQUIRE_EQ(tokens.size(), 1);
            REQUIRE_EQ(errors.size(), 0);
            REQUIRE(tokens[0].is_of_type<as::token::Mnemonic>());
            REQUIRE_EQ(std::get<as::token::Mnemonic>(tokens[0].token_type).mnemonic.get_name(), mnemonic_name);
            auto m = sim::str_to_mnemonic(instr);
            REQUIRE(m.has_value());
            REQUIRE_EQ(std::get<as::token::Mnemonic>(tokens[0].token_type).mnemonic, *m);
        }
    }

    for (const auto &[instr, mnemonic_name] : instructions) {
        SUBCASE(std::format("Instruction: s.{}: Check scalar", instr).data()) {
            auto scalar_instr = std::format("s.{}", instr);
            std::println("Checking: {}", scalar_instr);
            input = scalar_instr;
            const auto [tokens, errors] = as::collect_tokens(input);

            REQUIRE_EQ(tokens.size(), 1);
            REQUIRE_EQ(errors.size(), 0);
            REQUIRE(tokens[0].is_of_type<as::token::Mnemonic>());
            REQUIRE(std::get<as::token::Mnemonic>(tokens[0].token_type).mnemonic.is_scalar());
            auto m = sim::str_to_mnemonic(scalar_instr);
            REQUIRE(m.has_value());
            REQUIRE_EQ(std::get<as::token::Mnemonic>(tokens[0].token_type).mnemonic, *m);
}
    }

}

TEST_CASE("Lexing labels") {
    std::string_view input;

    const auto labels = std::array{
        "label:"sv,
        "label123:"sv,
        "label_123:"sv,
        "Label:"sv
    };

    for (const auto &label : labels) {
        SUBCASE(std::format("Label: correct input: '{}'", label).data()) {
            input = label;
            const auto [tokens, errors] = as::collect_tokens(input);

            REQUIRE_EQ(tokens.size(), 1);
            REQUIRE_EQ(errors.size(), 0);
            REQUIRE(tokens[0].is_of_type<as::token::Label>());
            REQUIRE_EQ(std::get<as::token::Label>(tokens[0].token_type).name, label.substr(0, label.size() - 1));
        }
    }


}
