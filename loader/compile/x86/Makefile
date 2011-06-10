TARGET:		machine loader
S=		../..

CFLAGS=		-DEXT2 -DPXE -DRAMDISK -DX86_64 -DELF32 -DELF64 -DFAT -DVBE
#CFLAGS+=	-DDEBUG_SERIAL
include		$S/compile/Makefile.common
OBJS+=		pxe.o x86_64.o stub_x86_64.o vbe.o

stub.o:		$S/x86/stub.S
		$(CC) $(CFLAGS) -c -o stub.o $S/x86/stub.S

platform.o:	$S/x86/platform.c
		$(CC) $(CFLAGS) -c -o platform.o $S/x86/platform.c

pxe.o:		$S/x86/pxe.c
		$(CC) $(CFLAGS) -c -o pxe.o $S/x86/pxe.c

vbe.o:		$S/x86/vbe.c
		$(CC) $(CFLAGS) -c -o vbe.o $S/x86/vbe.c

x86_64.o:	$S/x86/x86_64.c
		$(CC) $(CFLAGS) -c -o x86_64.o $S/x86/x86_64.c

stub_x86_64.o:	$S/x86/stub_x86_64.S
		$(CC) $(CFLAGS) -c -o stub_x86_64.o $S/x86/stub_x86_64.S

loader:		$(OBJS) $S/conf/ld.x86
		$(LD) -b elf32-i386 -T $S/conf/ld.x86 -o loader $(OBJS)

machine:
		ln -sf ../../../include/ananas/i386 machine

clean:
		rm -f loader machine $(OBJS)
