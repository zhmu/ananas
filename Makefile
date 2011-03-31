TC_PREFIX?=	toolchain/prefix.${ARCH}
ifeq (${ARCH},i386)
TARGET=		i386-elf-ananas
endif
ifeq (${ARCH},amd64)
TARGET=		amd64-elf-ananas
endif
ifeq (${ARCH},powerpc)
TARGET=		powerpc-elf-ananas
endif
ifeq ("${TARGET}", "")
$(error Architecture undefined or unknown - try ARCH='i386', 'amd64' or 'powerpc')
endif
KERNEL?=	LINT

TOUCH?=		touch

.PHONY:		toolchain

all:		pre-everything toolchain.${ARCH} crt libc.${ARCH} kernel dash

toolchain.${ARCH}:	${TC_PREFIX}
		(cd toolchain && ${MAKE} ARCH=${ARCH} TARGET=${TARGET} PREFIX=$(realpath ${TC_PREFIX}))

crt:
		(cd lib/crt/${ARCH} && ${MAKE} install TC_PREFIX=$(realpath ${TC_PREFIX}))

syscalls:	.syscalls

.syscalls:	kern/syscalls.in
		(cd kern && ${MAKE} AWK=${AWK})
		@${TOUCH} .syscalls

libc.${ARCH}:	syscalls
		(cd lib/libc/compile/${ARCH} && ${MAKE} install TC_PREFIX=$(realpath ${TC_PREFIX}))

headers.${ARCH}:
		(cd include && ${MAKE} install ARCH=${ARCH} TC_PREFIX=$(realpath ${TC_PREFIX}))

kernel:		output.${ARCH}/kernel

dash:		output.${ARCH}/dash

output.${ARCH}/kernel:	kernel/arch/${ARCH}/compile/${KERNEL}/kernel
		cp kernel/arch/${ARCH}/compile/${KERNEL}/kernel output.${ARCH}/kernel

kernel/arch/${ARCH}/compile/${KERNEL}/kernel:	kernel/arch/${ARCH}/compile/${KERNEL} syscalls
		(cd kernel/arch/${ARCH}/compile/${KERNEL} && ${MAKE} CC=$(realpath ${TC_PREFIX})/bin/${TARGET}-gcc LD=$(realpath ${TC_PREFIX})/bin/${TARGET}-ld OBJCOPY=$(realpath ${TC_PREFIX}/bin/${TARGET}-objcopy) OBJDUMP=$(realpath ${TC_PREFIX}/bin/${TARGET}-objdump) NM=$(realpath ${TC_PREFIX}/bin/${TARGET}-nm))

kernel/arch/${ARCH}/compile/${KERNEL}:	kernel/arch/${ARCH}/conf/${KERNEL} kernel/tools/config/config
		(cd kernel/arch/${ARCH}/conf && ../../../tools/config/config ${KERNEL})

output.${ARCH}/dash:
		(cd apps/dash && ${MAKE} ARCH=${ARCH})
		cp apps/dash/dash-${ARCH} output.${ARCH}/dash

kernel/tools/config/config:
		(cd kernel/tools/config && ${MAKE})

kernel/tools/file2inc/file2inc:
		(cd kernel/tools/file2inc && ${MAKE})
		
${TC_PREFIX}:
		mkdir ${TC_PREFIX}

pre-everything:
		if [ ! -d output.${ARCH} ]; then mkdir output.${ARCH}; fi

clean:
		(cd lib/crt/${ARCH} && ${MAKE} clean)
		(cd kern && ${MAKE} clean)
		(cd lib/libc/compile/${ARCH} && ${MAKE} clean)
		(cd kernel/tools/config && ${MAKE} clean)
		(cd kernel/tools/file2inc && ${MAKE} clean)
		(cd apps/dash && ${MAKE} ARCH=${ARCH} clean)
		(if [ -d kernel/arch/${ARCH}/compile/${KERNEL} ]; then cd kernel/arch/${ARCH}/compile/${KERNEL} && ${MAKE} clean; fi)
		rm -rf output.${ARCH} .syscalls

#
# really-clean also forces the toolchain to be completely unextracted / built
#
really-clean:	clean
		(cd toolchain && ${MAKE} ARCH=${ARCH} PREFIX=$(realpath ${TC_PREFIX}) reallyclean)
		rm -rf ${TC_PREFIX}
