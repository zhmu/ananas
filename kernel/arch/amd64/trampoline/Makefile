TARGET:		trampoline

S=		../../..

CFLAGS=		-m32 -march=i386 -I$S/../include -I$S -I. -std=c99
ASFLAGS=	$(CFLAGS) -DASM

OBJS=		entry.o relocate.o init.o lib.o

trampoline:	$(OBJS)
		$(LD) -b elf32-i386 -T $S/conf/ld.trampoline.amd64 -o trampoline $(OBJS)

clean:
		rm -f trampoline $(OBJS)
