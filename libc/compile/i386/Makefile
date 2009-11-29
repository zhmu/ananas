TARGET:		libc.a

CFLAGS=		-m32 -march=i386 -I$S/../include -std=c99
ARCH=		i386

MDOBJS		+= md_syscall.o crt0.o

include ../../conf/Makefile.std

md_syscall.o:	$S/arch/i386/md_syscall.S
		$(CC) $(ASMFLAGS) -c -o md_syscall.o $S/arch/i386/md_syscall.S

crt0.o:		$S/arch/i386/crt0.S
		$(CC) $(ASMFLAGS) -c -o crt0.o $S/arch/i386/crt0.S

