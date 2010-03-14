OBJS=	main.o
S=	../..
LIBS=	$S/libc/compile/i386/libc.a

CFLAGS=	-m32 -march=i386 -I$S/include -I. -std=c99

sh:	machine $(OBJS)
	$(CC) $(CFLAGS) -Bstatic -nostdlib -u _start -o sh $(OBJS) $(LIBS)

machine:
	ln -sf $S/include/sys/i386 machine

clean:
	rm -f sh machine $(OBJS)
