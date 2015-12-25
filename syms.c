/*
 * Postscript definitions.
 *
 * This file is part of abcm2ps.
 *
 * Copyright (C) 1998-2003 Jean-François Moine
 * Adapted from abc2ps, Copyright (C) 1996,1997 Michael Methfessel
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <string.h>

#include "abcparse.h"
#include "abc2ps.h"

/*  subroutines to define postscript macros which draw symbols	*/

static char ps_head[] =
	"/xymove{2 copy /y exch def /x exch def M}!\n"

	/* str cshow - center at current pt */
	"/cshow{dup stringwidth pop 2 div neg 0 RM show}!\n"

	/* str lshow - show left-aligned */
	"/lshow{dup stringwidth pop neg 0 RM show}!\n"

	/* str showb - show in box */
	"/showb{	dup currentpoint 3 -1 roll show\n"
	"	0.6 setlinewidth\n"
#if PS_LEVEL >= 2
	"	exch 2 sub exch 3 sub 3 -1 roll\n"
	"	stringwidth pop 4 add fh 4 add rectstroke}!\n"
#else
	"	M -2 -3 RM stringwidth pop 4 add fh 4 add 2 copy\n"
	"	0 exch RL 0 RL neg 0 exch RL neg 0 RL stroke}!\n"
#endif

#if 0
	"/cshowb{ % usage: str cshowb - show centered in box\n"
	"	dup stringwidth pop dup 2 div neg 0 RM currentpoint 4 -1 roll show\n"
	"	0.6 setlinewidth\n"
#if PS_LEVEL >= 2
	"	exch 2 sub exch 3 sub 3 -1 roll\n"
	"	4 add fh 4 add rectstroke}!\n"
#else
	"	M -2 -3 RM 4 add fh 4 add 2 copy\n"
	"	0 exch RL 0 RL neg 0 exch RL neg 0 RL stroke}!\n"
#endif
#endif

	/* l x y wln - write ligne */
	"/wln{M 0.8 setlinewidth 0 RL stroke}!\n"

	"/whf{3 add 3 3 1 roll wln}!\n"

	/* x y tclef - treble clef */
	"/tclef{	M\n"
	"	-1.9 3.7 RM\n"
	"	-3.3 1.9 -3.1 6.8 2.4 8.6 RC\n"
	"	7.0 0.0 9.8 -8.0 4.1 -11.7 RC\n"
	"	-5.2 -2.4 -12.5 0.0 -13.3 6.2 RC\n"
	"	-0.7 6.4 4.15 10.5 10.0 15.3 RC\n"
	"	4.0 4.0 3.6 6.1 2.8 9.6 RC\n"
	"	-2.3 -1.5 -4.7 -4.8 -4.5 -8.5 RC\n"
	"	0.8 -12.2 3.4 -17.3 3.5 -26.3 RC\n"
	"	0.3 -4.4 -1.2 -6.2 -3.8 -6.2 RC\n"
	"	-3.7 -0.1 -5.8 4.3 -2.8 6.1 RC\n"
	"	3.9 1.9 6.1 -4.6 1.4 -4.8 RC\n"
	"	0.7 -1.2 4.6 -0.8 4.2 4.2 RC\n"
	"	-0.2 10.3 -3.0 15.7 -3.5 28.3 RC\n"
	"	0.0 4.1 0.6 7.4 5.0 10.6 RC\n"
	"	2.3 -3.2 2.9 -10.0 1.0 -12.7 RC\n"
	"	-2.4 -4.3 -11.5 -10.3 -11.8 -15.0 RC\n"
	"	0.4 -7.0 6.9 -8.5 11.7 -6.1 RC\n"
	"	3.9 3.0 1.3 8.8 -3.7 8.1 RC\n"
	"	-4.0 -0.2 -4.8 -3.1 -2.7 -5.7 RC\n"
	"	fill}!\n"

	"/stclef{exch 0.85 div exch 0.85 div gsave 0.85 dup scale tclef grestore}!\n"

	/* x y octu - upper '8' */
	"/octu{/Times-Roman 12 selectfont M -1.5 34 RM (8) show}!\n"
	/* x y octl - lower '8' */
	"/octl{/Times-Roman 12 selectfont M -3.5 -19 RM (8) show}!\n"

	/* x y bclef - bass clef */
	"/bclef{	M\n"
	"	-8.8 3.5 RM\n"
	"	6.3 1.9 10.2 5.6 10.5 10.8 RC\n"
	"	0.3 4.9 -0.5 8.1 -2.6 8.8 RC\n"
	"	-2.5 1.2 -5.8 -0.7 -5.9 -4.1 RC\n"
	"	1.8 3.1 6.1 -0.6 3.1 -3.0 RC\n"
	"	-3.0 -1.4 -5.7 2.3 -1.9 7.0 RC\n"
	"	2.6 2.3 11.4 0.6 10.1 -8.0 RC\n"
	"	-0.1 -4.6 -5.0 -10.2 -13.3 -11.5 RC\n"
	"	15.5 17.0 RM\n"
	"	0.0 1.5 2.0 1.5 2.0 0.0 RC\n"
	"	0.0 -1.5 -2.0 -1.5 -2.0 0.0 RC\n"
	"	0.0 -5.5 RM\n"
	"	0.0 1.5 2.0 1.5 2.0 0.0 RC\n"
	"	0.0 -1.5 -2.0 -1.5 -2.0 0.0 RC\n"
	"	fill}!\n"

	"/sbclef{exch 0.85 div exch 0.85 div gsave 0.85 dup scale 0 3 T bclef grestore}!\n"

	"/cchalf{0 0 M 0.0 12.0 RM\n"
	"	2.6 5.0 RL\n"
	"	2.3 -5.8 5.2 -2.4 4.7 1.6 RC\n"
	"	0.4 3.9 -3.0 6.7 -5.1 4.0 RC\n"
	"	4.1 0.5 0.9 -5.3 -0.9 -1.4 RC\n"
	"	-0.5 3.4 6.5 4.3 7.8 -0.8 RC\n"
	"	1.9 -5.6 -4.1 -9.8 -6.0 -5.4 RC\n"
	"	-1.6 -3.0 RL\n"
	"	fill}!\n"

	/* x y cclef */
	"/cclef{	gsave T\n"
	"	cchalf 0 24 T 1 -1 scale cchalf\n"
	"	-5.0 0 M 0 24 RL 3 0 RL 0 -24 RL fill\n"
	"	-0.5 0 M 0 24 RL 0.8 setlinewidth stroke grestore}!\n"

	"/scclef{exch 0.85 div exch 0.85 div gsave 0.85 dup scale\n"
	"	0 2 T cclef grestore}!\n"

	/* x y pclef */
	"/pclef{	M 1.4 setlinewidth -2.7 2 RM\n"
	"	0 20 RL 5.4 0 RL 0 -20 RL -5.4 0 RL stroke}!\n"
	"/spclef{pclef}!\n"

	/* t dx dy x y bm - beam, depth t */
	"/bm{	M 3 copy RL neg 0 exch RL\n"
	"	neg exch neg exch RL 0 exch RL fill}!\n"

	/* str x y bnum - number on beam */
	"/bnum{M /Times-Italic 12 selectfont cshow}!\n"

	/* x1 y1 x2 y2 hbr - half bracket */
	"/hbr{M dlw lineto 0 -3 RL stroke}!\n"

	/* x y r00 - longa rest */
	"/r00{	xymove\n"
	"	-1 6 RM 0 -12 RL 3 0 RL 0 12 RL fill}!\n"

	/* x y r0 - breve rest */
	"/r0{	xymove\n"
	"	-1 6 RM 0 -6 RL 3 0 RL 0 6 RL fill}!\n"

	/* x y r1 - rest */
	"/r1{	xymove\n"
	"	-3 6 RM 0 -3 RL 7 0 RL 0 3 RL fill}!\n"

	/* x y r2 - half rest */
	"/r2{	xymove\n"
	"	-3 0 RM 0 3 RL 7 0 RL 0 -3 RL fill}!\n"

	/* x y r4 - quarter rest */
	"/r4{	xymove\n"
	"	-0.5 8.9 RM\n"
	"	1.3 -3.4 RL\n"
	"	-2.0 -4.5 RL\n"
	"	3.1 -4.8 RL\n"
	"	-3.2 3.5 -5.8 -1.4 -1.4 -3.8 RC\n"
	"	-1.9 2.0 -0.8 5.0 2.4 2.6 RC\n"
	"	-2.2 4.2 RL\n"
	"	0.0 0.0 2.0 4.7 2.1 4.7 RC\n"
	"	-3.3 5.0 RL\n"
	"	fill}!\n"

	/* 1/8 .. 1/64 rest element */
	"/r8e{	-1.5 -1.5 -2.4 -2.0 -3.6 -2.0 RC\n"
	"	2.4 2.8 -2.8 4.0 -2.8 1.2 RC\n"
	"	0.0 -2.7 4.3 -2.4 5.9 -0.6 RC\n"
	"	fill}!\n"

	/* x y r8 - eighth rest */
	"/r8{	xymove\n"
	"	0.5 setlinewidth 3.3 4.0 RM\n"
	"	-3.4 -9.6 RL stroke\n"
	"	x y M 3.4 4.0 RM r8e}!\n"

	/* x y r16 - 16th rest */
	"/r16{	xymove\n"
	"	0.5 setlinewidth 3.3 4.0 RM\n"
	"	-4.0 -15.6 RL stroke\n"
	"	x y M 3.4 4.0 RM r8e\n"
	"	x y M 1.9 -2.0 RM r8e}!\n"

	/* x y r32 - 32th rest */
	"/r32{	xymove\n"
	"	0.5 setlinewidth 4.8 10.0 RM\n"
	"	-5.5 -21.6 RL stroke\n"
	"	x y M 4.9 10.0 RM r8e\n"
	"	x y M 3.4 4.0 RM r8e\n"
	"	x y M 1.9 -2.0 RM r8e}!\n"

	/* x y r64 - 64th rest */
	"/r64{	xymove\n"
	"	0.5 setlinewidth 4.8 10.0 RM\n"
	"	-7.0 -27.6 RL stroke\n"
	"	x y M 4.9 10.0 RM r8e\n"
	"	x y M 3.4 4.0 RM r8e\n"
	"	x y M 1.9 -2.0 RM r8e\n"
	"	x y M 0.3 -8.0 RM r8e}!\n"

	/* dx dy dt - dot shifted by dx,dy */
	"/dt{y add exch x add exch M currentpoint 1.2 0 360 arc fill}!\n"

	/* x y hld - fermata */
	"/hld{	1.5 add 2 copy 1.5 add M currentpoint 1.3 0 360 arc\n"
	"	M -7.5 0 RM\n"
	"	0 11.5 15 11.5 15 0 RC\n"
	"	-0.25 0 RL\n"
	"	-1.25 9 -13.25 9 -14.50 0 RC\n"
	"	fill}!\n"

	/* x y dnb - down bow */
	"/dnb{	dlw M\n"
	"	-3.2 2.0 RM\n"
	"	0.0 7.2 RL\n"
	"	6.4 0.0 RM\n"
	"	0.0 -7.2 RL\n"
	"	currentpoint stroke M\n"
	"	-6.4 4.8 RM\n"
	"	0.0 2.4 RL\n"
	"	6.4 0.0 RL\n"
	"	0.0 -2.4 RL\n"
	"	fill}!\n"

	/* x y upb - up bow */
	"/upb{	dlw M -2.6 9.4 RM\n"
	"	2.6 -8.8 RL\n"
	"	2.6 8.8 RL\n"
	"	stroke}!\n"

	/* x y grm - gracing mark */
	"/grm{	M -5 2.5 RM\n"
	"	5.0 8.5 5.5 -4.5 10.0 2.0 RC\n"
	"	-5.0 -8.5 -5.5 4.5 -10.0 -2.0 RC fill}!\n"

	/* x y stc - staccato mark */
	"/stc{M currentpoint 1.2 0 360 arc fill}!\n"

	/* x y emb - emphasis bar */
	"/emb{	1.2 setlinewidth 1 setlinecap M\n"
	"	-2.5 0 RM 5 0 RL stroke 0 setlinecap}!\n"

	/* x y cpu - roll sign above head */
	"/cpu{	M -6 0 RM\n"
	"	0.4 7.3 11.3 7.3 11.7 0 RC\n"
	"	-1.3 6 -10.4 6 -11.7 0 RC fill}!\n"

	/* x y sld - slide */
	"/sld{	M -7.2 -4.8 RM\n"
	"	1.8 -0.7 4.5 0.2 7.2 4.8 RC\n"
	"	-2.1 -5.0 -5.4 -6.8 -7.6 -6.0 RC fill}!\n"

	/* x y trl - trill sign */
	"/trl{	/Times-BoldItalic 16 selectfont\n"
	"	M -4 2 RM (tr) show}!\n"

	/* x y umrd - upper mordent */
	"/umrd{	4 add M\n"
	"	2.2 2.2 RL 2.1 -2.9 RL 0.7 0.7 RL\n"
	"	-2.2 -2.2 RL -2.1 2.9 RL -0.7 -0.7 RL\n"
	"	-2.2 -2.2 RL -2.1 2.9 RL -0.7 -0.7 RL\n"
	"	2.2 2.2 RL 2.1 -2.9 RL 0.7 0.7 RL fill}!\n"

	/* x y lmrd - lower mordent */
	"/lmrd{	2 copy umrd 8 add M\n"
	"	0.6 setlinewidth 0 -8 RL stroke}!\n"

	/* str x y fng - finger (0-5) */
	"/fng{/Bookman-Demi 8 selectfont M -3 1 RM show}!\n"

	/* str x y dacs - D.C. / D.S. */
	"/dacs{/Times-Roman 16 selectfont 3 add M cshow}!\n"

	/* x y brth - breath */
	"/brth{/Times-BoldItalic 30 selectfont 6 add M (,) show}!\n"

	/* str x y pf - p, f, pp, .. */
	"/pf{/Times-BoldItalic 16 selectfont 5 add M cshow}!\n"

	/* str x y sfz */
	"/sfz{	exch 4 sub exch 5 add M pop\n"
	"	/Times-Italic 14 selectfont (s) show\n"
	"	/Times-BoldItalic 16 selectfont (f) show\n"
	"	/Times-Italic 14 selectfont (z) show}!\n"

	/* x y coda - coda */
	"/coda{	1 setlinewidth 2 add 2 copy M 0 20 RL\n"
	"	2 copy 10 add exch -10 add exch M 20 0 RL stroke\n"
	"	10 add 6 0 360 arc 1.7 setlinewidth stroke}!\n"

	/* x y sgno - segno */
	"/sgno{	M 0 3 RM currentpoint currentpoint currentpoint\n"
	"	1.5 -1.7 6.4 0.3 3.0 3.7 RC\n"
	"	-10.4 7.8 -8.0 10.6 -6.5 11.9 RC\n"
	"	4.0 1.9 5.9 -1.7 4.2 -2.6 RC\n"
	"	-1.3 -0.7 -2.9 1.3 -0.7 2.0 RC\n"
	"	-1.5 1.7 -6.4 -0.3 -3.0 -3.7 RC\n"
	"	10.4 -7.8 8.0 -10.6 6.5 -11.9 RC\n"
	"	-4.0 -1.9 -5.9 1.7 -4.2 2.6 RC\n"
	"	1.3 0.7 2.9 -1.3 0.7 -2.0 RC\n"
	"	fill\n"
	"	M 0.8 setlinewidth -6.0 1.2 RM 12.6 12.6 RL stroke\n"
	"	7 add exch -6 add exch 1.2 0 360 arc fill\n"
	"	8 add exch 6 add exch 1.2 0 360 arc fill}!\n"

	/* w x y cresc - (de)crescendo */
	"/cresc{	1.2 setlinewidth 6 add M\n"
	"	dup 4 RL neg 4 RL stroke}!\n"

	/* x y dplus - + decoration */
	"/dplus{	1.2 setlinewidth M 0 0.5 RM 0 6 RL\n"
	"	-3 -3 RM 6 0 RL stroke}!\n"

	/* x y accent - accent */
	"/accent{1.2 setlinewidth M -4 2 RM\n"
	"	8 2 RL -8 2 RL stroke}!\n"

	/* x y turn - turn */
	"/turn{	M 5.2 8 RM\n"
	"	1.4 -0.5 0.9 -4.8 -2.2 -2.8 RC\n"
	"	-4.8 3.5 RL\n"
	"	-3.0 2.0 -5.8 -1.8 -3.6 -4.4 RC\n"
	"	1.0 -1.1 2.0 -0.8 2.1 0.1 RC\n"
	"	0.1 0.9 -0.7 1.2 -1.9 0.6 RC\n"
	"	-1.4 0.5 -0.9 4.8 2.2 2.8 RC\n"
	"	4.8 -3.5 RL\n"
	"	3.0 -2.0 5.8 1.8 3.6 4.4 RC\n"
	"	-1.0 1.1 -2 0.8 -2.1 -0.1 RC\n"
	"	-0.1 -0.9 0.7 -1.2 1.9 -0.6 RC\n"
	"	fill}!\n"

	/* x y trnx - turn with line through it */
	"/turnx{	2 copy turn M\n"
	"	0.6 setlinewidth 0 1.5 RM 0 9 RL stroke}!\n"

	/* x y lphr - longphrase */
	"/lphr{1.2 setlinewidth M 0 -18 RL stroke}!\n"

	/* x y mphr - mediumphrase */
	"/mphr{1.2 setlinewidth M 0 -12 RL stroke}!\n"

	/* x y sphr - shortphrase */
	"/sphr{1.2 setlinewidth M 0 -6 RL stroke}!\n"

	/* len x y ltr - long trill */
	"/ltr{	gsave 4 add T\n"
	"	0 6 3 -1 roll{\n"
	/*		% first loop draws left half of squiggle; second draws right\n*/
	"		0 1 1{\n"
	"			0.0 0.4 M\n"
	"			2.0 1.9 3.4 2.3 3.9 0.0 curveto\n"
	"			2.1 0.0 lineto\n"
	"			1.9 0.8 1.4 0.7 0.0 -0.4 curveto\n"
	"			fill\n"
	"			pop 180 rotate -6 0 translate\n"
	"		} for\n"
	/*		% shift axes right one squiggle*/
	"		pop 6 0 translate\n"
	"	} for\n"
	"	grestore}!\n"

	/* len x ylow arp - arpeggio */
	"/arp{gsave 90 rotate exch neg ltr grestore}!\n"

	/* x y wedge - wedge */
	"/wedge{1 add M -1.5 5 RL 3 0 RL -1.5 -5 RL fill}!\n"

	/* x y opend - 'open' sign */
	"/opend{dlw M currentpoint 3 add 2.5 -90 270 arc stroke}!\n"

	/* x y snap - 'snap' sign */
	"/snap{	dlw M currentpoint -3 6 RM\n"
	"	0 5 6 5 6 0 RC\n"
	"	0 -5 -6 -5 -6 0 RC\n"
	"	5 add M 0 -6 RL stroke}!\n"

	/* x y thumb - 'thumb' sign */
	"/thumb{	dlw M currentpoint -2.5 7 RM\n"
	"	0 6 5 6 5 0 RC\n"
	"	0 -6 -5 -6 -5 0 RC\n"
	"	2 add M 0 -4 RL stroke}!\n"

	/* y hl - helper line at height y */
	"/hl{	0.8 setlinewidth x -6.5 add exch M\n"
	"	13 0 RL stroke}!\n"

	/* y hl1 - longer helper line */
	"/hl1{	0.8 setlinewidth x -8 add exch M\n"
	"	16 0 RL stroke}!\n"

	/* -- accidentals -- */
	/* x y sh0 - sharp sign */
	"/sh0{	gsave T 0.9 setlinewidth\n"
	"	-1.2 -8.4 M 0 15.4 RL\n"
	"	1.4 -7.2 M 0 15.4 RL stroke\n"
	"	-2.6 -3 M 5.4 1.6 RL 0 -2.2 RL -5.4 -1.6 RL 0 2.2 RL fill\n"
	"	-2.6 3.4 M 5.4 1.6 RL 0 -2.2 RL -5.4 -1.6 RL 0 2.2 RL fill\n"
	"	grestore}!\n"
	/* dx sh - sharp relative to head */
	"/sh{x add y sh0}!\n"
	/* x y ft0 - flat sign */
	"/ft0{	gsave T 0.8 setlinewidth\n"
	"	-1.8 2.5 M\n"
	"	6.4 3.3 6.5 -3.6 0 -6.6 RC\n"
	"	4.6 3.9 4.5 7.6 0 5.7 RC\n"
	"	currentpoint fill M\n"
	"	0 7.1 RM 0 -12.6 RL stroke\n"
	"	grestore}!\n"
	/* dx ft - flat relative to head */
	"/ft{x add y ft0}!\n"
	/* x y nt0 - natural sign */
	"/nt0{	gsave T 0.5 setlinewidth\n"
	"	-2 -4.3 M 0 12.2 RL\n"
	"	1.3 -7.8 M 0 12.2 RL stroke\n"
	"	2.1 setlinewidth\n"
	"	-2 -2.9 M 3.3 0.6 RL\n"
	"	-2 2.4 M 3.3 0.6 RL stroke\n"
	"	grestore}!\n"
	/* dx nt - natural relative to head */
	"/nt{x add y nt0}!\n"
	/* x y ftx - narrow flat sign */
	"/ftx{	M -1.4 2.7 RM\n"
	"	5.7 3.1 5.7 -3.6 0.0 -6.7 RC\n"
	"	3.9 4.0 4.0 7.6 0.0 5.8 RC\n"
	"	currentpoint fill M\n"
	"	dlw 0 7.1 RM 0 -12.4 RL stroke}!\n"
	/* x y dft0 ft - double flat sign */
	"/dft0{2 copy exch 2.5 sub exch ftx exch 1.5 add exch ftx}!\n"
	/* dx dft - double flat relative to head */
	"/dft{x add y dft0}!\n"
	/* x y dsh0 - double sharp */
	"/dsh0{	2 copy M 0.7 setlinewidth\n"
	"	-2 -2 RM 4 4 RL\n"
	"	-4 0 RM 4 -4 RL stroke\n"
	"	0.5 setlinewidth 2 copy M 1.3 -1.3 RM\n"
	"	2 -0.2 RL 0.2 -2 RL -2 0.2 RL -0.2 2 RL fill\n"
	"	2 copy M 1.3 1.3 RM\n"
	"	2 0.2 RL 0.2 2 RL -2 -0.2 RL -0.2 -2 RL fill\n"
	"	2 copy M -1.3 1.3 RM\n"
	"	-2 0.2 RL -0.2 2 RL 2 -0.2 RL 0.2 -2 RL fill\n"
	"	M -1.3 -1.3 RM\n"
	"	-2 -0.2 RL -0.2 -2 RL 2 0.2 RL 0.2 2 RL fill}!\n"
	/* dx dsh - double sharp relative to head */
	"/dsh{x add y dsh0}!\n"

	/* accidentals in guitar chord */
	"/tempstr 1 string def\n"
	"/sharp_glyph{\n"
	"	fh 0.4 mul 0 RM currentpoint\n"
	"	gsave T fh 0.08 mul dup scale 0 7 sh0 grestore\n"
	"	fh 0.4 mul 0 RM}!\n"
	"/flat_glyph{\n"
	"	fh 0.4 mul 0 RM currentpoint\n"
	"	gsave T fh 0.08 mul dup scale 0 5 ft0 grestore\n"
	"	fh 0.4 mul 0 RM}!\n"
	"/nat_glyph{\n"
	"	fh 0.4 mul 0 RM currentpoint\n"
	"	gsave T fh 0.08 mul dup scale 0 7 nt0 grestore\n"
	"	fh 0.4 mul 0 RM}!\n"
	/* str gcshow - guitar chord */
	"/gcshow{\n"
	"	{dup 129 eq {sharp_glyph}\n"
	"	  {dup 130 eq {flat_glyph}\n"
	"	    {dup 131 eq {nat_glyph}\n"
	"		{tempstr 0 2 index put tempstr show}\n"
	"		ifelse}\n"
	"	    ifelse}\n"
	"	  ifelse pop}\n"
	"	forall}!\n"
	/* x y w h box - draw a box */
	"/box{0.6 setlinewidth"
#if PS_LEVEL >= 2
	" rectstroke}!\n"
#else
	" 4 2 roll M 2 copy\n"
	"	0 exch RL 0 RL neg 0 exch RL neg 0 RL stroke}!\n"
#endif

	/* h x y bar - thin bar */
	"/bar{M dlw 0 exch RL stroke}!\n"

	/* h x y dbar - dashed bar */
	"/dabar{[5] 0 setdash bar [] 0 setdash}!\n"

	/* h x y thbar - thick bar */
	"/thbar{M dup 0 exch RL 3 0 RL 0 exch neg RL fill}!\n"

	/* x y rdots - repeat dots */
	"/rdots{	9 add M currentpoint 2 copy 1.2 0 360 arc\n"
	"	6 add M currentpoint 1.2 0 360 arc fill}!\n"

	/* x y csig - C timesig */
	"/csig{	M\n"
	"	1.0 17.3 RM\n"
	"	0.9 -0.0 2.3 -0.7 2.4 -2.2 RC\n"
	"	-1.2 2.0 -3.6 -0.1 -1.6 -1.7 RC\n"
	"	2.0 -1.0 3.8 3.5 -0.8 4.7 RC\n"
	"	-2.0 0.4 -6.4 -1.3 -5.8 -7.0 RC\n"
	"	0.4 -6.4 7.9 -6.8 9.1 -0.7 RC\n"
	"	-2.3 -5.6 -6.7 -5.1 -6.8 0.0 RC\n"
	"	-0.5 4.4 0.7 7.5 3.5 6.9 RC\n"
	"	fill}!\n"

	/* x y ctsig - C| timesig */
	"/ctsig{dlw 2 copy csig 4 add M 0 16 RL stroke}!\n"

	/* (top) (bot) x y tsig - time signature */
	"/tsig{	M gsave /Times-Bold 16 selectfont 1.2 1 scale\n"
	"	0 1 RM currentpoint 3 -1 roll cshow\n"
	"	M 0 12 RM cshow grestore}!\n"

	/* (meter) x y stsig - single time signature */
	"/stsig{	M gsave /Times-Bold 18 selectfont 1.2 1 scale\n"
	"	0 6 RM cshow grestore}!\n"

	/* l x y staff - 5 lines staff */
	"/staff{	M dlw dup 0 RL dup neg 6 RM\n"
	"	dup 0 RL dup neg 6 RM\n"
	"	dup 0 RL dup neg 6 RM\n"
	"	dup 0 RL dup neg 6 RM\n"
	"	0 RL stroke}!\n"

	/* x1 x2 sep0 - hline separator */
	"/sep0{dlw 0 M 0 lineto stroke}!\n"

	"/hbrce{	-2.5 1.0 RM\n"
	"	-4.5 -4.6 -7.5 -12.2 -4.4 -26.8 RC\n"
	"	3.5 -14.3 3.2 -21.7 -2.1 -24.2 RC\n"
	"	7.4 2.4 7.3 14.2 3.5 29.5 RC\n"
	"	-2.7 9.5 -1.5 16.2 3.0 21.5 RC\n"
	"	fill}!\n"
	/* h x y brace */
	"/brace{	gsave T 0 0 M 0.01 mul 1.0 exch scale hbrce\n"
	"	0 -100 M 1 -1 scale hbrce grestore}!\n"

	/* h x y bracket */
	"/bracket{M dlw -5 2 RM currentpoint\n"
	"	-1.7 2 RM 10.5 -1 12 4.5 12 3.5 RC\n"
	"	0 -1 -3.5 -5.5 -8.5 -5.5 RC fill\n"
	"	3 setlinewidth M 0 2 RM\n"
	"	0 exch neg -8 add RL currentpoint stroke\n"
	"	dlw M -1.7 0 RM\n"
	"	10.5 1 12 -4.5 12 -3.5 RC\n"
	"	0 1 -3.5 5.5 -8.5 5.5 RC fill}!\n"

	/* nb_measures x y mrest */
	"/mrest{	gsave T 1 setlinewidth\n"
	"	-20 6 M 0 12 RL 20 6 M 0 12 RL stroke\n"
	"	5 setlinewidth -20 12 M 40 0 RL stroke\n"
	"	/Times-Bold 15 selectfont 0 28 M cshow grestore}!\n"

	/* x y mrep - measure repeat */
	"/mrep{	2 copy 2 copy\n"
	"	M -5 16 RM currentpoint 1.4 0 360 arc\n"
	"	M 5 8 RM currentpoint 1.4 0 360 arc\n"
	"	M -7 6 RM 11 12 RL 3 0 RL -11 -12 RL -3 0 RL\n"
	"	fill}!\n"

	/* x y mrep2 - measure repeat 2 times */
	"/mrep2{	2 copy 2 copy\n"
	"	M -5 18 RM currentpoint 1.4 0 360 arc\n"
	"	M 5 6 RM currentpoint 1.4 0 360 arc fill\n"
	"	M 1.8 setlinewidth\n"
	"	-7 4 RM 14 10 RL -14 -4 RM 14 10 RL\n"
	"	stroke}!\n"

	/* str bracket_type dx x y repbra - repeat bracket */
	"/repbra{gsave dlw T 0 -20 M\n"
	"	0 20 3 index 1 ne {RL} {RM} ifelse 0 RL 0 ne {0 -20 RL} if stroke\n"
	"	4 -13 M show grestore}!\n"

	/* pp2x pp1x p1 pp1 pp2 p2 p1 SL */
	"/SL{M curveto RL curveto fill}!\n"

	/* -- text -- */
	"/dsp{dup stringwidth pop}!\n"
	"/glue{	2 copy length exch length add string dup 4 2 roll 2 index 0 3 index\n"
	"	putinterval exch length exch putinterval}!\n"
	"/TXT{/txt exch def}!\n"
	"/rejoin{( ) search pop exch glue}!\n"
	"/measure{dsp txt stringwidth pop add textwidth 2 add gt}!\n"
	"/join{txt exch glue TXT}!\n"
	"/find{search {pop 3 -1 roll 1 add 3 1 roll}{pop exit} ifelse}!\n"
	"/spacecount{0 exch ( ) {find} loop}!\n"
	"/jproc{dsp textwidth exch sub exch dup spacecount}!\n"
	"/popzero{dup 0 eq {pop}{div} ifelse}!\n"
	"/justify{jproc 1 sub 3 2 roll exch popzero 0 32 4 3 roll widthshow} def\n"

	/* str lwidth P1 */
	"/P1{	/textwidth exch def () TXT\n"
	"	dup spacecount{\n"
	"		rejoin measure {gsave txt show grestore LF () TXT join}{join} ifelse\n"
	"	} repeat gsave txt show grestore LF () TXT pop}!\n"

	/* str lwidth P2 */
	"/P2{	/textwidth exch def () TXT\n"
	"	dup spacecount{\n"
	"		rejoin measure {gsave txt justify grestore LF () TXT join}{join} ifelse\n"
	"	} repeat gsave txt show grestore LF () TXT pop}!\n"

	/* x y hd - full head */
	"/hd{	xymove\n"
	"	3.5 2.0 RM\n"
	"	-2.0 3.5 -9.0 -0.5 -7.0 -4.0 RC\n"
	"	2.0 -3.5 9.0 0.5 7.0 4.0 RC fill}!\n"

	/* x y Hd - open head for half */
	"/Hd{	xymove\n"
	"	3.0 1.6 RM\n"
	"	-1.0 1.8 -7.0 -1.4 -6.0 -3.2 RC\n"
	"	1.0 -1.8 7.0 1.4 6.0 3.2 RC\n"
	"	0.5 0.3 RM\n"
	"	2.0 -3.8 -5.0 -7.6 -7.0 -3.8 RC\n"
	"	-2.0 3.8 5.0 7.6 7.0 3.8 RC\n"
	"	fill}!\n"

	/* x y HD - open head for whole */
	"/HD{	xymove\n"
	"	-1.6 2.4 RM\n"
	"	2.8 1.6 6.0 -3.2 3.2 -4.8 RC\n"
	"	-2.8 -1.6 -6.0 3.2 -3.2 4.8 RC\n"
	"	7.2 -2.4 RM\n"
	"	0.0 1.8 -2.2 3.2 -5.6 3.2 RC\n"
	"	-3.4 0.0 -5.6 -1.4 -5.6 -3.2 RC\n"
	"	0.0 -1.8 2.2 -3.2 5.6 -3.2 RC\n"
	"	3.4 0.0 5.6 1.4 5.6 3.2 RC\n"
	"	fill}!\n"

	/* x y HDD - round breve */
	"/HDD{	dlw HD\n"
	"	x y M -6 -4 RM 0 8 RL\n"
	"	x y M 6 -4 RM 0 8 RL stroke}!\n"

	/* x y breve - square breve */
	"/breve{	xymove\n"
	"	2.5 setlinewidth -6 -2.7 RM 12 0 RL\n"
	"	0 5.4 RM -12 0 RL stroke\n"
	"	dlw x y M -6 -5 RM 0 10 RL\n"
	"	x y M 6 -5 RM 0 10 RL stroke}!\n"

	/* x y longa */
	"/longa{	xymove\n"
	"	2.5 setlinewidth -6 -2.7 RM 12 0 RL\n"
	"	0 5.4 RM -12 0 RL stroke\n"
	"	dlw x y M -6 -5 RM 0 10 RL\n"
	"	x y M 6 -10 RM 0 15 RL stroke}!\n"

	/* tin whistle */
	"/tw_head{/Helvetica 8.0 selectfont\n"
	"	0 -45 M 90 rotate (WHISTLE) show -90 rotate\n"
	"	/Helvetica-Bold 36.0 selectfont\n"
	"	0 -45 M show .5 setlinewidth newpath}!\n"
	"/tw_under{\n"
	"	1 index 2.5 sub -4 M 2.5 -2.5 RL 2.5 2.5 RL\n"
	"	-2.5 -2.5 RM 0 6 RL stroke}!\n"
	"/tw_over{\n"
	"	1 index 2.5 sub -3 M 2.5 2.5 RL 2.5 -2.5 RL\n"
	"	-2.5 2.5 RM 0 -6 RL stroke}!\n"
	"/tw_0{7 sub 2 copy 3.5 sub 3 0 360 arc stroke}!\n"
	"/tw_1{7 sub 2 copy 3.5 sub 2 copy 3 90 270 arc fill 3 270 90 arc stroke}!\n"
	"/tw_2{7 sub 2 copy 3.5 sub 3 0 360 arc fill}!\n"
	"/tw_p{pop -55 M 0 6 RL -3 -3 RM 6 0 RL stroke}!\n"
	"/tw_pp{	pop 3 sub -53.5 M 6 0 RL\n"
	"	-1.5 -1.5 RM 0 3 RL\n"
	"	-3 0 RM 0 -3 RL stroke}!\n";

static char *enc_tb[MAXENC] = {
	/* 1 */
	"/space /exclamdown /cent /sterling /currency /yen /brokenbar /section\n"
	"/dieresis /copyright /ordfeminine /guillemotleft /logicalnot /hyphen /registered /macron\n"
/*	"/degree /plusminus /twosuperior /threesuperior /acute /mu /paragraph /bullet\n" */
	"/degree /plusminus /twosuperior /threesuperior /acute /mu /paragraph /periodcentered\n"
/*	"/cedilla /dotlessi /ordmasculine /guillemotright /onequarter /onehalf /threequarters /questiondown\n" */
	"/cedilla /onesuperior /ordmasculine /guillemotright /onequarter /onehalf /threequarters /questiondown\n"
	/* (300) */
	"/Agrave /Aacute /Acircumflex /Atilde /Adieresis /Aring /AE /Ccedilla\n"
	"/Egrave /Eacute /Ecircumflex /Edieresis /Igrave /Iacute /Icircumflex /Idieresis\n"
	"/Eth /Ntilde /Ograve /Oacute /Ocircumflex /Otilde /Odieresis /multiply\n"
	"/Oslash /Ugrave /Uacute /Ucircumflex /Udieresis /Yacute /Thorn /germandbls\n"
	"/agrave /aacute /acircumflex /atilde /adieresis /aring /ae /ccedilla\n"
	"/egrave /eacute /ecircumflex /edieresis /igrave /iacute /icircumflex /idieresis\n"
	"/eth /ntilde /ograve /oacute /ocircumflex /otilde /odieresis /divide\n"
	"/oslash /ugrave /uacute /ucircumflex /udieresis /yacute /thorn /ydieresis",
	/* 2 */
	"/space /Aogonek /breve /Lslash /currency /Lcaron /Sacute /section\n"
	"/dieresis /Scaron /Scedilla /Tcaron /Zacute /hyphen /Zcaron /Zdotaccent\n"
	"/degree /aogonek /ogonek /lslash /acute /lcaron /sacute /caron\n"
	"/cedilla /scaron /scedilla /tcaron /zacute /hungarumlaut /zcaron /zdotaccent\n"
	/* (300) */
	"/Racute /Aacute /Acircumflex /Abreve /Adieresis /Lacute /Cacute /Ccedilla\n"
	"/Ccaron /Eacute /Eogonek /Edieresis /Ecaron /Iacute /Icircumflex /Dcaron\n"
	"/Dbar /Nacute /Ncaron /Oacute /Ocircumflex /Ohungarumlaut /Odieresis /multiply\n"
	"/Rcaron /Uring /Uacute /Uhungarumlaut /Udieresis /Yacute /Tcedilla /germandbls\n"
	"/racute /aacute /acircumflex /abreve /adieresis /lacute /cacute /ccedilla\n"
	"/ccaron /eacute /eogonek /edieresis /ecaron /iacute /icircumflex /dcaron\n"
	"/dbar /nacute /ncaron /oacute /ocircumflex /ohungarumlaut /odieresis /divide\n"
	"/rcaron /uring /uacute /uhungarumlaut /udieresis /yacute /tcedilla /dotaccent",
	/* 3 */
	"/space /Hstroke /breve /sterling /currency /yen /Hcircumflex /section\n"
	"/dieresis /Idotaccent /Scedilla /Gbreve /Jcircumflex /hyphen /registered /Zdotaccent\n"
	"/degree /hstroke /twosuperior /threesuperior /acute /mu /hcircumflex /bullet\n"
	"/cedilla /dotlessi /scedilla /gbreve /jcircumflex /onehalf /threequarters /zdotaccent\n"
	/* (300) */
	"/Agrave /Aacute /Acircumflex /Atilde /Adieresis /Cdotaccent /Ccircumflex /Ccedilla\n"
	"/Egrave /Eacute /Ecircumflex /Edieresis /Igrave /Iacute /Icircumflex /Idieresis\n"
	"/Eth /Ntilde /Ograve /Oacute /Ocircumflex /Gdotaccent /Odieresis /multiply\n"
	"/Gcircumflex /Ugrave /Uacute /Ucircumflex /Udieresis /Ubreve /Scircumflex /germandbls\n"
	"/agrave /aacute /acircumflex /atilde /adieresis /cdotaccent /ccircumflex /ccedilla\n"
	"/egrave /eacute /ecircumflex /edieresis /igrave /iacute /icircumflex /idieresis\n"
	"/eth /ntilde /ograve /oacute /ocircumflex /gdotaccent /odieresis /divide\n"
	"/gcircumflex /ugrave /uacute /ucircumflex /udieresis /ubreve /scircumflex /dotaccent",
	/* 4 */
	"/space /Aogonek /kra /Rcedilla /currency /Itilde /Lcedilla /section\n"
	"/dieresis /Scaron /Emacron /Gcedilla /Tbar /hyphen /Zcaron /macron\n"
	"/degree /aogonek /ogonek /rcedilla /acute /itilde /lcedilla /caron\n"
	"/cedilla /scaron /emacron /gcedilla /tbar /Eng /zcaron /eng\n"
	/* (300) */
	"/Amacron /Aacute /Acircumflex /Atilde /Adieresis /Aring /AE /Iogonek\n"
	"/Ccaron /Eacute /Eogonek /Edieresis /Edotaccent /Iacute /Icircumflex /Imacron\n"
	"/Eth /Ncedilla /Omacron /Kcedilla /Ocircumflex /Otilde /Odieresis /multiply\n"
	"/Oslash /Uogonek /Uacute /Ucircumflex /Udieresis /Utilde /Umacron /germandbls\n"
	"/amacron /aacute /acircumflex /atilde /adieresis /aring /ae /iogonek\n"
	"/ccaron /eacute /eogonek /edieresis /edotaccent /iacute /icircumflex /imacron\n"
	"/dbar /ncedilla /omacron /kcedilla /ocircumflex /otilde /odieresis /divide\n"
	"/oslash /uogonek /uacute /ucircumflex /udieresis /utilde /umacron /dotaccent",
	/* 5 */
	"/space /exclamdown /cent /sterling /currency /yen /brokenbar /section\n"
	"/dieresis /copyright /ordfeminine /guillemotleft /logicalnot /hyphen /registered /macron\n"
	"/degree /plusminus /twosuperior /threesuperior /acute /mu /paragraph /bullet\n"
	"/cedilla /dotlessi /ordmasculine /guillemotright /onequarter /onehalf /threequarters /questiondown\n"
	/* (300) */
	"/Agrave /Aacute /Acircumflex /Atilde /Adieresis /Aring /AE /Ccedilla\n"
	"/Egrave /Eacute /Ecircumflex /Edieresis /Igrave /Iacute /Icircumflex /Idieresis\n"
	"/Gbreve /Ntilde /Ograve /Oacute /Ocircumflex /Otilde /Odieresis /multiply\n"
	"/Oslash /Ugrave /Uacute /Ucircumflex /Udieresis /Idotaccent /Scedilla /germandbls\n"
	"/agrave /aacute /acircumflex /atilde /adieresis /aring /ae /ccedilla\n"
	"/egrave /eacute /ecircumflex /edieresis /igrave /iacute /icircumflex /idieresis\n"
	"/gbreve /ntilde /ograve /oacute /ocircumflex /otilde /odieresis /divide\n"
	"/oslash /ugrave /uacute /ucircumflex /udieresis /dotlessi /scedilla /ydieresis",
	/* 6 */
	"/space /Aogonek /Emacron /Gcedilla /Imacron /Itilde /Kcedilla /Lcedilla\n"
	"/acute /Rcedilla /Scaron /Tbar /Zcaron /hyphen /kra /Eng\n"
	"/dbar /aogonek /emacron /gcedilla /imacron /itilde /kcedilla /lcedilla\n"
	"/nacute /rcedilla /scaron /tbar /zcaron /section /germandbls /eng\n"
	/* (300) */
	"/Amacron /Aacute /Acircumflex /Atilde /Adieresis /Aring /AE /Iogonek\n"
	"/Ccaron /Eacute /Eogonek /Edieresis /Edotaccent /Iacute /Icircumflex /Idieresis\n"
	"/Dbar /Ncedilla /Omacron /Oacute /Ocircumflex /Otilde /Odieresis /Utilde\n"
	"/Oslash /Uogonek /Uacute /Ucircumflex /Udieresis /Yacute /Thorn /Umacron\n"
	"/amacron /aacute /acircumflex /atilde /adieresis /aring /ae /iogonek\n"
	"/ccaron /eacute /eogonek /edieresis /edotaccent /iacute /icircumflex /idieresis\n"
	"/eth /ncedilla /omacron /oacute /ocircumflex /otilde /odieresis /utilde\n"
	"/oslash /uogonek /uacute /ucircumflex /udieresis /yacute /thorn /umacron"
};

/* -- define which latin encoding -- */
void define_encoding(int enc)	/* Latin encoding number */
{
	if (enc <= 0)
		return;
	fprintf(fout,
		"/ISOLatin%dEncoding [\n"
		"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
		"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
		"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
		"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
		"/space /exclam /quotedbl /numbersign /dollar /percent /ampersand /quoteright\n"
		"/parenleft /parenright /asterisk /plus /comma /minus /period /slash\n"
		"/zero /one /two /three /four /five /six /seven\n"
		"/eight /nine /colon /semicolon /less /equal /greater /question\n"
		/* (100) */
		"/at /A /B /C /D /E /F /G\n"
		"/H /I /J /K /L /M /N /O\n"
		"/P /Q /R /S /T /U /V /W\n"
		"/X /Y /Z /bracketleft /backslash /bracketright /asciicircum /underscore\n"
		"/quoteleft /a /b /c /d /e /f /g\n"
		"/h /i /j /k /l /m /n /o\n"
		"/p /q /r /s /t /u /v /w\n"
		"/x /y /z /braceleft /bar /braceright /asciitilde /.notdef\n"
		/* (200) */
		"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
		"/.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n"
		"/dotlessi /grave /acute /circumflex /tilde /macron /breve /dotaccent\n"
		"/dieresis /.notdef /ring /cedilla /.notdef /hungarumlaut /ogonek /caron\n"
		"%s\n"
		"] def\n",
		enc, enc_tb[enc - 1]);
}

/* -- define_font -- */
void define_font(char name[],
		 int num)
{
	if (strcmp(name, "Symbol") == 0) {
		fprintf(fout, "/F%d{dup 0.8 mul /fh exch def"
			" /%s exch selectfont}!\n",
			num, name);
		return;
	}
	fprintf(fout, "/%s-ISO /%s mkfontext\n"
		"/F%d{dup 0.8 mul /fh exch def /%s-ISO exch selectfont}!\n",
		name, name, num, name);
}

/* -- def_stems -- */
static void def_stems(FILE *fp)
{
	/* len su - up stem */
	fprintf(fp, "/su{dlw x y M %.1f %.1f RM %.1f sub 0 exch RL stroke}!\n",
		STEM_XOFF, STEM_YOFF, STEM_YOFF);

	/* len sd - down stem */
	fprintf(fp, "/sd{dlw x y M %.1f %.1f RM neg %.1f add 0 exch RL stroke}!\n",
		-STEM_XOFF, -STEM_YOFF, STEM_YOFF);
}

/* -- stem and flags -- */
static void def_flags(FILE *fp)
{
	/* n len sfu - stem and n flag up */
	fprintf(fp, "/sfu{	dlw x y M %.1f %.1f RM\n"
		"	%.1f sub 0 exch RL currentpoint stroke\n"
		"	M dup 1 eq\n"
		"	  {\n"
		"		pop\n"
		"		0.6 -5.6 9.6 -9.0 5.6 -18.4 RC\n"
		"		1.6 6.0 -1.3 11.6 -5.6 12.8 RC\n"
		"		fill\n"
		"	  }{\n"
		"		2 1 3 -1 roll {\n"
		"			pop currentpoint\n"
		"			0.9 -3.7 9.1 -6.4 6.0 -12.4 RC\n"
		"			1.0 5.4 -4.2 8.4 -6.0 8.4 RC\n"
		"			fill -5.4 add M\n"
		"		} for\n"
		"		1.2 -3.2 9.6 -5.7 5.6 -14.6 RC\n"
		"		1.6 5.4 -1.0 10.2 -5.6 11.4 RC\n"
		"		fill\n"
		"	  }\n"
		"	ifelse}!\n",
		STEM_XOFF, STEM_YOFF, STEM_YOFF);

	/* n len sfd - stem and n flag down */
	fprintf(fp, "/sfd{	dlw x y M -%.1f -%.1f RM\n"
		"	neg %.1f add 0 exch RL currentpoint stroke\n"
		"	M dup 1 eq\n"
		"	  {\n"
		"		pop\n"
		"		0.6 5.6 9.6 9.0 5.6 18.4 RC\n"
		"		1.6 -6.0 -1.3 -11.6 -5.6 -12.8 RC\n"
		"		fill\n"
		"	  }{\n"
		"		2 1 3 -1 roll {\n"
		"			pop currentpoint\n"
		"			0.9 3.7 9.1 6.4 6.0 12.4 RC\n"
		"			1.0 -5.4 -4.2 -8.4 -6.0 -8.4 RC\n"
		"			fill 5.4 add M\n"
		"		} for\n"
		"			1.2 3.2 9.6 5.7 5.6 14.6 RC\n"
		"			1.6 -5.4 -1.0 -10.2 -5.6 -11.4 RC\n"
		"		fill\n"
		"	  }\n"
		"	ifelse}!\n",
		STEM_XOFF, STEM_YOFF, STEM_YOFF);

	/* n len sfs - stem and n straight flag down */
	fprintf(fp, "/sfs{	dlw x y M -%.1f -%.1f RM\n"
		"	neg %.1f add 0 exch RL currentpoint stroke\n"
		"	M 1 1 3 -1 roll {\n"
		"		pop currentpoint\n"
		"		7 %.1f RL\n"
		"		0 %.1f RL\n"
		"		-7 -%.1f RL\n"
		"		fill 5.4 add M\n"
		"	} for}!\n",
		STEM_XOFF, STEM_YOFF, STEM_YOFF,
		BEAM_DEPTH, BEAM_DEPTH, BEAM_DEPTH);
}

/* -- grace notes -- */
static void def_gnote(FILE *fp)
{
	/* x y ghd - grace note head */
	fprintf(fp, "/ghd{	xymove\n"
		"	-1.3 1.5 RM\n"
		"	2.4 2 5 -1 2.6 -3 RC\n"
		"	-2.4 -2 -5 1 -2.6 3 RC fill}!\n"

		/* l gu - grace note stem */
		"/gu{	0.6 setlinewidth x y M\n"
		"	%.1f 0 RM 0 exch RL stroke}!\n",
		GSTEM_XOFF);

	/* n len sgu - gnote stem and n flag up */
	fprintf(fp, "/sgu{	0.6 setlinewidth x y M %.1f 0 RM\n"
		"	0 exch RL currentpoint stroke\n"
		"	M dup 1 eq\n"
		"	  {\n"
		"		pop\n"
		"		0.6 -3.4 5.6 -3.8 3.0 -10.0 RC\n"
		"		1.2 4.4 -1.4 7.0 -3.0 7.0 RC\n"
		"		fill\n"
		"	  }{\n"
		"		1 1 3 -1 roll {\n"
		"			pop currentpoint\n"
		"			1.0 -3.2 5.6 -2.8 3.2 -8.0 RC\n"
		"			1.4 4.8 -2.4 5.4 -3.2 5.2 RC\n"
		"			fill -3.5 add M\n"
		"		} for\n"
		"	  }\n"
		"	ifelse}!\n",
		GSTEM_XOFF);

	/* n len sgs - gnote stem and n straight flag up */
	fprintf(fp, "/sgs{	0.6 setlinewidth x y M %.1f 0 RM\n"
		"	0 exch RL currentpoint stroke\n"
		"	M 1 1 3 -1 roll {\n"
		"		pop currentpoint\n"
		"		3 -1.5 RL 0 -2 RL -3 1.5 RL\n"
		"		closepath fill -3 add M\n"
		"	} for}!\n",
		GSTEM_XOFF);

	/* ga - acciaccatura */
	fprintf(fp,
		"/ga{x y M -1 4 RM 9 5 RL stroke}!\n"

		/* x y ghl - grace note helper line */
		"/ghl{	0.6 setlinewidth x -3 add exch M\n"
		"	6 0 RL stroke}!\n"

		/* x1 y2 x2 y2 x3 y3 x0 y0 gsl */
		"/gsl{dlw M curveto stroke}!\n"

		/* -- grace note accidentals -- */
		"/gsc{gsave x add y T 0.7 dup scale 0 0}!\n"
		/* x y gsh */
		"/gsh{gsc sh0 grestore}!\n"
		/* x y gnt */
		"/gnt{gsc nt0 grestore}!\n"
		/* x y gft */
		"/gft{gsc ft0 grestore}!\n"
		/* x y gdsh */
		"/gdsh{gsc dsh0 grestore}!\n"
		/* x y gdft */
		"/gdft{gsc dft0 grestore}!\n");
}

/* -- define_symbols: write postscript macros to file -- */
void define_symbols(void)
{
	fputs(ps_head, fout);
	def_stems(fout);
	def_flags(fout);
	def_gnote(fout);
}
