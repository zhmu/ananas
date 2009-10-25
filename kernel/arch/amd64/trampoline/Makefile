TARGET:		trampoline.a

S=		../../..

CFLAGS=		-m32 -march=i386 -I$S/../include -I$S -I. -std=c99
ASFLAGS=	$(CFLAGS) -DASM

OBJS=		entry.o relocate.o init.o lib.o

trampoline.a:	machine $(OBJS)
		$(AR) rcs trampoline.a $(OBJS)

machine:
		ln -sf ../../../../include/i386 machine

clean:
		rm -f trampoline.a machine $(OBJS)
