/*
 * abcm2ps: a program to typeset tunes written in abc format using PostScript
 *
 * Copyright (C) 1998-2000 Jean-François Moine
 *
 * Adapted from abc2ps-1.2.5:
 *  Copyright (C) 1996,1997  Michael Methfessel
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  The author can be contacted as follows:
 *
 * Jean-François Moine
 *	mailto:moinejf@free.fr
 * or	mailto:Jean-Francois.Moine@bst.bsf.alcatel.fr
 *
 * Original page:
 *	http://moinejf.free.fr/
 *
 * Original abc2ps:
 *  Michael Methfessel
 *  msm@ihp-ffo.de
 *  Institute for Semiconductor Physics, PO Box 409,
 *  D-15204 Frankfurt (Oder), Germany
 *
 */

/* Main program abcm2ps.c */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <string.h>

#include "abcparse.h"
#include "abc2ps.h"

/* -- global variables -- */

struct ISTRUCT info, default_info;
unsigned char deco_glob[128], deco_tune[128];
struct SYMBOL *sym;		/* (points to the symbols of the current voice) */

char fontnames[MAXFONTS][STRLFMT];	/* list of needed fonts */
int  nfontnames;

char page_init[201];		/* initialization string after page break */
int tunenum;			/* number of current tune */
int nsym;			/* number of symbols in line */
int pagenum;			/* current page in output file */

float posy;			/* vertical position on page */

#ifdef DEBUG
int verbose = VERBOSE0;		/* verbosity, global and within tune */
#endif

int in_page;

				/* switches modified by flags: */
int gmode;			/* switch for glue treatment */
int pagenumbers;		/* write page numbers ? */
int epsf;			/* for EPSF postscript output */
int choose_outname;		/* 1 names outfile w. title/fnam */

char outf[STRL1];		/* output file name */

int  file_initialized;		/* for output file */
FILE *fout;			/* output file */
int nepsf;			/* counter for epsf output files */

/* -- local variables -- */

static struct FORMAT sfmt;	/* format after initialization */
static int include_xrefs;	/* to include xref numbers in title */
static int one_per_page;	/* new page for each tune ? */
static int write_history;	/* write history and notes ? */
static float alfa_c;		/* max compression allowed */
static int bars_per_line;	/* bars for auto linebreaking */
static int encoding;		/* latin encoding number */
static int continue_lines;	/* flag to continue all lines */
static int deco_old;		/* abc2ps decorations */
static int help_me;		/* need help ? */
static int landscape;		/* flag for landscape output */
static float lmargin;		/* left margin */
static float indent;		/* 1st line indentation */
static int music_only;		/* no vocals if 1 */
static int pretty;		/* for pretty but sprawling layout */
static float scalefac; 		/* scale factor for symbol size */
static float staffsep; 		/* staff separation */
static char styf[STRL1];	/* layout style file name */
static char *styd;		/* layout style directory */
static float swidth;		/* staff width */
static int measurenb;		/* measure numbering (-1: none, 0: on the left, or every n bars) */
static int measurebox;		/* display measure numbers in a box */
static int measurefirst;	/* first measure number */
static char *sel;		/* current input file selector */
char *in_fname;			/* current input file name */

/* memory arena (for clrarena & getarena) */
static struct str_a {
	char	str[4096];	/* memory area */
	char	*p;		/* pointer in area */
	struct str_a *n;	/* next area */
	int	r;		/* remaining space in area */
} *str_r, *str_p;		/* root and current area pointers */

/* -- local functions -- */

static void cutext(char *fid);
static void do_filter(struct abctune *t,
		      char *sel);
static void do_select(struct abctune *t,
		      int first_tune,
		      int last_tune);
static void getext(char *fid,
		   char *ext);
static void init_ops(void);
static void output_file(void);
static char *read_file(void);
static int set_page_format(void);
static void usage(void);
static void write_version(void);

static void clrarena(void);

/* -- main program -- */
int main(int argc,
	 char *argv[])
{
	char ext[41];
	int j;
	char *p;
	char c, *aaa;

	/* -- set default options and parse arguments -- */
	printf("abcm2ps-" VERSION " (" VDATE ")\n");
	if (argc <= 1)
		usage();

	/* -- initialize -- */
	init_ops();
	page_init[0] = '\0';
	nfontnames = 0;
	abc_init((void *(*)(int sz)) getarena,	/* alloc */
		 0,	/* free */
		 sizeof(struct SYMBOL) - sizeof(struct abcsym),
		 0);	/* don't keep comments */

	/* parse the arguments - as soon as a file ends, it is treated */
	help_me = 0;
	in_fname = 0;
	while (--argc > 0) {
		argv++;
		p = *argv;
		if (*p == '+') {	/* switch off flags with '+' */
			while (*++p != '\0') {
				switch (*p) {
				case 'B': bars_per_line = 0; break;
				case 'c': continue_lines = 0; break;
				case 'E': epsf = 0; break;
				case 'F': styf[0] = '\0'; break;
				case 'j':
				case 'k': measurenb = -1; break;
				case 'l': landscape = 0; break;
				case 'M': music_only = 0; break;
				case 'N': pagenumbers = 0; break;
				case 'n': write_history = 0; break;
				case 'O':
					choose_outname = 0;
					strcpy(outf, OUTPUTFILE);
					break;
				case 'p': pretty = 0; break;
				case 'x': include_xrefs = 0; break;
				case '1': one_per_page = 0; break;
				default:
					printf("++++ Cannot switch off flag: +%c\n",
					       *p);
					return 1;
				}
			}
			continue;
		}

		if (*p == '-') {	     /* interpret a flag with '-'*/
			if (p[1] == '\0') {
				if (in_fname != 0)
					output_file();
				in_fname = "";		/* read from stdin */
				continue;
			}
			while (*++p != '\0') {
				switch (c = *p) {

					/* simple flags */
				case 'c': continue_lines = 1; break;
				case 'E': epsf = 1; break;
				case 'H': help_me = 1; break;
				case 'h': usage(); break;
				case 'l': landscape = 1; break;
				case 'M': music_only = 1; break;
				case 'N': pagenumbers = 1; break;
				case 'n': write_history = 1; break;
				case 'b':
				case 'C':
				case 'R':
				case 'S':
				case 'T':
				case 'o':
					printf("'-%c' is obsolete - option ignored\n",
					       c);
					break;
				case 'P': pretty = 2; break;
				case 'p': pretty = 1; break;
				case 'u': deco_old = 1; break;
				case 'V':
					write_version();
					exit(0);
				case 'x': include_xrefs = 1; break;
				case '1': one_per_page = 1; break;

				case 'e':	/* filtering */
					if (sel != 0) {
						printf("++++Too many '-e'\n");
						return 1;
					}
					sel = p + 1;
					if (sel[0] == '\0') {
						if (--argc <= 0) {
							printf("++++ No filter in '-e'\n");
							return 1;
						}
						argv++;
						sel = *argv;
					} else {
						while (p[1] != '\0')	/* stop */
							p++;
					}
					break;

					/* flags with parameter.. */
				case 'a':
				case 'B':
				case 'D':
				case 'd':
				case 'f':
				case 'F':
				case 'g':
				case 'I':
				case 'j':
				case 'k':
				case 'L':
				case 'm':
				case 'O':
				case 's':
				case 'v':
				case 'w':
				case 'Y':
					aaa = p + 1;
					if (aaa[0] != '\0'
					    && strchr("gO", c)) {	/* no sticky arg */
						printf("++++ Incorrect usage of flag -%c\n",
						       c);
						return 1;
					}

					if (aaa[0] == '\0') {
						argv++;
						aaa = *argv;
						if (--argc <= 0 || aaa[0] == '-') {
							printf("++++ Missing parameter after flag -%c\n",
							       c);
							return 1;
						}
					} else {
						while (p[1] != '\0')	/* stop */
							p++;
					}

					if (strchr("BbfjkLsvY", c)) {	    /* check num args */
						for (j = 0; j < strlen(aaa); j++)
							if (!strchr("0123456789.",
								    aaa[j])) {
								if (aaa[j] == 'b'
								    && aaa[j+1] == '\0'
								    && (c == 'j'
									|| c == 'k'))
									break;
								printf("++++ Invalid parameter <%s> for flag -%c\n",
								       aaa, c);
								return 1;
							}
					}

					switch (c) {
					case 'a':
						sscanf(aaa, "%f", &alfa_c);
						if (alfa_c > 1. || alfa_c < 0) {
							printf("++++ Bad parameter for flag -a: %s\n",
							       aaa);
							return 1;
						}
						break;
					case 'B':
						sscanf(aaa, "%d", &bars_per_line);
						continue_lines = 0;
						break;
					case 'D':
						styd = aaa;
						break;
					case 'd':
						staffsep = scan_u(aaa);
						break;
					case 'f':
						sscanf(aaa, "%d", &measurefirst);
						break;
					case 'F':
						strcpy(styf, aaa);
						break;
					case 'g':
						if (abbrev(aaa, "shrink", 2))
							gmode = G_SHRINK;
						else if (abbrev(aaa, "stretch", 2))
							gmode = G_STRETCH;
						else if (abbrev(aaa, "space", 2))
							gmode = G_SPACE;
						else if (abbrev(aaa, "fill", 2))
							gmode = G_FILL;
						else {
							printf("++++ Bad parameter for flag -g: %s\n",
							       aaa);
							return 1;
						}
						break;
					case 'I':
						indent = scan_u(aaa);
						break;
					case 'j':
					case 'k':
						sscanf(aaa, "%d", &measurenb);
						if (aaa[strlen(aaa) - 1] == 'b')
							measurebox = 1;
						else	measurebox = 0;
						break;
					case 'L':
						sscanf(aaa, "%d", &encoding);
						if (encoding < 0 || encoding > 6) {
							printf("++++ Bad encoding value %s - changed to 0\n",
								aaa);
							encoding = 0;
						}
						break;
					case 'm':
						lmargin = scan_u(aaa);
						break;
					case 'O':
						if (!strcmp(aaa, "=")) {
							choose_outname = 1;
						} else {
							getext(aaa, ext);
							if (strcmp(ext, "ps")
							    && strcmp(ext, "eps")
							    && ext[0] != '\0') {
								printf("Wrong extension for output file: %s\n",
								       aaa);
								return 1;
							}
							strcpy(outf, aaa);
							strext(outf, "ps");
							choose_outname = 0;
						}
						break;
					case 's':
						sscanf(aaa, "%f", &scalefac);
						break;
					case 'v':
#ifndef DEBUG
						printf("Program not compiled with -DDEBUG - option '-v' ignored\n");
#else
						sscanf(aaa, "%d", &verbose);
#endif
						break;
					case 'w':
						swidth = scan_u(aaa);
						break;
					case 'Y':
						/*fixme ??*/
						break;
					}
					break;
				default:
					printf("++++ Unknown flag: -%c\n", c);
					return 1;
				}
			}
			continue;
		}

		if (strstr(p, ".fmt")) {	/* implicit -F */
			strcpy(styf, p);
			continue;
		}

		if (in_fname != 0)
			output_file();
		in_fname = p;
	}

	if (help_me) {
		if (fout == 0 && set_page_format() != 0)
			exit(3);
		print_format();
		exit(0);
	}

	if (in_fname != 0)
		output_file();

	if (!epsf && fout == 0) {
		printf("No input file specified\n");
		exit(1);
	}

	close_output_file();

	return 0;
}

/* -- output the current ABC file -- */
static void output_file(void)
{
	struct abctune *t;
	char *file;

	/* read the file into memory */
	if ((file = read_file()) == 0) {
#ifdef unix
		perror("read_file");
#endif
		printf("++++ Cannot read input file '%s'\n",
		       in_fname);
		return;
	}

	/* initialize if not already done */
	if (fout == 0) {
		if (set_page_format() != 0)
			exit(3);
		sfmt = cfmt;
	} else {
		cfmt = sfmt;
		ops_into_fmt(&cfmt);
	}
	if (epsf)
		cutext(outf);

	memset(&default_info, 0, sizeof default_info);
	default_info.title[0] = "(notitle)";
	memcpy(&info, &default_info, sizeof info);
	reset_deco(deco_old);
	memcpy(&deco_tune, &deco_glob, sizeof deco_tune);
	if (!epsf) {
		if (choose_outname)
			strcpy(outf, in_fname);
		strext(outf, "ps");
		open_output_file(outf);
	}
	printf("%s ", in_fname);
#ifdef DEBUG
	if (verbose >= 3)
#endif
		printf("\n");
	clrarena();
	t = abc_parse(file);
	free(file);

	if (sel) {
		do_filter(t, sel);
		sel = 0;
	} else	do_select(t,
			  -32000,
			  (int) ((unsigned) (~0) >> 1));
	/*abc_free(t);	(useless) */
#ifdef DEBUG
	printf("\n");
#endif
}

/* -- cut off extension on a file identifier -- */
static void cutext(char *fid)
{
	char *p;

	if ((p = strrchr(fid, DIRSEP)) == 0)
		p = fid;
	if ((p = strrchr(p, '.')) != 0)
		*p = '\0';
}

/* -- do filtering on an input file -- */
static void do_filter(struct abctune *t, char *sel)
{
	int cur_sel;
	int end_sel;
	int n;

	for (;;) {
		sscanf(sel, "%d%n", &cur_sel, &n);
		sel += n;
		if (*sel == '-') {
			sel++;
			end_sel = (int) ((unsigned) (~0) >> 1);
			sscanf(sel, "%d%n", &end_sel, &n);
			sel += n;
		} else	end_sel = cur_sel;
		do_select(t, cur_sel, end_sel);
		if (*sel != ',')
			break;
		sel++;
	}
}

/* -- do a tune selection -- */
static void do_select(struct abctune *t,
		      int first_tune,
		      int last_tune)
{
	while (t != 0) {
		struct abcsym *s;
		int print_tune;

		print_tune = 0;
		for (s = t->first_sym; s != 0; s = s->next) {
			if (s->type == ABC_T_INFO
			    && s->text[0] == 'X') {
				int i;

				if (sscanf(s->text, "X:%d", &i) == 1
				    && i >= first_tune
				    && i <= last_tune)
					print_tune = 1;
				break;
			}
		}
		if (print_tune
		    || !t->client_data) {
			do_tune(t, !print_tune);
			t->client_data = 1;	/* treated */
		}
		t = t->next;
	}
}

/* -- get extension on a file identifier -- */
static void getext(char *fid,
		   char *ext)
{
	char *p;

	if ((p = strrchr(fid, DIRSEP)) == 0)
		p = fid;
	if ((p = strrchr(p, '.')) != 0) {
		strcpy(ext, p + 1);
	} else	ext[0] = '\0';
}

/* -- init_ops -- */
static void init_ops(void)
{
	one_per_page	= -1;
	landscape	= -1;
	scalefac	= -1.0;
	lmargin		= -1.0;
	indent		= -1.0;
	swidth		= -1.0;
	write_history   = -1;
	staffsep	= -1;
	continue_lines  = -1;
	include_xrefs   = -1;
	music_only	= -1;
	alfa_c		= -1.0;
	encoding	= -1;
	measurenb = -1;
	measurebox = -1;
	measurefirst = -1;

	pagenumbers	= 0;
	styf[0] = '\0';

	styd = DEFAULT_FDIR;
	strcpy(outf, OUTPUTFILE);
	pretty		= 0;
	epsf		= 0;
	choose_outname	= 0;
	gmode		= G_FILL;
}

/* -- ops_into_fmt -- */
void ops_into_fmt(struct FORMAT *fmt)
{
	if (landscape >= 0)
		fmt->landscape = landscape;
	if (scalefac >= 0)
		fmt->scale = scalefac;
	if (lmargin >= 0)
		fmt->leftmargin = lmargin;
	if (indent >= 0)
		fmt->indent = indent;
	if (swidth >= 0)
		fmt->staffwidth = swidth;
	if (continue_lines >= 0)
		fmt->continueall = continue_lines;
	if (write_history >= 0)
		fmt->writehistory = write_history;
	if (bars_per_line >= 0)
		fmt->barsperstaff = bars_per_line;
	if (encoding >= 0)
		fmt->encoding = encoding;
	if (include_xrefs >= 0)
		fmt->withxrefs = include_xrefs;
	if (staffsep >= 0)
		fmt->staffsep = staffsep;
	if (one_per_page >= 0)
		fmt->oneperpage = one_per_page;
	if (music_only >= 0)
		fmt->musiconly = music_only;
	if (measurenb >= 0)
		fmt->measurenb = measurenb;
	if (measurebox >= 0)
		fmt->measurebox = measurebox;
	if (measurefirst >= 0)
		fmt->measurefirst = measurefirst;
	if (alfa_c >= 0)
		fmt->maxshrink = alfa_c;
}

/* -- read an input file -- */
static char *read_file(void)
{
	int fsize;
	FILE *fin;
	char *file;

	if (in_fname[0] == '\0') {
		in_fname = "stdin";
		fsize = 8096;
		file = malloc(fsize);
		if (fread(file, 1, fsize, stdin) < 0) {
			free(file);
			return 0;
		}
	} else {
		if ((fin = fopen(in_fname, "rb")) == 0) {
			char new_file[256];
			char ext[41];

			getext(in_fname, ext);
			if (ext[0] != '\0')
				return 0;
			sprintf(new_file, "%s.abc", in_fname);
			if ((fin = fopen(new_file, "rb")) == 0)
				return 0;
		}
		if (fseek(fin, 0L, SEEK_END) < 0) {
			fclose(fin);
			return 0;
		}
		fsize = ftell(fin);
		rewind(fin);
		if ((file = malloc(fsize + 2)) == 0) {
			fclose(fin);
			return 0;
		}

		if (fread(file, 1, fsize, fin) != fsize) {
			fclose(fin);
			free(file);
			return 0;
		}
		file[fsize] = '\0';
		fclose(fin);
	}
	return file;
}

/* -- set_page_format --- */
static int set_page_format(void)
{
	switch (pretty) {
	default: set_standard_format(); break;
	case 1:  set_pretty_format(); break;
	case 2:  set_pretty2_format(); break;
	}

	read_fmt_file("fonts.fmt", styd);
	if (styf[0] != '\0') {
		strext(styf, "fmt");
		if (read_fmt_file(styf, styd) < 0) {
			printf("++++ Cannot open file: %s\n",
			       styf);
			return -1;
		}
		strcpy(cfmt.name, styf);
	}
	ops_into_fmt(&cfmt);

	make_font_list();
	return 0;
}

/* -- set extension on a file identifier -- */
void strext(char *fid,
	    char *ext)
{
	char *p, *q;

	if ((p = strrchr(fid, DIRSEP)) == 0)
		p = fid;
	if ((q = strrchr(p, '.')) == 0)
		strcat(p, ".");
	else	q[1] = '\0';
	strcat(p, ext);
}

/* -- display usage and exit -- */
static void usage(void)
{
	printf("ABC to Postscript translator.\n"
	       "Usage: abcm2ps [options] file [file_options] ..\n"
	       "where:\n"
	       " file   input files in abc format, or '-'\n"
	       " options and file_options:\n"
	       "  .output file options:\n"
	       "     -E      produce EPSF output, one tune per file\n"
	       "     -O fff  set outfile name to fff\n"
	       "     -O =    make outfile name from infile/title\n"
	       "  .output formatting:\n"
	       "     -p      pretty output (looks better, needs more space)\n"
	       "     -P      select second predefined pretty output style\n"
	       "     -s xx   set scale factor for symbol size to xx\n"
	       "     -w xx   set staff width (cm/in/pt)\n"
	       "     -m xx   set left margin (cm/in/pt)\n"
	       "     -d xx   set staff separation (cm/in/pt)\n"
	       "     -a xx   set max shrinkage to xx (between 0 and 1)\n"
	       "     -F foo  read format from \"foo.fmt\"\n"
	       "     -D bar  look for format files in directory \"bar\"\n"
	       "  .output options:\n"
	       "     -I xx   indent 1st line (cm/in/pt)\n"
	       "     -x      include xref numbers in output\n"
	       "     -n      include notes and history in output\n"
	       "     -N      write page numbers\n"
	       "     -1      write one tune per page\n"
	       "     -l      landscape mode\n"
	       "     -j n[b] number the measures every n bars (or on the left if n=0)\n"
	       "             if 'b', display in a box\n"
	       "     -k n[b] same as '-j' (abc2ps compatibility)\n"
	       "     -f n    set the first measure number to n\n"
	       "  .line breaks:\n"
	       "     -c      auto line break\n"
	       "     -B bb   break every bb bars\n"
	       "  .input file selection/options:\n"
	       "     -e pattern\n"
	       "             xref list of tunes to select\n"
	       "     -u      abc2ps implicit decorations\n"
	       "     -L n    set char encoding to Latin number n\n"
	       "  .help/configuration:\n"
	       "     -V      show program version\n"
	       "     -h      show this command summary\n"
	       "     -H      show the format parameters\n"
#ifdef DEBUG
	       "     -v nn   set verbosity level to nn\n"
#endif
	       "     -g xx   set glue mode to shrink|space|stretch|fill\n");
	exit(0);
}

/* -- write_version -- */
static void write_version(void)
{
	printf("Compiled: " __DATE__ "\n"
	       "Style: %s\n"
	       "Options:",
	       style);
#ifdef BSTEM_DOWN
	printf(" BSTEM_DOWN");
#endif
#ifdef DEBUG
	printf(" DEBUG");
#endif
#ifdef US_LETTER
	printf(" US_LETTER");
#endif
#if !defined(DEBUG) && !defined(DEBUG) && !defined(US_LETTER)
	printf(" NONE\n");
#else
	printf("\n");
#endif

	if (strlen(DEFAULT_FDIR) > 0)
		printf("Default format directory %s\n", DEFAULT_FDIR);
}

/* -- arena routines -- */
static void clrarena(void)
{
	str_p = 0;
}

/* The area is 8 bytes aligned to handle correctly int and pointers access
 * on some machines as Sun Sparc. */
char *getarena(int len)
{
	char *p;

	len = (len + 7) & ~7;		/* align at 64 bits boundary */
	if (str_p == 0
	    || str_p->r < len) {
		if (str_p == 0) {
			if (str_r == 0) {
				str_r = calloc(1, sizeof *str_r);
				str_p = str_r;
			} else	str_p = str_r;
		} else {
			if (str_p->n == 0)
				str_p->n = calloc(1, sizeof *str_r);
			str_p = str_p->n;
		}
		str_p->p = str_p->str;
		str_p->r = sizeof str_p->str;
	}
	p = str_p->p;
	str_p->p += len;
	str_p->r -= len;
	return p;
}
