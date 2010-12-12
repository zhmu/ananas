OBJS=		mmtest.o mm.o kernel-glue.o

# cflags for our wrapper code
WCFLAGS=	-std=c99 -I.
# cflags for kernel stuff
KCFLAGS=	$(WCFLAGS) -I../../include
ARCH?=		amd64
K?=		../../kernel

mmtest:		machine $(OBJS)
		$(CC) -o mmtest $(OBJS)

mmtest.o:	ananas mmtest.c
		$(CC) $(KCFLAGS) -c -o mmtest.o mmtest.c

kernel-glue.o:	ananas kernel-glue.c
		$(CC) $(WCFLAGS) -c -o kernel-glue.o kernel-glue.c

# kernel files below here
mm.o:		$K/kern/mm.c
		$(CC) $(KCFLAGS) -c -o mm.o $K/kern/mm.c

machine:
		ln -sf ../../include/ananas/${ARCH} machine

ananas:
		ln -sf ../../include/ananas

clean:
		rm -f mmtest machine ananas options.h $(OBJS)
