#!/bin/sh -e

. ./settings.sh

MAKE_ARGS=

# try to auto-detect the number of cpus in this system
if [ -f /proc/cpuinfo ]; then
    NCPUS=`grep processor /proc/cpuinfo|wc -l`
    MAKE_ARGS="${MAKE_ARGS} -j${NCPUS}"
fi

TOOLCHAIN=`realpath ${TOOLCHAIN}`

export PATH=${TOOLCHAIN}/bin:${PATH}

# GMP
mkdir -p /work/tmp/gmp
cd /work/tmp/gmp
../../src/gmp/configure --prefix=${TOOLCHAIN}
make ${MAKE_ARGS}
make install

# Binutils
mkdir -p /work/build/binutils
cd /work/build/binutils
../../src/binutils-gdb/configure --target=${TARGET} --disable-nls --disable-werror --prefix=${TOOLCHAIN} --with-sysroot=${SYSROOT}
make ${MAKE_ARGS}
make install

# MPFR
mkdir -p /work/build/mpfr
cd /work/build/mpfr
../../src/mpfr/configure --prefix=${TOOLCHAIN} --with-gmp=${TOOLCHAIN}
make ${MAKE_ARGS}
make install

# MPC
mkdir -p /work/build/mpc
cd /work/build/mpc
../../src/mpc/configure --prefix=${TOOLCHAIN} --with-gmp=${TOOLCHAIN}
make ${MAKE_ARGS}
make install

# GCC; we try to build as little as possible (i.e. no libgcc)
mkdir -p /work/build/gcc
cd /work/build/gcc
../../src/gcc/configure --target=${TARGET} --disable-nls --with-newlib --without-headers --enable-languages='c,c++' --prefix=${TOOLCHAIN} --disable-libstdcxx --disable-build-with-cxx --disable-libssp --disable-libquadmath --with-sysroot=${SYSROOT} --with-gmp=${TOOLCHAIN} --with-gxx-include-dir=${SYSROOT}/usr/include/c++/${GCC_VERSION}
#make ${MAKE_ARGS}
#make install
make ${MAKE_ARGS} all-gcc
#make ${MAKE_ARGS} all-target-gcc
make install-gcc
#make install-target-gcc

# ensure the 'fixed' gcc includes are removed; the supplied limits.h will
# confuse us (this needs proper fixing - the issue is that the created limits.h
# will not properly provide _MAX defines which fails the coreutils build)
rm -rf ${TOOLCHAIN}/lib/gcc/${TARGET}/${GCC_VERSION}/include-fixed
