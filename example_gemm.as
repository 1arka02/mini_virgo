.blocks 1
.warps 1

# Matrix Multiplication (GEMM) Example on Virgo Architecture
# This example uses the Matrix Unit (systolic array) to compute C = A * B
# for 4x4 matrices. The SIMT cores load data from global memory into
# shared memory, configure the Matrix Unit via MMIO, wait for completion,
# then copy results back to global memory.
#
# Memory Layout:
#   Global Memory:  A at byte offset 0, B at byte offset 64
#   Shared Memory:  A at 0x8000_0000, B at 0x8000_0040, C at 0x8000_0080
#   MMIO Registers: 0x8001_0000 (CTRL), +4 (STATUS), +8 (DIM_M), etc.

# Base Addresses
lui x5, 0x80000        # x5 = 0x8000_0000 (Shared Memory base)
lui x6, 0x80010        # x6 = 0x8001_0000 (MMIO base)

# Thread ID (x1) * 4 = byte offset for this thread
slli x7, x1, 2         # x7 = thread_id * 4

# 1. Load A[thread_id] from Global Memory to Shared Memory
#    Each of 32 threads loads one word; first 16 cover the 4x4 matrix
lw x8, 0(x7)           # x8 = GlobalMem[thread_id * 4]
add x9, x5, x7         # x9 = SharedMem base + thread_offset
sw x8, 0(x9)           # SharedMem[thread_offset] = x8

# 2. Load B[thread_id] from Global Memory (offset 64) to Shared Memory (offset 64)
lw x10, 64(x7)         # x10 = GlobalMem[64 + thread_id * 4]
sw x10, 64(x9)         # SharedMem[64 + thread_offset] = x10

# 3. Configure Matrix Unit Dimensions: M=4, N=4, K=4
addi x11, x0, 4        # x11 = 4
sw x11, 8(x6)          # DIM_M = 4
sw x11, 12(x6)         # DIM_N = 4
sw x11, 16(x6)         # DIM_K = 4

# 4. Configure Matrix Unit Base Addresses
sw x5, 20(x6)          # ADDR_A = 0x8000_0000

addi x12, x5, 64
sw x12, 24(x6)         # ADDR_B = 0x8000_0040

addi x13, x5, 128
sw x13, 28(x6)         # ADDR_C = 0x8000_0080

# 5. START Matrix Unit: write 1 to CTRL register
addi x14, x0, 1
sw x14, 0(x6)          # CTRL = 1 (START)

# 6. Wait for Matrix Unit to finish (NOP delay)
#    The Matrix Unit computes C = A * B in the systolic array.
#    We insert enough NOPs for it to complete.
addi x0, x0, 0
addi x0, x0, 0
addi x0, x0, 0
addi x0, x0, 0
addi x0, x0, 0
addi x0, x0, 0
addi x0, x0, 0
addi x0, x0, 0
addi x0, x0, 0
addi x0, x0, 0
addi x0, x0, 0
addi x0, x0, 0
addi x0, x0, 0
addi x0, x0, 0
addi x0, x0, 0
addi x0, x0, 0
addi x0, x0, 0
addi x0, x0, 0
addi x0, x0, 0
addi x0, x0, 0

# 7. Copy results from Shared Memory C back to Global Memory
#    C is at SharedMem + 128 = 0x8000_0080
addi x17, x5, 128      # x17 = SharedMem + 128
add x17, x17, x7       # x17 = SharedMem + 128 + thread_offset
lw x19, 0(x17)         # x19 = SharedMem[C + thread_offset]

addi x20, x0, 128      # x20 = 128 (GlobalMem C base)
add x20, x20, x7       # x20 = 128 + thread_offset
sw x19, 0(x20)         # GlobalMem[128 + thread_offset] = x19

halt
