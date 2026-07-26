
# vdcmaniac
# A modern Oscar64/C128 remake inspired by VDC Mode Mania (Tokra/Mike,
# Akronyme Analogiker, 2012) -- see README.md
# Written by Xander Mol

# Target
SYS = c128e

# Just the usual way to find out if we're
# using cmd.exe to execute make rules.
ifneq ($(shell echo),)
  CMD_EXE = 1
endif

ifdef CMD_EXE
  NULLDEV = nul:
  DEL     = -del /f
  RMDIR   = rmdir /s /q
  MKDIR   = mkdir
else
  NULLDEV = /dev/null
  DEL     = $(RM)
  RMDIR   = $(RM) -r
  MKDIR   = mkdir -p
endif

# Tooling paths
CC = /home/xahmol/oscar64/bin/oscar64

# Application names
MAIN = vdcmaniac
LMC  = vdcelmc

# Build versioning
VERSION_MAJOR     = 0
VERSION_MINOR     = 1
VERSION_TIMESTAMP = $(shell date "+%Y%m%d-%H%M")
VERSION           = v$(VERSION_MAJOR)$(VERSION_MINOR)-$(VERSION_TIMESTAMP)

# Common compile flags
#   -i=include       : add include/ to header search path
#   -tm=c128e        : target Commodore 128 80-column mode
#   -O2              : optimise
#   -dNOFLOAT        : disable float support (saves space)
#   -dVERSION        : pass version string to source
CFLAGS  = -i=include \
          -tm=$(SYS) \
          -O2 \
          -dNOFLOAT \
          -dVERSION="\"$(VERSION)\""

# Sources
MAINSRC = src/main.c

# All sources reachable via #pragma compile chains from src/main.c.
# Listed here so make rebuilds when any header or library changes.
# NB: include/vdc_menu.c, vdc_softscroll.c and vdc_textscroller.c are
# present in include/ but not yet #include-d from main.c -- add them here
# once main.c starts pulling them in. krill.c/.h are listed below even
# though main.c doesn't #include krill.h yet either (asset-loading-roadmap.md
# Phase 0: infrastructure only, no demo behaviour change) -- harmless now,
# and already in place for when Phase 1 wires krill_load() into a demo
# function's asset loading.
MAIN_SRCS = src/main.c \
            include/banking.c include/vdc_core.c \
            include/vdc_win.c include/vdc_raster.c \
            include/defines.h include/banking.h \
            include/vdc_core.h include/vdc_win.h \
            include/vdc_raster.h include/peekpoke.h \
            include/krill.c include/krill.h

# Deployment to Ultimate II+
# Set ULTIP1 (and optionally ULTIP2) in .env -- see README "Building from source"
-include .env
ULTIP1  ?= <set_ULTIP1_in_.env>
ULTUSB  ?= usb1
ULTPATH  = /$(ULTUSB)/temp/
ULTFTP1  = ftp://$(ULTIP1)$(ULTPATH)
ifdef ULTIP2
ULTFTP2  = ftp://$(ULTIP2)$(ULTPATH)
endif

# ZIP file contents
ZIP = build/$(MAIN)_$(VERSION).zip
README = README.pdf
ZIPLIST = build/krill/*.* $(README)
PRGLIST = -write $(MAIN).prg $(MAIN) -write $(LMC).prg $(LMC)
KRILLLIST = -write install-c128.prg install-c128 -write loader-c128.prg loader-c128
# TSCrunch-compressed real assets, loaded via krill_loadcompd() -- krill is
# the only build target (2026-07-26: the plain bnk_load()-based standard
# build was dropped entirely, so there is no separate raw-asset disk any
# more). Each entry here is produced by baking the real load destination
# into a copy of the source asset's own 2-byte PRG header, then running it
# through krill/loader/tools/tscrunch (-i) and
# krill/loader/tools/compressedfileconverter.pl -- see krill_manual.md.
KRILL_COMPRESSED_ASSETS = -write idi8bcmp idi8bcmp \
         -write titleevk titleevk -write titleodk titleodk \
         -write fli1bitk fli1bitk -write fli1colk fli1colk \
         -write fli2bitk fli2bitk -write fli2colk fli2colk \
         -write fli3bitk fli3bitk -write fli3colk fli3colk \
         -write ihfli1bek ihfli1bek -write ihfli1cek ihfli1cek -write ihfli1bok ihfli1bok -write ihfli1cok ihfli1cok \
         -write ihfli2bek ihfli2bek -write ihfli2cek ihfli2cek -write ihfli2bok ihfli2bok -write ihfli2cok ihfli2cok \
         -write ihfli3bek ihfli3bek -write ihfli3cek ihfli3cek -write ihfli3bok ihfli3bok -write ihfli3cok ihfli3cok \
         -write itfli1bek itfli1bek -write itfli1cek itfli1cek -write itfli1bok itfli1bok -write itfli1cok itfli1cok \
         -write itfli2bek itfli2bek -write itfli2cek itfli2cek -write itfli2bok itfli2bok -write itfli2cok itfli2cok \
         -write itfli3bek itfli3bek -write itfli3cek itfli3cek -write itfli3bok itfli3bok -write itfli3cok itfli3cok \
         -write hfli1btk hfli1btk -write hfli1clk hfli1clk \
         -write hfli2btk hfli2btk -write hfli2clk hfli2clk \
         -write hfli3btk hfli3btk -write hfli3clk hfli3clk \
         -write imono1evk imono1evk -write imono1odk imono1odk \
         -write imono2evk imono2evk -write imono2odk imono2odk \
         -write imono3evk imono3evk -write imono3odk imono3odk \
         -write im8001evk im8001evk -write im8001odk im8001odk \
         -write im8002evk im8002evk -write im8002odk im8002odk \
         -write im8003evk im8003evk -write im8003odk im8003odk \
         -write musick musick

# Generated picture assets (see tools/vdc_convert.py) -- regenerated from
# assets/source/*.png automatically when python3 is available; if not, the
# build proceeds with a warning and whatever converted files already exist.
# The 18-photo hires-mode set is deliberately NOT part of this automatic
# list -- see the make rule comment further down.
GENERATED_ASSETS = assets/vdce-scrtit.eve assets/vdce-scrtit.odd

########################################

.SUFFIXES:
.PHONY: all clean deploy deploy2 check-deploy check-deploy2 docs vice krill

all: $(MAIN).prg bootsect.bin krill README.pdf

$(MAIN).prg: $(MAIN_SRCS)
	@$(MKDIR) build/krill 2>$(NULLDEV) ; true
	$(CC) $(CFLAGS) -dKRILL -n -o=build/krill/$(MAIN).prg $(MAINSRC)

bootsect.bin: $(MAIN).prg $(GENERATED_ASSETS)
	@$(MKDIR) build/krill 2>$(NULLDEV) ; true
	$(CC) -tf=bin -rt=src/bootsect.c -o=build/krill/bootsect.bin
	# Every real picture asset is loaded via krill_loadcompd() from the
	# TSCrunch-compressed files below -- no raw picture files are shipped.
	cp assets/idi8bcmp assets/titleevk assets/titleodk \
	   assets/fli1bitk assets/fli1colk assets/fli2bitk assets/fli2colk assets/fli3bitk assets/fli3colk \
	   assets/ihfli1bek assets/ihfli1cek assets/ihfli1bok assets/ihfli1cok \
	   assets/ihfli2bek assets/ihfli2cek assets/ihfli2bok assets/ihfli2cok \
	   assets/ihfli3bek assets/ihfli3cek assets/ihfli3bok assets/ihfli3cok \
	   assets/itfli1bek assets/itfli1cek assets/itfli1bok assets/itfli1cok \
	   assets/itfli2bek assets/itfli2cek assets/itfli2bok assets/itfli2cok \
	   assets/itfli3bek assets/itfli3cek assets/itfli3bok assets/itfli3cok \
	   assets/hfli1btk assets/hfli1clk assets/hfli2btk assets/hfli2clk assets/hfli3btk assets/hfli3clk \
	   assets/imono1evk assets/imono1odk assets/imono2evk assets/imono2odk assets/imono3evk assets/imono3odk \
	   assets/im8001evk assets/im8001odk assets/im8002evk assets/im8002odk \
	   assets/im8003evk assets/im8003odk \
	   assets/musick build/krill
#	cp assets/chars*.prg build/krill

# asset-loading-roadmap.md Phase 4: builds Krill's loader with both
# LOAD_RAW_API and LOAD_COMPD_API (TSCrunch) enabled, via
# krill-config/loaderconfig.inc (EXTCONFIGPATH -- keeps the vendored krill/
# tree untouched). ZP=e0, not the original raw-only build's ZP=f5: the
# compressed-load API needs a 22-byte zero-page window, and $e0-$f5 was
# verified clear of Oscar64's own zero-page usage for the C128 target in
# this project's (non -xz) build (Oscar64's own default __zeropage region is
# $f7-$ff; its documented default compiler-register range for this target
# tops out at $8f -- see krill_manual.md). include/krill.h's KRILL_ZPSTART
# and the KRILLZP struct layout must match this build's ZP/config exactly.
# This one binary now serves both raw and compressed loads -- there is no
# separate raw-only loader build any more.
loader-c128.prg:
	@$(MKDIR) build/krill 2>$(NULLDEV) ; true
	cd krill/loader/; $(DEL) build/*.* 2>$(NULLDEV)
	cd krill/loader/; make PLATFORM=c128 prg INSTALL=A000 RESIDENT=0b00 ZP=e0 PROJECT= EXTCONFIGPATH=$(CURDIR)/krill-config
	cd krill/loader/; $(RMDIR) build/intermediate 2>$(NULLDEV)
	cd krill/loader/; $(DEL) build/transient*.* 2>$(NULLDEV)
	cp krill/loader/build/*.prg build/krill
	cp krill/loader/build/loadersymbols-c128.inc build/krill

# Builds the full testable krill d81 (build/krill/$(MAIN)-krill.d81) -- the
# only build target (2026-07-26: the plain bnk_load()-based standard build
# was dropped entirely) -- the -dKRILL-compiled binary, Krill's own
# install/loader prgs, and the TSCrunch-compressed real assets
# ($(KRILL_COMPRESSED_ASSETS)) every demo section loads via
# krill_loadcompd().
krill: $(MAIN).prg bootsect.bin loader-c128.prg
	c1541 -cd build/krill -format "$(MAIN),xm" d81 $(MAIN)-krill.d81
	c1541 -cd build/krill -attach $(MAIN)-krill.d81 -bwrite bootsect.bin 1 0
	c1541 -cd build/krill -attach $(MAIN)-krill.d81 -bpoke 40 1 16 $$27 %11111110
	c1541 -cd build/krill -attach $(MAIN)-krill.d81 -bam 1 1
	c1541 -cd build/krill -attach $(MAIN)-krill.d81 $(PRGLIST) $(KRILLLIST) $(KRILL_COMPRESSED_ASSETS)

## Creating ZIP file for distribution
#$(ZIP):
#	zip -j $(ZIP) build/krill/*.d* build/standard/*.d* $(README)
#

# Converted picture assets (requires python3 + Pillow: pip install Pillow).
# See tools/vdc_convert.py for the conversion technique (credited there).
#
# The 18-photo set (3 per hires mode -- fli1-3/hfli1-3/ihfli1-3/itfli1-3/
# imono1-3/im8001-2, see defines.h for sources/licenses) is NOT wired into
# an automatic make rule like titlescreen below: each conversion takes
# anywhere from ~20s (fli/imono) to ~20 minutes (itfli, cell_height=3) --
# auto-regenerating all 18 on every build (e.g. because a source PNG's
# mtime changed) would make ordinary `make` runs unpredictably slow. These
# are produced once, by hand (`python3 tools/vdc_convert.py --mode <mode>
# --input assets/source/<name> --out-prefix assets/<prefix>`), then
# TSCrunch-compressed (see krill_manual.md) and checked into `assets/` as
# committed binary artifacts, exactly like the KRILL_COMPRESSED_ASSETS list
# above already treats every other real picture -- redo only when a photo
# is actually being replaced, not on every build.
assets/vdce-scrtit.eve assets/vdce-scrtit.odd: assets/original/vdc_maniac_title.png tools/vdc_convert.py
	@if which python3 >/dev/null 2>&1; then \
		python3 tools/vdc_convert.py --mode titlescreen --input assets/original/vdc_maniac_title.png --out-prefix assets/vdce-scrtit; \
	else \
		echo "WARNING: python3 not found -- assets/vdce-scrtit.eve/.odd not regenerated"; \
	fi

# Regenerate README.pdf from README.md (requires pandoc).
# Install: sudo apt install pandoc texlive-xetex
docs: README.pdf

README.pdf: README.md pandoc-defaults.yaml pandoc-header.tex
	@if which pandoc >/dev/null 2>&1; then \
		pandoc --defaults=pandoc-defaults.yaml README.md -o README.pdf; \
	else \
		echo "WARNING: pandoc not found -- README.pdf not updated (install: sudo apt install pandoc texlive-xetex)"; \
	fi

# Cleaning repo of build files
clean:
	$(DEL) build/*.* 2>$(NULLDEV)
	$(DEL) build/krill/*.* 2>$(NULLDEV)
	$(DEL) krill/loader/build/*.* 2>$(NULLDEV)

# Check Ultimate II+ is reachable before deploying
check-deploy:
	@curl -s --connect-timeout 3 $(ULTFTP1)/ >/dev/null 2>&1 || \
		(echo "ERROR: Cannot reach U64 at $(ULTIP1) -- check ULTIP1 in .env" && false)

check-deploy2:
ifndef ULTIP2
	$(error ULTIP2 is not set in .env -- cannot deploy to second machine)
endif
	@curl -s --connect-timeout 3 $(ULTFTP2)/ >/dev/null 2>&1 || \
		(echo "ERROR: Cannot reach U64 at $(ULTIP2) -- check ULTIP2 in .env" && false)

# To deploy software to UII+ enter make deploy. Obviously C128 needs to be powered on with UII+ and USB drive connected.
deploy: check-deploy krill
	wput -u build/krill/*.prg build/krill/$(MAIN)-krill.d* $(ULTFTP1)

deploy2: check-deploy2 krill
	wput -u build/krill/*.prg build/krill/$(MAIN)-krill.d* $(ULTFTP2)

# To run software using VICE x128 -- krill is the only build target.
vice: krill
	x128 build/krill/$(MAIN)-krill.d81
