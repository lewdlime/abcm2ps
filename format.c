/*  
 *  This file is part of abc2ps, Copyright (C) 1996,1997 Michael Methfessel
 *  Modified for abcm2ps, Copyright (C) 1998,1999 Jean-François Moine
 *  See file abc2ps.c for details.
 */

#include <stdio.h>
#include <math.h>
#include <time.h>
#include <string.h>

#include "abcparse.h"
#include "abc2ps.h"

struct FORMAT cfmt;		/* current format for output */

/* format table */
struct format {
	char *name;
	short type;
#define FORMAT_I 0	/* int */
#define FORMAT_F 1	/* float */
#define FORMAT_S 2	/* font spec */
#define FORMAT_U 3	/* float with unit */
#define FORMAT_B 4	/* boolean */
	short subtype;
	void *v;
} format_tb[] = {
	{"pageheight", FORMAT_U, 0, &cfmt.pageheight},
	{"staffwidth", FORMAT_U, 0, &cfmt.staffwidth},
	{"topmargin", FORMAT_U, 0, &cfmt.topmargin},
	{"botmargin", FORMAT_U, 0, &cfmt.botmargin},
	{"leftmargin", FORMAT_U, 0, &cfmt.leftmargin},
	{"topspace", FORMAT_U, 0, &cfmt.topspace},
	{"wordsspace", FORMAT_U, 0, &cfmt.wordsspace},
	{"titlespace", FORMAT_U, 0, &cfmt.titlespace},
	{"subtitlespace", FORMAT_U, 0, &cfmt.subtitlespace},
	{"composerspace", FORMAT_U, 0, &cfmt.composerspace},
	{"musicspace", FORMAT_U, 0, &cfmt.musicspace},
	{"partsspace", FORMAT_U, 0, &cfmt.partsspace},
	{"staffsep", FORMAT_U, 0, &cfmt.staffsep},
	{"vocalspace", FORMAT_U, 0, &cfmt.vocalspace},
	{"textspace", FORMAT_U, 0, &cfmt.textspace},
	{"indent", FORMAT_U, 0, &cfmt.indent},
	{"scale", FORMAT_F, 0, &cfmt.scale},
	{"maxshrink", FORMAT_F, 0, &cfmt.maxshrink},
	{"lineskipfac", FORMAT_F, 0, &cfmt.lineskipfac},
	{"parskipfac", FORMAT_F, 0, &cfmt.parskipfac},
	{"barsperstaff", FORMAT_I, 0, &cfmt.barsperstaff},
	{"measurenb", FORMAT_I, 0, &cfmt.measurenb},
	{"measurebox", FORMAT_I, 0, &cfmt.measurebox},
	{"measurefirst", FORMAT_I, 0, &cfmt.measurefirst},
	{"encoding", FORMAT_I, 1, &cfmt.encoding},
	{"titleleft", FORMAT_B, 0, &cfmt.titleleft},
	{"titlecaps", FORMAT_B, 0, &cfmt.titlecaps},
	{"landscape", FORMAT_B, 0, &cfmt.landscape},
	{"musiconly", FORMAT_B, 0, &cfmt.musiconly},
	{"stretchstaff", FORMAT_B, 0, &cfmt.stretchstaff},
	{"stretchlast", FORMAT_B, 0, &cfmt.stretchlast},
	{"continueall", FORMAT_B, 0, &cfmt.continueall},
	{"writehistory", FORMAT_B, 0, &cfmt.writehistory},
	{"withxrefs", FORMAT_B, 0, &cfmt.withxrefs},
	{"oneperpage", FORMAT_B, 0, &cfmt.oneperpage},
	{"titlefont", FORMAT_S, 0, &cfmt.titlefont},
	{"subtitlefont", FORMAT_S, 0, &cfmt.subtitlefont},
	{"vocalfont", FORMAT_S, 0, &cfmt.vocalfont},
	{"partsfont", FORMAT_S, 1, &cfmt.partsfont},
	{"tempofont", FORMAT_S, 0, &cfmt.tempofont},
	{"textfont", FORMAT_S, 0, &cfmt.textfont},
	{"composerfont", FORMAT_S, 0, &cfmt.composerfont},
	{"wordsfont", FORMAT_S, 0, &cfmt.wordsfont},
	{"gchordfont", FORMAT_S, 0, &cfmt.gchordfont},
	{0, 0, 0, 0}		/* end of table */
};

/*  subroutines connected with page layout  */

/* -- fontspec -- */
static void fontspec(struct FONTSPEC *f,
		     char *name,
		     float size)
{
	strcpy(f->name, name);
	f->size = size;
	f->swfac = 1.0;
	if (strcmp(f->name, "Times-Bold") == 0)
		f->swfac = 1.05;
	else if (strcmp(f->name, "Helvetica-Bold") == 0)
		f->swfac = 1.15;
	else if (strstr(f->name, "Helvetica")
		 || strstr(f->name,"Palatino"))
		f->swfac = 1.10;
}

/* -- add_font -- */
/* checks font list, adds font if new */
static int add_font(struct FONTSPEC *f)
{
	int fnum;

	for (fnum = nfontnames; --fnum >= 0; )
		if (strcmp(f->name, fontnames[fnum]) == 0)
			return fnum;		/* already there */

	if (nfontnames >= MAXFONTS) {
		printf("++++ Too many fonts\n");
		exit(1);
	}
	fnum = nfontnames;
	strcpy(fontnames[fnum], f->name);
#ifdef DEBUG
	if (verbose >= 10)
		printf("New font %s at %d\n", f->name, fnum);
#endif
	nfontnames++;
	return fnum;
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
	add_font(&f->titlefont);
	add_font(&f->subtitlefont);
	add_font(&f->composerfont);
	add_font(&f->partsfont);
	add_font(&f->vocalfont);
	add_font(&f->textfont);
	add_font(&f->wordsfont);
	add_font(&f->gchordfont);
}

/* -- set_standard_format -- */
void set_standard_format(void)
{
	struct FORMAT *f;

	f = &cfmt;
	memset(f, 0, sizeof *f);
	strcpy(f->name, "standard");
	f->pageheight	= PAGEHEIGHT;
	f->staffwidth	= STAFFWIDTH;
	f->leftmargin	= LEFTMARGIN;
	f->topmargin	= 1.0 * CM;
	f->botmargin	= 1.0 * CM;
	f->topspace	= 0.8 * CM;
	f->titlespace 	= 0.2 * CM;
	f->subtitlespace = 0.1 * CM;
	f->composerspace = 0.2 * CM;
	f->musicspace	= 0.2 * CM;
	f->partsspace	= 0.3 * CM;
	f->staffsep	= 46.0 * PT;
	f->vocalspace	= 23.0 * PT;
	f->textspace	= 0.5 * CM;
/*	f->indent	= 0.0 * CM; */
/*	f->wordsspace	= 0.0 * CM; */
	f->scale	= 0.70;
	f->maxshrink	= 0.65;
/*	f->landscape	= 0; */
/*	f->titleleft	= 0; */
	f->stretchstaff	= 1;
/*	f->stretchlast	= 0; */
/*	f->continueall	= 0; */
/*	f->writehistory	= 0; */
/*	f->withxrefs	= 0; */
/*	f->oneperpage	= 0; */
/*	f->musiconly	= 0; */
/*	f->titlecaps	= 0; */
/*	f->barsperstaff	= 0; */
/*	f->encoding	= 0; */
	f->lineskipfac	= 1.1;
	f->parskipfac	= 0.4;
	f->measurenb = -1;
/*	f->measurebox = 0; */
	f->measurefirst = 1;
	fontspec(&f->titlefont,	"Times-Roman", 15.0);
	fontspec(&f->subtitlefont, "Times-Roman", 12.0);
	fontspec(&f->composerfont, "Times-Italic", 11.0);
	fontspec(&f->partsfont,	"Times-Roman", 11.0);
	fontspec(&f->tempofont,	"Times-Bold", 10.0);
	fontspec(&f->vocalfont,	"Times-Bold", 13.0);
	fontspec(&f->textfont,	"Times-Roman", 12.0);
	fontspec(&f->wordsfont,	"Times-Roman", 12.0);
	fontspec(&f->gchordfont, "Helvetica", 12.0);
#ifdef DEBUG
	if (verbose >= 10)
		printf("Loading format \"%s\"\n", f->name);
#endif
}

/* -- set_pretty_format -- */
void set_pretty_format(void)
{
	struct FORMAT *f;

	f = &cfmt;
	memset(f, 0, sizeof *f);
	strcpy(f->name, "pretty");
	f->pageheight	= PAGEHEIGHT;
	f->staffwidth	= STAFFWIDTH;
	f->leftmargin	= LEFTMARGIN;
	f->topmargin	= 1.0 * CM;
	f->botmargin	= 1.0 * CM;
	f->topspace	= 0.8 * CM;
	f->titlespace 	= 0.4 * CM;
	f->subtitlespace = 0.1 * CM;
	f->composerspace = 0.25 * CM;
	f->musicspace	= 0.25 * CM;
	f->partsspace	= 0.3 * CM;
	f->staffsep	= 50.0 * PT;
	f->vocalspace	=  23.0 * PT;
	f->textspace	= 0.5 * CM;
/*	f->indent	= 0.0 * CM; */
/*	f->wordsspace	= 0.0 * CM; */
	f->scale	= 0.8;
	f->maxshrink	= 0.55;
/*	f->landscape	= 0; */
/*	f->titleleft	= 0; */
	f->stretchstaff = 1;
/*	f->stretchlast	= 0; */
/*	f->continueall	= 0; */
/*	f->writehistory	= 0; */
/*	f->withxrefs	= 0; */
/*	f->oneperpage	= 0; */
/*	f->musiconly	= 0; */
/*	f->titlecaps	= 0; */
/*	f->barsperstaff = 0; */
/*	f->encoding	= 0; */
	f->lineskipfac	= 1.1;
	f->parskipfac	= 0.1;
	f->measurenb = -1;
/*	f->measurebox = 0; */
	f->measurefirst = 1;
	fontspec(&f->titlefont,	"Times-Roman", 18.0);
	fontspec(&f->subtitlefont,  "Times-Roman", 15.0);
	fontspec(&f->composerfont,  "Times-Italic", 12.0);
	fontspec(&f->partsfont,	"Times-Roman", 12.0);
	fontspec(&f->tempofont,	"Times-Bold", 10.0);
	fontspec(&f->vocalfont,	"Times-Bold", 14.0);
	fontspec(&f->textfont,	"Times-Roman", 10.0);
	fontspec(&f->wordsfont,	"Times-Roman", 10.0);
	fontspec(&f->gchordfont, "Helvetica", 12.0);
}

/* -- set_pretty2_format -- */
void set_pretty2_format(void)
{
	struct FORMAT *f;

	f = &cfmt;
	memset(f, 0, sizeof *f);
	strcpy(f->name, "pretty2");
	f->pageheight	= PAGEHEIGHT;
	f->staffwidth	= STAFFWIDTH;
	f->leftmargin	= LEFTMARGIN;
	f->topmargin	= 1.0 * CM;
	f->botmargin	= 1.0 * CM;
	f->topspace	= 0.8 * CM;
	f->titlespace	= 0.4 * CM;
	f->subtitlespace = 0.1 * CM;
	f->composerspace = 0.3 * CM;
	f->musicspace	= 0.25 * CM;
	f->partsspace	= 0.2 * CM;
	f->staffsep	= 55.0 * PT;
	f->vocalspace	= 23.0 * PT;
	f->textspace	= 0.2 * CM;
/*	f->indent	= 0.0 * CM; */
/*	f->wordsspace	= 0.0 * CM; */
	f->scale	= 0.70;
	f->maxshrink	= 0.55;
/*	f->landscape	= 0; */
	f->titleleft	= 1;
	f->stretchstaff	= 1;
/*	f->stretchlast	= 0; */
/*	f->continueall	= 0; */
/*	f->writehistory = 0; */
/*	f->withxrefs	= 0; */
/*	f->oneperpage	= 0; */
/*	f->musiconly	= 0; */
/*	f->titlecaps	= 0; */
/*	f->barsperstaff	= 0; */
/*	f->encoding	= 0; */
	f->lineskipfac	= 1.1;
	f->parskipfac	= 0.1;
	f->measurenb = -1;
/*	f->measurebox = 0; */
	f->measurefirst = 1;
	fontspec(&f->titlefont,	"Helvetica-Bold", 16.0);
	fontspec(&f->subtitlefont, "Helvetica-Bold", 13.0);
	fontspec(&f->composerfont, "Helvetica",	10.0);
	fontspec(&f->partsfont,	"Times-Roman", 12.0);
	fontspec(&f->tempofont,	"Times-Bold", 10.0);
	fontspec(&f->vocalfont,	"Times-Bold", 13.0);
	fontspec(&f->textfont,	"Times-Roman", 10.0);
	fontspec(&f->wordsfont,	"Times-Roman", 10.0);
	fontspec(&f->gchordfont, "Helvetica", 12.0);
}

/* -- print_format -- */
void print_format(void)
{
	struct format *fd;
static char yn[2][5]={"no","yes"};

	printf("Format \"%s\":\n", cfmt.name);
	for (fd = format_tb; fd->name; fd++) {
		switch (fd->type) {
		case FORMAT_I:
			switch (fd->subtype) {
			case 0:
				printf("  %-13s %d\n",
				       fd->name,
				       *((int *) fd->v));
				break;
			case 1:
				if (*((int *) fd->v) != 0)
					printf("  encoding      ISOLatin%d\n",
					       *((int *) fd->v));
				else	printf("  encoding      ASCII\n");
				break;
			}
			break;
		case FORMAT_F:
			printf("  %-13s %.2f\n",
			       fd->name,
			       *((float *) fd->v));
			break;
		case FORMAT_S: {
			struct FONTSPEC *s;

			s = (struct FONTSPEC *) fd->v;
			if (fd->subtype == 0
			    || !cfmt.partsbox)
				printf("  %-13s %s %.1f\n",
				       fd->name,
				       s->name, s->size);
			else	printf("  %-13s %s %.1f box\n",
				       fd->name,
				       s->name, s->size);
			break;
		}
		case FORMAT_U:
			printf("  %-13s %.2fcm\n",
			       fd->name,
			       *((float *) fd->v) / CM);
			break;
		case FORMAT_B:
			printf("  %-13s %s\n",
			       fd->name,
			       yn[*((int *) fd->v)]);
		}
	}
}

/* -- g_logv: read a logical variable -- */
static int g_logv(char *l)
{
	char t[31];

	if (*l == 0)
		return 1;
	get_str(t, l, sizeof t);
	if (!strcmp(t, "1") || !strcmp(t, "yes") || !strcmp(t, "true"))
		return 1;
	if (!strcmp(t, "0") || !strcmp(t, "no") || !strcmp(t, "false"))
		return 0;
	printf("++++ Unknown logical \"%s\" in line: %s\n", t, l);
	exit(1);
}

/* -- g_fltv: read a float variable, no units -- */
static float g_fltv(char *l)
{
	float v;

	sscanf(l, "%f", &v);
	return v;
}

/* -- g_intv: read an int variable, no units -- */
static int g_intv(char *l)
{
	int v;

	sscanf(l, "%d", &v);
	return v;
}

/* -- g_fspc: read a font specifier -- */
static void g_fspc(char *l,
		   struct FONTSPEC *fn)
{
	char	fname[STRLFMT];
	float	fsize;
	char *p;

	fsize = fn->size;
	p = get_str(fname, l, sizeof fname);
	if (*p != '\0')
		fsize = g_fltv(p);
	fontspec(fn,
		 strcmp(fname, "*") != 0 ? fname : fn->name,
		 fsize);
	if (!file_initialized)
		add_font(fn);
}

/* -- read a line with a format directive, set in format struct f -- */
int interpret_format_line(char *l)
{
	struct format *fd;
	char *p;
	char w[81];

	p = get_str(w, l, sizeof w);
	if (w[0] == '\0'
	    || w[0] == '%')
		return 0;
#ifdef DEBUG
	if (verbose >= 6)
		printf("Interpret format line: %s\n", l);
#endif
	if (!strcmp(w, "end"))
		return 1;

	for (fd = format_tb; fd->name; fd++)
		if (strcmp(w, fd->name) == 0)
			break;
	if (fd->name) {
		switch (fd->type) {
		case FORMAT_I:
			*((int *) fd->v) = g_intv(p);
			if (fd->subtype == 1
			    && (cfmt.encoding < 0 || cfmt.encoding > 6)) {
				ERROR(("Bad encoding value %d - changed to 0",
				       cfmt.encoding));
				cfmt.encoding = 0;
			}
			return 0;
		case FORMAT_F:
			*((float *) fd->v) = g_fltv(p);
			return 0;
		case FORMAT_S:
			g_fspc(p, (struct FONTSPEC *) fd->v);
			if (fd->subtype == 1
			    && strstr(p, "box") != 0)
				cfmt.partsbox = 1;
			return 0;
		case FORMAT_U:
			*((float *) fd->v) = scan_u(p);
			return 0;
		case FORMAT_B:
			*((int *) fd->v) = g_logv(p);
			return 0;
		}
	}

	if (!strcmp(w, "font")) {
		struct FONTSPEC tempfont;
		int fnum;

		get_str(w, p, sizeof w);
		for (fnum = nfontnames; --fnum >= 0; ) {
			if (!strcmp(w, fontnames[fnum]))
				break;
		}
		if (fnum < 0) {
			if (file_initialized) {
				ERROR(("Cannot predefine when output file open: %s",
				       l));
				exit(1);
			}
			tempfont.size = 12.0;
			g_fspc(p, &tempfont);
		}
		return 0;
	}
#ifdef DEBUG
	if (verbose >= 5)
		printf("Ignore format line: %s\n", l);
#endif
	return 2;
}

/* -- read_fmt_file -- */
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
		if (interpret_format_line(line))
			break;
	}
	fclose(fp);
	return 0;
}
