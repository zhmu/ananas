ARCH?=		i386
TARGET=		${ARCH}-elf-ananas
PREFIX?=	$(realpath build.${ARCH})

VERSION?=	3.82
MIRROR?=	http://ftp.gnu.org/gnu/make/
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

make-${VERSION}.tar.gz:
		${DOWNLOAD} ${MIRROR}/make-${VERSION}.tar.gz

.extract:	make-${VERSION}.tar.gz
		${TAR} xf make-${VERSION}.tar.gz
		@${TOUCH} .extract

.patch:		.extract make.ananas.diff
		(cd make-${VERSION} && patch -p1 < ${R}/make.ananas.diff)
		@${TOUCH} .patch

.configure:	.patch
		(cd make-${VERSION} && ./configure CFLAGS="${CFLAGS}" CPPFLAGS="${CPPFLAGS}" CC=${CC} LD=${LD} --host=${TARGET} --prefix=${PREFIX})
		@${TOUCH} .configure

.build:		.configure
		(cd make-${VERSION} && make)
		@${TOUCH} .build

.install:	.build
		(cd make-${VERSION} && make install)

clean:
		rm -rf make-${VERSION}
		rm -f .extract .patch .configure .build
