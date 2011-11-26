#!/bin/sh

T=$1
if [ -z "$T" ]; then
	echo "usage: $0 path_to_gdb"
	exit 1
fi

# patch 'config.sub' file
awk '{ print }
/-none\)/ {
	print "\t\t;;"
	print "\t-ananas)"
	;;
}' < $T/config.sub > $T/config.sub.new
mv $T/config.sub.new $T/config.sub

# patch 'bfd/config.bfd file'
awk '{ print }
/# START OF targmatch.h/ {
	print "  i[3-7]86-*-ananas*)"
	print "    targ_defvec=bfd_elf32_i386_vec"
	print "    targ64_selvecs=bfd_elf64_x86_64_vec"
	print "    ;;"
	print "  x86_64-*-ananas*)"
	print "    targ_defvec=bfd_elf64_x86_64_vec"
	print "    targ_selvecs=bfd_elf32_i386_vec"
	print "    ;;"
	print "   powerpc-*-ananas*)"
	print "    targ_defvec=bfd_elf32_powerpc_vec"
	print "    targ_selvecs=\"bfd_elf32_powerpcle_vec ppcboot_vec\""
	print "    ;;"
	print "   arm-*-ananas*)"
	print "    targ_defvec=bfd_elf32_littlearm_vec"
	print "    targ_selvec=bfd_elf32_bigarm_vec"
	print "    ;;"
}' < $T/bfd/config.bfd > $T/bfd/config.bfd.new
mv $T/bfd/config.bfd.new $T/bfd/config.bfd
