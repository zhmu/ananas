ARCH?=		i386
TARGET=		${ARCH}-elf-ananas
PREFIX?=	$(realpath build.${ARCH})

VERSION?=	2.20.1
MIRROR?=	http://ftp.gnu.org/gnu/binutils/

# tools to use
DOWNLOAD?=	wget
TAR?=		tar
TOUCH?=		touch
SED?=		sed

R=		${CURDIR}
CC=		${R}/../../toolchain/prefix.${ARCH}/bin/${TARGET}-gcc
CPP=		${R}/../../toolchain/prefix.${ARCH}/bin/${TARGET}-cpp
AR=		${R}/../../toolchain/prefix.${ARCH}/bin/${TARGET}-ar
#CFLAGS=		-I${R}/../../include -I${R}/include-${ARCH}

binutils:	.install

binutils-${VERSION}.tar.gz:
		${DOWNLOAD} ${MIRROR}/binutils-${VERSION}.tar.gz

.extract:	binutils-${VERSION}.tar.gz
		${TAR} xf binutils-${VERSION}.tar.gz
		@${TOUCH} .extract

.patch:		.extract
		SED=${SED} ../../toolchain/scripts/binutils_patch.sh binutils-${VERSION}
		@${TOUCH} .patch

.configure:	.patch
		(cd binutils-${VERSION} && ./configure CFLAGS="${CFLAGS}" CPP=${CPP} CPPFLAGS="${CPPFLAGS}" CC=${CC} LD=${LD} AR=${AR} --host=${TARGET} --prefix=${PREFIX})
		@${TOUCH} .configure

.build:		.configure
		(cd binutils-${VERSION} && make)
		@${TOUCH} .build

.install:	.build
		(cd binutils-${VERSION} && make install)

clean:
		rm -rf binutils-${VERSION}
		rm -f .extract .patch .configure .build
