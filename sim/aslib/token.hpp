#pragma once
#include "common.hpp"
#include "instructions.hpp"
#include <string_view>
#include <variant>
#include <cstdint>
#include <verilated.h>

namespace as {

#define DEFINE_TOKEN_TYPE(name, ...)                                                                                   \
    struct name {                                                                                                      \
        __VA_ARGS__                                                                                                    \
        auto operator==(const name&) const -> bool = default;                                                    \
        auto operator!=(const name&) const -> bool = default;                                                    \
    };

namespace token {
DEFINE_TOKEN_TYPE(BlocksDirective)
DEFINE_TOKEN_TYPE(WarpsDirective)
DEFINE_TOKEN_TYPE(Mnemonic, sim::Mnemonic mnemonic;)
DEFINE_TOKEN_TYPE(Label, std::string_view name;)
DEFINE_TOKEN_TYPE(LabelRef, std::string_view label_name;)
DEFINE_TOKEN_TYPE(Immediate, std::int32_t value;)
DEFINE_TOKEN_TYPE(Register, sim::Register register_data;)
DEFINE_TOKEN_TYPE(Comma);
DEFINE_TOKEN_TYPE(Lparen);
DEFINE_TOKEN_TYPE(Rparen);

using TokenType = std::variant<BlocksDirective, WarpsDirective, Mnemonic, Label, LabelRef, Immediate, Register, Comma, Lparen, Rparen>;

template<typename T, typename... Ts>
inline auto token_type_to_str() -> std::string {
    if constexpr (sizeof...(Ts) == 0) {
        if constexpr (std::is_same_v<T, BlocksDirective>) {
            return ".blocks";
        } else if constexpr (std::is_same_v<T, WarpsDirective>) {
            return ".warps";
        } else if constexpr (std::is_same_v<T, Mnemonic>) {
            return "mnemonic";
        } else if constexpr (std::is_same_v<T, Label>) {
            return "label";
        } else if constexpr (std::is_same_v<T, LabelRef>) {
            return "label_ref";
        } else if constexpr (std::is_same_v<T, Immediate>) {
            return "immediate";
        } else if constexpr (std::is_same_v<T, Register>) {
            return "register";
        } else if constexpr (std::is_same_v<T, Comma>) {
            return "','";
        } else if constexpr (std::is_same_v<T, Lparen>) {
            return "'('";
        } else if constexpr (std::is_same_v<T, Rparen>) {
            return "')'";
        }
        return "unknown";
    } else {
        return token_type_to_str<T>() + " or " + token_type_to_str<Ts...>();
    }
}
}

struct Token {
    token::TokenType token_type;
    std::uint32_t col;

    auto operator==(const Token &other) const -> bool {
        return token_type == other.token_type && col == other.col;
    }

    template<typename ...T>
    [[nodiscard]] auto is_of_type() const -> bool {
        return (std::holds_alternative<T>(token_type) || ...);
    }

    template<typename T>
    [[nodiscard]] auto as() -> T & {
        return std::get<T>(token_type);
    }

    template<typename T>
    [[nodiscard]] auto as() const -> const T & {
        return std::get<T>(token_type);
    }

    [[nodiscard]] auto to_str() const -> std::string {
        return std::visit(overloaded{
                          [](const token::BlocksDirective &) -> std::string { return ".threads"; },
                          [](const token::WarpsDirective &) -> std::string { return ".warps"; },
                          [](const token::Mnemonic &m) -> std::string { return m.mnemonic.to_str(); },
                          [](const token::Label &l) -> std::string { return std::string(l.name); },
                          [](const token::LabelRef &lr) -> std::string { return std::string(lr.label_name); },
                          [](const token::Immediate &i) -> std::string { return std::to_string(i.value); },
                          [](const token::Register &r) -> std::string { return std::format("{}", r.register_data.to_str()); },
                          [](const token::Comma &) -> std::string { return "','"; },
                          [](const token::Lparen &) -> std::string { return "'('"; },
                          [](const token::Rparen &) -> std::string { return "')'"; },
                          },
                      token_type);
    }
};

}
