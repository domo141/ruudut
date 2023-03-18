#!/bin/sh
#
# $ mkx11bg.sh $
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#	Copyright (c) 2024 Tomi Ollila
#	    All rights reserved
#
# Created: Wed 31 Jan 2024 18:17:47 EET too
# Last modified: Sun 17 Nov 2024 20:04:49 +0200 too

case ${BASH_VERSION-} in *.*) set -o posix; shopt -s xpg_echo; esac
case ${ZSH_VERSION-} in *.*) emulate ksh; esac

set -euf  # hint: (z|ba|da|'')sh -x thisfile [args] to trace execution

die () { printf '%s\n' '' "$@" ''; exit 1; } >&2

case $# in 5) test -f "$5" && die "output file '$5' exists"
	;; 2|4)
	;; *) die "Usage: ${0##*/} scale shift [color0 color1 [ofile.xpm]]" ''\
		"Try e.g. ${0##*/} 3 0 black 639 ;: to see how it works..."
esac

case $1 in [1-9]) ;; *) die "scale '$1' not in [1-9]"; esac
case $2 in [0-3]) ;; *) die "shift '$2' not in [0-3]"; esac

case $# in 4|5)
	tstit () {
		test ${#1} -lt 3 && "'$1' not at least 3 chars..."
		case $1 in '#'*) die "'$1' starts with '#' (not needed)"
		;; *[!0-9a-z' ']*)
			die "'$1 contains chars outside 0-9, a-f and ' '"
		;; *[!0-9a-f]*)
			case $1 in [0-9]*) die "'$1' starts with a number"
			esac
		;; *)
			case ${#1} in 3|6)
				;; *) die "length of '$1' not 3 or 6"
			esac
			set -- '#'$1 $2
		esac
		eval $2='$1'
	}
	tstit "$3" c3; tstit "$4" c4
esac

s=$1 r=$2

l () {
	d= x= c=0
	while test $c -lt $s
	do d=.$d x=x$x c=$((c + 1))
	done
}
l
qp= qs=
l () {
	case $r in 0) l=$1$2$3$4
		;; 1) l=$4$1$2$3
		;; 2) l=$3$4$1$2
		;; 3) l=$2$3$4$1
	esac
	c=0
	while test $c -lt $s
	do 	c=$((c + 1)); test 0$c = $h$s && qs=${qs%?}; echo $qp$l$qs
	done
}
test $# = 5 && exec > "$5"
test $# = 2 || {
	c=$((s * 4))
	qp='"' qs='",'
	printf %s\\n '/* XPM */' 'static char *x11bg[] = {' "\"$c $c 2 1"\", \
	       "\". c $c3"\", "\"x c $c4"\",
}
h=1
l $d $x $d $d
l $x $d $d $d
l $d $d $x $d
h=0
l $d $d $d $x

test $# = 2 || echo '};'


# Local variables:
# mode: shell-script
# sh-basic-offset: 8
# tab-width: 8
# End:
# vi: set sw=8 ts=8
