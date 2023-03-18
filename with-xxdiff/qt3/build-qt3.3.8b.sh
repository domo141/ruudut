#!/bin/sh
#
# $ build-qt3.3.8b.sh $
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#	Copyright (c) 2024 Tomi Ollila
#	    All rights reserved
#
# Created: Mon 21 Oct 2024 22:53:12 EEST too
# Last modified: Mon 21 Oct 2024 23:49:32 +0300 too

case ${BASH_VERSION-} in *.*) set -o posix; shopt -s xpg_echo; esac
case ${ZSH_VERSION-} in *.*) emulate ksh; esac

set -euf  # hint: (z|ba|da|'')sh -x thisfile [args] to trace execution

LANG=C LC_ALL=C; export LANG LC_ALL; unset LANGUAGE

saved_IFS=$IFS; readonly saved_IFS

die () { printf '%s\n' '' "$@" ''; exit 1; } >&2

x () { printf '+ %s\n' "$*" >&2; "$@"; }
x_eval () { printf '+ %s\n' "$*" >&2; eval "$*"; }
x_exec () { printf '+ %s\n' "$*" >&2; exec "$@"; exit not reached; }

test $# = 1 || die "Usage: ${0##*/} path/to/qt-x11-free-3.3.8b.tar.gz" '' \
	": Tip; script -ec '$0 {file}'"

test -f "$1" || die "'$1': no such file"

xdg_data_home=${XDG_DATA_HOME:-$HOME/.local/share}
case $xdg_data_home in *["$IFS"]*) die "Whitespace in '$xdg_data_home'"; esac
qtdir=$xdg_data_home/qt-3.3.8b
test -d $qtdir && die "The (directory) '$qtdir' exists" \
"- move away for (re)build (or edit $0)."

sha256_file () {
	set -- `sha256sum "$1"`
	sha256=$1
}
sha256_file "$1"

exp_sha256=1b7a1ff62ec5a9cb7a388e2ba28fda6f960b27f27999482ebeceeadb72ac9f6e

test $sha256 = $exp_sha256 || die "'$1': Unexpected sha256:" \
	"expected: $exp_sha256" "actual:   $sha256"

qtsrcdir=qt-x11-free-3.3.8b

x mkdir $qtdir
x tar -C $qtdir -xf "$1"

x cd $qtdir/qtsrcdir

x_eval QTDIR=$PWD
x_eval 'PATH=$QTDIR/bin:$PATH'
x_eval 'MANPATH=$QTDIR/doc/man${MANPATH:+:$MANPATH}'
x_eval 'LD_LIBRARY_PATH=$QTDIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}'

x ./configure --prefix=$qtdir
x make
x make install
x ln -s libqt.so.3.3.8 $qtdir/lib/libqt-mt.so

# Local variables:
# mode: shell-script
# sh-basic-offset: 8
# tab-width: 8
# End:
# vi: set sw=8 ts=8
