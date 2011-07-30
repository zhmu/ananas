TARGET=		mmtest
OBJS=		mmtest.o
LIBS=		../framework/framework.a
include		../Makefile.common

mmtest.o:	ananas mmtest.c
		$(CC) $(KCFLAGS) -c -o mmtest.o mmtest.c
