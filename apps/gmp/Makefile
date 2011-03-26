ARCH?=		i386
TARGET=		${ARCH}-elf-ananas
PREFIX?=	$(realpath build.${ARCH})

VERSION?=	5.0.1
MIRROR?=	ftp://ftp.gmplib.org/pub/gmp-5.0.1/

# tools to use
DOWNLOAD?=	wget
TAR?=		tar
TOUCH?=		touch

R=		${CURDIR}
CC=		${R}/../../toolchain/prefix.${ARCH}/bin/${TARGET}-gcc
CPP=		${R}/../../toolchain/prefix.${ARCH}/bin/${TARGET}-cpp
AR=		${R}/../../toolchain/prefix.${ARCH}/bin/${TARGET}-ar
LD=		${R}/../../toolchain/prefix.${ARCH}/bin/${TARGET}-ld
NM=		${R}/../../toolchain/prefix.${ARCH}/bin/${TARGET}-nm

binutils:	.install

gmp-${VERSION}.tar.bz2:
		${DOWNLOAD} ${MIRROR}/gmp-${VERSION}.tar.bz2

.extract:	gmp-${VERSION}.tar.bz2
		${TAR} xf gmp-${VERSION}.tar.bz2
		@${TOUCH} .extract

.patch:		.extract gmp.ananas.diff
		(cd gmp-${VERSION} && patch -p1 < ${R}/gmp.ananas.diff)
		@${TOUCH} .patch

.configure:	.patch
		(cd gmp-${VERSION} && ./configure CFLAGS="${CFLAGS}" CPP=${CPP} CPPFLAGS="${CPPFLAGS}" CC=${CC} LD=${LD} NM=${NM} AR=${AR} --host=${TARGET} --prefix=${PREFIX})
		@${TOUCH} .configure

.build:		.configure
		(cd gmp-${VERSION} && make)
		@${TOUCH} .build

.install:	.build
		(cd gmp-${VERSION} && make install)
		mkdir -p ${R}/../../toolchain/prefix.${ARCH}/${ARCH}-elf-ananas/include
		mkdir -p ${R}/../../toolchain/prefix.${ARCH}/${ARCH}-elf-ananas/lib
		cp -R ${PREFIX}/include/* ${R}/../../toolchain/prefix.${ARCH}/${ARCH}-elf-ananas/include
		cp -R ${PREFIX}/lib/* ${R}/../../toolchain/prefix.${ARCH}/${ARCH}-elf-ananas/lib
		${TOUCH} .install

clean:
		rm -rf gmp-${VERSION}
		rm -f .extract .patch .configure .build
