TARGET:		libc.a
TC_PREFIX?=	../../../../toolchain/prefix.i386

CFLAGS=		-I$S/../../include -I. -std=c99 
# XXX does not work. wonder why?
#CFLAGS+=	-fPIC
ARCH=		i386
CC=		${TC_PREFIX}/bin/i586-elf-ananas-gcc
AR=		${TC_PREFIX}/bin/i586-elf-ananas-ar

MDOBJS		+= setjmp.o

include		../../conf/Makefile.std

install:	${TC_PREFIX}/i586-elf-ananas/lib/libc.a

${TC_PREFIX}/i586-elf-ananas/lib/libc.a:	libc.a
		cp libc.a ${TC_PREFIX}/i586-elf-ananas/lib

md_syscall.o:	$S/arch/i386/md_syscall.S
		$(CC) $(ASMFLAGS) -c -o md_syscall.o $S/arch/i386/md_syscall.S

setjmp.o:	$S/arch/i386/setjmp.S
		$(CC) $(ASMFLAGS) -c -o setjmp.o $S/arch/i386/setjmp.S
