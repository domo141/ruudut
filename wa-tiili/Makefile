
.SUFFIXES:
MAKEFLAGS += --no-builtin-rules --no-builtin-variables
MAKEFLAGS += --warn-undefined-variables
unexport MAKEFLAGS

# build dir -- fyi: changeable on make command line (i.e. not from env)
BD = build
GD = $(BD)/gen.d
# make USE_WLR_LAYER_SHELL=0 to unuse
USE_WLR_LAYER_SHELL = 1

#help: MAKEFLAGS += -q #did not work as desired (too late ?)
ifndef MAKECMDGOALS
MAKEFLAGS += -q
endif

.PHONY: help
help:
	$(info )
	$(info Enter)
	$(info : make all ;: to compile $(BD)/tiili)
	$(info : make distclean ;: to remove $(BD)/)
	$(info : other, look inside $(MAKEFILE_LIST))
	$(info )

all: $(BD)/tiili

CC = cc
#CFLAGS = -ggdb -DUSE_WLR_LAYER_SHELL=$(USE_WLR_LAYER_SHELL)
CFLAGS = -std=c11 -O2 -DUSE_WLR_LAYER_SHELL=$(USE_WLR_LAYER_SHELL) -I$(GD)

wl-protocols.h = $(GD)/xdg-shell-client-protocol.h
wl-protocols.c = $(GD)/xdg-shell-protocol.c

ifneq ($(USE_WLR_LAYER_SHELL),0)
wl-protocols.h += $(GD)/wlr-layer-shell-unstable-v1-client-protocol.h
wl-protocols.c += $(GD)/wlr-layer-shell-unstable-v1-protocol.c
endif

wl-protocols-i.h = $(wl-protocols.h)
wl-protocols-e.h = $(wl-protocols.h)
wl-protocols-i.c = $(wl-protocols.c)
wl-protocols-e.c = $(wl-protocols.c)

wl-protocols-e.h += $(GD)/ext-workspace-v1-client-protocol.h
wl-protocols-e.c += $(GD)/ext-workspace-v1-protocol.c

$(BD)/tiili: tiili.c $(wl-protocols-e.h) $(wl-protocols-e.c) $(GD)/txts.ch
	$(CC) $(CFLAGS) -o $@ $< $(wl-protocols-e.c) -lwayland-client -lrt

$(BD):
	mkdir -p $@

$(GD): | $(BD)
	mkdir $@

#wl-protocols-e.h += $(GD)/xdg-shell-client-protocol.h
#wl-protocols-e.c += $(GD)/xdg-shell-protocol.c

vpath %.xml /usr/share/wayland-protocols/stable/xdg-shell/
vpath %.xml /usr/share/wayland-protocols/staging/ext-workspace/

$(GD)/%-client-protocol.h: %.xml | $(GD)
	wayland-scanner client-header $< $@

$(GD)/%-protocol.c: %.xml | $(GD)
	wayland-scanner private-code $< $@

$(GD)/txts.ch: | $(GD)
	$(info )
	$(warning Execute mk-txtimgs.pl to create $@)
	$(warning E.g. ; ./mk-txtimgs.pl $(GD)/ - 20 +2 w monospace)
	$(error then run $(MAKE) again *** )

# for reference #
.PHONY: srv-prots
srv-prots: $(GD)/xxx-workspace-index-me-unstable-v3-server-protocol.h

$(GD)/%-server-protocol.h: %.xml | $(GD)
	wayland-scanner server-header $< $@

M =
.PHONY: run
run: $(BD)/tiili
	setsid -f $< $M

WC = echo give WC=... on command line

.PHONY: vwoc  # view without comments (hacky imprecise approach but useful)
vwoc:         # useful to view wayland protocol header files
ifeq (vwoc, $(lastword $(MAKECMDGOALS)))
	@/bin/echo usage: make $@ filename.h
	@/bin/false
endif
	sed '/\/[*]/,/[*]\//'d $(lastword $(MAKECMDGOALS))

.PHONY: cls # just for developer command line convenience(?)
cls:
	printf \\033\\143

.PHONY: clean
clean:
	rm -r '$(GD)'

.PHONY: distclean
distclean: clean
	rm -r '$(BD)'

# "rotate" clean
.PHONY: rotclean
rotclean:
	@set -x; test -d $(BD) || exit 0; v5oos=`./verpul.sh -q -4` || exit; \
	test -e $(BD),$$v5oos && { : $(BD),$$v5oos exists; exit 1; } || \
	mv $(BD) $(BD),$$v5oos

# .txz useable when in vc repo tree (*/* - exclude subdirs)
ifneq ($(wildcard ../tools/ztarfiles.pl),)
.PHONY: .txz
.txz:
	../tools/ztarfiles.pl xz tiili git-clean '*/*'
endif
