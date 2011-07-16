ARCH?=		i386
TARGET=		${ARCH}-elf-ananas
PREFIX?=	$(realpath build.${ARCH})

VERSION?=	0.9
MIRROR?=	http://www.multiprecision.org/mpc/download/

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

../distfiles/mpc-${VERSION}.tar.gz:
		(cd ../distfiles && ${DOWNLOAD} ${MIRROR}/mpc-${VERSION}.tar.gz)

.extract:	../distfiles/mpc-${VERSION}.tar.gz
		${TAR} xf ../distfiles/mpc-${VERSION}.tar.gz
		@${TOUCH} .extract

.patch:		.extract mpc.ananas.diff
		(cd mpc-${VERSION} && patch -p1 < ${R}/mpc.ananas.diff)
		@${TOUCH} .patch

.configure:	.patch
		(cd mpc-${VERSION} && ./configure CFLAGS="${CFLAGS}" CPP=${CPP} CPPFLAGS="${CPPFLAGS}" CC=${CC} LD=${LD} NM=${NM} AR=${AR} --host=${TARGET} --prefix=${PREFIX})
		@${TOUCH} .configure

.build:		.configure
		(cd mpc-${VERSION} && make)
		@${TOUCH} .build

.install:	.build
		(cd mpc-${VERSION} && make install)
		mkdir -p ${R}/../../toolchain/prefix.${ARCH}/${ARCH}-elf-ananas/include
		mkdir -p ${R}/../../toolchain/prefix.${ARCH}/${ARCH}-elf-ananas/lib
		cp -R ${PREFIX}/include/* ${R}/../../toolchain/prefix.${ARCH}/${ARCH}-elf-ananas/include
		cp -R ${PREFIX}/lib/* ${R}/../../toolchain/prefix.${ARCH}/${ARCH}-elf-ananas/lib
		${TOUCH} .install

clean:
		rm -rf mpc-${VERSION}
		rm -f .extract .patch .configure .build
