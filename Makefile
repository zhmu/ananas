TC_PREFIX?=	toolchain/prefix.${ARCH}
ifeq (${ARCH},i386)
TARGET=		i386-elf-ananas
endif
ifeq (${ARCH},amd64)
TARGET=		amd64-elf-ananas
endif
ifeq ("${TARGET}", "")
$(error Architecture undefined or unknown - try ARCH='i386' or 'amd64')
endif
KERNEL?=	LINT

TOUCH?=		touch

.PHONY:		toolchain

all:		pre-everything toolchain crt libc kernel

toolchain:	${TC_PREFIX}
		(cd toolchain && ${MAKE} TARGET=${TARGET} PREFIX=$(realpath ${TC_PREFIX}))

crt:
		(cd lib/crt/${ARCH} && ${MAKE} install TC_PREFIX=$(realpath ${TC_PREFIX}))

syscalls:	.syscalls

.syscalls:	kern/syscalls.in
		(cd kern && ${MAKE} AWK=${AWK})
		@${TOUCH} .syscalls

libc:		syscalls
		(cd lib/libc/compile/${ARCH} && ${MAKE} install TC_PREFIX=$(realpath ${TC_PREFIX}))

kernel:		output/kernel.${ARCH}

output/kernel.${ARCH}:	kernel/arch/${ARCH}/compile/${KERNEL}/kernel
		cp kernel/arch/${ARCH}/compile/${KERNEL}/kernel output/kernel.${ARCH}

kernel/arch/${ARCH}/compile/${KERNEL}/kernel:	kernel/arch/${ARCH}/compile/${KERNEL} syscalls
		(cd kernel/arch/${ARCH}/compile/${KERNEL} && ${MAKE} CC=$(realpath ${TC_PREFIX})/bin/${TARGET}-gcc LD=$(realpath ${TC_PREFIX})/bin/${TARGET}-ld OBJCOPY=$(realpath ${TC_PREFIX}/bin/${TARGET}-objcopy) OBJDUMP=$(realpath ${TC_PREFIX}/bin/${TARGET}-objdump))

kernel/arch/${ARCH}/compile/${KERNEL}:	kernel/arch/${ARCH}/conf/${KERNEL} kernel/tools/config/config
		(cd kernel/arch/${ARCH}/conf && ../../../tools/config/config ${KERNEL})

kernel/tools/config/config:
		(cd kernel/tools/config && ${MAKE})

kernel/tools/file2inc/file2inc:
		(cd kernel/tools/file2inc && ${MAKE})
		
${TC_PREFIX}:
		mkdir ${TC_PREFIX}

pre-everything:
		if [ ! -d output ]; then mkdir output; fi

clean:
		(cd lib/crt/${ARCH} && ${MAKE} clean)
		(cd kern && ${MAKE} clean)
		(cd lib/libc/compile/${ARCH} && ${MAKE} clean)
		(cd kernel/tools/config && ${MAKE} clean)
		(cd kernel/tools/file2inc && ${MAKE} clean)
		(cd kernel/arch/${ARCH}/compile/${KERNEL} && ${MAKE} clean)
		rm -rf output .syscalls

#
# really-clean also forced the toolchain to be rebuilt
#
really-clean:	clean
		(cd toolchain && ${MAKE} PREFIX=$(realpath ${TC_PREFIX}) clean)
		rm -rf ${TC_PREFIX}
