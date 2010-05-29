TARGET:		bootblock

bootblock.o:	bootblock.s
		$(CC) $(CFLAGS) -c -o bootblock.o bootblock.s

bootblock:	ld.i386 bootblock.o
		$(LD) -T ld.i386 -o bootblock bootblock.o

merge_mbr:	merge_mbr.c
		cc -o merge_mbr merge_mbr.c

clean:
		rm -f bootblock bootblock.o
