#!/bin/sh
#
# This is the overal Ananas build script; it is responsible for invoking CMake
# and building everything from nothing completely up to something usable.
#
# XXX This should be integrated in a single CMakeLists.txt at some point;
#     however, as we build and install things like libc, this is nontrivial.
#

build()
{
	workpath=$1
	srcpath=$2

	echo "*** Building ${srcpath}"
	mkdir -p ${WORKDIR}/${workpath}
	cd ${WORKDIR}/${workpath}
	cmake ${CMAKE_ARGS} ${ROOT}/${srcpath}
	ninja install
}

TARGET="x86_64-ananas-elf"
OUTDIR="/tmp/ananas-build"
WORKDIR="/tmp/ananas-work"
ROOT=`pwd`

# see if the clang compiler is sensible - we try to invoke it
X=`${TARGET}-clang -v 2>&1`
if [ $? -ne 0 ]; then
	echo "cannot invoke ${TARGET}-clang - is your path set correctly?"
	exit 1
fi

# from here on, we can no longer deal with failure
set +e

# clean up the output and work directories
rm -rf ${OUTDIR} ${WORKDIR}
mkdir -p ${OUTDIR} ${WORKDIR}

# generate a toolchain.txt file - this is necessary for CMake to correctly
# cross-build things.
TOOLCHAIN_TXT="${OUTDIR}/toolchain.txt"
cat > ${TOOLCHAIN_TXT} <<END
set(CMAKE_SYSTEM_NAME Linux)

set(triple ${TARGET})

set(CMAKE_SYSROOT ${OUTDIR})

# Force the compiler instead of letting CMake detect it - the reason is that
# we will not have an usable environment when we begin (we are constructing it
# on the fly)
include(CMakeForceCompiler)
CMAKE_FORCE_C_COMPILER(\${triple}-clang GNU)
CMAKE_FORCE_CXX_COMPILER(\${triple}-clang GNU)

set(CMAKE_ASM_COMPILER \${CMAKE_C_COMPILER})

set(CMAKE_LD_LINKER ld)
END

CMAKE_ARGS="-GNinja -DCMAKE_INSTALL_PREFIX=${OUTDIR} -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_TXT}"

# start by include, libsyscall - these are pre-requisites to build a libc
build include include
build libsyscall lib/libsyscall

# build libc, crt and libm
build libc lib/libc
build crt lib/crt
build libm lib/libm

# at this point, we should have a fully functional C build environment - replace the bogus toolchain.txt
# by something sensible to reflect this. If CMake rejects our compiler, we have a problem
cat > ${TOOLCHAIN_TXT} <<END
set(CMAKE_SYSTEM_NAME Linux)

set(triple ${TARGET})
set(CMAKE_SYSROOT ${OUTDIR})

# ensure we'll have something to assemble things - for whatever reason, CMake
# cannot detect this by itself?
set(CMAKE_ASM_COMPILER \${CMAKE_C_COMPILER})

set(CMAKE_C_COMPILER \${triple}-clang)
set(CMAKE_C_COMPILER_TARGET \${triple})
set(CMAKE_CXX_COMPILER \${triple}-clang)
set(CMAKE_CXX_COMPILER_TARGET \${triple})
END

# build the runtime linker so we can run shared library things
build rtld lib/rtld

# build the multiboot stub so we can make something bootable
build multiboot-stub multiboot-stub

# build the kernel
build kernel kernel
