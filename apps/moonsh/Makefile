OBJS=	main.o
# lua
OBJS+=	lapi.o lcode.o ldebug.o ldo.o ldump.o lfunc.o lgc.o llex.o
OBJS+=	lmem.o lobject.o lopcodes.o lparser.o lstate.o lstring.o
OBJS+=	ltable.o ltm.o lundump.o lvm.o lzio.o
OBJS+=	lauxlib.o lbaselib.o ldblib.o
OBJS+=	ltablib.o lstrlib.o linit.o
#OBJS+=	lmathlib.o
OBJS+=	liolib.o loslib.o loadlib.o

ARCH?=	i386
S=	../..

CFLAGS=	-I$S/include -I. -std=c99

moonsh:	machine $(OBJS)
	$(CC) $(CFLAGS) -static -o moonsh $(OBJS) $(LIBS)

machine:
	ln -sf $S/include/sys/${ARCH} machine

clean:
	rm -f moonsh machine $(OBJS)
