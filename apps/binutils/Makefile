ARCH?=		i386
TARGET=		${ARCH}-elf-ananas
PREFIX?=	/usr
DESTDIR?=	$(realpath ../output.${ARCH})

include		../../conf/Makefile.inc

VERSION?=	$(BINUTILS_VER)
MIRROR?=	http://ftp.gnu.org/gnu/binutils/

# tools to use
DOWNLOAD?=	wget
TAR?=		tar
TOUCH?=		touch
SED?=		sed

binutils:	.install

../distfiles/binutils-${VERSION}.tar.bz2:
		(cd ../distfiles && ${DOWNLOAD} ${MIRROR}/binutils-${VERSION}.tar.bz2)

.extract:	../distfiles/binutils-${VERSION}.tar.bz2
		${TAR} xf ../distfiles/binutils-${VERSION}.tar.bz2
		@${TOUCH} .extract

.patch:		.extract
		SED=${SED} ../../toolchain/scripts/binutils_patch.sh binutils-${VERSION}
		@${TOUCH} .patch

.configure:	.patch
		(cd binutils-${VERSION} && ./configure --host=${TARGET} --prefix=${PREFIX} --disable-nls --without-cloog --disable-libstdcxx)
		@${TOUCH} .configure

.build:		.configure
		(cd binutils-${VERSION} && make)
		@${TOUCH} .build

.install:	.build
		(cd binutils-${VERSION} && make install DESTDIR=${DESTDIR})

clean:
		rm -rf binutils-${VERSION}
		rm -f .extract .patch .configure .build
