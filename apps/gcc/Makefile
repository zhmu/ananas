ARCH?=		i386
TARGET=		${ARCH}-elf-ananas
PREFIX?=	/usr
DESTDIR?=	$(realpath ../output.${ARCH})

include		../../conf/Makefile.inc

VERSION?=	$(GCC_VER)
MIRROR?=	http://ftp.gnu.org/gnu/gcc/
PATCH?=		gcc.ananas.diff

# tools to use
DOWNLOAD?=	wget
TAR?=		tar
TOUCH?=		touch
SED?=		sed

# XXX this is unfortunate, but required for now
CPPFLAGS=	${CFLAGS}

gcc:		.install

../distfiles/gcc-${VERSION}.tar.gz:
		(cd ../distfiles && ${DOWNLOAD} ${MIRROR}/gcc-${VERSION}/gcc-${VERSION}.tar.gz)

.extract:	../distfiles/gcc-${VERSION}.tar.gz
		${TAR} xf ../distfiles/gcc-${VERSION}.tar.gz
		@${TOUCH} .extract

.patch:		.extract
		SED=${SED} ../../toolchain/scripts/gcc_patch.sh gcc-${VERSION}
		@${TOUCH} .patch

.configure:	.patch
		(cd gcc-${VERSION} && ./configure --host=${TARGET} --prefix=${PREFIX} --disable-nls --without-headers --enable-languages=c --disable-multilib --disable-libssp)
		@${TOUCH} .configure

.build:		.configure
		(cd gcc-${VERSION} && ${MAKE})
		@${TOUCH} .build

.install:	.build
		(cd gcc-${VERSION} && make install DESTDIR=${DESTDIR})

clean:
		rm -rf gcc-${VERSION}
		rm -f .extract .patch .configure .build
