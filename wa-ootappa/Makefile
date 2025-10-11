
.SUFFIXES:
MAKEFLAGS += --no-builtin-rules --no-builtin-variables
MAKEFLAGS += --warn-undefined-variables
unexport MAKEFLAGS

# build dir -- fyi: changeable on make command line (i.e. not from env)
BD = build

#help: MAKEFLAGS += -q #did not work as desired (too late ?)
ifndef MAKECMDGOALS
MAKEFLAGS += -q
endif

.PHONY: help
help:
	$(info )
	$(info Enter)
	$(info : make all ;: to compile $(BD)/ootappa)
	$(info : make distclean ;: to remove $(BD)/)
	$(info : other, look inside $(MAKEFILE_LIST))
	$(info )

.PHONY: all
all: $(BD)/ootappa
	ls -l $<

N_CH = $(BD)/numbrle.ch

CC = cc
#CFLAGS = -std=c11 -ggdb -I$(BD)
CFLAGS = -std=c11 -O2 -I$(BD)

wl-protocols.h = $(BD)/xdg-shell-client-protocol.h
wl-protocols.c = $(BD)/xdg-shell-protocol.c

wl-protocols.h += $(BD)/wlr-layer-shell-unstable-v1-client-protocol.h
wl-protocols.c += $(BD)/wlr-layer-shell-unstable-v1-protocol.c

$(BD)/ootappa: ootappa.c $(wl-protocols.h) $(wl-protocols.c) $(N_CH)
	$(CC) $(CFLAGS) -o $@ $< $(wl-protocols.c) -lwayland-client -lrt

$(BD):
	mkdir -p $(BD)

$(N_CH): num.pbms.xz ./supp/coderle.pl | $(BD)
	./supp/coderle.pl $(BD)

vpath %.xml /usr/share/wayland-protocols/stable/xdg-shell/

$(BD)/%-client-protocol.h: %.xml | $(BD)
	wayland-scanner client-header $< $@

$(BD)/%-protocol.c: %.xml | $(BD)
	wayland-scanner private-code $< $@

.PHONY: cls # just for developer command line convenience(?)
cls:
	printf \\033\\143

.PHONY: clean
clean:
	rm -f $(wl-protocols.c) $(wl-protocols.h)

.PHONY: distclean
distclean: clean
	rm -rf $(BD)

# .txz useable when in vc repo tree
ifneq ($(wildcard ../tools/ztarfiles.pl),)
.PHONY: .txz
.txz:
	../tools/ztarfiles.pl xz ootappa git-clean
endif
