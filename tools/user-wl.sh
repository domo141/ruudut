#!/bin/sh
#
# $ wl -- wayland related commands executor $
# based on
# multitool-tmpl3.sh -- perhaps the best of these in multitool-tmpl series
#
# Created: Tue 02 Apr 2024 19:39:56 EEST too
# Last modified: Sat 11 Oct 2025 14:31:43 +0300 too
#
# SPDX-License-Identifier: Unlicense
#

case ${BASH_VERSION-} in *.*) set -o posix; shopt -s xpg_echo; esac
case ${ZSH_VERSION-} in *.*) emulate ksh; esac

set -euf  # hint: (z|ba|da|'')sh -x thisfile [args] to trace execution

die () { printf '%s\n' '' "$@" ''; exit 1; } >&2

x () { printf '+ %s\n' "$*" >&2; "$@"; }
x_bg () { printf '+ %s\n' "$*" >&2; "$@" & }
x_env () { printf '+ %s\n' "$*" >&2; env "$@"; }
x_eval () { printf '+ %s\n' "$*" >&2; eval "$*"; }
x_exec () { printf '+ %s\n' "$*" >&2; exec "$@"; exit not reached; }

bn0=${0##*/}

usage () { die "Usage: $bn0 $cmd $@"; }

cmds=

cmds=$cmds'
cmd_i   interactively choose command from list to be executed'
cmd_i ()
{
	command -v fzf >/dev/null &&
		set -- fzf +s -e --tac --no-sort --no-info ||
		set -- awk '/.../ { printf "%2d %s\n", FNR, $0 }'
	l=`"$@" << EOF
$0 size
$0 dirs
xev
wev
wayland-info
weston-info
wlrctl output list
wlr-randr | less

EOF` || exit 0
	test $1 = awk && {
		echo "$l"
		echo "Install 'fzf' to execute from the list above"
		exit 0
	}
	test "$l" || exit 0
	case $l in *'|'*) x_eval "$l"; exit; esac
	x_exec $l
	exit not reached
}

cmds=$cmds'
cmd_screenshot screenshot (grim, optionally slurp)'
cmd_screenshot ()
{
	case ${1-} in [1-9]|[1-9][0-9]) sleepsecs=$1; shift ;; *) sleepsecs=
	esac

	test $# = 0 && usage \
		"[sleepsecs] [d] [c] ('f'|'s'|wxh+x+y) [e] [t]" '' \
		" 'f': \"full\" output | 's': rectangle using slurp" \
		"  wxh+x+y - or slurp format - to enter rectangle" \
		" 'd': \"dry run\" | 'c': capture cursor | 'e': edit" \
		" 't': temporary; show after capture, then remove"

	gcnv () {
		case $a in *[!0-9x+]*) die "Unexpected chars in '$a'"; esac
		case $a in *[!0-9]) die "Unexpected format in '$a'"; esac
		: ifs=$IFS; IFS=+x
		set -- $a
		IFS=$ifs #; unset ifs
		test $# = 4 || die "Unexpected format in '$a'"
		s="$3,$4 $1x$2"
	}
	gchk () {
		case $a in *[!0-9,x' ']*) die "Unexpected chars in '$a'"; esac
		case $a in *[!0-9]) die "Unexpected format in '$a'"; esac
		IFS=' , x'
		set -- $a
		IFS=$ifs
		test $# = 4 || die "Unexpected format in '$a'"
		s=$a
	}
	d= c= t= e= s=
	for a
	do	case $a in *d*) d=true	; esac
		case $a in *c*) c='-c'	; esac
		case $a in *t*) t=-tmp	; esac
		case $a in *e*) e=true	; esac
		case $a in *f*) s=f	; esac
		case $a in *s*) s=s	; esac
		case $a in [1-9]*x[1-9]*'+'[1-9]*+[1-9]*) gcnv; esac
		case $a in [1-9]*,[1-9]*' '[1-9]*x[1-9]*) gchk; esac
	done
	test "$t" && test "$e" && die "'t' and 'e' mutually exclusive"

	test "$s" || die "no 'f' nor 's' (nor geom) in args '$s'"

	test $sleepsecs && x sleep $sleepsecs

	case $s in s) s=`slurp -d` ox=${s##* }
		;; f) ox=fs s=
		;; *) ox=${s##* }
	esac

	f=`date +screenshot-%Y%m%d-%H%M%S-$ox$t.png`
	if test "$d"
	then
		echo grim $c ${s:+-g "'$s'"} $f
		exit
	fi
	x grim $c ${s:+-g "$s"} $f

	test "$e" && gthumb $f

	test "$t" || exec ls -go $f

	x feh $f
	x_exec rm $f
	exit not reached
}

cmds=$cmds'
cmd_pngshrink shrink png image (to post-tune screenshot)'
cmd_pngshrink ()
{
	test $# -ge 2 ||
	  usage "origpng shrinkedpng [pngquant options]" '' \
		'Without pngquant options "conversion" lossless' '' \
		"'' as pngquant options will run pngquant (once) w/o options"\
		"Otherwise pngquant run twice - first that optionless run" ''\
	'Some tried potentially effective pngquant options:' '' \
	" 64 (or any other value) - number of colors reduced to 'num'" \
	" --posterize 4 --nofs    - another way to reduce... use w/ 'num'"

	command -v optipng >/dev/null  || die "'optipng': no such command"
	test $# -le 2 || command -v pngquant >/dev/null ||
		die "'pngquant': no such command"

	case $2 in *.png) ;; *) die "Output file '$2' does not end with '.png'"
	esac
	test -f "$1" || die "'$1' input file missing"
	test -e "$2" && die "'$2' output file exists"
	ifile=$1 ofile=$2; shift 2

	trap 'x rm -f "$ofile.wip"' 0
	if test $# = 0
	then x cp $ifile $ofile.wip
	else x pngquant -f -o "$ofile.wip" "$ifile"
	fi
	test "${1-}" && x pngquant -f -o "$ofile.wip" "$@" "$ofile.wip"

	x optipng --strip all -o9 "$ofile.wip"
	s1=`stat -c %s "$ifile"`
	s2=`stat -c %s "$ofile.wip"`
	test $s2 -ge $s1 && exit 0
	# traps don't run after exec so trap - 0 not here
	x_exec mv "$ofile.wip" "$ofile"
	exit not reached
}

# this could take similar options like screenshot but SMOP
cmds=$cmds'
cmd_record wf-recorder w/ slurp for region to record'
cmd_record ()
{
	test $# = 0 && usage 'sleepsecs [extra wf-recorder opts]'
	case $1 in 0) echo slurp '(rght cancels)'
		;; [0-9]|[1-9][0-9]) echo slurp '(rght cancels)', then sleep $1
		;; *) die "'$1' not numeric in 0-99"
	esac
	test $1 = 0 || x sleep $1
	slurp=`slurp -d`; ox=${slurp##* }
	f=`date +wl-rec-%Y%m%d-%H%M%S-$ox.mkv`
	shift
	x_exec wf-recorder -g "$slurp" -f $f "$@"
	exit not reached
}

cmds=$cmds'
cmd_size  size(s) of the output(s)'
cmd_size ()
{
	exec perl -x "$0" size
	exit not reached
}

cmds=$cmds'
cmd_dirs list some wayland-related (system) dirs'
cmd_dirs ()
{
	ds=
	for d in /usr/share/wayland-sessions ${XDG_RUNTIME_DIR-}
	do test -d $d && ds=$ds\ $d
	done
	test $# = 0 || x_exec ls -goF "$@" $ds
	test -t 0 || x_exec ls -goF $ds
	x ls -goF $ds | ${PAGER:-less}
	exit
}

cmds=$cmds'
cmd_debug  run client w/ WAYLAND_DEBUG=1'
cmd_debug ()
{
	test $# = 0 && usage 'command [args]'
	export WAYLAND_DEBUG=1
	x_exec "$@"
	exit not reached
}

# ---

ifs=$IFS; readonly ifs
IFS='
'
test $# = 0 && {
	echo
	echo Usage: $0 '{command} [args]'
	echo
	echo Commands of $bn0 "('.' to list, '.. cmd(pfx)' to view source):"
	echo
	# 2 outcommented alternatives to the fork(2)less shell implementation
	#echo "$cmds" | sed 's/ .*//; s/cmd_/  /' | column
	#perl -x "$0" cmdcols
	set -- $cmds # xx x x x x
	rows=$((($# + 4) / 5))
	cols=$((($# + ($rows - 1)) / $rows))
	#echo argc $# - rows $rows - cols $cols
	c=0; while test $c -lt $rows; do eval r$c="'  '"; c=$((c + 1)); done
	c=0; i=0
	for arg
	do arg=${arg%% *}; arg=${arg#cmd_}
	   case $arg in *_*) arg=${arg%_*}-${arg#*_}; esac
	   test $i -lt $(($# - rows)) && {
	      # one ' ' less than '?'s can handle cmd that has just 1 char
	      arg=$arg'           '; arg=${arg%${arg#????????????}}; } # 11, 12
	   eval r$c='$r'$c'$arg'
	   #printf '%2d - %d  |%s|\n' $i $c $arg
	   i=$((i + 1)); c=$((i % rows))
	done
	#as $IFS is \n, no need to quote \$r$c
	c=0; while test $c -lt $rows; do eval echo \$r$c; c=$((c + 1)); done
	echo
	echo Command can be abbreviated to any unambiguous prefix.
	echo
	exit 0
}
cm=$1; shift

case $#/$cm
in 0/.)
	# in a multitool w/ just a few commands this can be above when $# = 0
	set -- $cmds
	IFS=' '
	echo
	for cmd
	do	set -- $cmd; cmd=${1#cmd_}; shift
		case $cmd in *_*) cmd=${cmd%_*}-${cmd#*_}; esac
		printf ' %-10s  %s\n' $cmd "$*"
	done
	echo
	exit
;; 1/..)
	set +x
	# $1 not sanitized but that should not be too much of a problem...
	exec sed -n "/^cmd_$1/,/^}/p; \${g;p}" "$0"
;; */.) cm=$1; shift
;; */..) cmd=..; usage cmd-prefix

#;; */d) cm=diff
;; *-*-*) die "'$cm' with too many '-'s"
;; *-*) cm=${cm%-*}_${cm#*-}
esac

cc= cp=
for m in $cmds
do
	m=${m%% *}; m=${m#cmd_}
	case $m in
		$cm) cp= cc=1 cmd=$cm; break ;;
		$cm*) cp=$cc; cc=$m${cc:+, $cc}; cmd=$m
	esac
done
IFS=$ifs

test "$cc" || die "$0: $cm -- command not found."
test "$cp" && die "$0: $cm -- ambiguous command: matches $cc."

unset cc cp cm
#set -x
cmd'_'$cmd "$@"
exit

#!perl
#line 210
#---- 210

use 5.8.1;
use strict;
use warnings;

if ($ARGV[0] eq 'sfzf') {
	open I, '<', $ARGV[1] or die "Failed to open '$ARGV[1]': $!\n";
	$_ = join '', sort <I>;
	close I;
	# 100-200KB buffer, pipe(2) has much smaller
	socketpair R,W, 1,1,0; # af_unix,sock_stream,pf_unspec
	#shutdown R, 1; # no more writing for reader
	syswrite W, $_;
	close W; #close-on-exec but closed anyway
	open STDIN, '>&', R;
	close R; #close-on-exec but warnings complain
	rename $ARGV[1], $ARGV[1] . 'g' if $ARGV[1] =~ /,$/;
	exec qw/fzf +s -e --tac --no-sort --no-mouse/;
	exit 'not reached'
}
if ($ARGV[0] eq 'size') {
	open P, '-|', 'wlr-randr' or exit 1; #die "$!\n";
	print "\n";
	my ($mmx, $mmy) = (99, 66);
	while (<P>) {
		print($_), next if /^\S/;
		($mmx, $mmy) = ($1, $2), next if /size:\s+(\d+)x(\d+)\s+mm/;
		if (/(\d+)x(\d+)\s+px.*\sHz\s.*current/) {
			my $dmm = sqrt($mmx * $mmx + $mmy * $mmy);
			printf "  $1x$2 px, ${mmx}x$mmy mm" .
				" (%2.1fx%2.1f in) (%d mm,  %2.1f\" /)\n",
				$mmx / 25.4, $mmy / 25.4, $dmm, $dmm / 25.4;
				#sqrt($mmx * $mmx + $mmy * $mmy) / 25.4;
			printf "  x: %2.2f px/mm, %3.1f ppi  :",
				$1 / $mmx, $1 * 25.4 / $mmx;
			printf "  y: %2.2f px/mm, %3.1f ppi\n\n",
				$2 / $mmy, $2 * 25.4 / $mmy;
			($mmx, $mmy) = (99, 66)
		}
	}
	exit ! close P
}

# Local variables:
# mode: shell-script
# sh-basic-offset: 8
# tab-width: 8
# End:
# vi: set sw=8 ts=8
