# Building and booting linux-based image from scratch on Pynq-z2 (zynq-7000 SoC) with TrustZone and Secure Boot support 

This is an educational bring-up project intended as documentation for my future self and anyone interested in learning the full Zynq-7000 boot chain, Secure-boot and TrustZone integration.

The build system in this project intentionally avoids Yocto/Buildroot and instead uses simple Makefiles + shell scripts to expose every step of the process.

A follow-up production-oriented version of this project (Yocto-based) is being worked on here:  
https://github.com/zakimadaoui/pynq-project2-custom-yocto-build-system

## Learning objectives 
- Learn/Document how to Build a complete Linux system from scratch
- Understand the Zynq-7000 SoC boot process  
- Learn about u-boot
- Understand and implement secure boot on Zynq-7000
- Learn about bringing-up and integrating a trusted execution environment like OPTEE alongside linux
- Learn anything else that comes up along the way :)

## Acheived so far, and shortly how?
- Basic bootable linux image
  - patch Xilinx FSBL to add Pynq-Z2 board support
  - patch u-boot to add Pynq-Z2 board support
  - generate device-tree for pynq-z2 baord + basic IP in the PL using Xilinx tools 
  - build FSBL, device-tree, Linux kernel, U-boot, Busybox
  - Create a basic initramfs that includes busybox, simple init scripts and glibc
  - Assemble everything to a bootable image using Xilinx bootgen tool

- Bootable image with a TEE alonside Linux. This is the most complex part — Zynq-7000 support for OP-TEE has been abandoned years ago and the Linux BSP assumes secure-world access ONLY, so significant work was required. That included:
  - Patch OPTEE-OS to:
    - Add Pynq-Z2 baord support 
    - Configure secure memory layout valid for pynq-z2 512MB DDR
    - fix crashes in early secure-world boot
  - Patch the Xilinx BSP within linux kernel to avoid accesses to secure registers/peripherals (which would cause exceptions at boot).  
  - Patch linux kernel configuration to allow enabling OPTEE driver for zynq-7000
  - Craft a custom linux device tree with fixed clocks and only perepherals accessible from Non-secure world (without this kernel panics at boot at very early stages)
  - Configure linux kernel with optee support
  - Configure u-boot to boot to optee instead of linux, and optee to jump to Linux
  - Configure optee to patch device tree at boot to add reserved and shared memory nodes
  - Build optee_os, optee_client, optee_examples, FSBL,  device-tree, u-boot, linux kernel, busybox
  - Create initramfs with busybox, glibc, optee_client and optee_examples
  - Assemble everything to a bootable image using Xilinx bootgen tool
  
### TBD / Work in progress :
- Secure bootable Image
- Rust trustlets/secure services

## Build system usage
This project provides a very simple makefile based buildsystem. But, before going into the make commands, the following host dependencies are needed.

### Host Dependencies:
```bash
sudo apt install gcc-arm-linux-gnueabihf # and dependencies
sudo apt install gcc-arm-none-eabi # and dependencies
sudo apt install bison flex openssl libssl-dev 
sudo apt-get install uuid-dev libgnutls28-dev 
sudo apt install pkg-config meson ninja-build # needed for building dtc
```
TBD: maybe a Dockerfile would be better at somepoint...

### Makefile usage:
Use one of the following 3 commands, depending on what final image you are after

```bash
make simple_image     # Linux only
make optee_image      # OP-TEE + Linux
make secure_image     # Enable secure boot chain (WIP)
```

To clean any image build use:
```bash
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

The build outputs will be placed in a directory called `artifacts`
`make simple_image` produces `artifacts/BOOT.bin`
`make optee_image` produces `artifacts/BOOT-optee.bin`
`make secure_image` produces `artifacts/BOOT-secure.bin`

All you have to do is to copy the `BOOT*.bin` image to an SD card under the name `BOOT.bin`. The partion where `BOOT.bin` is copied must be created according to the instructions mentioned in [How to format SD card for SD bootd](#How-to-format-SD-card-for-SD-boot) section.


## Memory layouts and boot process for different images
### Layout of simple image
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

At boot the memory layout looks like this:
```
 +---------------------------------------+  <- 0x0000_0000
 |            On-chip SRAM / OCM1(64KB)  |  # FSBL runs here first, then SRAM is free for use
 |            On-chip SRAM / OCM2(64KB)  |  
 |            On-chip SRAM / OCM3(64KB)  |  
 +---------------------------------------+  <- 0x0003_0000
 |              Reserved                 |  
 +---------------------------------------+  <- 0x0010_0000 (DDR base)
 |               unused                  |
 +---------------------------------------+  <- 0x0200_0000
 |              boot.scr                 |
 +---------------------------------------+  <- 0x02A0_0000
 |              system.dtb               |
 +---------------------------------------+  <- 0x0300_0000
 |               zImage                  |
 +---------------------------------------+  <- 0x0400_0000
 |               u-boot.bin              |
 +---------------------------------------+  <- 0x0500_0000
 |               uInitrd (initramfs)     |
 +---------------------------------------+  <- 0x05xx_xxxx
 |              Free / unused DDR        |
 +---------------------------------------+  <- 0x2000_0000  (end of 512MB DDR)
 |              Empty / Reserved         |
 +---------------------------------------+  <- 0xFFFF_0000  
 |            On-chip SRAM / OCM4(64KB)  | 
 +---------------------------------------+  <- 0xFFFF_FFFF
```
### Basic boot process
1. BootROM loads FSBL from non-volatile memory into OCM and executes it.

2. FSBL:
   - Initializes clocks, DDR controller, and MIO.
   - Loads the PL bitstream.
   - Loads all remaining BIF artifacts into DDR at the specified BIF load addresses
   - Jumps to U-Boot’s startup address (0x04000000).

3. U-Boot:
   - Executes boot.scr.
      - Updates the DTB (adds memory node, MAC address, chosen nodes, etc.).
      - Passes the DTB pointer and initrd start/end to the kernel loader.
      - Calls `bootz` to start Linux with:
          - zImage at 0x03000000
          - patched FDT pointer
          - initrd at 0x05000000

4. Linux kernel:
   - Parses the DTB passed from U-Boot.
   - Loads/initramfs from the initrd region.
   - After consuming initrd, releases the memory region and the remainder of DDR becomes available as normal system RAM.

[![](https://img.plantuml.biz/plantuml/svg/NP9DJm8n48Rl_HLpHBEBcsXYmi4W874L0W6yw7heRWTesBROTXKm_dVRBX-1NZhJTpwUcRHjBDMs4YMnt9O8KoxMxdhVog_uFaaIw2XbeL-g7q8-hZyXZX_2qsGq96HlG0i6YZBeJpi3uD8g67M7cQII0Mwmg2oUGdr-Y6l1imjGypaXUp-Lt49Hgc9b8kZr9X4Cqz5130t60yYvDLOZ5MT29-s36uFFQM5DZU0AP3A6tFLjca9xssoDHT5aDlVUBXLqWTwguX2LSnaypw5PNT_ZleKLjE2b4cjmbPDDSfU4GyX1UwhdFBOx2cDgYoNqPXre21HARq4gXb4cXuzTe8nfD6xQOgaCoj-_2D0MpQjJjiOrrGWhr19BD-B2PB9HaqVGxyqYDwodFQHutW6NrUQrpcW5efrdlZqmd-jZgWrwm8jtlzSRNxpkGCNSN4G4aMd-ZTgSL_KKVek4j54ozxd-0G00)](https://editor.plantuml.com/uml/NP9DJm8n48Rl_HLpHBEBcsXYmi4W874L0W6yw7heRWTesBROTXKm_dVRBX-1NZhJTpwUcRHjBDMs4YMnt9O8KoxMxdhVog_uFaaIw2XbeL-g7q8-hZyXZX_2qsGq96HlG0i6YZBeJpi3uD8g67M7cQII0Mwmg2oUGdr-Y6l1imjGypaXUp-Lt49Hgc9b8kZr9X4Cqz5130t60yYvDLOZ5MT29-s36uFFQM5DZU0AP3A6tFLjca9xssoDHT5aDlVUBXLqWTwguX2LSnaypw5PNT_ZleKLjE2b4cjmbPDDSfU4GyX1UwhdFBOx2cDgYoNqPXre21HARq4gXb4cXuzTe8nfD6xQOgaCoj-_2D0MpQjJjiOrrGWhr19BD-B2PB9HaqVGxyqYDwodFQHutW6NrUQrpcW5efrdlZqmd-jZgWrwm8jtlzSRNxpkGCNSN4G4aMd-ZTgSL_KKVek4j54ozxd-0G00)

### Layout of image with TEE / TrustZone support
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

At boot the memory layout looks like this:
```
 +---------------------------------------+  <- 0x0000_0000
 |            On-chip SRAM / OCM1(64KB)  |  # FSBL runs here first, then SRAM is free for use
 |            On-chip SRAM / OCM2(64KB)  |  
 |            On-chip SRAM / OCM3(64KB)  |  
 +---------------------------------------+  <- 0x0003_0000
 |              Reserved                 |  
 +---------------------------------------+  <- 0x0010_0000 (DDR base)
 |               unused                  |
 +---------------------------------------+  <- 0x0200_0000
 |              boot.scr                 |
 +---------------------------------------+  <- 0x02A0_0000
 |              system.dtb               |
 +---------------------------------------+  <- 0x0300_0000
 |               zImage                  |
 +---------------------------------------+  <- 0x0400_0000
 |               u-boot.bin              |
 +---------------------------------------+  <- 0x0500_0000
 |               uInitrd (initramfs)     |
 +---------------------------------------+  <- 0x05xx_xxxx
 |              Free / unused DDR        |
 +---------------------------------------+  <- 0x1000_0000 (TZDRAM_BASE)
 | uTee (optee_os)    |                  |   ^
 |                    |  TEE_RAM (4MB)   |   |   
 | TEE private secure |                  |   |
 |   external memory  +------------------+   |TZDRAM_SIZE (32MB)
 |                    |                  |   |
 |                    |  TA_RAM (28MB)   |   |
 |                    |                  |   |
 +---------------------------------------+   v 0x1200_0000 (TEE_SHMEM_START)
 |                    |                  |   ^
 |     Non secure     |  SHM             |   | TEE_SHMEM_SIZE (1MB)
 |   shared memory    |                  |   |
 +---------------------------------------+   v 0x1210_0000
 |              Free / unused DDR        |
 +---------------------------------------+  <- 0x1FFF_FFFF  (end of 512MB DDR)
 |              Empty / Reserved         |
 +---------------------------------------+  <- 0xFFFF_0000  
 |            On-chip SRAM / OCM4(64KB)  | 
 +---------------------------------------+  <- 0xFFFF_FFFF
```
### Boot process

Similar to the simple boot process described earlier, but instead of u-boot handing-off to linux, u-boot hands-off to optee_os which:
  - Sets up the secure monitor 
  - Sets up the secure-memory regions
  - Configure which peripherals are secure/non-secure 
  - Configures hardware for secure world operations (Caches, MMU, SMP...)
  - Create MMU table for secure world
  - Extends the device-tree with an optee node, reserved memory area node and shared memory area node.
  - enter monitor mode with SMC
  - Switch NS bit to non-secure
  - Switch CPU mode to Svc and jump to linux start forwarding the arguments from uboot
  
then the linux kernel starts its boot process in non-secure world

[![](https://img.plantuml.biz/plantuml/svg/NLDDRzim3BthL_2O3dRcOWC615ZHnKcmRF4SnEcbymwAJITQ8ocJw3RfiFy-oOxpKPz8FVBnFOA-jyGDrNBBID0MuV6W_eIVeYY2eTO4Bu8si1oF6VlvxXU4uLtpl4WJneu1362cUGcJR3W3eKX36YUvuge4t6DH4vvBVNzJDu2lha1N5MF7VAe4GR7LnPCDO3HQ191Cqmvi1indi19aoI2NLy1mEW7dGLIy8DjAS0Iv2c7bvlPjONni3vPGzajQ-UnrAlaQVLI7JTLAg2RpEapPydkv3yqQ1l2jbZjmpD4--XWr7sCTDcvzdiQTW3cdOWEZvH1w4gKs1r2wn21NoPSOU5aQjCvjiT4MrTz_0I0L_Pjpjy4TDmXFQ1HkdSMrpLMlLHx0PBGCd5cljclnUjCT8QRMjNirqkczZJgzPon3gdTWdMo3eTIEHPiBEDQg4ck7mOQRyiMBo5MbpR43z0uD9w6LlUHCuekibxIiDtuMPF6Y_oFxV7igy59dGjNxxwsvWQCcYESFpX9apmL8joXep-ZtUVKVEYlikddEZKM_w3Vl_ZxU-oMrc-Aoicyg6h0quXcDlMRr4lqri7jKfVz1_WC0)](https://editor.plantuml.com/uml/NLDDRzim3BthL_2O3dRcOWC615ZHnKcmRF4SnEcbymwAJITQ8ocJw3RfiFy-oOxpKPz8FVBnFOA-jyGDrNBBID0MuV6W_eIVeYY2eTO4Bu8si1oF6VlvxXU4uLtpl4WJneu1362cUGcJR3W3eKX36YUvuge4t6DH4vvBVNzJDu2lha1N5MF7VAe4GR7LnPCDO3HQ191Cqmvi1indi19aoI2NLy1mEW7dGLIy8DjAS0Iv2c7bvlPjONni3vPGzajQ-UnrAlaQVLI7JTLAg2RpEapPydkv3yqQ1l2jbZjmpD4--XWr7sCTDcvzdiQTW3cdOWEZvH1w4gKs1r2wn21NoPSOU5aQjCvjiT4MrTz_0I0L_Pjpjy4TDmXFQ1HkdSMrpLMlLHx0PBGCd5cljclnUjCT8QRMjNirqkczZJgzPon3gdTWdMo3eTIEHPiBEDQg4ck7mOQRyiMBo5MbpR43z0uD9w6LlUHCuekibxIiDtuMPF6Y_oFxV7igy59dGjNxxwsvWQCcYESFpX9apmL8joXep-ZtUVKVEYlikddEZKM_w3Vl_ZxU-oMrc-Aoicyg6h0quXcDlMRr4lqri7jKfVz1_WC0)

## Resources 
Git repos, see: 
https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/18842156/Fetch+Sources

Link                                         | Content
---------------------------------------------------------|----------------------------------------------
https://github.com/Xilinx/linux-xlnx.git                 | The Linux kernel with Xilinx patches and drivers
https://github.com/Xilinx/u-boot-xlnx.git                | The u-boot bootloader with Xilinx patches and drivers
https://github.com/Xilinx/device-tree-xlnx.git           | Device Tree generator plugin for xsdk
https://git.kernel.org/pub/scm/utils/dtc/dtc.git         | Device Tree compiler (required to build U-Boot)
https://github.com/Xilinx/embeddedsw.git                 | Xilinx embeddedsw repository for bare-metal applications such as FSBL, PMU Firmware, PLM
https://pynq.readthedocs.io/en/v2.7.0/overlay_design_methodology/board_settings.html | Link to Pynq Z1 and Z2 Vivado board files  


### pynq-z2 board support in vivado
C:\Xilinx\Vivado\2021.2\data\boards
Where C:\Xilinx is the default path where Vivado is installed
Add a new folder and name it ‘board_files’
Download Pynq board files from this link
https://pynq.readthedocs.io/en/v2.7.0/overlay_design_methodology/board_settings.html
Extract board files then copy them to ‘board_files’ folder
Restart Vivado


https://www.youtube.com/watch?v=3D2-OPArCiA&list=PLAIW2DtZ4NI77mo2qFhvZKGZ1n1W8vWYY&index=16


###  FSBL manual build instructions:
- https://github.com/Xilinx/embeddedsw/blob/master/lib/sw_apps/zynq_fsbl/misc/Readme.txt

### Generating and compiling the device tree:
- https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/18842279/Build+Device+Tree+Blob
- https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/341082130/Quick+guide+to+Debugging+Device+Tree+Generator+Issues

### Device-tree tweaking 101
https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/18842482/Device+Tree+Tips

### building U-boot
- https://docs.u-boot.org/en/latest/board/xilinx/zynq.html
- https://github.com/Xilinx/u-boot-xlnx 

### How to format SD card for SD boot

https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/18842385/How+to+format+SD+card+for+SD+boot


## QEMU
To test u-boot

```bash
qemu-system-arm \
  -M xilinx-zynq-a9 \
  -m 1024M \
  -serial mon:stdio \
  -serial null \
  -kernel artifacts/u-boot.elf 
  -nographic
  # -sd sdcardfile 
```

Or to boot a Linux kernel:

```bash
qemu-system-arm \
  -M xilinx-zynq-a9 \
  -m 512M \
  -serial mon:stdio \
  -kernel artifacts/uImage \
  -initrd artifacts/initramfs.cpio.gz \
  -dtb artifacts/system.dtb \
  -append "console=ttyPS0,115200 earlyprintk"
  -nographic
```