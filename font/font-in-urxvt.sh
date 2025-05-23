#!/bin/sh
#
# $ font-in-urxvt.sh $
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#	Copyright (c) 2023 Tomi Ollila
#	    All rights reserved
#
# Created: Sun 30 Apr 2023 13:40:18 EEST too
# Last modified: Sat 19 Apr 2025 20:24:57 +0300 too

# this hopefully helps someone to pick their font...

case ${BASH_VERSION-} in *.*) set -o posix; shopt -s xpg_echo; esac
case ${ZSH_VERSION-} in *.*) emulate ksh; esac

set -euf  # hint: (z|ba|da|'')sh -x thisfile [args] to trace execution

LANG=C.UTF-8 LC_ALL=C.UTF-8; export LANG LC_ALL; unset LANGUAGE

die () { printf '%s\n' '' "$@" ''; exit 1; } >&2

test $# -gt 0 || die "Usage: ${0##*/} [xft:]font [more urxvt options]"

x_eval () { printf '+ %s\n' "$*" >&2; eval "$*"; }

x_setsid_f () { printf '+ %s\n' "$*" >&2; setsid -f "$@"; }

if test $1 != -bg
then
	x_setsid_f urxvt +sb -b 0 -cr black -sl 0 -geometry 50x22 -fn "$@" \
		-e sh "$0" -bg "$@"
	exit
fi
shift

# font to window title
printf '\033]0;%s\007' "-fn $*"


out () {
	echo ' ' $*
	echo ' ' abcdefghijklmnopqrstuvwxyz '1234567890 []'
	echo ' ' ABCDEFGHIJKLMNOPQRSTUVWXYZ '!"#$%&/()= {}'
	echo
}

echo
printf "\033[1m" # bold
out 'bold'
printf "\033[0m" # normal
printf "\033[3m" # italic
out 'italic'
printf "\033[1m" # bold italic (add bold to italic)
out 'bold italic'
printf "\033[0m" # normal
out 'normal'

if command -v xprop >/dev/null
then
	( set -- `xprop -id $WINDOWID | \
		sed -n '/resize increment/ s/[^0-9][^0-9]*/ /gp'`
	  if test $# = 2
	  then echo "  resize increment: $1 x $2" \
		    "(80x24: $(($1 * 80)) x $(($2 * 24)))"
	  else x_eval 'xprop -id $WINDOWID | grep resize'
	  fi
	)
else
	echo "  'xprop' does not exist, cannot echo glyph size."
	echo "  Install xprop or suggest better alternative."
fi

echo
echo '  Press ENTER or Ctrl-C to exit.'
echo

read a

# Local variables:
# mode: shell-script
# sh-basic-offset: 8
# tab-width: 8
# End:
# vi: set sw=8 ts=8
