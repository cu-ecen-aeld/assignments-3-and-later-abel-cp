#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    # for vmlinux
    # deep clean
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    # select target defconfig
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    # build all
    make -j 2 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    # build modules
    # make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
    # build devicetree
    # make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
fi

echo "Adding the Image in outdir"
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}
   
echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir -p ${OUTDIR}/rootfs/bin \
         ${OUTDIR}/rootfs/dev \
         ${OUTDIR}/rootfs/etc \
         ${OUTDIR}/rootfs/lib \
         ${OUTDIR}/rootfs/lib64 \
         ${OUTDIR}/rootfs/proc \
         ${OUTDIR}/rootfs/sys \
         ${OUTDIR}/rootfs/sbin \
         ${OUTDIR}/rootfs/tmp \
         ${OUTDIR}/rootfs/usr \
         ${OUTDIR}/rootfs/usr/bin \
         ${OUTDIR}/rootfs/usr/sbin \
         ${OUTDIR}/rootfs/var \
         ${OUTDIR}/rootfs/var/log \
	 ${OUTDIR}/rootfs/home

cd "$OUTDIR"

if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make distclean
    make defconfig

else
    cd busybox
fi

# TODO: Make and install busybox
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} CONFIG_PREFIX=${OUTDIR}/rootfs install

echo "Library dependencies"

# read -p "press [enter] to continue"

cd ${OUTDIR}/rootfs
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"
# read -p "press [enter] to continue"

# TODO: Add library dependencies to rootfs
export SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)
echo $SYSROOT
# read -p "press [enter] to continue"

cd ${OUTDIR}/rootfs
cp -a "$SYSROOT/lib/ld-linux-aarch64.so.1" lib
cp -a "$SYSROOT/lib64/ld-2.31.so" lib64
#ln -s ld-2.31.so lib/ld-linux-aarch64.so.1

cp -a "$SYSROOT/lib64/libm.so.6" lib64
cp -a "$SYSROOT/lib64/libm-2.31.so" lib64

cp -a "$SYSROOT/lib64/libc.so.6" lib64
cp -a "$SYSROOT/lib64/libc-2.31.so" lib64

cp -a "$SYSROOT/lib64/libresolv.so.2" lib64
cp -a "$SYSROOT/lib64/libresolv-2.31.so" lib64

# TODO: Make device nodes
## sudo mknod -m 666 dev/null c 1 3
## sudo mknod -m 600 dev/console c 5 1
mknod -m 666 dev/null c 1 3
mknod -m 600 dev/console c 5 1
# TODO: Clean and build the writer utility
cd ${FINDER_APP_DIR} 
make clean
make
# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
## mkdir ${OUTDIR}/rootfs/home/finder-app
## mkdir ${OUTDIR}/rootfs/home/conf
## pwd
## cp -a conf/* ${OUTDIR}/rootfs/home/conf
## cp -a conf ${OUTDIR}/rootfs/home/finder-app
## cp -a finder.sh ${OUTDIR}/rootfs/home/finder-app
## cp -a finder-test.sh ${OUTDIR}/rootfs/home/finder-app
## cp -a autorun-qemu.sh ${OUTDIR}/rootfs/home/finder-app
cp -a conf ${OUTDIR}/rootfs/home/
cp -a finder.sh ${OUTDIR}/rootfs/home/
cp -a finder-test.sh ${OUTDIR}/rootfs/home/
cp -a autorun-qemu.sh ${OUTDIR}/rootfs/home/

# TODO: Chown the root directory
cd ${OUTDIR}/rootfs
# sudo chown -R root:root *
chown -R root:root *

pwd
echo "Create initramfs.cpio"

# TODO: Create initramfs.cpio.gz
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
cd ${OUTDIR}
gzip -f initramfs.cpio
ls initramfs*

