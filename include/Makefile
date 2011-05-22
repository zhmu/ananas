TC_PREFIX?=	../toolchain/prefix.${ARCH}
GCC_VER?=	4.4.3
DESTDIR?=	${TC_PREFIX}/lib/gcc/${ARCH}-elf-ananas/${GCC_VER}/include

install:
	mkdir -p ${DESTDIR}
	cp -R * ${DESTDIR}
	mkdir -p ${DESTDIR}/machine
	cp -R ananas/${ARCH}/* ${DESTDIR}/machine
