TARGET:		mbstub
OBJS=		stub.o main.o io.o relocate.o lib.o amd64.o bootinfo.o verify.o
CFLAGS+=	-std=c99 -m32 -O2 -mno-red-zone -mno-sse -I../include -DLOADER -Wall
CFLAGS+=	-DDEBUG=0

mbstub:		$(OBJS) ldscript
		$(LD) -b elf32-i386 -T ldscript -o mbstub $(OBJS)

stub.o:	stub.S
		$(CC) $(CFLAGS) -c -o stub.o stub.S

clean:
		rm -f mbstub $(OBJS)
