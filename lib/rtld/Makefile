OBJS=		start.o rtld.o debug.o lib.o malloc.o

CXX=		${TOOL_PREFIX}clang
LINK=		gcc
ARCH=		amd64

CXXFLAGS+=	--sysroot=${SYSROOT}
CXXFLAGS+=	-std=c++11 -fno-rtti -fno-exceptions -fno-builtin
CXXFLAGS+=	-fPIC -O3 -g
# hide all symbols by default so we won't accidently publish things applications can use
CXXFLAGS+=	-fvisibility=hidden

LDFLAGS+=	-shared -Wl,-Bsymbolic -Wl,--hash-style=sysv
LDFLAGS+=	-nostdlib -e entry

all:		machine ld-ananas.so

machine:	../../include/ananas/${ARCH}
		ln -sf ../../include/ananas/${ARCH} machine

start.o:	${ARCH}/start.S
		$(CXX) $(CXXFLAGS) -c -o start.o ${ARCH}/start.S

rtld.o:		common/rtld.cpp
		$(CXX) $(CXXFLAGS) -c -o rtld.o common/rtld.cpp

debug.o:	common/debug.cpp
		$(CXX) $(CXXFLAGS) -c -o debug.o common/debug.cpp

lib.o:		common/lib.cpp
		$(CXX) $(CXXFLAGS) -c -o lib.o common/lib.cpp

malloc.o:	common/malloc.cpp
		$(CXX) $(CXXFLAGS) -c -o malloc.o common/malloc.cpp

ld-ananas.so:	$(OBJS)
		$(LINK) $(LDFLAGS) -o ld-ananas.so $(OBJS)

clean:
		rm -f ld-ananas.so $(OBJS)
