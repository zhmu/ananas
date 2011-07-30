target:		framework.a

TARGET=		dummy
OBJS=		kernel-glue.o device-glue.o libc-glue.o mm.o print.o framework.o
CLEAN_FILES=	framework.a
include		../Makefile.common
GENEXT2FS?=	genext2fs 

framework.a:	$(OBJS)
		$(AR) cr framework.a $(OBJS)

# glue files
kernel-glue.o:	ananas kernel-glue.c
		$(CC) $(WCFLAGS) -c -o kernel-glue.o kernel-glue.c

device-glue.o:	ananas device-glue.c
		$(CC) $(WCFLAGS) -c -o device-glue.o device-glue.c

libc-glue.o:	ananas libc-glue.c
		$(CC) $(WCFLAGS) -c -o libc-glue.o libc-glue.c

framework.o:	ananas framework.c
		$(CC) $(WCFLAGS) -c -o framework.o framework.c

# kernel files
mm.o:		$K/kern/mm.c ananas
		$(CC) $(KCFLAGS) -c -o mm.o $K/kern/mm.c

print.o:	$K/lib/kern/print.c ananas
		$(CC) $(KCFLAGS) -c -o print.o $K/lib/kern/print.c
