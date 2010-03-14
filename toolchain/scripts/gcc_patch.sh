#!/bin/sh

T=$1
if [ "x$T" == "x" ]; then
	echo "usage: $0 path_to_gcc"
	exit 1
fi

# patch 'config.sub'
sed -r 's/(\-aros\*) \\$/\1 \| -ananas\* \\/' < $T/config.sub > $T/config.sub.new
mv $T/config.sub.new $T/config.sub

# patch 'gcc/config.gcc'
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
        print "\ttmake_file=\"i386/t-i386elf svr4\""
        print "\tuse_fixproto=\"yes\""
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
#define TARGET_VERSION fprintf(stderr, " (i386 ananas)");

/*
 * STARTFILE_SPEC is used to determine which file is used as startup
 * object while linking.
 */
#undef STARTFILE_SPEC
#define STARTFILE_SPEC "		\
%{!shared:crt1.o%s}			\
crtbegin.o%s				\
"
' > $T/gcc/config/ananas.h

# patch 'libgcc/config.host'
awk '
{ print }
/case \${host} in/ { STATE = 1 }
/# Support site-specific machine types./ && STATE == 1 {
	print "i[3-7]86-*-ananas*)"
	print "\t;;"
	STATE = 0
}' < $T/libgcc/config.host > $T/libgcc/config.host.new
mv $T/libgcc/config.host.new $T/libgcc/config.host
