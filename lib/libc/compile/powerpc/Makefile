TARGET:		libc.a
TC_PREFIX?=	../../../../toolchain/prefix.powerpc

CFLAGS=		-I$S/../../include -I. -std=c99 -fPIC
ARCH=		powerpc
CC=		${TC_PREFIX}/bin/powerpc-elf-ananas-gcc
AR=		${TC_PREFIX}/bin/powerpc-elf-ananas-ar

MDOBJS		+= setjmp.o

include ../../conf/Makefile.std

install:	${TC_PREFIX}/powerpc-elf-ananas/lib/libc.a

${TC_PREFIX}/powerpc-elf-ananas/lib/libc.a:	libc.a
		cp libc.a ${TC_PREFIX}/powerpc-elf-ananas/lib


md_syscall.o:	$S/arch/powerpc/md_syscall.S
		$(CC) $(ASMFLAGS) -c -o md_syscall.o $S/arch/powerpc/md_syscall.S

setjmp.o:	$S/arch/powerpc/setjmp.S
		$(CC) $(ASMFLAGS) -c -o setjmp.o $S/arch/powerpc/setjmp.S
