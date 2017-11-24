/*
 * Postscript definitions.
 *
 * This file is part of abcm2ps.
 *
 * Copyright (C) 1998-2017 Jean-François Moine
 * Adapted from abc2ps, Copyright (C) 1996,1997 Michael Methfessel
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <string.h>

#include "abcm2ps.h"

static char ps_head[] =
	"/xydef{/y exch def/x exch def}!\n"
	"/xymove{/x 2 index def/y 1 index def M}!\n"

	/* str showc - center at current pt */
	"/showc{dup stringwidth pop .5 mul neg 0 RM show}!\n"

	/* str showr - show right-aligned */
	"/showr{dup stringwidth pop neg 0 RM show}!\n"

	/* str showb - show in box */
	"/showb{	dup currentpoint 3 -1 roll show\n"
	"	.6 SLW\n"
	"	exch 2 sub exch 3 sub 3 -1 roll\n"
	"	stringwidth pop 4 add\n"
	"	currentfont/ScaleMatrix get 0 get .8 mul\n"
	"	4 add rectstroke}!\n"

	/* x y oct - upper/lower clef '8' */
	"/oct{/Times-Roman 12 selectfont M(8)show}!\n"

	/* x y octu - upper '8' - compatibility */
	"/octu{oct}!\n"
	/* x y octl - lower '8'  - compatibility */
	"/octl{oct}!\n"

	/* t dx dy x y bm - beam, depth t */
	"/bm{	M 3 copy RL neg 0 exch RL neg exch neg exch RL 0 exch RL fill}!\n"

	/* str x y bnum - tuplet number / ratio */
	"/bnum{M/Times-Italic 12 selectfont showc}!\n"
	/* same with clearing below the number */
	"/bnumb{	currentgray/Times-Italic 12 selectfont\n"
	"	3 index stringwidth pop 4 add\n"
	"	dup .5 mul neg 4 index add 3 index 3 -1 roll 8\n"
	"	1.0 setgray rectfill setgray M showc}!\n"

	/* dx dy x y tubr - tuplet bracket */
	"/tubr{3 sub M 0 3 RL RL 0 -3 RL dlw stroke}!\n"
	"/tubrl{3 add M 0 -3 RL RL 0 3 RL dlw stroke}!\n"

	/* dx dy dt - dot relative to head */
	"/dt{x y M RM currentpoint 1.2 0 360 arc fill}!\n"

	/* x y dnb - down bow */
	"/dnb{	dlw M -3.2 2 RM\n"
	"	0 7.2 RL\n"
	"	6.4 0 RM\n"
	"	0 -7.2 RL\n"
	"	currentpoint stroke M\n"
	"	-6.4 4.8 RM\n"
	"	0 2.4 RL\n"
	"	6.4 0 RL\n"
	"	0 -2.4 RL\n"
	"	fill}!\n"

	/* x y upb - up bow */
	"/upb{	dlw M -2.6 9.4 RM\n"
	"	2.6 -8.8 RL\n"
	"	2.6 8.8 RL\n"
	"	stroke}!\n"

	/* x y grm - gracing mark */
	"/grm{	M -5 2.5 RM\n"
	"	5 8.5 5.5 -4.5 10 2 RC\n"
	"	-5 -8.5 -5.5 4.5 -10 -2 RC fill}!\n"

	/* x y stc - staccato mark */
	"/stc{	3 add M currentpoint 1.2 0 360 arc fill}!\n"

	/* x y emb - emphasis bar */
	"/emb{	1.2 SLW 1 setlinecap M -2.5 3 RM 5 0 RL stroke 0 setlinecap}!\n"

	/* x y cpu - roll sign above head */
	"/cpu{	M -6 0 RM\n"
	"	0.4 7.3 11.3 7.3 11.7 0 RC\n"
	"	-1.3 6 -10.4 6 -11.7 0 RC fill}!\n"

	/* x y sld - slide */
	"/sld{	M -7.2 -4.8 RM\n"
	"	1.8 -0.7 4.5 0.2 7.2 4.8 RC\n"
	"	-2.1 -5 -5.4 -6.8 -7.6 -6 RC fill}!\n"

	/* x y trl - trill sign */
	"/trl{	/Times-BoldItalic 16 selectfont M -4 2 RM(tr)show}!\n"

	/* str x y fng - finger (0-5) */
	"/fng{/Bookman-Demi 8 selectfont M -3 1 RM show}!\n"

	/* str x y dacs - D.C. / D.S. */
	"/dacs{/Times-Roman 16 selectfont 3 add M showc}!\n"

	/* str x y crdc - italic annotations */
	"/crdc{/Times-Italic 14 selectfont 5 add M show}!\n"

	/* x y brth - breath */
	"/brth{/Times-BoldItalic 30 selectfont 6 add M(,)show}!\n"

	/* str x y pf - p, f, pp, .. */
	"/pf{/Times-BoldItalic 16 selectfont 5 add M show}!\n"

	/* str x y sfz */
	"/sfz{	M -7 5 RM pop\n"
	"	/Times-Italic 14 selectfont(s)show\n"
	"	/Times-BoldItalic 16 selectfont(f)show\n"
	"	/Times-Italic 14 selectfont(z)show}!\n"

	/* w x y cresc - crescendo */
	"/cresc{	1 SLW M dup 5 RM\n"
	"	defl 1 and 0 eq\n"
	"	{dup neg 4 RL 4 RL}\n"
	"	{dup neg 2.2 RL 0 3.6 RM 2.2 RL}\n"
	"	ifelse stroke}!\n"

	/* w x y dim - diminuendo */
	"/dim{	1 SLW 5 add M\n"
	"	defl 2 and 0 eq\n"
	"	{dup 4 RL neg 4 RL}\n"
	"	{dup 2.2 RL 0 3.6 RM neg 2.2 RL}\n"
	"	ifelse stroke}!\n"

	// w x y o8va - ottava
	"/o8va{	M\n"
	"	defl 1 and 0 eq\n"
	"	{14 sub currentpoint\n"
	"	-5 0 RM /Times-BoldItalic 12 selectfont(8)show\n"
	"	0 4 RM /Times-BoldItalic 10 selectfont(va)show\n"
	"	M 14 0 RM}if 0 6 RM\n"
	"	[6] 0 setdash 5 sub 0 RL currentpoint stroke [] 0 setdash\n"
	"	M defl 2 and 0 eq\n"
	"	{0 -6 RL stroke}if}!\n"

	// w x y o8vb - ottava bassa
	"/o8vb{	M\n"
	"	defl 1 and 0 eq\n"
	"	{8 sub currentpoint\n"
	"	-5 0 RM /Times-BoldItalic 12 selectfont(8)show\n"
	"	0 4 RM /Times-BoldItalic 10 selectfont(vb)show\n"
	"	M 8 0 RM}if\n"
	"	[6] 0 setdash 5 sub 0 RL currentpoint stroke [] 0 setdash\n"
	"	M defl 2 and 0 eq\n"
	"	{0 6 RL stroke}if}!\n"

	/* x y dplus - plus */
	"/dplus{	1.2 SLW 0.5 add M 0 6 RL -3 -3 RM 6 0 RL stroke}!\n"

	/* x y trnx - turn with line through it */
	"/turnx{	2 copy turn .6 SLW 1.5 add M 0 9 RL stroke}!\n"

	/* x y lphr - longphrase */
	"/lphr{1.2 SLW M 0 -18 RL stroke}!\n"

	/* x y mphr - mediumphrase */
	"/mphr{1.2 SLW M 0 -12 RL stroke}!\n"

	/* x y sphr - shortphrase */
	"/sphr{1.2 SLW M 0 -6 RL stroke}!\n"

	/* w x y ltr - long trill */
	"/ltr{	gsave 4 add T\n"
	"	0 6 3 -1 roll{\n"
	/*		% first loop draws left half of squiggle; second draws right\n*/
	"		2{\n"
	"			0 0.4 M\n"
	"			2 1.9 3.4 2.3 3.9 0 C\n"
	"			2.1 0 L\n"
	"			1.9 0.8 1.4 0.7 0 -0.4 C\n"
	"			fill\n"
	"			180 rotate -6 0 T\n"
	"		}repeat\n"
	/*		% shift axes right one squiggle*/
	"		pop 6 0 T\n"
	"	}for\n"
	"	grestore}!\n"

	/* h x ylow arp - arpeggio */
	"/arp{gsave 90 rotate exch neg ltr grestore}!\n"

	/* x2 y2 x1 y1 gliss - line glissando */
	"/gliss{	gsave 2 copy T\n"
		"exch 4 -1 roll exch sub 3 1 roll sub\n" // dx dy
		"2 copy exch atan dup rotate\n"		// dx dy alpha
		"exch pop cos div\n"			// len
		"8 0 M 14 sub 0 RL stroke "
		"grestore}!\n"

	/* x2 y2 x1 y1 glisq - squiggly glissando */
	"/glisq{	gsave 2 copy T\n"
		"exch 4 -1 roll exch sub 3 1 roll sub\n" // dx dy
		"2 copy exch atan dup rotate\n"		// dx dy alpha
		"exch pop cos div\n"			// len
		"17 sub 8 -4 ltr "
		"grestore}!\n"

	/* x y wedge - wedge */
	"/wedge{1 add M -1.5 5 RL 3 0 RL -1.5 -5 RL fill}!\n"

	/* x y opend - 'open' sign */
	"/opend{dlw M currentpoint 3 add 2.5 -90 270 arc stroke}!\n"

	/* x y snap - 'snap' sign */
	"/snap{	dlw 2 copy M -3 6 RM\n"
	"	0 5 6 5 6 0 RC\n"
	"	0 -5 -6 -5 -6 0 RC\n"
	"	5 add M 0 -6 RL stroke}!\n"

	/* x y thumb - 'thumb' sign */
	"/thumb{	dlw 2 copy M -2.5 7 RM\n"
	"	0 6 5 6 5 0 RC\n"
	"	0 -6 -5 -6 -5 0 RC\n"
	"	2 add M 0 -4 RL stroke}!\n"

	/* n x y trem - <n> tremolo on one note */
	"/trem{	M -4.5 0 RM{\n"
	"		currentpoint\n"
	"		9 3 RL 0 -3 RL -9 -3 RL 0 3 RL\n"
	"		fill 5.4 sub M\n"
	"	}repeat}!\n"

	/* x y hl - ledger line */
	"/hl{	.8 SLW M -6 0 RM 12 0 RL stroke}!\n"
	/* x y hl1 - longer ledger line */
	"/hl1{	.8 SLW M -7 0 RM 14 0 RL stroke}!\n"
	/* x y hl2 - more longer ledger line */
	"/hl2{	.7 SLW M -9 0 RM 18 0 RL stroke}!\n"

	/* ancillary function for grace note accidentals */
	"/gsc{gsave y T .8 dup scale 0 0}!\n"

	// accidentals for text
	"/uflat{<95200028\n"		/* width 400 */
	"	0064000001b802ee\n"
	"	006402ea\n"
	"	008402ea\n"
	"	0084000c\n"
	"	00640008\n"
	"	00840154\n"
	"	00b2019c011c01ae01540168\n"
	"	01b800fa00dc00220084000c\n"
	"	00840028\n"
	"	00ba0028014c00f60106014a\n"
	"	00d401860084014e00840128\n"
	"	><0b00010303030a0105050105050a>}cvlit def\n"
	"/unat{<95200022\n"		/* width 380 */
	"	003cff42013602ee\n"
	"	006002ee\n"
	"	004002ee\n"
	"	00400022\n"
	"	0060002a\n"
	"	01160060\n"
	"	0116ff46\n"
	"	0136ff46\n"
	"	01360208\n"
	"	011401fe\n"
	"	006001cc\n"
	"	006002ee\n"
	"	0060009e\n"
	"	0060015c\n"
	"	01160190\n"
	"	011600d4\n"
	"	><0b00012a030a0123030a>}cvlit def\n"
	"/usharp{<95200024\n"		/* width 460 */
	"	003cff42019a02ee\n"
	"	008802be\n"
	"	0088ff44\n"
	"	00a8ff44\n"
	"	00a802be\n"
	"	0128ff76\n"
	"	0148ff76\n"
	"	014802ee\n"
	"	012802ee\n"
	"	004001d0\n"
	"	0040015c\n"
	"	019201bc\n"
	"	01920230\n"
	"	00400076\n"
	"	00400002\n"
	"	01920064\n"
	"	019200d6\n"
	"	><0b000123030a0123030a0123030a0123030a>}cvlit def\n"
	"/udblesharp{<95200046\n"	/* width 460 */
	"	003c006e019001c2\n"
	"	00f0011a\n"
	"	01180140013a015e018e015e\n"
	"	018e01be\n"
	"	012e01be\n"
	"	012e016a0110014800ea0122\n"
	"	00c2014800a4016a00a401be\n"
	"	004401be\n"
	"	0044015e\n"
	"	009a015e00bc014000e2011a\n"
	"	00bc00f4009a00d6004400d6\n"
	"	00440076\n"
	"	00a40076\n"
	"	00a400ca00c200ec00ea0112\n"
	"	011000ec012e00ca012e0076\n"
	"	018e0076\n"
	"	018e00d6\n"
	"	013a00d6011800f400f0011a\n"
	"	><0b0001050303050503030505030305050303050a>}cvlit def\n"
	"/udbleflat{<9520004c\n"	/* width 500 */
	"	00140000022602ee\n"
	"	001402ea\n"
	"	002c02ea\n"
	"	002c000c\n"
	"	00140008\n"
	"	002c0154\n"
	"	004e019c009e01ae00c80168\n"
	"	011300fa00660022002c000c\n"
	"	002c0028\n"
	"	0054002800c200f6008d014a\n"
	"	00680186002c014e002c0128\n"
	"	010e02ea\n"
	"	012602ea\n"
	"	0126000c\n"
	"	010e0008\n"
	"	01260154\n"
	"	0148019c019801ae01c20168\n"
	"	020d00fa016000220126000c\n"
	"	01260028\n"
	"	014e002801bc00f60187014a\n"
	"	016201860126014e01260128\n"
	"	><0b000123030a0105050105050a0123030a0105050105050a>}cvlit def\n"

	/* some microtone accidentals */
	/* 1/4 ton sharp */
	"/sh1{	gsave T .9 SLW\n"
	"	0 -7.8 M 0 15.4 RL stroke\n"
	"	-1.8 -2.7 M 3.6 1.1 RL 0 -2.2 RL -3.6 -1.1 RL 0 2.2 RL fill\n"
	"	-1.8 3.7 M 3.6 1.1 RL 0 -2.2 RL -3.6 -1.1 RL 0 2.2 RL fill\n"
	"	grestore}!\n"
	/* 3/4 ton sharp */
	"/sh513{	gsave T .8 SLW\n"
	"	-2.5 -8.7 M 0 15.4 RL\n"
	"	0 -7.8 M 0 15.4 RL\n"
	"	2.5 -6.9 M 0 15.4 RL stroke\n"
	"	-3.7 -3.1 M 7.4 2.2 RL 0 -2.2 RL -7.4 -2.2 RL 0 2.2 RL fill\n"
	"	-3.7 3.2 M 7.4 2.2 RL 0 -2.2 RL -7.4 -2.2 RL 0 2.2 RL fill\n"
	"	grestore}!\n"
	/* 1/4 ton flat */
	"/ft1{gsave -1 1 scale exch neg exch ft0 grestore}!\n"
	/* x y ftx - narrow flat sign */
	"/ftx{	-1.4 2.7 RM\n"
	"	5.7 3.1 5.7 -3.6 0 -6.7 RC\n"
	"	3.9 4 4 7.6 0 5.8 RC\n"
	"	currentpoint fill 7.1 add M\n"
	"	dlw 0 -12.4 RL stroke}!\n"
	/* 3/4 ton flat */
	"/ft513{2 copy gsave -1 1 scale exch neg 3 add exch M ftx grestore\n"
	"	M 1.5 0 RM ftx}!\n"
	/* microscale= 4 */
	"/sh4tb[/.notdef/sh1/sh0/sh513/dsh0]def\n"
	"/sh4{sh4tb exch get cvx exec}!\n"
	"/ft4tb[/.notdef/ft1/ft0/ft513/dft0]def\n"
	"/ft4{ft4tb exch get cvx exec}!\n"

	/* -- bars -- */
	/* h x y bar - thin bar */
	"/bar{M 1 SLW 0 exch RL stroke}!\n"
	/* h x y dotbar - dotted bar */
	"/dotbar{[5] 0 setdash bar [] 0 setdash}!\n"
	/* h x y thbar - thick bar */
	"/thbar{3 -1 roll 3 exch rectfill}!\n"
	/* x y rdots - repeat dots */
	"/rdots{	2 copy 9 add M currentpoint 1.2 0 360 arc\n"
	"	15 add M currentpoint 1.2 0 360 arc fill}!\n"

	/* x y xxsig - old time signatures ('o', 'o.', 'c' 'c.') */
	"/pmsig{0.3 SLW 12 add M currentpoint 5 0 360 arc stroke}!\n"
	"/pMsig{2 copy pmsig 12 add M currentpoint 1.3 0 360 arc fill}!\n"
	"/imsig{0.3 SLW 12 add 2 copy 5 add M 5 60 300 arc stroke}!\n"
	"/iMsig{2 copy imsig 12 add M currentpoint 1.3 0 360 arc fill}!\n"

	/* (top) (bot) x y tsig - time signature */
	"/tsig{	1 add M gsave/Times-Bold 16 selectfont 1.2 1 scale\n"
	"	currentpoint 3 -1 roll showc\n"
	"	12 add M showc grestore}!\n"

	/* (meter) x y stsig - single time signature */
	"/stsig{	7 add M gsave/Times-Bold 18 selectfont 1.2 1 scale\n"
	"	showc grestore}!\n"

	/* l x sep0 - hline separator */
	"/sep0{	dlw 0 M 0 RL stroke}!\n"

	/* h x y bracket */
	"/bracket{M -5 2 RM currentpoint\n"
	"	-1.7 2 RM 10.5 -1 12 4.5 12 3.5 RC\n"
	"	0 -1 -3.5 -5.5 -8.5 -5.5 RC fill\n"
	"	3 SLW 2 add M\n"
	"	0 exch neg 8 sub RL currentpoint stroke\n"
	"	M -1.7 0 RM\n"
	"	10.5 1 12 -4.5 12 -3.5 RC\n"
	"	0 1 -3.5 5.5 -8.5 5.5 RC fill}!\n"

	/* x y srep - sequence repeat */
	"/srep{	M -1 -6 RM 11 12 RL 3 0 RL -11 -12 RL -3 0 RL fill}!\n"

	/* str dy bracket_type dx x y repbra - repeat bracket */
	"/repbra{gsave dlw T 0 -20 M\n"
	"	0 20 3 index 1 and 1 eq{RL}{RM}ifelse 0\n"
	"	RL 2 and 2 eq{0 -20 RL}if stroke\n"
	"	4 exch M show grestore}!\n"

	/* pp2x pp1x p1 pp1 pp2 p2 p1 SL - slur / tie */
	"/SL{M RC RL RC closepath fill}!\n"

	/* pp2x pp1x p1 pp1 pp2 p2 p1 dSL - dotted slur / tie */
	"/dSL{	M [4] 0 setdash .8 SLW RC stroke [] 0 setdash}!\n"

	/* -- text -- */
	"/strw{	gsave 0 -2000 M/strop/show load def 1 setgray str\n"
	"	0 setgray currentpoint pop/w exch def grestore}!\n"
	"/jshow{w 0 32 4 -1 roll widthshow}!\n"
	"/strop/show load def\n"
	"/arrayshow{{dup type/stringtype eq{strop}{glyphshow}ifelse}forall}def\n"

	/* str gcshow - guitar chord */
	"/gcshow{show}!\n"
	"/agcshow{arrayshow}!\n"
	/* x y w h box - draw a box */
	"/box{.6 SLW rectstroke}!\n"
	/* set the end of a box */
	"/boxend{currentpoint pop/x exch def}!\n"
	/* mark the right most  end of a box */
	"/boxmark{currentpoint pop dup x gt\n"
	"	{/x exch def}{pop}ifelse}!\n"
	/* x y dy boxdraw - draw a box around a guitar chord */
	"/boxdraw{x 3 index sub 2 add exch box}!\n"
	/* w str gxshow - expand a guitar chord */
	"/gxshow{0 9 3 -1 roll widthshow}!\n"

	/* str anshow - annotation */
	"/anshow{show}!\n"
	"/aanshow{arrayshow}!\n"

	/* -- lyrics under notes -- */
	/* w x y wln - underscore line */
	"/wln{M .8 SLW 0 RL stroke}!\n"
	/* w x y hyph - hyphen */
	"/hyph{	.8 SLW 3 add M\n"
	"	dup cvi 20 idiv 3 mul 25 add\n"	/* w d */
	"	1 index cvi exch idiv 1 add "	/* w n */
		"exch "				/* n w */
		"1 index div\n"			/* n dx */
	"	dup 4 sub "			/* n dx (dx-4) */
		"3 1 roll "			/* (dx-4) n dx */
		".5 mul 2 sub 0 RM\n"		/* (dx / 2 - 4) rmoveto */
	"	{4 0 RL dup 0 RM}repeat stroke pop}!\n"
	/* str lyshow - lyrics */
	"/lyshow{show}!\n"
	"/alyshow{arrayshow}!\n"

	/* -- default percussion heads -- */
	/* x y pfthd - percussion flat head */
	"/pfthd{/x 2 index def/y 1 index def dsh0\n"
	"	.7 SLW x y M x y 4 0 360 arc stroke}!\n"
	/* same for dble sharp/flat */
	"/pdshhd{pshhd}!\n"
	"/pdfthd{pfthd}!\n"

	/* x y ghd - grace note head */
	"/ghd{	xymove\n"
	"	1.7 1.5 RM\n"
	"	-1.32 2.31 -5.94 -0.33 -4.62 -2.64 RC\n"
	"	1.32 -2.31 5.94 0.33 4.62 2.64 RC fill}!\n"

	/* dx dy gua / gda - acciaccatura */
	"/gua{x y M -1 4 RM RL stroke}!\n"
	"/gda{x y M -5 -4 RM RL stroke}!\n"

	/* x y ghl - grace note ledger line */
	"/ghl{	.6 SLW M -3.5 0 RM 7 0 RL stroke}!\n"

	/* x1 y2 x2 y2 x3 y3 x0 y0 gsl - grace note slur */
	"/gsl{dlw M RC stroke}!\n"

	/* x y custos */
	"/custos{2 copy M -4 0 RM 2 2.5 RL 2 -2.5 RL 2 2.5 RL 2 -2.5 RL\n"
	"	-2 -2.5 RL -2 2.5 RL -2 -2.5 RL -2 2.5 RL fill\n"
	"	M 3.5 0 RM 5 7 RL dlw stroke}!\n"

#ifdef HAVE_PANGO
	"/glypharray{{glyphshow}forall}!\n"
#endif

	/* x y showerror */
	"/showerror{gsave 1 0.7 0.7 setrgbcolor 2.5 SLW newpath\n"
	"	30 0 360 arc stroke grestore}!\n"

	"/pdfmark where{pop}{userdict/pdfmark/cleartomark load put}ifelse\n"

	"0 setlinecap 0 setlinejoin\n";

/* PS direct glyphs */
static char psdgl[] = 
	"/hbrce{	-2.5 1 RM\n"
	"	-4.5 -4.6 -7.5 -12.2 -4.4 -26.8 RC\n"
	"	3.5 -14.3 3.2 -21.7 -2.1 -24.2 RC\n"
	"	7.4 2.4 7.3 14.2 3.5 29.5 RC\n"
	"	-2.7 9.5 -1.5 16.2 3 21.5 RC\n"
	"	fill}!\n"
	/* h x y brace */
	"/brace{	gsave T 0 0 M .01 mul 1 exch scale hbrce\n"
	"	0 -100 M 1 -1 scale hbrce grestore}!\n"
	/* x y sgno - segno */
	"/sgno{	3 add M currentpoint currentpoint currentpoint\n"
	"	1.5 -1.7 6.4 0.3 3 3.7 RC\n"
	"	-10.4 7.8 -8 10.6 -6.5 11.9 RC\n"
	"	4 1.9 5.9 -1.7 4.2 -2.6 RC\n"
	"	-1.3 -0.7 -2.9 1.3 -0.7 2 RC\n"
	"	-1.5 1.7 -6.4 -0.3 -3 -3.7 RC\n"
	"	10.4 -7.8 8 -10.6 6.5 -11.9 RC\n"
	"	-4 -1.9 -5.9 1.7 -4.2 2.6 RC\n"
	"	1.3 0.7 2.9 -1.3 0.7 -2 RC\n"
	"	fill\n"
	"	M 0.8 SLW -6 1.2 RM 12.6 12.6 RL stroke\n"
	"	7 add exch 6 sub exch 1.2 0 360 arc fill\n"
	"	8 add exch 6 add exch 1.2 0 360 arc fill}!\n"
	/* x y coda - coda */
	"/coda{	1 SLW 2 add 2 copy M 0 20 RL\n"
	"	2 copy M -10 10 RM 20 0 RL stroke\n"
	"	10 add 6 0 360 arc 1.7 SLW stroke}!\n"
	/* x y tclef - treble clef */
	"/utclef{<95200072\n"
	"	0000ff2e01c2030c\n"
	"	00ac0056\n"
	"	0064007f006400f400e00112\n"
	"	0176011c01bc0056013a0012\n"
	"	00c8ffde002700120015009a\n"
	"	0006014f0072017f00f101e8\n"
	"	0149023f0140026d012f02ba\n"
	"	00fc029900d1025100d60200\n"
	"	00e700f500fa008a0107ffc2\n"
	"	010dff6200f4ff3c00baff3b\n"
	"	006aff3a003cff98007dffc0\n"
	"	00d2ffe90102ff5b009cff57\n"
	"	00b3ff4600f8ff3200f6ffb3\n"
	"	00ec009200cf010900c4021c\n"
	"	00c4027600c402be01240304\n"
	"	015c02bc0163021e013a01e3\n"
	"	00f001790039013b003b00a7\n"
	"	0044000e00cfffee01370022\n"
	"	018d0063015400e200e700d2\n"
	"	00a000c6007e008f00ac0056\n"
	"	><0b000132050a>}cvlit def\n"
	"/tclef{gsave T -10 -6 T .045 dup scale utclef ufill grestore}!\n"
	/* x y cclef */
	"/ucclef{<95200066\n"
	"	006effbe01e70256\n"
	"	00d10108\n"
	"	00d10002\n"
	"	00c40002\n"
	"	00c40213\n"
	"	00d10213\n"
	"	00d10113\n"
	"	00ea012700fa013701100180\n"
	"	011e0161011d014d0148013a\n"
	"	01a2011801a80244011f01f3\n"
	"	015301e0013a01a3011401a6\n"
	"	00ba01cc01350256019f01eb\n"
	"	01e7019c01a000fa01190131\n"
	"	0109010a\n"
	"	011900e4\n"
	"	01a0011b01e70079019f002a\n"
	"	0135ffbe00ba00490114006f\n"
	"	013a007201530035011f0022\n"
	"	01a8ffd101a200fd014800db\n"
	"	011d00c8011b00bd0110009b\n"
	"	00fa00e400ea00f400d10108\n"
	"	006e0213\n"
	"	00a70213\n"
	"	00a70002\n"
	"	006e0002\n"
	"	006e0213\n"
	"	><0b000125032605220326050a0124030a>}cvlit def\n"
	"/cclef{gsave T -12 -12 T .045 dup scale ucclef ufill grestore}!\n"
	/* x y bclef - bass clef */
	"/ubclef{<95200046\n"
	"	00000050019a0244\n"
	"	00010057\n"
	"	007d007a00df00a500ff0143\n"
	"	012a022700580239003f01aa\n"
	"	007a01fa00dc0194009b015c\n"
	"	005d012d00280172003101b4\n"
	"	00460241013f023c01430180\n"
	"	014200d100d9007800010057\n"
	"	01660151\n"
	"	016601750199017301990151\n"
	"	0199012c0166012d01660151\n"
	"	016401d2\n"
	"	016401f6019701f4019701d2\n"
	"	019701ac016401ad016401d2\n"
	"	><0b000126050a0122050a0122050a>}cvlit def\n"
	"/bclef{gsave T -10 -18 T .045 dup scale ubclef ufill grestore}!\n"
	/* x y pclef */
	"/pclef{	exch 2.7 sub exch -9 add 5.4 18 1.4 SLW rectstroke}!\n"
	"/spclef{pclef}!\n"
	"/stclef{gsave T -10 -6 T .037 dup scale utclef ufill grestore}!\n"
	"/scclef{gsave T -12 -10 T .037 dup scale ucclef ufill grestore}!\n"
	"/sbclef{gsave T -10 -15 T .037 dup scale ubclef ufill grestore}!\n"
	/* x y csig - C timesig */
	"/csig{	M\n"
	"	6 5.3 RM\n"
	"	0.9 0 2.3 -0.7 2.4 -2.2 RC\n"
	"	-1.2 2 -3.6 -0.1 -1.6 -1.7 RC\n"
	"	2 -1 3.8 3.5 -0.8 4.7 RC\n"
	"	-2 0.4 -6.4 -1.3 -5.8 -7 RC\n"
	"	0.4 -6.4 7.9 -6.8 9.1 -0.7 RC\n"
	"	-2.3 -5.6 -6.7 -5.1 -6.8 0 RC\n"
	"	-0.5 4.4 0.7 7.5 3.5 6.9 RC\n"
	"	fill}!\n"
	/* x y ctsig - C| timesig */
	"/ctsig{dlw 2 copy csig M 5 -8 RM 0 16 RL stroke}!\n"
	/* x y HDD - round breve */
	"/HDD{	dlw HD\n"
	"	x y M -6 -4 RM 0 8 RL\n"
	"	12 0 RM 0 -8 RL stroke}!\n"
	/* x y breve - square breve */
	"/breve{	xymove\n"
	"	2.5 SLW -6 -2.7 RM 12 0 RL\n"
	"	0 5.4 RM -12 0 RL stroke\n"
	"	dlw x y M -6 -5 RM 0 10 RL\n"
	"	12 0 RM 0 -10 RL stroke}!\n"
	/* x y HD - open head for whole */
	"/HD{	xymove\n"
	"	-2.7 1.4 RM\n"
	"	1.5 2.8 6.9 0 5.3 -2.7 RC\n"
	"	-1.5 -2.8 -6.9 0 -5.3 2.7 RC\n"
	"	8.3 -1.4 RM\n"
	"	0 1.5 -2.2 3 -5.6 3 RC\n"
	"	-3.4 0 -5.6 -1.5 -5.6 -3 RC\n"
	"	0 -1.5 2.2 -3 5.6 -3 RC\n"
	"	3.4 0 5.6 1.5 5.6 3 RC fill}!\n"
	/* x y Hd - open head for half */
	"/Hd{	xymove\n"
	"	3 1.6 RM\n"
	"	-1 1.8 -7 -1.4 -6 -3.2 RC\n"
	"	1 -1.8 7 1.4 6 3.2 RC\n"
	"	0.5 0.3 RM\n"
	"	2 -3.8 -5 -7.6 -7 -3.8 RC\n"
	"	-2 3.8 5 7.6 7 3.8 RC fill}!\n"
	/* x y hd - full head */
	"/uhd{{	100 -270 640 280\n"
	"	560 82\n"
	"	474 267 105 105 186 -80\n"
	"	267 -265 636 -102 555 82\n"
	"	}<0b000122050a>}cvlit def\n"
	"/hd{	/x 2 index def/y 1 index def\n"
	"	gsave T -7.4 0 T .02 dup scale uhd ufill grestore}!\n"
	/* x y ft0 - flat sign */
	"/ft0{	gsave T -3.5 -3.5 T .018 dup scale uflat ufill grestore}!\n"
	/* x y nt0 - natural sign */
	"/nt0{	gsave T -3 -5 T .018 dup scale unat ufill grestore}!\n"
	/* x y sh0 - sharp sign */
	"/sh0{	gsave T -4 -5 T .018 dup scale usharp ufill grestore}!\n"
	/* x y dsh0 - double sharp */
	"/dsh0{	gsave T -4 -5 T .018 dup scale udblesharp ufill grestore}!\n"
	/* x y pshhd - percussion sharp head */
	"/pshhd{/x 2 index def/y 1 index def dsh0}!\n"
	/* x y dft0 - double flat sign */
	"/dft0{	gsave T -4 -3.5 T .018 dup scale udbleflat ufill grestore}!\n"
	/* x y accent - accent */
	"/accent{1.2 SLW M -4 1 RM 8 2 RL -8 2 RL stroke}!\n"
	/* x y marcato - accent */
	"/marcato{M -3 0 RM 3 7 RL 3 -7 RL -1.5 0 RL -1.8 4.2 RL -1.7 -4.2 RL fill}!\n"
	/* x y hld - fermata */
	"/hld{	1.5 add 2 copy 1.5 add M currentpoint 1.3 0 360 arc\n"
	"	M -7.5 0 RM\n"
	"	0 11.5 15 11.5 15 0 RC\n"
	"	-0.25 0 RL\n"
	"	-1.25 9 -13.25 9 -14.5 0 RC\n"
	"	fill}!\n"
	/* x y r00 - longa rest */
	"/r00{	xymove -1.5 -6 RM currentpoint 3 12 rectfill}!\n"
	/* x y r0 - breve rest */
	"/r0{	xymove -1.5 0 RM currentpoint 3 6 rectfill}!\n"
	/* x y r1 - rest */
	"/r1{	xymove -3.5 3 RM currentpoint 7 3 rectfill}!\n"
	/* x y r2 - half rest */
	"/r2{	xymove -3.5 0 RM currentpoint 7 3 rectfill}!\n"
	/* x y r4 - quarter rest */
	"/r4{	xymove\n"
	"	-1 8.5 RM\n"
	"	3.6 -5.1 RL\n"
	"	-2.1 -5.2 RL\n"
	"	2.2 -4.3 RL\n"
	"	-2.6 2.3 -5.1 0 -2.4 -2.6 RC\n"
	"	-4.8 3 -1.5 6.9 1.4 4.1 RC\n"
	"	-3.1 4.5 RL\n"
	"	1.9 5.1 RL\n"
	"	-1.5 3.5 RL\n"
	"	fill}!\n"
	/* 1/8 .. 1/64 rest element */
	"/r8e{	-1.5 -1.5 -2.4 -2 -3.6 -2 RC\n"
	"	2.4 2.8 -2.8 4 -2.8 1.2 RC\n"
	"	0 -2.7 4.3 -2.4 5.9 -0.6 RC\n"
	"	fill}!\n"
	/* x y r8 - eighth rest */
	"/r8{	xymove\n"
	"	.5 SLW 3.3 4 RM\n"
	"	-3.4 -9.6 RL stroke\n"
	"	x y M 3.4 4 RM r8e}!\n"
	/* x y r16 - 16th rest */
	"/r16{	xymove\n"
	"	.5 SLW 3.3 4 RM\n"
	"	-4 -15.6 RL stroke\n"
	"	x y M 3.4 4 RM r8e\n"
	"	x y M 1.9 -2 RM r8e}!\n"
	/* x y r32 - 32th rest */
	"/r32{	xymove\n"
	"	.5 SLW 4.8 10 RM\n"
	"	-5.5 -21.6 RL stroke\n"
	"	x y M 4.9 10 RM r8e\n"
	"	x y M 3.4 4 RM r8e\n"
	"	x y M 1.9 -2 RM r8e}!\n"
	/* x y r64 - 64th rest */
	"/r64{	xymove\n"
	"	.5 SLW 4.8 10 RM\n"
	"	-7 -27.6 RL stroke\n"
	"	x y M 4.9 10 RM r8e\n"
	"	x y M 3.4 4 RM r8e\n"
	"	x y M 1.9 -2 RM r8e\n"
	"	x y M 0.4 -8 RM r8e}!\n"
	/* x y r128 - 128th rest */
	"/r128{	xymove\n"
	"	.5 SLW 5.8 16 RM\n"
	"	-8.5 -33.6 RL stroke\n"
	"	x y M 5.9 16 RM r8e\n"
	"	x y M 4.4 10 RM r8e\n"
	"	x y M 2.9 4 RM r8e\n"
	"	x y M 1.4 -2 RM r8e\n"
	"	x y M -0.1 -8 RM r8e}!\n"
	/* x y mrest */
	"/mrest{	M currentpoint 1 SLW\n"
	"	-20 -6 RM 0 12 RL 40 0 RM 0 -12 RL stroke\n"
	"	M 5 SLW -20 0 RM 40 0 RL stroke}!\n"
	/* x y mrep - measure repeat */
	"/mrep{	2 copy 2 copy\n"
	"	M -5 3 RM currentpoint 1.4 0 360 arc\n"
	"	M 5 -3 RM currentpoint 1.4 0 360 arc\n"
	"	M -7 -6 RM 11 12 RL 3 0 RL -11 -12 RL -3 0 RL fill}!\n"
	/* x y mrep2 - measure repeat 2 times */
	"/mrep2{	2 copy 2 copy\n"
	"	M -5 6 RM currentpoint 1.4 0 360 arc\n"
	"	M 5 -6 RM currentpoint 1.4 0 360 arc fill\n"
	"	M 1.8 SLW\n"
	"	-7 -8 RM 14 10 RL -14 -4 RM 14 10 RL stroke}!\n"
	/* x y turn - turn */
	"/turn{	M 5.2 8 RM\n"
	"	1.4 -0.5 0.9 -4.8 -2.2 -2.8 RC\n"
	"	-4.8 3.5 RL\n"
	"	-3 2 -5.8 -1.8 -3.6 -4.4 RC\n"
	"	1 -1.1 2 -0.8 2.1 0.1 RC\n"
	"	0.1 0.9 -0.7 1.2 -1.9 0.6 RC\n"
	"	-1.4 0.5 -0.9 4.8 2.2 2.8 RC\n"
	"	4.8 -3.5 RL\n"
	"	3 -2 5.8 1.8 3.6 4.4 RC\n"
	"	-1 1.1 -2 0.8 -2.1 -0.1 RC\n"
	"	-0.1 -0.9 0.7 -1.2 1.9 -0.6 RC\n"
	"	fill}!\n"
	/* x y umrd - upper mordent */
	"/umrd{	4 add M\n"
	"	2.2 2.2 RL 2.1 -2.9 RL 0.7 0.7 RL\n"
	"	-2.2 -2.2 RL -2.1 2.9 RL -0.7 -0.7 RL\n"
	"	-2.2 -2.2 RL -2.1 2.9 RL -0.7 -0.7 RL\n"
	"	2.2 2.2 RL 2.1 -2.9 RL 0.7 0.7 RL fill}!\n"
	/* x y lmrd - lower mordent */
	"/lmrd{	2 copy umrd M .6 SLW 0 8 RL stroke}!\n"
	// dummy !ped! and !ped-up!
	"/ped{	/Times-BoldItalic 16 selectfont M -10 2 RM(Ped)show}!\n"
	"/pedoff{	/Times-BoldItalic 16 selectfont M -4 2 RM(*)show}!\n"
	/* x y longa */
	"/longa{	xymove\n"
	"	2.5 SLW -6 -2.7 RM 12 0 RL\n"
	"	0 5.4 RM -12 0 RL stroke\n"
	"	dlw x y M -6 -5 RM 0 10 RL\n"
	"	12 0 RM 0 -16 RL stroke}!\n";

/* PS font glyphs */
static char psfgl[] =
	"/musgly{music 24 selectfont glyphshow}!\n"
	"/brace{gsave\n"
	"	T -7.5 0 M -.042 mul 3 exch scale/uniE000 musgly\n"
	"	grestore}!\n"
	"/sgno{	M -6 4 RM/uniE047 musgly}!\n"
	"/coda{	M -12 6 RM/uniE048 musgly}!\n"
	"/tclef{M -8 0 RM/uniE050 musgly}!\n"
	"/cclef{M -8 0 RM/uniE05C musgly}!\n"
	"/bclef{M -8 0 RM/uniE062 musgly}!\n"
	"/pclef{M -6 0 RM/uniE069 musgly}!\n"
	"/stclef{M -8 0 RM/uniE07A musgly}!\n"
	"/scclef{M -8 0 RM/uniE07B musgly}!\n"
	"/sbclef{M -7 0 RM/uniE07C musgly}!\n"
	"/csig{	M 0 0 RM/uniE08A musgly}!\n"
	"/ctsig{M 0 0 RM/uniE08B musgly}!\n"
	"/HDD{	xymove -7 0 RM/uniE0A0 musgly}!\n"
	"/breve{xymove -6 0 RM/uniE0A1 musgly}!\n"
	"/HD{	xymove -5.2 0 RM/uniE0A2 musgly}!\n"
	"/Hd{	xymove -3.8 0 RM/uniE0A3 musgly}!\n"
	"/hd{	xymove -3.7 0 RM/uniE0A4 musgly}!\n"
	"/ft0{	M -3 0 RM/uniE260 musgly}!\n"
	"/nt0{	M -2 0 RM/uniE261 musgly}!\n"
	"/sh0{	M -3 0 RM/uniE262 musgly}!\n"
	"/dsh0{	M -3 0 RM/uniE263 musgly}!\n"
	"/pshhd{xymove -3 0 RM/uniE263 musgly}!\n"
	"/dft0{	M -3 0 RM/uniE264 musgly}!\n"
	"/accent{M -3 0 RM/uniE4A0 musgly}!\n"
	"/marcato{M -3 0 RM/uniE4AC musgly}!\n"
	"/hld{	M -7 0 RM/uniE4C0 musgly}!\n"
	"/r00{	xymove -1.5 0 RM/uniE4E1 musgly}!\n"
	"/r0{	xymove -1.5 0 RM/uniE4E2 musgly}!\n"
	"/r1{	xymove -3.5 6 RM/uniE4E3 musgly}!\n"
	"/r2{	xymove -3.2 0 RM/uniE4E4 musgly}!\n"
	"/r4{	xymove -3 0 RM/uniE4E5 musgly}!\n"
	"/r8{	xymove -3 0 RM/uniE4E6 musgly}!\n"
	"/r16{	xymove -4 0 RM/uniE4E7 musgly}!\n"
	"/r32{	xymove -4 0 RM/uniE4E8 musgly}!\n"
	"/r64{	xymove -4 0 RM/uniE4E9 musgly}!\n"
	"/r128{	xymove -4 0 RM/uniE4EA musgly}!\n"
	"/mrest{M -10 0 RM/uniE4EE musgly}!\n"
	"/mrep{	M -6 0 RM/uniE500 musgly}!\n"
	"/mrep2{M -9 0 RM/uniE501 musgly}!\n"
	"/turn{	M -4 0 RM/uniE567 musgly}!\n"
	"/umrd{	M -7 2 RM/uniE56C musgly}!\n"
	"/lmrd{	M -7 2 RM/uniE56D musgly}!\n"
	"/ped{	M -10 0 RM/uniE650 musgly}!\n"
	"/pedoff{M -6 0 RM/uniE655 musgly}!\n"
	"/longa{xymove -6 0 RM/uniE95C musgly}!\n";

/* -- define a font -- */
void define_font(char name[],
		 int num,
		 int enc)
{
	if (enc == 0)		/* utf-8 */
		fprintf(fout, "/%s-utf8/%s mkfont\n"
			"/F%d{/%s-utf8 exch selectfont}!\n",
			name, name, num, name);
	else			/* native encoding */
		fprintf(fout, "/F%d{/%s exch selectfont}!\n", num, name);
}

/* -- output the symbol definitions -- */
void define_symbols(void)
{
	char *p, *q, *r;

	p = cfmt.musicfont;
	fputs(ps_head, fout);
	fputs(p ? psfgl : psdgl, fout);

	// if a music font, give it a name
	if (p) {
		q = strchr(p, '(');
		if (q) {		// hope "url(...)"
			q++;
			r = strrchr(q, '.');	// remove the file type
			if (!r)
				r = q + strlen(p) - 1;		// ')'
			p = strrchr(q, DIRSEP);	// and the directory path
			if (p)
				q = p + 1;
			fprintf(fout, "/music/%.*s def\n", (int) (r - q), q);
		} else {
			fprintf(fout, "/music/%s def\n", p);
		}
	}

	/* len su - up stem */
	fprintf(fout, "/su{dlw x y M %.1f %.1f RM %.1f sub 0 exch RL stroke}!\n",
		STEM_XOFF, STEM_YOFF, STEM_YOFF);

	/* len sd - down stem */
	fprintf(fout, "/sd{dlw x y M %.1f %.1f RM %.1f add 0 exch RL stroke}!\n",
		-STEM_XOFF, -STEM_YOFF, STEM_YOFF);

	/* n len sfu - stem and n flags up */
	fprintf(fout, "/sfu{	dlw x y M %.1f %.1f RM\n"
		"	%.1f sub 0 exch RL currentpoint stroke\n"
		"	M dup 1 eq{\n"
		"		pop\n"
		"		0.6 -5.6 9.6 -9 5.6 -18.4 RC\n"
		"		1.6 6 -1.3 11.6 -5.6 12.8 RC fill\n"
		"	  }{\n"
		"		1 sub{	currentpoint\n"
		"			0.9 -3.7 9.1 -6.4 6 -12.4 RC\n"
		"			1 5.4 -4.2 8.4 -6 8.4 RC\n"
		"			fill 5.4 sub M\n"
		"		}repeat\n"
		"		1.2 -3.2 9.6 -5.7 5.6 -14.6 RC\n"
		"		1.6 5.4 -1 10.2 -5.6 11.4 RC fill\n"
		"	  }ifelse}!\n",
		STEM_XOFF, STEM_YOFF, STEM_YOFF);

	/* n len sfd - stem and n flags down */
	fprintf(fout, "/sfd{	dlw x y M %.1f %.1f RM\n"
		"	%.1f add 0 exch RL currentpoint stroke\n"
		"	M dup 1 eq{\n"
		"		pop\n"
		"		0.6 5.6 9.6 9 5.6 18.4 RC\n"
		"		1.6 -6 -1.3 -11.6 -5.6 -12.8 RC fill\n"
		"	  }{\n"
		"		1 sub{	currentpoint\n"
		"			0.9 3.7 9.1 6.4 6 12.4 RC\n"
		"			1 -5.4 -4.2 -8.4 -6 -8.4 RC\n"
		"			fill 5.4 add M\n"
		"		}repeat\n"
		"		1.2 3.2 9.6 5.7 5.6 14.6 RC\n"
		"		1.6 -5.4 -1 -10.2 -5.6 -11.4 RC fill\n"
		"	  }ifelse}!\n",
		-STEM_XOFF, -STEM_YOFF, STEM_YOFF);

	/* n len sfs - stem and n straight flag down */
	fprintf(fout, "/sfs{	dup 0 lt{\n"
		"		dlw x y M -%.1f -%.1f RM\n"
		"		%.1f add 0 exch RL currentpoint stroke\n"
		"		M{	currentpoint\n"
		"			7 %.1f RL\n"
		"			0 %.1f RL\n"
		"			-7 -%.1f RL\n"
		"			fill 5.4 add M\n"
		"		}repeat\n"
		"	}{\n"
		"		dlw x y M %.1f %.1f RM\n"
		"		%.1f sub 0 exch RL currentpoint stroke\n"
		"		M{	currentpoint\n"
		"			7 -%.1f RL\n"
		"			0 -%.1f RL\n"
		"			-7 %.1f RL\n"
		"			fill 5.4 sub M\n"
		"		}repeat\n"
		"	}ifelse}!\n",
		STEM_XOFF, STEM_YOFF, STEM_YOFF,
		BEAM_DEPTH, BEAM_DEPTH, BEAM_DEPTH,
		STEM_XOFF, STEM_YOFF, STEM_YOFF,
		BEAM_DEPTH, BEAM_DEPTH, BEAM_DEPTH);

	/* len gu - grace note stem up */
	fprintf(fout, "/gu{	.6 SLW x y M\n"
		"	%.1f 0 RM 0 exch RL stroke}!\n"

	/* len gd - grace note stem down */
		"/gd{	.6 SLW x y M\n"
		"	%.1f 0 RM 0 exch RL stroke}!\n",
		GSTEM_XOFF, -GSTEM_XOFF);

	/* n len sgu - gnote stem and n flag up */
	fprintf(fout, "/sgu{	.6 SLW x y M %.1f 0 RM\n"
		"	0 exch RL currentpoint stroke\n"
		"	M dup 1 eq{\n"
		"		pop\n"
		"		0.6 -3.4 5.6 -3.8 3 -10 RC\n"
		"		1.2 4.4 -1.4 7 -3 7 RC fill\n"
		"	  }{\n"
		"		{	currentpoint\n"
		"			1 -3.2 5.6 -2.8 3.2 -8 RC\n"
		"			1.4 4.8 -2.4 5.4 -3.2 5.2 RC\n"
		"			fill 3.5 sub M\n"
		"		}repeat\n"
		"	  }ifelse}!\n",
		GSTEM_XOFF);

	/* n len sgd - gnote stem and n flag down */
	fprintf(fout, "/sgd{	.6 SLW x y M %.1f 0 RM\n"
		"	0 exch RL currentpoint stroke\n"
		"	M dup 1 eq{\n"
		"		pop\n"
		"		0.6 3.4 5.6 3.8 3 10 RC\n"
		"		1.2 -4.4 -1.4 -7 -3 -7 RC fill\n"
		"	  }{\n"
		"		{	currentpoint\n"
		"			1 3.2 5.6 2.8 3.2 8 RC\n"
		"			1.4 -4.8 -2.4 -5.4 -3.2 -5.2 RC\n"
		"			fill 3.5 add M\n"
		"		}repeat\n"
		"	  }ifelse}!\n",
		-GSTEM_XOFF);

	/* n len sgs - gnote stem and n straight flag up */
	fprintf(fout, "/sgs{	.6 SLW x y M %.1f 0 RM\n"
		"	0 exch RL currentpoint stroke\n"
		"	M{	currentpoint\n"
		"		3 -1.5 RL 0 -2 RL -3 1.5 RL\n"
		"		closepath fill 3 sub M\n"
		"	}repeat}!\n",
		GSTEM_XOFF);
}
