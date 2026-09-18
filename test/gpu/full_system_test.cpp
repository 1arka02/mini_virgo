// The test directory should be passed in the TESTS_DIR variable
// For each test there should be files:
// <test_name>.as - the assembly file with test code
// <test_name>.expected - the expected data memory state after the test; Should be in the data reader memory format (see sim/aslib/data_reader.hpp)
// <test_name>.data (optional) - the data loaded into the data memory; Should be in the data reader memory format (see sim/aslib/data_reader.hpp)

#include "Vgpu_gpu.h"
#include "common.hpp"
#include "data_reader.hpp"
#include "emitter.hpp"
#include "parser.hpp"
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>
#include "sim.hpp"
#include <filesystem>
namespace fs = std::filesystem;

constexpr auto INST_NUM_CHANNELS = Vgpu_gpu::INSTRUCTION_MEM_NUM_CHANNELS;
constexpr auto DATA_NUM_CHANNELS = Vgpu_gpu::DATA_MEM_NUM_CHANNELS;
constexpr auto MAX_CYCLES = 10000u;

TEST_CASE("Full system test") {
#ifndef TESTS_DIR
    FAIL("TESTS_DIR not defined, please pass, as a variable, the directory with tests in the format specified at the beginning of this test file.");
#endif

    const auto test_dir = fs::path{TESTS_DIR};
    REQUIRE(fs::exists(test_dir));

    auto test_names = std::unordered_set<std::string>{};
    for (const auto & entry : fs::directory_iterator(TESTS_DIR)) {
        const auto file_name = entry.path().filename().string();
        //split at '.' and take the first part
        const auto test_name = file_name.substr(0, file_name.find('.'));
        test_names.insert(test_name);
    }

    for (const auto test_name : test_names) {
        SUBCASE(std::format("Test: {}", test_name).c_str()) {
            const auto as_file = test_dir / (test_name + ".as");
            const auto expected_file = test_dir / (test_name + ".expected");
            const auto data_file = test_dir / (test_name + ".data");

            REQUIRE(fs::exists(as_file));
            REQUIRE(fs::exists(expected_file));
            const auto expected_data_mem = as::read_data(expected_file);
            REQUIRE(expected_data_mem.has_value());

            auto gpu = Vgpu{};

            auto data_mem = sim::make_data_memory<DATA_NUM_CHANNELS>(&gpu);
            if (fs::exists(data_file)) {
                const auto data = as::read_data(data_file);
                REQUIRE(data.has_value());
                data_mem.memory = *data;
            }

            auto instruction_mem = sim::make_instruction_memory<INST_NUM_CHANNELS>(&gpu);

            auto input_file = as::open_file(as_file);
            REQUIRE(input_file.has_value());
            const auto lines = as::get_lines(*input_file);
            input_file->close();

            auto program_or_err = as::parse_program(lines);
            REQUIRE(program_or_err.has_value());

            const auto& [blocks, warps, instructions, label_mappings] = program_or_err.value();
            const auto machine_code = as::translate_to_binary(*program_or_err);

            for (auto i = 0u; i < machine_code.size(); i++) {
                instruction_mem.memory[i] = machine_code[i].bits;
            }

            sim::set_kernel_config(gpu, 0, 0, blocks, warps);

            auto done = sim::simulate(gpu, instruction_mem, data_mem, MAX_CYCLES);

            if(!done) {
                FAIL(std::format("Simulation did not finish after {} cycles", MAX_CYCLES));
            }

            for(const auto [address, value] : *expected_data_mem) {
                CHECK(data_mem[address] == expected_data_mem->at(address));
            }
        }
    }
}
