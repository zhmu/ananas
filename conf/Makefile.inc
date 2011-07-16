ifeq '${TARGET}' 'avr32-elf-ananas'
# AVR32 port uses old binutils/gcc versions
BINUTILS_VER=	2.18
GCC_VER=	4.2.2
else
# everything else is supported by mainline binutils/gcc
BINUTILS_VER=	2.21
GCC_VER=	4.6.1
endif
GDB_VER=	7.2
