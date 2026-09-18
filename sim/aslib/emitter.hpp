#pragma once

#include "instructions.hpp"
#include "parser.hpp"

namespace as {
    auto translate_to_binary(const parser::Program& program) -> std::vector<sim::InstructionBits>;
}
