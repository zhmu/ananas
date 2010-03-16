TARGET:		libc.a
TC_PREFIX?=	../../../../toolchain/prefix

CFLAGS=		-I$S/../../include -I. -std=c99 -fPIC
ARCH=		i386
CC=		${TC_PREFIX}/bin/i586-elf-ananas-gcc
AR=		${TC_PREFIX}/bin/i586-elf-ananas-ar

MDOBJS		+= md_syscall.o setjmp.o

include ../../conf/Makefile.std

install:	${TC_PREFIX}/i586-elf-ananas/lib/libc.a

${TC_PREFIX}/i586-elf-ananas/lib/libc.a:	libc.a
		cp libc.a ${TC_PREFIX}/i586-elf-ananas/lib

md_syscall.o:	$S/arch/i386/md_syscall.S
		$(CC) $(ASMFLAGS) -c -o md_syscall.o $S/arch/i386/md_syscall.S

crt0.o:		$S/arch/i386/crt0.S
		$(CC) $(ASMFLAGS) -c -o crt0.o $S/arch/i386/crt0.S

setjmp.o:	$S/arch/i386/setjmp.S
		$(CC) $(ASMFLAGS) -c -o setjmp.o $S/arch/i386/setjmp.S
