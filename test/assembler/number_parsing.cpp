#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "parser_utils.hpp"
#include "common.hpp"
#include <string_view>
#include <expected>

TEST_CASE("parse_num - Decimal Numbers") {
    std::string_view input;

    SUBCASE("Positive Decimal") {
        input = "42";
        auto result = as::parse_num(input);
        REQUIRE(result.has_value());
        CHECK(*result == 42);
        CHECK(input.empty());
    }

    SUBCASE("Negative Decimal") {
        input = "-42";
        auto result = as::parse_num(input);
        REQUIRE(result.has_value());
        CHECK(*result == -42);
        CHECK(input.empty());
    }

    SUBCASE("Trailing Characters") {
        input = "123abc";
        auto result = as::parse_num(input);
        REQUIRE_FALSE(result.has_value());
        CHECK(input == "abc");
    }

    SUBCASE("Empty Input") {
        input = "";
        auto result = as::parse_num(input);
        REQUIRE_FALSE(result.has_value());
    }

    SUBCASE("Only Negative Sign") {
        input = "-";
        auto result = as::parse_num(input);
        REQUIRE_FALSE(result.has_value());
    }
}

TEST_CASE("parse_num - Hexadecimal Numbers") {
    std::string_view input;

    SUBCASE("Positive Hexadecimal") {
        input = "0x2A";
        auto result = as::parse_num(input);
        REQUIRE(result.has_value());
        CHECK(*result == 42);
        CHECK(input.empty());
    }

    SUBCASE("Negative Hexadecimal") {
        input = "-0x2A";
        auto result = as::parse_num(input);
        REQUIRE(result.has_value());
        CHECK(*result == -42);
        CHECK(input.empty());
    }

    SUBCASE("Invalid Hexadecimal") {
        input = "0xZZ";
        auto result = as::parse_num(input);
        REQUIRE_FALSE(result.has_value());
    }
}

TEST_CASE("parse_num - Binary Numbers") {
    std::string_view input;

    SUBCASE("Positive Binary") {
        input = "0b101010";
        auto result = as::parse_num(input);
        REQUIRE(result.has_value());
        CHECK(*result == 42);
        CHECK(input.empty());
    }

    SUBCASE("Negative Binary") {
        input = "-0b101010";
        auto result = as::parse_num(input);
        REQUIRE(result.has_value());
        CHECK(*result == -42);
        CHECK(input.empty());
    }

    SUBCASE("Invalid Binary") {
        input = "0b1201";
        auto result = as::parse_num(input);
        REQUIRE_FALSE(result.has_value());
    }
}

TEST_CASE("parse_num - Octal Numbers") {
    std::string_view input;

    SUBCASE("Positive Octal") {
        input = "052";
        auto result = as::parse_num(input);
        REQUIRE(result.has_value());
        CHECK(*result == 42);
        CHECK(input.empty());
    }

    SUBCASE("Negative Octal") {
        input = "-052";
        auto result = as::parse_num(input);
        REQUIRE(result.has_value());
        CHECK(*result == -42);
        CHECK(input.empty());
    }

    SUBCASE("Invalid Octal") {
        input = "09";
        auto result = as::parse_num(input);
        REQUIRE_FALSE(result.has_value());
    }
}

TEST_CASE("parse_num - Edge Cases") {
    std::string_view input;

    SUBCASE("Maximum 32-bit Integer") {
        input = "2147483647";
        auto result = as::parse_num(input);
        REQUIRE(result.has_value());
        CHECK(*result == 2147483647);
        CHECK(input.empty());
    }

    SUBCASE("Minimum 32-bit Integer") {
        input = "-2147483648";
        auto result = as::parse_num(input);
        REQUIRE_FALSE(result.has_value());
    }

    SUBCASE("Out of Range (Positive)") {
        input = "2147483648"; // Greater than INT32_MAX
        auto result = as::parse_num(input);
        REQUIRE_FALSE(result.has_value());
    }

    SUBCASE("Out of Range (Negative)") {
        input = "-2147483649"; // Less than INT32_MIN
        auto result = as::parse_num(input);
        REQUIRE_FALSE(result.has_value());
    }
}

TEST_CASE("parse_num - Invalid Inputs") {
    std::string_view input;

    SUBCASE("Empty Input") {
        input = "";
        auto result = as::parse_num(input);
        REQUIRE_FALSE(result.has_value());
    }

    SUBCASE("Floating Point Number") {
        input = "42.0";
        auto result = as::parse_num(input);
        REQUIRE(result.has_value());
        CHECK(*result == 42);
        CHECK(input == ".0");
    }

    SUBCASE("Scientific Notation") {
        input = "1e3";
        auto result = as::parse_num(input);
        REQUIRE_FALSE(result.has_value());
    }

    SUBCASE("Invalid Characters") {
        input = "abc";
        auto result = as::parse_num(input);
        REQUIRE_FALSE(result.has_value());
    }
}
