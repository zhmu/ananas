#!/usr/bin/perl -w

use Data::Dumper;

our $NM = "nm";

sub lookup_sym {
	my ($addr) = @_;
	our @syms;

	my $cur_sym = undef;
	my $cur_addr = undef;
	foreach (@syms) {
		my ($sym_addr, $sym_name) = @{ $_ };
		last if $sym_addr > $addr;
		$cur_sym = $sym_name;
		$cur_addr = $sym_addr;
	}
	return sprintf("0x%x", $addr) unless $cur_addr;
	return $cur_addr ne $addr ? sprintf("%s+0x%x", $cur_sym, $addr - $cur_addr) : $cur_sym;
}

my ($fname) = @ARGV;
$fname = "kernel" unless $fname;

my %syms;
foreach (split(/\n/, `$NM $fname`)) {
	die unless /^([a-z0-9]*)\s+.\s+(.+)$/;
	next unless $1;
	$syms{hex($1)} = $2;

}

our @syms;
foreach (sort keys %syms) {
	push @syms, [ $_, $syms{$_} ];
}


while(<>) {
	next unless /^\((\d+)\) 0x([a-f0-9]+)$/;
	printf "(%s) %s\n", $1, &lookup_sym(hex($2));
}
#print &lookup_sym(hex("0xc0106a9f"));
