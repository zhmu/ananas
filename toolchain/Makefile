# ARCH can be any of {i386,amd64,powerpc,avr32}
ARCH?=		i386
PREFIX?=	$(realpath prefix.${ARCH})
TARGET=		${ARCH}-elf-ananas

# determine binutils/gcc/gdb versions to use and extra patches
include		../conf/Makefile.inc
ifeq '${TARGET}' 'avr32-elf-ananas'
# AVR32 port needs specific patches
BINUTILS_PATCH=	http://avr32linux.org/twiki/pub/Main/BinutilsPatches/binutils-2.18.atmel.1.0.1.patch.bz2
GCC_PATCH=	http://avr32linux.org/twiki/pub/Main/GccPatches/gcc-4.2.2.atmel.1.1.3.patch.bz2
endif

# for i386, enable the EFI target as well
ifeq '${TARGET}' 'i386-elf-ananas'
BINUTILS_CONFIG_EXTRA=	--enable-targets=i386-efi-pe
endif

# environment settings
MIRROR?=	http://ftp.gnu.org/gnu
MAKE_BUILDARG?=	-j8

# tools to use
DOWNLOAD?=	wget
TAR?=		tar
TOUCH?=		touch
ECHO?=		echo
SED?=		sed

TARGET:		gcc

binutils:	pre-everything .bu.${BINUTILS_VER}.${ARCH}.includes
gcc:		binutils .gcc.${GCC_VER}.extract .gcc.${GCC_VER}.patch .gcc.${GCC_VER}.${ARCH}.configure .gcc.${GCC_VER}.${ARCH}.build .gcc.${GCC_VER}.${ARCH}.install
gdb:		.gdb.${GDB_VER}.${ARCH}.install

#
# This kludge exists because gmake will have processed $(realpath ...) before
# the directory is created, causing it to be empty. Force the user to create
# the prefix directory before we continue.
#
pre-everything:
		@if [ "x${PREFIX}" = "x" ]; then echo "*** prefix directory does not exist - please create"; false; fi

# binutils steps
.bu.${BINUTILS_VER}.extract:	distfiles/binutils-${BINUTILS_VER}.tar.bz2
		@${ECHO} ">> Extracting binutils-${BINUTILS_VER}.tar.bz2..."
		if [ ! -d src ]; then mkdir src; fi
		${TAR} xf distfiles/binutils-${BINUTILS_VER}.tar.bz2 -C src
		@${TOUCH} .bu.${BINUTILS_VER}.extract

ifdef BINUTILS_PATCH
distfiles/binutils-${BINUTILS_VER}.diff.bz2:
		@${ECHO} ">> Downloading binutils-${BINUTILS_VER}.diff.bz2"
		if [ ! -d distfiles ]; then mkdir distfiles; fi
		(cd distfiles && ${DOWNLOAD} -O binutils-${BINUTILS_VER}.diff.bz2 ${BINUTILS_PATCH})

.bu.${BINUTILS_VER}.prepatch:	distfiles/binutils-${BINUTILS_VER}.diff.bz2 .bu.${BINUTILS_VER}.extract
		@${ECHO} ">> Applying additional patches to binutils"
		(cd src/binutils-${BINUTILS_VER} && bzcat ../../distfiles/binutils-${BINUTILS_VER}.diff.bz2|patch -p1)
		@${TOUCH} .bu.${BINUTILS_VER} prepatch
else
.bu.${BINUTILS_VER}.prepatch:	.bu.${BINUTILS_VER}.extract
		@${TOUCH} .bu.${BINUTILS_VER}.prepatch
endif

.bu.${BINUTILS_VER}.patch:	.bu.${BINUTILS_VER}.prepatch scripts/binutils_patch.sh
		@${ECHO} ">> Applying local patches to binutils"
		SED=${SED} scripts/binutils_patch.sh src/binutils-${BINUTILS_VER}
		@${TOUCH} .bu.${BINUTILS_VER}.patch

.bu.${BINUTILS_VER}.${ARCH}.configure: .bu.${BINUTILS_VER}.patch
		@${ECHO} ">> Configuring binutils (${ARCH})"
		if [ ! -d build/binutils.${BINUTILS_VER}.${ARCH} ]; then mkdir -p build/binutils.${BINUTILS_VER}.${ARCH}; fi
		(cd build/binutils.${BINUTILS_VER}.${ARCH} && ../../src/binutils-${BINUTILS_VER}/configure --target=${TARGET} --prefix=${PREFIX} ${BINUTILS_CONFIG_EXTRA})
		@# hack for old binutils that misdetect makeinfo >=4.11
		${SED} -r 's#^MAKEINFO = .*missing makeinfo#MAKEINFO = /usr/bin/makeinfo#' < build/binutils.${BINUTILS_VER}.${ARCH}/Makefile > build/binutils.${BINUTILS_VER}.${ARCH}/Makefile.tmp
		mv build/binutils.${BINUTILS_VER}.${ARCH}/Makefile.tmp build/binutils.${BINUTILS_VER}.${ARCH}/Makefile
		@${TOUCH} .bu.${BINUTILS_VER}.${ARCH}.configure

.bu.${BINUTILS_VER}.${ARCH}.build:	.bu.${BINUTILS_VER}.${ARCH}.configure
		@${ECHO} ">> Building binutils (${ARCH})"
		(cd build/binutils.${BINUTILS_VER}.${ARCH} && ${MAKE} ${MAKE_BUILDARG})
		@${TOUCH} .bu.${BINUTILS_VER}.${ARCH}.build

.bu.${BINUTILS_VER}.${ARCH}.install:	.bu.${BINUTILS_VER}.${ARCH}.build
		@${ECHO} ">> Installing binutils (in ${PREFIX})"
		(cd build/binutils.${BINUTILS_VER}.${ARCH} && ${MAKE} install)
		@${TOUCH} .bu.${BINUTILS_VER}.${ARCH}.install

.bu.${BINUTILS_VER}.${ARCH}.includes:	.bu.${BINUTILS_VER}.${ARCH}.install
		@${ECHO} ">> Setting up include symlink (in ${PREFIX})"
		@# XXX gmake-ism
		ln -sf $(realpath ../include) ${PREFIX}
		@${TOUCH} .bu.${BINUTILS_VER}.${ARCH}.includes

# GCC steps
.gcc.${GCC_VER}.extract:	distfiles/gcc-${GCC_VER}.tar.bz2
		@${ECHO} ">> Extracting gcc-${GCC_VER}.tar.bz2..."
		if [ ! -d src ]; then mkdir src; fi
		${TAR} xf distfiles/gcc-${GCC_VER}.tar.bz2 -C src
		@${TOUCH} .gcc.${GCC_VER}.extract

ifdef GCC_PATCH
distfiles/gcc-${GCC_VER}.diff.bz2:
		@${ECHO} ">> Downloading gcc-${GCC_VER}.diff.bz2"
		if [ ! -d distfiles ]; then mkdir distfiles; fi
		(cd distfiles && ${DOWNLOAD} -O gcc-${GCC_VER}.diff.bz2 ${GCC_PATCH})

.gcc.${GCC_VER}.prepatch:	distfiles/gcc-${GCC_VER}.diff.bz2 .gcc.${GCC_VER}.extract
		@${ECHO} ">> Applying additional patches to gcc"
		(cd src/gcc-${GCC_VER} && bzcat ../../distfiles/gcc-${GCC_VER}.diff.bz2|patch -p1)
		@${TOUCH} .gcc.${GCC_VER}.prepatch
else
.gcc.${GCC_VER}.prepatch:	.gcc.${GCC_VER}.extract
		@${TOUCH} .gcc.${GCC_VER}.prepatch
endif

distfiles/gcc-${GCC_VER}.diff:
		@${ECHO} ">> Downloading additional gcc patch"
		if [ ! -d distfiles ]; then mkdir distfiles; fi
		(cd distfiles && ${DOWNLOAD} -O gcc-${GCC_VER}.diff ${GCC_PATCH})

.gcc.${GCC_VER}.patch:	.gcc.${GCC_VER}.prepatch
		@${ECHO} ">> Applying local patches to gcc"
		SED=${SED} scripts/gcc_patch.sh src/gcc-${GCC_VER}
		@${TOUCH} .gcc.${GCC_VER}.patch

.gcc.${GCC_VER}.${ARCH}.configure:	.gcc.${GCC_VER}.patch
		@${ECHO} ">> Configuring gcc (${ARCH})"
		if [ ! -d build/gcc.${GCC_VER}.${ARCH} ]; then mkdir -p build/gcc.${GCC_VER}.${ARCH}; fi
		@# XXX the '--disable-libssp' is a temporary kludge since we do
		@# not define the necessary things in stdio.h yet (it tries to
		@# open /dev/urandom and read from it)
		(cd build/gcc.${GCC_VER}.${ARCH} && ../../src/gcc-${GCC_VER}/configure --target=${TARGET} --prefix=${PREFIX} --disable-nls --enable-languages=c --without-headers --with-gmp=/usr/local --with-mpfr=/usr/local --with-mpc=/usr/local --disable-libssp --disable-libquadmath)
		@${TOUCH} .gcc.${GCC_VER}.${ARCH}.configure

.gcc.${GCC_VER}.${ARCH}.build:	.gcc.${GCC_VER}.${ARCH}.configure
		@${ECHO} ">> Building gcc (${ARCH}; this takes some time)"
		(cd build/gcc.${GCC_VER}.${ARCH} && ${MAKE} ${MAKE_BUILDARG})
		@${TOUCH} .gcc.${GCC_VER}.${ARCH}.build

.gcc.${GCC_VER}.${ARCH}.install:	.gcc.${GCC_VER}.${ARCH}.build
		@${ECHO} ">> Installing gcc (in ${PREFIX})"
		(cd build/gcc.${GCC_VER}.${ARCH} && ${MAKE} install)
		@${TOUCH} .gcc.${GCC_VER}.${ARCH}.install

# GDB steps
.gdb.${GDB_VER}.extract:	distfiles/gdb-${GDB_VER}.tar.bz2
		@${ECHO} ">> Extracting gdb-${GDB_VER}.tar.bz2..."
		if [ ! -d src ]; then mkdir src; fi
		${TAR} xf distfiles/gdb-${GDB_VER}.tar.bz2 -C src
		@${TOUCH} .gdb.${GDB_VER}.extract

.gdb.${GDB_VER}.patch:	.gdb.${GDB_VER}.extract
		@${ECHO} ">> Apply local patches to gdb"
		scripts/gdb_patch.sh src/gdb-${GDB_VER}
		@${TOUCH} .gdb.${GDB_VER}.patch

.gdb.${GDB_VER}.${ARCH}.configure:	.gdb.${GDB_VER}.patch
		@${ECHO} ">> Configuring gdb (${ARCH})"
		if [ ! -d build/gdb.${GDB_VER}.${ARCH} ]; then mkdir -p build/gdb.${GDB_VER}.${ARCH}; fi
		(cd build/gdb.${GDB_VER}.${ARCH} && ../../src/gdb-${GDB_VER}/configure --target=${TARGET} --prefix=${PREFIX})
		@${TOUCH} .gdb.${GDB_VER}.${ARCH}.configure

.gdb.${GDB_VER}.${ARCH}.build:	.gdb.${GDB_VER}.${ARCH}.configure
		@${ECHO} ">> Building gdb (${ARCH})"
		(cd build/gdb.${GDB_VER}.${ARCH} && ${MAKE} ${MAKE_BUILDARG})
		@${TOUCH} .gdb.${GDB_VER}.${ARCH}.build

.gdb.${GDB_VER}.${ARCH}.install:	.gdb.${GDB_VER}.${ARCH}.build
		@${ECHO} ">> Installing gdb (in ${PREFIX})"
		(cd build/gdb.${GDB_VER}.${ARCH} && ${MAKE} install)
		@${TOUCH} .gdb.${GDB_VER}.${ARCH}.install

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
		rm -f .bu.${BINUTILS_VER}.${ARCH}.configure .bu.${BINUTILS_VER}.${ARCH}.build .bu.${BINUTILS_VER}.${ARCH}.install .bu.${BINUTILS_VER}.${ARCH}.includes
		rm -f .gcc.${GCC_VER}.${ARCH}.configure .gcc.${GCC_VER}.${ARCH}.build .gcc.${GCC_VER}.${ARCH}.install

reallyclean:	clean
		rm -rf src
		rm -f .bu.${BINUTILS_VER}.extract .bu.${BINUTILS_VER}.patch .bu.${BINUTILS_VER}.prepatch
		rm -f .gcc.${GCC_VER}.extract .gcc.${GCC_VER}.patch .gcc.${GCC_VER}.prepatch
