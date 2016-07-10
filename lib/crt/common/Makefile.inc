CC=		${TOOL_PREFIX}clang
LD=		${TOOL_PREFIX}ld

OBJS=		crt1_s.o cstart.o crtn.o

# flags
CFLAGS=		--sysroot ${SYSROOT}
CFLAGS+=	-I$S/../../include -I. -std=c99
CFLAGS+=	-Wall -Werror
CFLAGS+=	-Wno-builtin-requires-header

crt1.o:		crt1_s.o cstart.o
		$(LD) -r -o crt1.o crt1_s.o cstart.o

crt1_s.o:	crt1.S
		$(CC) $(CFLAGS) -c -o crt1_s.o crt1.S

cstart.o:	../common/cstart.c
		$(CC) $(CFLAGS) -c -o cstart.o ../common/cstart.c

install:	${SYSROOT}/lib/crt1.o ${SYSROOT}/lib/crtn.o

${SYSROOT}/lib/crt1.o: crt1.o
		cp crt1.o ${SYSROOT}/lib

${SYSROOT}/lib/crtn.o: crtn.o
		cp crtn.o ${SYSROOT}/lib

clean:
		rm -f crt1.o $(OBJS)
