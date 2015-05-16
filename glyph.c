/*
 * utf-8 to glyph translation
 *
 * This file is part of abcm2ps.
 *
 * Copyright (C) 2011-2015 Jean-François Moine (http://moinejf.free.fr)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "abcm2ps.h"

static char *c2[64] = {
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	"nbspace",	"exclamdown",	"cent",		"sterling",
	"currency",	"yen",		"brokenbar",	"section",
	"dieresis",	"copyright",	"ordfeminine",	"guillemotleft",
	"logicalnot",	"sfthyphen",	"registered",	"macron",
	"degree",	"plusminus",	"twosuperior",	"threesuperior",
	"acute",	"mu",		"paragraph",	"periodcentered",
	"cedilla",	"onesuperior",	"ordmasculine",	"guillemotright",
	"onequarter",	"onehalf",	"threequarters","questiondown"
};
static char *c3[64] = {
	"Agrave",	"Aacute",	"Acircumflex",	"Atilde",
	"Adieresis",	"Aring",	"AE",		"Ccedilla",
	"Egrave",	"Eacute",	"Ecircumflex",	"Edieresis",
	"Igrave",	"Iacute",	"Icircumflex",	"Idieresis",
	"Eth",		"Ntilde",	"Ograve",	"Oacute",
	"Ocircumflex",	"Otilde",	"Odieresis",	"multiply",
	"Oslash",	"Ugrave",	"Uacute",	"Ucircumflex",
	"Udieresis",	"Yacute",	"Thorn",	"germandbls",
	"agrave",	"aacute",	"acircumflex",	"atilde",
	"adieresis",	"aring",	"ae",		"ccedilla",
	"egrave",	"eacute",	"ecircumflex",	"edieresis",
	"igrave",	"iacute",	"icircumflex",	"idieresis",
	"eth",		"ntilde",	"ograve",	"oacute",
	"ocircumflex",	"otilde",	"odieresis",	"divide",
	"oslash",	"ugrave",	"uacute",	"ucircumflex",
	"udieresis",	"yacute",	"thorn",	"ydieresis"
};
static char *c4[64] = {
	"Amacron",	"amacron",	"Abreve",	"abreve",
	"Aogonek",	"aogonek",	"Cacute",	"cacute",
	"Ccircumflex",	"ccircumflex",	"Cdotaccent",	"cdotaccent",
	"Ccaron",	"ccaron",	"Dcaron",	"dcaron",
	"Dcroat",	"dcroat",	"Emacron",	"emacron",
	"Ebreve",	"ebreve",	"Edotaccent",	"edotaccent",
	"Eogonek",	"eogonek",	"Ecaron",	"ecaron",
	"Gcircumflex",	"gcircumflex",	"Gbreve",	"gbreve",
	"Gdotaccent",	"gdotaccent",	"Gcommaaccent",	"gcommaaccent",
	"Hcircumflex",	"hcircumflex",	"Hbar",		"hbar",
	"Itilde",	"itilde",	"Imacron",	"imacron",
	"Ibreve",	"ibreve",	"Iogonek",	"iogonek",
	"Idotaccent",	"dotlessi",	"IJ",		"ij",
	"Jcircumflex",	"jcircumflex",	"Kcedilla",	"kcedilla",
	"kgreenlandic",	"Lacute",	"lacute",	"Lcedilla",
	"lcedilla",	"Lcaron",	"lcaron",	"Ldot"
};
static char *c5[64] = {
	"ldot",		"Lslash",	"lslash",	"Nacute",
	"nacute",	"Ncedilla",	"ncedilla",	"tmacron",
	"ncaron",	"napostrophe",	"Eng",		"eng",
	"Omacron",	"omacron",	"Obreve",	"obreve",
	"Ohungarumlaut","ohungarumlaut","OE",		"oe",
	"Racute",	"racute",	"Rcommaaccent",	"rcommaaccent",
	"Rcaron",	"rcaron",	"Sacute",	"sacute",
	"Scircumflex",	"scircumflex",	"Scedilla",	"scedilla",
	"Scaron",	"scaron",	"Tcedilla",	"tcedilla",
	"Tcaron",	"tcaron",	"Tbar",		"tbar",
	"Utilde",	"utilde",	"Umacron",	"umacron",
	"Ubreve",	"ubreve",	"Uring",	"uring",
	"Uhungarumlaut","uhungarumlaut","Uogonek",	"uogonek",
	"Wcircumflex",	"wcircumflex",	"Ycircumflex",	"ycircumflex",
	"Ydieresis",	"Zacute",	"zacute",	"Zdotaccent",
	"zdotaccent",	"Zcaron",	"zcaron",	"longs"
};

static char *ce[64] = {
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, "Delta", NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

static char *e299[64] = {
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
//	NULL, NULL, NULL, NULL, NULL, "uni266D", "uni266E", "uni266F",
	NULL, NULL, NULL, NULL, NULL, "flat", "natural", "sharp",
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};
static char **e2[64] = {
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, e299, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

static char *f09d84[64] = {
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
//	NULL, NULL, "u1D12A", "u1D12B",
	NULL, NULL, "double_sharp", "double_flat",
					NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};
static char **f09d[64] = {
	NULL, NULL, NULL, NULL, f09d84, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};
static char ***f0[64] = {
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, f09d, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

/* 1st character - c2..ff */
static char **utf_1[62] = {
			c2,	c3,	c4,	c5,	NULL,	NULL,
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	ce,	NULL,
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,
	NULL,	NULL,	(char **) e2, NULL,
					NULL,	NULL,	NULL,	NULL,
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,
	(char **) f0, NULL, NULL, NULL,	NULL,	NULL,	NULL,	NULL,
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,
};

/* output the glyph of a utf-8 character */
/* return the next character */
char *glyph_out(char *p)
{
	int i1, i2, i3, i4;
	char **g, *q;

	g = NULL;
	i1 = (unsigned char) *p++ - 0xc2;
	i2 = (unsigned char) *p++ - 0x80;
	if (i1 >= 0xe0 - 0xc2) {
		i3 = (unsigned char) *p++ - 0x80;
		if (i1 >= 0xf0 - 0xc2)
			i4 = (unsigned char) *p++ - 0x80;
		else
			i4 = -1;
	} else {
		i3 = -1;
		i4 = -1;
	}
	if (i1 >= 0 && i2 >= 0) {
		g = (char **) utf_1[i1];
		if (g) {
			g = (char **) g[i2];
			if (i3 >= 0 && g) {
				g = (char **) g[i3];
				if (i4 >= 0 && g)
					g = (char **) g[i4];
			}
		}
		q = (char *) g;
	} else {
		q = NULL;
	}
	if (!q)
		q = ".notdef";
	a2b("/%s", q);
	return p;
}

/* -- add a glyph -- */
/* %%glyph hex_value glyph_name */
void glyph_add(char *p)
{
	int val, i1, i2, i3, i4;
	char **g, **g1, *q;

	val = strtoul(p, &q, 16);	/* unicode value */
	if (val < 0x80
	 || val >= 0x100000) {
		 error(1, 0, "Bad unicode value '%s'", p);
		 return;
	}
	p = q;
	while (isspace(*p))
		p++;
	i3 = i4 = -1;
	if (val < 0x0400) {
		i1 = (val >> 6) - 2;
		i2 = val & 0x3f;
	} else if (val < 0x10000) {
		i1 = (val >> 12) + 0x20 - 2;
		i2 = (val >> 6) & 0x3f;
		i3 = val & 0x3f;
	} else {
		i1 = (val >> 18) + 0x30 - 2;
		i2 = (val >> 12) & 0x3f;
		i3 = (val >> 6) & 0x3f;
		i4 = val & 0x3f;
	}
	g1 = utf_1[i1];
	if (!g1) {
		g1 = calloc(64, sizeof(char **));
		utf_1[i1] = g1;
	}
	if (i3 < 0) {
		g1[i2] = strdup(p);
		return;
	}
	g = (char **) g1[i2];
	if (!g) {
		g = calloc(64, sizeof(char **));
		g1[i2] = (char *) g;
	}
	if (i4 < 0) {
		g[i3] = strdup(p);
		return;
	}
	g1 = (char **) g[i3];
	if (!g1) {
		g1 = calloc(64, sizeof(char **));
		g[i3] = (char *) g1;
	}
	g1[i4] = strdup(p);
}
