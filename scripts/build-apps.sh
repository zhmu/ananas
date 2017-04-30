#!/bin/sh -e

MAKE_JOBS=-j8

do_libc()
{
	cd $R/lib/libc/build/${ARCH}
	make ${MAKE_ARGS} clean
	make ${MAKE_ARGS}
	make ${MAKE_ARGS} install
}

do_crt()
{
	cd $R/lib/crt/${ARCH}
	make ${MAKE_ARGS} clean
	make ${MAKE_ARGS}
	make ${MAKE_ARGS} install
}

do_headers()
{
	cd $R/include
	make ${MAKE_ARGS} install
}

do_dash()
{
	cd $R/apps/dash
	rm -f dash-${ARCH}
	make ARCH=${ARCH} clean
	make ARCH=${ARCH} ${MAKE_ARGS}
}

do_coreutils()
{
	cd $R/apps/coreutils
	make ARCH=${ARCH} clean
	make ARCH=${ARCH} ${MAKE_ARGS} MAKEINFO=true
}

install_includes()
{
	cd $R/include
	make ${MAKE_ARGS}
}

install_includes_target()
{
	cd $R/include
	make ARCH=${ARCH} SYSROOT=${TARGET}
}

install_lib()
{
	cp $R/lib/libc/build/${ARCH}/libc.a ${TARGET}/usr/lib
	cp $R/lib/crt/${ARCH}/crt*.o ${TARGET}/usr/lib
}

install_tree()
{
	mkdir -p ${TARGET}/usr/lib
}

install_build()
{
	cd $R/apps/output.${ARCH}
	cp -R * ${TARGET}
}

ARCH=amd64
TOOLCHAIN_ARCH=x86_64
if [ -z "$TARGET" ]; then
	TARGET=/mnt
fi
R=`cd .. && pwd`

if [ ! -d ${R}/sysroot.${ARCH} ]; then
	echo "*** sysroot does not exist"
	exit 1
fi
SYSROOT=`realpath ${R}/sysroot.${ARCH}`
TOOL_PREFIX="${TOOLCHAIN_ARCH}-ananas-elf-"

# ensure the immediate output directory exists, or configure
# will try to place things in /
rm -rf $R/apps/output.${ARCH}
mkdir -p $R/apps/output.${ARCH}

# make sure we have our compiler in our path; configure will not raise red
# flags if it can't find it
CROSS_CC=`which ${TOOL_PREFIX}clang || true`
if [ ! -x "$CROSS_CC" ]; then
	echo "*** clang not found; is the correct cross-path set?"
	exit 1
fi

# remove apps from target; leave bootloader/kernel intact
rm -rf ${TARGET}/bin ${TARGET}/lib ${TARGET}/share ${TARGET}/usr

# throw the sysroot away; we'll need to repopulate it
rm -rf ${SYSROOT}
mkdir ${SYSROOT}
mkdir ${SYSROOT}/usr
mkdir ${SYSROOT}/usr/include
mkdir ${SYSROOT}/usr/lib

export MAKE_ARGS="SYSROOT=${SYSROOT} TOOL_PREFIX=${TOOL_PREFIX} ARCH=${ARCH}"

do_libc
do_headers
install_includes
do_crt
do_dash
do_coreutils

install_tree
install_build
install_lib
install_includes_target
