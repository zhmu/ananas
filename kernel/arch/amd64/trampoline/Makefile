TARGET:		trampoline.a

S=		../../..

CFLAGS=		-m32 -march=i386 -I$S/../include -I$S -I. -std=c99
ASFLAGS=	$(CFLAGS) -DASM

OBJS=		entry.o relocate.o init.o lib.o

trampoline.a:	$(OBJS)
		$(AR) rcs trampoline.a $(OBJS)

clean:
		rm -f trampoline.a $(OBJS)
