`default_nettype none
`timescale 1ns/1ns

`include "common.sv"

// Virgo MMIO Matrix Controller
// Implements the asynchronous MMIO register file interface for the Cluster Matrix Unit.
// Enables SIMT sub-cores to configure, trigger, and poll matrix operations non-blockingly.
module matrix_controller #(
    parameter int ADDR_WIDTH = 32,
    parameter int DATA_WIDTH = 32
) (
    input wire clk,
    input wire reset,

    // MMIO Slave Interface (from Cluster Interconnect)
    input wire                     mmio_read_valid,
    input wire [ADDR_WIDTH-1:0]    mmio_read_address,
    output logic                   mmio_read_ready,
    output logic [DATA_WIDTH-1:0]  mmio_read_data,

    input wire                     mmio_write_valid,
    input wire [ADDR_WIDTH-1:0]    mmio_write_address,
    input wire [DATA_WIDTH-1:0]    mmio_write_data,
    output logic                   mmio_write_ready,

    // Matrix Unit Control & Status Signals
    output logic                   mu_start,
    output logic                   mu_accumulate,
    output logic                   mu_clear_acc,
    output logic [3:0]             mu_dim_m,
    output logic [3:0]             mu_dim_n,
    output logic [3:0]             mu_dim_k,
    output logic [ADDR_WIDTH-1:0]  mu_addr_a,
    output logic [ADDR_WIDTH-1:0]  mu_addr_b,
    output logic [ADDR_WIDTH-1:0]  mu_addr_c,
    output logic [1:0]             mu_precision,

    input wire                     mu_busy,
    input wire                     mu_done,
    input wire [DATA_WIDTH-1:0]    mu_cycle_count
);

    // MMIO Registers
    logic [3:0]            reg_dim_m;
    logic [3:0]            reg_dim_n;
    logic [3:0]            reg_dim_k;
    logic [ADDR_WIDTH-1:0] reg_addr_a;
    logic [ADDR_WIDTH-1:0] reg_addr_b;
    logic [ADDR_WIDTH-1:0] reg_addr_c;
    logic                  reg_accumulate;
    logic                  reg_done_sticky;
    logic [1:0]            reg_precision;

    assign mu_dim_m      = reg_dim_m;
    assign mu_dim_n      = reg_dim_n;
    assign mu_dim_k      = reg_dim_k;
    assign mu_addr_a     = reg_addr_a;
    assign mu_addr_b     = reg_addr_b;
    assign mu_addr_c     = reg_addr_c;
    assign mu_accumulate = reg_accumulate;
    assign mu_precision  = reg_precision;

    always_ff @(posedge clk) begin
        if (reset) begin
            mmio_read_ready  <= 1'b0;
            mmio_read_data   <= '0;
            mmio_write_ready <= 1'b0;

            mu_start         <= 1'b0;
            mu_clear_acc     <= 1'b0;
            reg_accumulate   <= 1'b0;
            reg_dim_m        <= 4'd4;
            reg_dim_n        <= 4'd4;
            reg_dim_k        <= 4'd4;
            reg_addr_a       <= `VIRGO_SHARED_MEM_BASE;
            reg_addr_b       <= `VIRGO_SHARED_MEM_BASE + 32'h0000_0040;
            reg_addr_c       <= `VIRGO_SHARED_MEM_BASE + 32'h0000_0080;
            reg_done_sticky  <= 1'b0;
            reg_precision    <= 2'b00;
        end else begin
            // Single-cycle pulses auto-clear
            mu_start     <= 1'b0;
            mu_clear_acc <= 1'b0;

            if (mu_done) begin
                reg_done_sticky <= 1'b1;
            end

            // MMIO Read Handling
            if (mmio_read_valid && !mmio_read_ready) begin
                if (mmio_read_address == `VIRGO_REG_SYNC) begin
                    if (!mu_busy) begin
                        mmio_read_ready <= 1'b1;
                        mmio_read_data  <= 32'd1;
                    end else begin
                        mmio_read_ready <= 1'b0;
                    end
                end else begin
                    mmio_read_ready <= 1'b1;
                    case (mmio_read_address)
                        `VIRGO_REG_CTRL: begin
                            mmio_read_data <= {29'b0, mu_clear_acc, reg_accumulate, mu_busy};
                        end
                        `VIRGO_REG_STATUS: begin
                            mmio_read_data <= {30'b0, reg_done_sticky, mu_busy};
                        end
                        `VIRGO_REG_DIM_M: begin
                            mmio_read_data <= {28'b0, reg_dim_m};
                        end
                        `VIRGO_REG_DIM_N: begin
                            mmio_read_data <= {28'b0, reg_dim_n};
                        end
                        `VIRGO_REG_DIM_K: begin
                            mmio_read_data <= {28'b0, reg_dim_k};
                        end
                        `VIRGO_REG_ADDR_A: begin
                            mmio_read_data <= reg_addr_a;
                        end
                        `VIRGO_REG_ADDR_B: begin
                            mmio_read_data <= reg_addr_b;
                        end
                        `VIRGO_REG_ADDR_C: begin
                            mmio_read_data <= reg_addr_c;
                        end
                        `VIRGO_REG_CYCLES: begin
                            mmio_read_data <= mu_cycle_count;
                        end
                        `VIRGO_REG_PRECISION: begin
                            mmio_read_data <= {30'b0, reg_precision};
                        end
                        default: begin
                            mmio_read_data <= '0;
                        end
                    endcase
                end
            end else begin
                mmio_read_ready <= 1'b0;
            end

            // MMIO Write Handling
            if (mmio_write_valid && !mmio_write_ready) begin
                mmio_write_ready <= 1'b1;
                case (mmio_write_address)
                    `VIRGO_REG_CTRL: begin
                        $display("MMIO_CTRL: write val=%h, start=%b acc=%b clear=%b", mmio_write_data, mmio_write_data[0], mmio_write_data[1], mmio_write_data[2]);
                        if (mmio_write_data[0]) begin
                            mu_start        <= 1'b1;
                            reg_done_sticky <= 1'b0; // clear done on start
                        end
                        reg_accumulate <= mmio_write_data[1];
                        if (mmio_write_data[2]) begin
                            mu_clear_acc <= 1'b1;
                        end
                    end
                    `VIRGO_REG_STATUS: begin
                        // Writing 0 to status clears sticky done
                        if (!mmio_write_data[1]) begin
                            reg_done_sticky <= 1'b0;
                        end
                    end
                    `VIRGO_REG_DIM_M: begin
                        reg_dim_m <= mmio_write_data[3:0];
                    end
                    `VIRGO_REG_DIM_N: begin
                        reg_dim_n <= mmio_write_data[3:0];
                    end
                    `VIRGO_REG_DIM_K: begin
                        reg_dim_k <= mmio_write_data[3:0];
                    end
                    `VIRGO_REG_ADDR_A: begin
                        reg_addr_a <= mmio_write_data;
                    end
                    `VIRGO_REG_ADDR_B: begin
                        reg_addr_b <= mmio_write_data;
                    end
                    `VIRGO_REG_ADDR_C: begin
                        reg_addr_c <= mmio_write_data;
                    end
                    `VIRGO_REG_PRECISION: begin
                        reg_precision <= mmio_write_data[1:0];
                    end
                    default: begin
                        // Ignore unknown write addresses
                    end
                endcase
            end else begin
                mmio_write_ready <= 1'b0;
            end
        end
    end

endmodule
