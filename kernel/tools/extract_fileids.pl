#!/usr/bin/perl -w

use strict;
use Data::Dumper;

my ($OBJDUMP, $FNAME, $OUTNAME) = @ARGV;
die "usage: extract_fileids.pl objdump in out" unless defined $OUTNAME;
our $ADDR_LEN = 4;

sub
get_section {
	my ($section, $filename) = @_; # XXX not escaped

	my $file_format = undef;
	my $state = 0;
	my $first_addr = undef;
	my $binary = "";
	foreach (split(/\n/, `$OBJDUMP -s -j '$section' '$filename'`)) {
		if ($state eq 0 and /^$filename:\s+file format (.+)$/) {
			$file_format = $1; next;
		}
		if ($state eq 0 and /^Contents of section/) {
			$state = 1; next;
		}
		next unless $state eq 1;

		die "fatal: cannot parse '$_'\n" unless /^\s+([a-f0-9]+) ([a-f0-9 ]+)  /;
		my ($addr, $hexdata) = ($1, $2);
		$first_addr = $addr unless $first_addr;
		$hexdata =~ s/ //g;
		my @chars = split(//, $hexdata);
		while ($#chars > 0) {
			my ($char) = (shift @chars) . (shift @chars);
			$binary .= chr(hex($char));
		}
	}
	return [ hex($first_addr), $file_format, $binary ];
}

sub
get_format_info {
	my ($format) = @_;

	# returns little_endian (0 or 1), address length (4 or8)
	return [ 0, 4 ] if $format =~ /-i386$/;
	die "unknown format '$format'";
}

my %names;
my ($name_addr, $name_format, $name_data) = @{ &get_section(".tracenames", $FNAME) };
my $sym_addr = $name_addr;
my $cur_name = "";
foreach (split(//, $name_data)) {
	$name_addr++;
	if ($_ eq chr(0)) {
		$names{$sym_addr} = $cur_name;
		$sym_addr = $name_addr;
		$cur_name = "";
	} else {
		$cur_name .= $_;
	}
}

my @ids;
my ($id_addr, $id_format, $id_data) = @{ &get_section(".traceids", $FNAME) };
die "file format changed?!" unless $name_format eq $id_format;
my ($little_endian, $addr_len) = @{ &get_format_info($name_format) };
my $cur_addr = "";
foreach (split(//, $id_data)) {
	my $byte = sprintf("%02x", ord($_));
	if ($little_endian) {
		$cur_addr .= $byte;
	} else  {
		$cur_addr = $byte . $cur_addr;
	}
	next unless length($cur_addr) eq $addr_len * 2;
	$cur_addr = hex($cur_addr);
	die "error: tracename not found" unless defined $names{$cur_addr};
	push @ids, $names{$cur_addr};
	$cur_addr = "";
}

open F, "+>$OUTNAME" or die "Can't create $OUTNAME: $!";
my $id = 1;
foreach my $fname (@ids)
{
	while ($fname =~ /^\.\.\//) { $fname = substr($fname, 3); }
	printf F "% 4s = %s\n", $id, $fname;
	$id++;
}
close F;
