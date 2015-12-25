/*
 * Formatting functions.
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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "abcparse.h"
#include "abc2ps.h"

struct FORMAT cfmt;		/* current format for output */

static char *fontnames[MAXFONTS];	/* list of font names */
static char used_font[MAXFONTS];	/* used fonts */
static int nfontnames;
static float staffwidth;

/* format table */
static struct format {
	char *name;
	short type;
#define FORMAT_I 0	/* int */
#define FORMAT_R 1	/* float */
#define FORMAT_F 2	/* font spec */
#define FORMAT_U 3	/* float with unit */
#define FORMAT_B 4	/* boolean */
#define FORMAT_S 5	/* string */
	short subtype;		/* special cases - see code */
	void *v;
} format_tb[] = {
	{"autoclef", FORMAT_B, 0, &cfmt.autoclef},
	{"barsperstaff", FORMAT_I, 0, &cfmt.barsperstaff},
	{"botmargin", FORMAT_U, 0, &cfmt.botmargin},
	{"composerfont", FORMAT_F, 0, &cfmt.composerfont},
	{"composerspace", FORMAT_U, 0, &cfmt.composerspace},
	{"continueall", FORMAT_B, 0, &cfmt.continueall},
	{"encoding", FORMAT_I, 1, &cfmt.encoding},
	{"exprabove", FORMAT_B, 0, &cfmt.exprabove},
	{"exprbelow", FORMAT_B, 0, &cfmt.exprbelow},
	{"footer", FORMAT_S, 0, &cfmt.footer},
	{"footerfont", FORMAT_F, 0, &cfmt.footerfont},
	{"freegchord", FORMAT_B, 0, &cfmt.freegchord},
	{"flatbeams", FORMAT_B, 0, &cfmt.flatbeams},
	{"gchordbox", FORMAT_B, 0, &cfmt.gchordbox},
	{"gchordfont", FORMAT_F, 3, &cfmt.gchordfont},
	{"graceslurs", FORMAT_B, 0, &cfmt.graceslurs},
	{"header", FORMAT_S, 0, &cfmt.header},
	{"headerfont", FORMAT_F, 0, &cfmt.headerfont},
	{"indent", FORMAT_U, 0, &cfmt.indent},
	{"infofont", FORMAT_F, 0, &cfmt.infofont},
	{"infoline", FORMAT_B, 0, &cfmt.infoline},
	{"infospace", FORMAT_U, 0, &cfmt.infospace},
	{"landscape", FORMAT_B, 0, &cfmt.landscape},
	{"leftmargin", FORMAT_U, 0, &cfmt.leftmargin},
	{"lineskipfac", FORMAT_R, 0, &cfmt.lineskipfac},
	{"maxshrink", FORMAT_R, 0, &cfmt.maxshrink},
	{"measurebox", FORMAT_B, 0, &cfmt.measurebox},
	{"measurefirst", FORMAT_I, 2, &cfmt.measurefirst},
	{"measurefont", FORMAT_F, 2, &cfmt.measurefont},
	{"measurenb", FORMAT_I, 0, &cfmt.measurenb},
	{"musiconly", FORMAT_B, 0, &cfmt.musiconly},
	{"musicspace", FORMAT_U, 0, &cfmt.musicspace},
	{"notespacingfactor", FORMAT_R, 1, &cfmt.notespacingfactor},
	{"oneperpage", FORMAT_B, 0, &cfmt.oneperpage},
	{"pageheight", FORMAT_U, 0, &cfmt.pageheight},
	{"pagewidth", FORMAT_U, 0, &cfmt.pagewidth},
	{"parskipfac", FORMAT_R, 0, &cfmt.parskipfac},
	{"partsbox", FORMAT_B, 0, &cfmt.partsbox},
	{"partsfont", FORMAT_F, 1, &cfmt.partsfont},
	{"partsspace", FORMAT_U, 0, &cfmt.partsspace},
	{"printparts", FORMAT_B, 0, &cfmt.printparts},
	{"printtempo", FORMAT_B, 0, &cfmt.printtempo},
	{"repeatfont", FORMAT_F, 0, &cfmt.repeatfont},
	{"rightmargin", FORMAT_U, 0, &cfmt.rightmargin},
	{"scale", FORMAT_R, 0, &cfmt.scale},
	{"slurheight", FORMAT_R, 0, &cfmt.slurheight},
	{"splittune", FORMAT_B, 0, &cfmt.splittune},
	{"squarebreve", FORMAT_B, 0, &cfmt.squarebreve},
	{"staffsep", FORMAT_U, 0, &cfmt.staffsep},
	{"staffwidth", FORMAT_U, 1, &staffwidth},
	{"straightflags", FORMAT_B, 0, &cfmt.straightflags},
	{"stretchlast", FORMAT_B, 0, &cfmt.stretchlast},
	{"stretchstaff", FORMAT_B, 0, &cfmt.stretchstaff},
	{"subtitlefont", FORMAT_F, 0, &cfmt.subtitlefont},
	{"subtitlespace", FORMAT_U, 0, &cfmt.subtitlespace},
	{"sysstaffsep", FORMAT_U, 0, &cfmt.sysstaffsep},
	{"tempofont", FORMAT_F, 0, &cfmt.tempofont},
	{"textfont", FORMAT_F, 0, &cfmt.textfont},
	{"textspace", FORMAT_U, 0, &cfmt.textspace},
	{"titlecaps", FORMAT_B, 0, &cfmt.titlecaps},
	{"titlefont", FORMAT_F, 0, &cfmt.titlefont},
	{"titleleft", FORMAT_B, 0, &cfmt.titleleft},
	{"titlespace", FORMAT_U, 0, &cfmt.titlespace},
	{"topmargin", FORMAT_U, 0, &cfmt.topmargin},
	{"topspace", FORMAT_U, 0, &cfmt.topspace},
	{"vocalabove", FORMAT_B, 0, &cfmt.vocalabove},
	{"vocalfont", FORMAT_F, 0, &cfmt.vocalfont},
	{"vocalspace", FORMAT_U, 0, &cfmt.vocalspace},
	{"withxrefs", FORMAT_B, 0, &cfmt.withxrefs},
	{"wordsfont", FORMAT_F, 0, &cfmt.wordsfont},
	{"wordsspace", FORMAT_U, 0, &cfmt.wordsspace},
	{"writehistory", FORMAT_B, 0, &cfmt.writehistory},
	{0, 0, 0, 0}		/* end of table */
};

/*  subroutines connected with page layout  */

/* -- add a font -- */
static int add_font(char *fname)
{
	int fnum;

	for (fnum = nfontnames; --fnum >= 0; )
		if (strcmp(fname, fontnames[fnum]) == 0)
			return fnum;		/* already there */

	if (nfontnames >= MAXFONTS) {
		error(1, 0, "Too many fonts\n");
		exit(1);
	}
	if (file_initialized)
		error(1, 0,
		      "Cannot have a new font when the output file is opened");
	fnum = nfontnames++;
	fontnames[fnum] = strdup(fname);
	strcpy(fontnames[fnum], fname);
	return fnum;
}

/* -- fontspec -- */
static void fontspec(struct FONTSPEC *f,
		     char *name,
		     float size)
{
	if (name != 0)
		f->fnum = add_font(name);
	else	name = fontnames[f->fnum];
	f->size = size;
	f->swfac = 1.0;
	if (strcmp(name, "Times-Bold") == 0)
		f->swfac = 1.05;
	else if (strcmp(name, "Helvetica-Bold") == 0)
		f->swfac = 1.15;
	else if (strstr(name, "Helvetica")
		 || strstr(name,"Palatino"))
		f->swfac = 1.10;
}

/* -- output the font definitions -- */
void define_fonts(void)
{
	int i;

	for (i = 0; i < nfontnames; i++) {
		if (used_font[i])
			define_font(fontnames[i], i);
	}
}

/* -- mark the used fonts -- */
void make_font_list(void)
{
	struct FORMAT *f;

	f = &cfmt;
	used_font[f->titlefont.fnum] = 1;
	used_font[f->subtitlefont.fnum] = 1;
	used_font[f->composerfont.fnum] = 1;
	used_font[f->partsfont.fnum] = 1;
	used_font[f->vocalfont.fnum] = 1;
	used_font[f->textfont.fnum] = 1;
	used_font[f->tempofont.fnum] = 1;
	used_font[f->wordsfont.fnum] = 1;
	used_font[f->gchordfont.fnum] = 1;
	used_font[f->infofont.fnum] = 1;
	used_font[f->footerfont.fnum] = 1;
	used_font[f->headerfont.fnum] = 1;
	used_font[f->repeatfont.fnum] = 1;
	used_font[f->measurefont.fnum] = 1;
}

/* -- set the default format -- */
void set_format(void)
{
	struct FORMAT *f;

	f = &cfmt;
	memset(f, 0, sizeof *f);
	f->pageheight	= PAGEHEIGHT;
	f->pagewidth	= PAGEWIDTH;
	f->leftmargin	= MARGIN;
	f->rightmargin	= MARGIN;
	f->topmargin	= 1.0 * CM;
	f->botmargin	= 1.0 * CM;
	f->topspace	= 0.8 * CM;
	f->titlespace 	= 0.2 * CM;
	f->subtitlespace = 0.1 * CM;
	f->composerspace = 0.2 * CM;
	f->musicspace	= 0.2 * CM;
	f->partsspace	= 0.3 * CM;
	f->staffsep	= 46.0 * PT;
	f->sysstaffsep	= 34.0 * PT;
	f->vocalspace	= 23.0 * PT;
	f->textspace	= 0.5 * CM;
	f->scale	= 0.75;
	f->slurheight	= 1.0;
	f->maxshrink	= 0.65;
	f->stretchstaff	= 1;
	f->graceslurs	= 1;
	f->lineskipfac	= 1.1;
	f->parskipfac	= 0.4;
	f->measurenb = -1;
	f->measurefirst = 1;
	f->printparts = 1;
	f->printtempo = 1;
	f->autoclef = 1;
	f->notespacingfactor = 1.414;
	fontspec(&f->titlefont,	"Times-Roman", 20.0);
	fontspec(&f->subtitlefont, "Times-Roman", 16.0);
	fontspec(&f->composerfont, "Times-Italic", 14.0);
	fontspec(&f->partsfont,	"Times-Roman", 15.0);
	fontspec(&f->tempofont,	"Times-Bold", 15.0);
	fontspec(&f->vocalfont,	"Times-Bold", 13.0);
	fontspec(&f->textfont,	"Times-Roman", 16.0);
	fontspec(&f->wordsfont,	"Times-Roman", 16.0);
	fontspec(&f->gchordfont, "Helvetica", 12.0);
	fontspec(&f->infofont,	"Times-Italic", 14.0); /* same as composer by default */
	fontspec(&f->footerfont, "Times-Roman", 12.0);	/* not scaled */
	fontspec(&f->headerfont, "Times-Roman", 12.0);	/* not scaled */
	fontspec(&f->repeatfont, "Times-Roman", 13.0);
	fontspec(&f->measurefont, "Times-Italic", 14.0);
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
			case 1:
				if (*((int *) fd->v) != 0)
					printf("ISOLatin%d\n", *((int *) fd->v));
				else	printf("ASCII\n");
				break;
			}
			break;
		case FORMAT_R:
			printf("%.2f\n", *((float *) fd->v));
			break;
		case FORMAT_F: {
			struct FONTSPEC *s;

			s = (struct FONTSPEC *) fd->v;
			printf("%s %.1f", fontnames[s->fnum], s->size);
			if ((fd->subtype == 1 && cfmt.partsbox)
			    || (fd->subtype == 2 && cfmt.measurebox)
			    || (fd->subtype == 3 && cfmt.gchordbox))
				printf(" box");
			printf("\n");
			break;
		}
		case FORMAT_U:
			if (fd->subtype == 0)
				printf("%.2fcm\n", *((float *) fd->v) / CM);
			else	printf("%.2fcm\n",
					(cfmt.pagewidth
						- cfmt.leftmargin
						- cfmt.rightmargin)
					/ CM);
			break;
		case FORMAT_B:
			printf("%s\n", yn[*((int *) fd->v)]);
			break;
		case FORMAT_S:
			if ((char *) fd->v != 0)
				printf("\"%s\"\n", (char *) fd->v);
			else	printf("\"\"\n");
			break;
		}
	}
}

/* -- read a boolean value -- */
static int g_logv(char *l)
{
	switch (*l) {
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
			"++++ Unknown logical '%s' - false assumed\n", l);
		break;
	}
	return 0;
}

/* --  read a float variable, no units -- */
static float g_fltv(char *l)
{
	return atof(l);
}

/* -- read a font specifier -- */
static void g_fspc(char *p,
		   struct FONTSPEC *fn)
{
	char fname[STRLFMT];
	float fsize;

	fsize = fn->size;
	p = get_str(fname, p, sizeof fname);
	if (*p != '\0')
		fsize = g_fltv(p);
	fontspec(fn,
		 strcmp(fname, "*") != 0 ? fname : 0,
		 fsize);
	if (!file_initialized)
		used_font[fn->fnum] = 1;
}

/* -- parse a format line -- */
/* return:
 *	0: format modified
 *	1: 'end' found
 *	2: format not modified */
int interpret_format_line(char *w,		/* keyword */
			  char *p)		/* argument */
{
	struct format *fd;

	if (*w == '\0'
	    || *w == '%')
		return 2;
	if (strcmp(w, "end") == 0)
		return 1;

	if (strcmp(w, "deco") == 0) {
		deco_add(p);
		return 2;
	}

	if (strcmp(w, "postscript") == 0) {
		if (!file_initialized)
			add_text(p, TEXT_PS);
		return 2;
	}

	for (fd = format_tb; fd->name; fd++)
		if (strcmp(w, fd->name) == 0)
			break;
	if (fd->name) {
		switch (fd->type) {
		case FORMAT_I:
			sscanf(p, "%d", (int *) fd->v);
			switch (fd->subtype) {
			case 1:
				if ((unsigned) cfmt.encoding > MAXENC) {
					error(1, 0,
					      "Bad encoding value %d - reset to 0",
					      cfmt.encoding);
					cfmt.encoding = 0;
				}
				break;
			case 2:
				nbar = nbar_rep = cfmt.measurefirst;
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
					break;
				}
				dot_space = sqrt(cfmt.notespacingfactor);
				w = space_tb[SPACETB_SZ/2];
				for (i = SPACETB_SZ/2; --i >= 0; ) {
					w /= cfmt.notespacingfactor;
					space_tb[i] = w;
				}
				w = space_tb[SPACETB_SZ/2];
				for (i = SPACETB_SZ/2; ++i < SPACETB_SZ; ) {
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

			l = strlen(p) + 1;
			*((char **) fd->v) = malloc(l);
			if (*p == '"')
				get_str(*((char **) fd->v), p, l);
			else	strcpy(*((char **) fd->v), p);
			break;
		    }
		}
		return 0;
	}

	if (!strcmp(w, "font")) {
		int fnum;

		fnum = add_font(p);
		used_font[fnum] = 1;
		return 0;
	}
	return 2;
}

/* -- read a format file -- */
int read_fmt_file(char *filename,
		  char *dirname)
{
	FILE *fp;
	char fname[256];

	strcpy(fname, filename);
	strext(fname, "fmt");
	if ((fp = fopen(fname, "r")) == 0) {
		if (*dirname == '\0')
			return -1;
                sprintf(fname, "%s%c%s", dirname, DIRSEP, filename);
		if ((fp = fopen(fname, "r")) == 0)
			return -1;
	}

	for (;;) {
		char line[BSIZE], *p, *q;

		if (!fgets(line, sizeof line, fp))
			break;
		line[strlen(line) - 1] = '\0';	/* remove '\n' */
		q = line;
		while (isspace(*q))
			q++;
		p = q;
		while (*p != '\0' && !isspace(*p))
			p++;
		if (*p != '\0')
			*p++ = '\0';
		while (isspace(*p))
			p++;
		if (interpret_format_line(q, p) == 1)
			break;
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
	PUT2("%.1f F%d ", font->size, fnum);
}
