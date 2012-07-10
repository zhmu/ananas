#!/bin/sh -e

do_libc()
{
	cd $R/lib/libc/compile/${ARCH}
	make clean
	make
	make install
}

do_crt()
{
	cd $R/lib/crt/${ARCH}
	make clean
	make
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
	make
}

do_coreutils()
{
	cd $R/apps/coreutils
	rm -rf build.${ARCH}
	mkdir -p build.${ARCH}
	#
	# XXX coreutils doesn't build correctly because it cannot create man
	# files (which we don't care about) - use this hackery to see if it
	# installs what we do care about, that's good enough for now
	#
	make clean
	make .install || true
	if [ ! -f $R/apps/output.${ARCH}/bin/yes ]; then
		echo  "*** coreutils build failure"
		exit 1
	fi
}

do_gmp()
{
	cd $R/apps/gmp
	make clean
	make
}

do_mpfr()
{
	cd $R/apps/mpfr
	make clean
	make
}

do_mpc()
{
	cd $R/apps/mpc
	make clean
	make
}

do_binutils()
{
	cd $R/apps/binutils
	make clean
	make
}

do_gcc()
{
	cd $R/apps/gcc
	make clean
	make
}

do_make()
{
	cd $R/apps/make
	make clean
	make
}

do_jpeg()
{
	cd $R/apps/jpeg
	make clean
	make
}

install_includes()
{
	cd $R/include
	make ARCH=${ARCH} DESTDIR=${TARGET}/usr/include
}

install_lib()
{
	cp $R/lib/libc/compile/${ARCH}/libc.a ${TARGET}/usr/lib
	cp $R/lib/crt/${ARCH}/crt1*.o ${TARGET}/usr/lib
}

install_build()
{
	cd $R/apps/output.${ARCH}
	cp -R * ${TARGET}
}

ARCH=i386
TARGET=/mnt
R=`cd .. && pwd`
cd $R

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
