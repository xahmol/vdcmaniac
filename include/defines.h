/*
Oscar64 VDC function library

Written in 2024 by Xander Mol

https://github.com/xahmol/Oscar64Test

https://www.idreamtin8bits.com/

Code and resources from others used:

-   Oscar64 cross compiler

    https://github.com/drmortalwombat/oscar64

    Many thanks also to https://github.com/drmortalwombat to provide extrordinary support and tips for making this and adapting Oscar64 to my needs faster than I could ask it.

-   Screens used in the demo made with my own VDC Screen Editor.

    https://github.com/xahmol/VDCScreenEdit

-   Commodore logo charset created using CharPad Pro.

    https://subchristsoftware.itch.io/c64-pro-editions

-   C128 Programmers Reference Guide: For the basic VDC register routines and VDC code inspiration

    http://www.zimmers.net/anonftp/pub/cbm/manuals/c128/C128_Programmers_Reference_Guide.pdf

-   Tokra: For the optimal VDC registry settings for 80x50 and 80x70 textmodes,
    and for the VDC-FLI (480x252, 8x1 colour cells) and VDC-IMONO (720x700,
    interlace monochrome) register settings, from "VDC Mode Mania":

    https://github.com/xahmol/vdcmodemania-oscar64/tree/main/original/v12

    Also for "VDC SpectruMania" (2021, Akronyme Analogiker), used as a
    reference for VDC Spectrum mode (spectrum_demo()/convert_spectrum()):
    disassembling his scr-copy.bin conversion routine confirmed it writes
    no VDC horizontal/vertical timing registers at all, validating
    vdcmaniac's own choice to reuse VDC_HIRES_640x200_Color_PAL's
    already-proven timing unchanged rather than inventing new register
    values. No code or assets from this release are used -- own
    independent implementation, verified against the disassembly rather
    than copied from it:

    https://www.tokra.de/c128/vdcspectrumania.zip (conversion routine
    disassembled) / https://csdb.dk/release/?id=206013 (CSDb release)

-   "Colour Spectrum" (2021, Crest -- concept by Tokra, code by JackAsser,
    loader by Krill), 2nd place in the Mixed Demo Competition at
    Underground Conference 11: a second reference point for the idea of
    showing ZX Spectrum-sourced graphics on the C128's VDC, alongside
    VDC SpectruMania above, ahead of VDC Spectrum's own from-scratch real-
    .scr-dump decode. No code or assets from this release are used:

    https://csdb.dk/release/?id=205653

-   Michael Kircher: dithering/colour-cell technique (per-8-pixel-cell
    brute-force background/foreground search with Floyd-Steinberg error
    diffusion) studied from his 2011 vdc_quant.c/hfli_quant.c converters,
    bundled unmodified in this repo at original/v12/converters/source/ for
    reference. tools/vdc_convert.py is an independent reimplementation of
    the same published technique (those files carry no license permitting
    reuse of the code itself).

-   Turbo Rascal Syntax Error (TRSE) demoscene tooling/technique writeups
    (lemonspawn.com/turbo-rascal-syntax-error-expected-but-begin/): the
    sine-table plasma (plasma_demo()/init_plasma()) and colour-cycling
    (rotate_demo()/init_rotate()/rotup()/rotdown()) effects are the same
    underlying algorithm/code shown there, translated from TRSE's own
    Pascal-like source language into Oscar64 C for this project's own VDC
    bitmap/palette layout -- a port, not an independent reimplementation.

-   GoDot (github.com/godot64/GoDot), a C128 VDC paint/image-processing
    suite: its "BASIC 8"/iPaint picture-format saver/loader source
    (savers/s_IPaint.a, savers/s_Basic8Mode1.a, loaders/l_Basic8Select.a)
    is where the odd/even-interlace-field colour-blend technique
    `tools/vdc_convert.py`'s `convert_colour_cells_paired()` now defaults
    to for VDC-IFLI was identified and confirmed (against real
    disk-bundled iPaint sample pictures' own colour-index bytes, not
    copied code or reproduced picture content):

    https://github.com/godot64/GoDot/tree/712196dac1916ef6e7993c25e40903e1c531c0d1

Real photographs used as source pictures for the VDC hires-mode showcase
sections (3 per mode, cycled on keypress -- see main_menu()/fli_color_demo()
et al in src/main.c), converted via tools/vdc_convert.py:

-   VDC-FLI:

    Bold, distinct Van Gogh brushstroke colour blocks suit this mode's
    coarse 8x1 colour-cell resolution well:

    "Wheat Field with Cypresses" (1889), Vincent van Gogh, The Met, public
    domain (author died 1890):
    https://commons.wikimedia.org/wiki/File:Vincent_van_Gogh_-_Wheat_Field_with_Cypresses_-_Google_Art_Project.jpg

    "Starry Night Over the Rhone" (1888), Vincent van Gogh, Musee d'Orsay,
    public domain:
    https://commons.wikimedia.org/wiki/File:Vincent_van_Gogh_-_Starry_Night_-_Google_Art_Project_2.jpg

    "Vase with Irises Against a Yellow Background" (1890), Vincent van
    Gogh, Van Gogh Museum, public domain:
    https://commons.wikimedia.org/wiki/File:Vincent_van_Gogh_-_Irises_-_Google_Art_Project.jpg

-   VDC-HFLI:

    Public-domain Japanese ukiyo-e woodblock prints -- flat bold colour
    fields, well suited to this mode's colour-cell resolution:

    "The Great Wave off Kanagawa" (c. 1831), Katsushika Hokusai, public
    domain (author died 1849):
    https://commons.wikimedia.org/wiki/File:Katsushika_Hokusai_-_Thirty-Six_Views_of_Mount_Fuji-_The_Great_Wave_Off_the_Coast_of_Kanagawa_-_Google_Art_Project.jpg

    "Fine Wind, Clear Morning" ("Red Fuji", c. 1830-1832), Katsushika
    Hokusai, public domain:
    https://commons.wikimedia.org/wiki/File:Katsushika_Hokusai_-_Fine_Wind,_Clear_Morning_(Gaif%C5%AB_kaisei)_-_Google_Art_Project.jpg

    "Ejiri in Suruga Province" (c. 1830-1832), Katsushika Hokusai, public
    domain:
    https://commons.wikimedia.org/wiki/File:Ejiri_in_the_Suruga_province.jpg

-   VDC-IHFLI:

    "Passiflora caerulea (makro close-up).jpg" by Petar Milošević, CC BY-SA 4.0:
    https://commons.wikimedia.org/wiki/File:Passiflora_caerulea_(makro_close-up).jpg

    "Sonnenblume Helianthus 1.JPG" by Böhringer Friedrich, CC BY-SA 2.5:
    https://commons.wikimedia.org/wiki/File:Sonnenblume_Helianthus_1.JPG

    "Keel-billed Toucan - Ramphastos sulfuratus, Caves Branch Jungle Lodge,
    Belmopan, Belize.jpg" by Judy Gallagher, CC BY 2.0:
    https://commons.wikimedia.org/wiki/File:Keel-billed_Toucan_-_Ramphastos_sulfuratus,_Caves_Branch_Jungle_Lodge,_Belmopan,_Belize.jpg

-   VDC-ITFLI:

    "Tutanchamun Maske.jpg" (Tutankhamun funerary mask, front view) by
    MykReeve, CC BY-SA 3.0 / GFDL 1.2+:
    https://commons.wikimedia.org/wiki/File:Tutanchamun_Maske.jpg

    "Hyacinth macaw (Anodorhynchus hyacinthinus) head.JPG", the Pantanal,
    Brazil, by Charles J. Sharp, CC BY-SA 4.0:
    https://commons.wikimedia.org/wiki/File:Hyacinth_macaw_(Anodorhynchus_hyacinthinus)_head.JPG

    "De Hamburgerbrug met de Oudegracht en de Domtoren in de Stad
    Utrecht.jpg" by Jan dijkstra, CC BY-SA 4.0:
    https://commons.wikimedia.org/wiki/File:De_Hamburgerbrug_met_de_Oudegracht_en_de_Domtoren_in_de_Stad_Utrecht.jpg

-   VDC-IMONO:

    "Strasbourg Cathedral Exterior" by David Iliff (Diliff), CC BY-SA 3.0 /
    GFDL 1.2+:
    https://commons.wikimedia.org/wiki/File:Strasbourg_Cathedral_Exterior_-_Diliff.jpg

    "Zebras Ngorongoro Crater.jpg" by Muhammad Mahdi Karim, GFDL 1.2:
    https://commons.wikimedia.org/wiki/File:Zebras_Ngorongoro_Crater.jpg

    "Portrait de femme en tenue traditionnelle de Berbère Algérien.jpg" by
    Samia Dib Benkaci, CC BY-SA 4.0:
    https://commons.wikimedia.org/wiki/File:Portrait_de_femme_en_tenue_traditionnelle_de_Berb%C3%A8re_Alg%C3%A9rien.jpg

-   VDC-IM800:

    "Portrait-of-a-woman.jpg" by Mark Sherman, CC BY 2.0:
    https://commons.wikimedia.org/wiki/File:Portrait-of-a-woman.jpg

    "The History of Apple Pie - Kelly Lee Owens (2013).jpg" by Sylvain lasco,
    CC BY-SA 4.0:
    https://commons.wikimedia.org/wiki/File:The_History_of_Apple_Pie_-_Kelly_Lee_Owens_(2013).jpg

    Third picture: "Maupi", the author's own cat (not Commons-sourced).

-   VDC-VSCROLL:

    "Kinryuzan Temple, Asakusa" (Asakusa Kinryuzan), print #99 from One
    Hundred Famous Views of Edo, 1856, Utagawa Hiroshige, public domain
    (author died 1858). High-resolution scan from The Met's Open Access
    collection (object 56689):
    https://www.metmuseum.org/art/collection/search/56689

-   VDC-PANORAMA:

    "Nine Dragons" (九龍圖卷), Chen Rong, 1244, ink and colour on paper,
    Museum of Fine Arts, Boston -- public domain (artist died ca. 1266).
    A genuine Chinese handscroll (emakimono-style, 46.3 x 1496.4 cm --
    designed to be viewed by continuous horizontal unrolling, the exact
    real-world precedent for this section's own R27-based horizontal
    pan), high-resolution scan (30020x1116) from Wikimedia Commons
    (PD-Art tag, reproduction of a pre-1931-published public-domain
    work), not the museum's own restrictively-licensed reproduction:
    https://commons.wikimedia.org/wiki/File:Chen_Rong_-_Nine_Dragons.jpg

    Cropped to a single dramatic passage (dragons emerging from cloud
    and a swirling wave, source region x=[10006,20012] of the full
    scroll) and converted via tools/vdc_convert.py --mode panorama
    (plain Floyd-Steinberg 1-bit dither, same convert_imono() every
    other mono mode uses) -- see assets/source/panorama_chenrong_
    ninedragons.jpg.

-   VDC-PANORAMA 2D:

    "The Last Stand of the Kusunoki at Shijōnawate" (楠家勇士四條縄手にて討死),
    Utagawa Kuniyoshi, 1857, colour woodblock triptych, British Museum
    (1907,0531,0.229.1-3) -- public domain (artist died 1861). A dramatic
    night battle scene (samurai under a hail of arrows against a black
    sky), chosen for this combined horizontal+vertical pan specifically
    for its strong black/white graphic contrast once dithered mono, and
    its wide-but-not-extreme aspect (1902x896, 2.123:1) closely matching
    this mode's own stored bitmap (904x426, 2.122:1) with almost no crop.
    High-resolution scan from Wikimedia Commons (PD-Japan/PD-Art-two):
    https://commons.wikimedia.org/wiki/File:The_last_stand_of_the_Kusunoki_at_Shijonawate.jpg

    Converted via tools/vdc_convert.py --mode panorama2d (plain
    Floyd-Steinberg 1-bit dither) -- see assets/source/panorama2d_
    kuniyoshi_kusunoki.jpg.

-   VDC Spectrum:

    Real ZX Spectrum .scr screen-memory dumps (standard format, not
    Commodore-specific -- see tools/vdc_convert.py's own convert_spectrum()
    for the decode). Three real demoscene graphics-competition entries,
    credited to their own sceners/parties per the demoscene's own
    reuse-with-credit norm -- sourced via zxart.ee's public API
    (zxart.ee/api/types:zxPicture/...), not redistributed game screenshots:

    "np" (2015), prof4d, 1st place at CC Winter - DiHalt Lite 2015:
    https://zxart.ee/eng/authors/p/prof4d/np8/

    "Prisoner of Time" (2001), PheeL, 1st place at Chaos Constructions 2001:
    https://zxart.ee/eng/authors/p/pheel/prisoner-of-time/

    "Cursed Eighth" (2010), Piesiu, 1st place at the Chaos Constructions
    2010 ZX Graphics compo:
    https://zxart.ee/eng/authors/p/piesiu/cursed-eighth/

    The odd/even-field colour-blend technique VDC-IHFLI/VDC-ITFLI's own
    converter now defaults to (tools/vdc_convert.py's
    convert_colour_cells_paired()) was confirmed against real disk-bundled
    "iPaint" (BASIC 8) sample pictures during the same research pass --
    colour-index statistics only, no copyrighted picture content
    reproduced. See project history/TODO.md for the fuller writeup.

-   Music: "Maniac" (Michael Sembello, from Flashdance, 1983), SID cover
    by Antti Hannula (Flex), 2010, Artline Designs -- the resident
    background tune, played throughout the demo and credited on the same
    terms in its own end-credits scroller:

    https://csdb.dk/release/?id=94553 (source release) /
    https://deepsid.chordian.net/?file=/MUSICIANS/H/Hannula_Antti/Maniac.sid

    Relocated for this project's own memory layout via sidreloc (Linus
    Akesson, MIT licence) -- see tools/sidreloc/CREDIT.md and the loading
    comment above SIDINIT/SIDPLAY in this file for the technical detail.

-   Scott Hutter - VDC Core functions inspiration:

    https://github.com/Commodore64128/vdc_gui/blob/master/src/vdc_core.c

    (used as starting point)

-   Scott Robison for teaching me how o create a C128 disk boot sector

-   Francesco Sblendorio - Screen Utility: used for inspiration:

    https://github.com/xlar54/ultimateii-dos-lib/blob/master/src/samples/screen_utility.c

-   DevDef: Commodore 128 Assembly - Part 3: The 80-column (8563) chip

    https://devdef.blogspot.com/2018/03/commodore-128-assembly-part-3-80-column.html

-   Tips and Tricks for C128: VDC

    http://commodore128.mirkosoft.sk/vdc.html

-   Steve Goldsmith - C3L Commodore 128 CP/M C Library

    https://github.com/sgjava/c3l

    (Used for inspiration and for the text wrap and random sentence generator functions)

-   Bart van Leeuwen: For inspiration and advice while coding. Also for providing the excellent Device Manager ROM to make testing on real hardware very easy

-   Tokra: For extensive real-hardware testing and feedback on this
    project after the v1.0.0 release -- live VICE-monitor register-dump
    comparisons against his own working "VDC Mode Mania" build that found
    a VSYNC (register 7) off-by-one on VDC-IMONO/VDC-IM800 and a REFRESH
    (register 36) boot-baseline leak, a direct screenshot review of
    raster_calibrate() that found its CIA-timer sync-point bug, and the
    real-monitor observation ("interlace is super fiddly -- some displays
    may actually need the +1 value") behind this project's own live
    VSYNC-nudge feature. See vdc_core.c's/vdc_raster.c's own credit
    comments at each specific fix, and the v1.0.1 changelog entry in
    README.md for the full writeup. Thank you for the time and the
    precision of the reports.

-   Original windowing system code on Commodore 128 by unknown author.
   
-   Tested using real hardware (C128D and C128DCR) plus VICE.

The code can be used freely as long as you retain a notice describing original source and author.

THE PROGRAMS ARE DISTRIBUTED IN THE HOPE THAT THEY WILL BE USEFUL, BUT WITHOUT ANY WARRANTY. USE THEM AT YOUR OWN RISK!
*/

#ifndef __DEFINES_H_
#define __DEFINES_H_

// Memory map bank 1
#define MEM_SID 0x2000
#define MEM_SCREEN 0x4000
#define MEM_CHARSET 0xC000

// "Maniac" (Michael Sembello, from Flashdance, 1983), SID cover by Antti
// Hannula (Flex), 2010, Artline Designs -- see this file's own credit
// block above for the full attribution. A PAL-native/VBI-tempo
// composition (PSID v2 "speed" field, song 1's bit clear -- see the HVSC
// URL below): the tune just follows whatever rate raster_irq_playframe()
// (vdc_raster.c) calls it at, exactly once per IRQ, no software tempo
// correction of any kind. This project deliberately does NOT attempt to
// correct playback tempo for a PAL/NTSC mismatch -- an earlier
// rate-accumulator mechanism did (speeding up/slowing down SIDPLAY calls
// to approximate the tune's native tempo on the "wrong" host standard);
// real-hardware/VICE feedback on v1.0.2 found the corrected playback
// audibly worse than simply letting the tune run ~19% fast on NTSC, so it
// was removed by deliberate choice (see project memory). Attention point
// for any future tune swap: a CIA-timer-tempo tune (PSID speed bit set)
// will just play at its own fixed rate regardless of host standard --
// same "no correction" policy applies, don't reintroduce rate-correction
// machinery without a specific request.
//
// https://csdb.dk/release/?id=94553 (source release) /
// https://deepsid.chordian.net/?file=/MUSICIANS/H/Hannula_Antti/Maniac.sid
// (HVSC copy; also mirrored at https://www.hvsc.c64.org/download/C64Music/
// MUSICIANS/H/Hannula_Antti/Maniac.sid) -- relocated from its native
// $1000 (init)/$1003 (play) to this project's target load address via
// tools/sidreloc (Linus Akesson, MIT license --
// https://www.linusakesson.net/software/sidreloc/, vendored at
// tools/sidreloc/, see tools/sidreloc/CREDIT.md):
//
//   tools/sidreloc/sidreloc -v -p 20 -z 80-df Maniac.sid maniac_pal_reloc.sid
//
// Result: init $2000, play $2003 -- sidreloc places init/play wherever
// the tune's own internal layout happens to land within the target
// page, tune-specific; don't assume these addresses carry over to a
// future tune swap. Zero page $fc/$fd -> $80/$81 (clear of both
// Oscar64's own default zeropage, $f7-$ff, and Krill's loader, $e0-$f5).
// sidreloc's own emulated verification: 0% bad pitches, 0% bad pulse
// widths. assets/music.prg is maniac_pal_reloc.sid's PSID payload with
// the PSID header stripped (payload already starts with its own 2-byte
// $2000 load-address prefix, i.e. it's already a valid PRG -- no
// repacking needed). assets/musick is the TSCrunch-compressed form:
//
//   tscrunch -i music.prg music_inplace.prg
//   perl krill/loader/tools/compressedfileconverter.pl music.prg music_inplace.prg musick
//
// (`-i`, not `-p` -- in-place mode's own load-address prefix is what the
// converter script's default (non-lc) type expects to read and replace;
// feeding it a headerless `-p` output silently consumes 2 bytes of real
// compressed data instead.)
#define SIDINIT 0x2000
#define SIDPLAY 0x2003


// References to steering chars
#define CH_CURS_UP 145    // Petscii control code for Cursor Up
#define CH_CURS_DOWN 17   // Petscii control code for Cursor Down
#define CH_CURS_LEFT 157  // Petscii control code for Cursor Left
#define CH_CURS_RIGHT 29  // Petscii control code for Cursor Right
#define CH_PI 222         // Petscii control code for Pi
#define CH_HOME 19        // Petscii control code for Home (HOME)
#define CH_CLEAR 147      // Petscii control code for Clear (SHIFT+HOME)
#define CH_DEL 20         // Petscii control code for delete (DEL)
#define CH_INS 148        // Petscii control code for Insert (SHIFT+DEL)
#define CH_ENTER 13       // Petscii control code for enter (RETURN)
#define CH_STOP 3         // Petscii control code for stop (STOP)
#define CH_RUN 10         // Petscii control code for run (SHIFT+STOP)
#define CH_TAB 9          // Petscii control code for tab (TAB)
#define CH_LIRA 92        // Petscii control code for pound sign
#define CH_ESC 27         // Petscii control code for escape (ESC)
#define CH_FONT_LOWER 14  // Petscii control code for lower case font
#define CH_FONT_UPPER 142 // Petscii control code for upper case font
#define CH_ARROW_UP 94    // Petscii control code for arrow up
#define CH_ARROW_LEFT 95  // Petscii control code for arrow left
#define CH_SPACE 32       // Petscii code and Screencode for space
#define CH_MINUS 45       // Screencode for minus
#define CH_BLACK 144      // Petscii control code for black           CTRL-1
#define CH_WHITE 5        // Petscii control code for white           CTRL-2
#define CH_DRED 28        // Petscii control code for dark red        CTRL-3
#define CH_LCYAN 159      // Petscii control code for light cyan      CTRL-4
#define CH_LPURPLE 156    // Petscii control code for light purple    CTRL-5
#define CH_DGREEN 30      // Petscii control code for dark green      CTRL-6
#define CH_DBLUE 31       // Petscii control code for dark blue       CTRL-7
#define CH_LYELLOW 158    // Petscii control code for light yellow    CTRL-8
#define CH_RVSON 18       // Petscii control code for RVS ON          CTRL-9
#define CH_RVSOFF 146     // Petscii control code for RVS OFF         CTRL-0
#define CH_DPURPLE 129    // Petscii control code for dark purple     C=-1
#define CH_DYELLOW 149    // Petscii control code for dark yellow     C=-2
#define CH_LRED 150       // Petscii control code for light red       C=-3
#define CH_DCYAN 151      // Petscii control code for dark cyan       C=-4
#define CH_DGREY 152      // Petscii control code for dark grey       C=-5
#define CH_LGREEN 153     // Petscii control code for light green     C=-6
#define CH_LBLUE 154      // Petscii control code for light blue      C=-7
#define CH_LGREY 155      // Petscii control code for light grey      C=-8

#endif // __DEFINES_H_