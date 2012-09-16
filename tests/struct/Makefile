TARGET=		structtest
OBJS=		structtest.o queue.o dqueue.o cbuffer.o
LIBS=		../framework/framework.a
include		../Makefile.common

structtest.o:	ananas structtest.c
		$(CC) $(WCFLAGS) -c -o structtest.o structtest.c

queue.o:	ananas queue.c
		$(CC) $(WCFLAGS) -c -o queue.o queue.c

dqueue.o:	ananas dqueue.c
		$(CC) $(WCFLAGS) -c -o dqueue.o dqueue.c

cbuffer.o:	ananas cbuffer.c
		$(CC) $(WCFLAGS) -c -o cbuffer.o cbuffer.c
