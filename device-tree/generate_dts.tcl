hsi open_hw_design hardware_design.xsa
hsi set_repo_path /tmp/device-tree-xlnx

set procs [hsi get_cells -hier -filter {IP_TYPE==PROCESSOR}]
puts "List of processors found in XSA is $procs"

hsi create_sw_design device-tree -os device_tree -proc ps7_cortexa9_0
hsi generate_target -dir dts
hsi close_hw_design [hsi current_hw_design]
exit