#!/usr/bin/perl
# -*- mode: cperl; cperl-indent-level: 4 -*-
# vertical background bars to rxvt-unicode window
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#	Copyright (c) 2021 Tomi Ollila
#	    All rights reserved
#
# Created: Sat 27 Nov 2021 15:53:37 EET too
# Last modified: Fri 13 Sep 2024 21:44:50 +0300 too

use 5.8.1;
use strict;
use warnings;

use File::Temp qw/ tempfile /;

die "'$ENV{TERM}' not rxvt-unicode*\n" unless $ENV{TERM} =~ /^rxvt-unicode/;

my $windowid = $ENV{WINDOWID} || # takes care both unset and empty (vs. //)
  die "'WINDOWID' not in environment (hint: '.' and paste printf...)\n";

my $hold = do {
    if (@ARGV) {
	if ($ARGV[0] eq '.') { shift; 1 }
	elsif ($ARGV[0] eq '..') { shift; 2 }
	else { 0 }
    }
    else { 0 }
};

$0 =~ s:.*/::,
die "\nUsage: $0 ['.'] [fgcolor] color [+]column [color [+]column...]\n"
  , "\n  color [+]column -- color and end column -- with + adds to previous."
  , "\n                     given in pairs, at least 2 pairs required."
  , "\n                     last column given as '.' -- current window width."
  , "\n  fgcolor         -- foreground text color if given. \"evenness\""
  , "\n                     of the arg count determines existence of this."
  , "\n  '.' -- instead of printing escape sequences, write the printf line to"
  , "\n         stdout. this line can then be pasted on another urxvt window"
  , "\n         where one may have e.g. ssh'd out to a remote system."
  , "\n         the created bg image is held until enter is pressed so that"
  , "\n         urxvt can access the image (font width expected to match).\n"
  , "\nExamples (note: first 3 makes bg dark but does not change text color):\n"
  , "\n  $0 000 72 311 +8 ;: 'black cols 1-72, #311 73-80, then again #000'"
  , "\n  $0 black 8 gray10 16 ;: 'black/gray10 change every 8 columns'"
  , "\n  $0 black 8 gray10 +8 ;: 'ditto'"
  , "\n  $0 aaa gray20 1 black +7 ;: '#aaa text, gray20 on every 8-\"tabstop\"'"
  , "\n  $0 222 gray60 1 gray80 8 ;: 'dark text, light bg, grayer tabstop'"
  , "\n\n"
  unless @ARGV >= 4;

sub setcolor($$$)
{
    # somewhat heuristic checks -- catches worst typos...
    #die "'$_[1]': less than 3 chars in '$_[2]' (hint: trailing '.'?)\n"
    die "'$_[1]': less than 3 chars in '$_[2]'\n"
      unless length $_[1] >= 3;
    die "'$_[1]': suspicious chars in $_[2]\n" unless $_[1] =~ /^#?[\w ]+$/;
    $_[0] = ($_[1] =~ /^[\da-f]+$/)? "#$_[1]": $_[1];
}
setcolor my $fgcolor, shift, 'fgcolor' if @ARGV & 1;

my @pixchrs = qw/1 2 3 4 5 6 7 8 9 0 a b c d e f/; # more to be added...

die "too many bars...\n" if @ARGV / 2 > @pixchrs;

my $prevcol = 0;
my @list;

my $tcols = qx/stty size/; $tcols =~ /^(\d+)\s+(\d+)/; $tcols = $2;
my $trows = $1;

$ARGV[$#ARGV] = $tcols if $ARGV[$#ARGV] eq '.';

my $cnt = 0;
while (@ARGV) {
    setcolor my $color, shift, 'color';
    my $column = shift;
    die "'$column': not [+]column\n" unless $column =~ /^([+]?)(\d+)$/;
    if ($1) {
	$column = $prevcol + $2;
    } else {
	die "'$2' not > $prevcol\n" unless $2 > $prevcol;
    }
    die "terminal (${tcols}x$trows) narrower than column ", $column, "\n"
      unless $hold or $tcols >= $column;
    $color = "#$color" if $color =~ /^[\da-f]+$/;
    push @list, [ $pixchrs[$cnt++], $color, $column - $prevcol ];
    $prevcol = $column;
}
$hold = 0 if $hold == 2;

my $x = qx/xwininfo -size -id $windowid/;
die "$x:^: no expected content...\n" unless $x =~ / x resize increment: (\d+)/;
$x = $1;

my $w = $prevcol * $x;
my $cc = @list;

my ($fh, $filename) = tempfile(UNLINK => 1, SUFFIX => '.xpm');
select $fh;

print <<"EOF";
/* XPM */
static char *pix[] = {
/* columns rows colors chars-per-pixel */
"$w 1 $cc 1",
EOF
print "\"$_->[0] c $_->[1]\",\n" for @list;
print "/* pixels */\n\"";
print $_->[0] x ($_->[2] * $x) for @list;
print "\",\n};\n";
#__END__
select STDOUT;
close $fh;

my ($e, $a) = $hold? ('printf "\033', '\007"' . "\n"): ("\033", "\007");

print "$e]20;$filename;style=tiled$a";
print "$e]10;$fgcolor$a" if defined $fgcolor;
STDOUT->flush();
if ($hold) {
    print "-- use the above line on another urxvt -- press enter to quit --\n";
    sysread STDIN, $_, 1;
} else {
    print substr('.234567890' x (($tcols / 10) + 1), 0, $tcols), "\n";
    # expect 1/4 sec is enough for urxvt to react on received escape sequence
    select undef, undef, undef, 0.25;
}
