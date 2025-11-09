#!/bin/bash
INITRAMFS_DIR=artifacts/initramfs/
rm ${INITRAMFS_DIR} -rf
rm artifacts/initramfs.cpio.gz -f
mkdir -p ${INITRAMFS_DIR}/{bin,sbin,etc/init.d,proc,sys,usr/{bin,sbin},dev}
make -C busybox SHELL=/bin/bash ARCH=arm CROSS_COMPILE=${CROSS_COMPILE} CONFIG_PREFIX=../${INITRAMFS_DIR} install

# copy startup script
cp rcS ${INITRAMFS_DIR}/etc/init.d/rcS
chmod a+x ${INITRAMFS_DIR}/etc/init.d/rcS

cd $INITRAMFS_DIR
ln -s bin/busybox init
find . | cpio -o -H newc --owner=0:0 | gzip  > ../initramfs.cpio.gz
mkimage -A arm -O linux -T ramdisk -C gzip -d ../initramfs.cpio.gz ../uInitrd
cd -
