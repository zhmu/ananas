DESTDIR?=	$(realpath ../output.${ARCH})

VERSION?=	0.5.6
MIRROR?=	http://gondor.apana.org.au/~herbert/dash/files/
PATCH?=		dash.ananas.diff

R=		${CURDIR}

include	../Makefile.inc

# tools to use
DOWNLOAD?=	wget
TAR?=		tar
TOUCH?=		touch

dash:		.extract .configure .build .install

../distfiles/dash-${VERSION}.tar.gz:
		(cd ../distfiles && ${DOWNLOAD} ${MIRROR}/dash-${VERSION}.tar.gz)

.extract:	../distfiles/dash-${VERSION}.tar.gz
		${TAR} xf ../distfiles/dash-${VERSION}.tar.gz
		@${TOUCH} .extract

.patch:		.extract dash.ananas.diff
		(cd dash-${VERSION} && patch -p1 < ${R}/dash.ananas.diff)
		@${TOUCH} .patch

.configure:	.patch
		(cd dash-${VERSION} && CC="${CC}" LD="${LD}" CFLAGS="${CFLAGS}" CPPFLAGS="${CFLAGS}" ./configure --host=${TARGET} --prefix=/)
		@${TOUCH} .configure

.build:		.configure
		(cd dash-${VERSION} && ${MAKE})
		@${TOUCH} .build

.install:	.build
		(cd dash-${VERSION} && ${MAKE} install DESTDIR=${DESTDIR})
		cp ${DESTDIR}/bin/dash ${DESTDIR}/bin/sh
		@${TOUCH} .install

clean:
		rm -rf dash-${VERSION}
		rm -f .extract .patch .configure .build .install
