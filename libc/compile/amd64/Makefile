TARGET:		libc.a

CFLAGS=		-m64 -march=athlon64 -I$S/../include -std=c99
ARCH=		amd64

MDOBJS		+= md_syscall.o crt0.o

include ../../conf/Makefile.std

md_syscall.o:	$S/arch/amd64/md_syscall.S
		$(CC) $(ASMFLAGS) -c -o md_syscall.o $S/arch/amd64/md_syscall.S

crt0.o:		$S/arch/amd64/crt0.S
		$(CC) $(ASMFLAGS) -c -o crt0.o $S/arch/amd64/crt0.S

