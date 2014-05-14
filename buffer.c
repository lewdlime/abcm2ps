/*
 * Postscript buffering functions.
 *
 * This file is part of abcm2ps.
 *
 * Copyright (C) 1998-2014 Jean-François Moine
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
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335  USA
 */

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef WIN32
#define snprintf _snprintf
#endif

#include "abc2ps.h" 

#define BUFFLN	80		/* max number of lines in output buffer */

static int ln_num;		/* number of lines in buffer */
static float ln_pos[BUFFLN];	/* vertical positions of buffered lines */
static char *ln_buf[BUFFLN];	/* buffer location of buffered lines */
static float ln_lmarg[BUFFLN];	/* left margin of buffered lines */
static float ln_scale[BUFFLN];	/* scale of buffered lines */
static signed char ln_font[BUFFLN];	/* font of buffered lines */
static float cur_lmarg = 0;	/* current left margin */
static float min_lmarg, max_rmarg;	/* margins for -E/-g */
static float cur_scale = 1.0;	/* current scale */
static float maxy;		/* remaining vertical space in page */
static float bposy;		/* current position in buffered data */
static int nepsf;		/* counter for -E/-g output files */
static int nbpages;		/* number of pages in the output file */
static int outbufsz;		/* size of outbuf */
static char outfnam[FILENAME_MAX]; /* internal file name for open/close */
static struct FORMAT *p_fmt;	/* current format while treating a new page */

int (*output)(FILE *out, const char *fmt, ...);

int in_page;			/* filling a PostScript page */
char *outbuf;			/* output buffer.. should hold one tune */
char *mbf;			/* where to a2b() */
int use_buffer;			/* 1 if lines are being accumulated */

/* -- cut off extension on a file identifier -- */
static void cutext(char *fid)
{
	char *p;

	if ((p = strrchr(fid, DIRSEP)) == NULL)
		p = fid;
	if ((p = strrchr(p, '.')) != NULL)
		*p = '\0';
}

/* -- open the output file -- */
void open_fout(void)
{
	int i;
	char fnm[FILENAME_MAX];

	strcpy(fnm, outfn);
	i = strlen(fnm) - 1;
	if (i < 0) {
		strcpy(fnm, svg || epsf > 1 ? "Out.xhtml" : OUTPUTFILE);
	} else if (i != 0 || fnm[0] != '-') {
		if (fnm[i] == '=' && in_fname) {
			char *p;

			if ((p = strrchr(in_fname, DIRSEP)) == NULL)
				p = in_fname;
			else
				p++;
			strcpy(&fnm[i], p);
			strext(fnm, svg || epsf > 1 ? "xhtml" : "ps");
		} else if (fnm[i] == DIRSEP) {
			strcpy(&fnm[i + 1],
				svg || epsf > 1 ? "Out.xhtml" : OUTPUTFILE);
		}
#if 0
/*fixme: fnm may be a directory*/
		else	...
#endif
	}
	if (svg == 1
	 && (i != 0 || fnm[0] != '-')) {
		cutext(fnm);
		i = strlen(fnm) - 1;
		if (strncmp(fnm, outfnam, i) != 0)
			nepsf = 0;
		sprintf(&fnm[i + 1], "%03d.svg", ++nepsf);
	} else if (strcmp(fnm, outfnam) == 0) {
		return;				/* same output file */
	}

	close_output_file();
	strcpy(outfnam, fnm);
	if (i != 0 || fnm[0] != '-') {
		if ((fout = fopen(fnm, "w")) == NULL) {
			error(1, NULL, "Cannot create output file %s - abort", fnm);
			exit(EXIT_FAILURE);
		}
	} else {
		fout = stdout;
	}
}

/* -- convert a date -- */
static void cnv_date(time_t *ltime)
{
	char buf[TEX_BUF_SZ];

	tex_str(cfmt.dateformat);
	strcpy(buf, tex_buf);
	strftime(tex_buf, TEX_BUF_SZ, buf, localtime(ltime));
}

/* initialize the min/max margin values */
/* (used only with eps -E and svg -g) */
void marg_init(void)
{
	min_lmarg = cfmt.pagewidth;
	max_rmarg = cfmt.pagewidth;
}

/* -- initialize the postscript file (PS or EPS) -- */
static void init_ps(char *str)
{
	time_t ltime;
	unsigned i;
	char version[32];

	if (epsf) {
		cur_lmarg = min_lmarg - 10;
		fprintf(fout, "%%!PS-Adobe-2.0 EPSF-2.0\n"
			"%%%%BoundingBox: 0 0 %.0f %.0f\n",
			(p_fmt->landscape ? p_fmt->pageheight : p_fmt->pagewidth)
				- cur_lmarg - max_rmarg + 10,
			-bposy);
		marg_init();
	} else {
		if (!fout)
			open_fout();
		fprintf(fout, "%%!PS-Adobe-2.0\n");
		fprintf(fout, "%%%%BoundingBox: 0 0 %.0f %.0f\n",
			p_fmt->pagewidth,
			p_fmt->pageheight);
	}
	fprintf(fout, "%%%%Title: %s\n", str);
	time(&ltime);
#ifndef WIN32
	strftime(tex_buf, TEX_BUF_SZ, "%b %e, %Y %H:%M", localtime(&ltime));
#else
	strftime(tex_buf, TEX_BUF_SZ, "%b %#d, %Y %H:%M", localtime(&ltime));
#endif
	fprintf(fout, "%%%%Creator: abcm2ps-" VERSION "\n"
		"%%%%CreationDate: %s\n", tex_buf);
	if (!epsf)
		fprintf(fout, "%%%%Pages: (atend)\n");
	fprintf(fout, "%%%%LanguageLevel: 3\n"
		"%%%%EndComments\n"
		"%%CommandLine:");
	for (i = 1; i < (unsigned) s_argc; i++) {
		char *p, *q;
		int space;

		p = s_argv[i];
		space = strchr(p, ' ') != NULL || strchr(p, '\n') != NULL;
		fputc(' ', fout);
		if (space)
			fputc('\'', fout);
		for (;;) {
			q = strchr(p, '\n');
			if (!q)
				break;
			fprintf(fout, " %.*s\n%%", (int) (q - p), p);
			p = q + 1;
		}
		fprintf(fout, "%s", p);
		if (space)
			fputc('\'', fout);
	}
	fprintf(fout, "\n\n");
	if (epsf)
		fprintf(fout, "save\n");
	strcpy(version, "/creator [(abcm2ps) " VERSION "] def");
	for (i = 0; i < strlen(version); i++) {
		if (version[i] == '.')
			version[i] = ' ';
	}
	fprintf(fout, "%%%%BeginSetup\n"
		"/!{bind def}bind def\n"
		"/bdef{bind def}!\n"		/* for compatibility */
		"/T/translate load def\n"
		"/M/moveto load def\n"
		"/RM/rmoveto load def\n"
		"/L/lineto load def\n"
		"/RL/rlineto load def\n"
		"/C/curveto load def\n"
		"/RC/rcurveto load def\n"
		"/SLW/setlinewidth load def\n"
		"/defl 0 def\n"	/* decoration flags - see deco.c for values */
		"/dlw{0.7 SLW}!\n"

		"%s\n", version);
	define_symbols();
	output = fprintf;
	user_ps_write();
	define_fonts();
	if (!epsf)
		fprintf(fout, "/setpagedevice where{pop\n"
			"	<</PageSize[%.0f %.0f]>>setpagedevice}if\n",
				p_fmt->pagewidth, p_fmt->pageheight);
	fprintf(fout, "%%%%EndSetup\n");
	file_initialized = 1;
}

/* -- initialize the svg file (option '-g') -- */
static void init_svg(char *str)
{
	cur_lmarg = min_lmarg - 10;
	output = svg_output;
#if 1 //fixme:test
	if (file_initialized > 0)
		fprintf(stderr, "??? init_svg: file_initialized\n");
#endif
	define_svg_symbols(str, nepsf,
		(p_fmt->landscape ? p_fmt->pageheight : p_fmt->pagewidth)
				- cur_lmarg - max_rmarg + 10,
		-bposy);
	file_initialized = 1;
	user_ps_write();
}

static void close_fout(void)
{
	long m;

	if (fout == stdout)
		goto out2;
	if (quiet)
		goto out1;
	m = ftell(fout);
	if (epsf || svg == 1)
		fprintf(stderr, "Output written on %s (%ld bytes)\n",
			outfnam, m);
	else
		fprintf(stderr,
			"Output written on %s (%d page%s, %d title%s, %ld bytes)\n",
			outfnam,
			nbpages, nbpages == 1 ? "" : "s",
			tunenum, tunenum == 1 ? "" : "s",
			m);
out1:
	fclose(fout);
out2:
	fout = NULL;
	file_initialized = 0;
}

/* -- close the output file -- */
/* epsf is always null */
void close_output_file(void)
{
	if (!fout)
		return;
	if (multicol_start != 0) {	/* if no '%%multicol end' */
		error(1, NULL, "No \"%%%%multicol end\"");
		multicol_start = 0;
		write_buffer();
	}
	if (tunenum == 0)
		error(0, NULL, "No tunes written to output file");
	close_page();
	switch (svg) {
	case 0:				/* PS */
		if (epsf == 0)
			fprintf(fout, "%%%%Trailer\n"
				"%%%%Pages: %d\n"
				"%%EOF\n", nbpages);
		close_fout();
		break;
	case 2:				/* -X */
		fputs("</body>\n"
			"</html>\n", fout);
	case 3:				/* -z */
		close_fout();
		break;
//	default:
//	case 1:				/* -v */
//		'fout' is closed in close_page
	}

	nbpages = tunenum = 0;
	defl = 0;
}

/* -- close the PS / SVG page -- */
void close_page(void)
{
	if (!in_page)
		return;
	in_page = 0;
	if (svg) {
		svg_close();
		if (svg == 1 && fout != stdout)
			close_fout();
		else
			fputs("</p>\n", fout);
	} else {
#if 1
		fprintf(fout, "grestore\n"
				"showpage\n"
				"%%%%EndPage: %d %d\n",
				nbpages, nbpages);
#else
		fputs("%%PageTrailer\n"
			"grestore\n"
			"showpage\n", fout);
#endif
	}
	cur_lmarg = 0;
	cur_scale = 1.0;
	outft = -1;
}

/* -- output a header/footer element -- */
static void format_hf(char *d, char *p)
{
	char *q;
	time_t ltime;

	for (;;) {
		if (*p == '\0')
			break;
		if ((q = strchr(p, '$')) != NULL)
			*q = '\0';
		d += sprintf(d, "%s", p);
		if (!q)
			break;
		p = q + 1;
		switch (*p) {
		case 'd':
			ltime = mtime;
			goto dput;
		case 'D':
			time(&ltime);
		dput:	cnv_date(&ltime);
			d += sprintf(d, "%s", tex_buf);
			break;
		case 'F':		/* ABC file name */
#if DIRSEP!='\\'
			d += sprintf(d, "%s", in_fname);
#else
			{
				int i;

				q = in_fname;
				i = TEX_BUF_SZ;
				for (;;) {
					if (--i <= 0 || *q == '\0')
						break;
					if ((*d++ = *q++) == '\\') {
						i--;
						*d++ = '\\';
					}
				}
				*d = '\0';
			}
#endif
			break;
		case 'I':		/* information field */
			p++;
			if (*p < 'A' || *p > 'Z' || !info[*p - 'A'])
				break;
			d += sprintf(d, "%s", &info[*p - 'A']->as.text[2]);
			break;
		case 'P':		/* page number */
			if (p[1] == '0') {
				p++;
				if (pagenum & 1)
					break;
			} else if (p[1] == '1') {
				p++;
				if ((pagenum & 1) == 0)
					break;
			}
			d += sprintf(d, "%d", pagenum);
			break;
		case 'T':		/* tune title */
			q = &info['T' - 'A']->as.text[2];
			tex_str(q);
			d += sprintf(d, "%s", tex_buf);
			break;
		case 'V':
			d += sprintf(d,"abcm2ps-"  VERSION);
			break;
		default:
			continue;
		}
		p++;
	}
	*d = '\0';		/* in case of empty string */
}

/* -- output the header or footer -- */
static float headfooter(int header,
			float pwidth,
			float pheight)
{
	char tmp[TEX_BUF_SZ], str[TEX_BUF_SZ + 1024];
	char *p, *q, *r, *mbf_sav;
	float size, y, wsize;
	struct FONTSPEC *f, f_sav;
	int cft_sav, dft_sav;

	if (header) {
		p = cfmt.header;
		f = &cfmt.font_tb[HEADERFONT];
		size = f->size;
		y = 2;
	} else {
		p = cfmt.footer;
		f = &cfmt.font_tb[FOOTERFONT];
		size = f->size;
		y = - (pheight - cfmt.topmargin - cfmt.botmargin)
			- size + 2;
	}
	if (*p == '-') {
		if (pagenum == 1)
			return 0;
		p++;
	}
	get_str_font(&cft_sav, &dft_sav);
	memcpy(&f_sav, &cfmt.font_tb[0], sizeof f_sav);
	wsize = 0;
	str_font(f - cfmt.font_tb);
	output(fout, "%.1f F%d ", size, f->fnum);
	outft = f - cfmt.font_tb;

	/* may have 2 lines */
	if ((r = strstr(p, "\\n")) != NULL) {
		if (!header)
			y += size;
		wsize += size;
		*r = '\0';
	}
	mbf_sav = mbf;
	for (;;) {
		tex_str(p);
		strcpy(tmp, tex_buf);
		format_hf(str, tmp);

		/* left side */
		p = str;
		if ((q = strchr(p, '\t')) != NULL) {
			if (q != p) {
				*q = '\0';
				output(fout, "%.1f %.1f M ",
					p_fmt->leftmargin, y);
				str_out(p, A_LEFT);
				a2b("\n");
				mbf = mbf_sav;
				if (svg)
					svg_write(mbf, strlen(mbf));
				else
					fputs(mbf, fout);
			}
			p = q + 1;
		}
		if ((q = strchr(p, '\t')) != NULL)
			*q = '\0';

		/* center */
		if (q != p) {
			output(fout, "%.1f %.1f M ",
				pwidth * 0.5, y);
			str_out(p, A_CENTER);
			a2b("\n");
			mbf = mbf_sav;
			if (svg)
				svg_write(mbf, strlen(mbf));
			else
				fputs(mbf, fout);
		}

		/* right side */
		if (q) {
			p = q + 1;
			if (*p != '\0') {
				output(fout, "%.1f %.1f M ",
					pwidth - p_fmt->rightmargin, y);
				str_out(p, A_RIGHT);
				a2b("\n");
				mbf = mbf_sav;
				if (svg)
					svg_write(mbf, strlen(mbf));
				else
					fputs(mbf, fout);
			}
		}
		if (!r)
			break;
		*r = '\\';
		p = r + 2;
		r = NULL;
		y -= size;
	}

	/* restore the fonts */
	*mbf_sav = '\0';
	memcpy(&cfmt.font_tb[0], &f_sav, sizeof cfmt.font_tb[0]);
	set_str_font(cft_sav, dft_sav);
	return wsize;
}

/* -- initialize the first page or a new page for svg -- */
/* the flag 'in_page' is always false and epsf is always null */
static void init_page(void)
{
	float pheight, pwidth;

	p_fmt = !info['X' - 'A'] ? &cfmt : &dfmt;	/* global format */

	nbpages++;
	if (svg) {
		if (file_initialized <= 0) {
			if (!fout)
				open_fout();
			define_svg_symbols(in_fname, nbpages,
				cfmt.landscape ? p_fmt->pageheight : p_fmt->pagewidth,
				cfmt.landscape ? p_fmt->pagewidth : p_fmt->pageheight);
			file_initialized = 1;
			output = svg_output;
		} else {
			define_svg_symbols(in_fname, nbpages,
				cfmt.landscape ? p_fmt->pageheight : p_fmt->pagewidth,
				cfmt.landscape ? p_fmt->pagewidth : p_fmt->pageheight);
		}
		user_ps_write();
	} else if (file_initialized <= 0) {
		init_ps(in_fname);
	}
	in_page = 1;
	outft = -1;

	if (!svg)
		fprintf(fout, "%%%%Page: %d %d\n",
			nbpages, nbpages);
	if (cfmt.landscape) {
		pheight = p_fmt->pagewidth;
		pwidth = cfmt.pageheight;
		if (!svg)
			fprintf(fout, "%%%%PageOrientation: Landscape\n"
				"gsave 90 rotate 0 %.1f T\n",
				-cfmt.topmargin);
	} else {
		pheight = cfmt.pageheight;
		pwidth = p_fmt->pagewidth;
		if (!svg)
			fprintf(fout, "gsave 0 %.1f T\n",
				pheight - cfmt.topmargin);
	}
	if (svg)
		output(fout, "0 %.1f T\n", -cfmt.topmargin);
	else
		output(fout,
			"%% --- width %.1f\n",		/* for index */
			((cfmt.landscape ? cfmt.pageheight : cfmt.pagewidth)
			 - cfmt.leftmargin - cfmt.rightmargin) / cfmt.scale);

	maxy = pheight - cfmt.topmargin - cfmt.botmargin;

	/* output the header and footer */
	if (!cfmt.header) {
		char *p = NULL;

		switch (pagenumbers) {
		case 1: p = "$P\t"; break;
		case 2: p = "\t\t$P"; break;
		case 3: p = "$P0\t\t$P1"; break;
		case 4: p = "$P1\t\t$P0"; break;
		}
		if (p)
			cfmt.header = strdup(p);
	}
	if (cfmt.header) {
		float dy;

		dy = headfooter(1, pwidth, pheight);
		if (dy != 0) {
			output(fout, "0 %.1f T\n", -dy);
			maxy -= dy;
		}
	}
	if (cfmt.footer)
		maxy -= headfooter(0, pwidth, pheight);
	pagenum++;
	outft = -1;
}

/* -- adjust the tune title part of the output file name -- */
static void epsf_fn_adj(char *p)
{
	char c;

	while ((c = *p) != '\0') {
		if (c == ' ')
			*p = '_';
		else if (c == DIRSEP || (unsigned) c >= 127)
			*p = '.';
		p++;
	}
}

/* -- build the title of the eps/svg file and check if correct utf-8 -- */
static void epsf_title(char *p, int sz)
{
	unsigned char c;

	snprintf(p, sz, "%.72s (%.4s)", in_fname, &info['X' - 'A']->as.text[2]);
	while ((c = (unsigned char) *p) != '\0') {
		if (c >= 0x80) {
			if ((c & 0xf8) == 0xf0) {
				if ((p[1] & 0xc0) != 0x80
				 && (p[2] & 0xc0) != 0x80
				 && (p[3] & 0xc0) != 0x80)
					*p = ' ';
			} else if ((c & 0xf0) == 0xe0) {
				if ((p[1] & 0xc0) != 0x80
				 && (p[2] & 0xc0) != 0x80)
					*p = ' ';
			} else if ((c & 0xe0) == 0xc0) {
				if ((p[1] & 0xc0) != 0x80)
					*p = ' ';
			} else {
				*p = ' ';
			}
		}
		p++;
	}
}

/* -- output a EPS (-E) or SVG (-g) file -- */
void write_eps(void)
{
	unsigned i;
	char *p, title[80];

	if (mbf == outbuf
	 || !info['X' - 'A'])
		return;

	p_fmt = &cfmt;				/* tune format */

	if (epsf != 3) {			/* if not -z */
		strcpy(outfnam, outfn);
		if (outfnam[0] == '\0')
			strcpy(outfnam, OUTPUTFILE);
		cutext(outfnam);
		i = strlen(outfnam) - 1;
		if (i == 0 && outfnam[0] == '-') {
			if (epsf == 1) {
				error(1, NULL, "Cannot use stdout with '-E' - abort");
				exit(EXIT_FAILURE);
			}
			fout = stdout;
		} else {
			if (outfnam[i] == '=') {
				p = &info['T' - 'A']->as.text[2];
				while (isspace((unsigned char) *p))
					p++;
					strncpy(&outfnam[i], p, sizeof outfnam - i - 4);
				outfnam[sizeof outfnam - 5] = '\0';
				epsf_fn_adj(&outfnam[i]);
			} else {
				if (i >= sizeof outfnam - 4 - 3)
					i = sizeof outfnam - 4 - 3;
				sprintf(&outfnam[i + 1], "%03d", ++nepsf);
			}
			strcat(outfnam, epsf == 1 ? ".eps" : ".svg");
			if ((fout = fopen(outfnam, "w")) == NULL) {
				error(1, NULL, "Cannot open output file %s - abort",
						outfnam);
				exit(EXIT_FAILURE);
			}
		}
	}
	epsf_title(title, sizeof title);
	if (epsf == 1) {
		init_ps(title);
		fprintf(fout, "0 %.1f T\n", -bposy);
		write_buffer();
		fprintf(fout, "showpage\nrestore\n");
	} else {
		if (epsf == 3 && file_initialized == 0)
			fputs("<br/>\n", fout);	/* new image in the same flow */
		init_svg(title);
		write_buffer();
		svg_close();
	}
	if (epsf != 3)
		close_fout();
	else
		file_initialized = 0;
	cur_lmarg = 0;
	cur_scale = 1.0;
}

/* -- start a new page -- */
/* epsf is always null */
static void newpage(void)
{
	close_page();
	init_page();
}

/*  subroutines to handle output buffer  */

/* -- update the output buffer pointer -- */
void a2b(char *fmt, ...)
{
	va_list args;

	if (mbf + BSIZE > outbuf + outbufsz) {
		if (epsf) {
			error(1, NULL, "Output buffer overflow - increase outbufsz");
			fprintf(stderr, "*** abort\n");
			exit(EXIT_FAILURE);
		}
		error(0, NULL, "Possible buffer overflow");
		write_buffer();
		use_buffer = 0;
	}
	va_start(args, fmt);
	mbf += vsnprintf(mbf, outbuf + outbufsz - mbf, fmt, args);
	va_end(args);
}

/* -- translate down by 'h' scaled points in output buffer -- */
void bskip(float h)
{
	if (h == 0)
		return;
	bposy -= h * cfmt.scale;
	a2b("0 %.2f T\n", -h);
}

/* -- initialize the output buffer -- */
void init_outbuf(int kbsz)
{
	if (outbuf)
		free(outbuf);
	outbufsz = kbsz * 1024;
	if (outbufsz < 0x10000)
		outbufsz = 0x10000;
	outbuf = malloc(outbufsz);
	if (!outbuf) {
		error(1, NULL, "Out of memory for outbuf - abort");
		exit(EXIT_FAILURE);
	}
	bposy = 0;
	ln_num = 0;
	mbf = outbuf;
}

/* -- write buffer contents, break at full pages -- */
void write_buffer(void)
{
	char *p_buf;
	int l, np;
	float p1, dp;
	int outft_sav;

	if (mbf == outbuf || multicol_start != 0)
		return;
	if (!in_page && !epsf)
		init_page();
	outft_sav = outft;
	p1 = 0;
	p_buf = outbuf;
	for (l = 0; l < ln_num; l++) {
		if (ln_pos[l] > 0) {		/* if in multicol */
			int ll;
			float pos;

			for (ll = l + 1; ll < ln_num; ll++) {
				if (ln_pos[ll] <= 0) {
					pos = ln_pos[ll];
					while (--ll >= l)
						ln_pos[ll] = pos;
					break;
				}
			}
		}
		dp = ln_pos[l] - p1;
		np = maxy + dp < 0 && !epsf;
		if (np) {
			newpage();
			if (ln_font[l] >= 0) {
				struct FONTSPEC *f;

				f = &cfmt.font_tb[ln_font[l]];
				output(fout, "%.1f F%d\n",
					f->size, f->fnum);
			}
		}
		if (ln_scale[l] != cur_scale) {
			output(fout, "%.3f dup scale\n",
				ln_scale[l] / cur_scale);
			cur_scale = ln_scale[l];
		}
		if (ln_lmarg[l] != cur_lmarg) {
			output(fout, "%.2f 0 T\n",
				(ln_lmarg[l] - cur_lmarg) / cur_scale);
			cur_lmarg = ln_lmarg[l];
		}
		if (np) {
			output(fout, "0 %.2f T\n", -cfmt.topspace);
			maxy -= cfmt.topspace * cfmt.scale;
		}
		if (*p_buf != '\001') {
			if (epsf > 1 || svg)
				svg_write(p_buf, ln_buf[l] - p_buf);
			else
				fwrite(p_buf, 1, ln_buf[l] - p_buf, fout);
		} else {			/* %%EPS - see parse.c */
			FILE *f;
			char line[BSIZE], *p, *q;

			p = strchr(p_buf + 1, '\n');
			fwrite(p_buf + 1, 1, p - p_buf, fout);
			p_buf = p + 1;
			p = strchr(p_buf, '%');
			*p++ = '\0';
			q = strchr(p, '\n');
			*q = '\0';
			if ((f = fopen(p, "r")) == NULL) {
				error(1, NULL, "Cannot open EPS file '%s'", p);
			} else {
				if (epsf > 1 || svg) {
					fprintf(fout, "<!--Begin document %s-->\n",
							p);
					svg_output(fout, "gsave\n"
							"%s T\n",
							p_buf);
					while (fgets(line, sizeof line, f))	/* copy the file */
						svg_write(line, strlen(line));
					svg_output(fout, "grestore\n"
							"%s T\n",
							p_buf);
					fprintf(fout, "<!--End document %s-->\n",
							p);
				} else {
					fprintf(fout,
						"save\n"
						"/showpage{}def/setpagedevice{pop}def\n"
						"%s T\n"
						"%%%%BeginDocument: %s\n",
						p_buf, p);
					while (fgets(line, sizeof line, f))	/* copy the file */
						fwrite(line, 1, strlen(line), fout);
					fprintf(fout, "%%%%EndDocument\n"
							"restore\n");
				}
				fclose(f);
			}
		}
		p_buf = ln_buf[l];
		maxy += dp;
		p1 = ln_pos[l];
	}
#if 1 //fixme:test
	if (*p_buf != '\0')
		fprintf(stderr, "??? bug - buffer not empty:\n%s\n", p_buf);
#endif
	outft = outft_sav;
	bposy = 0;
	ln_num = 0;
	mbf = outbuf;
}

/* -- add a block in the output buffer -- */
void block_put(void)
{
	if (mbf == outbuf)
		return;
//fixme: should be done sooner and should be adjusted when cfmt change...
	if (maxy == 0)
		maxy = (cfmt.landscape ? cfmt.pagewidth : cfmt.pageheight)
			- cfmt.topmargin - cfmt.botmargin;
	if (ln_num > 0 && mbf == ln_buf[ln_num - 1])
		return;				/* no data */
	if (ln_num >= BUFFLN) {
		char c, *p;

		error(1, NULL, "max number of buffer lines exceeded"
				" -- check BUFFLN");
		multicol_start = 0;
		p = ln_buf[ln_num - 1];
		c = *p;				/* (avoid "buffer not empty") */
		*p = '\0';
		write_buffer();
		multicol_start = maxy + bposy;
		*p = c;
		strcpy(outbuf, p);
		use_buffer = 0;
	}
	ln_buf[ln_num] = mbf;
	ln_pos[ln_num] = multicol_start == 0 ? bposy : 1;
	ln_lmarg[ln_num] = cfmt.leftmargin;
	if (epsf) {
		if (cfmt.leftmargin < min_lmarg)
			min_lmarg = cfmt.leftmargin;
		if (cfmt.rightmargin < max_rmarg)
			max_rmarg = cfmt.rightmargin;
	}
	ln_scale[ln_num] = cfmt.scale;
	ln_font[ln_num] = outft;
	ln_num++;

	if (!use_buffer) {
		write_buffer();
		return;
	}
}

/* -- handle completed block in buffer -- */
/* if the added stuff does not fit on current page, write it out
   after page break and change buffer handling mode to pass though */
void buffer_eob(void)
{
	block_put();
	if (maxy + bposy < 0
	 && !epsf
	 && multicol_start == 0) {
		if (in_page)
			newpage();
		write_buffer();
		use_buffer = 0;
	}
	if (epsf == 3)
		write_eps();		/* close the image */
}

/* -- dump buffer if not enough place for a music line -- */
void check_buffer(void)
{
	if (mbf + 5000 > outbuf + outbufsz) { /* assume music line < 5000 bytes */
		error(0, NULL,
		      "Possibly bad page breaks, outbufsz exceeded");
		write_buffer();
		use_buffer = 0;
	}
}

/* -- return the current vertical offset in the page -- */
float get_bposy(void)
{
	return maxy + bposy;
}
