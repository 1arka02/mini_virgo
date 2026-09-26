mat_a = [
    1, 2, 3, 4,
    2, 1, 0, 1,
    0, 1, 2, 3,
    3, 0, 1, 2
]

mat_b = [
    2, 0, 1, 3,
    1, 2, 0, 1,
    3, 1, 2, 0,
    0, 3, 1, 2
]

with open('/Users/alexa/Desktop/mini_virgo/gemm_data.txt', 'w') as f:
    for i, val in enumerate(mat_a):
        f.write(f"{i*4}: {val}\n")
    
    for i, val in enumerate(mat_b):
        f.write(f"{(16+i)*4}: {val}\n")
