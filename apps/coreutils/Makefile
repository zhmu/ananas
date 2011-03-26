ARCH?=		i386
TARGET=		${ARCH}-elf-ananas
PREFIX?=	$(realpath build.${ARCH})

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
		(cd coreutils-${VERSION} && ./configure CFLAGS="${CFLAGS}" CPPFLAGS="${CPPFLAGS}" CC=${CC} LD=${LD} --host=${TARGET} --prefix=${PREFIX})
		@${TOUCH} .configure

.build:		.configure
		(cd coreutils-${VERSION} && make)
		@${TOUCH} .build

.install:	.build
		(cd coreutils-${VERSION} && make install)

clean:
		rm -rf coreutils-${VERSION}
		rm -f .extract .patch .configure .build
