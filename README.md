#  Linux Image w/ OP-TEE for PYNQ-Z2 boards

Forked from https://github.com/zakimadaoui/pynq-project1-linux-from-scratch, and a lot of his README contents are still here for informational purposes! Aiming to make a quick test environment for OP-TEE on PYNQ-Z2 boards.

### Suggested Environment

I run Xilinx products on my host windows OS and have a development environment for builds in WSL. I also have petalinux installed in WSL to have XSCT. Rather than running SD cards back and forth, there's a Gemini-generated boot TCL script in this repo to enable quicker booting. WSL2 can't access USBs properly off the bat, so you'll need to do this to boot a board if you're on a similar setup to me: run `C:\Xilinx\2025.1\Vivado\bin\hw_server.bat` on your host OS, and then run `xsct boot_jtag.tcl TCP:<IP>:3121`, replacing `<IP>` with the IPv4 address listed for `vEthernet (WSL)` when you run `ipconfig`.

If you still want to create a bootable SD card, you'll need to follow instructions from [https://github.com/zakimadaoui/pynq-project1-linux-from-scratch#How-to-format-SD-card-for-SD-boot](https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/18842385/How+to+format+SD+card+for+SD+boot) to format an SD card and then copy over relevant artifacts to the boot partition. Artifacts are in artifacts/ from the project root, and you'll need to copy all the files listed in the [Linux](#Simple-Image-Layout) or [OPTEE](#OPTEE-Image-Layout) layouts.

### Host Dependencies:
```bash
sudo apt install git wget
sudo apt install gcc-arm-none-eabi
sudo apt install bison flex openssl libssl-dev 
sudo apt-get install uuid-dev libgnutls28-dev 
sudo apt install pkg-config meson ninja-build # needed for building dtc
```

Make sure you also have XSCT installed and on your path. 

### Makefile usage:

Important Make Commands
```bash
make simple_image     # Linux only
make optee_image      # OP-TEE + Linux
make clean
```

It is also possible to build individual components of the boot chain. For exmaple:
```bash
make fsbl
make uboot
make dtb 
make kernel
make bootgen
make rootfs
#....
```

### Simple Image Layout
```
simple_image:
{
  [bootloader]artifacts/fsbl.elf
  artifacts/bitstream.bit
  [load=0x04000000, startup=0x04000000] artifacts/u-boot.bin
  [load=0x02000000] artifacts/boot.scr
  [load=0x03000000] artifacts/zImage
  [load=0x02A00000] artifacts/system.dtb
  [load=0x05000000] artifacts/uInitrd
}
```

### OPTEE Image Layout
```
optee_image:
{
  [bootloader]artifacts/fsbl.elf
  artifacts/bitstream.bit
  [load=0x04000000, startup=0x04000000] artifacts/u-boot.bin
  [load=0x02000000] artifacts/boot_tee.scr
  [load=0x03000000] artifacts/zImage
  [load=0x02A00000] artifacts/system.dtb
  [load=0x05000000] artifacts/uInitrd
  [load=0x10000000] artifacts/uTee
}
```

### OPTEE Boot Process
  - U-Boot Hands off to OPTEE_OS which does the following
  - Sets up the secure monitor 
  - Sets up the secure-memory regions
  - Configure which peripherals are secure/non-secure 
  - Configures hardware for secure world operations (Caches, MMU, SMP...)
  - Create MMU table for secure world
  - Extends the device-tree with an optee node, reserved memory area node and shared memory area node.
  - enter monitor mode with SMC
  - Switch NS bit to non-secure
  - Switch CPU mode to Svc and jump to linux start forwarding the arguments from uboot
  - Linux boot starts in NS world
  
