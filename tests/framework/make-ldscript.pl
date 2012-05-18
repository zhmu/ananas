#!/usr/bin/perl -w
#
# This script parses ld(1)'s default linker script and appends the
# init/exit-function sections to the .data section.
#
use strict;

my $ldscript = "";
my $state = 0;
while(<>) {
	if (/^using internal linker script/) {
		$state++;
		next;
	}
	next if $state eq 0;
	next if /^[=]+$/;
	if ($state eq 2 && /}/) {
print <<END

                __initfuncs_begin = ALIGN(4);
                *(initfuncs)
                __initfuncs_end = .;
                __exitfuncs_begin = ALIGN(4);
                *(exitfuncs)
                __exitfuncs_end = .;
END
		; $state--;
	}
	if (/\.data\s*:/) {
		$state++;
	}
	print "$_";
}
