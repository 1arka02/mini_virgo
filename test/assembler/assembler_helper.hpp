#pragma once
#include "doctest.h"
#include "instructions.hpp"
#include "token.hpp"

inline void check_reg(const as::Token& token, sim::RegisterType type, std::uint32_t reg_num) {
    const auto data = sim::Register{.register_number = reg_num, .type = type};
    REQUIRE(token.is_of_type<as::token::Register>());
    REQUIRE_EQ(token.as<as::token::Register>().register_data, data);
}

inline void check_label(const as::Token& token, std::string_view label_name) {
    REQUIRE(token.is_of_type<as::token::Label>());
    REQUIRE_EQ(token.as<as::token::Label>().name, label_name);
}

inline void check_label_ref(const as::Token& token, std::string_view label_name) {
    REQUIRE(token.is_of_type<as::token::LabelRef>());
    REQUIRE_EQ(token.as<as::token::LabelRef>().label_name, label_name);
}

inline void check_immediate(const as::Token& token, std::int32_t value) {
    REQUIRE(token.is_of_type<as::token::Immediate>());
    REQUIRE_EQ(token.as<as::token::Immediate>().value, value);
}

inline void check_mnemonic(const as::Token& token, const std::string_view mnemonic) {
    REQUIRE(token.is_of_type<as::token::Mnemonic>());
    auto m = sim::str_to_mnemonic(mnemonic);
    REQUIRE(m.has_value());
    REQUIRE_EQ(token.as<as::token::Mnemonic>().mnemonic, *m);
}
