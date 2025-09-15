#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR="/tmp/aeld"
WORKING_DIRECTORY=$(pwd)
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
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

    # Add your kernel build steps here
    # First deep clean the kernel build tree
    echo CLEANING LINUX UIIL
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    
    # Then make the defconfig
    echo MAKING THE DEFAULT CONFIG
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    
    # Then make kernel image
    echo MAKING THE FULL KERNEL IMAGE
    make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    
    # Then make the modules
    echo MAKING ALL MODULES
    # make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules 
    
    # Finally build the device tree
    echo BUILDING DEVICE TREE
    # make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
fi

cp ${OUTDIR}/linux-stable/arch/arm64/boot/Image ${OUTDIR}

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    rm  -rf ${OUTDIR}/rootfs
fi

# Create necessary base directories
echo making base directories
mkdir -p rootfs
cd rootfs

mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log

cd ${OUTDIR}
if [ ! -d "${OUTDIR}/busybox" ]
then
    git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
else
    cd busybox
fi

# Make and install busybox
echo Making and installing busybox
make distclean
make defconfig
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

echo "Copying Library dependencies"
cp $WORKING_DIRECTORY/../cc-lib/ld-linux-aarch64.so.1 ${OUTDIR}/rootfs/lib/
cp $WORKING_DIRECTORY/../cc-lib/libm.so.6 ${OUTDIR}/rootfs/lib64/
cp $WORKING_DIRECTORY/../cc-lib/libresolv.so.2 ${OUTDIR}/rootfs/lib64/
cp $WORKING_DIRECTORY/../cc-lib/libc.so.6 ${OUTDIR}/rootfs/lib64/

# Make device nodes
cd "$OUTDIR/rootfs"
echo Making Device Nodes
if [ -e "dev/null" ]
then
    sudo mknod -m 666 dev/null c 1 3
    sudo mknod -m 622 dev/console c 5 1
fi

# Clean and build the writer utility

echo Making Writing Utility
cd $WORKING_DIRECTORY
make clean
make CROSS_COMPILE=${CROSS_COMPILE}

# Copy the finder related scripts and executables to the /home directory
# on the target rootfs
echo Copy scripts and executables to target
cp $WORKING_DIRECTORY/finder.sh ${OUTDIR}/rootfs/home/
cp $WORKING_DIRECTORY/finder-test.sh ${OUTDIR}/rootfs/home/
cp -r $WORKING_DIRECTORY/../conf ${OUTDIR}/rootfs/home/
cp $WORKING_DIRECTORY/autorun-qemu.sh ${OUTDIR}/rootfs/home/
cp $WORKING_DIRECTORY/writer.c ${OUTDIR}/rootfs/home 
cp $WORKING_DIRECTORY/writer ${OUTDIR}/rootfs/home 

# TODO: Create initramfs.cpio.gz
cd ${OUTDIR}/rootfs
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
cd ${OUTDIR}
gzip -f initramfs.cpio