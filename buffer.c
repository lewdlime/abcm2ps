/*
 * Postscript buffering functions.
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
#include <time.h>
#include <string.h>
#include <ctype.h>

#include "abcparse.h"
#include "abc2ps.h" 

#define BUFFLN	80		/* max number of lines in output buffer */

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
static struct FORMAT *p_fmt;	/* current format while treating a new page */

char *mbf;			/* where to PUTx() */
int nbuf;			/* number of bytes buffered */
int use_buffer;			/* 1 if lines are being accumulated */

/* -- convert a date -- */
static void cnv_date(time_t *ltime)
{
	char buf[TEX_BUF_SZ];

	tex_str(p_fmt->dateformat);
	strcpy(buf, tex_buf);
	strftime(tex_buf, TEX_BUF_SZ, buf, localtime(ltime));
}

/* -- initialize the postscript file -- */
static void init_ps(char *str, int is_epsf)
{
	time_t ltime;
	int i;

	if (is_epsf) {
/*fixme: no landscape for EPS?*/
		fprintf(fout, "%%!PS-Adobe-3.0 EPSF-3.0\n"
			"%%%%BoundingBox: 0 0 %.0f %.0f\n",
			p_fmt->pagewidth - p_fmt->leftmargin
				- p_fmt->rightmargin + 20,
			-bposy);
		cur_lmarg = p_fmt->leftmargin - 10;
	} else	fprintf(fout, "%%!PS-Adobe-%d.0\n",
			p_fmt->pslevel);

	fprintf(fout, "%%%%Title: %s\n", str);

	/* CreationDate */
	time(&ltime);
	strftime(tex_buf, TEX_BUF_SZ, "%b %e, %Y %H:%M", localtime(&ltime));
	fprintf(fout, "%%%%Creator: abcm2ps-" VERSION "\n"
		"%%%%CreationDate: %s\n", tex_buf);
	if (!is_epsf)
		fprintf(fout, "%%%%Pages: (atend)\n");
	fprintf(fout, "%%%%LanguageLevel: %d\n"
		"%%%%EndComments\n"
		"%%CommandLine:",
		p_fmt->pslevel);
	for (i = 1; i < s_argc; i++) {
		fprintf(fout,
			strchr(s_argv[i], ' ') != 0 ? " \'%s\'" : " %s",
			s_argv[i]);
	}
	fprintf(fout, "\n\n");

	if (is_epsf)
		fprintf(fout,
			"gsave /origstate save def mark\n"
			"100 dict begin\n");

	fprintf(fout, "%%%%BeginSetup\n"
		"/!{bind def}bind def\n"
		"/bdef{bind def}!\n"		/* for compatibility */
		"/T/translate load def\n"
		"/M/moveto load def\n"
		"/RM/rmoveto load def\n"
		"/RL/rlineto load def\n"
		"/RC/rcurveto load def\n"
		"/SLW/setlinewidth load def\n"
		"/defl 0 def\n"	/* decoration flags - see deco.c for values */
		"/dlw{0.7 SLW}!\n");
	if (p_fmt->pslevel < 2)
		/* (simple!) level2 emulation */
	    fprintf(fout, 
		"/cshow{exch dup{fh fh 4 index exec}forall pop pop}!\n"
/*		"/glyphshow{currentfont/Encoding .knownget not{{}} if\n" */
		"/glyphshow{currentfont/Encoding get\n"
		"   0 1 2 index length 1 sub{\n"	/* stack: glyph encoding index */
		"      2 copy get 3 index eq{exch pop exch pop null exit}if\n"
		"      pop\n"
		"    }\n"
		"   for null eq{(X) dup 0 4 -1 roll put show}{pop}ifelse\n"
		"}!\n"
		"/rectstroke{\n"
		"	4 2 roll M 1 index 0 RL 0 exch RL neg 0 RL closepath\n"
		"	stroke}!\n"
		"/rectfill{\n"
		"	4 2 roll M 1 index 0 RL 0 exch RL neg 0 RL closepath\n"
		"	fill}!\n"
		"/selectfont{exch findfont exch dup\n"
		"	type/arraytype eq{makefont}{scalefont}ifelse setfont}!\n");
	define_symbols();
	user_ps_write();
	define_fonts();
	fprintf(fout, "%%%%EndSetup\n");
	file_initialized = 1;
}

/* -- close_output_file -- */
void close_output_file(void)
{
	long m;

	if (fout == 0)
		return;
	if (multicol_start != 0) {	/* if no '%%multocol end' or filtering */
		multicol_start = 0;
		write_buffer();
	}
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
			"Output written on %s (%d page%s, %d title%s, %ld bytes)\n",
			outfnam,
			nbpages, nbpages == 1 ? "" : "s",
			tunenum, tunenum == 1 ? "" : "s",
			m);
	}
	fout = 0;
	file_initialized = 0;
	nbpages = tunenum = 0;
	defl = 0;
}

/* -- close the PS page -- */
void close_page(void)
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

/* -- output a header/footer element -- */
static void format_hf(char *p)
{
	char *q;
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
		case 'd':
			ltime = mtime;
			goto dput;
		case 'D':
			time(&ltime);
		dput:	cnv_date(&ltime);
			fprintf(fout, "%s", tex_buf);
			break;
		case 'F':		/* ABC file name */
#if DIRSEP!='\\'
			fprintf(fout, "%s", in_fname);
#else
			{
				int i;
				char *r;

				q = in_fname;
				r = tex_buf;
				i = TEX_BUF_SZ;
				for (;;) {
					if (--i <= 0 || *q == '\0')
						break;
					if ((*r++ = *q++) == '\\') {
						i--;
						*r++ = '\\';
					}
				}
				*r = '\0';
			}
			fprintf(fout, "%s", tex_buf);
#endif
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
			q = &info.title->as.text[2];
			while (isspace((unsigned char) *q))
				q++;
			tex_str(q);
			fprintf(fout, "%s", tex_buf);
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
	char str[512];
	char *p, *q, *r;
	float size, y, wsize;
	int fnum, fi_sav;
	struct FONTSPEC *f, f_sav;

	memcpy(&f_sav, &cfmt.font_tb[0], sizeof f_sav);
	fi_sav = strcf;

	if (header) {
		p = cfmt.header;
		f = &cfmt.headerfont;
		size = f->size;
		y = 2.;
	} else {
		p = cfmt.footer;
		f = &cfmt.footerfont;
		size = f->size;
		y = - (pheight - p_fmt->topmargin - p_fmt->botmargin)
			- size + 2.;
	}
	wsize = 0;
	str_font(f);
	fnum = f->fnum;
	fprintf(fout, "%.1f F%d ", size, fnum);

	/* may have 2 lines */
	if ((r = strstr(p, "\\n")) != 0) {
		if (!header)
			y += size;
		wsize += size;
		*r = '\0';
	}

	for (;;) {
		tex_str(p);
		strcpy(str, tex_buf);

		/* left side */
		p = str;
		if ((q = strchr(p, '\t')) != 0) {
			if (q != p) {
				*q = '\0';
				fprintf(fout, "%.1f %.1f M(",
					p_fmt->leftmargin, y);
				format_hf(p);
				fprintf(fout, ")show\n");
			}
			p = q + 1;
		}
		if ((q = strchr(p, '\t')) != 0)
			*q = '\0';

		/* center */
		if (q != p) {
			fprintf(fout, "%.1f %.1f M(",
				pwidth * 0.5, y);
			format_hf(p);
			fprintf(fout, ")showc\n");
		}

		/* right side */
		if (q != 0) {
			p = q + 1;
			if (*p != '\0') {
				fprintf(fout, "%.1f %.1f M(",
					pwidth - p_fmt->rightmargin, y);
				format_hf(p);
				fprintf(fout, ")showr\n");
			}
		}
		if (r == 0)
			break;
		*r = '\\';
		p = r + 2;
		r = 0;
		y -= size;
	}

	memcpy(&cfmt.font_tb[0], &f_sav, sizeof cfmt.font_tb[0]);
	strcf = fi_sav;

	return wsize;
}

/* -- initialize postscript page -- */
static void init_page(void)
{
	float pheight, pwidth;

	if (in_page)
		return;

	p_fmt = info.xref == 0 ? &cfmt : &dfmt;	/* global format */

	if (!file_initialized)
		init_ps(in_fname, 0);
	in_page = 1;
	nbpages++;

	fprintf(fout, "%%%%Page: %d %d\n",
		nbpages, nbpages);
	if (p_fmt->landscape) {
		pheight = p_fmt->pagewidth;
		pwidth = p_fmt->pageheight;
		fprintf(fout, "%%%%PageOrientation: Landscape\n"
			"gsave 90 rotate 0 %.1f T\n",
			-p_fmt->topmargin);
	} else {
		pheight = p_fmt->pageheight;
		pwidth = p_fmt->pagewidth;
		fprintf(fout, "gsave 0 %.1f T\n",
			pheight - p_fmt->topmargin);
	}

	posy = pheight - p_fmt->topmargin - p_fmt->botmargin;

	/* output the header and footer */
	if (cfmt.header == 0) {
		char *p = 0;

		switch (pagenumbers) {
		case 1: p = "$P\t"; break;
		case 2: p = "\t\t$P"; break;
		case 3: p = "$P0\t\t$P1"; break;
		case 4: p = "$P1\t\t$P0"; break;
		}
		if (p != 0)
			cfmt.header = strdup(p);
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
	if (font_init != 0)
		fprintf(fout,"%.1f F%d ", font_init->size, font_init->fnum);
}

/* -- open the output file -- */
void open_output_file(void)
{
	int i;
	char fnm[STRL1];

	strcpy(fnm, outf);
	i = strlen(fnm) - 1;
	if (i < 0)
		strcpy(fnm, OUTPUTFILE);
	else if (i == 0 && fnm[0] == '-')
		;
	else {
		if (fnm[i] == '=') {
			char *p;

			if ((p = strrchr(in_fname, DIRSEP)) == 0)
				p = in_fname;
			else	p++;
/*fixme: should check if there is a DIRSEP at the end of fnm*/
			strcpy(&fnm[i], p);
			strext(fnm, "ps");
		} else if (fnm[i] == DIRSEP)
			strcpy(&fnm[i + 1], OUTPUTFILE);
#if 0
/*fixme: fnm may be a directory*/
		else	...
#endif
	}
	if (strcmp(fnm, outfnam) == 0)
		return;

	close_output_file();
	strcpy(outfnam, fnm);
	if (i != 0 || fnm[0] != '-') {
		if ((fout = fopen(fnm, "w")) == 0) {
			fprintf(stderr, "Cannot create output file %s\n", fnm);
			exit(2);
		}
	} else	fout = stdout;
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
	int i;
	long m;
	char *p, fnm[STRL1], finf[STRL1];

	p_fmt = info.xref == 0 ? &cfmt : &dfmt;	/* global format */
	close_output_file();
	strcpy(fnm, outf);
	if (fnm[0] == '\0')
		strcpy(fnm, OUTPUTFILE);
	cutext(fnm);
	i = strlen(fnm) - 1;
	if (fnm[i] == '=') {
		p = &info.title->as.text[2];
		while (isspace((unsigned char) *p))
			p++;
		strncpy(&fnm[i], p, sizeof fnm - i - 4);
		fnm[sizeof fnm - 5] = '\0';
		epsf_title(&fnm[i]);
	} else	sprintf(&fnm[i + 1], "%03d", ++nepsf);
	strcat(fnm, ".eps");
	if ((fout = fopen(fnm, "w")) == 0) {
		fprintf(stderr, "Cannot open output file %s\n", fnm);
		exit(2);
	}
	sprintf(finf, "%.72s (%.4s)", in_fname, info.xref);
	init_ps(finf, 1);
	fprintf(fout, "0 %.1f T\n", -bposy);
	write_buffer();
	fprintf(fout, "showpage\nend\n"
		"cleartomark origstate restore grestore\n");
	m = ftell(fout);
	fclose(fout);
	fout = 0;
	fprintf(stderr, "Output written on %s (%ld bytes)\n",
		fnm, m);
	cur_lmarg = 0;
	cur_scale = 1.0;
}

/* -- start a new page -- */
static void newpage(void)
{
	close_page();
	init_page();
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
	bposy -= h * cfmt.scale;
	PUT1("0 %.2f T\n", -h);
}

/* -- clear_buffer -- */
void clear_buffer(void)
{
	nbuf = 0;
	bposy = 0;
	ln_num = 0;
	mbf = buf;
}

/* -- write buffer contents, break at full pages -- */
void write_buffer(void)
{
	int i, l, b2, np;
	float p1, dp;

	if (nbuf == 0 || multicol_start != 0)
		return;
	i = 0;
	p1 = 0;
	for (l = 0; l < ln_num; l++) {
		b2 = ln_buf[l];
		dp = ln_pos[l] - p1;
		np = posy + dp < 0 && !epsf;
		if (np)
			newpage();
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
		if (np) {
			fprintf(fout, "0 %.1f T\n", -cfmt.topspace);
			posy -= cfmt.topspace * cfmt.scale;
		}
		if (buf[i] != '\001')
			fwrite(&buf[i], 1, b2 - i, fout);
		else {				/* %%EPS - see parse.c */
			FILE *f;
			char line[BSIZE], *p;
			float x1, y1;

			i++;
			p = strchr(&buf[i], '\n');
			fwrite(&buf[i], 1, p - &buf[i] + 1, fout);
			sscanf(p + 1, "%f %f %s\n", &x1, &y1, line);
			if ((f = fopen(line, "r")) == 0) {
				error(1, 0, "Cannot open EPS file '%s'", line);
			} else {
				fprintf(fout,
					"%% ----- EPS file '%s' -----\n"
					"save\n"
					"/showpage{}def/setpagedevice{pop}def\n"
					"%.2f %.2f T\n", 
					line, -x1, -y1);
				while (fgets(line, sizeof line, f))	/* copy the file */
					fwrite(line, 1, strlen(line), fout);
				fclose(f);
				strcpy(line, "restore\n"
						"% ----- end EPS -----\n");
				fwrite(line, 1, strlen(line), fout);
			}
		}
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
	if (bposy == 0 && nbuf == 0)
		return;
	if (ln_num > 0 && ln_buf[ln_num - 1] == nbuf)
		return;
	if (ln_num >= BUFFLN) {
		error(1, 0, "max number of buffer lines exceeded"
			" -- check BUFFLN");
		write_buffer();
		use_buffer = 0;
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
	if (posy + bposy < 0 && !epsf && multicol_start == 0) {
		if (tunenum > 1)
			newpage();
		write_buffer();
		use_buffer = 0;
	}
}

/* -- dump buffer if not enough place for a music line -- */
void check_buffer(void)
{
	if (nbuf > BUFFSZ - 5000) {	/* assume music line < 5000 bytes */
		error(0, 0,
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
