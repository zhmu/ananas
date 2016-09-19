ifeq ("${SYSROOT}", "")
$(error Sysroot undefined)
endif
DESTDIR?=	${SYSROOT}/usr/include

install:
	mkdir -p ${DESTDIR}
	cp -R * ${DESTDIR}
	mkdir -p ${DESTDIR}/machine
	cp -R ananas/${ARCH}/* ${DESTDIR}/machine
