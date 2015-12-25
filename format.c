/*  
 *  This file is part of abc2ps, Copyright (C) 1996,1997 Michael Methfessel
 *  Modified for abcm2ps, Copyright (C) 1998-2001 Jean-François Moine
 */

#include <stdio.h>
#include <string.h>

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
	{"barsperstaff", FORMAT_I, 0, &cfmt.barsperstaff},
	{"botmargin", FORMAT_U, 0, &cfmt.botmargin},
	{"composerfont", FORMAT_F, 0, &cfmt.composerfont},
	{"composerspace", FORMAT_U, 0, &cfmt.composerspace},
	{"continueall", FORMAT_B, 0, &cfmt.continueall},
	{"encoding", FORMAT_I, 1, &cfmt.encoding},
	{"exprabove", FORMAT_B, 0, &cfmt.exprabove},
	{"exprbelow", FORMAT_B, 0, &cfmt.exprbelow},
	{"footer", FORMAT_S, 0, &cfmt.footer},
	{"freegchord", FORMAT_B, 0, &cfmt.freegchord},
	{"flatbeams", FORMAT_B, 0, &cfmt.flatbeams},
	{"gchordfont", FORMAT_F, 0, &cfmt.gchordfont},
	{"graceslurs", FORMAT_B, 0, &cfmt.graceslurs},
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
	{"measurenb", FORMAT_I, 0, &cfmt.measurenb},
	{"musiconly", FORMAT_B, 0, &cfmt.musiconly},
	{"musicspace", FORMAT_U, 0, &cfmt.musicspace},
	{"oneperpage", FORMAT_B, 0, &cfmt.oneperpage},
	{"pageheight", FORMAT_U, 0, &cfmt.pageheight},
	{"pagewidth", FORMAT_U, 0, &cfmt.pagewidth},
	{"parskipfac", FORMAT_R, 0, &cfmt.parskipfac},
	{"partsfont", FORMAT_F, 1, &cfmt.partsfont},
	{"partsspace", FORMAT_U, 0, &cfmt.partsspace},
	{"rightmargin", FORMAT_U, 0, &cfmt.rightmargin},
	{"scale", FORMAT_R, 0, &cfmt.scale},
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
		ERROR(("Too many fonts\n"));
		exit(1);
	}
	if (file_initialized)
		ERROR(("Cannot have a new font when the output file is opened"));
	fnum = nfontnames++;
	fontnames[fnum] = getarena(strlen(fname) + 1);
	strcpy(fontnames[fnum], fname);
#ifdef DEBUG
	if (verbose >= 10)
		printf("New font %s at %d\n", fname, fnum);
#endif
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
void define_fonts(FILE *fp)
{
	int i;

	for (i = 0; i < nfontnames; i++) {
		if (used_font[i])
			define_font(fp, fontnames[i], i, cfmt.encoding);
	}
}

/* -- make_font_list -- */
void make_font_list(void)
{
	struct FORMAT *f;

	f = &cfmt;
#ifdef DEBUG
	if (verbose >= 10)
		printf("Adding fonts from format..\n");
#endif
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
}

/* -- set_standard_format -- */
void set_standard_format(void)
{
	struct FORMAT *f;

	f = &cfmt;
	memset(f, 0, sizeof *f);
	f->name = "standard";
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
	f->sysstaffsep	= 36.0 * PT;
	f->vocalspace	= 23.0 * PT;
	f->textspace	= 0.5 * CM;
	f->scale	= 0.75;
	f->maxshrink	= 0.65;
	f->stretchstaff	= 1;
	f->graceslurs	= 1;
	f->lineskipfac	= 1.1;
	f->parskipfac	= 0.4;
	f->measurenb = -1;
	f->measurefirst = 1;
	fontspec(&f->titlefont,	"Times-Roman", 15.0);
	fontspec(&f->subtitlefont, "Times-Roman", 12.0);
	fontspec(&f->composerfont, "Times-Italic", 11.0);
	fontspec(&f->partsfont,	"Times-Roman", 15.0);
	fontspec(&f->tempofont,	"Times-Bold", 15.0);
	fontspec(&f->vocalfont,	"Times-Bold", 13.0);
	fontspec(&f->textfont,	"Times-Roman", 12.0);
	fontspec(&f->wordsfont,	"Times-Roman", 12.0);
	fontspec(&f->gchordfont, "Helvetica", 12.0);
	fontspec(&f->infofont, "Times-Italic", 11.0); /* same as composer by default */
}

/* -- set_pretty_format -- */
void set_pretty_format(void)
{
	struct FORMAT *f;

	set_standard_format();
	f = &cfmt;
	f->name = "pretty";
	f->titlespace 	= 0.4 * CM;
	f->composerspace = 0.25 * CM;
	f->musicspace	= 0.25 * CM;
	f->staffsep	= 50.0 * PT;
	f->sysstaffsep	= 40.0 * PT;
	f->scale	= 0.8;
	f->maxshrink	= 0.55;
	f->parskipfac	= 0.1;
	fontspec(&f->titlefont,	"Times-Roman", 18.0);
	fontspec(&f->subtitlefont,  "Times-Roman", 15.0);
	fontspec(&f->composerfont,  "Times-Italic", 12.0);
	fontspec(&f->partsfont,	"Times-Roman", 15.0);
	fontspec(&f->vocalfont,	"Times-Bold", 14.0);
	fontspec(&f->textfont,	"Times-Roman", 10.0);
	fontspec(&f->wordsfont,	"Times-Roman", 10.0);
	fontspec(&f->infofont, "Times-Italic", 12.0);
}

/* -- set_pretty2_format -- */
void set_pretty2_format(void)
{
	struct FORMAT *f;

	set_standard_format();
	f = &cfmt;
	f->name = "pretty2";
	f->titlespace	= 0.4 * CM;
	f->composerspace = 0.3 * CM;
	f->musicspace	= 0.25 * CM;
	f->partsspace	= 0.2 * CM;
	f->staffsep	= 55.0 * PT;
	f->sysstaffsep	= 45.0 * PT;
	f->textspace	= 0.2 * CM;
	f->maxshrink	= 0.55;
	f->titleleft	= 1;
	f->parskipfac	= 0.1;
	fontspec(&f->titlefont,	"Helvetica-Bold", 16.0);
	fontspec(&f->subtitlefont, "Helvetica-Bold", 13.0);
	fontspec(&f->composerfont, "Helvetica",	10.0);
	fontspec(&f->partsfont,	"Times-Roman", 17.0);
	fontspec(&f->textfont,	"Times-Roman", 10.0);
	fontspec(&f->wordsfont,	"Times-Roman", 10.0);
	fontspec(&f->infofont, "Helvetica", 10.0);
}

/* -- print the current format -- */
void print_format(void)
{
	struct format *fd;
static char yn[2][5]={"no","yes"};

	printf("Format \"%s\":\n", cfmt.name);
	for (fd = format_tb; fd->name; fd++) {
		printf("  %-13s ", fd->name);
		switch (fd->type) {
		case FORMAT_I:
			switch (fd->subtype) {
			case 0:
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
			if (fd->subtype == 0
			    || !cfmt.partsbox)
				printf("\n");
			else	printf(" box\n");
			break;
		}
		case FORMAT_U:
			if (fd->subtype == 0)
				printf("%.2fcm\n", *((float *) fd->v) / CM);
			else	printf("%.2fcm\n", (cfmt.pagewidth
						    - cfmt.leftmargin - cfmt.rightmargin)
					/ CM);
			break;
		case FORMAT_B:
			printf("%s\n", yn[*((int *) fd->v)]);
			break;
		case FORMAT_S:
			printf("%s\n", ((char *) fd->v));
			break;
		}
	}
}

/* -- read a logical variable -- */
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
		return 0;
	}
	printf("++++ Unknown logical in line: %s\n", l);
	exit(1);
}

/* --  read a float variable, no units -- */
static float g_fltv(char *l)
{
	float v;

	sscanf(l, "%f", &v);
	return v;
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
int interpret_format_line(char *p)
{
	struct format *fd;
	char w[81];

	if (*p == '\0'
	    || *p == '%')
		return 2;
	p = get_str(w, p, sizeof w);
#ifdef DEBUG
	if (verbose >= 6)
		printf("Interpret format line: %s\n", l);
#endif
	if (strcmp(w, "end") == 0)
		return 1;

	if (strcmp(w, "deco") == 0) {
		deco_add(p);
		return 2;
	}

	if (strcmp(w, "postscript") == 0) {
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
				if ((unsigned) cfmt.encoding > 6) {
					ERROR(("Bad encoding value %d - reset to 0",
					       cfmt.encoding));
					cfmt.encoding = 0;
				}
				break;
			case 2:
				nbar = cfmt.measurefirst;
				break;
			}
			break;
		case FORMAT_R:
			*((float *) fd->v) = g_fltv(p);
			break;
		case FORMAT_F:
			g_fspc(p, (struct FONTSPEC *) fd->v);
			if (fd->subtype == 1)
				cfmt.partsbox = strstr(p, "box") != 0;
			break;
		case FORMAT_U:
			*((float *) fd->v) = scan_u(p);
			if (fd->subtype == 1) {
				float rmargin;

				rmargin = (cfmt.landscape ? cfmt.pageheight : cfmt.pagewidth)
					- staffwidth - cfmt.leftmargin;
				if (rmargin < 0)
					printf("'staffwidth' too big\n");
				cfmt.rightmargin = rmargin;
			}
			break;
		case FORMAT_B:
			*((int *) fd->v) = g_logv(p);
			break;
		case FORMAT_S:
			*((char **) fd->v) = getarena(strlen(p) + 1);
			strcpy(*((char **) fd->v), p);
			break;
		}
		return 0;
	}

	if (!strcmp(w, "font")) {
		int fnum;

		get_str(w, p, sizeof w);
		fnum = add_font(w);
		used_font[fnum] = 1;
		return 0;
	}
#ifdef DEBUG
	if (verbose >= 5)
		printf("Ignore format line: %s\n", l);
#endif
	return 2;
}

/* -- read a format file -- */
int read_fmt_file(char *filename,
		  char *dirname)
{
	FILE *fp;
	char fname[201];

	strcpy(fname, filename);
	if ((fp = fopen(fname, "r")) == 0) {

		if (*dirname == '\0')
			return -1;
                sprintf(fname, "%s%c%s", dirname, DIRSEP, filename);
		if ((fp = fopen(fname, "r")) == 0)
			return -1;
	}

#ifdef DEBUG
	if (verbose >= 4)
		printf("Reading format file %s:\n", fname);
#endif
	for (;;) {
		char line[BSIZE];

		if (!fgets(line, BSIZE, fp))
			break;
		line[strlen(line) - 1] = '\0';	/* remove '\n' */
		if (interpret_format_line(line) == 1)
			break;
	}
	fclose(fp);
	return 0;
}

/* -- set_font -- */
void set_font(struct FONTSPEC *font)
{
	int fnum;

	fnum = font->fnum;
	if (!used_font[fnum]) {
		ERROR(("Font \"%s\" not predefined; using first in list",
			fontnames[fnum]));
		fnum = 0;
	};
	PUT2("%.1f F%d ", font->size, fnum);
}
