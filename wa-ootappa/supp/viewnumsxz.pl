#!/usr/bin/perl
# -*- mode: cperl; cperl-indent-level: 4 -*-

use 5.8.1;
use strict;
use warnings;

die "Usage: $0 zoom-level (1-9)\n" unless @ARGV == 1;

my $z = $ARGV[0] * 100;

my $pbmsxz = 'num.pbms.xz';
die "'$pbmsxz': no such file\n" unless -f $pbmsxz;
die "'$pbmsxz': unreadable\n" unless -f _;

END { unlink </tmp/xxzx.?.$$.pbm> };

my $n = 0;
open P, '-|', qw/xzcat/, $pbmsxz or die "Cannot run xzcat $pbmsxz: $!\n";
while (<P>) {
    chomp, die "'$_' not 'P4'" unless $_ eq "P4\n";
    my $l1 = $_;
    $_ = <P>;
    chomp, die "'$_' not {w} {h}\n" unless /^(\d+)\s+(\d+)\s+$/;
    die "'$1' not 200\n" unless $1 == 200;
    my $l2 = $_;
    #print "$1, $2\n";
    read P, $_, $1 * $2 / 8;

    open O, '>', "/tmp/xxzx.$n.$$.pbm" or die;
    print O $l1, $l2, $_;
    close O or die;
    $n++;
}

system qw/feh --force-aliasing --zoom/, $z, </tmp/xxzx.?.$$.pbm>;
