ARCH=		amd64

MDOBJS		+= setjmp.o

include		../../conf/Makefile.std

setjmp.o:	$S/arch/amd64/setjmp.S
		$(CC) $(ASMFLAGS) -c -o setjmp.o $S/arch/amd64/setjmp.S
