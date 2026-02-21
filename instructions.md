# Secure AXI Switch Peripheral — Build & Deploy Instructions

TrustZone-protected AXI peripheral that reads the PYNQ-Z2 slide switches, accessible only from OP-TEE's secure world. Includes a benchmark TA/CA for timing SMC calls and AXI read latency.

## Prerequisites

- **Windows**: Vivado 2024.2 installed at `C:\Xilinx\Vivado\2024.2\`
- **WSL**: Build dependencies from README.md + XSCT on PATH
- **Hardware**: PYNQ-Z2 in JTAG boot mode, USB connected, `hw_server.bat` running on Windows

## Step 1: Build Hardware (Windows)

Run the Vivado TCL script in batch mode. Two options depending on your setup:

**Option A** — Run from inside WSL (calls the Windows `.bat` directly):
```bash
cd ~/research/pynq-tee/pynq-project1-linux-from-scratch/vivado
/mnt/c/Xilinx/Vivado/2024.2/bin/vivado.bat -mode batch -source create_secure_switch_design.tcl
```

**Option B** — Copy to a Windows-native path first (avoids cross-filesystem issues):
```powershell
# PowerShell on Windows
Copy-Item -Recurse "C:\path\to\your\wsl\vivado" C:\temp\vivado_build
cd C:\temp\vivado_build
C:\Xilinx\Vivado\2024.2\bin\vivado.bat -mode batch -source create_secure_switch_design.tcl
```

The script will print the **peripheral address** (e.g., `0x43C00000`). If it differs from `0x43C00000`, update these two files:
- `patches/add_secure_switch_to_optee.patch` → `SECURE_SWITCH_BASE`
- `secure_switch/ta/secure_switch_ta.c` → `SECURE_SWITCH_PHYS`

Copy outputs to WSL:

```bash
cp vivado/output/hardware_design.xsa device-tree/hardware_design.xsa
cp vivado/output/bitstream.bit device-tree/simple_pynqz2_wrapper.bit
```

## Step 2: Build Software (WSL)

```bash
make optee_image
```

This builds everything: FSBL, U-Boot, OP-TEE (with secure switch MMIO mapping), kernel, rootfs (including the benchmark TA/CA), and packages it with the new bitstream.

## Step 3: Boot via JTAG

Start `hw_server.bat` on Windows, then from WSL:

```bash
xsct boot_jtag.tcl TCP:<WINDOWS_IP>:3121
```

No changes to `boot_jtag.tcl` are needed.

## Step 4: Verify

On the booted Linux console:

```bash
# Read switch state via secure TA
optee_benchmark_switch

# Benchmark 1000 iterations — prints min/avg/max for round-trip, AXI cycles, TA cycles
optee_benchmark_switch 1000

# Verify TrustZone protection — should FAIL with bus error
devmem 0x43C00000
```

## How It Works

```
Normal World (Linux)          Secure World (OP-TEE)          FPGA (PL)
┌─────────────────────┐      ┌──────────────────────┐      ┌──────────────┐
│ optee_benchmark_    │      │  secure_switch TA     │      │  AXI4-Lite   │
│ switch (CA)         │─SMC─▶│  phys_to_virt_io()   │─AXI─▶│  sw[1:0]     │
│                     │◀─────│  reads register 0     │◀─────│  register    │
└─────────────────────┘      └──────────────────────┘      └──────────────┘
│ devmem 0x43C00000   │──────── BLOCKED by TrustZone ──────▶│  ✗           │
```

- **SMC round-trip**: Measured by the CA via `clock_gettime(CLOCK_MONOTONIC)`
- **AXI read cycles**: Measured by the TA via ARM `PMCCNTR` cycle counter
- **SMC overhead**: Difference between round-trip time and TA execution cycles

## Files

| File | Purpose |
|------|---------|
| `vivado/create_secure_switch_design.tcl` | Batch-mode Vivado — creates project, builds bitstream + XSA |
| `vivado/secure_switch_axi.v` | AXI4-Lite slave — `reg0 = {30'b0, sw[1:0]}` |
| `vivado/pynqz2_switches.xdc` | Pin constraints (SW0=M20, SW1=M19) |
| `patches/add_secure_switch_to_optee.patch` | OP-TEE: maps PL peripheral into secure MMU |
| `secure_switch/ta/` | Trusted Application — MMIO read + cycle counting |
| `secure_switch/host/` | Client Application — TEE session + benchmark stats |

## Cross-Platform (AUP-ZU3)

The TA, CA, and Verilog RTL are portable. For ZynqMP:
- Override the peripheral address at TA build time: add `CFLAGS += -DSECURE_SWITCH_PHYS=0xA0000000` to the TA Makefile
- Create a separate Vivado TCL for the ZU3 part (`xczu3eg`) using `zynq_ultra_ps_e` IP
- OP-TEE platform changes go in `plat-zynqmp` instead of `plat-zynq7k`

## Dependency Versions

All cloned dependencies are pinned to specific commits in the Makefile header. Update the `*_SHA` variables when upgrading.
