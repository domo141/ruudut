#!/usr/bin/perl
# -*- mode: cperl; cperl-indent-level: 4 -*-
# $ mk-txtimgs.pl $
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#	Copyright (c) 2024 Tomi Ollila
#	    All rights reserved
#
# Created: Mon 01 Apr 2024 15:37:37 EEST too
# Last modified: Sun 23 Mar 2025 18:22:11 +0200 too

use 5.8.1;
use strict;
use warnings;
use POSIX qw/strftime setlocale LC_ALL/;

$0 =~ s:.*/::,
warn "\nUsage: $0 odir/ (locale|-|mon,...,sun) fontsize hadj wl fontname [O]

 odir: root dir where to write image;  './' bases to current dir

 '-' as locale:  use current locale for weekdaynames
  la_TE.UTF-8:   use given locale for weekdaynames (la_te, la.te... works, too)
 and mon,...sun: use given weekdaynames verbatim

 font size:

 hadj: 0/+extra/-less pixel lines below images

 week-letter: a letter (well, anything), shown before ISO week number

 fontname: 'monospace:slant=italic', 'hack:weight=bold', or so on...

 [O]: if defined use O instead of 0 (hack...)\n
" unless (@ARGV == 6 or @ARGV == 7);

my @cmds = qw/fc-match hb-view magick/; # 'convert(1)' is old 'magick(1)' :/
foreach (split /:/, $ENV{PATH}) {
    my $d = $_;
    foreach my $i (reverse 0 .. $#cmds) {
	splice @cmds, $i, 1 if -x "$d/$cmds[$i]" # pop elem, stackoverfl...hint
    }
    last unless @cmds
}
die "Required commands missing: '" . join("', '", @cmds) .
  "'\n(fontconfig/harfbuzz/imagemagick).\nInstall, then try again.\n" if @cmds;

exit 1 unless (@ARGV == 6 or @ARGV == 7);

my $o0 = defined $ARGV[6]? 'O': '0';

my $odir = shift;
die "Add trailing '/' to odir '$odir'\n" unless $odir =~ s:/+$::;

# based on https://www.perlmonks.org/?node_id=997760
sub xcrc16($)
{
    my $crc = 0;
    for (unpack 'C*', $_[0]) {
	$crc ^= $_;
	for (0..7) {
	    my $carry = $crc & 1;
	    $crc >>= 1;
	    $crc ^= 0x8408 if $carry;
	}
    }
    return $crc;
}

$_ = shift;
my $dd = sprintf '%04x', xcrc16 $_;

my @weekdays;

if (/^(.*?),(.*?),(.*?),(.*?),(.*?),(.*?),(.*?)$/) {
    @weekdays = ($1, $2, $3, $4, $5, $6, $7);
} elsif ($_ ne '-') {
    die "Commas (,) in $_, but not mon,...sun\n" if /,.*,/;
    $_ = lc($1).'_'.uc($2).'.UTF-8' if /^([a-z][a-z])[\W_]([a-z][a-z])$/i;
    setlocale(LC_ALL, $_)
}

my $fhgt = shift;
$fhgt =~ /^\d+$/ or die "'$fhgt' not all numeric";

my $txhx = shift; # hadj...

die "'$txhx': not '0' nor [+-][1-9]\n"
  unless $txhx eq '0' or $txhx =~ /^[+-]\d$/;
eval "\$txhx = $txhx";

my $wekl = shift;
my $fona = shift;

chdir $odir || die "'$odir': no such directory";

$dd = "images-$dd-$fhgt-$txhx-$wekl-$fona-$o0.d";

die "directory '$odir/$dd' exists\n",
  "(symlink 'txts.ch' manually or remove/rename...)\n" if -e $dd;

open I, '-|', qw/fc-match -f %{file}/, $fona or die $!;
my $ffp = <I>;
close I;
die "Cannot find font file for '$fona'\n", unless $ffp;
print "Font file: $ffp\n";

mkdir $dd;
chdir $dd or die "Cannot cd to '$dd': $!\n";

unless (@weekdays) {
    push @weekdays, strftime("%a", 0, 0, 12, $_, 0, 101) for (1..7);
}
print "Weekdays: @weekdays\n";

my $info = 'mk-txtimgs.pl dir/ ' . (join ',', @weekdays)
  . " $fhgt $txhx $wekl $fona" . ($o0 eq 'O'? ' O': '');

my (@fsfx, @txts);
sub mk_imgs($$$)
{
    #print "@_\n"; return;
    #although harfbuzz is used, output may not be so harfbuzzy ;D
    system qw/hb-view --margin=0/, "--font-size=$fhgt",
      '-o',"$_[1].png", $ffp, $_[2];
    system 'magick', "$_[1].png", "$_[1].pgm";
    return if $_[0] eq '';
    push @fsfx, [$_[0], $_[1]];
    push @txts, $_[2];
}

mk_imgs 0, '0', $o0;
mk_imgs 0, $_, $_ for (1..9);
mk_imgs 1, 'colon', ':';
mk_imgs 2, 'dot', '.';
mk_imgs 3, 'w', $wekl;
mk_imgs 4, 'plus', '+';
mk_imgs 5, 'minus', '-';
mk_imgs 6, "wd_$_", $weekdays[$_-1] for (1..7);

mk_imgs '', "LINE", " @txts ";

sub openI($) {
    open I, '<', $_[0] or die "Cannot open '$_[0]' for reading: $!\n";
    binmode I
}
sub openO($) {
    open O, '>', $_[0] or die "Cannot open '$_[0]' for writing: $!\n";
    binmode O;
}

my $H = -1;
my $x1 = 1234567890; # top "all white" pixels
my $x2 = 1234567890; # bot "all white" pixels
foreach my $ref (@fsfx)
{
    my $fn = "$ref->[1].pgm";
    openI $fn;
    $_ = <I>;
    die "'$fn' does not start with 'P5'\n" unless $_ eq "P5\n";
    $_ = <I>;
    /^(\d+)\s(\d+)$/ or die "'$fn': 2nd line is not 'width height'\n";
    my ($width, $height) = ($1, $2);
    $H = $height if $H < 0;
    die "'$fn': unexpected height ($height != $H)\n" unless $height == $H;
    $_ = <I>;
    chomp, die "'$fn': max gray val '$_' != '255'\n" unless $_ eq "255\n";
    my $x = 0;
    while ($x < $height) {
	read I, $_, $width;
	die unless length == $width;
	last unless tr/\xff// == $width;
	$x++;
    }
    $x1 = $x if $x < $x1;
    $x = 0;
    while (1) {
	read I, $_, $width;
	last unless length == $width;
	$x = 0, next unless tr/\xff// == $width;
	$x++;
    }
    $x2 = $x if $x < $x2;
    close I;
    #print "$x1, $x2, $fn\n";
}

$H = $H - $x1 - $x2 + $txhx;

printf "Height: $H";

my @widths;
my $bytes = 0;

# drop all white pixel lines -- and collect widths...
foreach my $ref (@fsfx)
{
    my $fn = "$ref->[1].pgm";
    my $w = $widths[$ref->[0]] // 0;
    openO "x-$fn";
    openI $fn;
    print O scalar(<I>); # P5
    $_ = <I>;
    /^(\d+)\s(\d+)$/ or die "'$fn': 2nd line is not 'width height'";
    my ($width, $height) = ($1, $2);
    $bytes += $width * $height * 4;
    if ($w == 0) { $widths[$ref->[0]] = $width; print "\nWidth group $ref->[0]: $width" }
    else {
	print(" *$width*\n"), $| = 1,
	  die "XXX non-monospace widths not yet supported...\n" if $width != $w;
	print " $width"
    }
    print O "$width $H\n";
    print O scalar(<I>); # 255
    read I, $_, $width * $x1;
    read I, $_, $width * $H;
    print O $_;
    close O;
    close I;
}
print "\n$bytes bytes\n";

openO "txts.ch.wip";
select O;

print "/* generated by $0 -- include once */

#include <stdint.h> // if not already included //

#define TEXTS_CH_INFO \"$info\"

#define TEXTHEIGHT $H

struct txtimg {
	int width;
	uint32_t pixels[];
};\n
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored \"-Wpedantic\"
\n";

foreach my $ref (@fsfx)
{
    #my $wo = $ref->[0];
    #my $w = $widths[$wo] // 0;
    my $n = $ref->[1];
    openI "x-$n.pgm";
    <I>; $_ = <I>; /(\d+)/;
    my $W = $1;
    #$widths[$wo] = $W if $W > $w;
    print "static const struct txtimg ti_$n = {
	.width = $W, // ${W}x${H}\n\t.pixels = {";
    <I>;
    my $i = 4;
    while (read I, $_, 1 > 0) {
	$_ = 255 - ord $_;
	$i = 0, print "\n\t\t" if ++$i == 5;
	printf "0xff%02x%02x%02x,", $_, $_, $_;
    }
    print "\n\t}\n};\n";
}

print "#pragma GCC diagnostic pop

#define TEXTWIDTH_NUM $widths[0]
#define TEXTWIDTH_COL $widths[1]
#define TEXTWIDTH_DOT $widths[2]
#define TEXTWIDTH_W $widths[3]
#define TEXTWIDTH_PLUS $widths[4]
#define TEXTWIDTH_MINUS $widths[5]
#define TEXTWIDTH_WDAYS $widths[6]
\n";
close O;
rename "txts.ch.wip", "txts.ch";
unlink '../txts.ch';
symlink "$dd/txts.ch", '../txts.ch';
print STDOUT "Wrote $odir/txts.ch\n";
