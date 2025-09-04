#
# This file is named 'Makefile' as that is where my muscle memory
# defaults to (Ma<tab>) although this needs GNU Make (gmake) to work)
#
# Enter `make` on command line to get help.
#

.ONESHELL: # gnu make >= 3.82 feature
ifneq ($(wildcard /bin/dash),)
SHELL = /bin/dash
else
SHELL = /bin/sh
endif
.SHELLFLAGS = -eufc
#.SHELLFLAGS = -xeufc
.NOTPARALLEL:
.SILENT:
.SUFFIXES:
MAKEFLAGS += --no-builtin-rules --no-builtin-variables
MAKEFLAGS += --warn-undefined-variables
unexport MAKEFLAGS

override MAKEFILE := $(strip $(MAKEFILE_LIST))# hint: make -pn -f /dev/null
# fyi: more .PHONYs, less *stat(2)s
.PHONY: $(MAKEFILE)

#$(error $(MAKECMDGOALS))
override CMD := $(firstword $(MAKECMDGOALS))
ifdef CMD
override ARGS := $(wordlist 2, 9999, $(MAKECMDGOALS))
ifdef ARGS
.PHONY: $(filter-out $(CMD), $(ARGS))
$(filter-out $(CMD), $(ARGS)): prexit0
endif
endif

.DEFAULT_GOAL = help
.PHONY: prexit0
prexit0: ; $(eval override .VARIABLES :=)
# using .VARIABLES is hack to "disable" vars.

C = @{u}

override define sh =
test '$(.VARIABLES)' || exit 0
#set -x
die () { printf '%s\n' '' "$$@" ''; exit 1; } >&2
x () { printf '+ %s\n' "$$*" >&2; "$$@"; }
x_eval () { printf '+ %s\n' "$$*" >&2; eval "$$*"; }
x_exec () { printf '+ %s\n' "$$*" >&2; exec "$$@"; }
set_dwt () {
	set -- `git -C $(XR_LUD) log -1 --abbrev=7 --format='%ci %h' $C`
	test $$# -ge 4 || exit 15
	wt=wt-$$1-$$4
}
endef

.PHONY: help # not listed
help:
	$(sh)
	echo Commands:
#	#set -x
	sed -n '/^\([.]\|[a-z][^ ]*\):/ {
		s/%XR_LUD/$(XR_LUD)/; s/^/  /; s/#//p; }' '$(MAKEFILE)'
	printf %s\\n '' 'Note: recipes may "eval" code given in args...' \
	"Note: '.' in args is for \"default\" option (be it sfx or wt...)" ''

XR_LUD = xr-labwc-upstream

.PHONY: fetch
fetch:  # git fetch -v -p changes in %XR_LUD/
	$(sh)
	td=$(XR_LUD)
	test -d $(XR_LUD) ||
	  die "'./$(XR_LUD)/' does not exist - clone or symlink (ln -sr ...)"
	x_exec git -C $(XR_LUD) fetch -v -p

.PHONY: mkwt
mkwt:   # make worktree from @{u} (or C=...) commit in %XR_LUD
	$(sh)
	set -- $(ARGS)
	test $$# = 1 || die "Usage: make $@ (wt-sfx | '.')"
	set_dwt
	test "$$1" = . || wt=$$wt-$$1
	test -e $$wt && die "'$$wt/' exists - created already (expected?)" '' \
		"Use another 'wt-sfx' for another worktree, or execute" \
		"  git -C $(XR_LUD) worktree remove $$wt" \
		'in preparation to recreate current worktree.'
	x git -C $(XR_LUD) worktree add "$$PWD/$$wt" $C
	x_exec git -C $(XR_LUD) worktree list

.PHONY: lswt
lswt:   # list worktrees
	$(sh)
	x_exec git -C $(XR_LUD) worktree list

.PHONY: apply
apply:  # apply patches from ./11-patches/ to worktree
	$(sh)
	test '$(ARGS)' || die "Usage: make $@ {worktree | '.'}"
	test '$(ARGS)' = . && set_dwt || wt='$(ARGS)'
	toolsd=$(CURDIR); toolsd=$${toolsd%/*}/tools
	x cd "$$wt"
	tcs=`git log -1 --format=%s`
	case $$tcs in +*top*commit*+)
	  die "$$wt head: '$$tcs' - already applied" \
		':; make mkwt . ;: for hints'
	esac
	set +f
	echo .../git-am-reproducibly.pl '../11-patches/00*.patch' >&2
	$$toolsd/git-am-reproducibly.pl $(CURDIR)/11-patches/00*.patch
	echo ':; cd' $$wt
	exec git describe --dirty

.PHONY: fwa
fwa:    # * shortcut fetch, mkwt and apply (@{u} or C=...) *
	test '$(.VARIABLES)' || exit 0
	$(MAKE) fetch $(MAKEOVERRIDES)
	$(MAKE) mkwt . $(MAKEOVERRIDES)
	exec \
	$(MAKE) apply . $(MAKEOVERRIDES)

.PHONY: lspz
lspz:   # list patches if arg: # of lines (e.g. 1, 4, 999)
	test '$(.VARIABLES)' || exit 0
	set +f
	test '$(ARGS)' || exec stat -c '%5s %n' 11-patches/*
	cd 11-patches
	exec head -'$(ARGS)' 0*

.PHONY: l
l:      # git log --pretty=fuller --name-status -m  in worktree / . / @{u}
	$(sh)
	test '$(ARGS)' || die "Usage: make $@ {worktree/ | '.' | u}"
	test '$(ARGS)' = . && set_dwt || wt='$(ARGS)'
	test '$(ARGS)' = u && { wt=$(XR_LUD); a=$C; } || a=
	x_exec git -C "$$wt" log --pretty=fuller --name-status -m $$a

.PHONY: t
t:      # tig in worktree / . / @{u}  (fyi: shows also "describe" info)
	$(sh)
	test '$(ARGS)' || die "Usage: make $@ {worktree/ | '.' | u}"
	test '$(ARGS)' = . && set_dwt || wt='$(ARGS)'
	test '$(ARGS)' = u && { wt=$(XR_LUD); a=$C; } || a=
	x_exec tig -C "$$wt" $$a

.PHONY: prune
prune:  # prune worktrees removed with rm(1)
	test '$(.VARIABLES)' || exit 0
	set -x
	git -C $(XR_LUD) worktree prune -v
	git -C $(XR_LUD) worktree list

.PHONY: .
.:      # run $(MAKE) in "default" (.) worktree
	$(sh)
	set_dwt
	echo cd $$wt
	exec $(MAKE) -C "$$wt" $(ARGS) --no-print-directory

#.PHONY: cmds
#cmds:   # echo some useable commands
#	echo


.PHONY: vars. # not listed
vars.:
	$(foreach v,$(.VARIABLES),$(info $(origin $v) $(flavor $v) $v = $($v)))

.PHONY: s
s:      # show code of any of the commands listed here
	$(sh)
	echo
	e=
	for a in $(ARGS)
	do test "$$a" = . && a='[.]'
	   e=$$e"/^$$a"':/,/^$$/p;'
	done
	exec sed -n "$$e" '$(MAKEFILE)'

# Local variables:
# mode: makefile
# End:
