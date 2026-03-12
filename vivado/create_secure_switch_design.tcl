# create_secure_switch_design.tcl -- PYNQ-Z2 (Zynq-7000)
#
# Creates a Vivado block design with two instances of secure_switch_axi
# connected to the Zynq PS via an AXI interconnect:
#
#   PS7 M_AXI_GP0 --> AXI Interconnect --> M00: secure_switch_0  (TZ-protected)
#                                      --> M01: ns_switch_0      (non-secure)
#
# Both peripherals read the same physical switches. The only difference is
# that M00 has CONFIG.M00_SECURE=1, which makes the interconnect reject
# AXI transactions where AxPROT[1]=1 (non-secure). This means only OP-TEE
# (running in secure world) can access secure_switch_0.
#
# IMPORTANT Zynq-7000 limitation:
#   When M00_SECURE=1, the AXI Interconnect IP blocks ALL non-secure
#   transactions on the entire interconnect -- not just M00. This means
#   --ns-axi can't work when M00_SECURE is enabled. We work around this
#   with two separate bitstreams:
#
#   vivado -mode batch -source create_secure_switch_design.tcl -tclargs secure
#     -> M00_SECURE=1, use for --smc and --smc-axi benchmarks
#
#   vivado -mode batch -source create_secure_switch_design.tcl -tclargs nosecure
#     -> M00_SECURE off, use for --ns-axi benchmarks
#
#   (The ZU3 does per-port filtering correctly, so it doesn't need this.)
#
# Outputs:
#   output/hardware_design.xsa
#   output/bitstream.bit

# --- Parse "secure" or "nosecure" flag (default: nosecure) ---
if {[llength $argv] > 0 && [lindex $argv 0] eq "secure"} {
    set ENABLE_M00_SECURE 1
    puts "*** BUILD MODE: SECURE (M00_SECURE=1) — non-secure AXI will be blocked ***"
} else {
    set ENABLE_M00_SECURE 0
    puts "*** BUILD MODE: NOSECURE (M00_SECURE off) — all AXI access allowed ***"
}

set script_dir [file dirname [file normalize [info script]]]
set proj_dir   [file join $script_dir "vivado_project"]
set output_dir [file join $script_dir "output"]

file mkdir $output_dir

# --- Create project ---
puts "=== Creating Vivado project ==="
create_project secure_switch_pynqz2 $proj_dir -part xc7z020clg400-1 -force

if {[catch {set_property board_part tul.com.tw:pynq-z2:part0:1.0 [current_project]} err]} {
    puts "WARNING: Could not set board_part: $err"
}

# --- Add RTL and constraints ---
puts "=== Adding source files ==="
add_files -norecurse [file join $script_dir "secure_switch_axi.v"]
add_files -fileset constrs_1 -norecurse [file join $script_dir "pynqz2_switches.xdc"]
update_compile_order -fileset sources_1

# --- Block design ---
puts "=== Creating block design ==="
create_bd_design "system"

# Zynq PS7
create_bd_cell -type ip -vlnv xilinx.com:ip:processing_system7:5.5 ps7

if {[catch {apply_board_connection -board_interface "ddr" -ip_intf "ps7/DDR" -diagram "system"} err]} {
    puts "INFO: Board automation not available, configuring PS manually."
}

set_property -dict [list \
    CONFIG.PCW_USE_M_AXI_GP0 {1} \
    CONFIG.PCW_EN_CLK0_PORT {1} \
    CONFIG.PCW_FPGA0_PERIPHERAL_FREQMHZ {100} \
    CONFIG.PCW_UART0_PERIPHERAL_ENABLE {1} \
    CONFIG.PCW_UART0_UART0_IO {MIO 14 .. 15} \
    CONFIG.PCW_EN_UART0 {1} \
    CONFIG.PCW_UIPARAM_DDR_PARTNO {MT41K256M16 RE-125} \
    CONFIG.PCW_UIPARAM_DDR_MEMORY_TYPE {DDR 3} \
    CONFIG.PCW_UIPARAM_DDR_FREQ_MHZ {525} \
    CONFIG.PCW_USB0_PERIPHERAL_ENABLE {1} \
    CONFIG.PCW_USB0_USB0_IO {MIO 28 .. 39} \
    CONFIG.PCW_ENET0_PERIPHERAL_ENABLE {1} \
    CONFIG.PCW_ENET0_ENET0_IO {MIO 16 .. 27} \
    CONFIG.PCW_ENET0_GRP_MDIO_ENABLE {1} \
    CONFIG.PCW_ENET0_GRP_MDIO_IO {MIO 52 .. 53} \
    CONFIG.PCW_SD0_PERIPHERAL_ENABLE {1} \
    CONFIG.PCW_SD0_SD0_IO {MIO 40 .. 45} \
    CONFIG.PCW_SD0_GRP_CD_ENABLE {1} \
    CONFIG.PCW_SD0_GRP_CD_IO {MIO 47} \
    CONFIG.PCW_QSPI_PERIPHERAL_ENABLE {1} \
    CONFIG.PCW_QSPI_QSPI_IO {MIO 1 .. 6} \
    CONFIG.PCW_GPIO_MIO_GPIO_ENABLE {1} \
    CONFIG.PCW_GPIO_EMIO_GPIO_ENABLE {1} \
    CONFIG.PCW_GPIO_EMIO_GPIO_WIDTH {64} \
    CONFIG.PCW_TTC0_PERIPHERAL_ENABLE {1} \
] [get_bd_cells ps7]

# Two instances of our switch peripheral
create_bd_cell -type module -reference secure_switch_axi secure_switch_0
create_bd_cell -type module -reference secure_switch_axi ns_switch_0

# AXI interconnect with 2 master ports (one per peripheral)
create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 axi_interconnect_0
set_property CONFIG.NUM_MI {2} [get_bd_cells axi_interconnect_0]

# --- Wiring ---
puts "=== Wiring block design ==="

# Clock: PS FCLK_CLK0 (100 MHz) drives everything
connect_bd_net [get_bd_pins ps7/FCLK_CLK0] [get_bd_pins ps7/M_AXI_GP0_ACLK]
connect_bd_net [get_bd_pins ps7/FCLK_CLK0] [get_bd_pins axi_interconnect_0/ACLK]
connect_bd_net [get_bd_pins ps7/FCLK_CLK0] [get_bd_pins axi_interconnect_0/S00_ACLK]
connect_bd_net [get_bd_pins ps7/FCLK_CLK0] [get_bd_pins axi_interconnect_0/M00_ACLK]
connect_bd_net [get_bd_pins ps7/FCLK_CLK0] [get_bd_pins axi_interconnect_0/M01_ACLK]
connect_bd_net [get_bd_pins ps7/FCLK_CLK0] [get_bd_pins secure_switch_0/s_axi_aclk]
connect_bd_net [get_bd_pins ps7/FCLK_CLK0] [get_bd_pins ns_switch_0/s_axi_aclk]

# Reset: PS FCLK_RESET0_N
connect_bd_net [get_bd_pins ps7/FCLK_RESET0_N] [get_bd_pins axi_interconnect_0/ARESETN]
connect_bd_net [get_bd_pins ps7/FCLK_RESET0_N] [get_bd_pins axi_interconnect_0/S00_ARESETN]
connect_bd_net [get_bd_pins ps7/FCLK_RESET0_N] [get_bd_pins axi_interconnect_0/M00_ARESETN]
connect_bd_net [get_bd_pins ps7/FCLK_RESET0_N] [get_bd_pins axi_interconnect_0/M01_ARESETN]
connect_bd_net [get_bd_pins ps7/FCLK_RESET0_N] [get_bd_pins secure_switch_0/s_axi_aresetn]
connect_bd_net [get_bd_pins ps7/FCLK_RESET0_N] [get_bd_pins ns_switch_0/s_axi_aresetn]

# AXI data path: PS -> interconnect -> peripherals
connect_bd_intf_net [get_bd_intf_pins ps7/M_AXI_GP0] [get_bd_intf_pins axi_interconnect_0/S00_AXI]
connect_bd_intf_net [get_bd_intf_pins axi_interconnect_0/M00_AXI] [get_bd_intf_pins secure_switch_0/s_axi]
connect_bd_intf_net [get_bd_intf_pins axi_interconnect_0/M01_AXI] [get_bd_intf_pins ns_switch_0/s_axi]

# TrustZone: optionally mark M00 as secure-only
if {$ENABLE_M00_SECURE} {
    set_property CONFIG.M00_SECURE {1} [get_bd_cells axi_interconnect_0]
    puts "INFO: M00_SECURE=1 set on axi_interconnect_0"
}

# Both peripherals share the same physical switch pins
create_bd_port -dir I -from 1 -to 0 sw
connect_bd_net [get_bd_ports sw] [get_bd_pins secure_switch_0/sw]
connect_bd_net [get_bd_ports sw] [get_bd_pins ns_switch_0/sw]

# DDR and fixed I/O
apply_bd_automation -rule xilinx.com:bd_rule:processing_system7 \
    -config {make_external "FIXED_IO, DDR"} [get_bd_cells ps7]

# --- Address assignment ---
puts "=== Assigning addresses ==="
assign_bd_address

puts "=============================================="
foreach {inst label} {secure_switch_0 "SECURE" ns_switch_0 "NON-SECURE"} {
    set addr_segs [get_bd_addr_segs -of_objects [get_bd_intf_pins ${inst}/s_axi]]
    foreach seg $addr_segs {
        set offset [get_property OFFSET $seg]
        set range  [get_property RANGE $seg]
        puts "  $label PERIPHERAL ($inst): $offset  RANGE: $range"
    }
}
puts "  Use secure address for CFG_SWITCH_BASE in OP-TEE build"
puts "  Use non-secure address for NS_SWITCH_ADDR in host app build"
puts "=============================================="

# --- Validate and save ---
puts "=== Validating design ==="
validate_bd_design
save_bd_design

set wrapper [make_wrapper -files [get_files system.bd] -top]
add_files -norecurse $wrapper
update_compile_order -fileset sources_1

# --- Build ---
puts "=== Running synthesis ==="
launch_runs synth_1 -jobs 8
wait_on_run synth_1
if {[get_property STATUS [get_runs synth_1]] != "synth_design Complete!"} {
    puts "ERROR: Synthesis failed!"
    exit 1
}

puts "=== Running implementation + bitstream ==="
launch_runs impl_1 -to_step write_bitstream -jobs 8
wait_on_run impl_1
if {[get_property STATUS [get_runs impl_1]] != "write_bitstream Complete!"} {
    puts "ERROR: Implementation/bitstream failed!"
    exit 1
}

# --- Export ---
puts "=== Exporting XSA and bitstream ==="

write_hw_platform -fixed -include_bit -force [file join $output_dir "hardware_design.xsa"]

set bit_file [glob -nocomplain [file join $proj_dir "secure_switch_pynqz2.runs" "impl_1" "*.bit"]]
if {[llength $bit_file] > 0} {
    file copy -force [lindex $bit_file 0] [file join $output_dir "bitstream.bit"]
} else {
    puts "WARNING: Could not find .bit file to copy"
}

puts ""
puts "=============================================="
puts "  BUILD COMPLETE"
puts "  XSA: [file join $output_dir hardware_design.xsa]"
puts "  BIT: [file join $output_dir bitstream.bit]"
puts ""
puts "  Next: copy to the build directory:"
puts "    cp vivado/output/hardware_design.xsa device-tree/hardware_design.xsa"
puts "    cp vivado/output/bitstream.bit device-tree/simple_pynqz2_wrapper.bit"
puts "=============================================="

exit
