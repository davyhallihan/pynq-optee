set url [lindex $argv 0]
if {$url == ""} {
    set url "TCP:127.0.0.1:3121"
}
connect -url $url
puts "Connected to hw_server"
puts "Available targets:"
set target_list [targets]
puts $target_list

if {[llength $target_list] == 0} {
    puts "ERROR: No targets found. Please check:"
    puts "1. Is the board powered on?"
    puts "2. Is the USB/JTAG cable connected?"
    puts "3. Do you have the necessary cable drivers installed?"
    puts "4. Is the boot mode jumper set correctly (JTAG mode)?"
    exit 1
}

if {[catch {targets -set -nocase -filter {name =~ "arm*#0"}} err]} {
    puts "Error selecting ARM target: $err"
    puts "Trying to select any target to inspect..."
    # Fallback or just exit
    exit 1
}
rst -system
after 1000

puts "Loading Bitstream..."
fpga artifacts/bitstream.bit

puts "Loading FSBL..."
dow artifacts/fsbl.elf
con
after 5000
stop

puts "Loading U-Boot..."
dow artifacts/u-boot.elf

puts "Loading Kernel..."
dow -data artifacts/zImage 0x03000000

puts "Loading Device Tree..."
dow -data artifacts/system.dtb 0x02A00000

puts "Loading Initrd..."
dow -data artifacts/uInitrd 0x05000000

puts "Loading TEE..."
dow -data artifacts/uTee 0x10000000

puts "Loading Boot Script..."
dow -data artifacts/boot_tee.scr 0x02000000

puts "Booting..."
con
