OBJS=		libkerntest.o print.o kernel-glue.o memset.o memcpy.o

# cflags for our wrapper code
WCFLAGS=	-std=c99 -I.
# cflags for kernel stuff
KCFLAGS=	$(WCFLAGS) -I../../include -Dmemset=kmemset -Dmemcpy=kmemcpy
ARCH?=		amd64
K?=		../../kernel

libkerntest:	machine $(OBJS)
		$(CC) -o libkerntest $(OBJS)

libkerntest.o:	ananas libkerntest.c
		$(CC) $(KCFLAGS) -c -o libkerntest.o libkerntest.c

kernel-glue.o:	ananas kernel-glue.c
		$(CC) $(WCFLAGS) -c -o kernel-glue.o kernel-glue.c

# kernel files below here
print.o:	$K/lib/kern/print.c
		$(CC) $(KCFLAGS) -c -o print.o $K/lib/kern/print.c

memset.o:	$K/lib/kern/memset.c
		$(CC) $(KCFLAGS) -c -o memset.o $K/lib/kern/memset.c

memcpy.o:	$K/lib/kern/memcpy.c
		$(CC) $(KCFLAGS) -c -o memcpy.o $K/lib/kern/memcpy.c

machine:
		ln -sf ../../include/ananas/${ARCH} machine

ananas:
		ln -sf ../../include/ananas

clean:
		rm -f libkerntest machine ananas options.h $(OBJS)
