#!/usr/bin/perl
# -*- mode: cperl; cperl-indent-level: 4 -*-
# $ codegen.pl $
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#	Copyright (c) 2025 Tomi Ollila
#	    All rights reserved
#
# Created: Fri 07 Mar 2025 20:35:28 EET too
# Last modified: Sun 09 Mar 2025 20:25:23 +0200 too

use 5.8.1;
use strict;
use warnings;

die "Usage: $0 odir/\n" unless @ARGV == 1;

my $odir = shift;
die "'$odir': no such directory\n" unless -d $odir;

my $pbmsxz = 'num.pbms.xz';
die "'$pbmsxz': no such file\n" unless -f $pbmsxz;
die "'$pbmsxz': unreadable\n" unless -f _;

my $ofile = "$odir/numbrle.ch";

open O, '>', "$ofile.wip" or die "Cannot create '$ofile.wip': $!\n";
select O;

my @i;

my $n = 0;
open P, '-|', qw/xzcat/, $pbmsxz or die "Cannot run xzcat $pbmsxz: $!\n";
while (<P>) {
    chomp, die "'$_' not 'P4'" unless $_ eq "P4\n";
    $_ = <P>;
    chomp, die "'$_' not {w} {h}\n" unless /^(\d+)\s+(\d+)\s+$/;
    die "'$1' not 200\n" unless $1 == 200;
    #print "$1, $2\n";
    read P, $_, $1 * $2 / 8;

    my @a;
    my $aa = [ ];
    for (split //, $_) {
        my $v = ord $_;
        push @$aa, $v & 128? 1: 0;
        push @$aa, $v & 64? 1: 0;
        push @$aa, $v & 32? 1: 0;
        push @$aa, $v & 16? 1: 0;
        push @$aa, $v & 8? 1: 0;
        push @$aa, $v & 4? 1: 0;
        push @$aa, $v & 2? 1: 0;
        push @$aa, $v & 1? 1: 0;
        if (@$aa == 200) {
            push @a, $aa;
            $aa = [];
        }
    }
    die if @$aa;
    my ($bt, $bb) = (-1, 0);
    my $r = 0;
    for (@a) {
        my $l = $_;
        for (@$l) {
            #print $_;
            if ($_) {
                $bb = $r;
                $bt = $r if $bt < 0;
            }
        }
        $r++;
        #print "\n";
    }
    $bb++;
    my $h = $bb - $bt;
    print "// $n t: $bt, b: $bb (h $h)\n";

    splice @a, $bb; # drop trailing "empty"
    #splice @a, 12;
    splice @a, 0, $bt; # drop leading "empty"
    my $ep = 1;
    my @rle = ( );
    my $c = 0;
    for $aa (@a) {
	#print "$aa->[0], $ep\n"; #; exit;
	for (@{$aa}) {
	    if ($_ == $ep) {
		#print "\n";
		push @rle, $c;
		$c = 0;
		$ep = !$ep;
	    }
	    $c++;
	    #print $_;
	}
	#print "\n"; #last; exit 0;
    }
    push @rle, $c;
    push @rle, 0 while (scalar @rle & 3);
    #print scalar @rle, ": @rle\n";

    my $l = scalar @rle / 4;
    push @i, [ $bt, 267 - $bb, $l ];

    print 'static uint32_t _fd_rle', $n, '[', $l, '] = {';
    my $pfx = "\n  ";
    $c = 0;
    while (@rle) {
	my $i = $rle[0] + ($rle[1] << 8) + ($rle[2] << 16) + ($rle[3] << 24);
	printf "${pfx}0x%08x", $i;
	$pfx = (++$c % 6) == 0? ",\n  ": ', ';
	splice @rle, 0, 4;
    }
    print "\n};\n\n";
    $n++;
}
close P;

print "uint32_t * gp[10] = {\n ";
print " _fd_rle$_," for(0..4);
print "\n ";
print " _fd_rle$_," for(5..8);
print " _fd_rle9\n};\n\n";

print "struct { unsigned char t; unsigned char b; short c; } gw[10] = {";

my $pfx = "\n";
for (@i) {
    my $t = $_->[0] - 2;
    die "internal error: $t\n" unless $t >= 0;
    print "$pfx  { $t, $_->[1], $_->[2] }";
    $pfx = ",\n";
}
print "\n};\n\n";

select STDOUT;
rename "$ofile.wip", $ofile or
  die "Cannot rename '$ofile.wip' to '$ofile': $!\n";

print "Wrote '$ofile'\n";
