TARGET=		fat
OBJS=		block.o dir.o fatfs.o inode.o
include		../common/Makefile.common

block.o:	machine $K/fs/fat/block.c
		$(CC) -c -o block.o $K/fs/fat/block.c $(CFLAGS)

dir.o:		machine $K/fs/fat/dir.c
		$(CC) -c -o dir.o $K/fs/fat/dir.c $(CFLAGS)

fatfs.o:	machine $K/fs/fat/fatfs.c
		$(CC) -c -o fatfs.o $K/fs/fat/fatfs.c $(CFLAGS)

inode.o:	machine $K/fs/fat/inode.c
		$(CC) -c -o inode.o $K/fs/fat/inode.c $(CFLAGS)
