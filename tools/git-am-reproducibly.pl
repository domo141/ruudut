#!/usr/bin/perl
# -*- mode: cperl; cperl-indent-level: 4 -*-
# $ git-am-reproducibly.pl $
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#	Copyright (c) 2025 Tomi Ollila
#	    All rights reserved
#
# Created: Mon 06 Jan 2025 21:08:27 EET too
# Last modified: Thu 01 May 2025 20:31:55 +0300 too

# lisää toimintaselitys...

use 5.8.1;
use strict;
use warnings;

use Time::Local qw/timegm/;

# for simplicity, no command line options (other than patch filenames)...

for (@ARGV) {
    die "'$_' begins with '-'\n" if /^-/;
    die "'$_': no such file\n" unless -f $_
}

# *usually* committer date is later (or same) than author date...
$_ = qx/git log -1 --abbrev=4 --format=%ct,%at,%h/; #print $_;

die unless /(\d+),(\d+),(\w+)/;
my $rsecs = (int (($1 >= $2? $1: $2) / 60) + 1) * 60;
my $hp = substr $3, 0, 4;

my %M = qw/Jan 0 Feb 1 Mar 2 Apr 3 May 4 Jun 5
	   Jul 6 Aug 7 Sep 8 Oct 9 Nov 10 Dec 11/;

$ENV{GIT_COMMITTER_EMAIL} = '<is@not.invalid>';

my $c = 0;
for my $f (@ARGV) {
    open I, '<', $f or die "Cannot open $f: $!\n";
    my $ts;
    my $tz = '';
    while (<I>) {
	# Date: Sat, 20 Jan 2024 19:27:57 +0200 or like
	if (/^Date:\s+.*?(\d+)\s+(\w+)\s+(\d{4})\s+
	     (\d\d):(\d\d):(\d\d)\s+(.)(\d\d)(\d\d)/x) {
	    $ts = timegm($6, $5, $4, $1, $M{$2}, $3 - 1900);
	    my $ds = $8 * 3600 + $9 * 60;
	    $ds = -$ds if $7 eq '+';
	    $ts += $ds;
	    $tz = $7.$8.$9;
	    last
	}
	last if /^\s*$/; # empty line should be enough here any whitespace...
    }
    close I;
    #print $ts, "\n";
    die "Cannot find Date: header in $f\n" unless $tz;
    $rsecs += 1;
    $rsecs = $ts if $ts > $rsecs;
    $ENV{GIT_COMMITTER_DATE} = "$rsecs $tz";
    $ENV{GIT_COMMITTER_NAME} = sprintf '%s+%04d', $hp, ++$c;
    die if system qw/git am -3/, $f;
}
