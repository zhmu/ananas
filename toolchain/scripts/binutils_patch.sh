#!/bin/sh

T=$1
if [ "x$T" == "x" ]; then
	echo "usage: $0 path_to_binutils"
	exit 1
fi

# patch 'config.sub'
sed -r 's/(\-aros\*) \\$/\1 \| -ananas\* \\/' < $T/config.sub > $T/config.sub.new
mv $T/config.sub.new $T/config.sub

# patch 'bfd/config.bfd file'
awk '{ print }
/# START OF targmatch.h/ {
	print "  i[3-7]86-*-ananas*)"
	print "    targ_defvec=bfd_elf32_i386_vec"
	print "    targ64_selvecs=bfd_elf64_x86_64_vec"
	print "    ;;"
}' < $T/bfd/config.bfd > $T/bfd/config.bfd.new
mv $T/bfd/config.bfd.new $T/bfd/config.bfd

# patch 'gas/configure.tgt'. note that we need 'i386' here due to
# name mangling done in the script (wants to treat amd64/i386 identical)
awk '{ print }
/i386-ibm-aix\*)/ {
	print "  i386-*-ananas)\t\t\tfmt=elf ;;"
}' < $T/gas/configure.tgt > $T/gas/configure.tgt.new
mv $T/gas/configure.tgt.new $T/gas/configure.tgt

# patch 'ld/configure.tgt'
awk '{print }
/^case "\$\{targ\}" in$/ {
	print "i[3-7]86-*-ananas*)\t\ttarg_emul=ananas_i386 ;;"
}' < $T/ld/configure.tgt > $T/ld/configure.tgt.new
mv $T/ld/configure.tgt.new $T/ld/configure.tgt

# create 'ld/emulparams/ananas_i386.sh'
echo 'SCRIPT_NAME=elf
OUTPUT_FORMAT="elf32-i386"
TEXT_START_ADDR=0x08000000
MAXPAGESIZE="CONSTANT (MAXPAGESIZE)"
COMMONPAGESIZE="CONSTANT (COMMONPAGESIZE)"
ARCH=i386
MACHINE=
NOP=0x90909090
TEMPLATE_NAME=elf32
GENERATE_SHLIB_SCRIPT=yes
GENERATE_PIE_SCRIPT=yes
NO_SMALL_DATA=yes
SEPARATE_GOTPLT=12
IREL_IN_PLT=' > $T/ld/emulparams/ananas_i386.sh

# patch 'ld/Makefile.in'
awk '{print} END {
	print ""
	print "eananas_i386.c:\t$(srcdir)/emulparams/ananas_i386.sh $(ELF_DEPS) $(srcdir)/scripttempl/elf.sc ${GEN_DEPENDS}"
	print "\t${GENSCRIPTS} ananas_i386 \"$(tdir_ananas_i386)\""
}' < $T/ld/Makefile.in > $T/ld/Makefile.in.new
mv $T/ld/Makefile.in.new $T/ld/Makefile.in
