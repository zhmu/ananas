#!/bin/sh -e

. ./settings.sh

MAKE_ARGS=

# try to auto-detect the number of cpus in this system
if [ -f /proc/cpuinfo ]; then
    NCPUS=`grep processor /proc/cpuinfo|wc -l`
    MAKE_ARGS="${MAKE_ARGS} -j${NCPUS}"
fi

TOOLCHAINDIR=`realpath ${TOOLCHAINDIR}`

export PATH=${TOOLCHAINDIR}/bin:${PATH}

# GMP
mkdir -p /work/tmp/gmp
cd /work/tmp/gmp
../../src/gmp/configure --prefix=${TOOLCHAINDIR}
make ${MAKE_ARGS}
make install

# Binutils
mkdir -p /work/build/binutils
cd /work/build/binutils
../../src/binutils-gdb/configure --target=${TARGET} --disable-nls --disable-werror --prefix=${TOOLCHAINDIR} --with-sysroot=${SYSROOTDIR}
make ${MAKE_ARGS}
make install

# MPFR
mkdir -p /work/build/mpfr
cd /work/build/mpfr
../../src/mpfr/configure --prefix=${TOOLCHAINDIR} --with-gmp=${TOOLCHAINDIR}
make ${MAKE_ARGS}
make install

# MPC
mkdir -p /work/build/mpc
cd /work/build/mpc
../../src/mpc/configure --prefix=${TOOLCHAINDIR} --with-gmp=${TOOLCHAINDIR}
make ${MAKE_ARGS}
make install

# GCC; we try to build as little as possible (i.e. no libgcc since that depends
# on libc, which we can't build here)
mkdir -p /work/build/gcc
    #--disable-shared
cd /work/build/gcc
#../../src/gcc/configure \
#    --target=${TARGET} \
#    --prefix=${TOOLCHAINDIR} \
#    --with-sysroot=${SYSROOTDIR} \
#    --with-newlib \
#    --without-headers \
#    --enable-initfini-array \
#    --disable-nls \
#    --disable-multilib \
#    --disable-decimal-float \
#    --disable-threads \
#    --disable-libatomic \
#    --disable-libgomp \
#    --disable-libquadmath \
#    --disable-libssp \
#    --disable-libvtv \
#    --disable-libstdcxx \
#    --enable-languages=c,c++ \
#    --with-gmp=${TOOLCHAINDIR} \
#    --with-gxx-include-dir=${SYSROOTDIR}/usr/include/c++/${GCC_VERSION}
../../src/gcc/configure \
    --target=${TARGET} \
    --disable-nls \
    --with-newlib \
    --without-headers \
    --enable-initfini-array \
    --enable-languages='c,c++' \
    --prefix=${TOOLCHAINDIR} \
    --disable-libstdcxx \
    --disable-build-with-cxx \
    --disable-libssp \
    --disable-libquadmath \
    --with-sysroot=${SYSROOTDIR} \
    --with-gmp=${TOOLCHAINDIR} \
    --with-gxx-include-dir=${SYSROOTDIR}/usr/include/c++/${GCC_VERSION}
make ${MAKE_ARGS} all-gcc
make install-gcc

# GCC will not be able to have found our limits.h as it is not installed yet;
# work around this by cobbling together a limits.h (based on
# https://linuxfromscratch.org/lfs/view/stable/chapter05/gcc-pass1.html)
cat ../../src/gcc/gcc/limitx.h ../../src/gcc/gcc/glimits.h  ../../src/gcc/gcc/limity.h > ${TOOLCHAINDIR}/lib/gcc/${TARGET}/${GCC_VERSION}/include-fixed/limits.h

# path the autotools config.sub to recognize our OS; it will be installed in
# the target image and allow us just to run autoconf to replace the original one
/work/patch-config-sub.py /usr/share/misc/config.sub /opt/files/config.sub
