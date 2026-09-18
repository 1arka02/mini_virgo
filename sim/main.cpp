#include <Vgpu.h>
#include <print>
#include <fstream>
#include <expected>
#include "common.hpp"
#include "data_reader.hpp"
#include "emitter.hpp"
#include "parser.hpp"
#include "error.hpp"
#include "sim.hpp"
#include <vector>
#include <string_view>

auto main(int argc, char** argv) -> int {
    if (argc < 2 || argc > 3) {
        std::println("Usage: {} <input file> [data file]", argv[0]);
        return 1;
    }

    const std::string_view input_filename = argv[1];
    auto data = std::optional<sim::data_memory_container_t>{};
    if (argc == 3) {
        auto data_or_error = as::read_data(argv[2]);

        if (!data_or_error) {
            std::println(stderr, "Failed to read data file '{}': {}", argv[2], data_or_error.error());
            return 1;
        }

        data = data_or_error.value();
    }

    auto input_file = sim::unwrap(as::open_file(input_filename));
    const auto lines = as::get_lines(input_file);
    input_file.close();

    auto program_or_err = as::parse_program(lines);

    if (!program_or_err.has_value()) {
        for (const auto& error : program_or_err.error()) {
            sim::print_error(error);
        }
        return 1;
    }

    const auto& [blocks, warps, instructions, label_mappings] = program_or_err.value();

    std::println("\nSuccesfully parsed the entire file.");
    std::println("Warps: {}, Blocks: {}", warps, blocks);
    std::println("Parsed {} instructions:", instructions.size());
    auto i = 0u;
    for (const auto& instr : instructions) {
        std::println("{:3}: {}", i, instr.to_str());
        i++;
    }

    std::println("Labels:");
    i = 0u;
    for (const auto& [label, line] : label_mappings) {
        std::println("{:3}: {}: {}", i, label, line);
        i++;
    }

    const auto machine_code = as::translate_to_binary(*program_or_err);
    Vgpu top{};

    constexpr auto num_channels = 8;
    auto data_mem = sim::make_data_memory<num_channels>(&top);
    if (data.has_value()) {
        data_mem.memory = data.value();
    }
    auto instruction_mem = sim::make_instruction_memory<num_channels>(&top);

    for (auto i = 0u; i < machine_code.size(); i++) {
        instruction_mem.memory[i] = machine_code[i].bits;
    }

    sim::set_kernel_config(top, 0, 0, blocks, warps);

    auto done = sim::simulate(top, instruction_mem, data_mem, 100000);

    if(!done) {
        std::println("Simulation didn't finish before the max operation limit!");
        return 1;
    }

    // Optionally, print data memory content
    data_mem.print_memory();

    return 0;
}
