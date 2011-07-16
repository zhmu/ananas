TC_PREFIX?=	../../../toolchain/prefix.${ARCH}
CC=		${TC_PREFIX}/bin/${ARCH}-elf-ananas-gcc
LD=		${TC_PREFIX}/bin/${ARCH}-elf-ananas-ld
OBJS=		crt1_s.o cstart.o
include		../../../conf/Makefile.inc

CFLAGS=		-Wall -Werror

crt1.o:		$(OBJS)
		$(LD) -r -o crt1.o $(OBJS)

crt1_s.o:	crt1.S
		$(CC) $(CFLAGS) -c -o crt1_s.o crt1.S

cstart.o:	../common/cstart.c
		$(CC) $(CFLAGS) -c -o cstart.o ../common/cstart.c

install:	${TC_PREFIX}/lib/gcc/${ARCH}-elf-ananas/${GCC_VER}/crt1.o

${TC_PREFIX}/lib/gcc/${ARCH}-elf-ananas/${GCC_VER}/crt1.o:	crt1.o
		cp crt1.o ${TC_PREFIX}/lib/gcc/${ARCH}-elf-ananas/${GCC_VER}

clean:
		rm -f crt1.o $(OBJS)
