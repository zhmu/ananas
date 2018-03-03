#!/bin/sh
#
# This is the overal Ananas build script; it is responsible for invoking CMake
# and building everything from nothing completely up to something usable.
#
# XXX This should be integrated in a single CMakeLists.txt at some point;
#     however, as we build and install things like libc, this is nontrivial.
#

# build an internal cmake project using cmake and ninja
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

# builds an external project using autoconf and make
build_external()
{
	srcpath=$1

	echo "*** Configuring ${srcpath}"
	cd ${ROOT}/external/${srcpath}
	CC="x86_64-ananas-elf-clang" CFLAGS="--sysroot ${OUTDIR} -DJOBS=0" ./configure --host=x86_64-ananas-elf --prefix=/

	echo "*** Building ${srcpath}"
	make

	echo "*** Installing ${srcpath}"
	make install DESTDIR=${OUTDIR}
}

usage()
{
	echo "usage: build.sh [-hc] [-abku]"
	echo ""
	echo " -h        this help"
	echo " -c        clean temporary directories beforehand"
	echo ""
	echo " -a        build everything"
	echo " -b        bootstrap (include, libsyscall, libc, libm, rtld)"
	echo " -e        build externals (dash, coreutils)"
	echo " -k        build kernel (mb-stub, kernel)"
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

# cheap commandline parser, inspired by https://gist.github.com/jehiah/855086
CLEAN=0
BOOTSTRAP=0
KERNEL=0
EXTERNALS=0
while [ "$1" != "" ]; do
	P="$1"
	case "$P" in
		-h)
			usage
			exit 1
			;;
		-a)
			BOOTSTRAP=1
			KERNEL=1
			EXTERNALS=1
			;;
		-b)
			BOOTSTRAP=1
			;;
		-c)
			CLEAN=1
			;;
		-e)
			EXTERNALS=1
			;;
		-k)
			KERNEL=1
			;;
		*)
			echo "unexpect parameter '$P'"
			usage
			exit 1
			;;
	esac
	shift
done

# from here on, we can no longer deal with failure
set +e

# clean up the output and work directories
if [ "$CLEAN" -ne 0 ]; then
	rm -rf ${OUTDIR} ${WORKDIR}
	mkdir -p ${OUTDIR} ${WORKDIR}
fi

TOOLCHAIN_TXT="${OUTDIR}/toolchain.txt"
if [ "$BOOTSTRAP" -ne 0 ]; then
	# generate a toolchain.txt file - this is necessary for CMake to correctly
	# cross-build things.
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

	CMAKE_ARGS="-GNinja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=${OUTDIR} -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_TXT}"

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

fi

# assume we have a sensible bootstrapped environment here
CMAKE_ARGS="-GNinja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=${OUTDIR} -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_TXT}"
if [ "$BOOTSTRAP" -ne 0 ]; then
	# build the runtime linker so we can run shared library things
	build rtld lib/rtld
fi

if [ "$KERNEL" -ne 0 ]; then
	# build the multiboot stub so we can make something bootable
	build multiboot-stub multiboot-stub

	# build the kernel
	build kernel kernel
fi

if [ "$EXTERNALS" -ne 0 ]; then
	# build dash
	build_external dash

	# build coreutils
	build_external coreutils

	# copy dash to /bin/sh so we can boot things
	cp ${OUTDIR}/bin/dash ${OUTDIR}/bin/sh
fi
