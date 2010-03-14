TARGET:		libc.a

CFLAGS=		-m64 -march=athlon64 -I$S/../include -std=c99 -DAMD64
ARCH=		amd64

MDOBJS		+= md_syscall.o crt0.o setjmp.o

include ../../conf/Makefile.std

md_syscall.o:	$S/arch/amd64/md_syscall.S
		$(CC) $(ASMFLAGS) -c -o md_syscall.o $S/arch/amd64/md_syscall.S

crt0.o:		$S/arch/amd64/crt0.S
		$(CC) $(ASMFLAGS) -c -o crt0.o $S/arch/amd64/crt0.S

setjmp.o:	$S/arch/amd64/setjmp.S
		$(CC) $(ASMFLAGS) -c -o setjmp.o $S/arch/amd64/setjmp.S

