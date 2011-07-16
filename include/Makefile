include		../conf/Makefile.inc
ifeq ("${ARCH}", "")
$(error Architecture undefined or unknown)
endif
TC_PREFIX?=	../toolchain/prefix.${ARCH}
DESTDIR?=	${TC_PREFIX}/lib/gcc/${ARCH}-elf-ananas/${GCC_VER}/include

install:
	mkdir -p ${DESTDIR}
	cp -R * ${DESTDIR}
	mkdir -p ${DESTDIR}/machine
	cp -R ananas/${ARCH}/* ${DESTDIR}/machine
