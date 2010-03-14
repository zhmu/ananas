CC=		../../../toolchain/prefix/bin/i586-elf-ananas-gcc
LD=		../../../toolchain/prefix/bin/i586-elf-ananas-ld
OBJS=		crt1_s.o cstart.o

crt1.o:		$(OBJS)
		$(LD) -r -o crt1.o $(OBJS)

crt1_s.o:	crt1.S
		$(CC) -c -o crt1_s.o crt1.S

cstart.o:	cstart.c
		$(CC) -c -o cstart.o cstart.c

install:	crt1.o
		cp crt1.o ../../../toolchain/prefix/lib/gcc/i586-elf-ananas/4.4.3

clean:
		rm -f crt1.o $(OBJS)
