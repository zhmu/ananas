TARGET=		structtest
OBJS=		structtest.o queue.o list.o cbuffer.o
LIBS=		../framework/framework.a
include		../Makefile.common

structtest.o:	ananas structtest.c
		$(CC) $(WCFLAGS) -c -o structtest.o structtest.c

queue.o:	ananas queue.c
		$(CC) $(WCFLAGS) -c -o queue.o queue.c

list.o:		ananas list.c
		$(CC) $(WCFLAGS) -c -o list.o list.c

cbuffer.o:	ananas cbuffer.c
		$(CC) $(WCFLAGS) -c -o cbuffer.o cbuffer.c
