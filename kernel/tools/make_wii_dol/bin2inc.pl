#!/usr/bin/perl -w

use strict;

die "usage: bin2inc.pl source identifier dest.inc" unless $#ARGV eq 2;

open my $FILE, "<" . $ARGV[0] or die "Can't open file: $!";
my $data;
my $len = sysread $FILE, $data, 1000;
close $FILE;

open F, "+>" . $ARGV[2] or die "Can't create file: $!";

printf F "unsigned char %s[%s] = {", $ARGV[1], $len;
my @char = split(//, $data);
for (my $i = 0; $i < $len; $i++) {
	printf F "\n\t" if $i % 8 == 0;
	printf F "0x%02x, ", ord($char[$i]);
}
printf F "\n};\n";
close F;
