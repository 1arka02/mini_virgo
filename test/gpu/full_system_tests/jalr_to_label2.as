.blocks 1
.warps 1

jalr x0, label2
label1:
addi x5, x5, 10
sw x5, 0(x0)
label2:
addi x5, x5, 50
sw x5, 0(x0)
halt
