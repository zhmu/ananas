ARCH?=		i386
TARGET=		${ARCH}-elf-ananas

VERSION?=	0.5.6
MIRROR?=	http://gondor.apana.org.au/~herbert/dash/files/
PATCH?=		dash.ananas.diff

# tools to use
DOWNLOAD?=	wget
TAR?=		tar
TOUCH?=		touch

R=		${CURDIR}
CC=		${R}/../../toolchain/prefix.${ARCH}/bin/${TARGET}-gcc
LD=		${R}/../../toolchain/prefix.${ARCH}/bin/${TARGET}-ld

dash:		.extract .configure .build dash-${ARCH}

../distfiles/dash-${VERSION}.tar.gz:
		(cd ../distfiles && ${DOWNLOAD} ${MIRROR}/dash-${VERSION}.tar.gz)

.extract:	../distfiles/dash-${VERSION}.tar.gz
		${TAR} xf ../distfiles/dash-${VERSION}.tar.gz
		@${TOUCH} .extract

.patch:		.extract dash.ananas.diff
		(cd dash-${VERSION} && patch -p1 < ${R}/dash.ananas.diff)
		@${TOUCH} .patch

.configure:	.patch
		(cd dash-${VERSION} && ./configure CFLAGS="${CFLAGS}" CC=${CC} LD=${LD} --host=${TARGET})
		@${TOUCH} .configure

.build:		.configure
		(cd dash-${VERSION} && ${MAKE})
		@${TOUCH} .build

dash-${ARCH}:	.build
		cp dash-${VERSION}/src/dash dash-${ARCH}

clean:
		rm -rf dash-${VERSION}
		rm -f .extract .patch .configure .build dash-${ARCH}
