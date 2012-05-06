#!/usr/bin/perl -w

my %space;
use strict;
while(<>) {
	chomp; s/^\s+//; s/\s+$//;
	next unless /^\d+: @ [a-f0-9]+, (.+):(\d+), (\d+) KB$/;
	my ($file, $line, $amount) = ($1, $2, $3);
	$file =~ s/^(\.\.\/)*//;
	my $id = "$file:$line";
	$space{$id} = 0 unless $space{$id};
	$space{$id} += $amount;
}

my $total = 0;
foreach (sort { $space{$b} <=> $space{$a} } keys %space) {
	printf "%s\t%s\n", $space{$_}, $_;
	$total += $space{$_};
}

print "total $total KB\n";
