
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
ZIPLIST = build/krill/*.* build/standard/*.* $(README)
PRGLIST = -write $(MAIN).prg $(MAIN) -write $(LMC).prg $(LMC)
KRILLLIST = -write install-c128.prg install-c128 -write loader-c128.prg loader-c128
# TSCrunch-compressed replacements for real assets, loaded via
# krill_loadcompd() instead of krill_load()/bnk_load() -- krill-variant d81
# only (the standard/non-KRILL binary has no decompressor and still needs
# the raw file from $(ASSETS)). Each entry here is produced by baking the
# real load destination into a copy of the source asset's own 2-byte PRG
# header, then running it through krill/loader/tools/tscrunch (-i) and
# krill/loader/tools/compressedfileconverter.pl -- see krill_manual.md.
KRILL_COMPRESSED_ASSETS = -write idi8bcmp idi8bcmp \
         -write titletpk titletpk -write titlebtk titlebtk \
         -write flibitk flibitk -write flicolk flicolk \
         -write ihflictk ihflictk -write ihflicbk ihflicbk \
         -write ihflibtk ihflibtk -write ihflibbk ihflibbk \
         -write itflictk itflictk -write itflicbk itflicbk \
         -write itflibtk itflibtk -write itflibbk itflibbk \
         -write hflibitk hflibitk -write hflicolk hflicolk \
         -write imonotpk imonotpk -write imonobtk imonobtk \
         -write im800btk im800btk -write im800bbk im800bbk \
         -write im960btk im960btk -write im960bbk im960bbk
# vdcfli-orig.*/vdcimono-orig.*: temporary diagnostic pictures (Tokra's own
# "merida"/"avril", copied verbatim from original/v12/sd2iec-version/) --
# remove once fli_color_demo()/mono_hires_xl_demo() are switched back to
# vdcfli.*/vdcimono.* (see the "DIAGNOSTIC SWAP" comments in src/main.c).
ASSETS = -write vdce-scrtit.top vdce-scrtit.top -write vdce-scrtit.bot vdce-scrtit.bot \
         -write vdcfli.bit vdcfli.bit -write vdcfli.col vdcfli.col \
         -write vdcimono.top vdcimono.top -write vdcimono.bot vdcimono.bot \
         -write vdcfli-orig.bit vdcfli-orig.bit -write vdcfli-orig.col vdcfli-orig.col \
         -write vdcimono-orig.top vdcimono-orig.top -write vdcimono-orig.bot vdcimono-orig.bot \
         -write vdcihfli.ct vdcihfli.ct -write vdcihfli.cb vdcihfli.cb \
         -write vdcihfli.bt vdcihfli.bt -write vdcihfli.bb vdcihfli.bb \
         -write vdcitfli.ct vdcitfli.ct -write vdcitfli.cb vdcitfli.cb \
         -write vdcitfli.bt vdcitfli.bt -write vdcitfli.bb vdcitfli.bb \
         -write vdchfli.bit vdchfli.bit -write vdchfli.col vdchfli.col \
         -write vdcim800.bt vdcim800.bt -write vdcim800.bb vdcim800.bb \
         -write vdcim960.bt vdcim960.bt -write vdcim960.bb vdcim960.bb \
         -write idi8blogo.scrn idi8blogo.scrn

# Generated picture assets (see tools/vdc_convert.py) -- regenerated from
# assets/source/*.png automatically when python3 is available; if not, the
# build proceeds with a warning and whatever converted files already exist.
GENERATED_ASSETS = assets/vdcfli.bit assets/vdcfli.col assets/vdcimono.top assets/vdcimono.bot \
                   assets/vdce-scrtit.top assets/vdce-scrtit.bot

########################################

.SUFFIXES:
.PHONY: all clean deploy deploy2 check-deploy check-deploy2 docs vice vice-stnd krill

all: $(MAIN).prg bootsect.bin d81 README.pdf

$(MAIN).prg: $(MAIN_SRCS)
	@$(MKDIR) build/standard 2>$(NULLDEV) ; true
	@$(MKDIR) build/krill 2>$(NULLDEV) ; true
	$(CC) $(CFLAGS) -dKRILL -n -o=build/krill/$(MAIN).prg $(MAINSRC)
	$(CC) $(CFLAGS) -n -o=build/standard/$(MAIN).prg $(MAINSRC)

bootsect.bin: $(MAIN).prg $(GENERATED_ASSETS)
	@$(MKDIR) build/standard 2>$(NULLDEV) ; true
	@$(MKDIR) build/krill 2>$(NULLDEV) ; true
	$(CC) -tf=bin -rt=src/bootsect.c -o=build/standard/bootsect.bin
	cp build/standard/bootsect.bin build/krill
	cp assets/vdce-scr*.* build/standard
	cp assets/vdcfli*.* assets/vdcimono*.* build/standard
	cp assets/vdcihfli.* assets/vdcitfli.* assets/vdchfli.* assets/vdcim800.* assets/vdcim960.* build/standard
	cp assets/idi8blogo.scrn build/standard
	# build/krill only needs idi8blogo.scrn as the raw reference for
	# krill_loadcompd_test()'s diagnostic (see src/main.c) -- every real
	# picture asset is now loaded via krill_loadcompd() from the
	# TSCrunch-compressed files below instead (KRILL_COMPRESSED_ASSETS),
	# so the raw vdce-scr*/vdcfli*/vdcimono*/vdcihfli*/vdcitfli*/vdchfli*/
	# vdcim800*/vdcim960* files are NOT copied here -- the krill-variant d81
	# doesn't have room for both copies of ~20 pictures, and the
	# krill-compiled binary never reads them raw any more.
	cp assets/idi8blogo.scrn build/krill
	cp assets/idi8bcmp assets/titletpk assets/titlebtk assets/flibitk assets/flicolk \
	   assets/ihflictk assets/ihflicbk assets/ihflibtk assets/ihflibbk \
	   assets/itflictk assets/itflicbk assets/itflibtk assets/itflibbk \
	   assets/hflibitk assets/hflicolk assets/imonotpk assets/imonobtk \
	   assets/im800btk assets/im800bbk assets/im960btk assets/im960bbk build/krill
#	cp assets/music*.prg build/standard
#	cp assets/chars*.prg build/standard
#	cp assets/music*.prg build/krill
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

# asset-loading-roadmap.md Phase 4: builds a full testable krill-variant d81
# (build/krill/$(MAIN)-krill.d81) -- the -dKRILL-compiled binary, Krill's own
# install/loader prgs, idi8blogo.scrn (the one raw file still needed, as the
# reference asset for krill_loadcompd_test()'s diagnostic), and the
# TSCrunch-compressed real assets ($(KRILL_COMPRESSED_ASSETS)) every demo
# section actually loads now via krill_loadcompd() -- NOT the raw $(ASSETS)
# list the standard build uses: this binary never calls krill_load()/
# bnk_load() for any real picture any more, and the disk doesn't have room
# for both a raw and compressed copy of ~20 pictures. Deliberately does not
# touch all/d81/vice -- those keep building/running only the standard
# (non-KRILL) variant, so the existing day-to-day workflow is unaffected.
krill: $(MAIN).prg bootsect.bin loader-c128.prg
	c1541 -cd build/krill -format "$(MAIN),xm" d81 $(MAIN)-krill.d81
	c1541 -cd build/krill -attach $(MAIN)-krill.d81 -bwrite bootsect.bin 1 0
	c1541 -cd build/krill -attach $(MAIN)-krill.d81 -bpoke 40 1 16 $$27 %11111110
	c1541 -cd build/krill -attach $(MAIN)-krill.d81 -bam 1 1
	c1541 -cd build/krill -attach $(MAIN)-krill.d81 $(PRGLIST) $(KRILLLIST) -write idi8blogo.scrn idi8blogo.scrn $(KRILL_COMPRESSED_ASSETS)

d81: $(MAIN).prg bootsect.bin
	c1541 -cd build/standard -format "$(MAIN),xm" d81 $(MAIN)-stnd.d81
	c1541 -cd build/standard -attach $(MAIN)-stnd.d81 -bwrite bootsect.bin 1 0
	c1541 -cd build/standard -attach $(MAIN)-stnd.d81 -bpoke 40 1 16 $27 %11111110
	c1541 -cd build/standard -attach $(MAIN)-stnd.d81 -bam 1 1
	c1541 -cd build/standard -attach $(MAIN)-stnd.d81 $(PRGLIST) $(ASSETS)

## Creating ZIP file for distribution
#$(ZIP):
#	zip -j $(ZIP) build/krill/*.d* build/standard/*.d* $(README)
#

# Converted picture assets (requires python3 + Pillow: pip install Pillow).
# See tools/vdc_convert.py for the conversion technique (credited there).
assets/vdcfli.bit assets/vdcfli.col: assets/source/vdcfli-source.png tools/vdc_convert.py
	@if which python3 >/dev/null 2>&1; then \
		python3 tools/vdc_convert.py --mode fli --input assets/source/vdcfli-source.png --out-prefix assets/vdcfli; \
	else \
		echo "WARNING: python3 not found -- assets/vdcfli.bit/.col not regenerated"; \
	fi

assets/vdcimono.top assets/vdcimono.bot: assets/source/vdcimono-source.png tools/vdc_convert.py
	@if which python3 >/dev/null 2>&1; then \
		python3 tools/vdc_convert.py --mode imono --input assets/source/vdcimono-source.png --out-prefix assets/vdcimono; \
	else \
		echo "WARNING: python3 not found -- assets/vdcimono.top/.bot not regenerated"; \
	fi

assets/vdce-scrtit.top assets/vdce-scrtit.bot: assets/original/vdc_maniac_title.png tools/vdc_convert.py
	@if which python3 >/dev/null 2>&1; then \
		python3 tools/vdc_convert.py --mode titlescreen --input assets/original/vdc_maniac_title.png --out-prefix assets/vdce-scrtit; \
	else \
		echo "WARNING: python3 not found -- assets/vdce-scrtit.top/.bot not regenerated"; \
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
	$(DEL) build/standard/*.* 2>$(NULLDEV)
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
deploy: check-deploy $(MAIN).prg
	wput -u build/standard/*.prg build/standard/$(MAIN)-stnd.d* $(ULTFTP1)

deploy2: check-deploy2 $(MAIN).prg
	wput -u build/standard/*.prg build/standard/$(MAIN)-stnd.d* $(ULTFTP2)

## To run software using VICE x128 -- krill (fast loader) build by default,
## since that's the variant actually meant to be tested/deployed going
## forward; make vice-stnd runs the plain bnk_load()-based build instead.
vice: krill
	x128 build/krill/$(MAIN)-krill.d81

vice-stnd: d81
	x128 build/standard/$(MAIN)-stnd.d81
