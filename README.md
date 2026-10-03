# mini-Virgo (Virgo Architecture)

An educational, SystemVerilog implementation of a parallel processor, modified to include the **Virgo micro-architecture** (a dedicated cluster-level matrix unit).

---

## Overview

`mini-Virgo` is a SystemVerilog RTL model of a simple RISC-V-like GPU. It explores modern GPU design concepts, specifically focusing on the integration of systolic arrays for matrix operations (GEMM).

Traditional GPUs rely entirely on SIMT (Single Instruction, Multiple Thread) cores for computation. The **Virgo** architecture disaggregates this by introducing a dedicated **Matrix Unit** (a systolic array) at the cluster level.

*   **Cluster (SM) Level:** The GPU is divided into clusters. Each cluster contains multiple SIMD cores (`CORES_PER_CLUSTER`).
*   **Dedicated Matrix Unit:** A systolic array (`matrix_unit.sv`) is instantiated inside each cluster. It is designed specifically for matrix multiplication and accumulation (GEMM) workloads, offering better FLOPS scalability and energy efficiency.
*   **Shared Memory:** Each cluster features a local shared memory. This memory acts as a high-speed buffer accessible by both the SIMT cores and the matrix unit.
*   **Interconnect & Arbitration:** A cluster interconnect (`cluster_interconnect.sv`) arbitrates access to the shared memory and MMIO registers between the cores and the matrix unit.
*   **MMIO Control:** The SIMT cores control the matrix unit via Memory-Mapped I/O (MMIO) registers (e.g., `VIRGO_REG_CTRL`, `VIRGO_REG_ADDR_A`, `VIRGO_REG_DIM_M`). Threads configure the dimensions and memory addresses, and then write a start bit to trigger the GEMM operation.

---

## Architecture

### Top-Level GPU

```mermaid
flowchart TB
    %% ── Top Level ──
    Host["Host / Kernel Config"]
    Host -->|"execution_start + kernel_config"| Dispatcher

    subgraph GPU ["GPU (gpu.sv)"]
        Dispatcher["Dispatcher<br/>(dispatcher.sv)"]

        subgraph Cluster0 ["Virgo Cluster (virgo_cluster.sv)"]
            direction TB

            subgraph Cores ["SIMT Sub-Cores"]
                direction LR
                subgraph Core0 ["Compute Core 0 (compute_core.sv)"]
                    direction TB
                    Warps0["Warps<br/>(warp scheduler)"]
                    Fetch0["Fetcher"]
                    Dec0["Decoder"]
                    ALU0["ALU"]
                    RF0["Vector & Scalar<br/>Register Files"]
                    LSU0["LSUs<br/>(per-thread + scalar)"]
                    Warps0 --> Fetch0 --> Dec0 --> ALU0
                    Dec0 --> RF0
                    Dec0 --> LSU0
                end
                subgraph Core1 ["Compute Core 1"]
                    direction TB
                    Warps1["Warps"]
                    Fetch1["Fetcher"]
                    Dec1["Decoder"]
                    ALU1["ALU"]
                    RF1["Register Files"]
                    LSU1["LSUs"]
                    Warps1 --> Fetch1 --> Dec1 --> ALU1
                    Dec1 --> RF1
                    Dec1 --> LSU1
                end
            end

            Interconnect["Cluster Interconnect<br/>(cluster_interconnect.sv)<br/>Address Decode & Arbitration"]

            LSU0 -->|"read/write requests"| Interconnect
            LSU1 -->|"read/write requests"| Interconnect

            subgraph MatrixPath ["Matrix Acceleration Path"]
                direction LR
                MMIO["Matrix Controller<br/>(matrix_controller.sv)<br/>MMIO Register File"]
                MU["Matrix Unit<br/>(matrix_unit.sv)<br/>Systolic Array"]
                MMIO -->|"start, dims, addrs"| MU
            end

            SharedMem["Shared Memory<br/>(shared_memory.sv)<br/>Dual-Port · 4 KB"]
            ClusterSync["Cluster Sync<br/>(cluster_sync.sv)"]

            Interconnect -->|"MMIO range<br/>0x8001_xxxx"| MMIO
            Interconnect <-->|"Shared mem range<br/>0x8000_xxxx<br/>Port A"| SharedMem
            MU <-->|"Port B"| SharedMem

            Dispatcher -->|"start / block_id"| Core0
            Dispatcher -->|"start / block_id"| Core1
            Core0 --- ClusterSync
            Core1 --- ClusterSync
        end

        DataMemCtrl["Data Memory Controller<br/>(mem_controller.sv)<br/>Multi-Channel Arbiter"]
        ProgMemCtrl["Program Memory Controller<br/>(mem_controller.sv)<br/>Read-Only Arbiter"]

        Interconnect -->|"Global range<br/>&lt; 0x8000_0000"| DataMemCtrl
        Fetch0 -->|"instruction fetch"| ProgMemCtrl
        Fetch1 -->|"instruction fetch"| ProgMemCtrl
    end

    DataMem[("Data Memory<br/>(External)")]
    InstrMem[("Instruction Memory<br/>(External)")]

    DataMemCtrl <--> DataMem
    ProgMemCtrl <--> InstrMem
```

---

### Dispatcher

```mermaid
flowchart TB
    subgraph Inputs ["Host Interface"]
        direction LR
        Host["start · kernel_config\n(num_blocks, num_warps_per_block,\nbase_instructions_address)"]
    end

    subgraph Disp ["Dispatcher (dispatcher.sv)"]
        direction TB

        subgraph State ["Internal State"]
            direction LR
            BD["blocks_dispatched"]
            BDone["blocks_done"]
            TotalBlocks["total_blocks\n= kernel_config.num_blocks"]
        end

        subgraph FSM ["Dispatch Logic (per cycle)"]
            direction TB
            CheckReset{"Core just\nreset?"}
            HasWork{"blocks_dispatched\n< total_blocks?"}
            Assign["Assign block_id\nassert core_start"]
            CheckDone{"core_done[i]\nasserted?"}
            ResetCore["Reset core\nblocks_done++"]
            KernelDone{"blocks_done\n== total_blocks?"}
            Done["done ← 1"]

            CheckReset -->|"Yes"| HasWork
            HasWork -->|"Yes"| Assign
            HasWork -->|"No"| CheckDone
            CheckReset -->|"No"| CheckDone
            CheckDone -->|"Yes"| ResetCore
            ResetCore --> KernelDone
            KernelDone -->|"Yes"| Done
        end
    end

    subgraph Cores ["Compute Cores (NUM_CORES)"]
        direction LR
        CoreCtrl["core_start · core_reset\ncore_block_id · core_done"]
    end

    Host --> State
    State --> FSM
    FSM --> CoreCtrl
    CoreCtrl -->|"core_done feedback"| FSM
```

The Dispatcher (`dispatcher.sv`) is the GPU-level work scheduler that maps thread blocks from a kernel launch onto available hardware cores. On `start`, it resets all cores and begins dispatching — assigning a unique `block_id` to each free core and asserting `core_start`. When a core signals `core_done`, the dispatcher resets it and dispatches the next pending block (if any remain). Once `blocks_done == total_blocks`, the kernel is complete. Think of it as a work queue: if the kernel requires 10 blocks but only 2 cores exist, the dispatcher feeds blocks to cores as they become free, 2 at a time, until all 10 are done.

---

### Virgo Cluster

```mermaid
flowchart TB
    subgraph External ["External Interfaces"]
        direction LR
        Dispatcher["Dispatcher\n(core_start, core_reset,\ncore_block_id, kernel_config)"]
        ProgMemCtrl["Program Memory Controller\n(instruction fetch channels)"]
        DataMemCtrl["Data Memory Controller\n(global read/write channels)"]
    end

    subgraph VirgoCluster ["Virgo Cluster (virgo_cluster.sv)"]
        direction TB

        subgraph Cores ["1. SIMT Sub-Cores (× CORES_PER_CLUSTER)"]
            direction LR
            Core0["Compute Core 0\n(compute_core.sv)\nWarps · Fetcher · Decoder\nALU · Reg Files · LSUs"]
            Core1["Compute Core 1\n(compute_core.sv)"]
        end

        Interconnect["2. Cluster Interconnect\n(cluster_interconnect.sv)\nAddress Decode · SM Arbiter · MMIO Arbiter\nResponse Demux"]

        SharedMem["3. Shared Memory\n(shared_memory.sv)\nDual-Port SRAM\n(MEM_WORDS × DATA_WIDTH)"]

        MatrixCtrl["4. Matrix Controller\n(matrix_controller.sv)\nMMIO Register File\nSync Barrier"]

        MatrixUnit["5. Matrix Unit\n(matrix_unit.sv)\nSystolic Array\n(MAX_DIM=8)"]

        ClusterSync["6. Cluster Sync\n(cluster_sync.sv)\ncore_done aggregation\nmatrix_ready status"]
    end

    %% Dispatcher → Cores
    Dispatcher -->|"start / reset\nblock_id / kernel_config"| Core0
    Dispatcher -->|"start / reset\nblock_id / kernel_config"| Core1

    %% Cores → Program Memory Controller (instruction fetches)
    Core0 -->|"instruction fetch\n(WARPS_PER_CORE channels)"| ProgMemCtrl
    Core1 -->|"instruction fetch"| ProgMemCtrl

    %% Cores → Interconnect (LSU requests)
    Core0 -->|"LSU read/write\n(THREADS+1 per core)"| Interconnect
    Core1 -->|"LSU read/write"| Interconnect

    %% Interconnect routing
    Interconnect -->|"addr < 0x8000_0000\nGlobal (pass-through)"| DataMemCtrl
    Interconnect <-->|"0x8000_xxxx\nPort A"| SharedMem
    Interconnect <-->|"0x8001_xxxx\nMMIO"| MatrixCtrl

    %% Matrix Controller ↔ Matrix Unit
    MatrixCtrl -->|"start, accumulate,\nclear_acc, dims, addrs"| MatrixUnit
    MatrixUnit -->|"busy, done,\ncycle_count"| MatrixCtrl

    %% Matrix Unit ↔ Shared Memory Port B
    MatrixUnit <-->|"Port B\n(dedicated read/write)"| SharedMem

    %% Cluster Sync
    Core0 --- ClusterSync
    Core1 --- ClusterSync
    MatrixUnit -->|"busy / done"| ClusterSync
```

The Virgo Cluster (`virgo_cluster.sv`) is the top-level Streaming Multiprocessor (SM) equivalent in the Virgo architecture. Each cluster integrates six sub-blocks:

1.  **SIMT Sub-Cores** (`compute_core.sv` × `CORES_PER_CLUSTER`): Each core contains warp schedulers, instruction fetchers, decoders, ALUs, vector/scalar register files, and per-thread + scalar LSUs. Instruction fetch channels connect directly out to the Program Memory Controller; LSU traffic is routed through the cluster interconnect.
2.  **Cluster Interconnect** (`cluster_interconnect.sv`): Receives all LSU requests from every core and performs address-based routing — global memory requests (`< 0x8000_0000`) pass through to the external Data Memory Controller, shared memory accesses are arbitrated onto Port A, and MMIO accesses are arbitrated to the Matrix Controller.
3.  **Shared Memory** (`shared_memory.sv`): A dual-ported SRAM scratchpad. Port A serves the SIMT cores (via the interconnect arbiter); Port B provides a dedicated, contention-free path for the Matrix Unit to stream tile data.
4.  **Matrix Controller** (`matrix_controller.sv`): An MMIO slave that exposes configuration registers (tile dimensions, base addresses, control/status) to the cores. It also implements a hardware barrier (`REG_SYNC`) that stalls a reading thread until the Matrix Unit finishes.
5.  **Matrix Unit** (`matrix_unit.sv`): A systolic-array GEMM engine (configurable up to 8×8 tiles). It reads matrices A and B from shared memory via Port B, performs multiply-accumulate, and writes the result matrix C back.
6.  **Cluster Sync** (`cluster_sync.sv`): Aggregates `core_done` signals from all sub-cores and tracks the Matrix Unit's busy/done status, providing cluster-wide synchronization primitives.

---

### Compute Core (SIMT Core)

```mermaid
flowchart TB
    subgraph External ["External Interfaces"]
        direction LR
        Dispatcher["Dispatcher\n(start, reset, block_id,\nkernel_config)"]
        InstrMem["Program Memory Controller\n(instruction fetch)"]
        Interconnect["Cluster Interconnect\n(data read/write)"]
    end

    subgraph ComputeCore ["Compute Core (compute_core.sv)"]
        direction TB

        WarpSched["Warp Scheduler\nRound-robin selection\n(skip IDLE / FETCH / DONE)"]

        subgraph PerWarp ["Per-Warp Circuitry (× WARPS_PER_CORE)"]
            direction LR
            Fetcher["Fetcher\n(fetcher.sv)\nIDLE → REQUESTING →\nWAITING → DONE"]
            Decoder["Decoder\n(decoder.sv)\nSingle-cycle decode"]
            ScalarRF["Scalar Reg File\n(scalar_reg_file.sv)\nexec mask, scalar rs1/rs2"]
            VectorRF["Vector Reg File\n(reg_file.sv)\n32 regs × THREADS\nrs1/rs2 per thread"]
        end

        subgraph SharedHW ["Shared Hardware (× THREADS_PER_WARP)"]
            direction LR
            ALUs["ALU Array\n(alu.sv × THREADS)\n+ 1 Scalar ALU"]
            LSUs["LSU Array\n(lsu.sv × THREADS)\n+ 1 Scalar LSU"]
        end

        subgraph WarpFSM ["Warp State Machine"]
            direction LR
            IDLE(["IDLE"])
            FETCH(["FETCH"])
            DECODE(["DECODE"])
            REQUEST(["REQUEST"])
            WAIT(["WAIT"])
            EXECUTE(["EXECUTE"])
            UPDATE(["UPDATE"])
            DONE(["DONE"])
            IDLE -->|"start"| FETCH
            FETCH -->|"fetcher done"| DECODE
            DECODE --> REQUEST
            REQUEST --> WAIT
            WAIT -->|"all LSUs ready"| EXECUTE
            EXECUTE --> UPDATE
            UPDATE -->|"halt"| DONE
            UPDATE -->|"next instr"| FETCH
        end
    end

    Dispatcher -->|"start / block_id\nkernel_config"| WarpSched
    WarpSched -->|"current_warp"| PerWarp
    Fetcher -->|"instruction"| Decoder
    Decoder -->|"decoded fields"| ScalarRF
    Decoder -->|"decoded fields"| VectorRF
    ScalarRF -->|"scalar rs1/rs2"| ALUs
    VectorRF -->|"vector rs1/rs2"| ALUs
    VectorRF -->|"vector rs1/rs2"| LSUs
    ScalarRF -->|"scalar rs1/rs2"| LSUs
    ALUs -->|"alu_out"| VectorRF
    ALUs -->|"scalar_alu_out"| ScalarRF
    LSUs -->|"lsu_out"| VectorRF
    LSUs -->|"scalar_lsu_out"| ScalarRF

    Fetcher <-->|"instruction fetch\n(per-warp channel)"| InstrMem
    LSUs <-->|"data read/write\n(THREADS+1 channels)"| Interconnect
```

The Compute Core (`compute_core.sv`) is the SIMT execution engine. Each core contains:

*   **Warp Scheduler:** Round-robin selects the next warp that is ready to execute (skipping warps in `IDLE`, `FETCH`, or `DONE` states). The selected warp drives the shared ALU and LSU arrays.
*   **Per-Warp Circuitry** (replicated `WARPS_PER_CORE` times): A **Fetcher** that asynchronously requests instructions from program memory, a **Decoder** that produces control signals in one cycle, a **Scalar Register File** (holding execution masks and scalar operands), and a **Vector Register File** (32 registers × `THREADS_PER_WARP`, providing per-thread `rs1`/`rs2`).
*   **Shared Hardware** (replicated `THREADS_PER_WARP` times): An **ALU** and **LSU** per thread, plus one scalar ALU and one scalar LSU. These are time-multiplexed across warps by the scheduler.
*   **Warp State FSM:** Each warp independently cycles through `FETCH → DECODE → REQUEST → WAIT → EXECUTE → UPDATE`, then either loops back to `FETCH` or transitions to `DONE` on a halt instruction. Execution masking handles divergent branches.
*   **Latency Hiding:** Although only one warp occupies the ALU/LSU hardware per cycle, all warps progress concurrently — fetchers run in parallel, so while one warp executes, others can be fetching or waiting on memory. The scheduler switches to a ready warp whenever the current one finishes its `UPDATE` stage.

---

### Fetcher

```mermaid
flowchart LR
    subgraph Fetcher ["Fetcher (fetcher.sv)"]
        direction LR
        IDLE(["FETCHER_IDLE"])
        FETCHING(["FETCHER_FETCHING"])
        DONE_F(["FETCHER_DONE"])

        IDLE -->|"warp_state == WARP_FETCH\nassert mem_read_valid\naddress ← PC"| FETCHING
        FETCHING -->|"mem_read_ready\nlatch instruction"| DONE_F
        DONE_F -->|"warp_state == WARP_DECODE"| IDLE
    end

    ProgMem["Program Memory\n(via mem_controller)"]
    WarpCtrl["Warp State\n(from compute_core)"]

    WarpCtrl -->|"warp_state, PC"| IDLE
    FETCHING <-->|"read_valid / ready\nread_address / data"| ProgMem
    DONE_F -->|"fetched instruction"| WarpCtrl
```

The Fetcher (`fetcher.sv`) is a lightweight 3-state FSM that retrieves a single instruction from program memory for its warp. In `IDLE`, it waits until the warp enters `WARP_FETCH`, then asserts `mem_read_valid` with the current PC. In `FETCHING`, it waits for the program memory controller to respond with `mem_read_ready` and latches the instruction data. In `DONE`, it holds the instruction until the warp advances to `WARP_DECODE`, then returns to idle. Each warp has its own dedicated fetcher instance, enabling multiple warps to fetch instructions in parallel — this is the foundation of the core's latency-hiding mechanism.

---

### Matrix Unit

```mermaid
flowchart TB
    subgraph Control ["Control Interface (from Matrix Controller)"]
        direction LR
        CtrlIn["start · accumulate · clear_acc\ndim_m · dim_n · dim_k\naddr_a · addr_b · addr_c"]
    end

    subgraph MatrixUnit ["Matrix Unit (matrix_unit.sv)"]
        direction TB

        Latch["Latch Control Registers\nreg_m, reg_n, reg_k\nreg_addr_a/b/c\nreg_accumulate"]

        subgraph Accumulator ["Internal Accumulator"]
            ACC["acc[MAX_DIM][MAX_DIM]\nSigned MAC Array\n(clear on start or clear_acc)"]
        end

        subgraph FSM ["State Machine"]
            direction LR
            IDLE(["ST_IDLE"])
            READ_A(["ST_READ_A"])
            WAIT_A(["ST_WAIT_A"])
            READ_B(["ST_READ_B"])
            WAIT_B(["ST_WAIT_B\nacc += A × B"])
            WRITE_C(["ST_WRITE_C"])
            WAIT_C(["ST_WAIT_C"])
            DONE_ST(["ST_DONE"])

            IDLE -->|"start"| READ_A
            READ_A -->|"assert read_valid\naddr = addr_a + offset"| WAIT_A
            WAIT_A -->|"mem_read_ready\nlatch elem_a"| READ_B
            READ_B -->|"assert read_valid\naddr = addr_b + offset"| WAIT_B
            WAIT_B -->|"mem_read_ready\nMAC: acc[i][j] += elem_a × elem_b"| READ_A
            WAIT_B -->|"k,j,i loops done"| WRITE_C
            WRITE_C -->|"assert write_valid\ndata = acc[i][j]"| WAIT_C
            WAIT_C -->|"mem_write_ready\nadvance i,j"| WRITE_C
            WAIT_C -->|"all elements written"| DONE_ST
            DONE_ST -->|"busy←0, done←1"| IDLE
        end

        LoopCtrl["Triple-Nested Loop Control\nfor i in [0,M)\n  for j in [0,N)\n    for k in [0,K)"]
        Profiler["Cycle Counter\n(cycle_count)"]
    end

    subgraph MemPort ["Shared Memory (Port B)"]
        SM["mem_read_valid / ready / data\nmem_write_valid / ready / data"]
    end

    subgraph StatusOut ["Status (to Matrix Controller)"]
        Status["busy · done · cycle_count"]
    end

    CtrlIn --> Latch
    Latch --> FSM
    LoopCtrl --> FSM
    FSM --> ACC
    FSM <-->|"read A, read B,\nwrite C"| SM
    FSM --> Profiler
    Profiler --> Status
    FSM --> Status
```

The Matrix Unit (`matrix_unit.sv`) is a systolic-array-based compute engine dedicated to General Matrix Multiply-Accumulate (GEMM). It reads tile data (matrices A and B) from shared memory via Port B, performs the multiply-accumulate through a configurable M×N×K pipeline, and writes the result (matrix C) back. An internal accumulator can be preserved across invocations (`accumulate` mode) or cleared (`clear_acc`), enabling tiled outer-product loops. A cycle counter tracks operation latency.

---

### Matrix Controller

```mermaid
flowchart TB
    subgraph Inputs ["From Cluster Interconnect (MMIO Slave)"]
        direction LR
        WR["mmio_write_valid<br/>mmio_write_address<br/>mmio_write_data"]
        RD["mmio_read_valid<br/>mmio_read_address"]
    end

    subgraph Controller ["Matrix Controller (matrix_controller.sv)"]
        direction TB

        subgraph RegFile ["MMIO Register File"]
            direction LR
            CTRL["REG_CTRL (0x0000)<br/>[0] START<br/>[1] ACCUMULATE<br/>[2] CLEAR_ACC"]
            STATUS["REG_STATUS (0x0004)<br/>[0] BUSY<br/>[1] DONE (sticky)"]
            DIMS["REG_DIM_M/N/K<br/>(0x0008 / 0x000C / 0x0010)<br/>Tile dimensions [3:0]"]
            ADDRS["REG_ADDR_A/B/C<br/>(0x0014 / 0x0018 / 0x001C)<br/>Shared mem offsets"]
            CYCLES["REG_CYCLES (0x0020)<br/>Cycle counter"]
            SYNC["REG_SYNC (0x0024)<br/>HW Barrier: stalls<br/>read_ready while busy"]
        end

        WriteLogic["Write Decode<br/>Address → Register Update<br/>(single-cycle ack)"]
        ReadLogic["Read Decode<br/>Address → Register Readback"]
        SyncLogic{"SYNC Read?"}
        BusyCheck{"mu_busy?"}
    end

    WR --> WriteLogic
    WriteLogic --> CTRL
    WriteLogic --> DIMS
    WriteLogic --> ADDRS
    WriteLogic --> STATUS

    RD --> SyncLogic
    SyncLogic -->|Yes| BusyCheck
    BusyCheck -->|"Busy → stall<br/>(hold ready low)"| SyncLogic
    BusyCheck -->|"Not busy → ack"| ReadOut["mmio_read_ready ✓<br/>mmio_read_data"]
    SyncLogic -->|No| ReadLogic
    ReadLogic --> ReadOut

    subgraph MU ["To Matrix Unit (matrix_unit.sv)"]
        direction LR
        CtrlOut["mu_start (pulse)<br/>mu_accumulate<br/>mu_clear_acc (pulse)"]
        DimsOut["mu_dim_m/n/k"]
        AddrsOut["mu_addr_a/b/c"]
        StatusIn["mu_busy<br/>mu_done<br/>mu_cycle_count"]
    end

    CTRL -->|"start / clear_acc<br/>(auto-clear pulses)"| CtrlOut
    DIMS --> DimsOut
    ADDRS --> AddrsOut
    StatusIn -->|"feedback"| STATUS
    StatusIn -->|"feedback"| CYCLES
    StatusIn --> BusyCheck
```

The Matrix Controller (`matrix_controller.sv`) is an MMIO slave that exposes the matrix unit's configuration as a set of memory-mapped registers in the `0x8001_0000` address range. SIMT cores write tile dimensions (`DIM_M/N/K`), shared-memory base addresses (`ADDR_A/B/C`), and control bits (`START`, `ACCUMULATE`, `CLEAR_ACC`) through ordinary store instructions. Reads to `REG_STATUS` return the busy/done state. A special `REG_SYNC` register acts as a hardware barrier — the controller holds `read_ready` low while the matrix unit is busy, effectively stalling the requesting thread until the GEMM operation completes.

---

### Cluster Interconnect

```mermaid
flowchart TB
    subgraph CoreLSUs ["Core LSUs (NUM_LSUS)"]
        direction LR
        RReq["core_read_valid<br/>core_read_address"]
        WReq["core_write_valid<br/>core_write_address<br/>core_write_data"]
    end

    subgraph Interconnect ["Cluster Interconnect (cluster_interconnect.sv)"]
        direction TB

        AddrDecode{"Address Decode<br/>per-LSU"}

        subgraph SMArb ["Shared Memory Arbiter"]
            direction TB
            SM_IDLE(["ARB_IDLE"])
            SM_SR(["ARB_SERVING_READ"])
            SM_SW(["ARB_SERVING_WRITE"])
            SM_IDLE -->|"write req found<br/>(priority)"| SM_SW
            SM_IDLE -->|"read req found"| SM_SR
            SM_SR -->|"sm_read_ready"| SM_IDLE
            SM_SW -->|"sm_write_ready"| SM_IDLE
        end

        subgraph MMIOArb ["MMIO Arbiter"]
            direction TB
            MM_IDLE(["ARB_IDLE"])
            MM_SR(["ARB_SERVING_READ"])
            MM_SW(["ARB_SERVING_WRITE"])
            MM_IDLE -->|"write req found<br/>(priority)"| MM_SW
            MM_IDLE -->|"read req found"| MM_SR
            MM_SR -->|"mmio_read_ready"| MM_IDLE
            MM_SW -->|"mmio_write_ready"| MM_IDLE
        end

        Demux["Response Demux<br/>Route ready & data<br/>back to requesting LSU"]
    end

    RReq --> AddrDecode
    WReq --> AddrDecode

    AddrDecode -->|"addr < 0x8000_0000<br/>Global Memory"| ExtMem["Global Memory Controller<br/>(pass-through)"]
    AddrDecode -->|"0x8000_0000 – 0x8000_0FFF<br/>Shared Memory"| SMArb
    AddrDecode -->|"0x8001_0000 – 0x8001_00FF<br/>MMIO"| MMIOArb

    subgraph Destinations ["Cluster-Local Ports"]
        direction LR
        SM["Shared Memory<br/>(Port A)"]
        MMIO["Matrix Controller<br/>(MMIO Slave)"]
    end

    SMArb <-->|"sm_read/write"| SM
    MMIOArb <-->|"mmio_read/write"| MMIO

    SM --> Demux
    MMIO --> Demux
    ExtMem --> Demux
    Demux -->|"core_read_ready<br/>core_read_data<br/>core_write_ready"| CoreLSUs
```

The Cluster Interconnect (`cluster_interconnect.sv`) demultiplexes memory requests from all core LSUs based on address:

*   **Global Memory** (`< 0x8000_0000`): passed through directly — each LSU has its own external channel, so no arbitration is needed.
*   **Shared Memory** (`0x8000_0000 – 0x8000_0FFF`) and **MMIO** (`0x8001_0000 – 0x8001_00FF`): both are single-ported resources, so concurrent access from multiple LSUs must be serialized.

Each resource has a **fixed-priority arbiter** — a 3-state FSM (`IDLE → SERVING_READ / SERVING_WRITE → IDLE`). In `IDLE`, the arbiter scans LSUs starting from index 0; the first LSU with a matching request wins. Writes are checked before reads, giving store operations priority. Once granted, the arbiter stays in the serving state until the destination asserts `ready`, then returns to `IDLE`.

The **response demux** routes `ready` and `data` signals back to the specific LSU that won arbitration. Only the winning LSU's `core_read_ready` / `core_write_ready` is asserted; all others see `0` (stalled) until the current transaction completes.

---

### Shared Memory

```mermaid
flowchart TB
    subgraph Clients ["Memory Clients"]
        direction LR
        PortA["SIMT Cores<br/>(via Cluster Interconnect)"]
        PortB["Matrix Unit<br/>(Dedicated Path)"]
    end

    subgraph SharedMem ["Shared Memory (shared_memory.sv)"]
        direction TB
        Bank["Dual-Port SRAM Array<br/>(MEM_WORDS x DATA_WIDTH)"]
    end

    PortA <-->|"Port A<br/>core_read/write_valid<br/>address, data, ready"| Bank
    PortB <-->|"Port B<br/>mu_read_valid<br/>address, data, ready"| Bank
```

The Shared Memory (`shared_memory.sv`) provides a high-throughput, low-latency dual-ported scratchpad memory local to each cluster. Port A is mapped to the cluster interconnect and serves read and write requests from the SIMT cores. Port B provides a dedicated read-only path directly to the Matrix Unit, ensuring that matrix data tiles can be streamed into the systolic array without interfering with core memory operations or arbitration bottlenecks.

---

### Memory Controller

```mermaid
flowchart TB
    subgraph Consumers ["Consumers (NUM_CONSUMERS)"]
        direction LR
        C_Req["consumer_read_valid<br/>consumer_write_valid<br/>address & data"]
    end

    subgraph MemCtrl ["Memory Controller (mem_controller.sv)"]
        direction TB
        Arbiter{"Channel Arbiter<br/>(NUM_CHANNELS)"}
        
        subgraph FSMs ["Channel FSMs"]
            direction LR
            IDLE(["IDLE"])
            WAITING(["WAITING<br/>(READ/WRITE)"])
            RELAYING(["RELAYING<br/>(READ/WRITE)"])
            IDLE -->|"request found"| WAITING
            WAITING -->|"mem_ready"| RELAYING
            RELAYING -->|"consumer ack"| IDLE
        end
        Arbiter -->|"Assigns requests to<br/>available channels"| FSMs
    end

    subgraph External ["External Memory Interface"]
        Mem["Global Memory Channel"]
    end

    C_Req --> Arbiter
    FSMs <-->|"mem_read/write_valid<br/>address, data, ready"| Mem
    FSMs -->|"consumer_ready<br/>consumer_read_data"| Consumers
```

The Memory Controller (`mem_controller.sv`) manages concurrent accesses to global memory (both instruction and data memory) from numerous consumers (Fetchers or LSUs). It instantiates multiple memory channels (`NUM_CHANNELS`) and tracks their state using independent FSMs. When a channel is idle, it scans the consumers to find a pending read or write request, locks onto it, and dispatches it to the external memory interface. Once the memory responds, the channel relays the data or acknowledgment back to the specific consumer and waits for a consumer acknowledgment before returning to the idle state.

---

## Default Hardware Resource Summary

The table below shows the concrete resource counts using the default parameters from `gpu.sv`. All parameters are configurable at synthesis time.

| Level | Resource | Count | Notes |
| :--- | :--- | :--- | :--- |
| **GPU** | Clusters | **1** | `NUM_CLUSTERS = 1` |
| | Data Memory Channels | 8 | `DATA_MEM_NUM_CHANNELS = 8` |
| | Instruction Memory Channels | 8 | `INSTRUCTION_MEM_NUM_CHANNELS = 8` |
| **Per Cluster** | Compute Cores | **2** | `CORES_PER_CLUSTER = 2` |
| | Matrix Unit | 1 | Systolic array, up to 8×8 tiles |
| | Shared Memory | 1 | 1024 words = 4 KB |
| | Cluster Interconnect | 1 | Address decode + arbitration |
| **Per Core** | Warps | **2** | `WARPS_PER_CORE = 2` |
| | Threads per Warp | **32** | `THREADS_PER_WARP = 32` |
| | ALUs (shared) | **33** | 32 per-thread + 1 scalar, time-multiplexed across warps |
| | LSUs (shared) | **33** | 32 per-thread + 1 scalar, time-multiplexed across warps |
| | Vector Register Files | **2** | 1 per warp (32 regs × 32 threads each) |
| | Scalar Register Files | **2** | 1 per warp (exec mask + scalar operands) |
| | Fetchers | **2** | 1 per warp (fetch in parallel) |
| | Decoders | **2** | 1 per warp |
| **GPU Totals** | Cores | 2 | 1 cluster × 2 cores |
| | Warps | 4 | 2 cores × 2 warps |
| | Threads | 128 | 4 warps × 32 threads |
| | ALUs | 66 | 2 cores × 33 ALUs |
| | LSUs | 66 | 2 cores × 33 LSUs |

---

## Directory Structure

The repository is structured to separate the hardware description, simulation harness, and tests:

| Directory / File | Description |
| :--- | :--- |
| `src/` | Contains all the SystemVerilog hardware source code. |
| `src/gpu.sv` | The top-level GPU module integrating clusters, dispatcher, and memory controllers. |
| `src/virgo_cluster.sv` | The top-level module for a Virgo cluster, instantiating cores, shared memory, and the matrix unit. |
| `src/matrix_unit.sv` | The systolic array implementation for matrix operations. |
| `src/matrix_controller.sv`| Handles MMIO requests and controls the matrix unit. |
| `src/compute_core.sv` | The SIMT core implementation containing warps, ALUs, and LSUs. |
| `src/common/` | Shared definitions, constants, memory map addresses (`VIRGO_SHARED_MEM_BASE`, etc.), and typedefs. |
| `sim/` | The Verilator simulation environment. Contains C++ code to wrap the Verilog model and create a standalone `simulator` binary, as well as a simple assembler. |
| `test/` | C++ test suites (using Doctest). Contains unit tests and full-system tests. |
| `test/gpu/virgo_tests.cpp` | Tests specifically for the Virgo architecture, including shared memory, MMIO, and GEMM functional verification. |
| `external/` | External dependencies (like `doctest.h`). |

---

## Getting Started: How to Run

### Prerequisites
To build and simulate the GPU, you will need:
*   [Verilator](https://www.veripool.org/wiki/verilator) (translates SystemVerilog to C++)
*   [CMake](https://cmake.org/) (for building the project)
*   A C++23 compatible compiler (e.g., `g++-14` or `clang++`)

### 1. Build the Simulator and Tests
From the root of the repository, create a build directory and compile the project:

```bash
mkdir -p build && cd build
cmake ..
make -j$(sysctl -n hw.ncpu)
```
This will generate the `simulator` executable in `build/sim/` and the test executable in `build/test/gpu/`.

### 2. Run the Assembly Simulator
The simulator reads an assembly file, assembles it, and runs the RTL simulation.

```bash
cd /path/to/mini_virgo
./build/sim/simulator example.as
```
*(Note: If you have a custom assembly file for GEMM, like `example_gemm.as`, simply pass it as the argument instead).*

The output will trace the execution and eventually dump the contents of memory to the console, allowing you to verify the results of the computation.

### 3. Run the Test Suite
To verify the functionality of the hardware, run the automated test suite:

```bash
# Run all tests
./build/test/gpu/virgo_tests

# Run a specific Virgo test (e.g., the GEMM tile test)
./build/test/gpu/virgo_tests -tc="Virgo Architecture: Dedicated Cluster Matrix Unit GEMM Tile*"
```
The test suite compiles specific assembly programs on the fly, runs the simulation, and asserts that the final memory state matches the expected software-computed results.

---

## Roadmap & Extending
This is an educational platform. Future areas for expansion include:
*   Adding new RISC-V instructions to the decoder (`src/decoder.sv`).
*   Enhancing the systolic array (`src/matrix_unit.sv`) to support different data types or larger tile sizes.
*   Writing custom assembly programs to benchmark memory latency and matrix operation throughput.
