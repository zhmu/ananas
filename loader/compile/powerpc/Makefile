TARGET:		machine loader
S=		../..

CFLAGS=		-mcpu=G4 -DEXT2
OBJS=
include		$S/compile/Makefile.common
include		$S/compile/Makefile.ofw

stub.o:		$S/powerpc/stub.S
		$(CC) $(CFLAGS) -c -o stub.o $S/powerpc/stub.S

platform.o:	$S/powerpc/platform.c
		$(CC) $(CFLAGS) -c -o platform.o $S/powerpc/platform.c

loader:		$(OBJS) $S/conf/ld.powerpc
		$(LD) -b elf32-powerpc -T $S/conf/ld.powerpc -o loader $(OBJS)
		cp loader /srv/tftpd/loader.ppc

ananasppc.iso:	loader boot.tbxi hfs.map
		mkdir -p tmp
		mkdir -p tmp/boot
		cp loader tmp/boot
		cp boot.tbxi tmp/boot
		mkisofs -hfs -o ananasppc.iso -no-desktop -r -hfs-volid "ANA" -part -hfs-bless tmp/boot -l -J -L -map hfs.map tmp
		rm -rf tmp

machine:
		ln -sf ../../../include/sys/powerpc machine

clean:
		rm -f loader machine $(OBJS)
