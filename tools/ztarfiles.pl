#!/usr/bin/perl
# -*- mode: cperl; cperl-indent-level: 4 -*-
# $ ztarfiles.pl $
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#	Copyright (c) 2020 Tomi Ollila
#	    All rights reserved
#
# Created: Fri 21 Aug 2020 18:18:04 EEST too (custar.pl)
# Created: Wed 26 Mar 2025 21:31:11 EET too (ztarfiles.pl)
# Last modified: Wed 09 Apr 2025 22:01:17 +0300 too

# SPDX-License-Identifier: BSD 2-Clause "Simplified" License

# this is used so one does not need to depend on gnu tar
# (or have any tar-specific options).
# not using record size of 10240 - simpler to end with two 512 byte blocks.
# hard links to files (already) collected, inodes in %hash is easy, so stored.
# symlinks are harder -- storing file content of cross-directory symlinks
# (then target need to exist) - symlink targets w/o '/'s are stored as is...

use 5.8.1;
use strict;
use warnings;

use POSIX qw/mktime/;

$ENV{TZ} = 'UTC';

die "\nUsage: $0 zff prefix style ([mtime files...] | excludes...)\n\n",
  "  zff: '[tar.]gz', '[tar.]xz', '[tar.]zst', 'tgz', 'txz' or 'tzst'\n",
  "  known styles: 'git-clean', 'files'\n",
  "  'excludes...' for 'git-clean' - 'mtime' and 'files...' for 'files'\n",
  "  mtime formats for files (in UTC):\n",
  "    yyyy-mm-dd  yyyy-mm-ddThh:mm:ss  yyyy-mm-dd+hh:mm:ss\n",
  "    yyyy-mm-dd+hh:mm  yyyy-mm-dd+hh  hh:mm  d  \@secs\n",
  "   hh:mm -- hour and min today,  d -- number of days ago 00:00\n",
  "\n" unless @ARGV > 2;

my $sfx = shift;
my $zc;
if ($sfx eq 'tar.gz' or $sfx eq 'tgz' or $sfx eq 'gz') {
    $sfx = 'tar.gz' if $sfx eq 'gz';
    $zc = 'gzip -9n'
} elsif ($sfx eq 'tar.xz' or $sfx eq 'txz' or $sfx eq 'xz') {
    $sfx = 'tar.xz' if $sfx eq 'xz';
    $zc = 'xz -9'
} elsif ($sfx eq 'tar.zst' or $sfx eq 'tzst' or $sfx eq 'zst') {
    $sfx = 'tar.zst' if $sfx eq 'zst';
    $zc = 'zstd -19'
} else {
    die qq['$sfx': unknown "zff"]
}

my $ofp = shift;

die "'$ofp' starts with '.'\n" if ord $ofp == 46;
die "'$ofp' starts with '/'\n" if ord $ofp == 47;
die "'$ofp' ends with '/'\n" if $ofp =~ m:/$:;
die "'$ofp' contains '$1'\n" if $ofp =~ m:(/[.][.]?/):;

my $style = shift;
my $gmtime;

if ($style eq 'files') {
    $gmtime = shift;

    if ($gmtime =~ /^@(\d+)$/) {
	$gmtime = $1;
    } elsif ($gmtime =~ /^(\d\d?):(\d\d)$/) {
	$gmtime = time;
	$gmtime = $gmtime - $gmtime % 86400 + $1 * 3600 + $2 * 60;
    } elsif ($gmtime =~ /^\d$/) {
	$gmtime = time - 86400 * $gmtime;
	$gmtime = $gmtime - $gmtime % 86400;
    } elsif ($gmtime =~
	     /(20\d\d)-([01]\d)-([0-3]\d)$/) {
	$gmtime = mktime(0, 0, 0, $3, $2 - 1, $1 - 1900);
    } elsif ($gmtime =~
	     /(20\d\d)-([01]\d)-([0-3]\d)\+([012]\d)$/) {
	$gmtime = mktime(0, 0, $4, $3, $2 - 1, $1 - 1900);
    } elsif ($gmtime =~
	     /(20\d\d)-([01]\d)-([0-3]\d)\+([012]\d):([0-5]\d)$/) {
	$gmtime = mktime(0, $5, $4, $3, $2 - 1, $1 - 1900);
    } elsif ($gmtime =~
	     /(20\d\d)-([01]\d)-([0-3]\d)[+T]([012]\d):([0-5]\d):([0-5]\d)$/) {
	$gmtime = mktime($6, $5, $4, $3, $2 - 1, $1 - 1900);
    }
    else { die "'$gmtime': unknown time format\n"; }
}
elsif ($style eq 'git-clean') {
    $_ = `git status --porcelain -uno .`;
    die "git tree not clean:\n$_" if $_;
    open P, '-|', qw/git log -1 --abbrev=7/, '--format=%h %ct %ci' or die $!;
    $_ = <P>;
    close P or die 'xxx';
    die "Failed to read git info\n" unless /(\w+)\s+(\d+)\s+(\d+)-(\d+)-(\d+)/;
    $gmtime = $2;
    $ofp .= "-$3$4$5-g$1";
    my @excludes = @ARGV;
    foreach (@excludes) {
	# good enough? glob-to-re conversion ?
	$_ = "^$_" unless s/^[*]+//;
	$_ = "$_\$" unless s/[*]+$//;
	s/[.]/[.]/g; s/[*]/.*/g; s/[?]/./g;
	$_ = qr/$_/;
    }
    open P, '-|', qw/git ls-files/ or die $!;
    my $crc16 = 0;
    @ARGV = grep {
	chomp;
	my $f = 1;
	foreach my $re (@excludes) { $f = 0, last if $_ =~ $re }
	if ($f) {
	    # based on https://www.perlmonks.org/?node_id=997760
	    for (unpack 'C*', $_) {
		$crc16 ^= $_;
		for (0..7) {
		    my $carry = $crc16 & 1;
		    $crc16 >>= 1;
		    $crc16 ^= 0x8408 if $carry;
		}
	    }
	} $f
    } <P>;
    $ofp .= sprintf '-%04x', $crc16;
    close P or die 'xxx';
}
else { die "'$style' not 'git-clean' nor 'files'" }

die "No files!\n" unless @ARGV;

# declare tarlisted.pm functions #

sub _tarlisted_mkhdr($$$$$$$$$$$$);
sub _tarlisted_writehdr($);
sub _tarlisted_xsyswrite($);

sub tarlisted_open($@); # name following optional compression program & args
sub tarlisted_close();

my $owip;
END { unlink $owip if defined $owip }

$owip = "$ofp.wip";
tarlisted_open $owip, (split ' ', $zc);

#use Data::Dumper;

$| = 1;

my $dotcount = 0;
my %links;
L: foreach (@ARGV) {
    my @st = lstat $_;
    warn("lstat '$_' failed: $!\n"), next unless @st;
    my $n = $ofp.'/'.$_;
    if (-l _) {
	my $l = readlink $_;
	unless ($l =~ m,/,) {
	    # storing symlink when no path components
	    _tarlisted_writehdr _tarlisted_mkhdr
	      $n, 0777, 0,0, 0, $gmtime, '2', $l, '','',-1,-1;
	    next
	}
	# else stat(2)ing -- the file content behind symlink stored
	@st = stat $_;
	warn("stat '$_' failed: $!\n"), next unless @st;
    }
    #print Dumper($_);
    my $prm = $st[2] & 0777;
    #print $_->[0], " ", $->[7], " ", $prm, "\n";
    print(((++$dotcount) % 72)? '.': "\n");
    warn("skipping directory '$_'\n"), next if -d _;
    warn("skipping chardev '$_'\n"), next if -c _;
    warn("skipping blockdev '$_'\n"), next if -d _;
    # fixme: fifos not yet handled (or is it a feature)
    warn("hmm, '$_' not file (fifo?)\n"), next unless -f _;

    # hard links we do
    my ($size, $type, $lname) = ($st[7], '0', '');
    if ($st[3] > 1) {
	my $devino = "$st[0].$st[1]";
	$lname = $links{$devino};
	if (defined $lname) {
	    $type = '1';
	    $size = 0;
	}
	else {
	    $links{$devino} = $n;
	}
    }
    _tarlisted_writehdr _tarlisted_mkhdr
      $n, $prm, 0,0, $size, $gmtime, $type, $lname, '','',-1,-1;

    next if $lname;

    open my $in, '<', $_ or die "opening '$_': $!\n";
    my $buf; my $tlen = 0;
    while ( (my $len = sysread($in, $buf, 524288)) > 0) {
	_tarlisted_xsyswrite $buf;
	$tlen += $len;
	die "$n grew while being archived\n" if $tlen > $size;
    }
    die "$n: Short read ($tlen != $size)!\n" if $tlen != $size;
    close $in; # fixme, check
    _tarlisted_xsyswrite "\0" x (512 - $size % 512) if $size % 512;
}
print "\n" if $dotcount % 72;

tarlisted_close and die "Closing tar file failed: $!\n";

$ofp .= '.' . $sfx;
rename $owip, $ofp;
undef $owip;
print "Wrote $ofp\n";

# from tarlisted.pm #

my $_tarlisted_pid;

sub _tarlisted_pipetocmd(@)
{
    pipe PR, PW;
    $_tarlisted_pid = fork;
    die "fork() failed: $!\n" unless defined $_tarlisted_pid;
    unless ($_tarlisted_pid) {
	# child
	close PW;
	open STDOUT, '>&TARLISTED';
	open STDIN, '>&PR';
	close PR;
	close TARLISTED;
	exec @_;
	die "exec() failed: $!";
    }
    # parent
    close PR;
    open TARLISTED, '>&PW';
    close PW;
}

sub _tarlisted_nampfx($$) {
    local $_ = $_[0];
    my $n = '';
    while (s:/([^/]+)$::) {
	$n = $n? "$n/$1": $1;
	last if length $n > 100;
	next if length $_ > 155;
	$_[1] = $_;
	return $n
    }
    die "'$_[0]': does not fit in ustar header file name fields\n"
}

# IEEE Std 1003.1-1988 (“POSIX.1”) ustar format
# name perm uid gid size mtime type lname uname gname devmajor devminor
sub _tarlisted_mkhdr($$$$$$$$$$$$)
{
    if (length($_[7]) > 100) {
	die "Link name '$_[7]' too long\n";
    }
    my $name = $_[0];
    my $prefix;
    if (length($name) > 100) {
	$name = _tarlisted_nampfx $name, $prefix
    }
    else {
	$prefix = ''
    }
    $name = pack('a100', $name);
    $prefix = pack('a155', $prefix);

    my $mode = sprintf("%07o\0", $_[1]);
    my $uid = sprintf("%07o\0", $_[2]);
    my $gid = sprintf("%07o\0", $_[3]);
    my $size = sprintf("%011o\0", $_[4]);
    my $mtime = sprintf("%011o\0", $_[5]);
    my $checksum = '        ';
    my $typeflag = $_[6];
    my $linkname = pack('a100', $_[7]);
    my $magic = "ustar\0";
    my $version = '00';
    my $uname = pack('a32', $_[8]);
    my $gname = pack('a32', $_[9]);
    my $devmajor = $_[10] < 0? "\0" x 8: sprintf("%07o\0", $_[10]);
    my $devminor = $_[11] < 0? "\0" x 8: sprintf("%07o\0", $_[11]);
    my $pad = pack('a12', '');

    my $hdr = join '', $name, $mode, $uid, $gid, $size, $mtime,
      $checksum, $typeflag, $linkname, $magic, $version, $uname, $gname,
	$devmajor, $devminor, $prefix, $pad;

    my $sum = 0;
    foreach (split //, $hdr) {
	$sum = $sum + ord $_;
    }
    $checksum = sprintf "%06o\0 ", $sum;
    $hdr = join '', $name, $mode, $uid, $gid, $size, $mtime,
      $checksum, $typeflag, $linkname, $magic, $version, $uname, $gname,
	$devmajor, $devminor, $prefix, $pad;

    return $hdr;
}

sub _tarlisted_xsyswrite($)
{
    my $len = syswrite TARLISTED, $_[0] or 0;
    my $l = length $_[0];
    while ($len < $l) {
	die "Short write!\n" if $len <= 0;
	my $nl = syswrite TARLISTED, $_[0], $l - $len, $len or 0;
	die "Short write!\n" if $nl <= 0;
	$len += $nl;
    }
}

sub _tarlisted_writehdr($)
{
    _tarlisted_xsyswrite $_[0];
}

sub tarlisted_open($@)
{
    die "tarlisted alreadly open\n" if defined $_tarlisted_pid;
    $_tarlisted_pid = 0;
    if ($_[0] eq '-') {
	open TARLISTED, '>&STDOUT' or die "dup stdout: $!\n";
    } else {
	open TARLISTED, '>', $_[0] or die "> $_[0]: $!\n";
    }
    shift;
    _tarlisted_pipetocmd @_ if @_;
}

sub tarlisted_close0()
{
    close TARLISTED; # fixme: need check here.
    $? = 0;
    waitpid $_tarlisted_pid, 0 if $_tarlisted_pid;
    undef $_tarlisted_pid;
    return $?;
}

sub tarlisted_close()
{
    # end archive
    _tarlisted_xsyswrite "\0" x 1024;
    return tarlisted_close0;
}
