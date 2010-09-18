TARGET:		machine loader
S=		../..

CFLAGS=		-DEXT2 -DPXE
include		$S/compile/Makefile.common
OBJS+=		pxe.o

stub.o:		$S/x86/stub.S
		$(CC) $(CFLAGS) -c -o stub.o $S/x86/stub.S

platform.o:	$S/x86/platform.c
		$(CC) $(CFLAGS) -c -o platform.o $S/x86/platform.c

pxe.o:		$S/x86/pxe.c
		$(CC) $(CFLAGS) -c -o pxe.o $S/x86/pxe.c

loader:		$(OBJS) $S/conf/ld.x86
		$(LD) -b elf32-i386 -T $S/conf/ld.x86 -o loader $(OBJS)
		../../../boot/merge_mbr ../../../boot/bootblock loader /home/rink/beundisk.img
		cp loader /srv/tftpd

machine:
		ln -sf ../../../include/ananas/i386 machine

clean:
		rm -f loader machine $(OBJS)
