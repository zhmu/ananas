#!/bin/sh

set -e

T=$1
if [ -z "$T" ]; then
	echo "usage: $0 path_to_gcc"
	exit 1
fi

# patch 'config.sub'
${SED} -r 's/(\-aos\*) \\$/\1 \| -ananas\* \\/' < $T/config.sub > $T/config.sub.tmp # gcc 4.4+
${SED} -r 's/(\-aros\*) \\$/\1 \| -ananas\* \\/' < $T/config.sub.tmp > $T/config.sub # gcc 4.2

# patch 'gcc/config.gcc'
# XXX we do not override powerpc-*-elf yet (should we?)
# XXX we do not override arm-*-elf yet (should we?)
# XXX is using the linux tmake fiels for avr32 ok?
awk '{ print }
/# Common parts for widely ported systems/ { STATE = 1 }
/case \${target} in/ && STATE == 0 { STATE = 2 }
/case \${target} in/ && STATE == 1 {
	print "*-*-ananas*)"
        print "  extra_parts=\"crtbegin.o crtend.o\""
        print "	  gas=yes"
        print "	  gnu_ld=yes"
        print "	  ;;"
	STATE = 0
}
/# Support site-specific machine types./ && STATE == 2 {
	print "i[3-7]86-*-ananas*)"
        print "\ttm_file=\"${tm_file} i386/unix.h i386/att.h dbxelf.h elfos.h i386/i386elf.h ananas.h\""
        print "\ttmake_file=\"i386/t-i386elf i386/t-crtstuff\""
        print "\t;;"
	print "x86_64-*-ananas*)"
        print "\ttm_file=\"${tm_file} i386/unix.h i386/att.h dbxelf.h elfos.h ananas.h\""
        print "\ttmake_file=\"i386/t-crtstuff\""
        print "\t;;"
	print "powerpc-*-ananas*)"
	print "\ttm_file=\"${tm_file} dbxelf.h elfos.h freebsd-spec.h rs6000/sysv4.h ananas.h\""
	print "\textra_options=\"${extra_options} rs6000/sysv4.opt\""
	print "\ttmake_file=\"rs6000/t-fprules rs6000/t-fprules-fpbit rs6000/t-ppcos ${tmake_file} rs6000/t-ppccomm\""
        print "\t;;"
	print "avr32-*-ananas*)"
	print "\ttm_file=\"dbxelf.h elfos.h ananas.h ananas-avr32.h avr32/avr32.h\""
	print "\ttmake_file=\"t-linux avr32/t-avr32 avr32/t-elf\""
        print "\textra_modes=avr32/avr32-modes.def"
        print "\tgnu_ld=yes"
	print "\t;;"
	print "arm-*-ananas*)"
	print "\ttm_file=\"dbxelf.h elfos.h arm/elf.h arm/aout.h arm/arm.h ananas.h\""
	print "\ttmake_file=\"${tmake_file} t-linux arm/t-arm\""
	print "\t;;"
	STATE = 0
}
' < $T/gcc/config.gcc > $T/gcc/config.gcc.new
mv $T/gcc/config.gcc.new $T/gcc/config.gcc

# generate 'gcc/config/ananas.h'
echo '#undef TARGET_OS_CPP_BUILTINS
#define TARGET_OS_CPP_BUILTINS()      \
  do {                                \
    builtin_define_std ("Ananas");    \
    builtin_define_std ("unix");      \
    builtin_assert ("system=Ananas"); \
    builtin_assert ("system=unix");   \
  } while(0);

#undef TARGET_VERSION
#define TARGET_VERSION fprintf(stderr, " (ananas)");

/*
 * STARTFILE_SPEC is used to determine which file is used as startup
 * object while linking.
 */
#undef STARTFILE_SPEC
#define STARTFILE_SPEC "		\
%{!shared:crt1.o%s}			\
crtbegin.o%s				\
"

#undef ENDFILE_SPEC
#define ENDFILE_SPEC "			\
crtend.o%s crtn.o%s			\
"
' > $T/gcc/config/ananas.h

# generate 'gcc/config/ananas-avr32.h'
echo '#undef TARGET_CPU_CPP_BUILTINS
#define TARGET_CPU_CPP_BUILTINS()        \
  do {                                   \
    builtin_define_std ("__avr32__");    \
    builtin_define_std ("__AVR32__");    \
    builtin_define (avr32_part->macro);  \
    builtin_define (avr32_arch->macro);  \
  } while(0);
' > $T/gcc/config/ananas-avr32.h

# patch 'libgcc/config.host' - note: not for gcc 4.2
if [ -f $T/libgcc/config.host ]; then
	awk '
	{ print }
	/case \${host} in/ { STATE = 1 }
	/# Support site-specific machine types./ && STATE == 1 {
		print "i[3-7]86-*-ananas*)"
		print "\t;;"
		print "x86_64-*-ananas*)"
		print "\t;;"
		print "powerpc-*-ananas*)"
		print "\t;;"
		print "arm-*-ananas*)"
		print "\t;;"
		STATE = 0
	}' < $T/libgcc/config.host > $T/libgcc/config.host.new
	mv $T/libgcc/config.host.new $T/libgcc/config.host
fi
