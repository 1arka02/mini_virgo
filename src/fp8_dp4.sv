`default_nettype none
`timescale 1ns/1ns

// FP8 (E4M3) Dot Product of 4 pairs (DP4)
// Unpacks two 32-bit words into four 8-bit E4M3 values each,
// multiplies corresponding pairs, and accumulates them into a 32-bit signed integer.
module fp8_dp4 (
    input  logic [31:0] a_packed,
    input  logic [31:0] b_packed,
    output logic signed [31:0] result
);
    logic [7:0] a[4], b[4];
    assign {a[3], a[2], a[1], a[0]} = a_packed;
    assign {b[3], b[2], b[1], b[0]} = b_packed;

    logic signed [31:0] prod[4];

    genvar i;
    generate
        for (i = 0; i < 4; i++) begin : gen_mult
            // E4M3 Format: 1 sign bit, 4 exponent bits, 3 mantissa bits
            wire sign_a = a[i][7];
            wire [3:0] exp_a = a[i][6:3];
            wire [2:0] man_a = a[i][2:0];

            wire sign_b = b[i][7];
            wire [3:0] exp_b = b[i][6:3];
            wire [2:0] man_b = b[i][2:0];

            wire sign_p = sign_a ^ sign_b;
            
            // Append implicit leading 1 (unless exponent is 0 -> subnormal)
            wire [3:0] norm_man_a = (exp_a == 0) ? {1'b0, man_a} : {1'b1, man_a};
            wire [3:0] norm_man_b = (exp_b == 0) ? {1'b0, man_b} : {1'b1, man_b};

            // 4b x 4b = 8b mantissa product
            wire [7:0] man_p = norm_man_a * norm_man_b;

            // Bias for E4M3 is 7.
            // true_exp = (exp_a - 7) + (exp_b - 7) = exp_a + exp_b - 14
            // Since man_a and man_b each have 3 fractional bits, man_p has 6.
            // To convert to an integer, we shift by (true_exp - 6).
            wire signed [8:0] true_exp = $signed({5'b0, exp_a}) + $signed({5'b0, exp_b}) - 9'd14;
            wire signed [8:0] shift = true_exp - 9'd6;

            logic signed [31:0] raw_val;
            always_comb begin
                if ((exp_a == 0 && man_a == 0) || (exp_b == 0 && man_b == 0)) begin
                    raw_val = 0; // Zero shortcut
                end else begin
                    if (shift >= 0)
                        raw_val = (32'(man_p)) << shift;
                    else
                        raw_val = (32'(man_p)) >> (-shift);
                end
            end

            assign prod[i] = sign_p ? -raw_val : raw_val;
        end
    endgenerate

    assign result = prod[0] + prod[1] + prod[2] + prod[3];
endmodule
