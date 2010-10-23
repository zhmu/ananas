# ARCH can be any of {i386,amd64,powerpc,avr32}
ARCH?=		i386
PREFIX?=	$(realpath prefix)
TARGET=		${ARCH}-elf-ananas

# determine binutils/gcc versions to use and extra patches
ifeq '${TARGET}' 'avr32-elf-ananas'
# AVR32 port requires an older binutils/gcc with patches
BINUTILS_VER=	2.18
GCC_VER=	4.2.2
BINUTILS_PATCH=	http://avr32linux.org/twiki/pub/Main/BinutilsPatches/binutils-2.18.atmel.1.0.1.patch.bz2
GCC_PATCH=	http://avr32linux.org/twiki/pub/Main/GccPatches/gcc-4.2.2.atmel.1.1.3.patch.bz2
else
# everything else is supported by mainline binutils/gcc
BINUTILS_VER=	2.20.1
GCC_VER=	4.4.3
endif
GDB_VER=	7.1

# environment settings
MIRROR?=	http://ftp.gnu.org/gnu
#MAKE_BUILDARG?=	-j4

# tools to use
DOWNLOAD?=	wget
TAR?=		tar
TOUCH?=		touch
ECHO?=		echo
SED?=		sed

TARGET:		gcc

binutils:	pre-everything .bu.includes
gcc:		binutils .gcc.extract .gcc.patch .gcc.configure .gcc.build .gcc.install
gdb:		.gdb.install

#
# This kludge exists because gmake will have processed $(realpath ...) before
# the directory is created, causing it to be empty. Force the user to create
# the prefix directory before we continue.
#
pre-everything:
		@if [ "x${PREFIX}" = "x" ]; then echo "*** prefix directory does not exist - please create"; false; fi

# binutils steps
.bu.extract:	distfiles/binutils-${BINUTILS_VER}.tar.bz2
		@${ECHO} ">> Extracting binutils-${BINUTILS_VER}.tar.bz2..."
		if [ ! -d src ]; then mkdir src; fi
		${TAR} xf distfiles/binutils-${BINUTILS_VER}.tar.bz2 -C src
		@${TOUCH} .bu.extract

ifdef BINUTILS_PATCH
distfiles/binutils-${BINUTILS_VER}.diff.bz2:
		@${ECHO} ">> Downloading binutils-${BINUTILS_VER}.diff.bz2"
		if [ ! -d distfiles ]; then mkdir distfiles; fi
		(cd distfiles && ${DOWNLOAD} -O binutils-${BINUTILS_VER}.diff.bz2 ${BINUTILS_PATCH})

.bu.prepatch:	distfiles/binutils-${BINUTILS_VER}.diff.bz2 .bu.extract
		@${ECHO} ">> Applying additional patches to binutils"
		(cd src/binutils-${BINUTILS_VER} && bzcat ../../distfiles/binutils-${BINUTILS_VER}.diff.bz2|patch -p1)
		@${TOUCH} .bu.prepatch
else
.bu.prepatch:	.bu.extract
		@${TOUCH} .bu.prepatch
endif

.bu.patch:	.bu.prepatch scripts/binutils_patch.sh
		@${ECHO} ">> Applying local patches to binutils"
		scripts/binutils_patch.sh src/binutils-${BINUTILS_VER}
		@${TOUCH} .bu.patch

.bu.configure: .bu.patch
		@${ECHO} ">> Configuring binutils"
		if [ ! -d build/binutils ]; then mkdir -p build/binutils; fi
		(cd build/binutils && ../../src/binutils-${BINUTILS_VER}/configure --target=${TARGET} --prefix=${PREFIX})
		@# hack for old binutils that misdetect makeinfo >=4.11
		${SED} -r 's#^MAKEINFO = .*missing makeinfo#MAKEINFO = /usr/bin/makeinfo#' < build/binutils/Makefile > build/binutils/Makefile.tmp
		mv build/binutils/Makefile.tmp build/binutils/Makefile
		@${TOUCH} .bu.configure

.bu.build:	.bu.configure
		@${ECHO} ">> Building binutils"
		(cd build/binutils && make ${MAKE_BUILDARG})
		@${TOUCH} .bu.build

.bu.install:	.bu.build
		@${ECHO} ">> Installing binutils (in ${PREFIX})"
		(cd build/binutils && make install)
		@${TOUCH} .bu.install

.bu.includes:	.bu.install
		@${ECHO} ">> Setting up include symlink (in ${PREFIX})"
		@# XXX gmake-ism
		ln -sf $(realpath ../include) ${PREFIX}
		@${TOUCH} .bu.includes

# GCC steps
.gcc.extract:	distfiles/gcc-${GCC_VER}.tar.bz2
		@${ECHO} ">> Extracting gcc-${GCC_VER}.tar.bz2..."
		if [ ! -d src ]; then mkdir src; fi
		${TAR} xf distfiles/gcc-${GCC_VER}.tar.bz2 -C src
		@${TOUCH} .gcc.extract

ifdef GCC_PATCH
distfiles/gcc-${GCC_VER}.diff.bz2:
		@${ECHO} ">> Downloading gcc-${GCC_VER}.diff.bz2"
		if [ ! -d distfiles ]; then mkdir distfiles; fi
		(cd distfiles && ${DOWNLOAD} -O gcc-${GCC_VER}.diff.bz2 ${GCC_PATCH})

.gcc.prepatch:	distfiles/gcc-${GCC_VER}.diff.bz2 .gcc.extract
		@${ECHO} ">> Applying additional patches to gcc"
		(cd src/gcc-${GCC_VER} && bzcat ../../distfiles/gcc-${GCC_VER}.diff.bz2|patch -p1)
		@${TOUCH} .gcc.prepatch
else
.gcc.prepatch:	.gcc.extract
		@${TOUCH} .gcc.prepatch
endif

distfiles/gcc-${GCC_VER}.diff:
		@${ECHO} ">> Downloading additional gcc patch"
		if [ ! -d distfiles ]; then mkdir distfiles; fi
		(cd distfiles && ${DOWNLOAD} -O gcc-${GCC_VER}.diff ${GCC_PATCH})

.gcc.patch:	.gcc.prepatch
		@${ECHO} ">> Applying local patches to gcc"
		scripts/gcc_patch.sh src/gcc-${GCC_VER}
		@${TOUCH} .gcc.patch

.gcc.configure:	.gcc.patch
		@${ECHO} ">> Configuring gcc"
		if [ ! -d build/gcc ]; then mkdir -p build/gcc; fi
		@# XXX the '--disable-libssp' is a temporary kludge since we do
		@# not define the necessary things in stdio.h yet (it tries to
		@# open /dev/urandom and read from it)
		(cd build/gcc && ../../src/gcc-${GCC_VER}/configure --target=${TARGET} --prefix=${PREFIX} --disable-nls --enable-languages=c --without-headers --with-gmp=/usr/local --with-mpfr=/usr/local --disable-libssp)
		@${TOUCH} .gcc.configure

.gcc.build:	.gcc.configure
		@${ECHO} ">> Building gcc (this takes some time)"
		(cd build/gcc && make ${MAKE_BUILDARG})
		@${TOUCH} .gcc.build

.gcc.install:	.gcc.build
		@${ECHO} ">> Installing gcc (in ${PREFIX})"
		(cd build/gcc && make install)
		@${TOUCH} .gcc.install

# GDB steps
.gdb.extract:	distfiles/gdb-${GDB_VER}.tar.bz2
		@${ECHO} ">> Extracting gdb-${GDB_VER}.tar.bz2..."
		if [ ! -d src ]; then mkdir src; fi
		${TAR} xf distfiles/gdb-${GDB_VER}.tar.bz2 -C src
		@${TOUCH} .gdb.extract

.gdb.patch:	.gdb.extract
		@${ECHO} ">> Apply local patches to gdb"
		scripts/gdb_patch.sh src/gdb-${GDB_VER}
		@${TOUCH} .gdb.patch

.gdb.configure:	.gdb.patch
		@${ECHO} ">> Configuring gdb"
		if [ ! -d build/gdb ]; then mkdir -p build/gdb; fi
		(cd build/gdb && ../../src/gdb-${GDB_VER}/configure --target=${TARGET} --prefix=${PREFIX})
		@${TOUCH} .gdb.configure

.gdb.build:	.gdb.configure
		@${ECHO} ">> Building gdb"
		(cd build/gdb && make ${MAKE_BUILDARG})
		@${TOUCH} .gdb.build

.gdb.install:	.gdb.build
		@${ECHO} ">> Installing gdb (in ${PREFIX})"
		(cd build/gdb && make install)
		@${TOUCH} .gdb.install

# downloading of necessary distfiles
distfiles/binutils-${BINUTILS_VER}.tar.bz2:
		@${ECHO} ">> Downloading binutils-${BINUTILS_VER}.tar.bz2"
		if [ ! -d distfiles ]; then mkdir distfiles; fi
		(cd distfiles && ${DOWNLOAD} ${MIRROR}/binutils/binutils-${BINUTILS_VER}.tar.bz2)

distfiles/gcc-${GCC_VER}.tar.bz2:
		@${ECHO} ">> Downloading gcc-${GCC_VER}.tar.bz2"
		if [ ! -d distfiles ]; then mkdir distfiles; fi
		(cd distfiles && ${DOWNLOAD} ${MIRROR}/gcc/gcc-${GCC_VER}/gcc-${GCC_VER}.tar.bz2)

distfiles/gdb-${GDB_VER}.tar.bz2:
		@${ECHO} ">> Downloading gdb-${GDB_VER}.tar.bz2"
		if [ ! -d distfiles ]; then mkdir distfiles; fi
		(cd distfiles && ${DOWNLOAD} ${MIRROR}/gdb/gdb-${GDB_VER}.tar.bz2)

clean:
		rm -rf prefix build
		rm -f .bu.configure .bu.build .bu.install .bu.includes
		rm -f .gcc.configure .gcc.build .gcc.install

reallyclean:	clean
		rm -rf src
		rm -f .bu.extract .bu.patch
		rm -f .gcc.extract .gcc.patch
