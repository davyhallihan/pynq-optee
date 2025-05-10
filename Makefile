# for uboot and linux kernel
CROSS_COMPILE = arm-linux-gnueabihf-
ARCH = arm
DEVICE_TREE = zynq-pynqz2
UBOOT_CONFIG = zynq_pynqz2_defconfig
# UBOOT_CONFIG = xilinx_zynq_virt_defconfig
export CROSS_COMPILE ARCH DEVICE_TREE

ROOTFS_DIR=artifacts/rootfs

# add mkimage to PATH
PATH := $(PATH):$(realpath uboot_src/tools)
export PATH

FSBL_CC=arm-none-eabi-gcc

artifacts:
	mkdir -p artifacts

fsbl: artifacts
	@if [ ! -d embeddedsw ]; then git clone https://github.com/Xilinx/embeddedsw.git; fi
	rm -rf embeddedsw/lib/sw_apps/zynq_fsbl/misc/zynq-pynqz1
	rm -rf embeddedsw/lib/sw_apps/zynq_fsbl/misc/zynq-pynqz2
	cd embeddedsw; git reset && git restore . && git clean -f && git apply ../patches/add-pynqz1-pynqz2-support-fsbl.patch 
	make -j8 -C embeddedsw/lib/sw_apps/zynq_fsbl/src/ SHELL=/bin/bash BOARD=$(DEVICE_TREE) CC=$(FSBL_CC)
	cd artifacts; rm fsbl.elf -f ; cp ../embeddedsw/lib/sw_apps/zynq_fsbl/src/fsbl.elf .

fsbl_clean:
	rm -rfv embeddedsw

uboot: artifacts
	@if [ ! -d uboot_src ]; then git clone https://github.com/Xilinx/u-boot-xlnx.git uboot_src --branch xilinx-v2024.2; fi

# patch u-boot to add support for pynq-z1 and pynq-z2
	rm uboot_src/board/xilinx/zynq/zynq-pynqz2/ -rf
	cd uboot_src; git reset && git restore . && git clean -f && git apply ../patches/add-pynq-z2-support-uboot.patch
	
	make -C uboot_src $(UBOOT_CONFIG) SHELL=/bin/bash
	
	export CROSS_COMPILE=arm-linux-gnueabihf- &&\
	export ARCH=arm && export DEVICE_TREE=$(DEVICE_TREE)  &&\
	make -j8 -C uboot_src SHELL=/bin/bash

	cd artifacts; rm u-boot.elf -f; cp ../uboot_src/u-boot.elf .

uboot_clean:
	make -C uboot_src distclean SHELL=/bin/bash


dtb:
	@if [ ! -d dtc ]; then git clone https://git.kernel.org/pub/scm/utils/dtc/dtc.git ; fi
	cd dtc && meson setup builddir && meson compile -C builddir
	cd device-tree && ./generate_dts.sh
	cd device-tree && ./build_dts.sh ../dtc/builddir/dtc

dtb_clean:
	rm -rfv dtc
	rm device-tree/dts device-tree/.Xil device-tree/extracted -rf

kernel: artifacts
	@if [ ! -d linux-xlnx ]; then git clone https://github.com/Xilinx/linux-xlnx.git ; fi
	make -C linux-xlnx SHELL=/bin/bash ARCH=arm xilinx_zynq_defconfig
# make -C linux-xlnx SHELL=/bin/bash ARCH=arm menuconfig
	make -j8 -C linux-xlnx SHELL=/bin/bash ARCH=arm UIMAGE_LOADADDR=0x10000000 uImage
# according to the meta-xilinx layer
# make -j8 -C linux-xlnx SHELL=/bin/bash ARCH=arm UIMAGE_LOADADDR=0x200000 uImage 

# make -C inux-xlnx ARCH=arm UIMAGE_LOADADDR=0x8000 uImage
	cd artifacts && cp ../linux-xlnx/arch/arm/boot/uImage .
	cd artifacts && cp ../linux-xlnx/arch/arm/boot/Image  .
	cd artifacts && cp ../linux-xlnx/arch/arm/boot/zImage  .

kernel_clean: 
	cd linux-xlnx && make SHELL=/bin/bash clean

rootfs:
	@if [ ! -d busybox ]; then git clone git://busybox.net/busybox.git ; fi
	cp patches/busybox_simple_config busybox/configs/busybox_simple_defconfig
	cd busybox && make SHELL=/bin/bash ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) busybox_simple_defconfig
	cd busybox && make -j$(nproc) SHELL=/bin/bash ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE)
	./make_rootfs.sh

rootfs_clean:
	rm -rfv $(ROOTFS_DIR)

# bootgen: artifacts fsbl uboot dtb
bootgen: 
	cp device-tree/simple_pynqz2_wrapper.bit artifacts/bitstream.bit
	export PATH=${PATH}:/home/zaki/tools/Xilinx/Vitis/2024.2/bin/ && bootgen -image image.bif -o artifacts/BOOT.bin -w

bootgen_clean:
	rm -f artifacts/BOOT.bin 

image_ub: 
	mkimage -f pynq_z2_fit_image.its ./artifacts/image.ub

sdcard: rootfs bootgen
	echo "TODO"


# clean: fsbl_clean uboot_clean rootfs_clean bootgen_clean dtb_clean
# rm -rf artifacts
	
.PHONY: fsbl fsbl_clean uboot uboot_clean kernel kernel_clean rootfs rootfs_clean bootgen bootgen_clean clean ub_image