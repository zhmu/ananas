TARGET:		machine loader
S=		../..

CFLAGS=		-mcpu=G4 -DISO9660 -DELF -DELF32
OBJS=
include		$S/compile/Makefile.common
include		$S/compile/Makefile.ofw

stub.o:		$S/powerpc/stub.S
		$(CC) $(CFLAGS) -c -o stub.o $S/powerpc/stub.S

platform.o:	$S/powerpc/platform.c
		$(CC) $(CFLAGS) -c -o platform.o $S/powerpc/platform.c

loader:		$(OBJS) $S/conf/ld.powerpc
		$(LD) -b elf32-powerpc -T $S/conf/ld.powerpc -o loader $(OBJS)

ananasppc.iso:	loader boot.tbxi hfs.map ../../../kernel/arch/powerpc/compile/LINT/kernel
		mkdir -p tmp
		mkdir -p tmp/boot
		cp ../../../kernel/arch/powerpc/compile/LINT/kernel tmp
		echo 'load kernel' > tmp/loader.cfg
		echo 'boot' >> tmp/loader.cfg
		cp loader tmp/boot
		cp boot.tbxi tmp/boot
		genisoimage -hfs -o ananasppc.iso -no-desktop -r -hfs-volid "ANA" -part -hfs-bless tmp/boot -l -J -L -map hfs.map tmp
		rm -rf tmp

machine:
		ln -sf ../../../include/ananas/powerpc machine

clean:
		rm -f loader machine $(OBJS)
