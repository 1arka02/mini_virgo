#include "Vgpu_gpu.h"
#include <span>
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>
#include "sim.hpp"
#include "instructions.hpp"

using namespace sim::instructions;

constexpr auto INST_NUM_CHANNELS = Vgpu_gpu::INSTRUCTION_MEM_NUM_CHANNELS;
constexpr auto DATA_NUM_CHANNELS = Vgpu_gpu::DATA_MEM_NUM_CHANNELS;
constexpr auto MAX_CYCLES = 10000;

auto run_sim(std::span<const sim::InstructionBits> instructions, std::span<const IData> data = {}, uint32_t num_blocks = 1, uint32_t num_warps_per_block = 1) -> sim::DataMemory<DATA_NUM_CHANNELS> {
    auto gpu = Vgpu{};

    auto instruction_memory = sim::make_instruction_memory<INST_NUM_CHANNELS>(&gpu);
    auto data_memory = sim::make_data_memory<DATA_NUM_CHANNELS>(&gpu);

    for (const auto &datum : data) {
        data_memory.push_data(datum);
    }

    for (const auto &instruction : instructions) {
        instruction_memory.push_instruction(instruction);
    }

    sim::set_kernel_config(gpu, 0, 0, num_blocks, num_warps_per_block);

    auto done = sim::simulate(gpu, instruction_memory, data_memory, MAX_CYCLES);

    REQUIRE(done);

    return data_memory;
}

TEST_CASE("mov + sw + halt") {

    auto data_memory = run_sim(std::array{
            addi(5_x, 1_x, 0),
            sw(5_x, 1_x, 0),
            halt()
            });

    for(auto i = 0; i < 32; i++) {
        CHECK(data_memory[i] == i);
    }
}

TEST_CASE("lw + sw") {
    auto data_memory = run_sim(
        std::array{
            lw(6_x, 0_x, 0),
            sw(1_x, 6_x, 0),
            halt()
        },
        std::array<IData, 3>{10, 20, 30}
    );

    for (auto i = 0; i < 32; i++) {
        CHECK(data_memory[i] == 10);
    }
}

TEST_CASE("add") {
    auto data_memory = run_sim(
        std::array{
            lw(6_x, 0_x, 0),
            lw(5_x, 0_x, 1),
            add(7_x, 6_x, 5_x),
            sw(1_x, 7_x, 0),
            halt()
        },
        std::array<IData, 2>{10, 20}
    );

    for (auto i = 0; i < 32; i++) {
        CHECK(data_memory[i] == 30);
    }
}

TEST_CASE("mask") {
    constexpr auto mem_cells_count = 2048;

    auto mask_instruction = lw(1_x, 0_x, 0);
    mask_instruction.bits |= 1 << 6;

    auto data_memory = run_sim(
        std::array{
            mask_instruction,
            addi(5_x, 1_x, 0),
            sw(5_x, 1_x, 0),
            halt()
        },
        std::array<IData, 1>{IData{1} << 2}
    );

    CHECK(data_memory[0] == 4);
    for (auto i = 1; i < 32; i++) {
        if (i == 2) {
            CHECK(data_memory[i] == 2);
        } else {
            CHECK(data_memory[i] == 0);
        }
    }
}

TEST_CASE("sx_slti") {
    auto data_memory = run_sim(
        std::array{
            addi(5_x, 1_x, 0),
            sx_slti(1_s, 5_x, 5),
            sw(5_x, 1_x, 0),
            halt()
        }
    );

    for (auto i = 0; i < 32; i++) {
        if (i < 5) {
            CHECK(data_memory[i] == i);
        } else {
            CHECK(data_memory[i] == 0);
        }
    }
}

TEST_CASE("lui + addi + sw") {
    auto data_memory = run_sim(
        std::array{
            lui(5_x, 1),
            addi(5_x, 5_x, 87),
            sw(0_x, 5_x, 0),
            halt()
        }
    );

    CHECK(data_memory[0] == (1 << 12) + 87);
}
