
# vdcmodemania-oscar64
# Remake of VDC Mode Mania for the Commodore 128 VDC (80-column) chip
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
MAIN = vdcexp
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

# Fastload compile flag definitions (not yet wired into `all` -- see commented
# d64/d71/krill/flossiec targets below, which are a work in progress)
FLOSSIECFLAGS = -dFLOSSIEC -dFLOSSIEC_BORDER=1 -dFLOSSIEC_NODISPLAY=1 -dFLOSSIEC_NOIRQ=0 -dFLOSSIEC_CODE=bcode1 -dFLOSSIEC_BSS=bbss1

# Sources
MAINSRC = src/main.c

# All sources reachable via #pragma compile chains from src/main.c.
# Listed here so make rebuilds when any header or library changes.
# NB: include/vdc_menu.c, vdc_softscroll.c, vdc_textscroller.c and krill.c
# are present in include/ but not yet #include-d from main.c -- add them
# here once main.c starts pulling them in.
MAIN_SRCS = src/main.c \
            include/banking.c include/vdc_core.c \
            include/vdc_win.c include/vdc_raster.c \
            include/defines.h include/banking.h \
            include/vdc_core.h include/vdc_win.h \
            include/vdc_raster.h include/peekpoke.h

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
ZIPLIST = build/flossiec/*.* build/krill/*.* build/standard/*.* $(README)
PRGLIST = -write $(MAIN).prg $(MAIN) -write $(LMC).prg $(LMC)
KRILLLIST = -write install-c128.prg install-c128 -write loader-c128.prg loader-c128
ASSETS = -write vdce-scrtit.top vdce-scrtit.top -write vdce-scrtit.bot vdce-scrtit.bot \
         -write vdcfli.bit vdcfli.bit -write vdcfli.col vdcfli.col \
         -write vdcimono.top vdcimono.top -write vdcimono.bot vdcimono.bot

# Generated picture assets (see tools/vdc_convert.py) -- regenerated from
# assets/source/*.png automatically when python3 is available; if not, the
# build proceeds with a warning and whatever converted files already exist.
GENERATED_ASSETS = assets/vdcfli.bit assets/vdcfli.col assets/vdcimono.top assets/vdcimono.bot

########################################

.SUFFIXES:
.PHONY: all clean deploy deploy2 check-deploy check-deploy2 docs vice

all: $(MAIN).prg bootsect.bin d81 README.pdf
#all: $(MAIN).prg bootsect.bin loader-c128.prg d64 d71 d81 $(ZIP)

$(MAIN).prg: $(MAIN_SRCS)
	@$(MKDIR) build/standard 2>$(NULLDEV) ; true
#	$(CC) $(CFLAGS) $(FLOSSIECFLAGS) -n -o=build/flossiec/$(MAIN).prg $(MAINSRC)
#	$(CC) $(CFLAGS) -dKRILL -n -o=build/krill/$(MAIN).prg $(MAINSRC)
	$(CC) $(CFLAGS) -n -o=build/standard/$(MAIN).prg $(MAINSRC)

bootsect.bin: $(MAIN).prg $(GENERATED_ASSETS)
	@$(MKDIR) build/standard 2>$(NULLDEV) ; true
	$(CC) -tf=bin -rt=src/bootsect.c -o=build/standard/bootsect.bin
#	cp build/standard/bootsect.bin build/krill
#	cp build/standard/bootsect.bin build/flossiec
	cp assets/vdce-scr*.* build/standard
	cp assets/vdcfli.* assets/vdcimono.* build/standard
#	cp assets/music*.prg build/standard
#	cp assets/chars*.prg build/standard
#	cp assets/vdce-scr*.prg build/krill
#	cp assets/music*.prg build/krill
#	cp assets/chars*.prg build/krill
#	cp assets/vdce-scr*.prg build/flossiec
#	cp assets/music*.prg build/flossiec
#	cp assets/chars*.prg build/flossiec

#loader-c128.prg:
#	cd krill/loader/; $(DEL) build/*.* 2>$(NULLDEV)
#	cd krill/loader/; make PLATFORM=c128 prg INSTALL=A000 RESIDENT=0b00 ZP=f5 PROJECT=
#	cd krill/loader/; $(RMDIR) build/intermediate 2>$(NULLDEV)
#	cd krill/loader/; $(DEL) build/transient*.* 2>$(NULLDEV)
#	cp krill/loader/build/*.prg build/krill
#
#d64:	bootsect.bin loader-c128.prg
#	c1541 -cd build/krill -format "$(MAIN),xm" d64 $(MAIN)-krill.d64
#	c1541 -cd build/krill -attach $(MAIN)-krill.d64 -bwrite bootsect.bin 1 0
#	c1541 -cd build/krill -attach $(MAIN)-krill.d64 -bpoke 18 0 4 $14 %11111110
#	c1541 -cd build/krill -attach $(MAIN)-krill.d64 -bam 1 1
#	c1541 -cd build/krill -attach $(MAIN)-krill.d64 $(PRGLIST) $(KRILLLIST) $(ASSETS)
#	c1541 -cd build/standard -format "$(MAIN),xm" d64 $(MAIN)-stnd.d64
#	c1541 -cd build/standard -attach $(MAIN)-stnd.d64 -bwrite bootsect.bin 1 0
#	c1541 -cd build/standard -attach $(MAIN)-stnd.d64 -bpoke 18 0 4 $14 %11111110
#	c1541 -cd build/standard -attach $(MAIN)-stnd.d64 -bam 1 1
#	c1541 -cd build/standard -attach $(MAIN)-stnd.d64 $(PRGLIST) $(ASSETS)
#	c1541 -cd build/flossiec -format "$(MAIN),xm" d64 $(MAIN)-fl.d64
#	c1541 -cd build/flossiec -attach $(MAIN)-fl.d64 -bwrite bootsect.bin 1 0
#	c1541 -cd build/flossiec -attach $(MAIN)-fl.d64 -bpoke 18 0 4 $14 %11111110
#	c1541 -cd build/flossiec -attach $(MAIN)-fl.d64 -bam 1 1
#	c1541 -cd build/flossiec -attach $(MAIN)-fl.d64 $(PRGLIST) $(ASSETS)
#
#
#d71:	bootsect.bin
#	c1541 -cd build/krill -format "$(MAIN),xm" d71 $(MAIN)-krill.d71
#	c1541 -cd build/krill -attach $(MAIN)-krill.d71 -bwrite bootsect.bin 1 0
#	c1541 -cd build/krill -attach $(MAIN)-krill.d71 -bpoke 18 0 4 $14 %11111110
#	c1541 -cd build/krill -attach $(MAIN)-krill.d71 -bam 1 1
#	c1541 -cd build/krill -attach $(MAIN)-krill.d71 $(PRGLIST) $(KRILLLIST) $(ASSETS)
#	c1541 -cd build/standard -format "$(MAIN),xm" d71 $(MAIN)-stnd.d71
#	c1541 -cd build/standard -attach $(MAIN)-stnd.d71 -bwrite bootsect.bin 1 0
#	c1541 -cd build/standard -attach $(MAIN)-stnd.d71 -bpoke 18 0 4 $14 %11111110
#	c1541 -cd build/standard -attach $(MAIN)-stnd.d71 -bam 1 1
#	c1541 -cd build/standard -attach $(MAIN)-stnd.d71 $(PRGLIST) $(ASSETS)
#
d81:
#	c1541 -cd build -attach $(PLASMA).d81 -write $(PLASMA).prg $(PLASMA) -write vdctestlmc.prg vdctestlmc
#	c1541 -cd build/krill -format "$(MAIN),xm" d81 $(MAIN)-krill.d81
#	c1541 -cd build/krill -attach $(MAIN)-krill.d81 -bwrite bootsect.bin 1 0
#	c1541 -cd build/krill -attach $(MAIN)-krill.d81 -bpoke 40 1 16 $27 %11111110
#	c1541 -cd build/krill -attach $(MAIN)-krill.d81 -bam 1 1
#	c1541 -cd build/krill -attach $(MAIN)-krill.d81 $(PRGLIST) $(KRILLLIST) $(ASSETS)
	c1541 -cd build/standard -format "$(MAIN),xm" d81 $(MAIN)-stnd.d81
	c1541 -cd build/standard -attach $(MAIN)-stnd.d81 -bwrite bootsect.bin 1 0
	c1541 -cd build/standard -attach $(MAIN)-stnd.d81 -bpoke 40 1 16 $27 %11111110
	c1541 -cd build/standard -attach $(MAIN)-stnd.d81 -bam 1 1
	c1541 -cd build/standard -attach $(MAIN)-stnd.d81 $(PRGLIST) $(ASSETS)

## Creating ZIP file for distribution
#$(ZIP):
#	zip -j $(ZIP) build/flossiec/*.d* build/krill/*.d* build/standard/*.d* $(README)
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
#	$(DEL) build/krill/*.* 2>$(NULLDEV)
#	$(DEL) build/flossiec/*.* 2>$(NULLDEV)
	$(DEL) build/standard/*.* 2>$(NULLDEV)
#	$(DEL) krill/loader/build/*.* 2>$(NULLDEV)

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
#	wput -u build/standard/*.prg build/standard/$(MAIN)-stnd.d* build/flossiec/$(MAIN)-fl.d* build/krill/$(MAIN)-krill.d* $(ULTFTP1)

deploy2: check-deploy2 $(MAIN).prg
	wput -u build/standard/*.prg build/standard/$(MAIN)-stnd.d* $(ULTFTP2)

## To run software using VICE x128
vice: $(MAIN).d81
	x128 build/standard/$(MAIN)-stnd.d81
