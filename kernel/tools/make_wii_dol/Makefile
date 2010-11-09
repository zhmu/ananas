CFLAGS+=	-std=c99

PPC_CC?=	../../../toolchain/prefix.powerpc/bin/powerpc-elf-ananas-gcc
PPC_LD?=	../../../toolchain/prefix.powerpc/bin/powerpc-elf-ananas-ld

make_dol:	make_dol.c trampoline.inc
		$(CC) $(CFLAGS) -o make_dol make_dol.c

trampoline.inc:	trampoline.bin bin2inc.pl
		perl bin2inc.pl trampoline.bin trampoline trampoline.inc

trampoline.bin:	trampoline.o ld.trampoline
		$(PPC_LD) -T ld.trampoline -o trampoline.bin trampoline.o

trampoline.o:	trampoline.S
		$(PPC_CC) -nostdlib -c -o trampoline.o trampoline.S

clean:
		rm -f make_dol trampoline.bin trampoline.inc trampoline.o
