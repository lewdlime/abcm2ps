/*  
 * This file is part of abcm2ps.
 * Copyright (C) 1998-2000 Jean-François Moine
 * Adapted from abc2ps, Copyright (C) 1996,1997 Michael Methfessel
 * See file abc2ps.c for details.
 */

#include <stdio.h>
#include <math.h>
#include <time.h>
#include <string.h>

#include "abcparse.h"
#include "abc2ps.h"

/*  subroutines to define postscript macros which draw symbols	*/

static char def_misc[] = "\n/cshow { %% usage: string cshow  - center at current pt\n"
	"  dup stringwidth pop 2 div neg 0 rmoveto\n"
	"  show\n"
	"} bind def\n"

	"\n/lshow { %% usage: string lshow - show left-aligned\n"
	"  dup stringwidth pop neg 0 rmoveto show\n"
	"} bind def\n"

	"\n/showb { %% usage: string showb - show in box\n"
	"  dup currentpoint 3 -1 roll show\n"
	"  moveto gsave 0.5 setlinewidth\n"
	"  -2 -3 rmoveto stringwidth pop 4 add fh 4 add 2 copy\n"
	"  0 exch rlineto 0 rlineto neg 0 exch rlineto neg 0 rlineto\n"
	"  stroke grestore\n"
	"} bind def\n"

	"\n/cshowb { %% usage: string cshowb - show centered in box\n"
	"  dup stringwidth pop dup 2 div neg 0 rmoveto currentpoint 4 -1 roll show\n"
	"  moveto gsave 0.5 setlinewidth\n"
	"  -2 -3 rmoveto 4 add fh 4 add 2 copy\n"
	"  0 exch rlineto 0 rlineto neg 0 exch rlineto neg 0 rlineto\n"
	"  stroke grestore\n"
	"} bind def\n"

	"\n/wd { moveto show } bind def\n"
	"/wln {\n"
	"  dup 3 1 roll moveto gsave 0.6 setlinewidth lineto stroke grestore\n"
	"} bind def\n"

	"/whf {moveto gsave 0.5 1.2 scale (-) show grestore} bind def\n";

static char def_tclef[] = "\n/tclef {  %% usage:  x y tclef  - treble clef \n"
	" moveto\n"
	" -1.9 3.7 rmoveto\n"
	" -2.85 1.55 -3.35 6.175 0.775 8.8 rcurveto\n"
	" 4.5 0.625 8.935 -1.53 8.935 -6.33 rcurveto\n"
	" 0.0 -4.43 -4.06 -6.65 -7.75 -6.65 rcurveto\n"
	" -4.43 0.0 -8.49 2.22 -8.935 7.855 rcurveto\n"
	" 0.125 4.125 1.5 7.875 6.5 11.25 rcurveto\n"
	" 5.5 3.625 7.06 6.15 7.375 7.75 rcurveto\n"
	" 0.25 2.375 -0.25 5.75 -2.5 3.625 rcurveto\n"
	" -1.25 -1.75 -2.5 -4.625 -2.875 -7.375 rcurveto\n"
	" 0.125 -13.75 3.25 -16.625 4.0 -26.0 rcurveto\n"
	" 0.0 -4.25 -1.5 -5.875 -4.5 -5.875 rcurveto\n"
	" -2.875 0.0 -4.625 2.0 -4.625 3.75 rcurveto\n"
	" 0.0 2.0 1.0 3.5 2.875 3.5 rcurveto\n"
	" 3.75 0.0 3.75 -5.875 0.75 -4.75 rcurveto\n"
	" 0.375 -2.25 4.625 -2.125 4.625 2.875 rcurveto\n"
	" -0.75 11.75 -4.0 15.0 -4.25 28.375 rcurveto\n"
	" 0.0 4.125 0.83 7.49 4.875 9.75 rcurveto\n"
	" 2.0 -2.5 3.125 -5.125 2.965 -7.43 rcurveto\n"
	" -0.465 -3.695 -2.59 -7.07 -5.84 -9.82 rcurveto\n"
	" -3.625 -2.125 -6.875 -5.125 -7.625 -10.875 rcurveto\n"
	" 0.875 -4.625 4.225 -5.895 7.185 -5.895 rcurveto\n"
	" 2.95 0.0 5.54 1.85 5.54 4.8 rcurveto\n"
	" 0.0 3.32 -2.95 4.43 -4.8 4.43 rcurveto\n"
	" -4.05 -0.21 -4.925 -3.21 -2.7 -5.76 rcurveto\n"
	"  fill\n} bind def\n"

	"\n/stclef {\n"
	"  exch 0.85 div exch 0.85 div gsave 0.85 0.85 scale tclef grestore\n"
	"} bind def\n";

static char def_bclef[] = "\n/bclef {  %% usage:  x y bclef  - bass clef \n"
	" moveto\n"
	" -8.8 3.8 rmoveto\n"
	" 7.55 2.2 11.8 7.7 11.8 11.7 rcurveto\n"
	" 0.0 4.5 -2.0 7.5 -4.3 7.3 rcurveto\n"
	" -1.0 0.2 -4.2 0.2 -5.7 -3.8 rcurveto\n"
	" 2.0 0.875 4.0 0.125 4.25 -1.0 rcurveto\n"
	" -0.25 -1.625 -1.25 -2.5 -2.75 -2.5 rcurveto\n"
	" -1.0 0.0 -2.0 0.9 -2.0 2.5 rcurveto\n"
	" 0.0 2.5 2.1 5.5 6.0 5.5 rcurveto\n"
	" 4.0 0.0 7.5 -2.5 7.5 -7.5 rcurveto\n"
	" 0.0 -5.5 -6.5 -11.0 -14.8 -12.2 rcurveto\n"
	" 16.9 17.0 rmoveto\n"
	" 0.0 1.5 2.0 1.5 2.0 0.0 rcurveto\n"
	" 0.0 -1.5 -2.0 -1.5 -2.0 0.0 rcurveto\n"
	" 0.0 -5.5 rmoveto\n"
	" 0.0 1.5 2.0 1.5 2.0 0.0 rcurveto\n"
	" 0.0 -1.5 -2.0 -1.5 -2.0 0.0 rcurveto\n"
	"  fill\n} bind def\n"

	"\n/sbclef {\n"
	"  exch 0.85 div exch 0.85 div gsave 0.85 0.85 scale 0 4 translate bclef grestore\n"
	"} bind def\n";

static char def_typeset[] = "\n/WS {   %%usage:	w nspaces str WS\n"
	"   dup stringwidth pop 4 -1 roll\n"
	"   sub neg 3 -1 roll div 0 8#040 4 -1 roll\n"
	"   widthshow\n"
	"} bind def\n"

	"\n/W1 { show pop pop } bind def\n"

	"\n/str 50 string def\n"
	"/W0 {\n"
	"   dup stringwidth pop str cvs exch show (	) show show pop pop\n"
	"} bind def\n"

	"\n/WC { counttomark 1 sub dup 0 eq { 0 }\n"
	"  {  ( ) stringwidth pop neg 0 3 -1 roll\n"
	"  {  dup 3 add index stringwidth pop ( ) stringwidth pop add\n"
	"  dup 3 index add 4 index lt 2 index 1 lt or\n"
	"  {3 -1 roll add exch 1 add} {pop exit} ifelse\n"
	"  } repeat } ifelse\n"
	"} bind def\n"

	"\n/P1 {\n"
	"  {  WC dup 0 le {exit} if\n"
	"	  exch pop gsave { exch show ( ) show } repeat grestore LF\n"
	"   } loop pop pop pop pop\n"
	"} bind def\n"

	"\n/P2 {\n"
	"   {  WC dup 0 le {exit} if\n"
	"	  dup 1 sub dup 0 eq\n"
	"	  { pop exch pop 0 }\n"
	"	  { 3 2 roll 3 index exch sub exch div } ifelse\n"
	"	  counttomark 3 sub 2 index eq { pop 0 } if exch gsave\n"
	"	  {  3 2 roll show ( ) show dup 0 rmoveto } repeat\n"
	"	  grestore LF pop\n"
	"   } loop pop pop pop pop\n"
	"} bind def\n";

static char def_tsig[] = "\n/tsig { %% usage: x y (top) (bot) tsig - draw time signature\n"
	"   4 2 roll moveto\n"
	"   gsave /Times-Bold 16 selectfont 1.2 1 scale\n"
	"   0 1 rmoveto currentpoint 3 -1 roll cshow\n"
	"   moveto 0 12 rmoveto cshow grestore\n"
	"} bind def\n";

static char def_dot[] = "\n/dt {  %% usage: dx dy dt  - dot shifted by dx,dy\n"
	"  y add exch x add exch 1.2 0 360 arc fill\n"
	"} bind def\n";

static char def_hl[] = "\n/hl {  %% usage: y hl  - helper line at height y\n"
	"   gsave 1 setlinewidth x exch moveto \n"
	"   -5.5 0 rmoveto 11 0 rlineto stroke grestore\n"
	"} bind def\n"

	"\n/hl1 {  %% usage: y hl1  - longer helper line\n"
	"   gsave 1 setlinewidth x exch moveto \n"
	"   -7 0 rmoveto 14 0 rlineto stroke grestore\n"
	"} bind def\n";

static char def_beam[] = "\n/bm {  %% usage: x1 y1 x2 y2 t bm  - beam, depth t\n"
	"  3 1 roll moveto dup 0 exch neg rlineto\n"
	"  dup 4 1 roll sub lineto 0 exch rlineto fill\n"
	"} bind def\n"

	"\n/bnum {  %% usage: x y (str) bnum	- number on beam\n"
	"  3 1 roll moveto gsave /Times-Italic 12 selectfont\n"
	"  cshow grestore\n"
	"} bind def\n"
  
	"\n/hbr {  %% usage: x1 y1 x2 y2 hbr	- half bracket\n"
	"  moveto lineto 0 -3 rlineto stroke\n"
	"} bind def\n";

static char def_bars[] = "\n/bar {  %% usage: h x y bar  - single bar\n"
	"  moveto 0 exch rlineto stroke\n"
	"} bind def\n"
	  
	"\n/dbar {  %% usage: h x y dbar  - thin double bar\n"
	"   moveto dup 0 exch rlineto dup -3 exch neg rmoveto\n"
	"   0 exch rlineto stroke\n"
	"} bind def\n"

	"\n/fbar1 {  %% usage: h x y fbar1  - fat double bar at start\n"
	"  moveto dup 0 exch rlineto 3 0 rlineto dup 0 exch neg rlineto \n"
	"  currentpoint fill moveto\n"
	"  3 0 rmoveto 0 exch rlineto stroke\n"
	"} bind def\n"

	"\n/fbar2 {  %% usage: h x y fbar2  - fat double bar at end\n"
	"  moveto dup 0 exch rlineto -3 0 rlineto dup 0 exch neg rlineto \n"
	"  currentpoint fill moveto\n"
	"  -3 0 rmoveto 0 exch rlineto stroke\n"
	"} bind def\n"

	"\n/rdots {  %% usage: x y rdots  - repeat dots \n"
	"  moveto 0 9 rmoveto currentpoint 2 copy 1.2 0 360 arc \n"
	"  moveto 0 6 rmoveto  currentpoint 1.2 0 360 arc fill\n"
	"} bind def\n";

static char def_csig[] = "\n"
	"/csig {  %% usage:  x y csig - C timesig\n"
	" moveto\n"
	" 1.0 17.25 rmoveto\n"
	" 1.0 0.0 2.3 -0.75 2.3 -2.19 rcurveto\n"
	" -0.5 1.69 -3.19 1.38 -3.19 -1.0 rcurveto\n"
	" 1.31 -1.56 3.37 -1.06 3.75 0.44 rcurveto\n"
	" -0.37 2.5 -1.87 3.25 -3.87 3.25 rcurveto\n"
	" -3.5 0.0 -5.25 -2.25 -5.25 -7.5 rcurveto\n"
	" 1.75 -4.0 6.0 -6.69 9.6 -0.06 rcurveto\n"
	" -3.37 -5.06 -6.69 -4.25 -7.37 0.062 rcurveto\n"
	" -0.5 4.0 0.62 7.31 4.0 7.0 rcurveto\n"
	" fill\n"
	" } bind def\n"
	" \n"
	"/ctsig {  %% usage:  x y ctsig - C| timesig\n"
	"  exch 4 add exch 2 copy csig 4 add moveto 0 16 rlineto stroke\n"
	"} bind def\n";

static char def_gchord[] = "\n/gc { %% usage: x y (str) gc  - draw guitar chord string\n"
	"  3 1 roll moveto show\n"
	"} bind def\n";

static char def_staff[] = "\n/staff {	%% usage: l staff  - draw staff\n"
	"  gsave 0.6 setlinewidth\n"
	"  dup 0 rlineto dup neg 6 rmoveto\n"
	"  dup 0 rlineto dup neg 6 rmoveto\n"
	"  dup 0 rlineto dup neg 6 rmoveto\n"
	"  dup 0 rlineto dup neg 6 rmoveto\n"
	"  dup 0 rlineto dup neg 6 rmoveto\n"
	"  pop stroke grestore\n"
	"} bind def\n"

	"\n/hbrce {\n"
	" -2.5 1.0 rmoveto\n"
	" -3.75 -4.0 -7.125 -9.875 -4.375 -26.875 rcurveto\n"
	" 3.75 -13.875 2.625 -20.125 -0.5 -22.5 rcurveto\n"
	" -3.75 -1.75 2.5 0.625 4.125 5.75 rcurveto\n"
	" 0.25 8.25 -0.125 13.75 -3.375 26.5 rcurveto\n"
	" -1.125 6.875 -0.125 12.0 4.125 17.125 rcurveto\n"
	"  fill\n"
	"} bind def\n"

	"\n/brace {  %%usage h x y brace\n"
	"  gsave translate 0 0 moveto 98 div 1.05 exch scale hbrce\n"
	"  0 -98 moveto 1 -1 scale hbrce grestore\n"
	"} bind def\n"

	"\n/bracket {  %%usage h x y braket\n"
	"  3 copy 3 -1 roll sub moveto -4 0 rlineto stroke\n"
	"  2 copy moveto -4 0 rlineto stroke exch 4 sub exch moveto\n"
	"  dup 0 exch neg rlineto -3 0 rlineto 0 exch rlineto\n"
	"  fill\n"
	"} bind def\n";

static char def_sep[] = "\n/sep0 { %% usage: x1 x2 sep0  - hline separator \n"
	"   0 moveto 0 lineto stroke\n"
	"} bind def\n";

/* -- define which latin encoding -- */
void define_encoding(FILE *fp,
		     int enc)	/* Latin encoding number */
{
	switch (enc) {
	case 1:
		fprintf(fp, "\n"
			"/ISOLatin1Encoding [\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/space /exclam /quotedbl /numbersign /dollar /percent /ampersand /quoteright\n"
			"/parenleft /parenright /asterisk /plus /comma /minus /period /slash\n"
			"/zero /one /two /three /four /five /six /seven\n"
			"/eight /nine /colon /semicolon /less /equal /greater /question\n"
			"%% 100\n"
			"/at /A /B /C /D /E /F /G\n"
			"/H /I /J /K /L /M /N /O\n"
			"/P /Q /R /S /T /U /V /W\n"
			"/X /Y /Z /bracketleft /backslash /bracketright /asciicircum /underscore\n"
			"/quoteleft /a /b /c /d /e /f /g\n"
			"/h /i /j /k /l /m /n /o\n"
			"/p /q /r /s /t /u /v /w\n"
			"/x /y /z /braceleft /bar /braceright /asciitilde /.notdef\n"
			"%% 200\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/space /exclamdown /cent /sterling /currency /yen /brokenbar /section\n"
			"/dieresis /copyright /ordfeminine /guillemotleft /logicalnot /hyphen /registered /macron\n"
			"/degree /plusminus /twosuperior /threesuperior /acute /mu /paragraph /bullet\n"
			"/cedilla /dotlessi /ordmasculine /guillemotright /onequarter /onehalf /threequarters /questiondown\n"
			"%% 300\n"
			"/Agrave /Aacute /Acircumflex /Atilde /Adieresis /Aring /AE /Ccedilla\n"
			"/Egrave /Eacute /Ecircumflex /Edieresis /Igrave /Iacute /Icircumflex /Idieresis\n"
			"/Eth /Ntilde /Ograve /Oacute /Ocircumflex /Otilde /Odieresis /multiply\n"
			"/Oslash /Ugrave /Uacute /Ucircumflex /Udieresis /Yacute /Thorn /germandbls\n"
			"/agrave /aacute /acircumflex /atilde /adieresis /aring /ae /ccedilla\n"
			"/egrave /eacute /ecircumflex /edieresis /igrave /iacute /icircumflex /idieresis\n"
			"/eth /ntilde /ograve /oacute /ocircumflex /otilde /odieresis /divide\n"
			"/oslash /ugrave /uacute /ucircumflex /udieresis /yacute /thorn /ydieresis\n"
			"] def\n");
		break;
	case 2:
		fprintf(fp, "\n"
			"/ISOLatin2Encoding [\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/space /exclam /quotedbl /numbersign /dollar /percent /ampersand /quoteright\n"
			"/parenleft /parenright /asterisk /plus /comma /minus /period /slash\n"
			"/zero /one /two /three /four /five /six /seven\n"
			"/eight /nine /colon /semicolon /less /equal /greater /question\n"
			"%% 100\n"
			"/at /A /B /C /D /E /F /G\n"
			"/H /I /J /K /L /M /N /O\n"
			"/P /Q /R /S /T /U /V /W\n"
			"/X /Y /Z /bracketleft /backslash /bracketright /asciicircum /underscore\n"
			"/quoteleft /a /b /c /d /e /f /g\n"
			"/h /i /j /k /l /m /n /o\n"
			"/p /q /r /s /t /u /v /w\n"
			"/x /y /z /braceleft /bar /braceright /asciitilde /.notdef\n"
			"%% 200\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/space /Aogonek /breve /Lslash /currency /Lcaron /Sacute /section\n"
			"/dieresis /Scaron /Scedilla /Tcaron /Zacute /hyphen /Zcaron /Zdotaccent\n"
			"/degree /aogonek /ogonek /lslash /acute /lcaron /sacute /caron\n"
			"/cedilla /scaron /scedilla /tcaron /zacute /hungarumlaut /zcaron /zdotaccent\n"
			"%% 300\n"
			"/Racute /Aacute /Acircumflex /Abreve /Adieresis /Lacute /Cacute /Ccedilla\n"
			"/Ccaron /Eacute /Eogonek /Edieresis /Ecaron /Iacute /Icircumflex /Dcaron\n"
			"/Dbar /Nacute /Ncaron /Oacute /Ocircumflex /Ohungarumlaut /Odieresis /multiply\n"
			"/Rcaron /Uring /Uacute /Uhungarumlaut /Udieresis /Yacute /Tcedilla /germandbls\n"
			"/racute /aacute /acircumflex /abreve /adieresis /lacute /cacute /ccedilla\n"
			"/ccaron /eacute /eogonek /edieresis /ecaron /iacute /icircumflex /dcaron\n"
			"/dbar /nacute /ncaron /oacute /ocircumflex /ohungarumlaut /odieresis /divide\n"
			"/rcaron /uring /uacute /uhungarumlaut /udieresis /yacute /tcedilla /dotaccent\n"
			"] def\n");
		break;
	case 3:
		fprintf(fp, "\n"
			"/ISOLatin3Encoding [\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/space /exclam /quotedbl /numbersign /dollar /percent /ampersand /quoteright\n"
			"/parenleft /parenright /asterisk /plus /comma /minus /period /slash\n"
			"/zero /one /two /three /four /five /six /seven\n"
			"/eight /nine /colon /semicolon /less /equal /greater /question\n"
			"%% 100\n"
			"/at /A /B /C /D /E /F /G\n"
			"/H /I /J /K /L /M /N /O\n"
			"/P /Q /R /S /T /U /V /W\n"
			"/X /Y /Z /bracketleft /backslash /bracketright /asciicircum /underscore\n"
			"/quoteleft /a /b /c /d /e /f /g\n"
			"/h /i /j /k /l /m /n /o\n"
			"/p /q /r /s /t /u /v /w\n"
			"/x /y /z /braceleft /bar /braceright /asciitilde /.notdef\n"
			"%% 200\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/space /Hstroke /breve /sterling /currency /yen /Hcircumflex /section\n"
			"/dieresis /Idotaccent /Scedilla /Gbreve /Jcircumflex /hyphen /registered /Zdotaccent\n"
			"/degree /hstroke /twosuperior /threesuperior /acute /mu /hcircumflex /bullet\n"
			"/cedilla /dotlessi /scedilla /gbreve /jcircumflex /onehalf /threequarters /zdotaccent\n"
			"%% 300\n"
			"/Agrave /Aacute /Acircumflex /Atilde /Adieresis /Cdotaccent /Ccircumflex /Ccedilla\n"
			"/Egrave /Eacute /Ecircumflex /Edieresis /Igrave /Iacute /Icircumflex /Idieresis\n"
			"/Eth /Ntilde /Ograve /Oacute /Ocircumflex /Gdotaccent /Odieresis /multiply\n"
			"/Gcircumflex /Ugrave /Uacute /Ucircumflex /Udieresis /Ubreve /Scircumflex /germandbls\n"
			"/agrave /aacute /acircumflex /atilde /adieresis /cdotaccent /ccircumflex /ccedilla\n"
			"/egrave /eacute /ecircumflex /edieresis /igrave /iacute /icircumflex /idieresis\n"
			"/eth /ntilde /ograve /oacute /ocircumflex /gdotaccent /odieresis /divide\n"
			"/gcircumflex /ugrave /uacute /ucircumflex /udieresis /ubreve /scircumflex /dotaccent\n"
			"] def\n");
		break;
	case 4:
		fprintf(fp, "\n"
			"/ISOLatin4Encoding [\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/space /exclam /quotedbl /numbersign /dollar /percent /ampersand /quoteright\n"
			"/parenleft /parenright /asterisk /plus /comma /minus /period /slash\n"
			"/zero /one /two /three /four /five /six /seven\n"
			"/eight /nine /colon /semicolon /less /equal /greater /question\n"
			"%% 100\n"
			"/at /A /B /C /D /E /F /G\n"
			"/H /I /J /K /L /M /N /O\n"
			"/P /Q /R /S /T /U /V /W\n"
			"/X /Y /Z /bracketleft /backslash /bracketright /asciicircum /underscore\n"
			"/quoteleft /a /b /c /d /e /f /g\n"
			"/h /i /j /k /l /m /n /o\n"
			"/p /q /r /s /t /u /v /w\n"
			"/x /y /z /braceleft /bar /braceright /asciitilde /.notdef\n"
			"%% 200\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/space /Aogonek /kra /Rcedilla /currency /Itilde /Lcedilla /section\n"
			"/dieresis /Scaron /Emacron /Gcedilla /Tbar /hyphen /Zcaron /macron\n"
			"/degree /aogonek /ogonek /rcedilla /acute /itilde /lcedilla /caron\n"
			"/cedilla /scaron /emacron /gcedilla /tbar /Eng /zcaron /eng\n"
			"%% 300\n"
			"/Amacron /Aacute /Acircumflex /Atilde /Adieresis /Aring /AE /Iogonek\n"
			"/Ccaron /Eacute /Eogonek /Edieresis /Edotaccent /Iacute /Icircumflex /Imacron\n"
			"/Eth /Ncedilla /Omacron /Kcedilla /Ocircumflex /Otilde /Odieresis /multiply\n"
			"/Oslash /Uogonek /Uacute /Ucircumflex /Udieresis /Utilde /Umacron /germandbls\n"
			"/amacron /aacute /acircumflex /atilde /adieresis /aring /ae /iogonek\n"
			"/ccaron /eacute /eogonek /edieresis /edotaccent /iacute /icircumflex /imacron\n"
			"/dbar /ncedilla /omacron /kcedilla /ocircumflex /otilde /odieresis /divide\n"
			"/oslash /uogonek /uacute /ucircumflex /udieresis /utilde /umacron /dotaccent\n"
			"] def\n");
		break;
	case 5:
		fprintf(fp, "\n"
			"/ISOLatin5Encoding [\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/space /exclam /quotedbl /numbersign /dollar /percent /ampersand /quoteright\n"
			"/parenleft /parenright /asterisk /plus /comma /minus /period /slash\n"
			"/zero /one /two /three /four /five /six /seven\n"
			"/eight /nine /colon /semicolon /less /equal /greater /question\n"
			"%% 100\n"
			"/at /A /B /C /D /E /F /G\n"
			"/H /I /J /K /L /M /N /O\n"
			"/P /Q /R /S /T /U /V /W\n"
			"/X /Y /Z /bracketleft /backslash /bracketright /asciicircum /underscore\n"
			"/quoteleft /a /b /c /d /e /f /g\n"
			"/h /i /j /k /l /m /n /o\n"
			"/p /q /r /s /t /u /v /w\n"
			"/x /y /z /braceleft /bar /braceright /asciitilde /.notdef\n"
			"%% 200\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/space /exclamdown /cent /sterling /currency /yen /brokenbar /section\n"
			"/dieresis /copyright /ordfeminine /guillemotleft /logicalnot /hyphen /registered /macron\n"
			"/degree /plusminus /twosuperior /threesuperior /acute /mu /paragraph /bullet\n"
			"/cedilla /dotlessi /ordmasculine /guillemotright /onequarter /onehalf /threequarters /questiondown\n"
			"%% 300\n"
			"/Agrave /Aacute /Acircumflex /Atilde /Adieresis /Aring /AE /Ccedilla\n"
			"/Egrave /Eacute /Ecircumflex /Edieresis /Igrave /Iacute /Icircumflex /Idieresis\n"
			"/Gbreve /Ntilde /Ograve /Oacute /Ocircumflex /Otilde /Odieresis /multiply\n"
			"/Oslash /Ugrave /Uacute /Ucircumflex /Udieresis /Idotaccent /Scedilla /germandbls\n"
			"/agrave /aacute /acircumflex /atilde /adieresis /aring /ae /ccedilla\n"
			"/egrave /eacute /ecircumflex /edieresis /igrave /iacute /icircumflex /idieresis\n"
			"/gbreve /ntilde /ograve /oacute /ocircumflex /otilde /odieresis /divide\n"
			"/oslash /ugrave /uacute /ucircumflex /udieresis /dotlessi /scedilla /ydieresis\n"
			"] def\n");
		break;
	case 6:
		fprintf(fp, "\n"
			"/ISOLatin6Encoding [\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/space /exclam /quotedbl /numbersign /dollar /percent /ampersand /quoteright\n"
			"/parenleft /parenright /asterisk /plus /comma /minus /period /slash\n"
			"/zero /one /two /three /four /five /six /seven\n"
			"/eight /nine /colon /semicolon /less /equal /greater /question\n"
			"%% 100\n"
			"/at /A /B /C /D /E /F /G\n"
			"/H /I /J /K /L /M /N /O\n"
			"/P /Q /R /S /T /U /V /W\n"
			"/X /Y /Z /bracketleft /backslash /bracketright /asciicircum /underscore\n"
			"/quoteleft /a /b /c /d /e /f /g\n"
			"/h /i /j /k /l /m /n /o\n"
			"/p /q /r /s /t /u /v /w\n"
			"/x /y /z /braceleft /bar /braceright /asciitilde /.notdef\n"
			"%% 200\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
			"/space /Aogonek /Emacron /Gcedilla /Imacron /Itilde /Kcedilla /Lcedilla\n"
			"/acute /Rcedilla /Scaron /Tbar /Zcaron /hyphen /kra /Eng\n"
			"/dbar /aogonek /emacron /gcedilla /imacron /itilde /kcedilla /lcedilla\n"
			"/nacute /rcedilla /scaron /tbar /zcaron /section /germandbls /eng\n"
			"%% 300\n"
			"/Amacron /Aacute /Acircumflex /Atilde /Adieresis /Aring /AE /Iogonek\n"
			"/Ccaron /Eacute /Eogonek /Edieresis /Edotaccent /Iacute /Icircumflex /Idieresis\n"
			"/Dbar /Ncedilla /Omacron /Oacute /Ocircumflex /Otilde /Odieresis /Utilde\n"
			"/Oslash /Uogonek /Uacute /Ucircumflex /Udieresis /Yacute /Thorn /Umacron\n"
			"/amacron /aacute /acircumflex /atilde /adieresis /aring /ae /iogonek\n"
			"/ccaron /eacute /eogonek /edieresis /edotaccent /iacute /icircumflex /idieresis\n"
			"/eth /ncedilla /omacron /oacute /ocircumflex /otilde /odieresis /utilde\n"
			"/oslash /uogonek /uacute /ucircumflex /udieresis /yacute /thorn /umacron\n"
			"] def\n");
		break;
	}
}

/* -- define_font -- */
void define_font(FILE *fp,
		 char name[],
		 int  num,
		 int enc)
{
	if (!strcmp(name, "Symbol")) {
		fprintf(fp, "\n/F%d { dup 0.8 mul  /fh exch def\n"
			"/%s exch selectfont } bind def\n",
			num, name);
		return;
	}
	if (enc == 0)
		enc = 1;
	fprintf(fp, "\n/%s findfont\n"
		"dup length dict begin\n"
		"   {1 index /FID ne {def} {pop pop} ifelse} forall\n"
		"   /Encoding ISOLatin%dEncoding def\n"
		"   currentdict\n"
		"end\n"
		"/%s-ISO exch definefont pop\n"
		"/F%d { dup 0.8 mul  /fh exch def\n"
		"/%s-ISO exch selectfont } bind def\n",
		name, enc, name, num, name);
}

/* -- add_cv -- */
static void add_cv(FILE *fp,
		   float f1,
		   float f2,
		   float p[][2],
		   int i0,
		   int ncv)
{
	int i, i1, m;

	i1 = i0;
	for (m = 0; m < ncv; m++) {
		fprintf(fp, " ");
		for (i = 0; i < 3; i++)
			fprintf(fp, " %.2f %.2f",
				f1 * (p[i1 + i][0] - p[i1 - 1][0]),
				f2 * (p[i1 + i][1] - p[i1 - 1][1]));
		fprintf(fp, " rcurveto\n");
		i1 += 3;
	}
}

/* -- add_sg -- */
static void add_sg(FILE *fp,
		   float f1,
		   float f2,
		   float p[][2],
		   int i0,
		   int nseg)
{
	int i;

	for (i = 0; i < nseg; i++)
		fprintf(fp, "  %.2f %.2f rlineto\n",
			f1*(p[i0+i][0]-p[i0+i-1][0]),
			f2*(p[i0+i][1]-p[i0+i-1][1]));
}

/* -- add_mv -- */
static void add_mv(FILE *fp,
		   float f1,
		   float f2,
		   float p[][2],
		   int i0)
{
	if (i0 == 0)
		fprintf(fp, "  %.2f %.2f rmoveto\n",
			f1 * p[i0][0], f2 * p[i0][1]);
	else	fprintf(fp, "  %.2f %.2f rmoveto\n",
			f1 * (p[i0][0] - p[i0 - 1][0]),
			f2 * (p[i0][1] - p[i0 - 1][1]));
}

/* -- def_stems -- */
static void def_stems(FILE *fp)
{
	fprintf(fp, "\n/su {  %% usage: len su	- up stem\n"
		"  x y moveto %.1f %.1f rmoveto %.1f sub 0 exch rlineto stroke\n"
		"} bind def\n",
		STEM_XOFF, STEM_YOFF, STEM_YOFF );

	fprintf(fp, "\n/sd {  %% usage: len sd  - down stem\n"
		"  x y moveto %.1f %.1f rmoveto neg %.1f add 0 exch rlineto stroke\n"
		"} bind def\n",
		-STEM_XOFF, -STEM_YOFF, STEM_YOFF);
}

/* -- def_deco -- */
static void def_deco(FILE *fp)
{
	fprintf(fp, "\n/grm {  %% usage:  y grm  - gracing mark\n"
		"  x exch moveto\n"
		"  -5 2.5 rmoveto\n"
		"  5.0 8.5 5.5 -4.5 10.0 2.0 rcurveto\n"
		"  -5.0 -8.5 -5.5 4.5 -10.0 -2.0 rcurveto\n"
		"  fill\n} bind def\n"

		"\n/stc {  %% usage:  y stc  - staccato mark\n"
		"  x exch 1.2 0 360 arc fill\n} bind def\n"

		"\n/cpu {  %% usage:  y cpu  - roll sign above head\n"
		"  x exch moveto\n"
		"  -5.85 0.00 rmoveto\n"
		"  0.45 7.29 11.25 7.29 11.70 0.00 rcurveto\n"
		"  -1.35 5.99 -10.35 5.99 -11.70 0.00 rcurveto\n"
		"  fill\n} bind def\n"

		"\n/sld {  %% usage:  y dx sld  - slide\n"
		"  x exch sub exch moveto\n"
		"  -7.20 -4.80 rmoveto\n"
		"  1.80 -0.70 4.50 0.20 7.20 4.80 rcurveto\n"
		"  -2.07 -5.00 -5.40 -6.80 -7.65 -6.00 rcurveto\n"
		"  fill\n} bind def\n"
  
		"\n/emb {  %% usage:  y emb  - emphasis bar\n"
		"  gsave 1.2 setlinewidth 1 setlinecap x exch moveto \n"
		"  -2.5 0 rmoveto 5 0 rlineto stroke grestore\n"
		"} bind def\n"

		"\n/trl {  %% usage:  y trl  - trill sign\n"
		"  gsave /Times-BoldItalic 14 selectfont\n"
		"  1 add x 4 sub exch moveto (tr) show grestore\n"
		"} bind def\n"

		"\n/umrd {  %% usage:  y umrd  - upper mordent\n"
		"  3 add x exch moveto\n"
		"  2.2 2.2 rlineto 2.18 -3.0 rlineto 0.62 0.8 rlineto\n"
		"  -2.2 -2.2 rlineto -2.18 3.0 rlineto -0.62 -0.8 rlineto\n"
		"  -2.2 -2.2 rlineto -2.18 3.0 rlineto -0.62 -0.8 rlineto\n"
		"  2.2 2.2 rlineto 2.18 -3.0 rlineto 0.62 0.8 rlineto\n"
		"  fill\n} bind def\n"

		"\n/lmrd {  %% usage:  y lmrd  - lower mordent\n"
		"  dup umrd 7 add x exch moveto\n"
		"  gsave 0.8 setlinewidth 0 -8 rlineto stroke grestore\n"
		"} bind def\n"

		"\n/fng {  %% usage:  (str) y fng  - finger (0-5)\n"
		"  gsave /Times-Roman 10 selectfont\n"
		"  x 3 sub exch moveto show grestore\n"
		"} bind def\n"

		"\n/dacs {  %% usage:  (str) y dacs  - D.C. / D.S.\n"
		"  gsave /Times-Roman 16 selectfont\n"
		"  1 add x exch moveto cshow grestore\n"
		"} bind def\n"

		"\n/brth {  %% usage:  (str) y brth  - breath\n"
		"  gsave /Times-BoldItalic 32 selectfont\n"
		"  6 add x 16 add exch moveto cshow grestore\n"
		"} bind def\n"
		
		"\n/pf {  %% usage:  (str) y pf  - p, f, pp, ..\n"
		"  gsave /Times-BoldItalic 16 selectfont\n"
		"  x exch 5 add moveto cshow grestore\n"
		"} bind def\n"

		"\n/sfz {  %% usage:  (str) y sfz\n"
		"  gsave x 4 sub exch 5 add moveto pop\n"
		"  /Times-Italic 12 selectfont (s) show\n"
		"  /Times-BoldItalic 16 selectfont (f) show\n"
		"  /Times-Italic 12 selectfont (z) show grestore\n"
		"} bind def\n"

		"\n/crdc {  %% usage:  (str) y crdc  - cresc, decresc, ..\n"
		"  gsave /Times-Italic 14 selectfont\n"
		"  x 4 sub exch 4 add moveto show grestore\n"
		"} bind def\n"

		"\n/coda {  %% usage: y coda - coda\n"
		"  gsave 1.2 setlinewidth dup x exch moveto 0 20 rlineto stroke\n"
		"  dup 10 add x -10 add exch moveto 20 0 rlineto stroke\n"
		"  10 add x exch 6 0 360 arc 1.8 setlinewidth stroke grestore\n"
		"} bind def\n"

		"\n/sgno {  %% usage: y sgno - segno\n"
		"  x exch moveto gsave\n"
		"  0 3 rmoveto currentpoint currentpoint currentpoint\n"
		"  2.8 -0.54 2.89 1.18 1.39 1.63 rcurveto\n"
		"  -2.2 -0.9 -1.4 -3.15 2.76 -2.48 rcurveto\n"
		"  3.1 2.4 2.54 6.26 -7.71 13.5 rcurveto\n"
		"  0.5 3.6 3.6 3.24 5.4 2.5 rcurveto\n"
		"  -2.8 0.54 -2.89 -1.18 -1.39 -1.63 rcurveto\n"
		"  2.2 0.9 1.4 3.15 -2.76 2.48 rcurveto\n"
		"  -3.1 -2.4 -2.54 -6.26 7.71 -13.5 rcurveto\n"
		"  -0.5 -3.6 -3.6 -3.24 -5.4 -2.5 rcurveto\n"
		"  fill\n"
		"  moveto 0.6 setlinewidth -5.6 1.6 rmoveto 12.5 12.5 rlineto stroke\n"
		"  7.2 add exch -5.6 add exch 1 0 360 arc fill\n"
		"  8.4 add exch 7 add exch 1 0 360 arc fill grestore\n"
		"} bind def\n");
}

/* -- def_deco1 -- */
static void def_deco1(FILE *fp)
{
static float q[8][2] = {    /* for down bow sign */
	{-4,0},{-4,9},{4,9},{4,0},
	{-4,6},{-4,9},{4,9},{4,6} };

	float f1,f2;

	f1=f2=0.5;
	fprintf(fp, "\n/hld {  %% usage:  y hld  - fermata\n"
		"  2 add x exch 2 copy 1.5 add 1.3 0 360 arc moveto\n"
		"  -7.5 0 rmoveto\n"
		"  0.0 11.5 15.0 11.5 15.0 0.0 rcurveto\n"
		"  -0.25 0.00 rlineto\n"
		"  -1.25 9.00 -13.25 9.00 -14.50 0.00 rcurveto\n"
		"  fill\n} bind def\n");

	f1=f2=0.8;
	fprintf(fp, "\n/dnb {  %% usage:  y dnb  - down bow\n"
		"  x exch moveto\n");
	add_mv(fp,f1,f2,q,0);
	add_sg(fp,f1,f2,q,1,3);
	fprintf(fp, "   currentpoint stroke moveto\n");
	add_mv(fp,f1,f2,q,4);
	add_sg(fp,f1,f2,q,5,3);
	fprintf(fp, "   fill\n} bind def\n"

		"\n/upb {  %% usage:  y upb  - up bow\n"
		"  x exch moveto\n"
		"  -2.56 8.80 rmoveto\n"
		"  2.56 -8.80 rlineto\n"
		"  2.56 8.80 rlineto\n"
		"  stroke\n} bind def\n");
}

/* -- def_flags1 -- */
static void def_flags1(FILE *fp)
{
static float p[13][2] = {
	{0.0, 0.0},  {1.5, -3.0},  {1.0, -2.5},  {4.0, -6.0}, {9.0, -10.0},
	{9.0, -16.0}, {8.0, -20.0}, {7.0, -24.0}, {4.0, -26.0},
	{6.5, -21.5}, {9.0, -15.0}, {4.0, -9.0}, {0.0, -8.0} };

	float f1,f2;

	f1=f2=6.0/9.0;
	fprintf(fp, "\n/f1u {  %% usage:  len f1u  - single flag up\n"
		"  y add x %.1f add exch moveto\n", STEM_XOFF);
	add_mv(fp,f1,f2,p,0);
	add_cv(fp,f1,f2,p,1,4);
	fprintf(fp, "   fill\n} bind def\n");

	f1=1.2*f1;
#if 1
	f2 = -f2;
#else
	for (i=0;i<13;i++) p[i][1] = -p[i][1];
#endif
	fprintf(fp, "\n/f1d {  %% usage:  len f1d  - single flag down\n"
		"  neg y add x %.1f sub exch moveto\n", STEM_XOFF);
	add_mv(fp,f1,f2,p,0);
	add_cv(fp,f1,f2,p,1,4);
	fprintf(fp, "   fill\n} bind def\n");
}

/* -- def_flags2 -- */
static void def_flags2(FILE *fp)
{
static float p[13][2] = {
	{0.0, 0.0},
	{2.0, -5.0},  {9.0, -6.0}, {7.5, -18.0},
	{7.5, -9.0},  {1.5, -6.5}, {0.0, -6.5},
	{2.0, -14.0}, {9.0, -14.0}, {7.5, -26.0},
	{7.5, -17.0}, {1.5, -14.5}, {0.0, -14.0},
};

	float f1,f2;

	f1=f2=6.0/9.0;			/* up flags */
	fprintf(fp, "\n/f2u {  %% usage:  len f2u  - double flag up\n"
		"  y add x %.1f add exch moveto\n", STEM_XOFF);
	add_mv(fp,f1,f2,p,0);
	add_cv(fp,f1,f2,p,1,4);
	fprintf(fp, "   fill\n} bind def\n");

	f1 *= 1.2;			/* down flags */
#if 1
	f2 = -f2;
#else
	for (i=0;i<13;i++) p[i][1] = -p[i][1];
#endif
	fprintf(fp, "\n/f2d {  %% usage:  len f2d  - double flag down\n"
		"  neg y add x %.1f sub exch moveto\n", STEM_XOFF);
	add_mv(fp,f1,f2,p,0);
	add_cv(fp,f1,f2,p,1,4);
	fprintf(fp, "   fill\n} bind def\n");
}

/* -- def_xflags -- */
static void def_xflags(FILE *fp)
{
static float p[7][2] = {
	{0.0, 0.0},
	{2.0, -7.5},  {9.0, -7.5}, {7.5, -19.5},
	{7.5, -10.5}, {1.5, -8.0}, {0.0, -7.5}
};

	float f1,f2;

	f1=f2=6.0/9.0;			/* extra up flag */
	fprintf(fp, "\n/xfu {  %% usage:  len xfu  - extra flag up\n"
		"  y add x %.1f add exch moveto\n", STEM_XOFF);
	add_mv(fp,f1,f2,p,0);
	add_cv(fp,f1,f2,p,1,2);
	fprintf(fp, "   fill\n} bind def\n");

	f1 *= 1.2;			/* extra down flag */
#if 1
	f2 = -f2;
#else
	for (i=0;i<7;i++)
		p[i][1] = -p[i][1];
#endif
	fprintf(fp, "\n/xfd {  %% usage:  len xfd  - extra flag down\n"
		"  neg y add x %.1f sub exch moveto\n", STEM_XOFF);
	add_mv(fp,f1,f2,p,0);
	add_cv(fp,f1,f2,p,1,2);
	fprintf(fp, "   fill\n} bind def\n"

		"\n/f3d {dup f2d 9.5 sub xfd} bind def\n"

		"\n/f4d {dup dup f2d 9.5 sub xfd 14.7 sub xfd} bind def\n"

		"\n/f3u {dup f2u 9.5 sub xfu} bind def\n"

		"\n/f4u {dup dup f2u 9.5 sub xfu 14.7 sub xfu} bind def\n");
}

/* -- def_acc -- */
static void def_acc(FILE *fp)
{
static float p[12][2]={
	{-2,3},{6,6.5},{6,-1},{-2,-4.5},{4,0},{4,4},{-2,2},{-2,10},{-2,-4}};
static float q[14][2]={
	{4,4},{4,7},{-4,5},{-4,2},{4,4},{4,-5},{4,-2},{-4,-4},{-4,-7},{4,-5},
	{2,-10},{2,11.5},{-2,-11.5},{-2,10} };
static float r[14][2]={
	{-2.5,-6}, {2.5,-5}, {2.5,-2}, {-2.5,-3}, {-2.5,6},
	{-2.5,2}, {2.5,3}, {2.5,6}, {-2.5,5}, {-2.5,2},
	{-2.5,11}, {-2.5,-5.5},
	{2.5,5.5}, {2.5,-11} };
static float s[25][2]={
	{0.7,0},{3.9,3},{6,3},{6.2,6.2},{3,6},{3,3.9},
	{0,0.7},{-3,3.9},{-3,6},{-6.2,6.2},{-6,3},{-3.9,3},
	{-0.7,0},{-3.9,-3},{-6,-3},{-6.2,-6.2},{-3,-6},{-3,-3.9},
	{0,-0.7},{3,-3.9},{3,-6},{6.2,-6.2},{6,-3},{3.9,-3},
	{0.7,0} };

	float f1,f2;

	f2=8.0/9.0;
	f1=f2*0.9;
	fprintf(fp, "\n/ft0 { %% usage:  x y ft0  - flat sign\n"
		"  moveto\n");
	add_mv(fp,f1,f2,p,0);
	add_cv(fp,f1,f2,p,1,2);
	fprintf(fp, "  currentpoint fill moveto\n");
	add_mv(fp,f1,f2,p,7);
	add_sg(fp,f1,f2,p,8,1);
	fprintf(fp, "  stroke\n } bind def\n"
		"/ft { %% usage: dx ft  - flat relative to head\n"
		" neg x add y ft0 } bind def\n");

	f2=8.0/9.0;	/* more narrow flat sign for double flat */
	f1=f2*0.8;
	fprintf(fp, "\n/ftx { %% usage:  x y ftx  - narrow flat sign\n"
		"  moveto\n");
	add_mv(fp,f1,f2,p,0);
	add_cv(fp,f1,f2,p,1,2);
	fprintf(fp, "  currentpoint fill moveto\n");
	add_mv(fp,f1,f2,p,7);
	add_sg(fp,f1,f2,p,8,1);
	fprintf(fp, "  stroke\n } bind def\n"
		"/dft0 { %% usage: x y dft0 ft  - double flat sign\n"
		"  2 copy exch 2.5 sub exch ftx exch 1.5 add exch ftx } bind def\n"
		"/dft { %% usage: dx dft  - double flat relative to head\n"
		"  neg x add y dft0 } bind def\n");

	f2=6.5/9.0;
	f1=f2*0.9;
	fprintf(fp, "\n/sh0 {  %% usage:  x y sh0  - sharp sign\n"
		"  moveto\n");
	add_mv(fp,f1,f2,q,0);
	add_sg(fp,f1,f2,q,1,4);
	add_mv(fp,f1,f2,q,5);
	add_sg(fp,f1,f2,q,6,4);
	fprintf(fp, "  currentpoint fill moveto\n");
	add_mv(fp,f1,f2,q,10);
	add_sg(fp,f1,f2,q,11,1);
	fprintf(fp, "  currentpoint stroke moveto\n");
	add_mv(fp,f1,f2,q,12);
	add_sg(fp,f1,f2,q,13,1);
	fprintf(fp, "  stroke\n } bind def\n");
	fprintf(fp, "/sh { %% usage: dx sh  - sharp relative to head\n"
		" neg x add y sh0 } bind def\n");

	f2=6.5/9.0;
	f1=f2*0.9;
	fprintf(fp, "\n/nt0 {  %% usage:  x y nt0  - neutral sign\n"
		"  moveto\n");
	add_mv(fp,f1,f2,r,0);
	add_sg(fp,f1,f2,r,1,4);
	add_mv(fp,f1,f2,r,5);
	add_sg(fp,f1,f2,r,6,4);
	fprintf(fp, "  currentpoint fill moveto\n");
	add_mv(fp,f1,f2,r,10);
	add_sg(fp,f1,f2,r,11,1);
	fprintf(fp, "  currentpoint stroke moveto\n");
	add_mv(fp,f1,f2,r,12);
	add_sg(fp,f1,f2,r,13,1);
	fprintf(fp, "  stroke\n } bind def\n"
		"/nt { %% usage: dx nt  - neutral relative to head\n"
		" neg x add y nt0 } bind def\n");

	f1=5.0/9.0;
	f2=f1;
	fprintf(fp, "\n/dsh0 {  %% usage:  x y dsh0  - double sharp\n"
		"  moveto\n");
	add_mv(fp,f1,f2,s,0);
	add_sg(fp,f1,f2,s,1,24);
	fprintf(fp, "  fill\n } bind def\n"
		"/dsh { %% usage: dx dsh  - double sharp relative to head\n"
		" neg x add y dsh0 } bind def\n");
}

/* -- def_rests -- */
static void def_rests(FILE *fp)
{
static float p[14][2]={
	{-1,17}, {15,4}, {-6,8}, {6.5,-5}, {-2,-2}, {-5,-11}, {1,-15},
	{-9,-11}, {-6,0}, {1,-1}, {-9,7}, {7,5}, {-1,17} };
static float q[16][2]={
	{8,14}, {5,9}, {3,5}, {-1.5,4},
	{4,11}, {-9,14}, {-9,7},
	{-9,4}, {-6,2}, {-3,2},
	{4,2}, {5,7}, {7,11},
	{-1.8,-20},  {-0.5,-20}, {8.5,14}};
static float r[29][2]={  
	{8,14}, {5,9}, {3,5}, {-1.5,4},
	{4,11}, {-9,14}, {-9,7},
	{-9,4}, {-6,2}, {-3,2},
	{4,2}, {5,7}, {7,11},
	{8,14}, {5,9}, {3,5}, {-1.5,4},
	{4,11}, {-9,14}, {-9,7},
	{-9,4}, {-6,2}, {-3,2},
	{4,2}, {5,7}, {7.3,11},
	{-1.8,-21},  {-0.5,-21}, {8.5,14} };
	float f1,f2;
	int i;

	fprintf(fp, "\n/r4 {  %% usage:  x y r4  -  quarter rest\n"
		"   dup /y exch def exch dup /x exch def exch moveto\n");
	f1=f2=6.0/11.5;
	add_mv(fp,f1,f2,p,0);
	add_cv(fp,f1,f2,p,1,4);
	fprintf(fp, "  fill\n } bind def\n");

	fprintf(fp, "\n/r8 {  %% usage:  x y r8  -  eighth rest\n"
		"   dup /y exch def exch dup /x exch def exch moveto\n");
	f1=f2=7/18.0;
	add_mv(fp,f1,f2,q,0);
	add_cv(fp,f1,f2,q,1,4);
	add_sg(fp,f1,f2,q,13,3);
	fprintf(fp, "  fill\n } bind def\n");

	for (i = 13; i < 26; i++) {
		r[i][0] -= 4.2;
		r[i][1] -= 14;
	}
	fprintf(fp, "\n/r16 {  %% usage:  x y r16  -  16th rest\n"
		"   dup /y exch def exch dup /x exch def exch moveto\n");
	f1=f2=7/18.0;
	add_mv(fp,f1,f2,r,0);
	add_cv(fp,f1,f2,r,1,4);
	add_sg(fp,f1,f2,r,13,1);
	add_cv(fp,f1,f2,r,14,4);
	add_sg(fp,f1,f2,r,26,3);
	fprintf(fp, "  fill\n } bind def\n");

	fprintf(fp,
		"\n/r0 {  %% usage:	x y r0	-  double rest\n"
		"  6 add dup /y exch def exch dup /x exch def exch moveto\n"
		"  -1 0 rmoveto 0 -6 rlineto 3 0 rlineto 0 6 rlineto fill\n"
		"} bind def\n"
		"\n/r1 {  %% usage:	x y r1	-  rest\n"
		"  6 add dup /y exch def exch dup /x exch def exch moveto\n"
		"  -3 0 rmoveto 0 -3 rlineto 6 0 rlineto 0 3 rlineto fill\n"
		"} bind def\n"
		"\n/r2 {  %% usage:	x y r2	-  half rest\n"
		"  dup /y exch def exch dup /x exch def exch moveto\n"
		"  -3 0 rmoveto 0 3 rlineto 6 0 rlineto 0 -3 rlineto fill\n"
		"} bind def\n");

	/* get 32nd, 64th rest by overwriting 8th and 16th rests */
	fprintf(fp,
		"\n/r32 {\n"
		"2 copy r16 5.5 add exch 1.6 add exch r8\n"
		"} bind def\n"

		"\n/r64 {\n"
		"2 copy 5.5 add exch 1.6 add exch r16\n"
		"5.5 sub exch 1.5 sub exch r16\n"
		"} bind def\n");

	for (i = 13; i < 26; i++) {
		r[i][0] += 4.2;
		r[i][1] += 14;
	}
}

/* -- def_ends -- */
static void def_ends(FILE *fp)
{
	/* old style had dy=20 */
	int y=50,dy=20;

	fprintf(fp, "\n/end1 {  %% usage: x1 x2 (str) y end1  - mark first ending\n"
		"  gsave 0 exch translate\n"
		"  3 1 roll %d moveto 0 %d rlineto dup %d lineto 0 %d rlineto stroke\n"
		"  4 add %d moveto /Times-Roman 13 selectfont 1.2 0.95 scale\n"
		"  show grestore\n"
		"} bind def\n",
		y-dy,  dy, y, -dy,  y-10);

	fprintf(fp, "\n/end2 {  %% usage: x1 x2 (str) y end2  - mark second ending\n"
		"  gsave 0 exch translate\n"
		"  3 1 roll %d moveto dup %d lineto 0 %d rlineto stroke\n"
		"  4 add %d moveto /Times-Roman 13 selectfont 1.2 0.95 scale\n"
		"  show grestore\n"
		"} bind def\n",
		y, y, -dy,  y-10);
}  

/* -- def_sl -- */
static void def_sl(FILE *fp)
{
/*	fprintf(fp, "\n/sl {  %% usage: x1 y2 x2 y2 x3 y3 x0 y0 sl\n"
		"  gsave %.1f setlinewidth moveto curveto stroke grestore\n"
		"} bind def\n", SLURWIDTH); */

	fprintf(fp, "\n/SL {	%% usage: pp2x pp1x p1 pp1 pp2 p2 p1 sl\n"
		"  moveto curveto rlineto curveto fill\n"
		"} bind def\n");
}

/* -- def_hd1 -- */
static void def_hd1(FILE *fp)
{
#if 1
	fprintf(fp, "\n/hd {  %% usage: x y hd  - full head\n"
		"  2 copy /y exch def /x exch def moveto\n"
		"  3.30 2.26 rmoveto\n"
		"  -2.26 3.30 -8.86 -1.22 -6.60 -4.52 rcurveto\n"
		"  2.26 -3.30 8.86 1.22 6.60 4.52 rcurveto\n"
		"  fill\n} bind def\n");
#else
static float p[7][2] = {
	{8.0, 0.0},  {8.0, 8.0}, {-8.0, 8.0}, {-8.0, 0.0}, {-8.0, -8.0},
	{8.0, -8.0}, {8.0, 0.0} };

	float c,s,xx,yy,f1,f2;
	int i;
/*float phi; */

/*phi=0.6;
  c=cos(phi);
  s=sin(phi); */

	c=0.825; s=0.565;

	for (i=0;i<7;i++) {
		xx = c*p[i][0] - s*p[i][1];
		yy = s*p[i][0] + c*p[i][1];
		p[i][0]=xx;
		p[i][1]=yy;
	}

	f1=f2=6.0/12.0;
	fprintf(fp, "\n/hd {  %% usage: x y hd  - full head\n"
		"  dup /y exch def exch dup /x exch def exch moveto\n");
	add_mv(fp,f1,f2,p,0);
	add_cv(fp,f1,f2,p,1,2);
	fprintf(fp, "   fill\n} bind def\n");
#endif
}

/* -- def_hd2 -- */
static void def_hd2(FILE *fp)
{
#if 1
	fprintf(fp, "\n/Hd {  %% usage: x y Hd  - open head for half\n"
		"  2 copy /y exch def /x exch def moveto\n"
		"  3.51 1.92 rmoveto\n"
		"  -2.04 3.73 -9.06 -0.10 -7.02 -3.83 rcurveto\n"
		"  2.04 -3.73 9.06 0.10 7.02 3.83 rcurveto\n"
		"  -0.44 -0.24 rmoveto\n"
		"  0.96 -1.76 -5.19 -5.11 -6.15 -3.35 rcurveto\n"
		"  -0.96 1.76 5.19 5.11 6.15 3.35 rcurveto\n"
		"  fill\n} bind def\n");
#else
static float p[14][2] = {
	{8.0, 0.0},  {8.0, 8.5},  {-8.0, 8.5}, {-8.0, 0.0}, {-8.0, -8.5},
	{8.0, -8.5}, {8.0, 0.0},  {7.0, 0.0},  {7.0, -4.0}, {-7.0, -4.0},
	{-7.0, 0.0}, {-7.0, 4.0}, {7.0, 4.0},  {7.0, 0.0} };

/*  float phi; */
	float c,s,xx,yy,f1,f2;
	int i;

/*phi=0.5;
  c=cos(phi);
  s=sin(phi); */

	c=0.878; s=0.479;

	for (i=0;i<14;i++) {
		xx = c*p[i][0] - s*p[i][1];
		yy = s*p[i][0] + c*p[i][1];
		p[i][0]=xx;
		p[i][1]=yy;
	}

	f1=f2=6.0/12.0;
	fprintf(fp, "\n/Hd {  %% usage: x y Hd  - open head for half\n"
		"  dup /y exch def exch dup /x exch def exch moveto\n");
	add_mv(fp,f1,f2,p,0);
	add_cv(fp,f1,f2,p,1,2);
	add_mv(fp,f1,f2,p,7);
	add_cv(fp,f1,f2,p,8,2);
	fprintf(fp, "   fill\n} bind def\n");
#endif
}

/* -- def_hd3 -- */
static void def_hd3(FILE *fp)
{
#if 1
	fprintf(fp, "\n/HD { %% usage: x y HD  - open head for whole\n"
		"  2 copy /y exch def /x exch def moveto\n"
		"  5.96 0.00 rmoveto\n"
		"  0.00 1.08 -2.71 3.52 -5.96 3.52 rcurveto\n"
		"  -3.25 0.00 -5.96 -2.44 -5.96 -3.52 rcurveto\n"
		"  0.00 -1.08 2.71 -3.52 5.96 -3.52 rcurveto\n"
		"  3.25 0.00 5.96 2.44 5.96 3.52 rcurveto\n"
		"  -8.13 1.62 rmoveto\n"
		"  1.62 2.17 5.96 -1.07 4.34 -3.24 rcurveto\n"
		"  -1.62 -2.17 -5.96 1.07 -4.34 3.24 rcurveto\n"
		"  fill\n} bind def\n"
		"\n/HDD {  %% usage: x y HDD - semibreve\n"
		"  HD\n"
		"  x y moveto -6 -4 rmoveto 0 8 rlineto stroke\n"
		"  x y moveto 6 -4 rmoveto 0 8 rlineto stroke\n"
		"} bind def\n");
#else
static float p[13][2] = {
	{11.0, 0.0}, {11.0, 2.0},  {6.0, 6.5},  {0.0, 6.5}, {-6.0, 6.5},
	{-11.0, 2.0}, {-11.0, 0.0}, {-11.0, -2.0}, {-6.0, -6.5},
	{0.0, -6.5},  {6.0, -6.5}, {11.0, -2.0},  {11.0, 0.0} };

static float q[8][2] = {
	 {11.0, 0.0}, {5.0, 0.0}, {5.0, -5.0}, {-5.0, -5.0}, {-5.0, 0.0},
	 {-5.0, 5.0}, {5.0, 5.0}, {5.0, 0.0}};

/*  float phi; */
	float c,s,xx,yy,f1,f2;
	int i;

/*phi=2.5;
  c=cos(phi);
  s=sin(phi); */

	c=-0.801; s=0.598;

	for (i=1;i<8;i++) {
		xx = c*q[i][0] - s*q[i][1];
		yy = s*q[i][0] + c*q[i][1];
		q[i][0]=xx;
		q[i][1]=yy;
	}

	f1=f2=6.5/12.0;
	fprintf(fp, "\n/HD { %% usage: x y HD  - open head for whole\n"
		"  dup /y exch def exch dup /x exch def exch moveto\n");
	add_mv(fp,f1,f2,p,0);
	add_cv(fp,f1,f2,p,1,4);
	add_mv(fp,f1,f2,q,1);
	add_cv(fp,f1,f2,q,2,2);
	fprintf(fp, "   fill\n} bind def\n"
		"/HDD {  %% usage: x y HDD - double note\n"
		"  HD\n"
		"  x y moveto -6 -4 rmoveto 0 8 rlineto stroke\n"
		"  x y moveto 6 -4 rmoveto 0 8 rlineto stroke\n"
		"} bind def\n");
#endif
}

/* -- def_gnote -- */
static void def_gnote(FILE *fp)
{
#if 1
	fprintf(fp, "\n/gn1 {  %% usage: x y l gn1 - grace note w. tail\n"
		"  3 1 roll 2 copy moveto\n"
		"  -1.29 1.53 rmoveto\n"
		"  2.45 2.06 5.02 -1.00 2.58 -3.06 rcurveto\n"
		"  -2.45 -2.06 -5.02 1.00 -2.58 3.06 rcurveto\n"
		"  fill moveto %.2f 0 rmoveto 0 exch rlineto\n"
		"  3 -4 4 -5 2 -8 rcurveto\n"
		"  stroke\n",
		GSTEM_XOFF);
	fprintf(fp, "} bind def\n"

		"\n/gn1s {  %% usage: x y l gn1s - short appoggiatura\n"
		"  3 1 roll 2 copy moveto\n"
		"  -1.29 1.53 rmoveto\n"
		"  2.45 2.06 5.02 -1.00 2.58 -3.06 rcurveto\n"
		"  -2.45 -2.06 -5.02 1.00 -2.58 3.06 rcurveto\n"
		"  fill moveto %.2f 0 rmoveto 0 exch rlineto\n"
		"  3 -4 4 -5 2 -8 rcurveto -5 2 rmoveto 7 4 rlineto\n"
		"  stroke\n",
		GSTEM_XOFF);
	fprintf(fp, "} bind def\n"

		"\n/gnt {  %% usage: x y l gnt - grace note\n"
		"  3 1 roll 2 copy moveto\n"
		"  -1.29 1.53 rmoveto\n"
		"  2.45 2.06 5.02 -1.00 2.58 -3.06 rcurveto\n"
		"  -2.45 -2.06 -5.02 1.00 -2.58 3.06 rcurveto\n"
		"  fill moveto %.2f 0 rmoveto 0 exch rlineto stroke\n",
		GSTEM_XOFF);
#else
static float p[7][2] = {
	{0,10}, {16,10}, {16,-10}, {0,-10}, {-16,-10}, {-16,10}, {0,10} };
/*  float phi; */
	float c, s, xx, yy, f1, f2;
	int i;

/*phi=0.7;
  c=cos(phi);
  s=sin(phi); */

	c = 0.765; s = 0.644;

	for (i = 0; i < 7; i++) {
		xx = c*p[i][0] - s*p[i][1];
		yy = s*p[i][0] + c*p[i][1];
		p[i][0] = xx;
		p[i][1] = yy;
	}

	f1 = f2 = 2. / 10.0;

	fprintf(fp, "\n/gn1 {  %% usage: x y l gn1 - grace note w. tail\n"
		"  3 1 roll 2 copy moveto\n");
	add_mv(fp, f1, f2, p, 0);
	add_cv(fp, f1, f2, p, 1, 2);
	fprintf(fp, "  fill moveto %.2f 0 rmoveto 0 exch rlineto\n"
		"  3 -4 4 -5 2 -8 rcurveto\n"
		"  stroke\n",
		GSTEM_XOFF);
	fprintf(fp, "} bind def\n"

		"\n/gn1s {  %% usage: x y l gn1s - short appoggiatura\n"
		"  3 1 roll 2 copy moveto\n");
	add_mv(fp, f1, f2, p, 0);
	add_cv(fp, f1, f2, p, 1, 2);
	fprintf(fp, "  fill moveto %.2f 0 rmoveto 0 exch rlineto\n"
		"  3 -4 4 -5 2 -8 rcurveto -5 2 rmoveto 7 4 rlineto\n"
		"  stroke\n",
		GSTEM_XOFF);
	fprintf(fp, "} bind def\n"

		"\n/gnt {  %% usage: x y l gnt - grace note\n"
		"  3 1 roll 2 copy moveto\n");
	add_mv(fp, f1, f2, p, 0);
	add_cv(fp, f1, f2, p, 1, 2);
	fprintf(fp, "  fill moveto %.2f 0 rmoveto 0 exch rlineto stroke\n",
		GSTEM_XOFF);
#endif
	fprintf(fp, "} bind def\n"

		"\n/gbm2 {  %% usage: x1 y1 x2 y2 gbm2 - double gnote beam\n"
		"  gsave 1.4 setlinewidth\n"
		"  4 copy 0.5 sub moveto 0.5 sub lineto stroke\n"
		"  3.4 sub moveto 3.4 sub lineto stroke grestore\n"
		"} bind def\n"

		"\n/gbm3 {  %% usage: x1 y1 x2 y2 gbm3  - triple gnote beam\n"
		"  gsave 1.2 setlinewidth\n"
		"  4 copy 0.3 sub moveto 0.3 sub lineto stroke\n"
		"  4 copy 2.5 sub moveto 2.5 sub lineto stroke\n"
		"  4.7 sub moveto 4.7 sub lineto stroke grestore\n"
		"} bind def\n"

		"\n/ghl {  %% usage: x y ghl  - grace note helper line\n"
		"   gsave 0.7 setlinewidth moveto \n"
		"   -3 0 rmoveto 6 0 rlineto stroke grestore\n"
		"} bind def\n"

		"\n/gsl {  %% usage: x1 y2 x2 y2 x3 y3 x0 y0 gsl\n"
		"  moveto curveto stroke\n"
		"} bind def\n"

		"\n/gsh0 {  %% usage: x y gsh0\n"
		"  gsave translate 0.7 0.7 scale 0 0 sh0 grestore\n"
		"} bind def\n"

		"\n/gft0 {  %% usage: x y gft0\n"
		"  gsave translate 0.7 0.7 scale 0 0 ft0 grestore\n"
		"} bind def\n"

		"\n/gnt0 {  %% usage: x y gnt0\n"
		"  gsave translate 0.7 0.7 scale 0 0 nt0 grestore\n"
		"} bind def\n");
}

/* -- def_cclef -- */
static void def_cclef(FILE *fp)
{
	fprintf(fp, "\n/cchalf {\n"
		" 0 0 moveto\n"
		" 0.00 12.00 rmoveto\n"
		" 1.2 2.75 rlineto\n"
		" 3.0125 -1.8125 5.125 -1.0 5.5 3.125 rcurveto\n"
		" 0.3125 3.9375 -0.6625 6.2125 -4.05 6.375 rcurveto\n"
		" -2.4675 -0.9375 -3.1375 -2.9375 -0.35 -1.75 rcurveto\n"
		" 0.9625 -2.9375 -1.9875 -4.1875 -3.0875 -0.0625 rcurveto\n"
		" 2.15 2.5 7.7475 2.2625 9.7125 -1.4375 rcurveto\n"
		" 0.225 -5.875 -2.9125 -8.3125 -6.725 -7.425 rcurveto\n"
		"  -0.60 -2.50 rlineto\n"
		"fill\n} bind def\n"

		"\n/cclef {	 %% usage: x y cclef\n"
		"  gsave translate\n"
		"  cchalf 0 24 translate 1 -1 scale cchalf\n"
		"  -6.5 0 moveto 0 24 rlineto 3 0 rlineto 0 -24 rlineto fill\n"
		"  -1.8 0 moveto 0 24 rlineto 0.8 setlinewidth stroke grestore\n"
		"} bind def\n"

		"\n/scclef { cclef } bind def\n");
}

/* ----- define_symbols: write postscript macros to file ------ */
void define_symbols(FILE *fp)
{
	fprintf(fp, def_misc);
	fprintf(fp, def_tclef);
	fprintf(fp, def_bclef);
	def_cclef(fp);
	def_hd1(fp);
	def_hd2(fp); 
	def_hd3(fp); 
	def_stems(fp);
	fprintf(fp, def_beam);
	def_sl(fp);
	fprintf(fp, def_dot);
	def_deco(fp);
	def_deco1(fp);
	fprintf(fp, def_hl);
	def_flags1(fp);
	def_flags2(fp);
	def_xflags(fp);
	def_acc(fp);
	fprintf(fp, def_gchord);
	def_rests(fp);
	fprintf(fp, def_bars);
	def_ends(fp);
	def_gnote(fp);
	fprintf(fp, def_csig);
	fprintf(fp, def_sep);
	fprintf(fp, def_tsig);
	fprintf(fp, def_staff);
	fprintf(fp, def_typeset);
}
