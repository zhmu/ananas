TARGET=		libkerntest
OBJS=		libkerntest.o memset.o memcpy.o
LIBS=		../framework/framework.a
KCFLAGS=	 -Dmemset=kmemset -Dmemcpy=kmemcpy
include		../Makefile.common

libkerntest.o:	ananas libkerntest.c
		$(CC) $(KCFLAGS) -c -o libkerntest.o libkerntest.c

# kernel files below here
memset.o:	ananas $K/lib/kern/memset.c
		$(CC) $(KCFLAGS) -c -o memset.o $K/lib/kern/memset.c

memcpy.o:	ananas $K/lib/kern/memcpy.c
		$(CC) $(KCFLAGS) -c -o memcpy.o $K/lib/kern/memcpy.c
