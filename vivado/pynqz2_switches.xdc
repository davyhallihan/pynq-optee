# PYNQ-Z2 Slide Switch Pin Constraints
#
# From PYNQ-Z2 schematic: SW0 and SW1 are directly connected to PL I/O
# on an HR (high-range) bank powered at 3.3V, hence LVCMOS33.
#
# This design only uses 2 switches (matching the secure_switch_axi peripheral).
# Both instances (secure_switch_0, ns_switch_0) share the same sw[1:0] input.

# --- SW0 ---
set_property PACKAGE_PIN M20 [get_ports {sw[0]}]
set_property IOSTANDARD LVCMOS33 [get_ports {sw[0]}]

# --- SW1 ---
set_property PACKAGE_PIN M19 [get_ports {sw[1]}]
set_property IOSTANDARD LVCMOS33 [get_ports {sw[1]}]
