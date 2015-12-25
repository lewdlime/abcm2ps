/*
 * Postscript buffering functions.
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
#include <time.h>
#include <string.h>

#include "abcparse.h"
#include "abc2ps.h" 

#define BUFFLN	100		/* max number of lines in output buffer */

static int ln_num;		/* number of lines in buffer */
static float ln_pos[BUFFLN];	/* vertical positions of buffered lines */
static int ln_buf[BUFFLN];	/* buffer location of buffered lines */
static float ln_lmarg[BUFFLN];	/* left margin of buffered lines */
static float ln_scale[BUFFLN];	/* scale of buffered lines */
static char buf[BUFFSZ];	/* output buffer.. should hold one tune */
static float cur_lmarg = 0;	/* current left margin */
static float cur_scale = 1.0;	/* current scale */
static float posy;		/* vertical position on page */
static float bposy;		/* current position in buffered data */
static int nepsf;		/* counter for epsf output files */
static int nbpages;		/* number of pages in the output file */
static char outfnam[STRL1];	/* internal file name for open/close */

char *mbf;			/* where to PUTx() */
int nbuf;			/* number of bytes buffered */
int use_buffer;			/* 1 if lines are being accumulated */

/*  subroutines for postscript output  */

/* -- initialize the postscript file -- */
static void init_ps(char *str,
		    int  is_epsf)
{
	time_t ltime;
	char tstr[41];
	int enc, i;

	if (is_epsf) {
/*fixme: no landscape for EPS?*/
		fprintf(fout, "%%!PS-Adobe-3.0 EPSF-3.0\n"
			"%%%%BoundingBox: 0 0 %.0f %.0f\n",
			cfmt.pagewidth - cfmt.leftmargin
				- cfmt.rightmargin + 20,
			-bposy);
		cur_lmarg = cfmt.leftmargin - 10;
	} else	fprintf(fout, "%%!PS-Adobe-3.0\n");

	/* Title */
	fprintf(fout, "%%%%Title: %s\n", str);

	/* CreationDate */
	time(&ltime);
	strcpy(tstr, ctime(&ltime));
/* Wed Jun 30 21:49:08 1993\n */
	tstr[10]='\0';
	tstr[16]='\0';
	tstr[24]='\0';
	fprintf(fout, "%%%%Creator: abcm2ps-" VERSION "\n"
		"%%%%CreationDate: %s, %s %s\n", &tstr[4], &tstr[20], &tstr[11]);
	if (!is_epsf)
		fprintf(fout, "%%%%Pages: (atend)\n");
	fprintf(fout, "%%%%LanguageLevel: %d\n"
		"%%%%EndComments\n"
		"%%CommandLine:",
		PS_LEVEL);
	for (i = 1; i < s_argc; i++) {
		fprintf(fout,
			strchr(s_argv[i], ' ') != 0 ? " \'%s\'" : " %s",
			s_argv[i]);
	}
	fprintf(fout, "\n\n");

	if (is_epsf)
		fprintf(fout,
			"gsave /origstate save def mark\n100 dict begin\n\n");

	fprintf(fout, "%%%%BeginSetup\n"
		"/!{bind def}bind def\n"
		"/bdef{bind def}!\n"		/* for compatibility */
		"/T/translate load def\n"
		"/M/moveto load def\n"
		"/RM/rmoveto load def\n"
		"/RL/rlineto load def\n"
		"/RC/rcurveto load def\n"
		"/dlw{0.7 setlinewidth}!\n"
#if PS_LEVEL==1
		"/selectfont{exch findfont exch dup	%% emulate level 2 op\n"
		"	type /arraytype eq {makefont}{scalefont} ifelse setfont}!\n"
#endif
		);
	define_encoding(cfmt.encoding);
	if ((enc = cfmt.encoding) == 0)
		enc = 1;
	fprintf(fout, "/mkfontext{\n"
		"	findfont dup length dict begin\n"
                "		{1 index /FID ne {def}{pop pop} ifelse} forall\n"
		"		/Encoding ISOLatin%dEncoding def\n"
		"		currentdict\n"
		"	end\n"
		"	definefont pop}!\n",
		enc);
	define_fonts();
	define_symbols();
	fprintf(fout, "0 setlinecap 0 setlinejoin\n");

	write_user_ps();
	fprintf(fout, "%%%%EndSetup\n");
	file_initialized = 1;
}

/* -- close_page -- */
static void close_page(void)
{
	if (!in_page)
		return;
	in_page = 0;

	fprintf(fout, "%%%%PageTrailer\n"
		"grestore\n"
		"showpage\n");
	cur_lmarg = 0;
	cur_scale = 1.0;
}

/* -- close_output_file -- */
void close_output_file(void)
{
	long m;

	if (fout == 0)
		return;
	if (tunenum == 0)
		error(0, 0, "No tunes written to output file");
	close_page();
	fprintf(fout, "%%%%Trailer\n"
		"%%%%Pages: %d\n"
		"%%EOF\n", nbpages);
	if (fout != stdout) {
		m = ftell(fout);
		fclose(fout);
		fprintf(stderr,
			"Output written on %s (%d page%s, %d title%s, %ld byte%s)\n",
			outfnam,
			nbpages, nbpages == 1 ? "" : "s",
			tunenum, tunenum == 1 ? "" : "s",
			m, m == 1 ? "" : "s");
	}
	fout = 0;
	file_initialized = 0;
	nbpages = tunenum = 0;
}

/* -- output a header/footer element -- */
static void format_hf(char *p)
{
	char *q;
	char s[256];
	time_t ltime;

	for (;;) {
		if (*p == '\0')
			break;
		if ((q = strchr(p, '$')) != 0)
			*q = '\0';
		fprintf(fout, "%s", p);
		if (q == 0)
			break;
		p = q + 1;
		switch (*p) {
		case 'D':
			time(&ltime);
			strcpy(s, ctime(&ltime));
/* Wed Jun 30 21:49:08 1993\n */
			s[10]='\0';
			s[16]='\0';
			s[24]='\0';
			fprintf(fout, "%s, %s %s", &s[4], &s[20], &s[11]);
			break;
		case 'F':		/* ABC file name */
			fprintf(fout, "%s", in_fname);
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
			fprintf(fout, "%d", pagenum);
			break;
		case 'T':		/* tune title */
			tex_str(s, info.title[0], sizeof s, 0);
			fprintf(fout, "%s", s);
			break;
		case 'V':
			fprintf(fout,"abcm2ps-"  VERSION);
			break;
		default:
			continue;
		}
		p++;
	}
}

/* -- output the header or footer -- */
static float headfooter(int header,
			float pwidth,
			float pheight)
{
	char str[256];
	char *p, *q, *r;
	float size, y, wsize;
	int fnum;

	if (header) {
		p = cfmt.header;
		size = cfmt.headerfont.size;
		fnum = cfmt.headerfont.fnum;
		y = 2.;
		wsize = 0;
	} else {
		p = cfmt.footer;
		size = cfmt.footerfont.size;
		fnum = cfmt.footerfont.fnum;
		y = - (pheight - cfmt.topmargin - cfmt.botmargin)
			- size + 2.;
		wsize = 0;
	}

	fprintf(fout, "%.1f F%d ", size, fnum);

	/* may have 2 lines */
	if ((r = strstr(p, "\\n")) != 0) {
		if (!header)
			y += size;
		wsize += size;
		*r = '\0';
	}

	for (;;) {
		tex_str(str, p, sizeof str, 0);

		/* left side */
		p = str;
		if ((q = strchr(p, '\t')) != 0) {
			if (q != p) {
				*q = '\0';
				fprintf(fout, "%.1f %.1f M (",
					cfmt.leftmargin, y);
				format_hf(p);
				fprintf(fout, ") show\n");
			}
			p = q + 1;
		}
		if ((q = strchr(p, '\t')) != 0)
			*q = '\0';
		if (q != p) {
			fprintf(fout, "%.1f %.1f M (",
				pwidth * 0.5, y);
			format_hf(p);
			fprintf(fout, ") cshow\n");
		}
		if (q != 0) {
			p = q + 1;
			if (*p != '\0') {
				fprintf(fout, "%.1f %.1f M (",
					pwidth - cfmt.rightmargin, y);
				format_hf(p);
				fprintf(fout, ") lshow\n");
			}
		}
		if (r == 0)
			break;
		*r = '\\';
		p = r + 2;
		r = 0;
		y -= size;
	}

	return wsize;
}

/* -- initialize postscript page -- */
static void init_page(void)
{
	float pheight, pwidth;

	if (in_page)
		return;

	if (!file_initialized)
		init_ps(in_fname, 0);
	in_page = 1;
	nbpages++;

	fprintf(fout, "%%%%Page: %d %d\n",
		nbpages, nbpages);
	if (cfmt.landscape) {
		pheight = cfmt.pagewidth;
		pwidth = cfmt.pageheight;
		fprintf(fout, "%%%%PageOrientation: Landscape\n"
			"gsave 90 rotate 0 %.1f T\n",
			-cfmt.topmargin);
	} else {
		pheight = cfmt.pageheight;
		pwidth = cfmt.pagewidth;
		fprintf(fout, "gsave 0 %.1f T\n",
			pheight - cfmt.topmargin);
	}

	posy = pheight - cfmt.topmargin - cfmt.botmargin;

	/* output the header and footer */
	if (cfmt.header == 0 && pagenumbers != 0) {
		switch (pagenumbers) {
		case 1: cfmt.header = "$P\t"; break;
		case 2: cfmt.header = "\t\t$P"; break;
		case 3: cfmt.header = "$P0\t\t$P1"; break;
		case 4: cfmt.header = "$P1\t\t$P0"; break;
		}
	}
	if (cfmt.header != 0) {
		float dy;

		dy = headfooter(1, pwidth, pheight);
		if (dy != 0) {
			fprintf(fout, "0 %.1f T\n", -dy);
			posy -= dy;
		}
	}
	if (cfmt.footer != 0)
		posy -= headfooter(0, pwidth, pheight);
	pagenum++;
}

/* -- open the output file -- */
void open_output_file(void)
{
	if (strcmp(outf, outfnam) == 0)
		return;

	if (fout != 0)
		close_output_file();

	strcpy(outfnam, outf);
	if (outf[0] != '-' || outf[1] != '\0') {
		if ((fout = fopen(outf, "w")) == 0) {
			fprintf(stderr, "Cannot create output file %s\n", outf);
			exit(2);
		}
	} else	fout = stdout;
}

/* -- epsf_title -- */
static void epsf_title(char *p)
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

/* -- output the EPS file -- */
void write_eps(void)
{
	char fnm[80], finf[80];

	close_output_file();
	if (choose_outname) {
		strncpy(fnm, info.title[0], sizeof fnm - 4);
		fnm[sizeof fnm - 5] = '\0';
		epsf_title(fnm);
		strcat(fnm, ".eps");
	} else	sprintf(fnm, "%.70s%03d.eps", outf, ++nepsf);
	sprintf(finf, "%.72s (%.4s)", in_fname, info.xref);
	if ((fout = fopen(fnm, "w")) == 0) {
		fprintf(stderr, "Cannot open output file %s\n", fnm);
		exit(2);
	}
	init_ps(finf, 1);
	fprintf(fout, "0 %.1f T\n", -bposy);
	write_buffer();
	fprintf(fout, "showpage\nend\n"
		"cleartomark origstate restore grestore\n");
	fclose(fout);
	fout = 0;
	cur_lmarg = 0;
	cur_scale = 1.0;
}

/* -- write_pagebreak -- */
void write_pagebreak()
{
	close_page();
	init_page();
	if (page_init[0] != '\0')
		fprintf(fout, "%s\n", page_init);
}

/*  subroutines to handle output buffer  */

/* -- update the output buffer pointer -- */
/* called from the PUTx() macros */
void a2b(void)
{
	int l;

	if (!in_page && !epsf)
		init_page();

	l = strlen(mbf);

	nbuf += l;
	if (nbuf >= BUFFSZ - 500) {	/* must have place for 1 more line */
		error(1, 0, "a2b: buffer full, BUFFSZ=%d", BUFFSZ);
		exit(3);
	}

	mbf += l;
}

/* -- translate down by 'h' absolute points in output buffer -- */
void abskip(float h)
{
	PUT1("0 %.2f T\n", -h / cfmt.scale);
	bposy -= h;
}

/* -- translate down by 'h' scaled points in output buffer -- */
void bskip(float h)
{
	PUT1("0 %.2f T\n", -h);
	bposy -= h * cfmt.scale;
}

/* -- clear_buffer -- */
void clear_buffer(void)
{
	nbuf = 0;
	bposy = 0.0;
	ln_num = 0;
	mbf = buf;
}

/* -- write buffer contents, break at full pages -- */
void write_buffer(void)
{
	int i, l, b2;
	float p1, dp;

	if (nbuf == 0)
		return;

	i = 0;
	p1 = 0;
	for (l = 0; l < ln_num; l++) {
		b2 = ln_buf[l];
		dp = ln_pos[l] - p1;
		if (posy + dp < 0 && !epsf)
			write_pagebreak();
		if (ln_scale[l] != cur_scale) {
			fprintf(fout, "%.2f dup scale\n",
				ln_scale[l] / cur_scale);
			cur_scale = ln_scale[l];
		}
		if (ln_lmarg[l] != cur_lmarg) {
			fprintf(fout, "%.1f 0 T\n",
				(ln_lmarg[l] - cur_lmarg) / cur_scale);
			cur_lmarg = ln_lmarg[l];
		}
		fwrite(&buf[i], 1, b2 - i, fout);
		i = b2;
		posy += dp;
		p1 = ln_pos[l];
	}

	fwrite(&buf[i], 1, nbuf - i, fout);

	clear_buffer();
}

/* -- handle completed block in buffer -- */
/* if the added stuff does not fit on current page, write it out
   after page break and change buffer handling mode to pass though */
void buffer_eob(void)
{
	if (ln_num >= BUFFLN) {
		error(1, 0, "max number of buffer lines exceeded"
			" -- check BUFFLN");
		exit(3);
	}

	ln_buf[ln_num] = nbuf;
	ln_pos[ln_num] = bposy;
	ln_lmarg[ln_num] = cfmt.leftmargin;
	ln_scale[ln_num] = cfmt.scale;
	ln_num++;

	if (!use_buffer) {
		write_buffer();
		return;
	}

	if ((posy + bposy < 0
	     || cfmt.oneperpage)
	    && !epsf) {
		if (tunenum != 1)
			write_pagebreak();
		write_buffer();
		use_buffer = 0;
	}
}

/* -- dump buffer if less than nb bytes available -- */
void check_buffer(void)
{
	if (nbuf + BUFFSZ1 > BUFFSZ) {
		error(0, ln_num,
		      "Possibly bad page breaks, BUFFSZ exceeded");
		write_buffer();
		use_buffer = 0;
	}
}

/* -- return the current vertical offset in the page -- */
float get_bposy(void)
{
	return posy + bposy;
}

/* -- set value in the output buffer -- */
/* The values are flagged as "\x01vnnn.nn" where
 * - 'v' is the variable index ('0'..'z')
 * - 'nnn.nn' is the value to add to the variable
 * The variables are:
 * '0'..'O': staff offsets */
/* this function must be called before buffer_eob() */
void set_buffer(float *p_v)
{
	char *p;
	int i;
	float v;

	if (ln_num == 0)
		p = buf;
	else	p = &buf[ln_buf[ln_num - 1]];
	while ((p = strchr(p, '\x01')) != 0) {
		i = p[1] - '0';
		sscanf(p + 2, "%f", &v);
		p += sprintf(p, "%7.1f", p_v[i] + v);
		*p = ' ';
	}
}
