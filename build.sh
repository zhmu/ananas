#!/bin/sh
#
# This is the overal Ananas build script; it is responsible for invoking CMake
# and building everything from nothing completely up to something usable.
#
# XXX This should be integrated in a single CMakeLists.txt at some point;
#     however, as we build and install things like libc, this is nontrivial.
#
# Some careful points of detail, search for the [...] for their implementation:
#
# [lld] We force LLVM LLD as linker in several places; the reason is that this
#       always uses .ctors/.dtors instead of the (admittingly more modern)
#       .initarray - we only support for former, and
#
# [ld] We force GNU LD as a linker during the bootstrapping phase - we cannot
#      (yet) build the kernel as it has specific ldscript needs
#      TODO: it is to be checked if this still holds
#
# [ar] If you set CMAKE_..._TARGET, CMake tries to be helpful by
#      looking for ${CMAKE_..._TARGET}-ar which it cannot find and will just
#      silenty try to invoke CMAKE_AR-NOTFOUND. We specifically look for
#      llvm-ar to avoid this.
#
# [cxx] Initially, we set the C++ compiler to 'clang' so that linking C++
#       examples does not fail due to us missing libc++.so and others. Once
#       these are build, we reset the CXX compiler to 'clang++' so we don't
#       have to explicitely mention C++ libraries.
#

# build an internal cmake project using cmake and ninja
build()
{
	workpath=$1
	srcpath=$2

	echo "*** Building ${srcpath}"
	mkdir -p ${WORKDIR}/${workpath}
	cd ${WORKDIR}/${workpath}
	cmake ${CMAKE_ARGS} ${CMAKE_ARGS_EXTRA} ${ROOT}/${srcpath}
	ninja install
}

# builds an external project using autoconf and make
build_external()
{
	srcpath=$1

	if [ ! -f "${ROOT}/external/${srcpath}/configure" ]; then
		echo "*** Autotool-ing ${srcpath}"
		cd ${ROOT}/external/${srcpath}
		autoreconf -if
	fi

	echo "*** Configuring ${srcpath}"
	cd ${ROOT}/external/${srcpath}
	CC="${CLANG_D}" CFLAGS="--target=${TARGET} --sysroot ${OUTDIR} -DJOBS=0" ./configure --host=x86_64-ananas-elf --prefix=/

	echo "*** Building ${srcpath}"
	make

	echo "*** Installing ${srcpath}"
	make install DESTDIR=${OUTDIR}
}

usage()
{
	echo "usage: build.sh [-hc] [-abekps] [-o path] [-w path] [-i disk.img]"
	echo ""
	echo " -h             this help"
	echo " -c             clean temporary directories beforehand"
	echo ""
	echo " -o path        use path as output directory (default: $OUTDIR)"
	echo " -w path        use path as work directory (default: $WORKDIR)"
	echo " -i disk.img    create disk image"
	echo ""
	echo " -a             build everything"
	echo " -b             bootstrap (include, libsyscall, libc, libm, rtld)"
	echo " -e             build externals (dash, coreutils)"
	echo " -k             build kernel (mb-stub, kernel)"
	echo " -p             build c++ support (libunwind, libc++abi, libc++)"
	echo " -s             build system utilities (init, ps, ...)"
}

TARGET="x86_64-ananas-elf"
OUTDIR="/tmp/ananas-build-new"
WORKDIR="/tmp/ananas-work-new"
ROOT=`pwd`
CLANG='clang'
CLANGXX='clang++'
CLANG_D="${CLANG}"
CLANGXX_D="${CLANGXX}"
IMAGE=""

# see if the clang compiler is sensible - we try to invoke it
X=`${CLANG} -v 2>&1`
if [ $? -ne 0 ]; then
	echo "cannot invoke '$CLANG' - is your path set correctly?"
	exit 1
fi

# XXX we should do some preliminary tests to see how sane this clang is
# (i.e. is it modern enough for our uses)

# cheap commandline parser, inspired by https://gist.github.com/jehiah/855086
CLEAN=0
BOOTSTRAP=0
KERNEL=0
EXTERNALS=0
CPLUSPLUS=0
SYSUTILS=0
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
			CPLUSPLUS=1
			SYSUTILS=1
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
		-p)
			CPLUSPLUS=1
			;;
		-s)
			SYSUTILS=1
			;;
		-o)
			shift
			OUTDIR=$1
			;;
		-w)
			shift
			WORKDIR=$1
			;;
		-i)
			shift
			IMAGE=$1
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
fi
mkdir -p ${OUTDIR} ${WORKDIR}

CMAKE_ARGS_EXTRA=""
TOOLCHAIN_TXT="${OUTDIR}/toolchain.txt"
if [ "$BOOTSTRAP" -ne 0 ]; then
	# generate a toolchain.txt file - this is necessary for CMake to correctly
	# cross-build things.
	cat > ${TOOLCHAIN_TXT} <<END
	set(CMAKE_SYSTEM_NAME Linux)

	set(CMAKE_SYSROOT ${OUTDIR})

	# Force the compiler instead of letting CMake detect it - the reason is that
	# we will not have an usable environment when we begin (we are constructing it
	# on the fly)
	set(CMAKE_WARN_DEPRECATED OFF)
	include(CMakeForceCompiler)
	CMAKE_FORCE_C_COMPILER(${CLANG} GNU)
	CMAKE_FORCE_CXX_COMPILER(${CLANGXX} GNU)

	set(CMAKE_ASM_COMPILER \${CMAKE_C_COMPILER})
	set(CMAKE_C_FLAGS "--target=${TARGET}" CACHE STRING "" FORCE)
	set(CMAKE_CXX_FLAGS "--target=${TARGET}" CACHE STRING "" FORCE)

	# [lld] Force lld as linker for shared libraries, but [ld] not for anything else
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=lld" CACHE STRING "Linker flags" FORCE)

	# [ld] Note that we expect CMAKE_LINKER to be 'ld' for now... I ought to check if this is needed
END

	CMAKE_ARGS="-GNinja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=${OUTDIR} -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_TXT}"

	# start by include, libsyscall - these are pre-requisites to build a libc
	build include include
	build libsyscall lib/libsyscall

	# build libraries
	build libc lib/libc
	build crt lib/crt
	build libm lib/libm
	build pthread lib/pthread

	# at this point, we should have a fully functional *C* build environment - replace the bogus toolchain.txt
	# by something sensible to reflect this. If CMake rejects our compiler, we have a problem
	cat > ${TOOLCHAIN_TXT} <<END
set(CMAKE_SYSTEM_NAME Linux)

set(triple ${TARGET})
set(CMAKE_SYSROOT ${OUTDIR})

# ensure we'll have something to assemble things - for whatever reason, CMake
# cannot detect this by itself?
set(CMAKE_ASM_COMPILER \${CMAKE_C_COMPILER})

# [ar] Force llvm-ar to be used
find_program(CMAKE_AR NAMES llvm-ar)

# [lld] force LLVM lld to be used
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=lld" CACHE STRING "Linker flags" FORCE)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld" CACHE STRING "Linker flags" FORCE)

set(CMAKE_C_COMPILER ${CLANG_D})
set(CMAKE_C_COMPILER_TARGET \${triple})
# [cxx] do not yet use clang++ here, we will not yet have all libraries necessary
set(CMAKE_CXX_COMPILER ${CLANG_D})
set(CMAKE_CXX_COMPILER_TARGET \${triple})
END

fi

# assume we have a sensible bootstrapped environment here
CMAKE_ARGS_BASE="-GNinja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=${OUTDIR} -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_TXT}"
CMAKE_ARGS="${CMAKE_ARGS} -DCMAKE_INSTALL_PREFIX=${OUTDIR}"
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

if [ "$CPLUSPLUS" -ne 0 ]; then
	# XXX I wonder if I should really want the usr/ suffix ?
	CMAKE_ARGS="${CMAKE_ARGS_BASE} -DCMAKE_INSTALL_PREFIX=${OUTDIR}/usr"
	CMAKE_ARGS="${CMAKE_ARGS} -DLLVM_PATH=${ROOT}/external/llvm"

	# libunwind
	CMAKE_ARGS_EXTRA="-DLLVM_ENABLE_LIBCXX:BOOL=ON -DLIBUNWIND_ENABLE_THREADS:BOOL=OFF"
	build libunwind external/llvm/projects/libunwind

	# libcxxabi
	CMAKE_ARGS_EXTRA="-DLIBCXXABI_LIBCXX_PATH=${ROOT}/external/llvm/projects/libcxxabi -DLLVM_ENABLE_LIBCXX:BOOL=ON -DLIBCXXABI_ENABLE_THREADS:BOOL=OFF -DLIBCXXABI_USE_LLVM_UNWINDER=YES"
	build libcxxabi external/llvm/projects/libcxxabi

	# libcxx
	CMAKE_ARGS_EXTRA="-DCMAKE_CROSSCOMPILING=True -DLIBCXX_CXX_ABI=libcxxabi -DLIBCXX_CXX_ABI_INCLUDE_PATHS=${ROOT}/external/llvm/projects/libcxxabi/include -DLIBCXX_ENABLE_THREADS:BOOL=OFF -DLIBCXX_ENABLE_EXPERIMENTAL_LIBRARY=NO -DLIBCXX_STANDARD_VER=c++17"
	build libcxx external/llvm/projects/libcxx

	# at this point, we should have a fully functional C/C++ build environment - replace C-only bogus toolchain.txt
	# by something sensible to reflect this. If CMake rejects our compiler, we have a problem
	cat > ${TOOLCHAIN_TXT} <<END
set(CMAKE_SYSTEM_NAME Linux)

set(triple ${TARGET})
set(CMAKE_SYSROOT ${OUTDIR})

# ensure we'll have something to assemble things - for whatever reason, CMake
# cannot detect this by itself?
set(CMAKE_ASM_COMPILER \${CMAKE_C_COMPILER})

# [ar] Force llvm-ar to be used
find_program(CMAKE_AR NAMES llvm-ar)

# [lld] force LLVM lld to be used
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=lld" CACHE STRING "Linker flags" FORCE)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld" CACHE STRING "Linker flags" FORCE)

set(CMAKE_C_COMPILER ${CLANG_D})
set(CMAKE_C_COMPILER_TARGET \${triple})
set(CMAKE_CXX_COMPILER ${CLANGXX_D})
set(CMAKE_CXX_COMPILER_TARGET \${triple})
END
fi

if [ "$SYSUTILS" -ne 0 ]; then
	CMAKE_ARGS="${CMAKE_ARGS_BASE} -DCMAKE_INSTALL_PREFIX=${OUTDIR}"

	build sysutils sysutils
fi

if [ ! -z "$IMAGE" ]; then
	echo "*** Creating disk image ${IMAGE}"
	cd ${ROOT}
	scripts/create-disk-image.py ${IMAGE} ${OUTDIR}
fi
