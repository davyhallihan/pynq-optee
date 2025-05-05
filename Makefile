# for uboot and linux kernel
CROSS_COMPILE = arm-linux-gnueabihf-
ARCH = arm
DEVICE_TREE = zynq-pynqz1
UBOOT_CONFIG = zynq_pynqz1_defconfig
# DEVICE_TREE = zynq-pynqz2
# UBOOT_CONFIG = zynq_pynqz2_defconfig
export CROSS_COMPILE ARCH DEVICE_TREE

# add mkimage to PATH
PATH := $(PATH):$(realpath uboot_src/tools)
export PATH

FSBL_CC=arm-none-eabi-gcc

artifacts:
	mkdir -p artifacts

fsbl: artifacts
	@if [ ! -d embeddedsw ]; then git clone https://github.com/Xilinx/embeddedsw.git; fi
	make -j8 -C embeddedsw/lib/sw_apps/zynq_fsbl/src/ SHELL=/bin/bash BOARD=$(DEVICE_TREE) CC=$(FSBL_CC)
	cd artifacts; rm fsbl.elf -f ; cp ../embeddedsw/lib/sw_apps/zynq_fsbl/src/fsbl.elf .

fsbl_clean:
	rm -rfv embeddedsw

uboot: artifacts
	@if [ ! -d uboot_src ]; then git clone https://github.com/Xilinx/u-boot-xlnx.git uboot_src --branch xilinx-v2024.2; fi

# patch u-boot to add support for pynq-z1 and pynq-z2
	cd uboot_src; git reset && git restore . && git clean -f && git apply ../patches/add-pynq-z1-and-z2-support-uboot.patch
	
	make -C uboot_src $(UBOOT_CONFIG) SHELL=/bin/bash
	
	export CROSS_COMPILE=arm-linux-gnueabihf- &&\
	export ARCH=arm && export DEVICE_TREE=$(DEVICE_TREE)  &&\
	make -j8 -C uboot_src SHELL=/bin/bash

	cd artifacts; rm u-boot.elf u-boot.bin -f; cp ../uboot_src/u-boot.bin . ; cp ../uboot_src/u-boot.elf .

uboot_clean:
	make -C uboot_src distclean SHELL=/bin/bash


dtb:
	@if [ ! -d dtc ]; then git clone https://git.kernel.org/pub/scm/utils/dtc/dtc.git ; fi
	cd dtc && meson setup builddir && meson compile -C builddir
	cd device-tree && ./generate_dts.sh
	cd device-tree && build_dts.sh ../dtc/builddir/dtc

dtb_clean:
	rm -rfv dtc
	rm device-tree/dts device-tree/.Xil device-tree/extracted -rf

kernel: artifacts
	@if [ ! -d linux-xlnx ]; then git clone https://github.com/Xilinx/linux-xlnx.git ; fi
	make -C linux-xlnx SHELL=/bin/bash ARCH=arm xilinx_zynq_defconfig
# make -C linux-xlnx SHELL=/bin/bash ARCH=arm menuconfig
	make -j8 -C linux-xlnx SHELL=/bin/bash ARCH=arm UIMAGE_LOADADDR=0x10000000 uImage
# make -C inux-xlnx ARCH=arm UIMAGE_LOADADDR=0x8000 uImage
	cd artifacts && cp ../linux-xlnx/arch/arm/boot/uImage .
	cd artifacts && cp ../linux-xlnx/arch/arm/boot/Image  .
	cd artifacts && cp ../linux-xlnx/arch/arm/boot/zImage  .

kernel_clean: 
	rm -rfv linux-xlnx

rootfs: artifacts
	echo "TODO" 

rootfs_clean:
	echo "TODO" 

bootgen: artifacts fsbl uboot kernel
	echo "TODO" 

bootgen_clean:
	echo "TODO" 

clean: fsbl_clean uboot_clean rootfs_clean bootgen_clean dtb_clean
	rm -rf artifacts

sdcard: rootfs bootgen
	echo "TODO"

.PHONY: fsbl fsbl_clean uboot uboot_clean kernel kernel_clean rootfs rootfs_clean bootgen bootgen_clean clean