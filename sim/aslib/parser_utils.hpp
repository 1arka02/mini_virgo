#pragma once

#include <string_view>
#include "common.hpp"
#include "instructions.hpp"
#include "error.hpp"

namespace as {

auto parse_num(std::string_view &source) -> std::expected<word_type, sim::Error>;
auto str_check_predicate(const std::string_view str, const std::function<bool(char)>& predicate) -> bool;
auto str_to_reg(std::string_view str) -> std::expected<sim::Register, sim::Error>;

}
