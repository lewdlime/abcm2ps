/*  
 *  This file is part of abc2ps, Copyright (C) 1996,1997 Michael Methfessel
 *  See file abc2ps.c for details.
 */

#include <stdio.h>
#include <math.h>
#include <time.h>
#include <string.h>

#include "abcparse.h"
#include "abc2ps.h" 

#define BUFFLN	100		/* max number of lines in output buffer */

static int ln_num;		/* number of lines in buffer */
static float ln_pos[BUFFLN];	/* vertical positions of buffered lines */
static int ln_buf[BUFFLN];	/* buffer location of buffered lines */
static char buf[BUFFSZ];	/* output buffer.. should hold one tune */
static float posx;		/* current left margin */

char *mbf;			/* where to PUTx() */
int nbuf;			/* number of bytes buffered */
float bposy;			/* current position in buffered data */
int use_buffer;			/* 1 if lines are being accumulated */
 
/*  subroutines for postscript output  */

/* -- init_ps -- */
void init_ps(FILE *fp,
	     char str[],
	     int  is_epsf,
	     float bx1,
	     float by1,
	     float bx2,
	     float by2)
{
	time_t ltime;
	char tstr[41];
	int i;

	if (is_epsf) {
#ifdef DEBUG
		if (verbose >= 8)
			printf("Open EPS file with title \"%s\"\n", str);
#endif
		fprintf(fp, "%%!PS-Adobe-3.0 EPSF-3.0\n"
			"%%%%BoundingBox: %.0f %.0f %.0f %.0f\n",
			bx1,by1,bx2,by2);
	} else {
#ifdef DEBUG
		if (verbose >= 8)
			printf("Open PS file with title \"%s\"\n", str);
#endif
		fprintf(fp, "%%!PS-Adobe-3.0\n");
	}

	/* Title */
	fprintf(fp, "%%%%Title: %s\n", str);

	/* CreationDate */
	time(&ltime);
	strcpy(tstr, ctime(&ltime));
	tstr[24]='\0';
	tstr[16]='\0';
	fprintf(fp, "%%%%Creator: abcm2ps " VERSION "\n"
		"%%%%CreationDate: %s %s\n", tstr + 4, tstr + 20);

#if PS_LEVEL == 2
	fprintf(fp, "%%%%LanguageLevel: 2\n");
#endif
	fprintf(fp, "%%%%EndComments\n\n");

	if (is_epsf)
		fprintf(fp, "gsave /origstate save def mark\n100 dict begin\n\n");

	fprintf(fp, "%%%%BeginSetup\n");
#if PS_LEVEL < 2
	fprintf(fp, "/selectfont { exch findfont exch dup   %% emulate level 2 op\n"
		"	type /arraytype eq {makefont}{scalefont} ifelse setfont\n"
		"} bind def\n");
#endif
	define_encoding(fp, cfmt.encoding);
#ifdef DEBUG
	if (verbose >= 7)
		printf("\nDefining ISO-Latin%d fonts in file header:\n",
			cfmt.encoding ? cfmt.encoding : 1);
#endif
	for (i = 0; i < nfontnames; i++) {
		define_font(fp, fontnames[i], i, cfmt.encoding);
#ifdef DEBUG
		if (verbose >= 7)
			printf("   F%d	 %s\n", i, fontnames[i]);
#endif
	}
	define_symbols(fp);
	fprintf(fp, "\n0 setlinecap 0 setlinejoin 0.8 setlinewidth\n"
		"\n/T {translate} bind def\n/M {moveto} bind def\n"
		"%%%%EndSetup\n");
	file_initialized = 1;
}

/* -- close_ps -- */
void close_ps(FILE *fp)
{
#ifdef DEBUG
	if (verbose >= 8)
		printf("closing PS file\n");
#endif
	fprintf(fp, "%%EOF\n\n");
}

/* -- initialize postscript page -- */
void init_page(FILE *fp)
{
#ifdef DEBUG
	if (verbose >= 10)
		printf("init_page called; in_page=%d\n", in_page);
#endif
	if (in_page)
		return;

	if (!file_initialized) {
#ifdef DEBUG
		if (verbose >= 10)
			printf("file not yet initialized; do it now\n");
#endif
		init_ps(fp, in_fname, 0, 0.0, 0.0, 0.0, 0.0);
	}
	in_page = 1;
	pagenum++;

#ifdef DEBUG
	if (verbose == 0)
		;
	else if (verbose <= 2)
		printf("[%d] ", pagenum);
	else	printf("[%d]\n", pagenum);
	fflush(stdout);
#endif
	fprintf(fp, "\n%% --- page %d\n"
		"%%%%Page: %d %d\n"
		"%%%%BeginPageSetup\n",
		pagenum, pagenum, pagenum);

	if (cfmt.landscape)
		fprintf(fp,"%%%%PageOrientation: Landscape\n");
	fprintf(fp,"gsave ");
	if (cfmt.landscape)
		fprintf(fp,"90 rotate 0 %.1f translate ",
			-cfmt.pageheight);
	fprintf(fp,"%.2f %.2f translate\n",
		cfmt.leftmargin, cfmt.pageheight-cfmt.topmargin);
	fprintf(fp, "%%%%EndPageSetup\n");

	/* write page number */
	if (pagenumbers) {
		fprintf(fp, "/Times-Roman 12 selectfont ");
#if 1
		/* page numbers always at right */
		fprintf(fp, "%.1f %.1f moveto (%d) lshow\n",
			cfmt.staffwidth, cfmt.topmargin-30.0, pagenum);

#else
		/* page number right/left for odd/even pages */
		if (pagenum%2==0)
			fprintf(fp, "%.1f %.1f moveto (%d) show\n",
				0.0, cfmt.topmargin-30.0, pagenum);
		else	fprintf(fp, "%.1f %.1f moveto (%d) lshow\n",
				cfmt.staffwidth, cfmt.topmargin-30.0, pagenum);
#endif
	}
}

/* -- close_page -- */
void close_page(FILE *fp)
{
#ifdef DEBUG
	if (verbose >= 10)
		printf("close_page called; in_page=%d\n", in_page);
#endif

	if (!in_page)
		return;
	in_page = 0;

	fprintf(fp, "\n%%%%PageTrailer\n"
		"grestore\n"
		"showpage\n");
}

/* -- initialize epsf file -- */
void init_epsf(FILE *fp)
{
	fprintf(fp, "%.2f %.2f translate\n",
		cfmt.leftmargin,
		cfmt.pageheight - cfmt.topmargin);
}

/* -- close epsf file -- */
void close_epsf(FILE *fp)
{
	fprintf(fp, "\nshowpage\nend\n"
		"cleartomark origstate restore grestore\n\n");
}

/* -- write_pagebreak -- */
void write_pagebreak(FILE *fp)
{
	close_page(fp);
	init_page(fp);
	if (page_init[0] != '\0')
		fprintf(fp, "%s\n", page_init);
	posy = cfmt.pageheight - cfmt.topmargin;
}

/*  subroutines to handle output buffer  */

/* -- update the output buffer pointer -- */
/* called from the PUTx() macros */
void a2b(void)
{
	int l;

	l = strlen(mbf);
	/*  printf ("Append %d <%s>\n", l, t); */

	nbuf += l;
	if (nbuf >= BUFFSZ - 500) {	/* must have place for 1 more line */
		ERROR(("a2b: buffer full, BUFFSZ=%d", BUFFSZ));
		exit(1);
	}

	mbf += l;
}

/* -- translate down by h points in output buffer -- */
void bskip(float h)
{
	if (h > 0.01) {
		PUT1("0 %.1f T\n", -h);
		bposy -= h;
	}
}

/* -- do horizontal shift if needed -- */
void check_margin(float new_posx)
{
	float dif;

	dif = new_posx - posx;
	if (dif * dif < 0.01)
		return;

	PUT1("%.2f 0 T\n", dif);
	posx = new_posx;
}

/* -- initialize page dimensions -- */
void init_pdims()
{
	posx = cfmt.leftmargin;
	posy = cfmt.pageheight - cfmt.topmargin;
}

/* -- clear_buffer -- */
void clear_buffer()
{
	nbuf = 0;
	bposy = 0.0;
	ln_num = 0;
	mbf = buf;
}

/* -- write buffer contents, break at full pages -- */
void write_buffer(FILE *fp)
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
		if (posy + dp < cfmt.botmargin && !epsf)
			write_pagebreak(fp);
		fwrite(&buf[i], 1, b2 - i, fp);
		i = b2;
		posy += dp;
		p1 = ln_pos[l];
	}

	fwrite(&buf[i], 1, nbuf - i, fp);

	clear_buffer();
}

/* -- handle completed block in buffer -- */
/* if the added stuff does not fit on current page, write it out
   after page break and change buffer handling mode to pass though */
void buffer_eob(FILE *fp)
{
	if (ln_num >= BUFFLN) {
		ERROR(("max number of buffer lines exceeded"
			" -- check BUFFLN"));
		exit(1);
	}

	ln_buf[ln_num] = nbuf;
	ln_pos[ln_num] = bposy;
	ln_num++;

	if (!use_buffer) {
		write_buffer(fp);
		return;
	}

	if ((posy + bposy < cfmt.botmargin
	     || cfmt.oneperpage)
	    && !epsf) {
		if (tunenum != 1)
			write_pagebreak(fp);
		write_buffer(fp);
		use_buffer = 0;
	}
}

/* -- dump buffer if less than nb bytes available -- */
void check_buffer(FILE *fp,
		  int nb)
{
	if (nbuf + nb > BUFFSZ) {
		ERROR(("possibly bad page breaks, BUFFSZ exceeded at line %d",
		      ln_num));
		write_buffer(fp);
		use_buffer = 0;
	}
}
