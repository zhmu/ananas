TARGET=		sio
OBJS=		sio.o
include		../common/Makefile.common

sio.o:		machine $K/dev/sio.c
		$(CC) -c -o sio.o $K/dev/sio.c $(CFLAGS)
