#include "lexer.hpp"
#include "parser.hpp"
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

TEST_CASE("Lexing directives") {
    std::string_view input;

    SUBCASE("Blocks Directive: correct input") {
        input = ".blocks";
        const auto [tokens, errors] = as::collect_tokens(input);

        REQUIRE_EQ(tokens.size(), 1);
        REQUIRE_EQ(errors.size(), 0);
        REQUIRE(tokens[0].is_of_type<as::token::BlocksDirective>());
    }

    SUBCASE("Blocks Directive: incorrect input") {
        input = ".block";
        const auto [tokens, errors] = as::collect_tokens(input);

        REQUIRE_EQ(tokens.size(), 0);
        REQUIRE_EQ(errors.size(), 1);
    }

    SUBCASE("Warps Directive: correct input") {
        input = ".warps";
        const auto [tokens, errors] = as::collect_tokens(input);

        REQUIRE_EQ(tokens.size(), 1);
        REQUIRE_EQ(errors.size(), 0);
        REQUIRE(tokens[0].is_of_type<as::token::WarpsDirective>());
    }

    SUBCASE("Warps Directive: incorrect input") {
        input = ".warp";
        const auto [tokens, errors] = as::collect_tokens(input);

        REQUIRE_EQ(tokens.size(), 0);
        REQUIRE_EQ(errors.size(), 1);
    }
}

TEST_CASE("Parsing directives") {
    std::string_view input;

    SUBCASE("Blocks Directive: correct input") {
        input = ".blocks 42";
        auto [tokens, errors] = as::collect_tokens(input);
        REQUIRE(errors.empty());
        const auto result = as::parse_line(tokens);

        REQUIRE(result.has_value());
        REQUIRE(std::holds_alternative<as::parser::BlocksDirective>(result.value()));
        CHECK(std::get<as::parser::BlocksDirective>(result.value()).number == 42);
    }

    SUBCASE("Blocks Directive: incorrect input: no number provided") {
        input = ".blocks";
        auto [tokens, errors] = as::collect_tokens(input);
        auto result = as::parse_line(tokens);

        REQUIRE_FALSE(result.has_value());
    }

    SUBCASE("Blocks Directive: incorrect input: negative number provided") {
        input = ".blocks -42";
        auto [tokens, errors] = as::collect_tokens(input);
        auto result = as::parse_line(tokens);

        REQUIRE_FALSE(result.has_value());
    }

    SUBCASE("Blocks Directive: incorrect input: invalid number provided") {
        input = ".blocks 42.0";
        auto [tokens, errors] = as::collect_tokens(input);
        REQUIRE_FALSE(errors.empty());
    }

    SUBCASE("Warps Directive: correct input") {
        input = ".warps 42";
        auto [tokens, errors] = as::collect_tokens(input);
        REQUIRE(errors.empty());
        auto result = as::parse_line(tokens);

        REQUIRE(result.has_value());
        REQUIRE(std::holds_alternative<as::parser::WarpsDirective>(result.value()));
        CHECK(std::get<as::parser::WarpsDirective>(result.value()).number == 42);
    }

    SUBCASE("Warps Directive: incorrect input: no number provided") {
        input = ".warps";
        auto [tokens, errors] = as::collect_tokens(input);
        auto result = as::parse_line(tokens);

        REQUIRE_FALSE(result.has_value());
    }

    SUBCASE("Warps Directive: incorrect input: negative number provided") {
        input = ".warps -42";
        auto [tokens, errors] = as::collect_tokens(input);
        auto result = as::parse_line(tokens);

        REQUIRE_FALSE(result.has_value());
    }

    SUBCASE("Warps Directive: incorrect input: invalid number provided") {
        input = ".warps 42.0";
        auto [tokens, errors] = as::collect_tokens(input);
        REQUIRE_FALSE(errors.empty());
    }

    SUBCASE("Invalid Directive: incorrect input") {
        input = ".invalid";
        auto [tokens, errors] = as::collect_tokens(input);
        auto result = as::parse_line(tokens);

        REQUIRE_FALSE(result.has_value());
    }

    SUBCASE("Invalid Directive: Tokens after directive") {
        input = ".blocks 42 .warps 42";
        auto [tokens, errors] = as::collect_tokens(input);
        auto result = as::parse_line(tokens);

        REQUIRE_FALSE(result.has_value());
    }

    SUBCASE("Invalid Directive: Tokens before directive") {
        input = "42 .blocks 42";
        auto [tokens, errors] = as::collect_tokens(input);
        auto result = as::parse_line(tokens);

        REQUIRE_FALSE(result.has_value());
    }
}
