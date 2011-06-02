TARGET:		libc.a
TC_PREFIX?=	../../../../toolchain/prefix.amd64

CFLAGS=		-I$S/../../include -I. -std=c99 -fPIC
CFLAGS+=	-Wall -Werror
ARCH=		amd64
CC=		${TC_PREFIX}/bin/amd64-elf-ananas-gcc
AR=		${TC_PREFIX}/bin/amd64-elf-ananas-ar

MDOBJS		+= setjmp.o

include		../../conf/Makefile.std

install:	${TC_PREFIX}/amd64-elf-ananas/lib/libc.a

${TC_PREFIX}/amd64-elf-ananas/lib/libc.a:	libc.a
		cp libc.a ${TC_PREFIX}/amd64-elf-ananas/lib


setjmp.o:	$S/arch/amd64/setjmp.S
		$(CC) $(ASMFLAGS) -c -o setjmp.o $S/arch/amd64/setjmp.S
