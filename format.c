/*
 * Formatting functions.
 *
 * This file is part of abcm2ps.
 *
 * Copyright (C) 1998-2006 Jean-François Moine
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
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <math.h>

#include "abcparse.h"
#include "abc2ps.h"

struct FORMAT cfmt;		/* current format for output */

char font_enc[MAXFONTS];		/* font encoding */
static char *fontnames[MAXFONTS];	/* list of font names */
static char used_font[MAXFONTS];	/* used fonts */
static int nfontnames;
static float staffwidth;

/* format table */
static struct format {
	char *name;
	void *v;
	char type;
#define FORMAT_I 0	/* int */
#define FORMAT_R 1	/* float */
#define FORMAT_F 2	/* font spec */
#define FORMAT_U 3	/* float with unit */
#define FORMAT_B 4	/* boolean */
#define FORMAT_S 5	/* string */
	char subtype;		/* special cases - see code */
	short lock;
} format_tb[] = {
	{"alignbars", &cfmt.alignbars, FORMAT_I, 0},
	{"aligncomposer", &cfmt.aligncomposer, FORMAT_I, 0},
	{"autoclef", &cfmt.autoclef, FORMAT_B, 0},
	{"annotationfont", &cfmt.annotationfont, FORMAT_F, 0},
	{"barnumbers", &cfmt.measurenb, FORMAT_I, 0},
	{"barsperstaff", &cfmt.barsperstaff, FORMAT_I, 0},
	{"botmargin", &cfmt.botmargin, FORMAT_U, 0},
	{"bstemdown", &cfmt.bstemdown, FORMAT_B, 0},
	{"combinevoices", &cfmt.combinevoices, FORMAT_B, 0},
	{"comball", &cfmt.comball, FORMAT_B, 0},
	{"composerfont", &cfmt.composerfont, FORMAT_F, 0},
	{"composerspace", &cfmt.composerspace, FORMAT_U, 0},
	{"contbarnb", &cfmt.contbarnb, FORMAT_B, 0},
	{"continueall", &cfmt.continueall, FORMAT_B, 0},
	{"dateformat", &cfmt.dateformat, FORMAT_S, 0},
	{"dynalign", &cfmt.dynalign, FORMAT_B, 0},
	{"encoding", &cfmt.encoding, FORMAT_I, 1},
	{"exprabove", &cfmt.exprabove, FORMAT_B, 0},
	{"exprbelow", &cfmt.exprbelow, FORMAT_B, 0},
	{"footer", &cfmt.footer, FORMAT_S, 0},
	{"footerfont", &cfmt.footerfont, FORMAT_F, 0},
	{"freegchord", &cfmt.freegchord, FORMAT_B, 0},
	{"flatbeams", &cfmt.flatbeams, FORMAT_B, 0},
	{"gchordbox", &cfmt.gchordbox, FORMAT_B, 0},
	{"gchordfont", &cfmt.gchordfont, FORMAT_F, 3},
	{"graceslurs", &cfmt.graceslurs, FORMAT_B, 0},
	{"header", &cfmt.header, FORMAT_S, 0},
	{"headerfont", &cfmt.headerfont, FORMAT_F, 0},
	{"historyfont", &cfmt.historyfont, FORMAT_F, 0},
	{"hyphencont", &cfmt.hyphencont, FORMAT_B, 0},
	{"indent", &cfmt.indent, FORMAT_U, 0},
	{"infofont", &cfmt.infofont, FORMAT_F, 0},
	{"infoline", &cfmt.infoline, FORMAT_B, 0},
	{"infospace", &cfmt.infospace, FORMAT_U, 0},
	{"landscape", &cfmt.landscape, FORMAT_B, 0},
	{"leftmargin", &cfmt.leftmargin, FORMAT_U, 0},
	{"lineskipfac", &cfmt.lineskipfac, FORMAT_R, 0},
	{"maxshrink", &cfmt.maxshrink, FORMAT_R, 0},
	{"maxstaffsep", &cfmt.maxstaffsep, FORMAT_U, 0},
	{"maxsysstaffsep", &cfmt.maxsysstaffsep, FORMAT_U, 0},
	{"measurebox", &cfmt.measurebox, FORMAT_B, 0},
	{"measurefirst", &cfmt.measurefirst, FORMAT_I, 2},
	{"measurefont", &cfmt.measurefont, FORMAT_F, 2},
	{"measurenb", &cfmt.measurenb, FORMAT_I, 0},
	{"musiconly", &cfmt.musiconly, FORMAT_B, 0},
	{"musicspace", &cfmt.musicspace, FORMAT_U, 0},
	{"notespacingfactor", &cfmt.notespacingfactor, FORMAT_R, 1},
	{"oneperpage", &cfmt.oneperpage, FORMAT_B, 0},
	{"pageheight", &cfmt.pageheight, FORMAT_U, 0},
	{"pagewidth", &cfmt.pagewidth, FORMAT_U, 0},
	{"parskipfac", &cfmt.parskipfac, FORMAT_R, 0},
	{"partsbox", &cfmt.partsbox, FORMAT_B, 0},
	{"partsfont", &cfmt.partsfont, FORMAT_F, 1},
	{"partsspace", &cfmt.partsspace, FORMAT_U, 0},
	{"printparts", &cfmt.printparts, FORMAT_B, 0},
	{"printtempo", &cfmt.printtempo, FORMAT_B, 0},
	{"pslevel", &cfmt.pslevel, FORMAT_I, 0},
	{"repeatfont", &cfmt.repeatfont, FORMAT_F, 0},
	{"rightmargin", &cfmt.rightmargin, FORMAT_U, 0},
	{"scale", &cfmt.scale, FORMAT_R, 0},
	{"setdefl", &cfmt.setdefl, FORMAT_B, 0},
	{"setfont-1", &cfmt.font_tb[1], FORMAT_F, 0},
	{"setfont-2", &cfmt.font_tb[2], FORMAT_F, 0},
	{"setfont-3", &cfmt.font_tb[3], FORMAT_F, 0},
	{"setfont-4", &cfmt.font_tb[4], FORMAT_F, 0},
#if DFONT_MIN!= 5
#	error Bad start of dynamic fonts
#endif
	{"shifthnote", &cfmt.shifthnote, FORMAT_B, 0},
	{"slurheight", &cfmt.slurheight, FORMAT_R, 0},
	{"splittune", &cfmt.splittune, FORMAT_B, 0},
	{"squarebreve", &cfmt.squarebreve, FORMAT_B, 0},
	{"staffsep", &cfmt.staffsep, FORMAT_U, 0},
	{"staffwidth", &staffwidth, FORMAT_U, 1},
	{"stemheight", &cfmt.stemheight, FORMAT_R, 0},
	{"straightflags", &cfmt.straightflags, FORMAT_B, 0},
	{"stretchlast", &cfmt.stretchlast, FORMAT_B, 0},
	{"stretchstaff", &cfmt.stretchstaff, FORMAT_B, 0},
	{"subtitlefont", &cfmt.subtitlefont, FORMAT_F, 0},
	{"subtitlespace", &cfmt.subtitlespace, FORMAT_U, 0},
	{"sysstaffsep", &cfmt.sysstaffsep, FORMAT_U, 0},
	{"tempofont", &cfmt.tempofont, FORMAT_F, 0},
	{"textfont", &cfmt.textfont, FORMAT_F, 0},
	{"textoption", &cfmt.textoption, FORMAT_I, 4},
	{"textspace", &cfmt.textspace, FORMAT_U, 0},
	{"titlecaps", &cfmt.titlecaps, FORMAT_B, 0},
	{"titlefont", &cfmt.titlefont, FORMAT_F, 0},
	{"titleformat", &cfmt.titleformat, FORMAT_S, 0},
	{"titleleft", &cfmt.titleleft, FORMAT_B, 0},
	{"titlespace", &cfmt.titlespace, FORMAT_U, 0},
	{"timewarn", &cfmt.timewarn, FORMAT_B, 0},
	{"topmargin", &cfmt.topmargin, FORMAT_U, 0},
	{"topspace", &cfmt.topspace, FORMAT_U, 0},
	{"tuplets", &cfmt.tuplets, FORMAT_I, 3},
	{"vocalabove", &cfmt.vocalabove, FORMAT_B, 0},
	{"vocalfont", &cfmt.vocalfont, FORMAT_F, 0},
	{"vocalspace", &cfmt.vocalspace, FORMAT_U, 0},
	{"voicefont", &cfmt.voicefont, FORMAT_F, 0},
	{"withxrefs", &cfmt.withxrefs, FORMAT_B, 0},
	{"wordsfont", &cfmt.wordsfont, FORMAT_F, 0},
	{"wordsspace", &cfmt.wordsspace, FORMAT_U, 0},
	{"writehistory", &cfmt.writehistory, FORMAT_B, 0},
	{0, 0, 0, 0}		/* end of table */
};

static char *enc_name[20] = {
	"us-ascii", 	/* 0 */
	"iso-8859-1", 	/* 1 */
	"iso-8859-2", 	/* 2 */
	"iso-8859-3", 	/* 3 */
	"iso-8859-4", 	/* 4 */
	"iso-8859-9",	/* 5 (Latin5) */
	"iso-8859-10",	/* 6 (Latin6) */
	"native",	/* 7 = ENC_NATIVE */
#if 0
	"utf-8",	/* 8 */
#endif
};

/* -- add a font -- */
static int add_font(char *fname, int encoding)
{
	int fnum;

	for (fnum = nfontnames; --fnum >= 0; )
		if (strcmp(fname, fontnames[fnum]) == 0
		    && encoding == font_enc[fnum])
			return fnum;		/* already here */

	if (nfontnames >= MAXFONTS) {
		error(1, 0, "Too many fonts");
		return 0;
	}
	if (file_initialized)
		error(1, 0,
		      "Cannot have a new font when the output file is opened");
	fnum = nfontnames++;
	fontnames[fnum] = strdup(fname);
	font_enc[fnum] = encoding;
	return fnum;
}

/* -- set a dynamic font -- */
static int dfont_set(struct FONTSPEC *f)
{
	int i;

	for (i = DFONT_MIN; i < cfmt.ndfont; i++) {
		if (cfmt.font_tb[i].fnum == f->fnum
		    && cfmt.font_tb[i].size == f->size)
			return i;
	}
	if (i >= DFONT_MAX - 1) {
		error(1, 0, "Too many dynamic fonts");
		return DFONT_MAX - 1;
	}
	memcpy(&cfmt.font_tb[i], f, sizeof cfmt.font_tb[0]);
	cfmt.ndfont = i + 1;
	return i;
}

/* -- define a font -- */
static void fontspec(struct FONTSPEC *f,
		     char *name,
		     int encoding,
		     float size)
{
	if (name != 0)
		f->fnum = add_font(name, encoding);
	else	name = fontnames[f->fnum];
	f->size = size;
	f->swfac = size;
	if (strncmp(name, "Times", 5) == 0) {
		if (strcmp(name, "Times-Bold") == 0)
			f->swfac *= 1.05;
	} else if (strcmp(name, "Helvetica-Bold") == 0)
		f->swfac *= 1.15;
	else if (strncmp(name, "Helvetica", 9) == 0
		 || strncmp(name, "Palatino", 8) == 0)
		f->swfac *= 1.10;
	else if (strncmp(name, "Courier", 7) == 0)
		f->swfac *= 1.35;
	else	f->swfac *= 1.2;		/* unknown font */
	if (f == &cfmt.gchordfont)
		cfmt.gcf = dfont_set(f);
	else if (f == &cfmt.annotationfont)
		cfmt.anf = dfont_set(f);
	else if (f == &cfmt.vocalfont)
		cfmt.vof = dfont_set(f);
}

/* -- output the font definitions with their encodings -- */
void define_fonts(void)
{
	int i, enc;

	enc = 0;
	for (i = 0; i < nfontnames; i++) {
		if (used_font[i])
			enc |= 1 << font_enc[i];
	}
	for (i = 0; ; i++) {
		if (enc == 0)
			break;
		if (enc & 1)
			define_encoding(i, enc_name[i]);
		enc >>= 1;
	}
	for (i = 0; i < nfontnames; i++) {
		if (used_font[i])
			define_font(fontnames[i], i, font_enc[i]);
	}
}

/* -- mark the used fonts -- */
void make_font_list(void)
{
	struct FORMAT *f;

	f = &cfmt;
	used_font[f->annotationfont.fnum] = 1;
	used_font[f->composerfont.fnum] = 1;
	used_font[f->footerfont.fnum] = 1;
	used_font[f->gchordfont.fnum] = 1;
	used_font[f->headerfont.fnum] = 1;
	used_font[f->historyfont.fnum] = 1;
	used_font[f->infofont.fnum] = 1;
	used_font[f->measurefont.fnum] = 1;
	used_font[f->partsfont.fnum] = 1;
	used_font[f->repeatfont.fnum] = 1;
	used_font[f->subtitlefont.fnum] = 1;
	used_font[f->tempofont.fnum] = 1;
	used_font[f->textfont.fnum] = 1;
	used_font[f->titlefont.fnum] = 1;
	used_font[f->vocalfont.fnum] = 1;
	used_font[f->voicefont.fnum] = 1;
	used_font[f->wordsfont.fnum] = 1;
}

/* -- set the default format -- */
void set_format(void)
{
	struct FORMAT *f;

	f = &cfmt;
	memset(f, 0, sizeof *f);
	f->ndfont = DFONT_MIN;
	f->pageheight = PAGEHEIGHT;
	f->pagewidth = PAGEWIDTH;
	f->leftmargin = MARGIN;
	f->rightmargin = MARGIN;
	f->topmargin = 1.0 CM;
	f->botmargin = 1.0 CM;
	f->topspace = 0.8 CM;
	f->titlespace = 0.2 CM;
	f->subtitlespace = 0.1 CM;
	f->composerspace = 0.2 CM;
	f->musicspace = 0.2 CM;
	f->partsspace = 0.3 CM;
	f->staffsep = 46.0 PT;
	f->sysstaffsep = 34.0 PT;
	f->maxstaffsep = 2000.0 PT;
	f->maxsysstaffsep = 2000.0 PT;
	f->vocalspace = 23.0 PT;
	f->textspace = 0.5 CM;
	f->scale = 0.75;
	f->slurheight = 1.0;
	f->maxshrink = 0.65;
	f->stretchstaff = 1;
	f->graceslurs = 1;
	f->lineskipfac = 1.1;
	f->parskipfac = 0.4;
	f->measurenb = -1;
	f->measurefirst = 1;
	f->printparts = 1;
	f->printtempo = 1;
	f->autoclef = 1;
	f->aligncomposer = A_RIGHT;
	f->dynalign = 1;
	f->notespacingfactor = 1.414;
	f->pslevel = 2;
	f->stemheight = STEM;
	f->dateformat = strdup("\\%b \\%e, \\%Y \\%H:\\%M");
	f->textoption = OBEYLINES;
	fontspec(&f->annotationfont, "Helvetica", 0, 12.0);
	fontspec(&f->composerfont, "Times-Italic", 0, 14.0);
	fontspec(&f->footerfont, "Times-Roman", 0, 12.0); /* not scaled */
	fontspec(&f->gchordfont, "Helvetica", 0, 12.0);
	fontspec(&f->headerfont, "Times-Roman", 0, 12.0); /* not scaled */
	fontspec(&f->historyfont, "Times-Roman", 0, 16.0);
	fontspec(&f->infofont,	"Times-Italic", 0, 14.0); /* same as composer by default */
	fontspec(&f->measurefont, "Times-Italic", 0, 14.0);
	fontspec(&f->partsfont, "Times-Roman", 0, 15.0);
	fontspec(&f->repeatfont, "Times-Roman", 0, 13.0);
	fontspec(&f->subtitlefont, "Times-Roman", 0, 16.0);
	fontspec(&f->tempofont, "Times-Bold", 0, 15.0);
	fontspec(&f->textfont,	"Times-Roman", 0, 16.0);
	fontspec(&f->titlefont, "Times-Roman", 0, 20.0);
	fontspec(&f->vocalfont, "Times-Bold", 0, 13.0);
	fontspec(&f->voicefont, "Times-Bold", 0, 13.0);
	fontspec(&f->wordsfont,	"Times-Roman", 0, 16.0);
}

/* -- print the current format -- */
void print_format(void)
{
	struct format *fd;
static char yn[2][5]={"no","yes"};

	for (fd = format_tb; fd->name; fd++) {
		printf("  %-13s ", fd->name);
		switch (fd->type) {
		case FORMAT_I:
			switch (fd->subtype) {
			default:
				printf("%d\n", *((int *) fd->v));
				break;
			case 1:			/* encoding */
				printf("%s\n", enc_name[*((int *) fd->v)]);
				break;
			case 3:			/* tuplets */
				printf("%d %d %d\n",
					cfmt.tuplets >> 8,
					(cfmt.tuplets >> 4) & 0x0f,
					cfmt.tuplets & 0x0f);
				break;
			}
			break;
		case FORMAT_R:
			printf("%.2f\n", *((float *) fd->v));
			break;
		case FORMAT_F: {
			struct FONTSPEC *s;

			s = (struct FONTSPEC *) fd->v;
			printf("%s", fontnames[s->fnum]);
			if (font_enc[s->fnum] != cfmt.encoding)
				printf(" %s",
				       enc_name[(unsigned) font_enc[s->fnum]]);
			printf(" %.1f", s->size);
			if ((fd->subtype == 1 && cfmt.partsbox)
			    || (fd->subtype == 2 && cfmt.measurebox)
			    || (fd->subtype == 3 && cfmt.gchordbox))
				printf(" box");
			printf("\n");
			break;
		}
		case FORMAT_U:
			if (fd->subtype == 0)
				printf("%.2fcm\n", *((float *) fd->v) / (1 CM));
			else	printf("%.2fcm\n",
					(cfmt.pagewidth
						- cfmt.leftmargin
						- cfmt.rightmargin)
					/ (1 CM));
			break;
		case FORMAT_B:
			printf("%s\n", yn[*((int *) fd->v)]);
			break;
		case FORMAT_S:
			printf("\"%s\"\n", *((char **) fd->v) != 0 ? *((char **) fd->v) : "");
			break;
		}
	}
}

/* -- get an encoding -- */
static int get_encoding(char *p)
{
	int l;
	char **e;

	l = 0;
	while (p[l] != '\0' && !isspace((unsigned char) p[l]))
		l++;
	if (strncasecmp(p, "ASCII", l) == 0)	/* backward compatibility */
		return 0;
	e = enc_name;
	for (;;) {
		if (strncmp(p, *e, l) == 0)
			break;
		e++;
		if (*e == 0
		    || e >= &enc_name[sizeof enc_name]) {
			if (e >= &enc_name[sizeof enc_name - 1]) {
				error(1, 0, "Too many encodings");
				return 0;
			}
			*e = (char *) malloc(l + 1);
			strncpy(*e, p, l);
			(*e)[l] = '\0';
		}
	}
	return e - enc_name;
}

/* -- get the option for text -- */
int get_textopt(char *p)
{
	int option;

	option = OBEYLINES;
	if (*p == '\0'
	    || strncmp(p, "obeylines", 9) == 0)
		;
	else if (strncmp(p, "align", 5) == 0
		 || strncmp(p, "justify", 7) == 0)
		option = T_JUSTIFY;
	else if (strncmp(p, "ragged", 6) == 0
		 || strncmp(p, "fill", 4) == 0)
		option = T_FILL;
	else if (strncmp(p, "center", 10) == 0)
		option = OBEYCENTER;
	else if (strncmp(p, "skip", 4) == 0)
		option = SKIP;
	else	option = -1;
	return option;
}

/* -- read a boolean value -- */
static int g_logv(char *p)
{
	switch (*p) {
	case 0:
	case '1':
	case 'y':
	case 'Y':
	case 't':
	case 'T':
		return 1;
	case '0':
	case 'n':
	case 'N':
	case 'f':
	case 'F':
		break;
	default:
		fprintf(stderr,
			"++++ Unknown logical '%s' - false assumed\n", p);
		break;
	}
	return 0;
}

/* --  read a float variable, no units -- */
static float g_fltv(char *p)
{
	return atof(p);
}

/* -- read a font specifier -- */
static void g_fspc(char *p,
		   struct FONTSPEC *fn)
{
	char fname[80];
	int encoding;
	float fsize;

	p = get_str(fname, p, sizeof fname);
	if (!isdigit((unsigned char) *p)) {
		if (*p == '*')
			encoding = font_enc[fn->fnum];
		else	encoding = get_encoding(p);
		while (*p != '\0' && !isspace((unsigned char) *p))
			p++;
		while (isspace((unsigned char) *p))
			p++;
	} else	encoding = cfmt.encoding;
	fsize = *p != '\0' ? g_fltv(p) : fn->size;
	fontspec(fn,
		 strcmp(fname, "*") != 0 ? fname : 0,
		 encoding,
		 fsize);
	if (!file_initialized)
		used_font[fn->fnum] = 1;
}

/* -- parse a format line -- */
void interpret_fmt_line(char *w,		/* keyword */
			char *p,		/* argument */
			int lock)
{
	struct format *fd;

	if (strcmp(w, "deco") == 0) {
		deco_add(p);
		return;
	}
	if (!strcmp(w, "font")) {
		int fnum, encoding;
		char fname[80];

		p = get_str(fname, p, sizeof fname);
		if (*p == 0)
			encoding = cfmt.encoding;
		else	encoding = get_encoding(p);
		fnum = add_font(fname, encoding);
		used_font[fnum] = 1;
		return;
	}
	if (strcmp(w, "format") == 0) {
		if (read_fmt_file(p) < 0)
			error(1, 0, "No such format file '%s'", p);
		return;
	}
	if (strcmp(w, "postscript") == 0) {
		if (!file_initialized)
			user_ps_add(p);
		return;
	}
	for (fd = format_tb; fd->name; fd++)
		if (strcmp(w, fd->name) == 0)
			break;
	if (fd->name == 0)
		return;
	if (fd->lock && !lock)
		return;
	fd->lock |= lock;
	switch (fd->type) {
	case FORMAT_I:
		if (fd->subtype == 3) {
			unsigned i1, i2, i3;

			if (sscanf(p, "%d %d %d", &i1, &i2, &i3) != 3
			    || i1 > 2 || i2 > 2 || i3 > 2) {
				error(1, 0,
				      "Bad 'tuplets' value '%s' - ignored",
				      p);
				return;
			}
			cfmt.tuplets = (i1 << 8) | (i2 << 4) | i3;
			break;
		}
		if (fd->subtype == 1 && !isdigit(*p))	/* 'encoding' */
			cfmt.encoding = get_encoding(p);
		else if (fd->subtype == 4 && !isdigit(*p)) /* 'textoption' */
			cfmt.textoption = get_textopt(p);
		else	sscanf(p, "%d", (int *) fd->v);
		switch (fd->subtype) {
		case 1:
			if (isdigit(*p)
			    && (unsigned) cfmt.encoding > MAXENC) {
				error(1, 0,
				      "Bad encoding value %d - reset to 0",
				      cfmt.encoding);
				cfmt.encoding = 0;
			} else {
				int i;

				for (i = 0; i < nfontnames; i++) {
					if (font_enc[i] == 0)
						font_enc[i] = cfmt.encoding;
				}
			}
			break;
		case 2:
			nbar = nbar_rep = cfmt.measurefirst;
			break;
		case 4:
			if (cfmt.textoption < 0) {
				error(1, 0,
				      "Bad 'textoption' value '%s'",
				      p);
				cfmt.textoption = OBEYLINES;
			}
			break;
		}
		break;
	case FORMAT_R:
		*((float *) fd->v) = g_fltv(p);
		if (fd->subtype == 1) {	/* note spacing factor */
			int i;
			float w;

			if (cfmt.notespacingfactor <= 0) {
				fprintf(stderr,
					"Bad value for 'notespacingfactor'\n");
				cfmt.notespacingfactor = 1;
				break;
			}
			dot_space = sqrt(cfmt.notespacingfactor);
			i = C_XFLAGS;		/* crotchet index */
			w = space_tb[i];
			for ( ; --i >= 0; ) {
				w /= cfmt.notespacingfactor;
				space_tb[i] = w;
			}
			i = C_XFLAGS;
			w = space_tb[i];
			for ( ; ++i < NFLAGS_SZ; ) {
				w *= cfmt.notespacingfactor;
				space_tb[i] = w;
			}
		}
		break;
	case FORMAT_F: {
		int b;

		g_fspc(p, (struct FONTSPEC *) fd->v);
		b = strstr(p, "box") != 0;
		switch (fd->subtype) {
		case 1:
			cfmt.partsbox = b;
			break;
		case 2:
			cfmt.measurebox = b;
			break;
		case 3:
			cfmt.gchordbox = b;
			break;
		}
		break;
	    }
	case FORMAT_U:
		*((float *) fd->v) = scan_u(p);
		if (fd->subtype == 1) {
			float rmargin;

			rmargin = (cfmt.landscape ? cfmt.pageheight : cfmt.pagewidth)
				- staffwidth - cfmt.leftmargin;
			if (rmargin < 0)
				fprintf(stderr, "'staffwidth' too big\n");
			cfmt.rightmargin = rmargin;
		}
		break;
	case FORMAT_B:
		*((int *) fd->v) = g_logv(p);
		break;
	case FORMAT_S: {
		int l;

		if (*((char **) fd->v) != 0)	/* !!no static allocation!! */
			free(*((char **) fd->v));
		l = strlen(p) + 1;
		*((char **) fd->v) = malloc(l);
		if (*p == '"')
			get_str(*((char **) fd->v), p, l);
		else	strcpy(*((char **) fd->v), p);
		break;
	    }
	}
}

/* -- remove the spaces and comments of a format line -- */
static char *clean_line(char *p)
{
	int i;
	char c, *q;

	while (isspace((unsigned char) *p))
		p++;
	q = p;
	i = 0;
	for (;;) {
		if ((c = *p++) == '%') {
			if (i > 0 && p[-2] != '\\')
				c = '\0';
		}
		if (c == '\0') {
			p--;
			break;
		}
		i++;
	}
	while (--i > 0) {
		c = *--p;
		if (!isspace((unsigned char) c)) {
			p[1] = '\0';
			break;
		}
	}
	return q;
}

/* -- lock a format -- */
void lock_fmt(void *fmt)
{
	struct format *fd;

	for (fd = format_tb; fd->name; fd++)
		if (fd->v == fmt)
			break;
	if (fd->name == 0)
		return;
	fd->lock = 1;
}

/* -- read a format file -- */
int read_fmt_file(char *filename)
{
	FILE *fp;
	char line[BSIZE], *p, *q;

	strcpy(line, filename);
	strext(line, "fmt");
	if ((fp = fopen(line, "r")) == 0) {
		if (*styd == '\0')
			return -1;
                sprintf(line, "%s%c%s", styd, DIRSEP, filename);
		strext(line, "fmt");
		if ((fp = fopen(line, "r")) == 0)
			return -1;
	}

	for (;;) {
		p = line;
		if (!fgets(p, sizeof line, fp))
			break;
		p = clean_line(p);
		if (*p == '\0')
			continue;
		if (strcmp(p, "beginps") == 0) {
			for (;;) {
				p = line;
				if (!fgets(p, sizeof line, fp))
					break;
#if 1
				p[strlen(p) - 1] = '\0';
#else
				p = clean_line(p);
#endif
				if (*p == '\0')
					continue;
				if (strcmp(p, "endps") == 0)
					break;
#if 1
				if (!file_initialized)
					user_ps_add(p);
#else
				interpret_fmt_line("postscript", p, 0);
#endif
			}
			continue;
		}
		if (strcmp(p, "end") == 0)
			break;
		q = p;
		while (*p != '\0' && !isspace((unsigned char) *p))
			p++;
		if (*p != '\0') {
			*p++ = '\0';
			while (isspace((unsigned char) *p))
				p++;
		}
		interpret_fmt_line(q, p, 0);
	}
	fclose(fp);
	return 0;
}

/* -- start a new font -- */
void set_font(struct FONTSPEC *font)
{
	int fnum;

	fnum = font->fnum;
	if (!used_font[fnum]) {
		error(1, 0,
		      "Font \"%s\" not predefined; using first in list",
		      fontnames[fnum]);
		fnum = 0;
	}
	if (font->size == 0)
		error(0, 0, "Font \"%s\" with a null size",
		      fontnames[fnum]);
	PUT2("%.1f F%d ", font->size, fnum);
}
