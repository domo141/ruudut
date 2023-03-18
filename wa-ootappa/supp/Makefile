.SUFFIXES:
MAKEFLAGS += --no-builtin-rules --no-builtin-variables
MAKEFLAGS += --warn-undefined-variables
unexport MAKEFLAGS

nums = 0 1 2 3 4 5 6 7 8 9

svgs = $(nums:%=numglyph-%.svg)
xbms = $(nums:%=numglyph-%.xbm)
pbms = $(nums:%=numglyph-%.pbm)
pgms = $(nums:%=numglyph-%.pgm)

.PHONY: help # this help
help:
	@echo try:
	@sed -n 's/[.]PHONY:/ make/p' Makefile
	@echo


.PHONY: svgs # do this first
svgs: $(svgs)
	@echo svgs done. other targets:
#	@echo make -j xbms
	@echo make -j pbms
	@echo make view '[z=$(z)]' '(and remember _ as flip)'

.PHONY: xbms
xbms: $(xbms)

.PHONY: pbms
pbms: $(pbms)

.PHONY: pgms
pgms: $(pgms)

z = 3
#FEH_OPTS=-m (on cli) (or -w or -l or -t or)
FEH_OPTS = --title='%wx%h %z %f' --zoom $(z)00 --force-aliasing
#FEH_OPTS += --reload 1
#FILES = $(xbms) $(svgs)
FILES = $(pbms) $(svgs)
.PHONY: view # views svgs & pbms by default
view:
	feh $(FEH_OPTS) $(FILES)

$(svgs) &: ./mk-glyphsvgs.pl
	./mk-glyphsvgs.pl

%.xbm: %.svg
	magick $< -flip $@

%.pbm: %.svg
	magick $< -flip $@

%.pgm: %.svg
	magick $< -flip $@

num.pbms.xz: $(pbms)
	cat $(pbms) | xz -9 > $@

.PHONY: clean
clean:
	@rm -fv $(svgs) $(xbms) $(pbms) $(pgms)

# Local variables:
# mode: makefile
# End:
