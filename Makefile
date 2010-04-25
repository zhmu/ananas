TC_PREFIX?=	toolchain/prefix.${ARCH}
ifeq (${ARCH},i386)
TARGET=		i586-elf-ananas
endif
ifeq (${ARCH},amd64)
TARGET=		amd64-elf-ananas
endif
ifeq ("${TARGET}", "")
$(error Architecture undefined or unknown - try ARCH='i386' or 'amd64')
endif
KERNEL?=	LINT

.PHONY:		toolchain

all:		pre-everything toolchain crt libc apps kernel

toolchain:	${TC_PREFIX}
		(cd toolchain && make TARGET=${TARGET} PREFIX=$(realpath ${TC_PREFIX}))

crt:
		(cd lib/crt/${ARCH} && make install TC_PREFIX=$(realpath ${TC_PREFIX}))

syscalls:	lib/libc/gen/syscalls.inc.c include/syscalls.h kernel/kern/syscalls.inc.c

lib/libc/gen/syscalls.inc.c:
		(cd kern && make)

libc:		syscalls
		(cd lib/libc/compile/${ARCH} && make install TC_PREFIX=$(realpath ${TC_PREFIX}))

apps:		moonsh

moonsh:		output/moonsh.${ARCH}

output/moonsh.${ARCH}:	libc
		(cd apps/moonsh && make ARCH=${ARCH} CC=$(realpath ${TC_PREFIX})/bin/${TARGET}-gcc)
		cp apps/moonsh/moonsh output/moonsh.${ARCH}

output/moonsh.${ARCH}.inc:	output/moonsh.${ARCH} kernel/tools/file2inc/file2inc
		kernel/tools/file2inc/file2inc output/moonsh.${ARCH} output/moonsh.${ARCH}.inc moonsh

kernel:		output/kernel.${ARCH}

output/kernel.${ARCH}:	kernel/arch/${ARCH}/compile/${KERNEL}/kernel
		cp kernel/arch/${ARCH}/compile/${KERNEL}/kernel output/kernel.${ARCH}

kernel/arch/${ARCH}/compile/${KERNEL}/kernel:	kernel/arch/${ARCH}/compile/${KERNEL} syscalls output/moonsh.${ARCH}.inc
		(cd kernel/arch/${ARCH}/compile/${KERNEL} && make CC=$(realpath ${TC_PREFIX})/bin/${TARGET}-gcc LD=$(realpath ${TC_PREFIX})/bin/${TARGET}-ld)

kernel/arch/${ARCH}/compile/${KERNEL}:	kernel/arch/${ARCH}/conf/${KERNEL} kernel/tools/config/config
		(cd kernel/arch/${ARCH}/conf && ../../../tools/config/config ${KERNEL})

kernel/tools/config/config:
		(cd kernel/tools/config && make)

kernel/tools/file2inc/file2inc:
		(cd kernel/tools/file2inc && make)
		
${TC_PREFIX}:
		mkdir ${TC_PREFIX}

pre-everything:
		if [ ! -d output ]; then mkdir output; fi

clean:
		(cd lib/crt/${ARCH} && make clean)
		(cd kern && make clean)
		(cd lib/libc/compile/${ARCH} && make clean)
		(cd apps/moonsh && make clean)
		(cd kernel/tools/config && make clean)
		(cd kernel/tools/file2inc && make clean)
		(cd kernel/arch/${ARCH}/compile/${KERNEL} && make clean)
		rm -rf output

#
# really-clean also forced the toolchain to be rebuilt
#
really-clean:	clean
		(cd toolchain && make PREFIX=$(realpath ${TC_PREFIX}) clean)
		rm -rf ${TC_PREFIX}
