TC_PREFIX?=	../../../toolchain/prefix
CC=		${TC_PREFIX}/bin/amd64-elf-ananas-gcc
LD=		${TC_PREFIX}/bin/amd64-elf-ananas-ld
OBJS=		crt1_s.o cstart.o

crt1.o:		$(OBJS)
		$(LD) -r -o crt1.o $(OBJS)

crt1_s.o:	crt1.S
		$(CC) -c -o crt1_s.o crt1.S

cstart.o:	cstart.c
		$(CC) -c -o cstart.o cstart.c

install:	${TC_PREFIX}/lib/gcc/amd64-elf-ananas/4.4.3/crt1.o

${TC_PREFIX}/lib/gcc/amd64-elf-ananas/4.4.3/crt1.o:	crt1.o
		cp crt1.o ${TC_PREFIX}/lib/gcc/amd64-elf-ananas/4.4.3

clean:
		rm -f crt1.o $(OBJS)
