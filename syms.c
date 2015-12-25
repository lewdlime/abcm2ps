/*
 * This file is part of abcm2ps.
 * Copyright (C) 1998-2002 Jean-François Moine
 * Adapted from abc2ps, Copyright (C) 1996,1997 Michael Methfessel
 * See file abc2ps.c for details.
 */

#include <stdio.h>
#include <time.h>

#include "abcparse.h"
#include "abc2ps.h"

/*  subroutines to define postscript macros which draw symbols	*/

static char ps_head[] =
	"\n/cshow { % usage: str cshow  - center at current pt\n"
	"	dup stringwidth pop 2 div neg 0 rmoveto show\n"
	"} bdef\n"

	"\n/lshow { % usage: str lshow - show left-aligned\n"
	"	dup stringwidth pop neg 0 rmoveto show\n"
	"} bdef\n"

	"\n/showb { % usage: str showb - show in box\n"
	"	dup currentpoint 3 -1 roll show\n"
	"	0.6 setlinewidth\n"
#if PS_LEVEL >= 2
	"	exch 2 sub exch 3 sub 3 -1 roll\n"
	"	stringwidth pop 4 add fh 4 add rectstroke\n"
#else
	"	M -2 -3 rmoveto stringwidth pop 4 add fh 4 add 2 copy\n"
	"	0 exch rlineto 0 rlineto neg 0 exch rlineto neg 0 rlineto stroke\n"
#endif
	"} bdef\n"

#if 0
	"\n/cshowb { % usage: str cshowb - show centered in box\n"
	"	dup stringwidth pop dup 2 div neg 0 rmoveto currentpoint 4 -1 roll show\n"
	"	0.6 setlinewidth\n"
#if PS_LEVEL >= 2
	"	exch 2 sub exch 3 sub 3 -1 roll\n"
	"	4 add fh 4 add rectstroke\n"
#else
	"	M -2 -3 rmoveto 4 add fh 4 add 2 copy\n"
	"	0 exch rlineto 0 rlineto neg 0 exch rlineto neg 0 rlineto stroke\n"
#endif
	"} bdef\n"
#endif

	"\n/wd { M show } bdef\n"
	"/wln {	% usage: l x y wln\n"
	"	M 0.8 setlinewidth 0 rlineto stroke\n"
	"} bdef\n"

	"/whf {3 add 3 3 1 roll wln} bdef\n"

	"\n/tclef {  % usage:  x y tclef  - treble clef\n"
	" M\n"
	" -1.9 3.7 rmoveto\n"
	" -3.3 1.9 -3.1 6.8 2.4 8.6 rcurveto\n"
	" 7.0 0.0 9.8 -8.0 4.1 -11.7 rcurveto\n"
	" -5.2 -2.4 -12.5 0.0 -13.3 6.2 rcurveto\n"
	" -0.7 6.4 4.15 10.5 10.0 15.3 rcurveto\n"
	" 4.0 4.0 3.6 6.1 2.8 9.6 rcurveto\n"
	" -2.3 -1.5 -4.7 -4.8 -4.5 -8.5 rcurveto\n"
	" 0.8 -12.2 3.4 -17.3 3.5 -26.3 rcurveto\n"
	" 0.3 -4.4 -1.2 -6.2 -3.8 -6.2 rcurveto\n"
	" -3.7 -0.1 -5.8 4.3 -2.8 6.1 rcurveto\n"
	" 3.9 1.9 6.1 -4.6 1.4 -4.8 rcurveto\n"
	" 0.7 -1.2 4.6 -0.8 4.2 4.2 rcurveto\n"
	" -0.2 10.3 -3.0 15.7 -3.5 28.3 rcurveto\n"
	" 0.0 4.1 0.6 7.4 5.0 10.6 rcurveto\n"
	" 2.3 -3.2 2.9 -10.0 1.0 -12.7 rcurveto\n"
	" -2.4 -4.3 -11.5 -10.3 -11.8 -15.0 rcurveto\n"
	" 0.4 -7.0 6.9 -8.5 11.7 -6.1 rcurveto\n"
	" 3.9 3.0 1.3 8.8 -3.7 8.1 rcurveto\n"
	" -4.0 -0.2 -4.8 -3.1 -2.7 -5.7 rcurveto\n"
	"  fill\n} bdef\n"

	"\n/stclef {\n"
	"	exch 0.85 div exch 0.85 div gsave 0.85 0.85 scale tclef grestore\n"
	"} bdef\n"

	"\n/bclef {  % usage:  x y bclef  - bass clef\n"
	" M\n"
	" -8.8 3.5 rmoveto\n"
	" 6.3 1.9 10.2 5.6 10.5 10.8 rcurveto\n"
	" 0.3 4.9 -0.5 8.1 -2.6 8.8 rcurveto\n"
	" -2.5 1.2 -5.8 -0.7 -5.9 -4.1 rcurveto\n"
	" 1.8 3.1 6.1 -0.6 3.1 -3.0 rcurveto\n"
	" -3.0 -1.4 -5.7 2.3 -1.9 7.0 rcurveto\n"
	" 2.6 2.3 11.4 0.6 10.1 -8.0 rcurveto\n"
	" -0.1 -4.6 -5.0 -10.2 -13.3 -11.5 rcurveto\n"
	" 15.5 17.0 rmoveto\n"
	" 0.0 1.5 2.0 1.5 2.0 0.0 rcurveto\n"
	" 0.0 -1.5 -2.0 -1.5 -2.0 0.0 rcurveto\n"
	" 0.0 -5.5 rmoveto\n"
	" 0.0 1.5 2.0 1.5 2.0 0.0 rcurveto\n"
	" 0.0 -1.5 -2.0 -1.5 -2.0 0.0 rcurveto\n"
	"  fill\n} bdef\n"

	"\n/sbclef {\n"
	"	exch 0.85 div exch 0.85 div gsave 0.85 0.85 scale 0 3 T bclef grestore\n"
	"} bdef\n"

	"\n/cchalf {\n"
	" 0 0 M 0.0 12.0 rmoveto\n"
	" 2.6 5.0 rlineto\n"
	" 2.3 -5.8 5.2 -2.4 4.7 1.6 rcurveto\n"
	" 0.4 3.9 -3.0 6.7 -5.1 4.0 rcurveto\n"
	" 4.1 0.5 0.9 -5.3 -0.9 -1.4 rcurveto\n"
	" -0.5 3.4 6.5 4.3 7.8 -0.8 rcurveto\n"
	" 1.9 -5.6 -4.1 -9.8 -6.0 -5.4 rcurveto\n"
	" -1.6 -3.0 rlineto\n"
	"fill\n} bdef\n"

	"\n/cclef { % usage: x y cclef\n"
	"	gsave T\n"
	"	cchalf 0 24 T 1 -1 scale cchalf\n"
	"	-5.0 0 M 0 24 rlineto 3 0 rlineto 0 -24 rlineto fill\n"
	"	-0.5 0 M 0 24 rlineto 0.8 setlinewidth stroke grestore\n"
	"} bdef\n"

	"\n/scclef {\n"
	"	exch 0.85 div exch 0.85 div gsave 0.85 0.85 scale 0 2 T cclef grestore"
	"} bdef\n"

	"\n/bm {  % usage: t dx dy x y bm  - beam, depth t\n"
	"	M 3 copy rlineto neg 0 exch rlineto\n"
	"	neg exch neg exch rlineto 0 exch rlineto fill\n"
	"} bdef\n"

	"\n/bnum {  % usage: str x y bnum - number on beam\n"
	"	M /Times-Italic 12 selectfont cshow\n"
	"} bdef\n"
  
	"\n/hbr {  % usage: x1 y1 x2 y2 hbr - half bracket\n"
	"	M dlw lineto 0 -3 rlineto stroke\n"
	"} bdef\n"

	"\n/dt {  % usage: dx dy dt - dot shifted by dx,dy\n"
	"	y add exch x add exch M currentpoint 1.2 0 360 arc fill\n"
	"} bdef\n"

	"\n/grm {  % usage: x y grm - gracing mark\n"
	"	M -5 2.5 rmoveto\n"
	"	5.0 8.5 5.5 -4.5 10.0 2.0 rcurveto\n"
	"	-5.0 -8.5 -5.5 4.5 -10.0 -2.0 rcurveto fill\n"
	"} bdef\n"

	"\n/stc {  % usage:  x y stc  - staccato mark\n"
	"	M currentpoint 1.2 0 360 arc fill\n"
	"} bdef\n"

	"\n/emb {  % usage: x y emb - emphasis bar\n"
	"	1.2 setlinewidth 1 setlinecap M\n"
	"	-2.5 0 rmoveto 5 0 rlineto stroke\n"
	"} bdef\n"

	"\n/cpu {  % usage: x y cpu - roll sign above head\n"
	"	M -6 0 rmoveto\n"
	"	0.45 7.3 11.25 7.3 11.7 0 rcurveto\n"
	"	-1.35 6 -10.35 6 -11.7 0 rcurveto fill\n"
	"} bdef\n"

	"\n/sld {  % usage: x y sld  - slide\n"
	"	M -7.2 -4.8 rmoveto\n"
	"	1.8 -0.7 4.5 0.2 7.2 4.8 rcurveto\n"
	"	-2.1 -5.0 -5.4 -6.8 -7.65 -6.0 rcurveto fill\n"
	"} bdef\n"
  
	"\n/trl {  % usage: x y trl - trill sign\n"
	"	/Times-BoldItalic 14 selectfont\n"
	"	M -4 1 rmoveto (tr) show\n"
	"} bdef\n"

	"\n/umrd {  % usage: x y umrd - upper mordent\n"
	"	4 add M\n"
	"	2.2 2.2 rlineto 2.18 -3.0 rlineto 0.62 0.8 rlineto\n"
	"	-2.2 -2.2 rlineto -2.18 3.0 rlineto -0.62 -0.8 rlineto\n"
	"	-2.2 -2.2 rlineto -2.18 3.0 rlineto -0.62 -0.8 rlineto\n"
	"	2.2 2.2 rlineto 2.18 -3.0 rlineto 0.62 0.8 rlineto fill\n"
	"} bdef\n"

	"\n/lmrd {  % usage: x y lmrd - lower mordent\n"
	"	2 copy umrd 8 add M\n"
	"	0.6 setlinewidth 0 -8 rlineto stroke\n"
	"} bdef\n"

	"\n/fng {  % usage:  str x y fng - finger (0-5)\n"
	"	/Times-Roman 10 selectfont\n"
	"	M -3 1 rmoveto show\n"
	"} bdef\n"

	"\n/dacs {  % usage:  str x y dacs - D.C. / D.S.\n"
	"	/Times-Roman 16 selectfont\n"
	"	1 add M cshow\n"
	"} bdef\n"

	"\n/brth {  % usage: x y brth - breath\n"
	"	/Times-BoldItalic 30 selectfont\n"
	"	6 add M (,) show\n"
	"} bdef\n"
		
	"\n/pf {  % usage: str x y pf - p, f, pp, ..\n"
	"	/Times-BoldItalic 16 selectfont\n"
	"	5 add M cshow\n"
	"} bdef\n"

	"\n/sfz {  % usage: str x y sfz\n"
	"	exch 4 sub exch 5 add M pop\n"
	"	/Times-Italic 14 selectfont (s) show\n"
	"	/Times-BoldItalic 16 selectfont (f) show\n"
	"	/Times-Italic 14 selectfont (z) show\n"
	"} bdef\n"

	"\n/crdc {  % usage: str x y crdc - cresc, decresc, ..\n"
	"	/Times-Italic 14 selectfont\n"
	"	exch 4 sub exch 4 add M show\n"
	"} bdef\n"

	"\n/coda {  % usage: x y coda - coda\n"
	"	1.2 setlinewidth 2 copy M 0 20 rlineto\n"
	"	2 copy 10 add exch -10 add exch M 20 0 rlineto stroke\n"
	"	10 add 6 0 360 arc 1.8 setlinewidth stroke\n"
	"} bdef\n"

	"\n/sgno {  % usage: x y sgno - segno\n"
	"	M\n"
	"	0 3 rmoveto currentpoint currentpoint currentpoint\n"
	"	2.8 -0.54 2.89 1.18 1.39 1.63 rcurveto\n"
	"	-2.2 -0.9 -1.4 -3.15 2.76 -2.48 rcurveto\n"
	"	3.1 2.4 2.54 6.26 -7.71 13.5 rcurveto\n"
	"	0.5 3.6 3.6 3.24 5.4 2.5 rcurveto\n"
	"	-2.8 0.54 -2.89 -1.18 -1.39 -1.63 rcurveto\n"
	"	2.2 0.9 1.4 3.15 -2.76 2.48 rcurveto\n"
	"	-3.1 -2.4 -2.54 -6.26 7.71 -13.5 rcurveto\n"
	"	-0.5 -3.6 -3.6 -3.24 -5.4 -2.5 rcurveto\n"
	"	fill\n"
	"	M 0.6 setlinewidth -5.6 1.6 rmoveto 12.5 12.5 rlineto stroke\n"
	"	7.2 add exch -5.6 add exch 1 0 360 arc fill\n"
	"	8.4 add exch 7 add exch 1 0 360 arc fill\n"
	"} bdef\n"

	"\n/cresc {  % usage: w x y cresc - (de)crescendo\n"
	"	1.2 setlinewidth 6 add M\n"
	"	dup 4 rlineto neg 4 rlineto stroke\n"
	"} bdef\n"

	"\n/dplus {  % usage: x y dplus - + decoration\n"
	"	1.2 setlinewidth M 0 0.5 rmoveto 0 6 rlineto\n"
	"	-3 -3 rmoveto 6 0 rlineto stroke\n"
	"} bdef\n"

	"\n/accent {  % usage: x y accent - accent\n"
	"	1.2 setlinewidth M -4 1 rmoveto\n"
	"	8 2 rlineto -8 2 rlineto stroke\n"
	"} bdef\n"

	"\n/turn {  % usage: x y turn - turn\n"
	"	M -3.5 5 rmoveto\n"
	"	-4.2 -5.5 -5.2 8.6 4.1 1.3 rcurveto\n"
	"	6.0 -4.5 6.0 2.1 2.4 0.7 rcurveto\n"
	"	4.2 5.5 5.4 -8.6 -4.1 -1.3 rcurveto\n"
	"	-6.0 4.5 -6.0 -2.1 -2.4 -0.7 rcurveto fill\n"
	"} bdef\n"

	"\n/turnx {  % usage: x y trnx - turn with line through it\n"
	"	2 copy turn M\n"
	"	0.6 setlinewidth 0 1.5 rmoveto 0 9 rlineto stroke\n"
	"} bdef\n"

	"\n/lphr {  % usage: x y lphr - longphrase\n"
	"	1.2 setlinewidth M\n"
	"	0 -18 rlineto stroke\n"
	"} bdef\n"

	"\n/mphr {  % usage: x y mphr - mediumphrase\n"
	"	1.2 setlinewidth M\n"
	"	0 -12 rlineto stroke\n"
	"} bdef\n"

	"\n/sphr {  % usage: x y sphr - shortphrase\n"
	"	1.2 setlinewidth M\n"
	"	0 -6 rlineto stroke\n"
	"} bdef\n"

	"/ltr {	% usage: len xleft y ltr - long trill\n"
	"	gsave 4 add T\n"
	"	0 6 3 -1 roll {\n"
	"		% first loop draws left half of squiggle; second draws right\n"
	"		0 1 1 {\n"
	"			0.0 0.4 M\n"
	"			2.0 1.9 3.4 2.3 3.9 0.0 curveto\n"
	"			2.1 0.0 lineto\n"
	"			1.9 0.8 1.4 0.7 0.0 -0.4 curveto\n"
	"			fill\n"
	"			pop 180 rotate -6 0 translate\n"
	"		} for\n"
	"		pop 6 0 translate	% shift axes right one squiggle\n"
	"	} for\n"
	"	grestore\n"
	"} bdef\n"

	"/arp {	% usage: len x ylow arp - arpeggio\n"
	"	gsave 90 rotate exch neg ltr grestore\n"
	"} bdef\n"

	"/wedge {	% usage: x y wedge - wedge\n"
	"	1 add M -1.5 5 rlineto 3 0 rlineto -1.5 -5 rlineto fill\n"
	"} bdef\n"

	"/opend {  % usage: x y opend - 'open' sign\n"
	"	dlw M currentpoint 3 add 2.5 -90 270 arc stroke\n"
	"} bdef\n"

	"/snap {  % usage: x y snap - 'snap' sign\n"
	"	dlw M currentpoint -3 6 rmoveto\n"
	"	0 5 6 5 6 0 rcurveto\n"
	"	0 -5 -6 -5 -6 0 rcurveto\n"
	"	5 add M 0 -6 rlineto stroke\n"
	"} bdef\n"

	"/thumb {  % usage: x y thumb - 'thumb' sign\n"
	"	dlw M currentpoint -2.5 7 rmoveto\n"
	"	0 6 5 6 5 0 rcurveto\n"
	"	0 -6 -5 -6 -5 0 rcurveto\n"
	"	2 add M 0 -4 rlineto stroke\n"
	"} bdef\n"

	"\n/hl {  % usage: y hl  - helper line at height y\n"
	"	0.8 setlinewidth x -6.5 add exch M\n"
	"	13 0 rlineto stroke\n"
	"} bdef\n"

	"\n/hl1 {  % usage: y hl1  - longer helper line\n"
	"	0.8 setlinewidth x -8 add exch M\n"
	"	16 0 rlineto stroke\n"
	"} bdef\n"

	"\n% accidentals\n"
	"/sh0 {  % usage:  x y sh0  - sharp sign\n"
	"	gsave T 0.9 setlinewidth\n"
	"	-1.2 -8.4 M 0 15.4 rlineto\n"
	"	1.4 -7.2 M 0 15.4 rlineto stroke\n"
	"	-2.6 -3 M 5.4 1.6 rlineto 0 -2.2 rlineto -5.4 -1.6 rlineto 0 2.2 rlineto fill\n"
	"	-2.6 3.4 M 5.4 1.6 rlineto 0 -2.2 rlineto -5.4 -1.6 rlineto 0 2.2 rlineto fill\n"
	"	grestore\n"
	"} bdef\n"
	"/sh { % usage: dx sh  - sharp relative to head\n"
	"	x add y sh0 } bdef\n"
	"\n"
	"/ft0 { % usage:  x y ft0  - flat sign\n"
	"	gsave T 0.8 setlinewidth\n"
	"	-1.8 2.5 M\n"
	"	6.4 3.3 6.5 -3.6 0 -6.6 rcurveto\n"
	"	4.6 3.9 4.5 7.6 0 5.7 rcurveto\n"
	"	currentpoint fill M\n"
	"	0 7.1 rmoveto 0 -12.6 rlineto stroke\n"
	"	grestore\n"
	"} bdef\n"
	"/ft { % usage: dx ft  - flat relative to head\n"
	"	x add y ft0 } bdef\n"
	"\n"
	"/nt0 { % usage:  x y nt0  - natural sign\n"
	"	gsave T 0.5 setlinewidth\n"
	"	-2 -4.325 M 0 12.15 rlineto\n"
	"	1.3 -7.85 M 0 12.15 rlineto stroke\n"
	"	2.1 setlinewidth\n"
	"	-2 -2.9 M 3.3 0.63 rlineto\n"
	"	-2 2.35 M 3.3 0.63 rlineto stroke\n"
	"	grestore\n"
	"} bdef\n"
	"/nt { % usage: dx nt  - natural relative to head\n"
	"	x add y nt0 } bdef\n"

	"\n/ftx { % usage: x y ftx - narrow flat sign\n"
	"	M -1.4 2.7 rmoveto\n"
	"	5.7 3.1 5.7 -3.6 0.0 -6.7 rcurveto\n"
	"	3.9 4.0 4.0 7.6 0.0 5.8 rcurveto\n"
	"	currentpoint fill M\n"
	"	dlw 0 7.1 rmoveto 0 -12.4 rlineto stroke\n"
	" } bdef\n"
	"/dft0 { % usage: x y dft0 ft - double flat sign\n"
	"	2 copy exch 2.5 sub exch ftx exch 1.5 add exch ftx } bdef\n"
	"/dft { % usage: dx dft - double flat relative to head\n"
	"	x add y dft0 } bdef\n"

	"\n% accidentals in guitar chord\n"
	"/tempstr 1 string def\n"
	"/sharp_glyph {\n"
	"	fh 0.4 mul 0 rmoveto currentpoint\n"
	"	gsave T fh 0.08 mul dup scale 0 7 sh0 grestore\n"
	"	fh 0.4 mul 0 rmoveto\n"
	"} bdef\n"
	"/flat_glyph {\n"
	"	fh 0.4 mul 0 rmoveto currentpoint\n"
	"	gsave T fh 0.08 mul dup scale 0 5 ft0 grestore\n"
	"	fh 0.4 mul 0 rmoveto\n"
	"} bdef\n"
	"/nat_glyph {\n"
	"	fh 0.4 mul 0 rmoveto currentpoint\n"
	"	gsave T fh 0.08 mul dup scale 0 7 nt0 grestore\n"
	"	fh 0.4 mul 0 rmoveto\n"
	"} bdef\n"
	"/gcshow {	% usage: (str) gcshow - guitar chord\n"
	"	{ dup 129 eq\n"
	"	  {pop sharp_glyph}\n"
	"	  { dup 130 eq\n"
	"	    {pop flat_glyph}\n"
	"	    { dup 131 eq\n"
	"	      {pop nat_glyph}\n"
	"	      {tempstr exch 0 exch put tempstr show}\n"
	"	      ifelse\n"
	"	    }\n"
	"	    ifelse\n"
	"	  }\n"
	"	  ifelse\n"
	"	}\n"
	"	forall\n"
	"} bdef\n"

	"\n/bar {  % usage: h x y bar  - single bar\n"
	"	M dlw 0 exch rlineto stroke\n"
	"} bdef\n"

	"\n/dbar {  % usage: h x y dbar  - thin double bar\n"
	"	M dlw dup 0 exch rlineto dup -3 exch neg rmoveto\n"
	"	0 exch rlineto stroke\n"
	"} bdef\n"

	"\n/fbar1 {  % usage: h x y fbar1  - fat double bar at start\n"
	"	M dlw dup 0 exch rlineto 3 0 rlineto dup 0 exch neg rlineto\n"
	"	currentpoint fill M\n"
	"	3 0 rmoveto 0 exch rlineto stroke\n"
	"} bdef\n"

	"\n/fbar2 {  % usage: h x y fbar2  - fat double bar at end\n"
	"	M dlw dup 0 exch rlineto -3 0 rlineto dup 0 exch neg rlineto\n"
	"	currentpoint fill M\n"
	"	-3 0 rmoveto 0 exch rlineto stroke\n"
	"} bdef\n"

	"\n/rdots {  % usage: x y rdots  - repeat dots\n"
	"	9 add M currentpoint 2 copy 1.2 0 360 arc\n"
	"	6 add M currentpoint 1.2 0 360 arc fill\n"
	"} bdef\n"

	"\n/csig {  % usage:  x y csig - C timesig\n"
	" M\n"
	" 1.0 17.3 rmoveto\n"
	" 0.9 -0.0 2.3 -0.7 2.4 -2.2 rcurveto\n"
	" -1.2 2.0 -3.6 -0.1 -1.6 -1.7 rcurveto\n"
	" 2.0 -1.0 3.8 3.5 -0.8 4.7 rcurveto\n"
	" -2.0 0.4 -6.4 -1.3 -5.8 -7.0 rcurveto\n"
	" 0.4 -6.4 7.9 -6.8 9.1 -0.7 rcurveto\n"
	" -2.3 -5.6 -6.7 -5.1 -6.8 0.0 rcurveto\n"
	" -0.5 4.4 0.7 7.5 3.5 6.9 rcurveto\n"
	" fill\n"
	" } bdef\n"

	"\n/ctsig {  % usage:  x y ctsig - C| timesig\n"
	"	dlw 2 copy csig 4 add M 0 16 rlineto stroke\n"
	"} bdef\n"

	"\n/tsig { % usage: (top) (bot) x y tsig - time signature\n"
	"	M gsave /Times-Bold 16 selectfont 1.2 1 scale\n"
	"	0 1 rmoveto currentpoint 3 -1 roll cshow\n"
	"	M 0 12 rmoveto cshow grestore\n"
	"} bdef\n"

	"\n/stsig { % usage: (meter) x y stsig - single time signature\n"
	"	M gsave /Times-Bold 18 selectfont 1.2 1 scale\n"
	"	0 6 rmoveto cshow grestore\n"
	"} bdef\n"

	"\n/staff { % usage: l staff  - draw staff\n"
	"	dlw dup 0 rlineto dup neg 6 rmoveto\n"
	"	dup 0 rlineto dup neg 6 rmoveto\n"
	"	dup 0 rlineto dup neg 6 rmoveto\n"
	"	dup 0 rlineto dup neg 6 rmoveto\n"
	"	0 rlineto stroke\n"
	"} bdef\n"

	"\n/sep0 { % usage: x1 x2 sep0  - hline separator\n"
	"	dlw 0 M 0 lineto stroke\n"
	"} bdef\n"

	"\n/hbrce {\n"
	"	-2.5 1.0 rmoveto\n"
	"	-4.5 -4.6 -7.5 -12.2 -4.4 -26.8 rcurveto\n"
	"	3.5 -14.3 3.2 -21.7 -2.1 -24.2 rcurveto\n"
	"	7.4 2.4 7.3 14.2 3.5 29.5 rcurveto\n"
	"	-2.7 9.5 -1.5 16.2 3.0 21.5 rcurveto\n"
	"	fill\n"
	"} bdef\n"
	"/brace {	% usage: h x y brace\n"
	"	gsave T 0 0 M 0.01 mul 1.0 exch scale hbrce\n"
	"	0 -100 M 1 -1 scale hbrce grestore\n"
	"} bdef\n"

	"\n/bracket {	% usage: h x y braket\n"
	"	M dlw -4 0 rlineto currentpoint stroke\n"
	"	3 setlinewidth M 0 exch neg rlineto currentpoint stroke\n"
	"	dlw M 4 0 rlineto stroke\n"
	"} bdef\n"

	"\n/mrest {	% usage: nb_measures x y mrest\n"
	"	gsave T 1 setlinewidth\n"
	"	0 -6 M 0 12 rlineto 40 -6 M 0 12 rlineto stroke\n"
	"	5 setlinewidth 0 0 M 40 0 rlineto stroke\n"
	"	/Times-Bold 15 selectfont 20 16 M cshow grestore\n"
	"} bdef\n"

	"\n/mrep {	% usage: x y mrep - measure repeat\n"
	"	2 copy 2 copy\n"
	"	M 2 10 rmoveto currentpoint 1.4 0 360 arc\n"
	"	M 11 2 rmoveto currentpoint 1.4 0 360 arc\n"
	"	M 10 12 rlineto 3 0 rlineto -10 -12 rlineto -3 0 rlineto fill\n"
	"} bdef\n"

	"\n/end1 {  % usage: (str) dx x y end1  - mark first ending\n"
	"	gsave dlw T 0 -20 M\n"
	"	0 20 rlineto 0 rlineto 0 -20 rlineto stroke\n"
	"	4 -10 M /Times-Roman 13 selectfont 1.2 0.95 scale\n"
	"	show grestore\n"
	"} bdef\n"

	"\n/end2 {  % usage: (str) dx x y end2  - mark second ending\n"
	"	gsave dlw T 0 -20 M\n"
	"	0 20 rlineto 0 rlineto stroke\n"
	"	4 -10 M /Times-Roman 13 selectfont 1.2 0.95 scale\n"
	"	show grestore\n"
	"} bdef\n"

	"\n/SL {  % usage: pp2x pp1x p1 pp1 pp2 p2 p1 SL\n"
	"	M curveto rlineto curveto fill\n"
	"} bdef\n"

	"\n%-- text --\n"
	"/dsp { dup stringwidth pop } bdef\n"
	"/glue { 2 copy length exch length add string dup 4 2 roll 2 index 0 3 index \n"
	"	putinterval exch length exch putinterval } bdef\n"
	"/TXT { /txt exch def } bdef\n"
	"/rejoin { ( ) search pop exch glue } bdef\n"
	"/measure { dsp txt stringwidth pop add textwidth 2 add gt } bdef\n"
	"/join { txt exch glue TXT } bdef\n"
	"/find { search { pop 3 -1 roll 1 add 3 1 roll } { pop exit } ifelse } bdef\n"
	"/spacecount { 0 exch ( ) { find } loop } bdef\n"
	"/jproc { dsp textwidth exch sub exch dup spacecount } bdef\n"
	"/popzero { dup 0 eq { pop }{ div } ifelse } bdef\n"
	"/justify { jproc 1 sub 3 2 roll exch popzero 0 32 4 3 roll widthshow } def\n"

	"\n/P1 {	% usage: str lwidth P1\n"
	"	/textwidth exch def () TXT\n"
	"	dup spacecount {\n"
	"		rejoin measure { gsave txt show grestore LF () TXT join } { join } ifelse\n"
	"	} repeat gsave txt show grestore LF () TXT pop\n"
	"} def\n"

	"\n/P2 {	% usage: str lwidth P2\n"
	"	/textwidth exch def () TXT\n"
	"	dup spacecount {\n"
	"		rejoin measure { gsave txt justify grestore LF () TXT join } { join } ifelse\n"
	"	} repeat gsave txt show grestore LF () TXT pop\n"
	"} def\n"

	"\n/hd {	% usage: x y hd  - full head\n"
	"	2 copy /y exch def /x exch def M\n"
	"	3.5 2.0 rmoveto\n"
	"	-2.0 3.5 -9.0 -0.5 -7.0 -4.0 rcurveto\n"
	"	2.0 -3.5 9.0 0.5 7.0 4.0 rcurveto fill\n"
	"} bdef\n"

	"\n/Hd {	% usage: x y Hd  - open head for half\n"
	"	2 copy /y exch def /x exch def M\n"
	"	3.51 1.92 rmoveto\n"
	"	-2.04 3.73 -9.06 -0.10 -7.02 -3.83 rcurveto\n"
	"	2.04 -3.73 9.06 0.10 7.02 3.83 rcurveto\n"
	"	-0.44 -0.24 rmoveto\n"
	"	0.96 -1.76 -5.19 -5.11 -6.15 -3.35 rcurveto\n"
	"	-0.96 1.76 5.19 5.11 6.15 3.35 rcurveto\n"
	"	fill\n} bdef\n"

	"\n/HD {	% usage: x y HD  - open head for whole\n"
	"	2 copy /y exch def /x exch def M\n"
	"	5.96 0.00 rmoveto\n"
	"	0.00 1.08 -2.71 3.52 -5.96 3.52 rcurveto\n"
	"	-3.25 0.00 -5.96 -2.44 -5.96 -3.52 rcurveto\n"
	"	0.00 -1.08 2.71 -3.52 5.96 -3.52 rcurveto\n"
	"	3.25 0.00 5.96 2.44 5.96 3.52 rcurveto\n"
	"	-8.13 1.62 rmoveto\n"
	"	1.62 2.17 5.96 -1.07 4.34 -3.24 rcurveto\n"
	"	-1.62 -2.17 -5.96 1.07 -4.34 3.24 rcurveto\n"
	"	fill\n} bdef\n"

	"\n/HDD {	% usage: x y HDD - round breve\n"
	"	dlw HD\n"
	"	x y M -6 -4 rmoveto 0 8 rlineto\n"
	"	x y M 6 -4 rmoveto 0 8 rlineto stroke\n"
	"} bdef\n"

	"\n/breve {	% usage: x y breve - square breve\n"
	"	2 copy /y exch def /x exch def M\n"
	"	3 setlinewidth -6 -2.5 rmoveto 12 0 rlineto\n"
	"	0 5 rmoveto -12 0 rlineto stroke\n"
	"	dlw x y M -6 -5 rmoveto 0 10 rlineto\n"
	"	x y M 6 -5 rmoveto 0 10 rlineto stroke\n"
	"} bdef\n"

	"\n/longa {	% usage: x y longa\n"
	"	2 copy /y exch def /x exch def M\n"
	"	3 setlinewidth -6 -2.5 rmoveto 12 0 rlineto\n"
	"	0 5 rmoveto -12 0 rlineto stroke\n"
	"	dlw x y M -6 -5 rmoveto 0 10 rlineto\n"
	"	x y M 6 -10 rmoveto 0 15 rlineto stroke\n"
	"} bdef\n";

/* -- define which latin encoding -- */
void define_encoding(FILE *fp,
		     int enc)	/* Latin encoding number */
{
	if (enc <= 0)
		return;
	fprintf(fp, "\n"
		"/ISOLatin%dEncoding [\n"
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
		"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n",
		enc);
	switch (enc) {
	case 1:
		fprintf(fp,
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
		fprintf(fp,
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
		fprintf(fp,
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
		fprintf(fp,
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
		fprintf(fp,
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
		fprintf(fp,
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
		 int num,
		 int enc)
{
	if (!strcmp(name, "Symbol")) {
		fprintf(fp, "\n/F%d { dup 0.8 mul /fh exch def\n"
			"/%s exch selectfont } bdef\n",
			num, name);
		return;
	}
	if (enc == 0)
		enc = 1;
	fprintf(fp, "\n/%s findfont\n"
		"dup length dict begin\n"
		"	{1 index /FID ne {def} {pop pop} ifelse} forall\n"
		"	/Encoding ISOLatin%dEncoding def\n"
		"	currentdict\n"
		"end\n"
		"/%s-ISO exch definefont pop\n"
		"/F%d { dup 0.8 mul /fh exch def\n"
		"/%s-ISO exch selectfont } bdef\n",
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
		fprintf(fp, " %.2f %.2f rlineto\n",
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
		fprintf(fp, " %.2f %.2f rmoveto\n",
			f1 * p[i0][0], f2 * p[i0][1]);
	else	fprintf(fp, " %.2f %.2f rmoveto\n",
			f1 * (p[i0][0] - p[i0 - 1][0]),
			f2 * (p[i0][1] - p[i0 - 1][1]));
}

/* -- def_stems -- */
static void def_stems(FILE *fp)
{
	fprintf(fp, "\n/su {  %% usage: len su	- up stem\n"
		"	dlw x y M %.1f %.1f rmoveto %.1f sub 0 exch rlineto stroke\n"
		"} bdef\n",
		STEM_XOFF, STEM_YOFF, STEM_YOFF );

	fprintf(fp, "\n/sd {  %% usage: len sd  - down stem\n"
		"	dlw x y M %.1f %.1f rmoveto neg %.1f add 0 exch rlineto stroke\n"
		"} bdef\n",
		-STEM_XOFF, -STEM_YOFF, STEM_YOFF);
}

/* -- def_deco1 -- */
static void def_deco1(FILE *fp)
{
static float q[8][2] = {    /* for down bow sign */
	{-4,0},{-4,9},{4,9},{4,0},
	{-4,6},{-4,9},{4,9},{4,6} };

	float f1, f2;

	fprintf(fp, "\n/hld {  %% usage: x y hld  - fermata\n"
		"  1.5 add 2 copy 1.5 add M currentpoint 1.3 0 360 arc\n"
		"  M -7.5 0 rmoveto\n"
		"  0 11.5 15 11.5 15 0 rcurveto\n"
		"  -0.25 0 rlineto\n"
		"  -1.25 9 -13.25 9 -14.50 0 rcurveto\n"
		"  fill\n} bdef\n");

	f1=f2=0.8;
	fprintf(fp, "\n/dnb {  %% usage: x y dnb - down bow\n"
		"	dlw M\n");
	add_mv(fp,f1,f2,q,0);
	add_sg(fp,f1,f2,q,1,3);
	fprintf(fp, "	currentpoint stroke M\n");
	add_mv(fp,f1,f2,q,4);
	add_sg(fp,f1,f2,q,5,3);
	fprintf(fp, " fill\n} bdef\n"

		"\n/upb {  %% usage: x y upb - up bow\n"
		"  dlw M -2.56 8.80 rmoveto\n"
		"  2.56 -8.80 rlineto\n"
		"  2.56 8.80 rlineto\n"
		"  stroke\n} bdef\n");
}

/* -- def_flags1 -- */
static void def_flags1(FILE *fp)
{
	fprintf(fp, "\n/f1u { %% usage: len f1u - single flag up\n"
		"	y add x %.1f add exch M\n"
		"	1.0 -2.7 3.0 -5.0 5.3 -7.8 rcurveto\n"
		"	2.1 -2.3 1.3 -10.2 0.4 -10.5 rcurveto\n"
		"	1.4 7.0 -1.4 12.3 -5.7 13.5 rcurveto\n"
		"	fill\n} bdef\n",
		STEM_XOFF);

	fprintf(fp, "\n/f1d { %% usage: len f1d - single flag down\n"
		"	neg y add x %.1f sub exch M\n"
		"	1.0 2.7 3.1 5.0 5.6 8.0 rcurveto\n"
		"	1.9 2.3 1.0 8.8 0.2 8.3 rcurveto\n"
		"	1.9 -4.6 -1.6 -10.5 -5.8 -11.5 rcurveto\n"
		"	fill\n} bdef\n",
		STEM_XOFF);

	fprintf(fp, "\n/f1s { %% usage: len f1s - straight single flag down\n"
		"	-%.1f %.1f %.1f 4 -1 roll neg y add x %.1f sub exch bm\n"
		"} bdef\n",
		BEAM_DEPTH, 2*STEM_XOFF, BEAM_DEPTH, STEM_XOFF);
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

	f1 = f2 = 6.0 / 9.0;			/* up flags */
	fprintf(fp, "\n/f2u { %% usage: len f2u - double flag up\n"
		"	y add x %.1f add exch M\n", STEM_XOFF);
	add_mv(fp,f1,f2,p,0);
	add_cv(fp,f1,f2,p,1,4);
	fprintf(fp, "  fill\n} bdef\n");

	f1 *= 1.2;			/* down flags */
	f2 = -f2;
	fprintf(fp, "\n/f2d { %% usage: len f2d - double flag down\n"
		"	neg y add x %.1f sub exch M\n", STEM_XOFF);
	add_mv(fp,f1,f2,p,0);
	add_cv(fp,f1,f2,p,1,4);
	fprintf(fp, "  fill\n} bdef\n");

	fprintf(fp, "\n/f2s { %% usage: len f2s - straight double flag down\n"
		"	-%.1f %.1f %.1f 4 -1 roll neg y add x %.1f sub exch 5 copy bm\n"
		"	%.1f add bm\n"
		"} bdef\n",
		BEAM_DEPTH, 2*STEM_XOFF, BEAM_DEPTH, STEM_XOFF, BEAM_SHIFT);

	fprintf(fp, "\n/f3s { %% usage: len f3s - straight triple flag down\n"
		"	-%.1f %.1f %.1f 4 -1 roll neg y add x %.1f sub exch 5 copy bm\n"
		"	%.1f add 5 copy bm\n"
		"	%.1f add bm\n"
		"} bdef\n",
		BEAM_DEPTH, 2*STEM_XOFF, BEAM_DEPTH, STEM_XOFF, BEAM_SHIFT, BEAM_SHIFT);
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

	f1 = f2 = 6.0 / 9.0;			/* extra up flag */
	fprintf(fp, "\n/xfu {  %% usage:  len xfu  - extra flag up\n"
		"	y add x %.1f add exch M\n", STEM_XOFF);
	add_mv(fp,f1,f2,p,0);
	add_cv(fp,f1,f2,p,1,2);
	fprintf(fp, "   fill\n} bdef\n");

	f1 *= 1.2;			/* extra down flag */
	f2 = -f2;
	fprintf(fp, "\n/xfd {  %% usage:  len xfd  - extra flag down\n"
		"	neg y add x %.1f sub exch M\n", STEM_XOFF);
	add_mv(fp,f1,f2,p,0);
	add_cv(fp,f1,f2,p,1,2);
	fprintf(fp, "   fill\n} bdef\n"

		"\n/f3d {dup f2d 9.5 sub xfd} bdef\n"

		"\n/f4d {dup dup f2d 9.5 sub xfd 14.7 sub xfd} bdef\n"

		"\n/f3u {dup f2u 9.5 sub xfu} bdef\n"

		"\n/f4u {dup dup f2u 9.5 sub xfu 14.7 sub xfu} bdef\n");
}

/* -- def_acc1 -- */
static void def_acc1(FILE *fp)
{
static float s[25][2]={
	{0.7,0},{3.9,3},{6,3},{6.2,6.2},{3,6},{3,3.9},
	{0,0.7},{-3,3.9},{-3,6},{-6.2,6.2},{-6,3},{-3.9,3},
	{-0.7,0},{-3.9,-3},{-6,-3},{-6.2,-6.2},{-3,-6},{-3,-3.9},
	{0,-0.7},{3,-3.9},{3,-6},{6.2,-6.2},{6,-3},{3.9,-3},
	{0.7,0} };

	float f1,f2;

	f1=5.0/9.0;
	f2=f1;
	fprintf(fp, "\n/dsh0 { %% usage: x y dsh0 - double sharp\n"
		"	M\n");
	add_mv(fp,f1,f2,s,0);
	add_sg(fp,f1,f2,s,1,24);
	fprintf(fp, "  fill\n } bdef\n"
		"/dsh { %% usage: dx dsh  - double sharp relative to head\n"
		" x add y dsh0 } bdef\n");
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

	fprintf(fp, "\n/r4 { %% usage: x y r4 - quarter rest\n"
		"	dup /y exch def exch dup /x exch def exch M\n");
	f1=f2=6.0/11.5;
	add_mv(fp,f1,f2,p,0);
	add_cv(fp,f1,f2,p,1,4);
	fprintf(fp, "  fill\n } bdef\n");

	fprintf(fp, "\n/r8 { %% usage: x y r8 - eighth rest\n"
		"	dup /y exch def exch dup /x exch def exch M\n");
	f1=f2=7/18.0;
	add_mv(fp,f1,f2,q,0);
	add_cv(fp,f1,f2,q,1,4);
	add_sg(fp,f1,f2,q,13,3);
	fprintf(fp, "  fill\n } bdef\n");

	for (i = 13; i < 26; i++) {
		r[i][0] -= 4.2;
		r[i][1] -= 14;
	}
	fprintf(fp, "\n/r16 { %% usage: x y r16 - 16th rest\n"
		"	dup /y exch def exch dup /x exch def exch M\n");
	f1=f2=7/18.0;
	add_mv(fp,f1,f2,r,0);
	add_cv(fp,f1,f2,r,1,4);
	add_sg(fp,f1,f2,r,13,1);
	add_cv(fp,f1,f2,r,14,4);
	add_sg(fp,f1,f2,r,26,3);
	fprintf(fp, "  fill\n } bdef\n");

	fprintf(fp,
		"\n/r00 {  %% usage: x y r00 - longa rest\n"
		"	6 add 2 copy /y exch def /x exch def M\n"
		"	-1 0 rmoveto 0 -12 rlineto 3 0 rlineto 0 12 rlineto fill\n"
		"} bdef\n"
		"\n/r0 {  %% usage: x y r0 - breve rest\n"
		"	6 add 2 copy /y exch def /x exch def M\n"
		"	-1 0 rmoveto 0 -6 rlineto 3 0 rlineto 0 6 rlineto fill\n"
		"} bdef\n"
		"\n/r1 {  %% usage: x y r1 - rest\n"
		"	6 add 2 copy /y exch def /x exch def M\n"
		"	-3 0 rmoveto 0 -3 rlineto 6 0 rlineto 0 3 rlineto fill\n"
		"} bdef\n"
		"\n/r2 {  %% usage: x y r2 - half rest\n"
		"	2 copy /y exch def /x exch def M\n"
		"	-3 0 rmoveto 0 3 rlineto 6 0 rlineto 0 -3 rlineto fill\n"
		"} bdef\n");

	/* get 32nd, 64th rest by overwriting 8th and 16th rests */
	fprintf(fp,
		"\n/r32 {\n"
		"2 copy r16 5.5 add exch 1.6 add exch r8\n"
		"} bdef\n"

		"\n/r64 {\n"
		"2 copy 5.5 add exch 1.6 add exch r16\n"
		"5.5 sub exch 1.5 sub exch r16\n"
		"} bdef\n");

	for (i = 13; i < 26; i++) {
		r[i][0] += 4.2;
		r[i][1] += 14;
	}
}

/* -- def_gnote -- */
static void def_gnote(FILE *fp)
{
	fprintf(fp, "\n/gn {	%% usage: l x y gn - grace note\n"
		"	dlw 2 copy M\n"
		"	-1.3 1.5 rmoveto\n"
		"	2.4 2 5 -1 2.6 -3 rcurveto\n"
		"	-2.4 -2 -5 1 -2.6 3 rcurveto fill\n"
		"	M %.1f 0 rmoveto 0 exch rlineto\n",
		GSTEM_XOFF);

	fprintf(fp, "} bdef\n"

		"\n/gn1 {  %% usage: l x y gn1 - grace note w. tail\n"
		"	gn\n"
		"	3 -4 4 -5 2 -8 rcurveto stroke\n"
		"} bdef\n"

		"\n/gn1a {  %% usage: l x y gn1s - short appoggiatura\n"
		"	gn\n"
		"	3 -4 4 -5 2 -8 rcurveto -5 2 rmoveto 7 4 rlineto\n"
		"	stroke\n"
		"} bdef\n"

		"\n/gn1h {  %% usage: l x y gn1h - grace note for bagpipe\n"
		"	gn\n"
		"	1 -3 4 -3 3 -5 rcurveto -3 2 rmoveto\n"
		"	1 -3 4 -3 3 -5 rcurveto -3 2 rmoveto\n"
		"	1 -3 4 -3 3 -5 rcurveto stroke\n"
		"} bdef\n"

		"\n/gn1s {  %% usage: l x y gn1s - grace note straight flags\n"
		"	gn\n"
		"	4 -2 rlineto -4 -1 rmoveto\n"
		"	4 -2 rlineto -4 -1 rmoveto\n"
		"	4 -2 rlineto stroke\n"
		"} bdef\n"

		"\n/gnt {  %% usage: l x y gnt - grace note\n"
		"	gn stroke\n"
		"} bdef\n"

		"\n/gbm2 {  %% usage: x1 y1 x2 y2 gbm2 - double gnote beam\n"
		"	1.4 setlinewidth\n"
		"	4 copy 0.5 sub M 0.5 sub lineto stroke\n"
		"	3.4 sub M 3.4 sub lineto stroke\n"
		"} bdef\n"

		"\n/gbm3 {  %% usage: x1 y1 x2 y2 gbm3  - triple gnote beam\n"
		"	1.2 setlinewidth\n"
		"	4 copy 0.3 sub M 0.3 sub lineto\n"
		"	4 copy 2.5 sub M 2.5 sub lineto\n"
		"	4.7 sub M 4.7 sub lineto stroke\n"
		"} bdef\n"

		"\n/ghl {  %% usage: x y ghl  - grace note helper line\n"
		"	0.6 setlinewidth M\n"
		"	-3 0 rmoveto 6 0 rlineto stroke\n"
		"} bdef\n"

		"\n/gsl {  %% usage: x1 y2 x2 y2 x3 y3 x0 y0 gsl\n"
		"	dlw M curveto stroke\n"
		"} bdef\n"

		"\n%% grace note accidentals\n"
		"/gsc { gsave T 0.7 dup scale 0 0 } bdef\n"
		"/gsh0 {  %% usage: x y gsh0\n"
		"	gsc sh0 grestore\n"
		"} bdef\n"
		"/gnt0 {  %% usage: x y gnt0\n"
		"	gsc nt0 grestore\n"
		"} bdef\n"
		"/gft0 {  %% usage: x y gft0\n"
		"	gsc ft0 grestore\n"
		"} bdef\n"
		"/gds0 {  %% usage: x y gds0\n"
		"	gsc dsh0 grestore\n"
		"} bdef\n"
		"/gdf0 {  %% usage: x y gdf0\n"
		"	gsc dft0 grestore\n"
		"} bdef\n");
}

/* -- define_symbols: write postscript macros to file -- */
void define_symbols(FILE *fp)
{
	fputs(ps_head, fp);
	def_stems(fp);
	def_deco1(fp);
	def_flags1(fp);
	def_flags2(fp);
	def_xflags(fp);
	def_acc1(fp);
	def_rests(fp);
	def_gnote(fp);
}
