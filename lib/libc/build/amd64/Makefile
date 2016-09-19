ARCH=		amd64

MDOBJS		+= setjmp.o

include		../Makefile.std

setjmp.o:	$S/platform/ananas/arch/${ARCH}/setjmp.S
		$(CC) $(CFLAGS) -DASM -c -o setjmp.o $S/platform/ananas/arch/${ARCH}/setjmp.S

