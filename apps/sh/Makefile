OBJS=	main.o
LIBS=	../../libc/compile/libc.a

CFLAGS=	-m32 -march=i386 -I$S/../include -std=c99

sh:	$(OBJS)
	$(CC) $(CFLAGS) -Bstatic -nostdlib -u _start -o sh $(OBJS) $(LIBS)

clean:
	rm -f sh $(OBJS)
