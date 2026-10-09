import struct

mat_a = [
    1, 2, 4, 4,
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

# Helper to pack 4 INT8s into a 32-bit integer (little-endian)
def pack_int8x4(row):
    assert len(row) == 4
    return (row[3] << 24) | (row[2] << 16) | (row[1] << 8) | row[0]

with open('/Users/alexa/Desktop/mini_virgo/gemm_data.txt', 'w') as f:
    f.write("# INT32 Mode (Legacy)\n")
    for i, val in enumerate(mat_a):
        f.write(f"{i*4}: {val}\n")
    for i, val in enumerate(mat_b):
        f.write(f"{(16+i)*4}: {val}\n")

    f.write("\n# INT8x4 Packed Mode (4 elements per word)\n")
    # For A: pack each row into 1 word
    for i in range(4):
        packed = pack_int8x4(mat_a[i*4:(i+1)*4])
        f.write(f"{(32+i)*4}: {packed}\n")

    # For B: pack each column into 1 word (since inner dim is K)
    # Note: For systolic array, we typically need to transpose B if we pack along K
    for j in range(4):
        col = [mat_b[0*4+j], mat_b[1*4+j], mat_b[2*4+j], mat_b[3*4+j]]
        packed = pack_int8x4(col)
        f.write(f"{(36+j)*4}: {packed}\n")

