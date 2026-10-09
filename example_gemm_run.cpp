#include "Vgpu_gpu.h"
#include "sim.hpp"
#include <iostream>
#include <vector>
#include <iomanip>

constexpr auto INST_NUM_CHANNELS = Vgpu_gpu::INSTRUCTION_MEM_NUM_CHANNELS;
constexpr auto DATA_NUM_CHANNELS = Vgpu_gpu::DATA_MEM_NUM_CHANNELS;

using namespace sim::instructions;

template <typename T>
void print_matrix(const char* name, T& mem, int base_addr, int rows, int cols) {
    std::cout << "Matrix " << name << ":\n";
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            std::cout << std::setw(4) << mem[(base_addr + (r * cols + c)) * 4] << " ";
        }
        std::cout << "\n";
    }
    std::cout << "\n";
}

int main() {
    std::cout << "--- Virgo GPU Matrix Multiplication Example ---\n\n";

    auto top = Vgpu{};

    auto data_mem = sim::make_data_memory<DATA_NUM_CHANNELS>(&top);
    auto instruction_mem = sim::make_instruction_memory<INST_NUM_CHANNELS>(&top);

    // Initialize 4x4 Matrices A and B in Global Memory (Packed INT8x4)
    // These numbers come from what generate_data.py produced!
    std::vector<unsigned int> mat_a = {
        67305985, // Row 0 packed
        16777474, // Row 1 packed
        50462976, // Row 2 packed
        33619971  // Row 3 packed
    };

    std::vector<unsigned int> mat_b = {
        196866,   // Col 0 packed
        50397696, // Col 1 packed
        16908289, // Col 2 packed
        33554691  // Col 3 packed
    };

    for (int i = 0; i < 4; i++) {
        data_mem[i * 4] = mat_a[i];            // Matrix A at byte offset 0 (only takes 4 words now!)
        data_mem[(16 + i) * 4] = mat_b[i];     // Matrix B at byte offset 64 (only takes 4 words now!)
    }

    std::cout << "Initial state in Global Memory:\n";
    print_matrix("A", data_mem, 0, 4, 4);
    print_matrix("B", data_mem, 16, 4, 4);

    // Assembly Instructions for Virgo GPU Matrix Unit
    
    // 1. Load base addresses
    instruction_mem.push_instruction(lui(5_x, 0x80000)); // x5 = 0x8000_0000 (Shared Mem)
    instruction_mem.push_instruction(lui(6_x, 0x80010)); // x6 = 0x8001_0000 (MMIO)
    instruction_mem.push_instruction(slli(7_x, 1_x, 2)); // x7 = thread_id * 4

    // 2. Load Matrices A and B from global memory to shared memory
    instruction_mem.push_instruction(lw(8_x, 7_x, 0));       // x8 = global_mem[i*4]
    instruction_mem.push_instruction(add(9_x, 5_x, 7_x));    // x9 = shared_mem_base + i*4
    instruction_mem.push_instruction(sw(9_x, 8_x, 0));       // shared_mem[A_offset] = x8

    instruction_mem.push_instruction(lw(10_x, 7_x, 64));     // x10 = global_mem[64 + i*4]
    instruction_mem.push_instruction(sw(9_x, 10_x, 64));     // shared_mem[B_offset] = x10

    // 3. Configure Matrix Unit (Thread 0)
    instruction_mem.push_instruction(addi(11_x, 0_x, 4));
    instruction_mem.push_instruction(sw(6_x, 11_x, 8));      // DIM_M = 4
    instruction_mem.push_instruction(sw(6_x, 11_x, 12));     // DIM_N = 4
    instruction_mem.push_instruction(sw(6_x, 11_x, 16));     // DIM_K = 4

    // SET PRECISION TO INT8x4 (Mode 1)
    instruction_mem.push_instruction(addi(15_x, 0_x, 1));
    instruction_mem.push_instruction(sw(6_x, 15_x, 40));     // REG_PRECISION = 1

    instruction_mem.push_instruction(sw(6_x, 5_x, 20));      // ADDR_A = Shared Mem + 0
    instruction_mem.push_instruction(addi(12_x, 5_x, 64));
    instruction_mem.push_instruction(sw(6_x, 12_x, 24));     // ADDR_B = Shared Mem + 64
    instruction_mem.push_instruction(addi(13_x, 5_x, 128));
    instruction_mem.push_instruction(sw(6_x, 13_x, 28));     // ADDR_C = Shared Mem + 128

    // 4. Start Matrix Unit (CTRL = 1)
    instruction_mem.push_instruction(addi(14_x, 0_x, 1));
    instruction_mem.push_instruction(sw(6_x, 14_x, 0));

    // 5. Wait for Matrix Unit to finish (Since beq is unimplemented in simlib, we just insert NOPs)
    for(int i = 0; i < 100; i++) {
        instruction_mem.push_instruction(addi(0_x, 0_x, 0)); // NOP
    }
    // 6. Write Result C back to Global Memory
    instruction_mem.push_instruction(lw(17_x, 9_x, 128));    // Load from Shared Mem + 128
    instruction_mem.push_instruction(sw(7_x, 17_x, 128));    // Store to Global Mem + 128
    
    instruction_mem.push_instruction(halt());

    // Configure the GPU with 1 block, 1 warp (32 threads, we need 16)
    sim::set_kernel_config(top, 0, 0, 1, 1);

    std::cout << "Running GPU Simulation...\n";
    auto done = sim::simulate(top, instruction_mem, data_mem, 50000);

    if (done) {
        std::cout << "Simulation completed successfully.\n\n";
        std::cout << "Result from Global Memory:\n";
        print_matrix("C = A * B", data_mem, 32, 4, 4);
    } else {
        std::cout << "Simulation timed out.\n";
    }

    return 0;
}
