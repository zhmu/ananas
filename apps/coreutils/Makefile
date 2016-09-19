ARCH?=		i386
TARGET=		${ARCH}-elf-ananas
PREFIX?=	/
DESTDIR?=	$(realpath ../output.${ARCH})

VERSION?=	8.23
MIRROR?=	http://ftp.gnu.org/gnu/coreutils/
PATCH?=		coreutils.ananas.diff

# tools to use
DOWNLOAD?=	wget
TAR?=		tar
TOUCH?=		touch

R=		${CURDIR}

include	../Makefile.inc

# XXX this is unfortunate, but required for now
CFLAGS+=	-DOK_TO_USE_1S_CLOCK
CPPFLAGS=	${CFLAGS}

coreutils:	.install

../distfiles/coreutils-${VERSION}.tar.xz:
		(cd ../distfiles && ${DOWNLOAD} ${MIRROR}/coreutils-${VERSION}.tar.xz)

.extract:	../distfiles/coreutils-${VERSION}.tar.xz
		${TAR} xf ../distfiles/coreutils-${VERSION}.tar.xz
		@${TOUCH} .extract

.patch:		.extract coreutils.ananas.diff
		(cd coreutils-${VERSION} && patch -p1 < ${R}/coreutils.ananas.diff)
		@${TOUCH} .patch

#
# We must invoke autoreconf as we patch the Makefile.am files, which means the
# corresponding Makefile.in's need to be recreated.
#
.autoreconf:	.patch
		(cd coreutils-${VERSION} && autoreconf)
		@${TOUCH} .autoreconf

.configure:	.autoreconf
		(cd coreutils-${VERSION} && CC="${CC}" LD="${LD}" CFLAGS="${CFLAGS}" CPPFLAGS="${CFLAGS}" ./configure --host=${TARGET} --prefix=/)
		@${TOUCH} .configure

.build:		.configure
		(cd coreutils-${VERSION} && make)
		@${TOUCH} .build

.install:	.build
		(cd coreutils-${VERSION} && make install DESTDIR=${DESTDIR})

clean:
		rm -rf coreutils-${VERSION}
		rm -f .extract .patch .configure .build .autoreconf
