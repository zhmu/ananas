ARCH?=		i386
TARGET=		${ARCH}-elf-ananas
PREFIX?=	/usr
DESTDIR?=	$(realpath ../output.${ARCH})

VERSION?=	5.0.1
MIRROR?=	https://gmplib.org/download/gmp/

# tools to use
DOWNLOAD?=	wget
TAR?=		tar
TOUCH?=		touch

R=		${CURDIR}

gmp:		.install

../distfiles/gmp-${VERSION}.tar.bz2:
		(cd ../distfiles && ${DOWNLOAD} ${MIRROR}/gmp-${VERSION}.tar.bz2)

.extract:	../distfiles/gmp-${VERSION}.tar.bz2
		${TAR} xf ../distfiles/gmp-${VERSION}.tar.bz2
		@${TOUCH} .extract

.patch:		.extract gmp.ananas.diff
		(cd gmp-${VERSION} && patch -p1 < ${R}/gmp.ananas.diff)
		@${TOUCH} .patch

.configure:	.patch
		(cd gmp-${VERSION} && ./configure --host=${TARGET} --prefix=${DESTDIR}/${PREFIX})
		@${TOUCH} .configure

.build:		.configure
		(cd gmp-${VERSION} && make)
		@${TOUCH} .build

.install:	.build
		(cd gmp-${VERSION} && make install)
		mkdir -p ${R}/../../toolchain/prefix.${ARCH}/${ARCH}-elf-ananas/include
		mkdir -p ${R}/../../toolchain/prefix.${ARCH}/${ARCH}-elf-ananas/lib
		cp -R ${DESTDIR}${PREFIX}/include/* ${R}/../../toolchain/prefix.${ARCH}/${ARCH}-elf-ananas/include
		cp -R ${DESTDIR}${PREFIX}/lib/* ${R}/../../toolchain/prefix.${ARCH}/${ARCH}-elf-ananas/lib
		${TOUCH} .install

clean:
		rm -rf gmp-${VERSION}
		rm -f .extract .patch .configure .build
