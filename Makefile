# for uboot and linux kernel
CROSS_COMPILE = arm-none-linux-gnueabihf-
ARCH = arm
DEVICE_TREE = zynq-pynqz2
export CROSS_COMPILE ARCH DEVICE_TREE

ROOTFS_DIR=artifacts/initramfs

# add xsct and arm-none-linux-gnueabihf-* to PATH
PATH := $(PATH):$(realpath xsct/Vitis/2024.2/bin):$(realpath gnu/arm-gnu-toolchain-14.3.rel1-x86_64-arm-none-linux-gnueabihf/bin)
export PATH

FSBL_CC=arm-none-eabi-gcc

# ---- Pinned dependency versions (commit SHAs) ----
# Update these when upgrading dependencies or regenerating patches.
EMBEDDEDSW_SHA    = 1bb19ac1ab06ab322ba4340bed372f93ca612a18
UBOOT_BRANCH      = xilinx-v2024.2
DTC_SHA            = 7f3184a6c550bb8fb59e93c9901d75dced889dcf
LINUX_SHA          = 7e47ae4ab22ed45a8326203c863f826423978f89
OPTEE_CLIENT_SHA   = 9d6f69844ff60ec0966cf3659abcc38eda8b31ea
OPTEE_EXAMPLES_SHA = ffddefd7b6edd5fe2b4991fc8a394b98a6276ffa
BUSYBOX_SHA        = 8d7ae80eda7e08ee361f8793683ae58216caf720
BOOTGEN_SHA        = 4f023ac2ca2ea452740194045fcecf1671f5b2c0
OPTEE_OS_SHA       = e97a138aba3f477dee888d0978e25ab2b7d78819

artifacts:
	mkdir -p artifacts

xsct:
	if [ ! -e xsct/Vitis/2024.2/bin/xsct ]; then \
		wget https://petalinux.xilinx.com/sswreleases/rel-v2024.2/xsct-trim/xsct-2024-2_1104.tar.xz -O /tmp/xsct.tar.xz && \
		mkdir -p xsct && \
		tar xvf /tmp/xsct.tar.xz -C ./xsct; \
	fi

gnu_toolchain: 
	if [ ! -d gnu ]; then \
		wget https://developer.arm.com/-/media/Files/downloads/gnu/14.3.rel1/binrel/arm-gnu-toolchain-14.3.rel1-x86_64-arm-none-linux-gnueabihf.tar.xz -O /tmp/gnu.tar.xz && \
		mkdir -p gnu && \
		tar xvf /tmp/gnu.tar.xz -C ./gnu; \
	fi


fsbl: artifacts
	@if [ ! -d embeddedsw ]; then git clone https://github.com/Xilinx/embeddedsw.git && cd embeddedsw && git checkout $(EMBEDDEDSW_SHA); fi
	rm -rf embeddedsw/lib/sw_apps/zynq_fsbl/misc/zynq-pynqz1
	rm -rf embeddedsw/lib/sw_apps/zynq_fsbl/misc/zynq-pynqz2
	cd embeddedsw; git reset && git restore . && git clean -f && git apply ../patches/add-pynqz1-pynqz2-support-fsbl.patch 
# need to build fsbl twice.. first build builds dependencies then second builds fsbl
	make -j8 -C embeddedsw/lib/sw_apps/zynq_fsbl/src/ SHELL=/bin/bash BOARD=$(DEVICE_TREE) CC=$(FSBL_CC) CFLAGS="-DSTDOUT_BASEADDRESS=0xe0000000 -DFSBL_DEBUG_INFO" || \
	make -j8 -C embeddedsw/lib/sw_apps/zynq_fsbl/src/ SHELL=/bin/bash BOARD=$(DEVICE_TREE) CC=$(FSBL_CC) CFLAGS="-DSTDOUT_BASEADDRESS=0xe0000000 -DFSBL_DEBUG_INFO"
	cd artifacts; rm fsbl.elf -f ; cp ../embeddedsw/lib/sw_apps/zynq_fsbl/src/fsbl.elf .

fsbl_clean:
	make -C embeddedsw/lib/sw_apps/zynq_fsbl/src clean

uboot: artifacts gnu_toolchain
	@if [ ! -d uboot_src ]; then git clone https://github.com/Xilinx/u-boot-xlnx.git uboot_src --branch $(UBOOT_BRANCH); fi

# patch u-boot to add support for pynq-z1 and pynq-z2
	rm uboot_src/board/xilinx/zynq/zynq-pynqz2/ -rf
	cd uboot_src; git reset && git restore . && git clean -f && git apply ../patches/add-pynq-z2-support-uboot.patch

# copy customized u-boot configuration
	cp patches/zynq_pynqz2_defconfig uboot_src/configs/zynq_pynqz2_defconfig
	make -C uboot_src zynq_pynqz2_defconfig SHELL=/bin/bash
# custom config is adapted from zynq default
# make -C uboot_src xilinx_zynq_virt_defconfig SHELL=/bin/bash
	
	make -j$(nproc) -C uboot_src \
		SHELL=/bin/bash \
		CROSS_COMPILE=arm-none-linux-gnueabihf- \
		ARCH=arm \
		DEVICE_TREE=$(DEVICE_TREE)

	cd artifacts; rm u-boot.elf -f; cp ../uboot_src/u-boot.elf .
	cp uboot_src/u-boot.bin artifacts/u-boot.bin

uboot_clean:
	make -C uboot_src distclean SHELL=/bin/bash

dtc: 
	@if [ ! -d dtc ]; then git clone https://git.kernel.org/pub/scm/utils/dtc/dtc.git && cd dtc && git checkout $(DTC_SHA); fi
	if [ ! -e  dtc/builddir/dtc ]; then cd dtc && meson setup builddir && meson compile -C builddir ; fi

dtb: dtc
	cd device-tree && ./generate_dts.sh
	gcc -I device-tree/dts -E -nostdinc -I device-tree/dts/include -undef -D__DTS__ -x assembler-with-cpp -o artifacts/system.dts device-tree/dts/system-top.dts
	dtc/builddir/dtc -I dts -O dtb -o artifacts/system.dtb artifacts/system.dts

# for executing linux in non-secure world we need a custom device tree with fixed clocks and only non-secure peripherals
dtb_optee: dtc
	gcc -I device-tree/dts_optee -E -nostdinc -I device-tree/dts_optee/include -undef -D__DTS__ -x assembler-with-cpp -o artifacts/system.dts device-tree/dts_optee/system-top.dts
	dtc/builddir/dtc -I dts -O dtb -o artifacts/system.dtb artifacts/system.dts

dtb_clean:
	rm -rfv dtc
	rm device-tree/dts device-tree/.Xil device-tree/extracted -rf
	rm artifacts/system.dts
	rm artifacts/system.dtb

kernel: artifacts gnu_toolchain
	@if [ ! -d linux-xlnx ]; then git clone https://github.com/Xilinx/linux-xlnx.git && cd linux-xlnx && git checkout $(LINUX_SHA); fi

# configure the kernel
	cd linux-xlnx; git reset && git restore . && git clean -f
	make -C linux-xlnx SHELL=/bin/bash ARCH=arm xilinx_zynq_defconfig
	make -j8 -C linux-xlnx SHELL=/bin/bash ARCH=arm
	cd artifacts && cp ../linux-xlnx/arch/arm/boot/zImage  .

# linux kernel with patches to make booting from optee/TEE works
kernel_tee: artifacts gnu_toolchain
	@if [ ! -d linux-xlnx ]; then git clone https://github.com/Xilinx/linux-xlnx.git && cd linux-xlnx && git checkout $(LINUX_SHA); fi
# Patch the kernel to enable TEE support
	cd linux-xlnx; git reset && git restore . && git clean -f && git apply ../patches/enable_optee_support_for_zynq.patch
	make -C linux-xlnx SHELL=/bin/bash ARCH=arm xilinx_zynq_defconfig
# Update the configuration to enable TEE
	cd linux-xlnx && ./scripts/kconfig/merge_config.sh -m .config ../patches/zynq_pynqz2_linux_tee_enable
	make -j8 -C linux-xlnx SHELL=/bin/bash ARCH=arm
	cd artifacts && cp ../linux-xlnx/arch/arm/boot/zImage  .

kernel_clean: 
	cd linux-xlnx && make SHELL=/bin/bash clean

optee_client: gnu_toolchain
	@if [ ! -d optee_client ]; then git clone https://github.com/OP-TEE/optee_client && cd optee_client && git checkout $(OPTEE_CLIENT_SHA); fi
	cd optee_client &&\
	mkdir -p build &&\
	cd build && cmake -DCMAKE_C_COMPILER=arm-none-linux-gnueabihf-gcc -DCMAKE_INSTALL_PREFIX=$(realpath artifacts/initramfs) .. &&\
	make -j$(nproc) &&\
	make install 

optee_examples:
	@if [ ! -d optee_examples ]; then git clone https://github.com/linaro-swg/optee_examples.git && cd optee_examples && git checkout $(OPTEE_EXAMPLES_SHA); fi
	make -C optee_examples examples \
						   CROSS_COMPILE=arm-none-linux-gnueabihf- \
						   TEEC_EXPORT=$(realpath artifacts/initramfs) TA_DEV_KIT_DIR=$(realpath optee_os/out/arm/export-ta_arm32)
	make -C optee_examples prepare-for-rootfs
	cp optee_examples/out/ca/* artifacts/initramfs/usr/bin
	mkdir -p artifacts/initramfs/lib/optee_armtz && cp optee_examples/out/ta/* artifacts/initramfs/lib/optee_armtz/
	mkdir -p artifacts/initramfs/lib/optee_armtz/plugins && cp optee_examples/out/plugins/* artifacts/initramfs/lib/optee_armtz/plugins/

secure_switch: gnu_toolchain
	make -C secure_switch \
		CROSS_COMPILE=arm-none-linux-gnueabihf- \
		TEEC_EXPORT=$(realpath artifacts/initramfs) \
		TA_DEV_KIT_DIR=$(realpath optee_os/out/arm/export-ta_arm32)
	cp secure_switch/host/optee_benchmark artifacts/initramfs/usr/bin/
	mkdir -p artifacts/initramfs/lib/optee_armtz
	cp secure_switch/ta/*.ta artifacts/initramfs/lib/optee_armtz/

busybox: gnu_toolchain
	@if [ ! -d busybox ]; then git clone https://git.busybox.net/busybox && cd busybox && git checkout $(BUSYBOX_SHA); fi
	cp patches/busybox_simple_config busybox/configs/busybox_simple_defconfig
	make -C busybox SHELL=/bin/bash ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) busybox_simple_defconfig
	make -C busybox -j$(nproc) SHELL=/bin/bash ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE)
	make -C busybox SHELL=/bin/bash ARCH=arm CROSS_COMPILE=${CROSS_COMPILE} CONFIG_PREFIX=$(realpath artifacts/initramfs) install
	cd artifacts/initramfs && ln -s bin/busybox init

glibc: gnu_toolchain
	cp -r gnu/arm-gnu-toolchain-*/arm-none-linux-gnueabihf/libc/lib/* artifacts/initramfs/lib/
	cp -r gnu/arm-gnu-toolchain-*/arm-none-linux-gnueabihf/libc/usr/lib/* artifacts/initramfs/usr/lib/

rootfs:
	./make_rootfs.sh
	make glibc
	make busybox
	./make_initramfs.sh

rootfs_optee:
	./make_rootfs.sh
	make glibc
	make busybox
	make optee_client
	make optee_examples
	make secure_switch
	./make_initramfs.sh

bootgen_bin:
	@if [ ! -f bootgen/bootgen ]; then git clone https://github.com/Xilinx/bootgen.git && cd bootgen && git checkout $(BOOTGEN_SHA) && make CROSS_COMPILE=""; fi

bootgen: bootgen_bin
	cp device-tree/simple_pynqz2_wrapper.bit artifacts/bitstream.bit
	./bootgen/bootgen -image image.bif -o artifacts/BOOT.bin -w

bootgen_optee: bootgen_bin
	cp device-tree/simple_pynqz2_wrapper.bit artifacts/bitstream.bit
	./bootgen/bootgen -image optee_image.bif -o artifacts/BOOT-optee.bin -w

optee:
	@if [ ! -d optee_os ]; then git clone https://github.com/OP-TEE/optee_os.git && cd optee_os && git checkout $(OPTEE_OS_SHA); fi
	cd optee_os; git reset && git restore . && git clean -f && git apply ../patches/fix_zynq_support_in_optee.patch
# Install PTA benchmark into OP-TEE core
	cp secure_switch/pta/pta_benchmark.c optee_os/core/pta/pta_benchmark.c
	@if ! grep -q pta_benchmark optee_os/core/pta/sub.mk; then \
		printf '\nsrcs-$$(CFG_BENCHMARK_PTA) += pta_benchmark.c\n' >> optee_os/core/pta/sub.mk; \
	fi
	cd optee_os; make \
		CFG_NS_ENTRY_ADDR=0x03000000 \
		CFG_TEE_CORE_LOG_LEVEL=4 \
		CROSS_COMPILE=arm-none-linux-gnueabihf- \
		DEBUG=1 \
		O=out/arm \
		CFG_DT=y \
		CFG_CORE_DEBUG=y\
		PLATFORM=zynq7k \
		CFG_BENCHMARK_PTA=y \
		CFG_SWITCH_BASE=0x40001000

	cp optee_os/out/arm/core/tee-raw.bin artifacts
	cp optee_os/out/arm/core/tee.elf artifacts
	uboot_src/tools/mkimage -A arm -O linux -C none -a 0x10000000 -e 0x10000000 -d artifacts/tee-raw.bin artifacts/uTee

optee_clean:
	make -C optee_os clean PLATFORM=zynq7k O=out/arm CFG_DT=y

boot_src:
	uboot_src/tools/mkimage -A arm -T script -C none -d boot.txt artifacts/boot.scr

boot_tee_src:
	uboot_src/tools/mkimage -A arm -T script -C none -d boot_tee.txt artifacts/boot_tee.scr

# non-secure development image:
simple_image: fsbl uboot boot_src dtb kernel rootfs bootgen

# trust-zone enabled image:
optee_image: fsbl uboot boot_tee_src optee dtb_optee kernel_tee rootfs_optee bootgen_optee

# trust-zone enabled image with secure-boot:
# optee_image_secure:

clean: fsbl_clean uboot_clean kernel_clean dtb_clean optee_clean
	rm -rf artifacts
	
.PHONY: fsbl fsbl_clean uboot uboot_clean kernel kernel_clean rootfs bootgen clean ub_image dtb dtb_optee dtc optee_image kernel_tee boot_src boot_tee_src simple_image optee_client busybox optee_examples secure_switch glibc gnu_toolchain rootfs_optee optee_clean bootgen_bin