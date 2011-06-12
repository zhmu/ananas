ARCH?=		i386
TARGET=		${ARCH}-elf-ananas
PREFIX?=	$(realpath build.${ARCH})

VERSION?=	8c
MIRROR?=	http://www.ijg.org/files/
PATCH?=		coreutils.ananas.diff

# tools to use
DOWNLOAD?=	wget
TAR?=		tar
TOUCH?=		touch

R=		${CURDIR}
CC=		${R}/../../toolchain/prefix.${ARCH}/bin/${TARGET}-gcc
LD=		${R}/../../toolchain/prefix.${ARCH}/bin/${TARGET}-ld
CPPFLAGS=	${CFLAGS}

make:		.install

../distfiles/jpegsrc.v${VERSION}.tar.gz:
		(cd ../distfiles && ${DOWNLOAD} ${MIRROR}jpegsrc.v${VERSION}.tar.gz)

.extract:	../distfiles/jpegsrc.v${VERSION}.tar.gz
		${TAR} xf ../distfiles/jpegsrc.v${VERSION}.tar.gz
		@${TOUCH} .extract

.patch:		.extract jpeg.ananas.diff
		(cd jpeg-${VERSION} && patch -p1 < ${R}/jpeg.ananas.diff)
		@${TOUCH} .patch

.configure:	.patch
		(cd jpeg-${VERSION} && ./configure CFLAGS="${CFLAGS}" CPPFLAGS="${CPPFLAGS}" CC=${CC} LD=${LD} --host=${TARGET} --prefix=${PREFIX})
		@${TOUCH} .configure

.build:		.configure
		(cd jpeg-${VERSION} && make)
		@${TOUCH} .build

.install:	.build
		(cd jpeg-${VERSION} && make install)

clean:
		rm -rf jpeg-${VERSION}
		rm -f .extract .patch .configure .build
