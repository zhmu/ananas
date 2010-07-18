TC_PREFIX?=	../../../toolchain/prefix.powerpc
CC=		${TC_PREFIX}/bin/powerpc-elf-ananas-gcc
LD=		${TC_PREFIX}/bin/powerpc-elf-ananas-ld
OBJS=		crt1_s.o cstart.o

crt1.o:		$(OBJS)
		$(LD) -r -o crt1.o $(OBJS)

crt1_s.o:	crt1.S
		$(CC) -c -o crt1_s.o crt1.S

cstart.o:	cstart.c
		$(CC) -c -o cstart.o cstart.c

install:	${TC_PREFIX}/lib/gcc/powerpc-elf-ananas/4.4.3/crt1.o

${TC_PREFIX}/lib/gcc/powerpc-elf-ananas/4.4.3/crt1.o:	crt1.o
		cp crt1.o ${TC_PREFIX}/lib/gcc/powerpc-elf-ananas/4.4.3

clean:
		rm -f crt1.o $(OBJS)
