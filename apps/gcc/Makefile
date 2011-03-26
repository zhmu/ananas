ARCH?=		i386
TARGET=		${ARCH}-elf-ananas
PREFIX?=	$(realpath build.${ARCH})

VERSION?=	4.4.3
MIRROR?=	http://ftp.gnu.org/gnu/gcc/
PATCH?=		gcc.ananas.diff

# tools to use
DOWNLOAD?=	wget
TAR?=		tar
TOUCH?=		touch
SED?=		sed

R=		${CURDIR}
CC=		${R}/../../toolchain/prefix.${ARCH}/bin/${TARGET}-gcc
LD=		${R}/../../toolchain/prefix.${ARCH}/bin/${TARGET}-ld
AR=		${R}/../../toolchain/prefix.${ARCH}/bin/${TARGET}-ar
# XXX this is unfortunate, but required for now
CPPFLAGS=	${CFLAGS}

gcc:		.install

gcc-${VERSION}.tar.gz:
		${DOWNLOAD} ${MIRROR}/gcc-${VERSION}/gcc-${VERSION}.tar.gz

.extract:	gcc-${VERSION}.tar.gz
		${TAR} xf gcc-${VERSION}.tar.gz
		@${TOUCH} .extract

.patch:		.extract
		SED=${SED} ../../toolchain/scripts/gcc_patch.sh gcc-${VERSION}
		@${TOUCH} .patch

.configure:	.patch
		(cd gcc-${VERSION} && ./configure CFLAGS="${CFLAGS}" CPPFLAGS="${CPPFLAGS}" CC=${CC} LD=${LD} AR=${AR} --host=${TARGET} --prefix=${PREFIX} --disable-nls --without-headers --enable-languages=c --disable-multilib --disable-libssp)
		@${TOUCH} .configure

.build:		.configure
		(cd gcc-${VERSION} && make)
		@${TOUCH} .build

.install:	.build
		(cd gcc-${VERSION} && make install)

clean:
		rm -rf gcc-${VERSION}
		rm -f .extract .patch .configure .build
