# This tests checks whether addi, sx.slti, sw and halt instructions work as expected
# It also checks whether comments and directives are ignored
# The output should be:
# memory[0] = 1
# memory[1] = 2
# memory[2] = 3
# memory[3] = 4

.blocks 1
.warps 1

# This is a comment
addi x5, x1, 1      # x5 := thread_id + 1
sx.slti s1, x5, 5   # s1[thread_id] := x5 < 5
sw x5, 0(x1)        # mem[thread_id] := x5
halt                # Stop the execution
