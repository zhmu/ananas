ARCH?=		i386
TARGET=		${ARCH}-elf-ananas
PREFIX?=	/
DESTDIR?=	$(realpath ../output.${ARCH})

VERSION?=	8.7
MIRROR?=	http://ftp.gnu.org/gnu/coreutils/
PATCH?=		coreutils.ananas.diff

# tools to use
DOWNLOAD?=	wget
TAR?=		tar
TOUCH?=		touch

R=		${CURDIR}
CC=		${R}/../../toolchain/prefix.${ARCH}/bin/${TARGET}-gcc
LD=		${R}/../../toolchain/prefix.${ARCH}/bin/${TARGET}-ld
# XXX this is unfortunate, but required for now
CFLAGS+=	-DOK_TO_USE_1S_CLOCK
CPPFLAGS=	${CFLAGS}

coreutils:	.install

../distfiles/coreutils-${VERSION}.tar.gz:
		(cd ../distfiles && ${DOWNLOAD} ${MIRROR}/coreutils-${VERSION}.tar.gz)

.extract:	../distfiles/coreutils-${VERSION}.tar.gz
		${TAR} xf ../distfiles/coreutils-${VERSION}.tar.gz
		@${TOUCH} .extract

.patch:		.extract coreutils.ananas.diff
		(cd coreutils-${VERSION} && patch -p1 < ${R}/coreutils.ananas.diff)
		@${TOUCH} .patch

.configure:	.patch
		(cd coreutils-${VERSION} && ./configure CFLAGS="${CFLAGS}" --host=${TARGET} --prefix=${PREFIX} --disable-nls)
		@${TOUCH} .configure

.build:		.configure
		(cd coreutils-${VERSION} && make)
		@${TOUCH} .build

.install:	.build
		(cd coreutils-${VERSION} && make install DESTDIR=${DESTDIR})

clean:
		rm -rf coreutils-${VERSION}
		rm -f .extract .patch .configure .build
