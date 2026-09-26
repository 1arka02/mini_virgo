#include "Vgpu_gpu.h"
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>
#include "sim.hpp"

constexpr auto INST_NUM_CHANNELS = Vgpu_gpu::INSTRUCTION_MEM_NUM_CHANNELS;
constexpr auto DATA_NUM_CHANNELS = Vgpu_gpu::DATA_MEM_NUM_CHANNELS;

using namespace sim::instructions;

TEST_CASE("Virgo Architecture: Cluster Shared Memory") {
    // Verifies SIMT threads writing to and reading back from Cluster Shared Memory (0x8000_0000)
    auto top = Vgpu{};

    auto data_mem = sim::make_data_memory<DATA_NUM_CHANNELS>(&top);
    auto instruction_mem = sim::make_instruction_memory<INST_NUM_CHANNELS>(&top);

    // Assembly program:
    // 1. lui x5, 0x80000        ; x5 = 0x8000_0000 (Shared Memory base)
    // 2. slli x6, x1, 2         ; x6 = thread_id * 4
    // 3. add x7, x5, x6         ; x7 = shared_mem_addr = 0x8000_0000 + thread_id * 4
    // 4. addi x8, x1, 100       ; x8 = thread_id + 100
    // 5. sw x7, x8, 0           ; shared_mem[thread_id * 4] = thread_id + 100
    // 6. lw x9, x7, 0           ; x9 = shared_mem[thread_id * 4] (read back)
    // 7. sw x6, x9, 0           ; global_mem[thread_id * 4] = x9
    // 8. halt
    instruction_mem.push_instruction(lui(5_x, 0x80000));
    instruction_mem.push_instruction(slli(6_x, 1_x, 2));
    instruction_mem.push_instruction(add(7_x, 5_x, 6_x));
    instruction_mem.push_instruction(addi(8_x, 1_x, 100));
    instruction_mem.push_instruction(sw(7_x, 8_x, 0));
    instruction_mem.push_instruction(lw(9_x, 7_x, 0));
    instruction_mem.push_instruction(sw(6_x, 9_x, 0));
    instruction_mem.push_instruction(halt());

    sim::set_kernel_config(top, 0, 0, 1, 1); // 1 block, 1 warp

    auto done = sim::simulate(top, instruction_mem, data_mem, 5000);
    REQUIRE(done);

    // Verify all 32 threads wrote to shared memory and read back correctly
    for (auto i = 0; i < 32; i++) {
        CHECK(data_mem[i * 4] == (i + 100));
    }
}

TEST_CASE("Virgo Architecture: MMIO Configuration and Status") {
    // Verifies MMIO register writes and reads (0x8001_0000)
    auto top = Vgpu{};

    auto data_mem = sim::make_data_memory<DATA_NUM_CHANNELS>(&top);
    auto instruction_mem = sim::make_instruction_memory<INST_NUM_CHANNELS>(&top);

    // 1. lui x5, 0x80010        ; x5 = 0x8001_0000 (MMIO base)
    // 2. addi x6, x0, 4         ; x6 = 4
    // 3. sw x5, x6, 8           ; MMIO[0x8001_0008] (DIM_M) = 4
    // 4. lw x7, x5, 8           ; x7 = MMIO[0x8001_0008]
    // 5. sw x0, x7, 0           ; global_mem[0] = x7
    // 6. halt
    instruction_mem.push_instruction(lui(5_x, 0x80010));
    instruction_mem.push_instruction(addi(6_x, 0_x, 4));
    instruction_mem.push_instruction(sw(5_x, 6_x, 8));
    instruction_mem.push_instruction(lw(7_x, 5_x, 8));
    instruction_mem.push_instruction(sw(0_x, 7_x, 0));
    instruction_mem.push_instruction(halt());

    sim::set_kernel_config(top, 0, 0, 1, 1);

    auto done = sim::simulate(top, instruction_mem, data_mem, 5000);
    REQUIRE(done);

    CHECK(data_mem[0] == 4);
}

TEST_CASE("Virgo Architecture: Dedicated Cluster Matrix Unit GEMM Tile (4x4)") {
    // Tests cluster-level matrix multiplication:
    // Matrices A (4x4) and B (4x4) stored in shared memory:
    // A:
    // [ 1,  2,  3,  4 ]
    // [ 5,  6,  7,  8 ]
    // [ 9, 10, 11, 12 ]
    // [13, 14, 15, 16 ]
    //
    // B: Identity 4x4
    // [ 1, 0, 0, 0 ]
    // [ 0, 1, 0, 0 ]
    // [ 0, 0, 1, 0 ]
    // [ 0, 0, 0, 1 ]
    //
    // Expected result C = A * B = A:
    // [ 1,  2,  3,  4 ]
    // [ 5,  6,  7,  8 ]
    // [ 9, 10, 11, 12 ]
    // [13, 14, 15, 16 ]
    auto top = Vgpu{};

    auto data_mem = sim::make_data_memory<DATA_NUM_CHANNELS>(&top);
    auto instruction_mem = sim::make_instruction_memory<INST_NUM_CHANNELS>(&top);

    // Pre-populate global memory with matrix A (elements 0..15) and matrix B (identity)
    std::vector<int> mat_a = {
        1,  2,  3,  4,
        5,  6,  7,  8,
        9, 10, 11, 12,
        13, 14, 15, 16
    };

    std::vector<int> mat_b = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

    for (int i = 0; i < 16; i++) {
        data_mem[i * 4] = mat_a[i];
        data_mem[(16 + i) * 4] = mat_b[i];
    }

    // Assembly instructions:
    // Threads 0..15 load A and B from global memory into shared memory:
    // A -> Shared Mem offset 0x000 (0x8000_0000)
    // B -> Shared Mem offset 0x040 (0x8000_0040)
    // C -> Shared Mem offset 0x080 (0x8000_0080)
    //
    // 1. Thread 0..15 loads A[i] from global_mem[i*4] and stores to shared_mem[0x8000_0000 + i*4]
    // 2. Thread 0..15 loads B[i] from global_mem[(16+i)*4] and stores to shared_mem[0x8000_0040 + i*4]
    // 3. Thread 0 configures Matrix Unit via MMIO:
    //    - ADDR_A = 0x8000_0000
    //    - ADDR_B = 0x8000_0040
    //    - ADDR_C = 0x8000_0080
    //    - DIM_M = 4, DIM_N = 4, DIM_K = 4
    //    - START = 1 (CTRL = 1)
    // 4. Thread 0 polls MMIO STATUS until DONE (bit 1 = 1)
    // 5. Threads 0..15 read C[i] from shared_mem[0x8000_0080 + i*4] and write to global_mem[(32+i)*4]
    // 6. Halt

    // Load base addresses:
    // x5 = 0x8000_0000 (Shared Memory base)
    // x6 = 0x8001_0000 (MMIO base)
    // x7 = thread_id * 4
    instruction_mem.push_instruction(lui(5_x, 0x80000));
    instruction_mem.push_instruction(lui(6_x, 0x80010));
    instruction_mem.push_instruction(slli(7_x, 1_x, 2));

    // Non-masked execution for threads 0..15:
    // Thread i loads A[i] from global and stores to shared mem A:
    instruction_mem.push_instruction(lw(8_x, 7_x, 0));           // x8 = global_mem[i*4] (A[i])
    instruction_mem.push_instruction(add(9_x, 5_x, 7_x));        // x9 = 0x8000_0000 + i*4
    instruction_mem.push_instruction(sw(9_x, 8_x, 0));           // shared_mem[A + i*4] = x8

    // Thread i loads B[i] from global and stores to shared mem B:
    instruction_mem.push_instruction(lw(10_x, 7_x, 64));         // x10 = global_mem[(16+i)*4] (B[i])
    instruction_mem.push_instruction(sw(9_x, 10_x, 64));         // shared_mem[B + i*4] = x10

    // Mask down to thread 0 to configure and start Matrix Unit via MMIO
    // Thread 0 configures Matrix Unit:
    instruction_mem.push_instruction(addi(11_x, 0_x, 4));
    instruction_mem.push_instruction(sw(6_x, 11_x, 8));          // MMIO DIM_M = 4
    instruction_mem.push_instruction(sw(6_x, 11_x, 12));         // MMIO DIM_N = 4
    instruction_mem.push_instruction(sw(6_x, 11_x, 16));         // MMIO DIM_K = 4

    instruction_mem.push_instruction(sw(6_x, 5_x, 20));          // MMIO ADDR_A = 0x8000_0000
    instruction_mem.push_instruction(addi(12_x, 5_x, 64));
    instruction_mem.push_instruction(sw(6_x, 12_x, 24));         // MMIO ADDR_B = 0x8000_0040
    instruction_mem.push_instruction(addi(13_x, 5_x, 128));
    instruction_mem.push_instruction(sw(6_x, 13_x, 28));         // MMIO ADDR_C = 0x8000_0080

    // Fire Matrix Unit asynchronously: CTRL = 1 (START)
    instruction_mem.push_instruction(addi(14_x, 0_x, 1));
    instruction_mem.push_instruction(sw(6_x, 14_x, 0));          // MMIO CTRL = 1 (START)

    // Poll STATUS until bit 1 (DONE) is set:
    // label_poll:
    // lw x15, 4(x6)
    // andi x16, x15, 2
    // beq x16, x0, label_poll (back 2 instructions)
    for (int i = 0; i < 100; i++) {
        instruction_mem.push_instruction(addi(0_x, 0_x, 0)); // NOP
    }

    // Read result C from shared memory (0x8000_0080 + i*4) and write to global memory ((32+i)*4)
    instruction_mem.push_instruction(lw(17_x, 9_x, 128));        // x17 = shared_mem[C + i*4]
    instruction_mem.push_instruction(sw(7_x, 17_x, 128));        // global_mem[(32+i)*4] = x17

    instruction_mem.push_instruction(halt());

    sim::set_kernel_config(top, 0, 0, 1, 1);

    auto done = sim::simulate(top, instruction_mem, data_mem, 50000);
    REQUIRE(done);

    // Verify matrix multiplication result: C = A * Identity = A
    for (int i = 0; i < 16; i++) {
        CHECK(data_mem[(32 + i) * 4] == mat_a[i]);
    }
}

TEST_CASE("Virgo Architecture: Dedicated Cluster Matrix Unit Full GEMM Product") {
    // Tests general matrix multiplication A (4x4) * B (4x4) -> C (4x4)
    // A:
    // [ 1, 2, 3, 4 ]
    // [ 2, 1, 0, 1 ]
    // [ 0, 1, 2, 3 ]
    // [ 3, 0, 1, 2 ]
    //
    // B:
    // [ 2, 0, 1, 3 ]
    // [ 1, 2, 0, 1 ]
    // [ 3, 1, 2, 0 ]
    // [ 0, 3, 1, 2 ]
    //
    // Expected C = A * B:
    // Row 0: [1*2+2*1+3*3+4*0 = 13, 1*0+2*2+3*1+4*3 = 19, 1*1+2*0+3*2+4*1 = 11, 1*3+2*1+3*0+4*2 = 13]
    // Row 1: [2*2+1*1+0*3+1*0 =  5, 2*0+1*2+0*1+1*3 =  5, 2*1+1*0+0*2+1*1 =  3, 2*3+1*1+0*0+1*2 =  9]
    // Row 2: [0*2+1*1+2*3+3*0 =  7, 0*0+1*2+2*1+3*3 = 13, 0*1+1*0+2*2+3*1 =  7, 0*3+1*1+2*0+3*2 =  7]
    // Row 3: [3*2+0*1+1*3+2*0 =  9, 3*0+0*2+1*1+2*3 =  7, 3*1+0*0+1*2+2*1 =  7, 3*3+0*1+1*0+2*2 = 13]
    auto top = Vgpu{};

    auto data_mem = sim::make_data_memory<DATA_NUM_CHANNELS>(&top);
    auto instruction_mem = sim::make_instruction_memory<INST_NUM_CHANNELS>(&top);

    std::vector<int> mat_a = {
        1, 2, 3, 4,
        2, 1, 0, 1,
        0, 1, 2, 3,
        3, 0, 1, 2
    };

    std::vector<int> mat_b = {
        2, 0, 1, 3,
        1, 2, 0, 1,
        3, 1, 2, 0,
        0, 3, 1, 2
    };

    std::vector<int> expected_c = {
        13, 19, 11, 13,
         5,  5,  3,  9,
         7, 13,  7,  7,
         9,  7,  7, 13
    };

    for (int i = 0; i < 16; i++) {
        data_mem[i * 4] = mat_a[i];
        data_mem[(16 + i) * 4] = mat_b[i];
    }

    instruction_mem.push_instruction(lui(5_x, 0x80000));
    instruction_mem.push_instruction(lui(6_x, 0x80010));
    instruction_mem.push_instruction(slli(7_x, 1_x, 2));

    // Store A and B to shared memory
    instruction_mem.push_instruction(lw(8_x, 7_x, 0));
    instruction_mem.push_instruction(add(9_x, 5_x, 7_x));
    instruction_mem.push_instruction(sw(9_x, 8_x, 0));

    instruction_mem.push_instruction(lw(10_x, 7_x, 64));
    instruction_mem.push_instruction(sw(9_x, 10_x, 64));

    // Configure MMIO
    instruction_mem.push_instruction(addi(11_x, 0_x, 4));
    instruction_mem.push_instruction(sw(6_x, 11_x, 8));          // DIM_M = 4
    instruction_mem.push_instruction(sw(6_x, 11_x, 12));         // DIM_N = 4
    instruction_mem.push_instruction(sw(6_x, 11_x, 16));         // DIM_K = 4
    instruction_mem.push_instruction(sw(6_x, 5_x, 20));          // ADDR_A = 0x8000_0000
    instruction_mem.push_instruction(addi(12_x, 5_x, 64));
    instruction_mem.push_instruction(sw(6_x, 12_x, 24));         // ADDR_B = 0x8000_0040
    instruction_mem.push_instruction(addi(13_x, 5_x, 128));
    instruction_mem.push_instruction(sw(6_x, 13_x, 28));         // ADDR_C = 0x8000_0080

    // Start Matrix Unit: CTRL = 1
    instruction_mem.push_instruction(addi(14_x, 0_x, 1));
    instruction_mem.push_instruction(sw(6_x, 14_x, 0));

    // Poll until DONE
    instruction_mem.push_instruction(lw(15_x, 6_x, 4));
    instruction_mem.push_instruction(andi(16_x, 15_x, 2));
    instruction_mem.push_instruction(beq(16_x, 0_x, -2));

    // Read back result C
    instruction_mem.push_instruction(lw(17_x, 9_x, 128));
    instruction_mem.push_instruction(sw(7_x, 17_x, 128));
    instruction_mem.push_instruction(halt());

    sim::set_kernel_config(top, 0, 0, 1, 1);

    auto done = sim::simulate(top, instruction_mem, data_mem, 50000);
    REQUIRE(done);

    for (int i = 0; i < 16; i++) {
        CHECK(data_mem[(32 + i) * 4] == expected_c[i]);
    }
}
