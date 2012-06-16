ARCH?=		i386
TARGET=		${ARCH}-elf-ananas
PREFIX?=	/usr
DESTDIR?=	$(realpath build.${ARCH})

VERSION?=	0.9
MIRROR?=	http://www.multiprecision.org/mpc/download/

# tools to use
DOWNLOAD?=	wget
TAR?=		tar
TOUCH?=		touch

R=		${CURDIR}

mpc:		.install

../distfiles/mpc-${VERSION}.tar.gz:
		(cd ../distfiles && ${DOWNLOAD} ${MIRROR}/mpc-${VERSION}.tar.gz)

.extract:	../distfiles/mpc-${VERSION}.tar.gz
		${TAR} xf ../distfiles/mpc-${VERSION}.tar.gz
		@${TOUCH} .extract

.patch:		.extract mpc.ananas.diff
		(cd mpc-${VERSION} && patch -p1 < ${R}/mpc.ananas.diff)
		@${TOUCH} .patch

.configure:	.patch
		(cd mpc-${VERSION} && ./configure --host=${TARGET} --prefix=${DESTDIR}${PREFIX} --with-gmp=${DESTDIR}${PREFIX} --with-mpfr=${DESTDIR}${PREFIX})
		@${TOUCH} .configure

.build:		.configure
		(cd mpc-${VERSION} && make)
		@${TOUCH} .build

.install:	.build
		(cd mpc-${VERSION} && make install)
		mkdir -p ${R}/../../toolchain/prefix.${ARCH}/${ARCH}-elf-ananas/include
		mkdir -p ${R}/../../toolchain/prefix.${ARCH}/${ARCH}-elf-ananas/lib
		cp -R ${DESTDIR}/${PREFIX}/include/* ${R}/../../toolchain/prefix.${ARCH}/${ARCH}-elf-ananas/include
		cp -R ${DESTDIR}/${PREFIX}/lib/* ${R}/../../toolchain/prefix.${ARCH}/${ARCH}-elf-ananas/lib
		${TOUCH} .install

clean:
		rm -rf mpc-${VERSION}
		rm -f .extract .patch .configure .build
