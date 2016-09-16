ARCH=		i386

MDOBJS		+= setjmp.o

include		../../conf/Makefile.std

md_syscall.o:	$S/arch/i386/md_syscall.S
		$(CC) $(ASMFLAGS) -c -o md_syscall.o $S/arch/i386/md_syscall.S

setjmp.o:	$S/arch/i386/setjmp.S
		$(CC) $(ASMFLAGS) -c -o setjmp.o $S/arch/i386/setjmp.S
