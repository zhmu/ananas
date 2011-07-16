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

do_dash()
{
	cd $R/apps/dash
	rm -rf build.${ARCH}
	rm -f dash-${ARCH}
	mkdir -p build.${ARCH}
	make clean
	make
	cp dash-${ARCH} ${TARGET}/bin/sh
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
	make || true
	cd coreutils-8.7
	make install || true
	if [ ! -f $R/apps/coreutils/build.${ARCH}/bin/yes ]; then
		echo  "*** coreutils build failure"
		exit 1
	fi
	cp -R $R/apps/coreutils/build.${ARCH}/* ${TARGET}
}

do_gmp()
{
	cd $R/apps/gmp
	rm -rf build.${ARCH}
	mkdir -p build.${ARCH}
	make clean
	make
	cp -R $R/apps/gmp/build.${ARCH}/* ${TARGET}/usr
}

do_mpfr()
{
	cd $R/apps/mpfr
	rm -rf build.${ARCH}
	mkdir -p build.${ARCH}
	make clean
	make
	cp -R $R/apps/mpfr/build.${ARCH}/* ${TARGET}/usr
}

do_mpc()
{
	cd $R/apps/mpc
	rm -rf build.${ARCH}
	mkdir -p build.${ARCH}
	make clean
	make
	cp -R $R/apps/mpc/build.${ARCH}/* ${TARGET}/usr
}

do_binutils()
{
	cd $R/apps/binutils
	rm -rf build.${ARCH}
	mkdir -p build.${ARCH}
	make clean
	make
	cp -R $R/apps/binutils/build.${ARCH}/* ${TARGET}/usr
}

do_gcc()
{
	cd $R/apps/gcc
	rm -rf build.${ARCH}
	mkdir -p build.${ARCH}
	make clean
	make
	cp -R $R/apps/gcc/build.${ARCH}/* ${TARGET}/usr
}

do_make()
{
	cd $R/apps/make
	rm -rf build.${ARCH}
	mkdir -p build.${ARCH}
	make clean
	make
	cp -R $R/apps/make/build.${ARCH}/* ${TARGET}/usr
}

do_jpeg()
{
	cd $R/apps/jpeg
	rm -rf build.${ARCH}
	mkdir -p build.${ARCH}
	make clean
	make
	cp -R $R/apps/jpeg/build.${ARCH}/* ${TARGET}/usr
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

ARCH=i386
TARGET=/mnt
R=`cd .. && pwd`
cd $R

# prepare target
rm -rf ${TARGET}/*
mkdir -p ${TARGET}/bin
mkdir -p ${TARGET}/usr

do_libc
do_crt
do_dash
do_coreutils
do_gmp
do_mpfr
do_mpc
do_jpeg
do_binutils
do_gcc
do_make

install_includes
install_lib
