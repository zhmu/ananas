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
CFLAGS=		-I${R}/../../include -I${R}/include-${ARCH}
# XXX this is unfortunate, but required for now
CFLAGS+=	-DOK_TO_USE_1S_CLOCK
CPPFLAGS=	${CFLAGS}

coreutils:	.install

coreutils-${VERSION}.tar.gz:
		${DOWNLOAD} ${MIRROR}/coreutils-${VERSION}.tar.gz

.extract:	coreutils-${VERSION}.tar.gz
		${TAR} xf coreutils-${VERSION}.tar.gz
		@${TOUCH} .extract

.patch:		.extract coreutils.ananas.diff
		(cd coreutils-${VERSION} && patch -p1 < ${R}/coreutils.ananas.diff)
		@${TOUCH} .patch

.configure:	.patch include-${ARCH}
		(cd coreutils-${VERSION} && ./configure CFLAGS="${CFLAGS}" CPPFLAGS="${CPPFLAGS}" CC=${CC} LD=${LD} --host=${TARGET} --prefix=${PREFIX})
		@${TOUCH} .configure

.build:		.configure
		(cd coreutils-${VERSION} && make)
		@${TOUCH} .build

.install:	.build
		(cd coreutils-${VERSION} && make install)

include-${ARCH}:
		(mkdir include-${ARCH} && cd include-${ARCH} && ln -sf ${R}/../../include/ananas/${ARCH} machine)

clean:
		rm -rf coreutils-${VERSION} include-${ARCH}
		rm -f .extract .patch .configure .build
