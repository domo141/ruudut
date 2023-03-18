
# Enter `make` on command line to get help.

.ONESHELL: # gnu make >= 3.82 feature
SHELL = /bin/dash
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

override define sh =
test '$(.VARIABLES)' || exit 0
#set -x
die () { printf '%s\n' '' "$$@" ''; exit 1; } >&2
x () { printf '+ %s\n' "$$*" >&2; "$$@"; }
x_eval () { printf '+ %s\n' "$$*" >&2; eval "$$*"; }
x_exec () { printf '+ %s\n' "$$*" >&2; exec "$$@"; }
endef

# a few simple sample functions

.PHONY: help
help:   # helep
	$(sh)
	echo Commands:
#	#set -x
	sed -n '/^[a-z][^ ]*:/ {
		s/%XX_WT/$(XX_WT)/; s/%XX_BD/$(XX_BD)/;
		s/^/  /; s/#//p; }' '$(MAKEFILE)'
	printf %s\\n '' 'Note: recipes may "eval" code given in args...'

XX_BD = bare-xxdiff-qt3
XX_WT = wt-1-9ee

# to define, and not to take from env (expect url= from command line)
url =
ifneq (,$(findstring ',$(url))) #'
$(error url "$(url)" contains "'" char(s))
endif

#commit 9ee2fd4e ref'erred as "origin/qt3" in origin (2024-01-20)
COMMIT = 9ee2fd4e7a096b1e1ab4367ab60328283a1c77a9

.PHONY: clone
clone:  # clone xxdiff repo - commit 9ee2fd4e7 (branch: qt3), and checkout wt
	$(sh)
	td=$(XX_BD)
	test -d $(XX_BD) && die "'./$(XX_BD)/' exists - cloned already? (rewt?)"
	test -d $(XX_BD).wip &&
		die "'./$(XX_BD).wip/' exists, remove before retrying..."
	test '$(url)' || die 'Usage: make $@ url={xxdiff-git-repo}'
	x git clone --bare -n --filter=tree:0 '$(url)' $(XX_BD).wip
	x mv $(XX_BD).wip $$td
	x git --git-dir=$(XX_BD) worktree add $(XX_WT) $(COMMIT)
	echo Worktree at $$PWD/$(XX_WT)/.
	cd $(XX_BD)
	mv HEAD 0ldHEAD
	echo $(COMMIT) > HEAD # note: 'master' is still same as remote master

.PHONY: apply
apply:  # apply patches from ./11-patches/ to ./%XX_WT/
	$(sh)
	set +f
	toolsd=$(CURDIR); toolsd=$${toolsd%/*/*}/tools
	x cd $(XX_WT)
	x_exec $$toolsd/git-am-reproducibly.pl $(CURDIR)/11-patches/000*.patch

.PHONY: log
log:    # git log --pretty=fuller --name-status -m ... in %XX_WT/
	set -x
	cd $(XX_WT)
	git log --pretty=fuller --name-status -m $(ARGS) || :


# here no ' checks -- vars not announced in helps
#QTDIR = /usr/lib64/qt-3.3
# v "deferred simple variable expansion" v #
QTDIR = $(eval QTDIR := $$(shell pkg-config --variable=prefix qt-mt))$(QTDIR)
QMAKE = $(QTDIR)/bin/qmake

.PHONY: build
build:  # build, tries to use pkg-config to find path to QTDIR and QMAKE
	$(sh)
	test '$(QTDIR)' || die "pkg-config ... qt-mt did not find anything" \
	'' 'Enter  make build QTDIR=...'
	(
	x cd $(XX_WT)/src
	x make -f Makefile.bootstrap makefile QTDIR=$(QTDIR) QMAKE=$(QMAKE)
#	# mangle verzion
#	#gd=`git describe --all --match '*qt3' HEAD`; gd=$${gd##*/}
	v () {  set -- `git log --abbrev=7 --format=%h $(COMMIT)..`
		test $$# = 0 && die 'apply?'
		gd=$$#-g9ee-$$1
	}; v
	sed -i '/XX_VERSION/ s/N`/N`'"-$$gd/" Makefile
	test -e version.h && rm version.h
	x_exec script -ec 'exec make QTDIR=$(QTDIR)'
	)
	pwd
	x strip -o xxdiff3t $(XX_WT)/bin/xxdiff
	x ls -go xxdiff3t

.PHONY: reset
reset:  # echo info of the ways to reset (as we forget... (TBD))
	echo forgot already... refresh tbd

.PHONY: rewt
rewt:   # recreate the worktree -- %XX_WT/ -- from %XX_BD/
	$(sh)
	test -e $(XX_WT) && {
		(cd $(XX_WT) && x_exec git show > GIT-SHOW)
		x mv -T $(XX_WT) $(XX_WT),`date +%Y%m%d-%H%M`
		(cd $(XX_BD) && x_exec git worktree prune)
	}
#	#die "'$$PWD/$(XX_WT)' exists"
	git --git-dir=$(XX_BD) worktree remove $(XX_WT) || :
	x git --git-dir=$(XX_BD) worktree add $(XX_WT) $(COMMIT)
	echo worktree at $$PWD/$(XX_WT)/.

.PHONY: mkxwt
mkxwt:  # make another worktree from given commit (for temporary use)
	$(sh)
	test -d $(XX_BD) || die "'$(XX_BD)/': does not exist - make clone first"
	test '$(ARGS)' || die "Usage: make $@ {cref}"
	cref='$(firstword $(ARGS))'
	IFS=' -:'
	set -- `git -C $(XX_BD) log -1 --abbrev=7 --format='%ci %h' "$$cref"`
	IFS=' '
	dir=wt-"$$1"$$2"$$3"-"$$4"$$5"$$6"-g"$$8"
	test -d $$dir && die "'$$dir' exists. remove for rebuild"
	x git -C $(XX_BD) worktree add $(CURDIR)/$$dir "$$cref"


.PHONY: vars.
vars.:  # list make .VARIABLES
	$(foreach v,$(.VARIABLES),$(info $(origin $v) $(flavor $v) $v = $($v)))

.PHONY: source
source: # show code of any of the commands listed here
	$(sh)
	echo
	e=
	for a in $(ARGS)
	do e=$$e"/^$$a"':/,/^$$/p;'
	done
	exec sed -n "$$e" '$(MAKEFILE)'

# Local variables:
# mode: makefile
# End:
