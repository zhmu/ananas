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
CFLAGS=		-I${R}/../../include -I${R}/include-${ARCH}

dash:		.extract .configure .build dash-${ARCH}

dash-${VERSION}.tar.gz:
		${DOWNLOAD} ${MIRROR}/dash-${VERSION}.tar.gz

.extract:	dash-${VERSION}.tar.gz
		${TAR} xf dash-${VERSION}.tar.gz
		@${TOUCH} .extract

.patch:		.extract dash.ananas.diff
		(cd dash-${VERSION} && patch -p1 < ${R}/dash.ananas.diff)
		@${TOUCH} .patch

.configure:	.patch include-${ARCH}
		(cd dash-${VERSION} && ./configure CFLAGS="${CFLAGS}" CC=${CC} LD=${LD} --host=${TARGET})
		@${TOUCH} .configure

.build:		.configure
		(cd dash-${VERSION} && make)
		@${TOUCH} .build

dash-${ARCH}:	.build
		cp dash-${VERSION}/src/dash dash-${ARCH}

include-${ARCH}:
		(mkdir include-${ARCH} && cd include-${ARCH} && ln -sf ${R}/../../include/ananas/${ARCH} machine)

clean:
		rm -rf dash-${VERSION} include-${ARCH}
		rm -f .extract .patch .configure .build dash-${ARCH}
