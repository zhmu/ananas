TARGET:		libc.a

CFLAGS=		-I$S/../include -std=c99 -fPIC
ARCH=		i386
CC=		./../../../toolchain/prefix/bin/i586-elf-ananas-gcc
AR=		./../../../toolchain/prefix/bin/i586-elf-ananas-ar

MDOBJS		+= md_syscall.o crt0.o setjmp.o

include ../../conf/Makefile.std

install:	libc.a
		cp libc.a ../../../toolchain/prefix/i586-elf-ananas/lib

md_syscall.o:	$S/arch/i386/md_syscall.S
		$(CC) $(ASMFLAGS) -c -o md_syscall.o $S/arch/i386/md_syscall.S

crt0.o:		$S/arch/i386/crt0.S
		$(CC) $(ASMFLAGS) -c -o crt0.o $S/arch/i386/crt0.S


setjmp.o:	$S/arch/i386/setjmp.S
		$(CC) $(ASMFLAGS) -c -o setjmp.o $S/arch/i386/setjmp.S
