#!/bin/sh -e

#MAKE_ARGS=-j8

do_libc()
{
	cd $R/lib/libc/compile/${ARCH}
	make clean
	make ${MAKE_ARGS}
	make install
}

do_crt()
{
	cd $R/lib/crt/${ARCH}
	make clean
	make ${MAKE_ARGS}
	make install
}

do_headers()
{
	cd $R/include
	make ARCH=${ARCH} install
}

do_dash()
{
	cd $R/apps/dash
	rm -rf build.${ARCH}
	rm -f dash-${ARCH}
	make clean
	make ${MAKE_ARGS}
}

do_coreutils()
{
	cd $R/apps/coreutils
	rm -rf build.${ARCH}
	mkdir -p build.${ARCH}
	make ${MAKE_ARGS}
}

do_gmp()
{
	cd $R/apps/gmp
	make clean
	make ${MAKE_ARGS}
}

do_mpfr()
{
	cd $R/apps/mpfr
	make clean
	make ${MAKE_ARGS}
}

do_mpc()
{
	cd $R/apps/mpc
	make clean
	make ${MAKE_ARGS}
}

do_binutils()
{
	cd $R/apps/binutils
	make clean
	make ${MAKE_ARGS}
}

do_gcc()
{
	cd $R/apps/gcc
	make clean
	make ${MAKE_ARGS}
}

do_make()
{
	cd $R/apps/make
	make clean
	make ${MAKE_ARGS}
}

do_jpeg()
{
	cd $R/apps/jpeg
	rm -rf build.${ARCH}
	mkdir -p build.${ARCH}
	make clean
	make ${MAKE_ARGS}
}

install_includes()
{
	cd $R/include
	make ARCH=${ARCH} DESTDIR=${TARGET}/usr/include
}

install_lib()
{
	cp $R/lib/libc/compile/${ARCH}/libc.a ${TARGET}/usr/lib
	cp $R/lib/crt/${ARCH}/crt*.o ${TARGET}/usr/lib
}

install_build()
{
	cd $R/apps/output.${ARCH}
	cp -R * ${TARGET}
}

ARCH=i386
TARGET=/mnt
R=`cd .. && pwd`

# ensure the immediate output directory exists, or configure
# will try to place things in /
mkdir -p $R/apps/output.${ARCH}

# make sure we have our compiler in our path; configure will not raise red
# flags if it can't find it
CROSS_CC=`which i386-elf-ananas-gcc || true`
if [ ! -x "$CROSS_CC" ]; then
	echo "*** gcc not found; is the correct cross-path set?"
	exit 1
fi

# prepare target
rm -rf ${TARGET}/*

do_libc
do_crt
do_headers
do_dash
do_coreutils
do_gmp
do_mpfr
do_mpc
do_jpeg
do_binutils
do_gcc
do_make

install_build
install_includes
install_lib
