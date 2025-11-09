# for uboot and linux kernel
CROSS_COMPILE = arm-linux-gnueabihf-
ARCH = arm
DEVICE_TREE = zynq-pynqz2
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
	make -j8 -C embeddedsw/lib/sw_apps/zynq_fsbl/src/ SHELL=/bin/bash BOARD=$(DEVICE_TREE) CC=$(FSBL_CC) CFLAGS="-DSTDOUT_BASEADDRESS=0xe0000000 -DFSBL_DEBUG_INFO"
	cd artifacts; rm fsbl.elf -f ; cp ../embeddedsw/lib/sw_apps/zynq_fsbl/src/fsbl.elf .

fsbl_clean:
	rm -rfv embeddedsw

uboot: artifacts
	@if [ ! -d uboot_src ]; then git clone https://github.com/Xilinx/u-boot-xlnx.git uboot_src --branch xilinx-v2024.2; fi

# patch u-boot to add support for pynq-z1 and pynq-z2
	rm uboot_src/board/xilinx/zynq/zynq-pynqz2/ -rf
	cd uboot_src; git reset && git restore . && git clean -f && git apply ../patches/add-pynq-z2-support-uboot-optee.patch

# copy customized u-boot configuration
	cp patches/zynq_pynqz2_defconfig uboot_src/configs/zynq_pynqz2_defconfig
	make -C uboot_src zynq_pynqz2_defconfig SHELL=/bin/bash
# custom config is adopted from zynq default
# make -C uboot_src xilinx_zynq_virt_defconfig SHELL=/bin/bash
	
	export CROSS_COMPILE=arm-linux-gnueabihf- &&\
	export ARCH=arm && export DEVICE_TREE=$(DEVICE_TREE)  &&\
	make -j8 -C uboot_src SHELL=/bin/bash

	cd artifacts; rm u-boot.elf -f; cp ../uboot_src/u-boot.elf .
	cp uboot_src/u-boot.bin artifacts/u-boot.bin

uboot_clean:
	make -C uboot_src distclean SHELL=/bin/bash

xsct:
	wget https://petalinux.xilinx.com/sswreleases/rel-v2024.2/xsct-trim/xsct-2024-2_1104.tar.xz
	# TODO: unpack, build/install, add to path 

dtc: 
	@if [ ! -d dtc ]; then git clone https://git.kernel.org/pub/scm/utils/dtc/dtc.git ; fi
	cd dtc && meson setup builddir && meson compile -C builddir

dtb: dtc
	cd device-tree && ./generate_dts.sh
	gcc -I device-tree/dts -E -nostdinc -I device-tree/dts/include -undef -D__DTS__ -x assembler-with-cpp -o artifacts/system.dts device-tree/dts/system-top.dts
	dtc/builddir/dtc -I dts -O dtb -o artifacts/system.dtb artifacts/system.dts

dtb_optee: dtc
	gcc -I device-tree/dts_optee -E -nostdinc -I device-tree/dts_optee/include -undef -D__DTS__ -x assembler-with-cpp -o artifacts/system.dts device-tree/dts_optee/system-top.dts
	dtc/builddir/dtc -I dts -O dtb -o artifacts/system.dtb artifacts/system.dts

dtb_clean:
	rm -rfv dtc
	rm device-tree/dts device-tree/.Xil device-tree/extracted -rf
	rm artifacts/system.dts
	rm artifacts/system.dtb

kernel: artifacts
	@if [ ! -d linux-xlnx ]; then git clone https://github.com/Xilinx/linux-xlnx.git ; fi

# configure the kernel
	cd linux-xlnx; git reset && git restore . && git clean -f
# cp patches/zynq_pynqz2_linux_defconfig linux-xlnx/arch/arm/configs/zynq_pynqz2_linux_defconfig
# make -C linux-xlnx SHELL=/bin/bash ARCH=arm zynq_pynqz2_linux_defconfig
	make -C linux-xlnx SHELL=/bin/bash ARCH=arm xilinx_zynq_defconfig
	make -j8 -C linux-xlnx SHELL=/bin/bash ARCH=arm
	cd artifacts && cp ../linux-xlnx/arch/arm/boot/Image  .
	cd artifacts && cp ../linux-xlnx/arch/arm/boot/zImage  .

# linux kernel with patches to make booting from optee/TEE works
kernel_tee: artifacts
	@if [ ! -d linux-xlnx ]; then git clone https://github.com/Xilinx/linux-xlnx.git ; fi
# Patch the kernel to enable TEE support
	cd linux-xlnx; git reset && git restore . && git clean -f && git apply ../patches/enable_optee_support_for_zynq.patch
	make -C linux-xlnx SHELL=/bin/bash ARCH=arm xilinx_zynq_defconfig
# Update the configuration to enable TEE
	cd linux-xlnx && ./scripts/kconfig/merge_config.sh -m .config ../patches/zynq_pynqz2_linux_tee_enable
	make -j8 -C linux-xlnx SHELL=/bin/bash ARCH=arm
	cd artifacts && cp ../linux-xlnx/arch/arm/boot/Image  .
	cd artifacts && cp ../linux-xlnx/arch/arm/boot/zImage  .

kernel_clean: 
	cd linux-xlnx && make SHELL=/bin/bash clean

rootfs:
	cp patches/busybox_simple_config busybox/configs/busybox_simple_defconfig
	cd busybox && make SHELL=/bin/bash ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) busybox_simple_defconfig
	cd busybox && make -j$(nproc) SHELL=/bin/bash ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE)
	./make_rootfs.sh

rootfs_clean:
	rm -rfv $(ROOTFS_DIR)

bootgen: 
	cp device-tree/simple_pynqz2_wrapper.bit artifacts/bitstream.bit
	export PATH=${PATH}:/home/zaki/tools/Xilinx/Vitis/2024.2/bin/ && bootgen -image image.bif -o artifacts/BOOT.bin -w

bootgen_optee:
	cp device-tree/simple_pynqz2_wrapper.bit artifacts/bitstream.bit
	export PATH=${PATH}:/home/zaki/tools/Xilinx/Vitis/2024.2/bin/ && bootgen -image optee_image.bif -o artifacts/BOOT-optee.bin -w

bootgen_clean:
	rm -f artifacts/BOOT.bin 

sdcard: rootfs bootgen
	echo "TODO"

optee:
	@if [ ! -d optee_os ]; then git clone https://github.com/OP-TEE/optee_os.git ; fi
	cd optee_os; git reset && git restore . && git clean -f && git apply ../patches/fix_zynq_support_in_optee.patch
	cd optee_os; make \
		CFG_NS_ENTRY_ADDR=0x03000000 \
		CFG_TEE_CORE_LOG_LEVEL=4 \
		CROSS_COMPILE=arm-linux-gnueabihf- \
		DEBUG=1 \
		O=out/arm \
		CFG_DT=y \
		CFG_CORE_DEBUG=y\
		PLATFORM=zynq7k
#CFG_DT_ADDR=0x02A00000 
	cp optee_os/out/arm/core/tee-raw.bin artifacts
	cp optee_os/out/arm/core/tee.elf artifacts
	mkimage -A arm -O linux -C none -a 0x10000000 -e 0x10000000 -d artifacts/tee-raw.bin artifacts/uTee

boot_src:
	mkimage -A arm -T script -C none -d boot.txt artifacts/boot.scr

boot_tee_src:
	mkimage -A arm -T script -C none -d boot_tee.txt artifacts/boot_tee.scr

# non-secure development image:
dev_image: dtb kernel rootfs uboot boot_src bootgen

# trust-zone enabled image:
optee_image: optee dtb_optee kernel_tee rootfs uboot boot_tee_src bootgen_optee
	# TODO: build and add tee-suplicant and linux userspace support.
	# TODO: build and add trustlets

# trust-zone enabled image with secure-boot:
# optee_image_secure:

clean: fsbl_clean uboot_clean rootfs_clean bootgen_clean dtb_clean
	rm -rf artifacts
	
.PHONY: fsbl fsbl_clean uboot uboot_clean kernel kernel_clean rootfs rootfs_clean bootgen bootgen_clean clean ub_image dtb dtb_optee dtc optee_image kernel_tee boot_src boot_tee_src dev_image