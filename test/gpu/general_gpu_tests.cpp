#include "Vgpu_gpu.h"
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>
#include "sim.hpp"

constexpr auto INST_NUM_CHANNELS = Vgpu_gpu::INSTRUCTION_MEM_NUM_CHANNELS;
constexpr auto DATA_NUM_CHANNELS = Vgpu_gpu::DATA_MEM_NUM_CHANNELS;

using namespace sim::instructions;

// ALU operation tests
TEST_CASE("ALU operations") {
    SUBCASE("sub") {
        auto top = Vgpu{};

        auto data_mem = sim::make_data_memory<8>(&top);
        auto instruction_mem = sim::make_instruction_memory<8>(&top);

        data_mem.push_data(50); // data_mem[0] = 50
        data_mem.push_data(20); // data_mem[1] = 20

        instruction_mem.push_instruction(lw(6_x, 0_x, 0)); // lw x6, 0(x0)
        instruction_mem.push_instruction(lw(5_x, 0_x, 1)); // lw x5, 1(x0)
        instruction_mem.push_instruction(sub(7_x, 6_x, 5_x)); // sub x7, x6, x5
        instruction_mem.push_instruction(sw(1_x, 7_x, 0));  // sw x7, 0(x1)
        instruction_mem.push_instruction(halt());

        // Prepare kernel configuration
        sim::set_kernel_config(top, 0, 0, 1, 1);

        // Run simulation
        auto done = simulate(top, instruction_mem, data_mem, 2000);

        REQUIRE(done);
        for(auto i = 0; i < 32; i++) {
            CHECK(data_mem[i] == 30);
        }
    }

    SUBCASE("and") {
        auto top = Vgpu{};

        auto data_mem = sim::make_data_memory<8>(&top);
        auto instruction_mem = sim::make_instruction_memory<8>(&top);

        data_mem.push_data(0b1100); // data_mem[0] = 12
        data_mem.push_data(0b1010); // data_mem[1] = 10

        instruction_mem.push_instruction(lw(6_x, 0_x, 0)); // lw x6, 0(x0)
        instruction_mem.push_instruction(lw(5_x, 0_x, 1)); // lw x5, 1(x0)
        instruction_mem.push_instruction(and_(7_x, 6_x, 5_x)); // and x7, x6, x5
        instruction_mem.push_instruction(sw(1_x, 7_x, 0));  // sw x7, 0(x1)
        instruction_mem.push_instruction(halt());

        // Prepare kernel configuration
        sim::set_kernel_config(top, 0, 0, 1, 1);

        // Run simulation
        auto done = simulate(top, instruction_mem, data_mem, 2000);

        REQUIRE(done);
        for(auto i = 0; i < 32; i++) {
            CHECK(data_mem[i] == 8);
        }
    }

    SUBCASE("or") {
        auto top = Vgpu{};

        auto data_mem = sim::make_data_memory<8>(&top);
        auto instruction_mem = sim::make_instruction_memory<8>(&top);

        data_mem.push_data(0b1100); // data_mem[0] = 12
        data_mem.push_data(0b1010); // data_mem[1] = 10

        instruction_mem.push_instruction(lw(6_x, 0_x, 0)); // lw x6, 0(x0)
        instruction_mem.push_instruction(lw(5_x, 0_x, 1)); // lw x5, 1(x0)
        instruction_mem.push_instruction(or_(7_x, 6_x, 5_x)); // or x7, x6, x5
        instruction_mem.push_instruction(sw(1_x, 7_x, 0));  // sw x7, 0(x1)
        instruction_mem.push_instruction(halt());

        // Prepare kernel configuration
        sim::set_kernel_config(top, 0, 0, 1, 1);

        // Run simulation
        auto done = simulate(top, instruction_mem, data_mem, 2000);

        REQUIRE(done);
        for(auto i = 0; i < 32; i++) {
            CHECK(data_mem[i] == 14);
        }
    }

    SUBCASE("xor") {
        auto top = Vgpu{};

        auto data_mem = sim::make_data_memory<8>(&top);
        auto instruction_mem = sim::make_instruction_memory<8>(&top);

        data_mem.push_data(0b1100); // data_mem[0] = 12
        data_mem.push_data(0b1010); // data_mem[1] = 10

        instruction_mem.push_instruction(lw(6_x, 0_x, 0)); // lw x6, 0(x0)
        instruction_mem.push_instruction(lw(5_x, 0_x, 1)); // lw x5, 1(x0)
        instruction_mem.push_instruction(xor_(7_x, 6_x, 5_x)); // xor x7, x6, x5
        instruction_mem.push_instruction(sw(1_x, 7_x, 0));   // sw x7, 0(x1)
        instruction_mem.push_instruction(halt());

        // Prepare kernel configuration
        sim::set_kernel_config(top, 0, 0, 1, 1);

        // Run simulation
        auto done = simulate(top, instruction_mem, data_mem, 2000);

        REQUIRE(done);
        for(auto i = 0; i < 32; i++) {
            CHECK(data_mem[i] == 6);
        }
    }

    SUBCASE("shift left logical") {
        auto top = Vgpu{};

        auto data_mem = sim::make_data_memory<8>(&top);
        auto instruction_mem = sim::make_instruction_memory<8>(&top);

        data_mem.push_data(1); // data_mem[0] = 1
        data_mem.push_data(3); // data_mem[1] = 3

        instruction_mem.push_instruction(lw(6_x, 0_x, 0)); // lw x6, 0(x0)
        instruction_mem.push_instruction(lw(5_x, 0_x, 1)); // lw x5, 1(x0)
        instruction_mem.push_instruction(sll(7_x, 6_x, 5_x)); // sll x7, x6, x5
        instruction_mem.push_instruction(sw(1_x, 7_x, 0));  // sw x7, 0(x1)
        instruction_mem.push_instruction(halt());

        // Prepare kernel configuration
        sim::set_kernel_config(top, 0, 0, 1, 1);

        // Run simulation
        auto done = simulate(top, instruction_mem, data_mem, 2000);

        REQUIRE(done);
        for(auto i = 0; i < 32; i++) {
            CHECK(data_mem[i] == 8);
        }
    }

    SUBCASE("shift right logical") {
        auto top = Vgpu{};

        auto data_mem = sim::make_data_memory<8>(&top);
        auto instruction_mem = sim::make_instruction_memory<8>(&top);

        data_mem.push_data(8); // data_mem[0] = 8
        data_mem.push_data(3); // data_mem[1] = 3

        instruction_mem.push_instruction(lw(6_x, 0_x, 0)); // lw x6, 0(x0)
        instruction_mem.push_instruction(lw(5_x, 0_x, 1)); // lw x5, 1(x0)
        instruction_mem.push_instruction(srl(7_x, 6_x, 5_x)); // srl x7, x6, x5
        instruction_mem.push_instruction(sw(1_x, 7_x, 0));  // sw x7, 0(x1)
        instruction_mem.push_instruction(halt());

        // Prepare kernel configuration
        sim::set_kernel_config(top, 0, 0, 1, 1);

        // Run simulation
        auto done = simulate(top, instruction_mem, data_mem, 2000);

        REQUIRE(done);
        for(auto i = 0; i < 32; i++) {
            CHECK(data_mem[i] == 1);
        }
    }

    SUBCASE("addi") {
        auto top = Vgpu{};

        auto data_mem = sim::make_data_memory<8>(&top);
        auto instruction_mem = sim::make_instruction_memory<8>(&top);

        instruction_mem.push_instruction(addi(6_x, 1_x, 10)); // addi x6, x1, 10
        instruction_mem.push_instruction(sw(1_x, 6_x, 0));    // sw x6, 0(x1)
        instruction_mem.push_instruction(halt());

        sim::set_kernel_config(top, 0, 0, 1, 1);

        auto done = simulate(top, instruction_mem, data_mem, 2000);
        REQUIRE(done);

        // data_mem[i] = i + 10
        for(auto i = 0; i < 32; i++) {
            CHECK(data_mem[i] == i + 10);
        }
    }

}

TEST_CASE("Multiple warps") {
    auto top = Vgpu{};

    auto data_mem = sim::make_data_memory<DATA_NUM_CHANNELS>(&top);
    auto instruction_mem = sim::make_instruction_memory<INST_NUM_CHANNELS>(&top);

    instruction_mem.push_instruction(addi(5_x, 1_x, 0)); // x5 = x1
    instruction_mem.push_instruction(sw(1_x, 5_x, 0));   // sw x5, 0(x1)
    instruction_mem.push_instruction(halt());

    sim::set_kernel_config(top, 0, 0, 1, 2); // 1 block, 2 warps per block

    auto done = simulate(top, instruction_mem, data_mem, 5000);
    REQUIRE(done);

    for(auto i = 0; i < 64; i++) {
        CHECK(data_mem[i] == i);
    }

}

TEST_CASE("Multiple blocks") {
    auto top = Vgpu{};

    auto data_mem = sim::make_data_memory<DATA_NUM_CHANNELS>(&top);
    auto instruction_mem = sim::make_instruction_memory<INST_NUM_CHANNELS>(&top);

    instruction_mem.push_instruction(addi(5_x, 2_x, 0)); // x5 = x2 (block id)
    instruction_mem.push_instruction(sw(1_x, 5_x, 0));   // sw x5, 0(x1)
    instruction_mem.push_instruction(halt());

    sim::set_kernel_config(top, 0, 0, 2, 1); // 2 blocks, 1 warp per block

    auto done = simulate(top, instruction_mem, data_mem, 5000);
    REQUIRE(done);

    for(auto i = 0; i < 64; i++) {
        if (i < 32)
            CHECK(data_mem[i] == 1);
    }

}

TEST_CASE("Conditional execution using mask") {
    auto top = Vgpu{};

    auto data_mem = sim::make_data_memory<DATA_NUM_CHANNELS>(&top);
    auto instruction_mem = sim::make_instruction_memory<INST_NUM_CHANNELS>(&top);

    instruction_mem.push_instruction(sx_slti(1_x, 1_x, 3));    // s1 = (x1 < 3) ? 1 : 0
    instruction_mem.push_instruction(addi(5_x, 0_x, 10));      // x5 = 10
    instruction_mem.push_instruction(sw(1_x, 5_x, 0));         // sw x5, 0(x1)
    instruction_mem.push_instruction(halt());

    sim::set_kernel_config(top, 0, 0, 1, 1);

    auto done = simulate(top, instruction_mem, data_mem, 5000);
    REQUIRE(done);

    for(auto i = 0; i < 32; i++) {
        if (i < 3)
            CHECK(data_mem[i] == 10);
        else
            CHECK(data_mem[i] == 0);
    }

}
