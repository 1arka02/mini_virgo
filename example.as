.blocks 1
.warps 4

# Simple test: load thread id, add 10, and store to memory
addi x5, x1, 10
add x6, x5, x5
sw x6, 0(x1)
halt
