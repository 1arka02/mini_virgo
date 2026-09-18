#pragma once
#include <print>
#include <string_view>
#include <string>
#include <cassert>
#include <expected>

namespace sim {

struct Error {
    std::string message;
    std::uint32_t column;
    std::uint32_t line;

    explicit Error(std::string message, std::uint32_t column = 0, std::uint32_t line = 0) :
        message(std::move(message)), column(column), line(line) {}

    auto with_column(std::uint32_t col) -> Error& {
        column = col;
        return *this;
    }

    auto with_line(std::uint32_t ln) -> Error& {
        line = ln;
        return *this;
    }

    auto operator==(const Error& other) const -> bool {
        return message == other.message && column == other.column && line == other.line;
    }

    auto operator!=(const Error& other) const -> bool {
        return !(*this == other);
    }
};

inline void print_error(const Error& error) {
    std::println(stderr, "Error:{}:{}: {}.", error.line, error.column, error.message);
    assert(false);
}

#ifndef NDEBUG
inline void assert_or_err(bool condition, const Error& error) {
    if (!condition) {
        print_error(error);
    }
}
#else
inline void assert_or_err(bool condition, const std::string_view message) {
    // no-op in release
}
#endif

template<typename T, typename E>
auto unwrap(std::expected<T, E>&& expected) -> T {
    if (!expected.has_value()) {
        std::println(stderr, "Error: {}.", expected.error());
        std::exit(1);
    }
    return std::move(*expected);
}
}
