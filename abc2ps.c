/*
 * abcm2ps: a program to typeset tunes written in ABC format using PostScript
 *
 * Copyright (C) 1998-2014 Jean-François Moine (http://moinejf.free.fr)
 *
 * Adapted from abc2ps-1.2.5:
 *  Copyright (C) 1996,1997  Michael Methfessel (msm@ihp-ffo.de)
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#ifdef linux
#include <unistd.h>
#endif

#include "abc2ps.h"
#include "front.h"

/* -- global variables -- */

INFO info;
unsigned char deco[256];
struct SYMBOL *sym;		/* (points to the symbols of the current voice) */

int tunenum;			/* number of current tune */
int pagenum = 1;		/* current page in output file */

				/* switches modified by command line flags: */
int quiet;			/* quiet mode */
int secure;			/* secure mode */
int annotate;			/* output source references */
int pagenumbers;		/* write page numbers */
int epsf;			/* for EPSF (1) or SVG (2) output */
int svg;			/* SVG (1) or XML (2 - HTML + SVG) output */
int showerror;			/* show the errors */

char outfn[FILENAME_MAX];	/* output file name */
int file_initialized;		/* for output file */
FILE *fout;			/* output file */
char *in_fname;			/* current input file name */
time_t mtime;			/* last modification time of the input file */
static time_t fmtime;		/*	"	"	of all files */

int s_argc;			/* command line arguments */
char **s_argv;

struct tblt_s *tblts[MAXTBLT];
struct cmdtblt_s cmdtblts[MAXCMDTBLT];
int ncmdtblt;

/* -- local variables -- */

static char abc_fn[FILENAME_MAX]; /* buffer for ABC file name */
static char *styd = DEFAULT_FDIR; /* format search directory */
static int def_fmt_done = 0;	/* default format read */
static struct SYMBOL notitle;

/* memory arena (for clrarena, lvlarena & getarena) */
#define MAXAREAL 3		/* max area levels:
				 * 0; global, 1: tune, 2: generation */
#define AREANASZ 8192		/* standard allocation size */
#define MAXAREANASZ 0x20000	/* biggest allocation size */
static int str_level;		/* current arena level */
static struct str_a {
	struct str_a *n;	/* next area */
	char	*p;		/* pointer in area */
	int	r;		/* remaining space in area */
	int	sz;		/* size of str[] */
	char	str[2];		/* start of memory area */
} *str_r[MAXAREAL], *str_c[MAXAREAL];	/* root and current area pointers */

/* -- local functions -- */
static void read_def_format(void);
static void treat_file(char *fn, char *ext);

static FILE *open_ext(char *fn, char *ext)
{
	FILE *fp;
	char *p;

	if ((fp = fopen(fn, "rb")) != NULL)
		return fp;
	if ((p = strrchr(fn, DIRSEP)) == NULL)
		p = fn;
	if (strrchr(p, '.') != NULL)
		return NULL;
	strcat(p, ".");
	strcat(p, ext);
	if ((fp = fopen(fn, "rb")) != NULL)
		return fp;
	return NULL;
}

/* -- open a file for reading -- */
FILE *open_file(char *fn,	/* file name */
		char *ext,	/* file type */
		char *rfn)	/* returned real file name */
{
	FILE *fp;
	char *p;
	int l;

	/* if there was some ABC file, try its directory */
	if (in_fname && in_fname != fn
	 && (p = strrchr(in_fname, DIRSEP)) != NULL) {
		l = p - in_fname + 1;
		strncpy(rfn, in_fname, l);
		strcpy(&rfn[l], fn);
		if ((fp = open_ext(rfn, ext)) != NULL)
			return fp;
	}

	/* try locally */
	strcpy(rfn, fn);
	if ((fp = open_ext(rfn, ext)) != NULL)
		return fp;

	/* try a format in the format directory */
	if (*ext != 'f' || *styd == '\0')
		return NULL;
	l = strlen(styd) - 1;
	if (styd[l] == DIRSEP)
		sprintf(rfn, "%s%s", styd, fn);
	else
		sprintf(rfn, "%s%c%s", styd, DIRSEP, fn);
	return open_ext(rfn, ext);
}

/* -- read a whole input file -- */
/* the real/full file name is put in tex_buf[] */
static char *read_file(char *fn, char *ext)
{
	size_t fsize;
	FILE *fin;
	char *file;

	if (*fn == '\0') {
		strcpy(tex_buf, "stdin");
		fsize = 0;
		file = malloc(8192);
		for (;;) {
			int l;

			l = fread(&file[fsize], 1, 8192, stdin);
			fsize += l;
			if (l != 8192)
				break;
			file = realloc(file, fsize + 8192);
		}
		if (ferror(stdin) != 0) {
			free(file);
			return 0;
		}
		if (fsize % 8192 == 0)
			file = realloc(file, fsize + 2);
		time(&fmtime);
	} else {
		struct stat sbuf;

		fin = open_file(fn, ext, tex_buf);
		if (!fin)
			return NULL;
		if (fseek(fin, 0L, SEEK_END) < 0) {
			fclose(fin);
			return NULL;
		}
		fsize = ftell(fin);
		rewind(fin);
		if ((file = malloc(fsize + 2)) == NULL) {
			fclose(fin);
			return NULL;
		}

		if (fread(file, 1, fsize, fin) != fsize) {
			fclose(fin);
			free(file);
			return NULL;
		}
		fstat(fileno(fin), &sbuf);
		memcpy(&fmtime, &sbuf.st_mtime, sizeof fmtime);
		fclose(fin);
	}
	file[fsize] = '\0';
	return file;
}

/* call back to handle %%format/%%abc-include - see front.c */
static void include_cb(unsigned char *fn)
{
	char abc_fn_sav[FILENAME_MAX];

	strcpy(abc_fn_sav, abc_fn);
	treat_file((char *) fn, "fmt");
	strcpy(abc_fn, abc_fn_sav);
}

/* -- treat an input file and generate the ABC file -- */
static void treat_file(char *fn, char *ext)
{
	struct abctune *t;
	char *file, *file2;
	int file_type, l;
	static int nbfiles;

	if (nbfiles > 2) {
		error(1, 0, "Too many included files");
		return;
	}

	/* initialize if not already done */
	if (!fout)
		read_def_format();

	/* read the file into memory */
	/* the real/full file name is in tex_buf[] */
	if ((file = read_file(fn, ext)) == NULL) {
		if (strcmp(fn, "default.fmt") != 0) {
			error(1, NULL, "Cannot read the input file '%s'", fn);
#if defined(unix) || defined(__unix__)
			perror("    read_file");
#endif
		}
		return;
	}
	if (!quiet)
		fprintf(stderr, "File %s\n", tex_buf);

	/* convert the strings */
	l = strlen(tex_buf);
	if (strcmp(&tex_buf[l - 3], ".ps") == 0) {
		file_type = FE_PS;
		frontend((unsigned char *) "%%beginps\n", 0);
	} else if (strcmp(&tex_buf[l - 4], ".fmt") == 0) {
		file_type = FE_FMT;
	} else {
		file_type = FE_ABC;
		strcpy(abc_fn, tex_buf);
		in_fname = abc_fn;
		mtime = fmtime;
	}

	nbfiles++;
	file2 = (char *) frontend((unsigned char *) file, file_type);
	nbfiles--;
	free(file);

	if (file_type == FE_PS)			/* PostScript file */
		file2 = (char *) frontend((unsigned char *) "%%endps", 0);

	if (nbfiles > 0)		/* if %%format */
		return;			/* don't free the preprocessed buffer */

//	memcpy(&deco_tune, &deco_glob, sizeof deco_tune);
	if (file_type == FE_ABC) {		/* if ABC file */
//		if (!epsf)
//			open_output_file();
		clrarena(1);			/* clear previous tunes */
	}
	t = abc_parse(file2);
	free(file2);
	front_init(0, 0, include_cb);		/* reinit the front-end */
	if (!t) {
		if (file_type == FE_ABC)
			error(1, NULL, "File '%s' is empty!", tex_buf);
		return;
	}

	while (t) {
		if (t->first_sym)		/*fixme:last tune*/
			do_tune(t);		/* generate */
		t = t->next;
	}
/*	abc_free(t);	(useless) */
}

/* -- read the default format -- */
static void read_def_format(void)
{
	if (def_fmt_done)
		return;
	def_fmt_done = 1;
	treat_file("default.fmt", "fmt");
}

/* -- set extension on a file name -- */
void strext(char *fn, char *ext)
{
	char *p, *q;

	if ((p = strrchr(fn, DIRSEP)) == NULL)
		p = fn;
	if ((q = strrchr(p, '.')) == NULL)
		strcat(p, ".");
	else
		q[1] = '\0';
	strcat(p, ext);
}

/* -- write the program version -- */
static void display_version(int full)
{
	fputs("abcm2ps-" VERSION " (" VDATE ")\n", stderr);
	if (!full)
		return;
	fputs("Compiled: " __DATE__ "\n"
	       "Options:"
#ifdef A4_FORMAT
		" A4_FORMAT"
#endif
#ifdef DECO_IS_ROLL
		" DECO_IS_ROLL"
#endif
#ifdef HAVE_PANGO
		" PANGO"
#endif
#if !defined(A4_FORMAT) && !defined(DECO_IS_ROLL) && !defined(HAVE_PANGO)
		" NONE"
#endif
		"\n", stderr);
	if (styd[0] != '\0')
		fprintf(stderr, "Default format directory: %s\n", styd);
}

/* -- display usage and exit -- */
static void usage(void)
{
	display_version(0);
	printf(	"ABC to Postscript translator.\n"
		"Usage: abcm2ps [options] file [file_options] ..\n"
		"where:\n"
		" file        input ABC file, or '-'\n"
		" options and file_options:\n"
		"  .output file options:\n"
		"     -E      produce EPSF output, one tune per file\n"
		"     -g      produce SVG output, one tune per file\n"
		"     -v      produce SVG output, one page per file\n"
		"     -X      produce SVG output in one XHTML file\n"
		"     -O fff  set outfile name to fff\n"
		"     -O =    make outfile name from infile/title\n"
		"     -i      indicate where are the errors\n"
		"     -k kk   size of the PS output buffer in Kibytes\n"
		"  .output formatting:\n"
		"     -s xx   set scale factor to xx\n"
		"     -w xx   set staff width (cm/in/pt)\n"
		"     -m xx   set left margin (cm/in/pt)\n"
		"     -d xx   set staff separation (cm/in/pt)\n"
		"     -a xx   set max shrinkage to xx (between 0 and 1)\n"
		"     -F foo  read format file \"foo.fmt\"\n"
		"     -D bar  look for format files in directory \"bar\"\n"
		"  .output options:\n"
		"     -l      landscape mode\n"
		"     -I xx   indent 1st line (cm/in/pt)\n"
		"     -x      add xref numbers in titles\n"
		"     -M      don't output the lyrics\n"
		"     -N n    set page numbering mode to n=\n"
		"             0=off 1=left 2=right 3=even left,odd right 4=even right,odd left\n"
		"     -1      write one tune per page\n"
		"     -G      no slur in grace notes\n"
		"     -j n[b] number the measures every n bars (or on the left if n=0)\n"
		"             if 'b', display in a box\n"
		"     -b n    set the first measure number to n\n"
		"     -f      have flat beams\n"
		"     -T n[v]   output the tablature 'n' for voice 'v' / all voices\n"
		"  .line breaks:\n"
		"     -c      auto line break\n"
		"     -B n    break every n bars\n"
		"  .input file selection/options:\n"
		"     -e pattern\n"
		"             tune selection\n"
		"  .help/configuration:\n"
		"     -V      show program version\n"
		"     -h      show this command summary\n"
		"     -H      show the format parameters\n"
		"     -S      secure mode\n"
		"     -q      quiet mode\n");
	exit(EXIT_SUCCESS);
}

#ifdef linux
/* -- where is the default format directory -- */
static void wherefmtdir(void)
{
	char exe[512], *p;
	FILE *f;
	int l;

	if ((l = readlink("/proc/self/exe", exe, sizeof exe)) <= 0)
		return;
	if ((p = strrchr(exe, '/')) == NULL)
		return;
	p++;
	if (p > &exe[5] && strncmp(p - 5, "/bin", 4) == 0) {
		strcpy(p - 4, "share/abcm2ps/");
		p += -4 + 14;
	}
	/* else, assume this is the source directory */

	/* check if a format file is present */
	strcpy(p, "tight.fmt");
	if ((f = fopen(exe, "r")) == NULL)
		return;
	fclose(f);

	/* change the format directory */
	p[-1] = '\0';
	styd = strdup(exe);
}
#endif

/* -- parse the tablature command ('-T n[v]') -- */
static struct cmdtblt_s *cmdtblt_parse(char *p)
{
	struct cmdtblt_s *cmdtblt;
	short val;

	if (ncmdtblt >= MAXCMDTBLT) {
		error(1, NULL, "++++ Too many '-T'");
		return NULL;
	}
	if (*p == '\0')
		val = -1;
	else {
		val = *p++ - '0' - 1;
		if ((unsigned) val > MAXTBLT) {
			error(1, NULL, "++++ Bad tablature number in '-T'\n");
			return 0;
		}
	}
	cmdtblt = &cmdtblts[ncmdtblt++];
	cmdtblt->index = val;
	cmdtblt->vn = p;
	return cmdtblt;
}

/* set a command line option */
static void set_opt(char *w, char *v)
{
	static char prefix = '%';	/* pseudo-comment prefix */

	if (!v)
		v = "";
	if (strlen(w) + strlen(v) >= TEX_BUF_SZ - 10) {
		error(1, NULL, "Command line '%s' option too long", w);
		return;
	}
	sprintf(tex_buf,		/* this buffer is available */
		"%%%c%s %s lock\n", prefix, w, v);
	if (strcmp(w, "abcm2ps") == 0)
		prefix = *v;
	frontend((unsigned char *) tex_buf, 0);
}

/* -- main program -- */
int main(int argc, char **argv)
{
	unsigned j;
	char *p, c, *aaa;

	if (argc <= 1)
		usage();

	/* set the global flags */
	s_argc = argc;
	s_argv = argv;
	aaa = NULL;
	while (--argc > 0) {
		argv++;
		p = *argv;
		if (*p != '-' || p[1] == '-') {
			if (*p == '+' && p[1] == 'F')	/* +F : no default format */
				def_fmt_done = 1;
			continue;
		}
		while ((c = *++p) != '\0') {	/* '-xxx' */
			switch (c) {
			case 'E':
				svg = 0;	/* EPS */
				epsf = 1;
				break;
			case 'g':
				svg = 0;	/* SVG one file per tune */
				epsf = 2;
				break;
			case 'h':
				usage();	/* no return */
			case 'q':
				quiet = 1;
				break;
			case 'S':
				secure = 1;
				break;
			case 'V':
				display_version(1);
				return EXIT_SUCCESS;
			case 'v':
				svg = 1;	/* SVG one file per pagee */
				epsf = 0;
				break;
			case 'X':
				svg = 2;	/* SVG/XHTML */
				epsf = 0;
				break;
			case 'k':
				if (p[1] == '\0') {
					if (--argc <= 0) {
						error(1, NULL, "No value for '-k' - aborting");
						return EXIT_FAILURE;
					}
					aaa = *++argv;
				} else {
					aaa = p + 1;
					p += strlen(p) - 1;
				}
				break;
			default:
				if (strchr("aBbDdeFfIjmNOsTw", c)) /* if with arg */
					p += strlen(p) - 1;	/* skip */
				break;
			}
		}
	}
	if (!quiet)
		display_version(0);

	/* initialize */
	outfn[0] = '\0';
	clrarena(0);				/* global */
	clrarena(1);				/* tunes */
	clrarena(2);				/* generation */
	if (aaa) {				/* '-k' output buffer size */
		int kbsz;

		sscanf(aaa, "%d", &kbsz);
		init_outbuf(kbsz);
	} else {
		init_outbuf(0);
	}
	abc_init(getarena,			/* alloc */
		0,				/* free */
		(void (*)(int level)) lvlarena, /* new level */
		sizeof(struct SYMBOL) - sizeof(struct abcsym),
		0);				/* don't keep comments */
//	memset(&info, 0, sizeof info);
	info['T' - 'A'] = &notitle;
	notitle.as.text = "T:";
	set_format();
	reset_deco();
	front_init(0, 0, include_cb);

#ifdef linux
	/* if not set, try to find where is the default format directory */
	if (styd[0] == '\0')
		wherefmtdir();
#endif
#ifdef HAVE_PANGO
	pg_init();
#endif

	/* parse the arguments - finding a new file, treat the previous one */
	argc = s_argc;
	argv = s_argv;
	while (--argc > 0) {
		argv++;
		p = *argv;
		if ((c = *p) == '\0')
			continue;
		if (c == '-') {
			int i;

			if (p[1] == '\0') {		/* '-' alone */
				if (in_fname) {
					treat_file(in_fname, "abc");
					frontend((unsigned char *) "select\n", 0);
				}
				in_fname = "";		/* read from stdin */
				continue;
			}
			i = strlen(p) - 1;
			if (p[i] == '-'
			 && p[1] != '-'
//fixme: 'e' may be preceded by other options
			 && p[1] != 'e'
			 && p[i -1] != 'O')
				c = '+'; /* switch off flags with '-x-' */
		}
		if (c == '+') {		/* switch off flags with '+' */
			while (*++p != '\0') {
				switch (*p) {
				case '-':
					break;
				case 'B':
					cfmt.barsperstaff = 0;
					lock_fmt(&cfmt.barsperstaff);
					break;
				case 'c':
					cfmt.continueall = 0;
					lock_fmt(&cfmt.continueall);
					break;
				case 'F':
//					 def_fmt_done = 1;
					break;
				case 'G':
					cfmt.graceslurs = 1;
					lock_fmt(&cfmt.graceslurs);
					break;
				case 'i':
					showerror = 0;
					break;
				case 'j':
					cfmt.measurenb = -1;
					lock_fmt(&cfmt.measurenb);
					break;
				case 'l':
					cfmt.landscape = 0;
					lock_fmt(&cfmt.landscape);
					break;
				case 'M':
					cfmt.fields[1] = 1 << ('w' - 'a');
					lock_fmt(&cfmt.fields);
					break;
				case 'N':
					pagenumbers = 0;
					break;
				case 'O':
					outfn[0] = '\0';
					break;
				case 'T': {
					struct cmdtblt_s *cmdtblt;

					aaa = p + 1;
					if (*aaa == '\0') {
						if (argc > 1
						    && argv[1][0] != '-') {
							aaa = *++argv;
							argc--;
						}
					} else {
						while (p[1] != '\0')	/* stop */
							p++;
						if (*p == '-')
							*p-- = '\0';	/* (not clean) */
					}
					cmdtblt = cmdtblt_parse(aaa);
					if (cmdtblt != 0)
						cmdtblt->active = 0;
					break;
				   }
				case 'x':
					cfmt.fields[0] &= ~(1 << ('X' - 'A'));
					lock_fmt(&cfmt.fields);
					break;
				case '0':
					cfmt.splittune = 0;
					lock_fmt(&cfmt.splittune);
					break;
				case '1':
					cfmt.oneperpage = 0;
					lock_fmt(&cfmt.oneperpage);
					break;
				default:
					error(1, NULL,
						"++++ Cannot switch off flag: +%c",
						*p);
					break;
				}
			}
			continue;
		}

		if (c == '-') {		     /* interpret a flag with '-' */
			if (p[1] == '-') {		/* long argument */
				p += 2;
				if (--argc <= 0) {
					error(1, NULL, "No argument for '--'");
					return EXIT_FAILURE;
				}
				argv++;
				set_opt(p, *argv);
				continue;
			}
			while ((c = *++p) != '\0') {
				switch (c) {

					/* simple flags */
				case 'A':
					annotate = 1;
					break;
				case 'c':
					cfmt.continueall = 1;
					lock_fmt(&cfmt.continueall);
					break;
				case 'E':
					break;
				case 'f':
					cfmt.flatbeams = 1;
					lock_fmt(&cfmt.flatbeams);
					break;
				case 'G':
					cfmt.graceslurs = 0;
					lock_fmt(&cfmt.graceslurs);
					break;
				case 'g':
					break;
				case 'H':
					if (!fout) {
						read_def_format();
						make_font_list();
					}
					print_format();
					return EXIT_SUCCESS;
				case 'i':
					showerror = 1;
					break;
				case 'l':
					cfmt.landscape = 1;
					lock_fmt(&cfmt.landscape);
					break;
				case 'M':
					cfmt.fields[1] &= ~(1 << ('w' - 'a'));
					lock_fmt(&cfmt.fields);
					break;
				case 'q':
				case 'S':
					break;
				case 'v':
				case 'X':
					break;
				case 'x':
					cfmt.fields[0] |= 1 << ('X' - 'A');
					lock_fmt(&cfmt.fields);
					break;
				case '0':
					cfmt.splittune = 1;
					lock_fmt(&cfmt.splittune);
					break;
				case '1':
					cfmt.oneperpage = 1;
					lock_fmt(&cfmt.oneperpage);
					break;

					/* flag with optional parameter */
				case 'N':
					if (p[1] == '\0'
					 && (argc <= 1
					  || !isdigit((unsigned) argv[1][0]))) {
						pagenumbers = 2; /* old behaviour */
						break;
					}
					/* fall thru */
					/* flags with parameter.. */
				case 'a':
				case 'B':
				case 'b':
				case 'D':
				case 'd':
				case 'e':
				case 'F':
				case 'I':
				case 'j':
				case 'k':
				case 'L':
				case 'm':
				case 'O':
				case 's':
				case 'T':
				case 'w':
					aaa = p + 1;
					if (*aaa == '\0') {
						aaa = *++argv;
						if (--argc <= 0
						 || (*aaa == '-' && c != 'O')) {
							error(1, NULL,
								"Missing parameter after '-%c' - aborting",
								c);
							return EXIT_FAILURE;
						}
					} else {
						p += strlen(p) - 1;	/* stop */
					}

					if (strchr("BbfjkNs", c)) {	/* check num args */
						for (j = 0; j < strlen(aaa); j++) {
							if (!strchr("0123456789.",
								    aaa[j])) {
								if (aaa[j] == 'b'
								 && aaa[j + 1] == '\0'
								 && c == 'j')
									break;
								error(1, NULL,
									"Invalid parameter <%s> for flag -%c",
									aaa, c);
								return EXIT_FAILURE;
							}
						}
					}

					switch (c) {
					case 'a':
						set_opt("maxshrink", aaa);
						break;
					case 'B':
						set_opt("barsperstaff", aaa);
						break;
					case 'b':
						set_opt("measurefirst", aaa);
						break;
					case 'D':
						styd = aaa;
						break;
					case 'd':
						set_opt("staffsep", aaa);
						break;
					case 'e':
						set_opt("select", aaa);
						break;
					case 'F':
						treat_file(aaa, "fmt");
						break;
					case 'I':
						set_opt("indent", aaa);
						break;
					case 'j':
						sscanf(aaa, "%d", &cfmt.measurenb);
						lock_fmt(&cfmt.measurenb);
						if (aaa[strlen(aaa) - 1] == 'b')
							cfmt.measurebox = 1;
						else
							cfmt.measurebox = 0;
						lock_fmt(&cfmt.measurebox);
						break;
					case 'k':
						break;
					case 'm':
						set_opt("leftmargin", aaa);
						break;
					case 'N':
						sscanf(aaa, "%d", &pagenumbers);
						if ((unsigned) pagenumbers > 4) {
							error(1, NULL,
								"'-N' value %s - changed to 2",
								aaa);
							pagenumbers = 2;
						}
						break;
					case 'O':
						if (strlen(aaa) >= sizeof outfn) {
							error(1, NULL, "'-O' too large - aborting");
							exit(EXIT_FAILURE);
						}
						strcpy(outfn, aaa);
						break;
					case 's':
						set_opt("scale", aaa);
						break;
					case 'T': {
						struct cmdtblt_s *cmdtblt;

						cmdtblt = cmdtblt_parse(aaa);
						if (cmdtblt)
							cmdtblt->active = 1;
						break;
					    }
					case 'w':
						set_opt("staffwidth", aaa);
						break;
					}
					break;
				default:
					error(1, NULL,
						"Unknown flag: -%c ignored", c);
					break;
				}
			}
			continue;
		}

		if (in_fname) {
			treat_file(in_fname, "abc");
			frontend((unsigned char *) "select\n", 0);
		}
		in_fname = p;
	}

	if (in_fname)
		treat_file(in_fname, "abc");
	if (multicol_start != 0) {		/* lack of %%multicol end */
		error(1, NULL, "Lack of %%%%multicol end");
		multicol_start = 0;
		buffer_eob();
		if (!info['X' - 'A']
		 && !epsf)
			write_buffer();
	}
	if (!epsf && !fout) {
		error(1, NULL, "No input file specified");
		return EXIT_FAILURE;
	}
	close_output_file();
	return severity == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* -- arena routines -- */
void clrarena(int level)
{
	struct str_a *a_p;

	if ((a_p = str_r[level]) == NULL) {
		str_r[level] = a_p = malloc(sizeof *str_r[0] + AREANASZ - 2);
		a_p->sz = AREANASZ;
		a_p->n = 0;
	}
	str_c[level] = a_p;
	a_p->p = a_p->str;
	a_p->r = sizeof a_p->str;
}

int lvlarena(int level)
{
	int old_level;

	old_level = str_level;
	str_level = level;
	return old_level;
}

/* The area is 8 bytes aligned to handle correctly int and pointers access
 * on some machines as Sun Sparc. */
void *getarena(int len)
{
	char *p;
	struct str_a *a_p;

	a_p = str_c[str_level];
	len = (len + 7) & ~7;		/* align at 64 bits boundary */
	if (len > a_p->r) {
		if (len > MAXAREANASZ) {
			error(1, NULL,
				"getarena - data too wide %d - aborting",
				len);
			exit(EXIT_FAILURE);
		}
		if (len > AREANASZ) {			/* big allocation */
			struct str_a *a_n;

			a_n = a_p->n;
			a_p->n = malloc(sizeof *str_r[0] + len - 2);
			a_p->n->n = a_n;
			a_p->n->sz = len;
		} else if (a_p->n == 0) {		/* standard allocation */
			a_p->n = malloc(sizeof *str_r[0] + AREANASZ - 2);
			a_p->n->n = 0;
			a_p->n->sz = AREANASZ;
		}
		str_c[str_level] = a_p = a_p->n;
		a_p->p = a_p->str;
		a_p->r = a_p->sz;
	}
	p = a_p->p;
	a_p->p += len;
	a_p->r -= len;
	return p;
}
