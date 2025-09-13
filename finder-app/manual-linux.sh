#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR="/tmp/aeld"

WORKING_DIRECTORY=/home/corey/code/assignment-1-Coreyboy1820/finder-app
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
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules 
    
    # Finally build the device tree
    echo BUILDING DEVICE TREE
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
fi

echo "Adding the Image in outdir"

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# Create necessary base directories
echo making base directories
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log

cd "$OUTDIR"
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
echo making and installing busybox
make distclean
make defconfig
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=${OUTDIR}/bin ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install


echo "Copying Library dependencies"
mapfile -t dependencies < <(${CROSS_COMPILE}readelf -a "${OUTDIR}/bin/busybox" \
                             | grep "program interpreter" \
                             | grep -o '/[^]]*')

for dependency in "${dependencies[@]}"
do
    file_name=$(basename $dependency)
    cp /home/corey/.local/opt/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/lib/$file_name ${OUTDIR}/lib64/
done

mapfile -t dependencies < <(${CROSS_COMPILE}readelf -a "${OUTDIR}/bin/busybox" \
    | grep "Shared library" \
    | grep -oP '(?<=\[)[^]]+(?=\])')

for dependency in "${dependencies[@]}"
do
    cp /home/corey/.local/opt/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/lib64/$dependency ${OUTDIR}/lib64/
done

# Make device nodes
cd "$OUTDIR"
echo Making Device Nodes
if [ -e "dev/null"]
then
    sudo mknod -m 666 dev/null c 1 3
    sudo mknod -m 666 dev/console c 5 1
fi

# TODO: Clean and build the writer utility

echo Making Writing Utility
cd $WORKING_DIRECTORY
make clean
make CROSS_COMPILE=${CROSS_COMPILE}

# Copy the finder related scripts and executables to the /home directory
# on the target rootfs
echo Copy scripts and executables to target
cp $WORKING_DIRECTORY/writer.c ${OUTDIR}/home 
cp $WORKING_DIRECTORY/writer ${OUTDIR}/home 

# TODO: Chown the root directory
cd "$OUTDIR"
chown -R root ${OUTDIR}

# TODO: Create initramfs.cpio.gz
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
gzip -f initramfs.cpio