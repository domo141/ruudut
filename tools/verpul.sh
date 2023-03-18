#!/bin/sh

set -euf  # hint: (da|ba|z|'')sh -x thisfile [args] to trace execution

q=false fth=false
while test $# -ge 1
do case $1 in -q | q ) q=true; shift
	   ;; -4 | 4 ) fth=true; shift
	   ;; *) break
   esac
done

test $# = 0 || set -- --date "$*"
secs=
eval `date -u "$@" +'secs=%s ye=%Y mo=%m da=%d wdn=%a ho=%H mi=%M se=%S'`
test "$secs"
date="$ye-$mo-$da ($wdn) $ho:$mi:$se"z

#secs=$(( 622800 * 1297 * 3 ))

# 3-char "version" changes every 5 hours -- and shifts one hour every day
# 'rotation' times: 1 week 12h, 38 weeks 4 days, and 26 years ~32weeks
dv=$((secs / 3600 / 5))

$q && s= || s="$secs $dv $date -- "

vchr () {
	case $1 in [0-9]) s=$s$1; return; esac
	set $1 a b c d e f g h i j k l m n o p q r s t u v w x y z
	shift $(($1 - 9))
	s=$s$1
}

vchr $((dv / 1296 % 36))
vchr $((dv / 36 % 36))
vchr $((dv % 36))

$q && { $fth && vchr $(((secs / 500) % 36)); echo $s; exit; }

s=$s' '

vchr $((dv / 1296 % 36))
vchr $((dv / 36 % 36))
vchr $((dv % 36))
vchr $(((secs / 500) % 36))

s=$s' - '

dv=$((secs / 3600 / 173)) # 7 days, 5 hours

vchr $((dv / 36 % 36))
vchr $((dv % 36))

ye=$((ye % 10))

VCHR () {
	mo=$((1$mo - 100))
	set 0 A B C D E F G H I J K L
	shift $mo
	mo=$1
}; VCHR

s=$s' - '$ye$mo-$da

printf '\n%s\n\n' "$s"
