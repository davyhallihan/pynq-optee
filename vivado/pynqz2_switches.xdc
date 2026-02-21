# PYNQ-Z2 Slide Switch Pin Constraints
# SW0 = M20, SW1 = M19 (active-high, directly connected to PL)

set_property PACKAGE_PIN M20 [get_ports {sw[0]}]
set_property IOSTANDARD LVCMOS33 [get_ports {sw[0]}]

set_property PACKAGE_PIN M19 [get_ports {sw[1]}]
set_property IOSTANDARD LVCMOS33 [get_ports {sw[1]}]
