TARGET:		machine loader
S=		../..

CFLAGS=		-I$S/../include -I$S -I. -std=c99
CFLAGS+=	-DDEBUG
OBJS=		stub.o platform.o main.o lib.o io.o diskio.o ext2fs.o vfs.o \
		elf.o

stub.o:		$S/x86/stub.S
		$(CC) $(CFLAGS) -c -o stub.o $S/x86/stub.S

platform.o:	$S/x86/platform.c
		$(CC) $(CFLAGS) -c -o platform.o $S/x86/platform.c

main.o:		$S/common/main.c
		$(CC) $(CFLAGS) -c -o main.o $S/common/main.c

lib.o:		$S/common/lib.c
		$(CC) $(CFLAGS) -c -o lib.o $S/common/lib.c

io.o:		$S/common/io.c
		$(CC) $(CFLAGS) -c -o io.o $S/common/io.c

diskio.o:	$S/common/diskio.c
		$(CC) $(CFLAGS) -c -o diskio.o $S/common/diskio.c

ext2fs.o:	$S/fs/ext2fs.c
		$(CC) $(CFLAGS) -c -o ext2fs.o $S/fs/ext2fs.c

vfs.o:		$S/common/vfs.c
		$(CC) $(CFLAGS) -c -o vfs.o $S/common/vfs.c

elf.o:		$S/common/elf.c
		$(CC) $(CFLAGS) -c -o elf.o $S/common/elf.c

loader:		$(OBJS) $S/conf/ld.x86
		$(LD) -b elf32-i386 -T $S/conf/ld.x86 -o loader $(OBJS)
		../../../boot/merge_mbr ../../../boot/bootblock loader /home/rink/beundisk.img

machine:
		ln -sf ../../../include/sys/i386 machine

clean:
		rm -f loader machine $(OBJS)
