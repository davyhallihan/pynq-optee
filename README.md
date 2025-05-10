# TODO:
- you should checkout https://github.com/Xilinx/bootgen
- you can build bootgen locally to avoid having it as an external user dependency

# Building and booting linux from scratch on Pynq-z2

## Learning objectives 
- Learn about the xilinx zynq 7000 SoC boot process
- patch, configure and compile:
  - Xilinx first stage boot loader (FSBL)
  - U-boot 
  - Linux kernel 
  - Device tree for pynq-z2
  - busybox
  - create a simple initramfs
- Create a bootable image for zynq 7000 SoC/pynq-z2 board which gives access to u-boot
- use u-boot to load linux kernel, device tree and rootfs and boot to linux 
- simple statup script to get a operation linux os.

## General steps for booting linux from scratch
-  Build FSBL
-  Build U-boot 
-  Build and modify a root filesystem
-  Build a device tree blob
-  Build the linux kernel
-  Create a boot image using bootgen or zynq-mkbootimage (opensource from antmicro) 
-  Prepare the boot medium. In my case SDCARD with FAT32 fs and copy the files into it


## Dependencies:
- Download Xilinx bootgen and PATH must include it
- download the cross compiler
```bash
sudo apt install gcc-arm-linux-gnueabihf # and dependencies
sudo apt install gcc-arm-none-eabi-gcc # and dependencies
sudo apt install bison flex openssl libssl-dev 
sudo apt-get install uuid-dev libgnutls28-dev 
sudo apt install pkg-config meson ninja-build # needed for building dtc

```

## Fetch Sources
https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/18842156/Fetch+Sources

All sources are kept under version control using git. These repositories are publicly available through https://github.com/xilinx.

The following table gives an overview of the relevant repositories:
Repository Name	                                         | Content
---------------------------------------------------------|----------------------------------------------
https://github.com/Xilinx/linux-xlnx.git                 | The Linux kernel with Xilinx patches and drivers
https://github.com/Xilinx/u-boot-xlnx.git                | The u-boot bootloader with Xilinx patches and drivers
https://github.com/Xilinx/device-tree-xlnx.git           | Device Tree generator plugin for xsdk
https://git.kernel.org/pub/scm/utils/dtc/dtc.git         | Device Tree compiler (required to build U-Boot)
https://github.com/Xilinx/embeddedsw.git                 | Xilinx embeddedsw repository for bare-metal applications such as FSBL, PMU Firmware, PLM
https://pynq.readthedocs.io/en/v2.7.0/overlay_design_methodology/board_settings.html | Link to Pynq Z1 and Z2 Vivado board files  
https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/18842385/How+to+format+SD+card+for+SD+boot



## Vivado design
C:\Xilinx\Vivado\2021.2\data\boards
Where C:\Xilinx is the default path where Vivado is installed
Add a new folder and name it ‘board_files’
Download Pynq board files from this link
https://pynq.readthedocs.io/en/v2.7.0/overlay_design_methodology/board_settings.html
Extract board files then copy them to ‘board_files’ folder
Restart Vivado


https://www.youtube.com/watch?v=3D2-OPArCiA&list=PLAIW2DtZ4NI77mo2qFhvZKGZ1n1W8vWYY&index=16



## Bootgen/mkzynqboot
TBD


## FSBL for zynq:
- https://github.com/Xilinx/embeddedsw/blob/master/lib/sw_apps/zynq_fsbl/misc/Readme.txt for build
- https://docs.amd.com/r/en-US/ug1283-bootgen-user-guide
- https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/18841798/Build+FSBL
- https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/439124055/Zynq-7000+FSBL


Zynq FSBL has 3 directories.
	1. data - It contains files for SDK
	2. src  - It contains the FSBL source files
	3. misc - It contains miscellaneous files required to
		  compile FSBL.
		  Builds for zc702, zc706 and zed boards are supported.

How to compile Zynq FSBL:
    0. 
```bash
git clone https://github.com/Xilinx/embeddedsw
```
	1.Go to the Fsbl src directory "lib/sw_apps/zynq_fsbl/src/"
	2. make "BOARD=<>" "CC=<>"
		a. Values for BOARD  are zc702, zc706, zed
		b. Value for CC is arm-none-eabi-gcc. Default value is also same.
	3.Give "make" to compile the fsbl with BSP. By default it is
	  built for zc702 board with arm-none-eabi-gcc compiler
	4.Below are the examples for compiling for different options
		a. To generate Fsbl for zc706 board
			i.make "BOARD=zc706"
		b.To generate Fsbl for zc702 board with debug enable
		  and RSA support
			i.make "BOARD=zc702" "CFLAGS=-DFSBL_DEBUG_INFO -DRSA_SUPPORT"
		c.To generate Fsbl for zc706 board and compile with arm-none-eabi-gcc
		  with MMC support
			i.make "BOARD=zc706" "CC=arm-none-eabi-gcc" "CFLAGS=-DMMC_SUPPORT"


## Generating and compiling the device tree:
- https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/18842279/Build+Device+Tree+Blob
- https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/341082130/Quick+guide+to+Debugging+Device+Tree+Generator+Issues

**Important note for the future** when desiring to extend the device tree in a yocto project or similar:
Since the DTG uses the XSA as an input file, and the XSA only contains IPs that are in the Block Design, then for the same reason the DTS output from the DTG will only have DT nodes for the IP in the BD. A user can add include files to the device tree (similar to how this would be achieved in C/C++). These are called DTSI files. In PetaLinux, users can use the `system-user.dtsi` template to add, or modify nodes in the DT. 


## U-boot
- https://docs.u-boot.org/en/latest/board/xilinx/zynq.html
- https://github.com/Xilinx/u-boot-xlnx 
- https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/18841973/Build+U-Boot 
- https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/749142017/Using%2BDistro%2BBoot%2BWith%2BXilinx%2BU-Boot#Booting-from-SD-Card
- https://docs.kernel.org/arch/arm/booting.html
- https://github.com/Xilinx/PYNQ/blob/master/boards/Pynq-Z2/petalinux_bsp/meta-user/recipes-bsp/u-boot/files/


To be able to build u-boot from source for the pynq-z2 board:

- Clone xilinx u-boot repository
```bash
git clone https://github.com/Xilinx/u-boot-xlnx.git
cd to u-boot directory "cd u-boot-xlnx".
```
- Add tool chain to path and then set tool chain as below:
```bash
    export CROSS_COMPILE=arm-linux-gnueabihf-
    export ARCH=arm
```

Configuring U-Boot (>= 2020.1 Release)
For 2020.1 and above releases common defconfig is being made. 
All Zynq (except for mini) ->	xilinx_zynq_virt_defconfig

To build U-boot for ZC702 board, follow below steps. 
```bash
make distclean
make xilinx_zynq_virt_defconfig
export DEVICE_TREE="zynq-zc702"
make
```
**NOTE:** device tree can be found under arch/arm/dts/ with the name zynq-zc702.dts


For this project's purpose we need to add support for the pynqz2 board. The following steps are needed.
- add a simple device tree for the pynqz2 in `arch/arm/dts/` (`arch/arm/dts/zynq-pynqz2.dts`) and append the list of device tree source files in `arch/arm/dts/Makefile`.
- add bsp files generated by Vitis/XSCT to board/xilinx/zynq/zynq-pynqz2/ (See: https://www.youtube.com/watch?v=3D2-OPArCiA&list=PLAIW2DtZ4NI77mo2qFhvZKGZ1n1W8vWYY&index=16)
- tweak the `configs/xilinx_zynq_virt_defconfig` to be more tailored for the pynq-z2 board and also more for development purposes.

The previous steps are summarized in `patches/add-pynq-z1-and-z2-support-uboot.patch`. This patch can be applied directly to the `u-boot-xlnx` repository to add support for the pynqz2 board. (tested on branch `xilinx-v2024.2`)


## Booting from SD Card


### Creating the SD card

https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/18842385/How+to+format+SD+card+for+SD+boot

TODO

```
sudo fdisk /dev/mmcblk0


```


TODO: explain the FIT image built


For example, when booting from MMC devices (SD card or eMMC), the following script is run from the boot.scr file by default in the Xilinx-provided U-Boot environment.  This can be modified based on your needs. 

```c
if test "${boot_target}" = "mmc0" || test "${boot_target}" = "mmc1" ; then
   if test -e ${devtype} ${devnum}:${distro_bootpart} /image.ub; then
      fatload ${devtype} ${devnum}:${distro_bootpart} 0x10000000 image.ub;
      bootm 0x10000000;
      exit;
   fi
   if test -e ${devtype} ${devnum}:${distro_bootpart} /Image; then
      fatload ${devtype} ${devnum}:${distro_bootpart} 0x00200000 Image;;
   fi
   if test -e ${devtype} ${devnum}:${distro_bootpart} /system.dtb; then
      fatload ${devtype} ${devnum}:${distro_bootpart} 0x00100000 system.dtb;
   fi
   if test -e ${devtype} ${devnum}:${distro_bootpart} /rootfs.cpio.gz.u-boot; then
      fatload ${devtype} ${devnum}:${distro_bootpart} 0x04000000 rootfs.cpio.gz.u-boot;
      booti 0x00200000 0x04000000 0x00100000
      exit;
   fi
   booti 0x00200000 - 0x00100000
   exit;
fi 
```

The following U-Boot commands illustrate loading a Linux image from a SD card using either individual images and a FIT image using the bootm command.
```bash
setenv bootargs "earlyprintk console=ttyPS0,115200"
u-boot> fatload mmc 0 0x3000000 uImage
u-boot> fatload mmc 0 0x2A00000 devicetree.dtb
u-boot> fatload mmc 0 0x2000000 uramdisk.image.gz
u-boot> bootm 0x3000000 0x2000000 0x2A00000
```

or if the FIT contains a initramfs image
```bash
setenv bootargs "earlyprintk console=ttyPS0,115200"
u-boot> fatload mmc 0 0x1000000 image.ub
u-boot> bootm 0x1000000
```

or if using a FIT but the rootfs is on the sdcard
```bash
setenv bootargs "earlyprintk console=ttyPS0,115200 root=/dev/mmcblk0p2 rw rootwait"
setenv bootcmd 'fatload mmc 0 0x1000000 image.ub  && bootm 0x1000000'
boot
```



With the bootm command, U-Boot is relocating the images before it boots Linux such that the addresses above may not be what the kernel sees. U-Boot also alters the device tree to tell the kernel where the ramdisk image is located in memory (initrd-start and initrd-end). The
bootm command sets the r2 register to the address of the device tree in memory which is not done by the go command.
The differences and use cases of using the booti commands and the bootm command have evolved over time.  As of U-Boot 2020.01, the primary difference is in handling of uncompressed Linux Image files (common for 64-bit Arm platforms) versus compressed Linux zImage files (common on 32-bit Arm platforms) as denoted in the U-Boot help.  As a general rule of thumb, only differentiate in usage depending on Linux image type rather than solely based on architecture.
booti - boot Linux kernel 'Image' format from memory
bootm - boot application image from memory


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
