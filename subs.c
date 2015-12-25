/*  
 *  This file is part of abc2ps, Copyright (C) 1996,1997 Michael Methfessel
 *  See file abc2ps.c for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <ctype.h>

#include "abcparse.h"
#include "abc2ps.h" 

#define MAXNTEXT	400	/* for text output */
#define MAXWLEN		21

static char outfnam[STRL1];	/* internal file name for open/close */
static char txt[MAXNTEXT][MAXWLEN];	/* for output of text */
int  ntxt;

/* width of characters according to the encoding */
/* these are the widths for Times-Roman, extracted from the 'a2ps' package */
static short ISOLatin1_w[256] = {
	  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	250,333,408,500,500,833,778,333,
	333,333,500,564,250,564,250,278,
	500,500,500,500,500,500,500,500,
	500,500,278,278,564,564,564,444,
	921,722,667,667,722,611,556,722,
	722,333,389,722,611,889,722,722,
	556,722,667,556,611,722,722,944,
	722,722,611,333,278,333,469,500,
	333,444,500,444,500,444,333,500,
	500,278,278,500,278,778,500,500,
	500,500,333,389,278,500,500,722,
	500,500,444,480,200,480,541,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	250,333,500,500,500,500,200,500,
	333,760,276,500,564,333,760,333,
	400,564,300,300,333,500,453,350,
	333,278,310,500,750,750,750,444,
	722,722,722,722,722,722,889,667,
	611,611,611,611,333,333,333,333,
	722,722,722,722,722,722,722,564,
	722,722,722,722,722,722,556,500,
	444,444,444,444,444,444,667,444,
	444,444,444,444,278,278,278,278,
	500,500,500,500,500,500,500,564,
	500,500,500,500,500,500,500,500,
};
static short ISOLatin2_w[256] = {
	  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	250,333,408,500,500,833,778,333,
	333,333,500,564,250,564,250,278,
	500,500,500,500,500,500,500,500,
	500,500,278,278,564,564,564,444,
	921,722,667,667,722,611,556,722,
	722,333,389,722,611,889,722,722,
	556,722,667,556,611,722,722,944,
	722,722,611,333,278,333,469,500,
	333,444,500,444,500,444,333,500,
	500,278,278,500,278,778,500,500,
	500,500,333,389,278,500,500,722,
	500,500,444,480,200,480,541,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	250,500,333,611,500,500,500,500,
	333,556,500,500,500,333,611,500,
	400,500,333,278,333,500,500,333,
	333,389,500,500,500,333,444,500,
	500,722,722,500,722,500,500,667,
	500,611,500,611,500,333,333,500,
	500,500,500,722,722,500,722,564,
	500,500,722,500,722,722,500,500,
	500,444,444,500,444,500,500,444,
	500,444,500,444,500,278,278,500,
	500,500,500,500,500,500,500,564,
	500,500,500,500,500,500,500,333,
};
static short ISOLatin3_w[256] = {
	  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	250,333,408,500,500,833,778,333,
	333,333,500,564,250,564,250,278,
	500,500,500,500,500,500,500,500,
	500,500,278,278,564,564,564,444,
	921,722,667,667,722,611,556,722,
	722,333,389,722,611,889,722,722,
	556,722,667,556,611,722,722,944,
	722,722,611,333,278,333,469,500,
	333,444,500,444,500,444,333,500,
	500,278,278,500,278,778,500,500,
	500,500,333,389,278,500,500,722,
	500,500,444,480,200,480,541,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	250,500,333,500,500,500,500,500,
	333,500,500,500,500,333,760,500,
	400,500,300,300,333,500,500,350,
	333,278,500,500,500,750,750,500,
	722,722,722,722,722,500,500,667,
	611,611,611,611,333,333,333,333,
	722,722,722,722,722,500,722,564,
	500,722,722,722,722,500,500,500,
	444,444,444,444,444,500,500,444,
	444,444,444,444,278,278,278,278,
	500,500,500,500,500,500,500,564,
	500,500,500,500,500,500,500,333,
};
static short ISOLatin4_w[256] = {
	  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	250,333,408,500,500,833,778,333,
	333,333,500,564,250,564,250,278,
	500,500,500,500,500,500,500,500,
	500,500,278,278,564,564,564,444,
	921,722,667,667,722,611,556,722,
	722,333,389,722,611,889,722,722,
	556,722,667,556,611,722,722,944,
	722,722,611,333,278,333,469,500,
	333,444,500,444,500,444,333,500,
	500,278,278,500,278,778,500,500,
	500,500,333,389,278,500,500,722,
	500,500,444,480,200,480,541,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	250,500,500,500,500,500,500,500,
	333,556,500,500,500,333,611,333,
	400,500,333,500,333,500,500,333,
	333,389,500,500,500,500,444,500,
	500,722,722,722,722,722,889,500,
	500,611,500,611,500,333,333,500,
	722,500,500,500,722,722,722,564,
	722,500,722,722,722,500,500,500,
	500,444,444,444,444,444,667,500,
	500,444,500,444,500,278,278,500,
	500,500,500,500,500,500,500,564,
	500,500,500,500,500,500,500,333,
};
static short ISOLatin5_w[256] = {
	  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	250,333,408,500,500,833,778,333,
	333,333,500,564,250,564,250,278,
	500,500,500,500,500,500,500,500,
	500,500,278,278,564,564,564,444,
	921,722,667,667,722,611,556,722,
	722,333,389,722,611,889,722,722,
	556,722,667,556,611,722,722,944,
	722,722,611,333,278,333,469,500,
	333,444,500,444,500,444,333,500,
	500,278,278,500,278,778,500,500,
	500,500,333,389,278,500,500,722,
	500,500,444,480,200,480,541,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	250,333,500,500,500,500,200,500,
	333,760,276,500,564,333,760,333,
	400,564,300,300,333,500,453,350,
	333,278,310,500,750,750,750,444,
	722,722,722,722,722,722,889,667,
	611,611,611,611,333,333,333,333,
	500,722,722,722,722,722,722,564,
	722,722,722,722,722,500,500,500,
	444,444,444,444,444,444,667,444,
	444,444,444,444,278,278,278,278,
	500,500,500,500,500,500,500,564,
	500,500,500,500,500,278,500,500,
};
static short ISOLatin6_w[256] = {
	  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	250,333,408,500,500,833,778,333,
	333,333,500,564,250,564,250,278,
	500,500,500,500,500,500,500,500,
	500,500,278,278,564,564,564,444,
	921,722,667,667,722,611,556,722,
	722,333,389,722,611,889,722,722,
	556,722,667,556,611,722,722,944,
	722,722,611,333,278,333,469,500,
	333,444,500,444,500,444,333,500,
	500,278,278,500,278,778,500,500,
	500,500,333,389,278,500,500,722,
	500,500,444,480,200,480,541,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,
	250,500,500,500,500,500,500,500,
	333,500,556,500,611,333,500,500,
	500,500,500,500,500,500,500,500,
	500,500,389,500,444,500,500,500,
	500,722,722,722,722,722,889,500,
	500,611,500,611,500,333,333,333,
	500,500,500,722,722,722,722,500,
	722,500,722,722,722,722,556,500,
	500,444,444,444,444,444,667,500,
	500,444,500,444,500,278,278,278,
	500,500,500,500,500,500,500,500,
	500,500,500,500,500,500,500,500,
};

static short *cw_tb[] = {
	ISOLatin1_w,	/* 0 = ascii */
	ISOLatin1_w,
	ISOLatin2_w,
	ISOLatin3_w,
	ISOLatin4_w,
	ISOLatin5_w,
	ISOLatin6_w
};

/* escaped character table */
/* adapted from the 'recode' package - first index is 128 + 32 */
static char ISOLatin1_c[] = 
	"NS!ICtPdCuYeBBSE':Co-a<<NO--Rg'-DG+-2S3S''MyPI.M',1S-o>>141234?I"
	"A!A'A>A?A:AAAEC,E!E'E>E:I!I'I>I:D-N?O!O'O>O?O:*XO/U!U'U>U:Y'THss"
	"a!a'a>a?a:aaaec,e!e'e>e:i!i'i>i:d-n?o!o'o>o?o:-:o/u!u'u>u:y'thy:";
static char ISOLatin2_c[] = 
	"NSA;'(L/CuL<S'SE':S<S,T<Z'--Z<Z.DGa;';l/''l<s''<',s<s,t<z''\"z<z."
	"R'A'A>A(A:L'C'C,C<E'E;E:E<I'I>D<D/N'N<O'O>O\"O:*XR<U0U'U\"U:Y'T,ss"
	"r'a'a>a(a:l'c'c,c<e'e;e:e<i'i>d<d/n'n<o'o>o\"o:-:r<u0u'u\"u:y't,'.";
static char ISOLatin3_c[] = 
	"NSH/'(PdCu  H>SE':I.S,G(J>--  Z.DGh/2S3S''Myh>.M',i.s,g(j>12  z."
	"A!A'A>  A:C.C>C,E!E'E>E:I!I'I>I:  N?O!O'O>G.O:*XG>U!U'U>U:U(S>ss"
	"a!a'a>  a:c.c>c,e!e'e>e:i!i'i>i:  n?o!o'o>g.o:-:g>u!u'u>u:u(s>'.";
static char ISOLatin4_c[] = 
	"NSA;kkR,CuI?L,SE':S<E-G,T/--Z<'-DGa;';r,''i?l,'<',s<e-g,t/NGz<ng"
	"A-A'A>A?A:AAAEI;C<E'E;E:E.I'I>I-D/N,O-K,O>O?O:*XO/U;U'U>U:U?U-ss"
	"a-a'a>a?a:aaaei;c<e'e;e:e.i'i>i-d/n,o-k,o>o?o:-:o/u;u'u>u:u?u-'.";
static char ISOLatin5_c[] = 
	"NS!ICtPdCuYeBBSE':Co-a<<NO--Rg'-DG+-2S3S''MyPI.M',1S-o>>141234?I"
	"A!A'A>A?A:AAAEC,E!E'E>E:I!I'I>I:G(N?O!O'O>O?O:*XO/U!U'U>U:I.S,ss"
	"a!a'a>a?a:aaaec,e!e'e;e:e.i'i>i-g(n?o!o'o>o?o:-:o/u!u'u>u:i.s,y:";
static char ISOLatin6_c[] = 
	"NSA;E-G,I-I?K,L,N'R,S<T/Z<--kkNGd/a;e-g,i-i?k,l,n'r,s<t/z<SEssng"
	"A-A'A>A?A:AAAEI;C<E'E;E:E.I'I>I:D/N,O-O'O>O?O:U?O/U;U'U>U:Y'THU-"
	"a-a'a>a?a:aaaei;c<e'e;e:e.i'i>i:d-n,o-o'o>o?o:u?o/u;u'u>u:y'thu-";
static char *esc_tb[] = {
	ISOLatin1_c,	/* 0 = ascii */
	ISOLatin1_c,
	ISOLatin2_c,
	ISOLatin3_c,
	ISOLatin4_c,
	ISOLatin5_c,
	ISOLatin6_c
};

struct text {
	struct text *next;
	float textw;
	char text[2];
} *text_tb[TEXT_MAX];

/*  miscellaneous subroutines  */

/* -- return the character width -- */
float cwid(char c)
{
	int ix = c & 0x00ff;
	return (float) cw_tb[cfmt.encoding][ix] / 1000.;
}

/* -- tex_str: change string taking care of some tex-style codes -- */
/* Puts \ in front of ( and ) in case brackets are not balanced,
   interprets all ISOLatin1..6 escape sequences as defined in rfc1345.
   Returns the length of the string as finally given out on paper.
   Also returns an estimate of the string width... */
int tex_str(char *d,
	    char *s,
	    int maxlen,
	    float *wid)
{
	int n;
	float w;
	char c1, c2;
	char *p_enc, *p;
	int i;

	n = 0;
	w = 0;
	maxlen--;		/* have room for EOS */
	p_enc = esc_tb[cfmt.encoding];
	while (*s != '\0') {
		switch (*s) {
		case '\\':			/* backslash sequences */
			s++;
			c1 = *s;
			c2 = s[1];
			if (c1 == '\0' || c2 == '\0')
				break;
			if (c1 == ' ') {
				if (--maxlen <= 0)
					break;
				*d++ = c1;
				n++;
				w += cwid(c1);
				break;
			}
			/* treat escape with octal value */
			if ((unsigned) (c1 - '0') <= 3
			    && (unsigned) (c2 - '0') <= 7) {
				if ((unsigned) (s[2] - '0') <= 7) {
					if ((maxlen -= 4) <= 0)
						break;
					*d++ = '\\';
					*d++ = c1;
					*d++ = c2;
					*d++ = s[2];
					n++;
					c1 = ((c1 - '0') << 6) + ((c2 - '0') << 3) + s[2] - '0';
					w += cwid(c1);
					s += 2;
					break;
				}
			}
			/* convert to rfc1345 */
			switch (c1) {
			case '`': c1 = '!'; break;
			case '^': c1 = '>'; break;
			case '~': c1 = '?'; break;
			case '"': c1 = ':'; break;
			/* special TeX sequences */
			case 'O': c1 = '/'; c2 = 'O'; s--; break;
			case 'o': c1 = '/'; c2 = 'o'; s--; break;
			case 'c': if (c2 == 'c' || c2 == 'C')
					c1 = ',';
				break;
			}
			switch (c2) {
			case '`': c2 = '!'; break;
			case '^': c2 = '>'; break;
			case '~': c2 = '?'; break;
			case '"': c2 = ':'; break;
			}
			for (i = 32 * 3, p = p_enc; --i >= 0; p += 2) {
				if ((*p == c1 && p[1] == c2)
				    || (*p == c2 && p[1] == c1)) {
					s++;
					c1 = (p - p_enc) / 2 + 128 + 32;
					break;
				}
			}
			/* fixme: bad hack for multi-line guitar chord*/
			if (c1 == 'n') {
				if (--maxlen <= 0)
					break;
				*d++ = '\\';
			}
			if (--maxlen <= 0)
				break;
			*d++ = c1;
			n++;
			w += cwid(c1);
			break;
		case '{':
		case '}':
			break;
		case '(':
		case ')':			/* ( ) becomes \( \) */
			if (--maxlen <= 0)
				break;
			*d++ = '\\';
			/* fall thru */
		default:		/* other characters: pass though */
			if (--maxlen <= 0)
				break;
			*d++ = *s;
			n++;
			w += cwid(*s);
		}
		s++;
	}
	*d = '\0';
	*wid = w;
	return n;
}

/* -- output a string in postscript -- */
static void put_str(char *str)
{
	char s[801];
	float w;

	tex_str(s, str, sizeof s, &w);
	PUT1("%s", s);
}

/* -- output a string in postscript with head and tail -- */
void put_str3(char *head,
	      char *str,
	      char *tail)
{
	PUT0(head);
	put_str(str);
	PUT0(tail);
}

/* -- set_font -- */
void set_font(struct FONTSPEC *font)
{
	int fnum;

	for (fnum = nfontnames; --fnum >= 0; ) {
		if (!strcmp(font->name, fontnames[fnum]))
			break;
	}
	if (fnum < 0) {
		ERROR(("Font \"%s\" not predefined; using first in list",
			font->name));
		fnum = 0;
	}
	PUT2("%.1f F%d ", font->size, fnum);
}

/* -- set_font_str -- */
static void set_font_str(char str[],
			 struct FONTSPEC *font)
{
	int fnum;

	for (fnum = nfontnames; --fnum >= 0; ) {
		if (!strcmp(font->name, fontnames[fnum]))
			break;
	}
	sprintf(str, "%.1f F%d ", font->size, fnum);
}

/* -- epsf_title -- */
void epsf_title(char title[],
		char fnm[])
{
	char *p,*q;

	p = title;
	q = fnm;
	while (*p != '\0') {
		if (*p == ' ')
			*q++ = '_';
		else	*q++ = *p;
		p++;
	}
	*q = '\0';
}

/* -- close_output_file -- */
void close_output_file()
{
	long m;

	if (fout == 0)
		return;

	close_page(fout);
	close_ps(fout);
	m = ftell(fout);
	fclose(fout);
	if (tunenum == 0)
		ERROR(("Warning: no tunes written to output file"));
	printf("Output written on %s (%d page%s, %d title%s, %ld byte%s)\n",
		outfnam,
		pagenum, pagenum == 1 ? "" : "s",
		tunenum, tunenum == 1 ? "" : "s",
		m, m == 1 ? "" : "s");
	fout = 0;
	file_initialized = 0;
}

/* -- open_output_file -- */
void open_output_file(char *fnam)
{
	if (strcmp(fnam, outfnam) == 0)
		return;

	if (fout != 0)
		close_output_file();

	strcpy(outfnam, fnam);
	if ((fout = fopen(outfnam, "w")) == 0) {
		printf("Cannot open output file %s\n", outf);
		exit(1);
	}
	pagenum = 0;
	tunenum = 0;
	file_initialized = 0;
}

/* -- add_to_text_block -- */
void add_to_text_block(char *ln,
		       int add_final_nl)
{
	char *c, *a;
	char word[MAXWLEN];
	int nt, nl, nc;

	nt = ntxt;
	c = ln;

	for (;;) {
		while (*c == ' ')
			c++;
		if (*c == '\0')
			break;
		a = word;
		nl = 0;
		nc = MAXWLEN;
		while (*c != ' ' && *c != '\0' && *c != '\n') {
			if (*c == '\\' && c[1] == '\\') {
				nl = 1;
				c += 2;
				break;
			}
			if (--nc > 0)
				*a++ = *c++;
			else	c++;
		}
		*a = '\0';
		if (nc <= 0) {
			ERROR(("Insanely long word truncated to %d chars: %s",
			       MAXWLEN-1, word));
		}
		if (nt >= MAXNTEXT) {
			ERROR(("'%s'\n"
			       "Text overflow; increase MAXNTEXT and recompile.",
			       ln));
			exit(1);
		}
		if (word[0] != '\0') {
			strcpy(txt[nt], word);
			nt++;
		}
		if (nl) {
			strcpy(txt[nt], "$$NL$$");
			nt++;
		}
	}
	if (add_final_nl) {
		strcpy(txt[nt], "$$NL$$");
		nt++;
	}
	ntxt = nt;
}

/* -- write_text_block -- */
void write_text_block(FILE *fp,
		      int  job,
		      int abc_state)
{
	int i, i1, i2, ntline, nc, mc, nbreak;
	float textwidth, ftline, ftline0, swfac, baseskip, parskip;
	float wwidth, wtot,spw;
	char str[81];

	if (ntxt <= 0)
		return;

	baseskip = cfmt.textfont.size * cfmt.lineskipfac;
	parskip = cfmt.textfont.size * cfmt.parskipfac;
	set_font_str(page_init, &cfmt.textfont);

	/* estimate text widths.. ok for T-R, wild guess for other fonts */
	swfac = cfmt.textfont.swfac;
	spw = cwid(' ');
	PUT1("/LF {0 %.1f rmoveto} bind def\n", -baseskip);

	/* output by pieces, separate at newline token */
	i1 = 0;
	while (i1 < ntxt) {
		i2 = -1;
		for (i = i1; i < ntxt; i++)
			if (!strcmp(txt[i], "$$NL$$")) {
				i2 = i;
				break;
			}
		if (i2 < 0)
			i2 = ntxt;
		bskip(baseskip);

		if (job == OBEYLINES) {
			PUT0("0 0 M (");
			for (i = i1; i < i2; i++) {
				tex_str(str, txt[i], sizeof str, &wwidth);
				PUT1("%s ", str);
			}
			PUT0(") show\n");
		} else if (job == OBEYCENTER) {
			PUT1("%.1f 0 M (", cfmt.staffwidth/2);
			for (i = i1; i < i2; i++) {
				tex_str(str, txt[i], sizeof str, &wwidth);
				PUT1("%s", str);
				if (i < i2 - 1)
					PUT0(" ");
			}
			PUT0(") cshow\n");
		} else {
			PUT0("0 0 M mark\n");
			nc = 0;
			mc = -1;
			wtot = -spw;
			for (i = i2 - 1; i >= i1; i--) {
				mc += tex_str(str, txt[i], sizeof str, &wwidth)+1;
				wtot += wwidth + spw;
				nc += strlen(str) + 2;
				if (nc >= 72) {
					nc = 0;
					PUT0("\n");
				}
				PUT1 ("(%s)", str);
			}
			PUT2(" %.1f P%d\n", cfmt.staffwidth,
			     job == RAGGED ? 1 : 2);
			/* first estimate: (total textwidth)/(available width) */
			textwidth = wtot * swfac * cfmt.textfont.size;
			if (strstr(cfmt.textfont.name, "Courier"))
				textwidth = 0.60 * mc * cfmt.textfont.size;
			ftline0 = textwidth / cfmt.staffwidth;
			/* revised estimate: assume some chars lost at each line end */
			nbreak = ftline0;
			textwidth += 5 * nbreak * cwid('a') * swfac * cfmt.textfont.size;
			ftline = textwidth / cfmt.staffwidth;
			ntline = ftline + 1.0;
#ifdef DEBUG
			if (verbose >= 10) {
				printf("first estimate %.2f, revised %.2f\n",
				       ftline0, ftline);
				printf("Output %d word%s, about %.2f lines (fac %.2f)\n",
				       i2 - i1, i2 - i1 == 1 ? "" : "s",
				       ftline, swfac);
			}
#endif
			bskip((ntline - 1) * baseskip);
		}

		buffer_eob(fp);
		/* next line to allow pagebreak after each text "line" */
		/* if (!epsf && !within_tune) write_buffer(fp); */
		i1 = i2 + 1;
	}
	bskip(parskip);
	buffer_eob(fp);
	/* next line to allow pagebreak after each paragraph */
	if (!epsf && abc_state != ABC_S_TUNE)
		write_buffer(fp);
	page_init[0] = '\0';
}

/* -- clear_text -- */
void clear_text(void)
{
	int i;

	for (i = TEXT_MAX; --i >= 0;)
		text_tb[i] = 0;
}

/* -- add_text -- */
void add_text(char *s,
	      int type)
{
	struct text *t, *r;

#if 1
	t = (struct text *) getarena(sizeof (struct text) - 2
				     + strlen(s) + 1);
	strcpy(t->text, s);
	t->textw = cwid('a') * strlen(s);
#else
	char b[256];
	float w;

	tex_str(b, s, sizeof b, &w);
	t = (struct text *) getarena(sizeof (struct text) - 2
				     + strlen(b) + 1);
	strcpy(t->text, b);
	t->textw = w;
#endif
	t->next = 0;
	if ((r = text_tb[type]) == 0)
		text_tb[type] = t;
	else {
		while (r->next != 0)
			r = r->next;
		r->next = t;
	}
}

/* -- put_words -- */
void put_words(FILE *fp)
{
	char str[81];
	unsigned char *p, *q;
	struct text *t, *u, *t_end;
	int n;
	float middle, max2col;

	if ((u = text_tb[TEXT_W]) == 0)
		return;

	set_font(&cfmt.wordsfont);
	set_font_str(page_init, &cfmt.wordsfont);

	/* see if we may have 2 columns */
	middle = 0.5 * cfmt.staffwidth;
	max2col = (middle - 45.) / (cfmt.wordsfont.swfac * cfmt.wordsfont.size);
	n = 0;
	for (t = u; t != 0; t = t->next) {
		if (t->textw > max2col) {
			n = 0;
			break;
		}
		if (t->text[0] == '\0')
			n++;
	}
	if (n > 0) {
		int i;

		n++;
		n /= 2;
		i = n;
		for (;;) {
			if (u->text[0] == '\0'
			    && --i <= 0)
				break;
			u = u->next;
		}
		t_end = u;
		u = u->next;
	} else {
		t_end = 0;
		u = 0;
	}

	/* output the text */
	bskip(cfmt.wordsspace);
	for (t = text_tb[TEXT_W]; t != 0 || u != 0;) {
		bskip(cfmt.lineskipfac * cfmt.wordsfont.size);
		if (t != 0) {
			p = t->text;
			q = str;
			if (isdigit(*p)) {
				while (*p != '\0') {
					*q++ = *p++;
					if (*p == ' '
					    || *(p - 1) == ':'
					    || *(p - 1) == '.')
						break;
				}
				if (*p == ' ')
					p++;
			}
			*q = '\0';

			/* permit page break at empty lines or stanza start */
			if (*p == '\0' || str[0] != '\0')
				buffer_eob(fp);
			if (str[0] != '\0')
				put_str3("45 0 M (",
					 str,
					 ") lshow\n");
			if (*p != '\0')
				put_str3("50 0 M (",
					p,
					") show\n");
			t = t->next;
			if (t == t_end)
				t = 0;
		}
		if (u != 0) {
			p = u->text;
			q = str;
			if (isdigit(*p)) {
				while (*p != '\0') {
					*q++ = *p++;
					if (*p == ' '
					    || *(p - 1) == ':'
					    || *(p - 1) == '.')
						break;
				}
				if (*p == ' ')
					p++;
			}
			*q = '\0';
			if (str[0] != '\0') {
				PUT1("%.2f 0 M (",
				     20. + middle);
				put_str(str);
				PUT0(") lshow\n");
			}
			if (*p != '\0') {
				PUT1("%.2f 0 M (",
				     25. + middle);
				put_str(p);
				PUT0(") show\n");
			}
			if (u->text[0] == '\0') {
				if (--n == 0) {
					if (t != 0)
						n++;
					else	middle *= 0.7;
				}
			}
			u = u->next;
		}
	}

	buffer_eob(fp);
	page_init[0] = '\0';
}

/* -- put_text -- */
void put_text(FILE *fp,
	      int type,
	      char str[])
{
	struct text *t;

	if ((t = text_tb[type]) == 0)
		return;

	PUT0("0 0 M\n");
	ntxt = 0;
	add_to_text_block(str, 0);
	while (t != 0) {
		add_to_text_block(t->text, 1);
		t = t->next;
	}
	write_text_block(fp, RAGGED, ABC_S_HEAD);
	buffer_eob(fp);
}

/* -- put_history -- */
void put_history(FILE *fp)
{
	struct text *t;
	float baseskip,parskip;

	set_font(&cfmt.textfont);
	set_font_str(page_init, &cfmt.textfont);
	baseskip = cfmt.textfont.size * cfmt.lineskipfac;
	parskip = cfmt.textfont.size * cfmt.parskipfac;

	bskip(cfmt.textspace);

	if (info.rhyth) {
		bskip(baseskip);
		put_str3("0 0 M (Rhythm: ",
			 info.rhyth,
			 ") show\n");
		bskip(parskip);
	}

	if (info.book) {
		bskip(0.5 * CM);
		put_str3("0 0 M (Book: ",
			 info.book,
			 ") show\n");
		bskip(parskip);
	}

	if (info.src) {
		bskip(0.5 * CM);
		put_str3("0 0 M (Source: ",
			 info.src,
			 ") show\n");
		bskip(parskip);
	}

	put_text(fp, TEXT_D, "Discography: ");
	put_text(fp, TEXT_N, "Notes: ");
	put_text(fp, TEXT_Z, "Transcription: ");

	if ((t = text_tb[TEXT_H]) != 0) {
		while (t != 0) {
			bskip(0.5 * CM);
			put_str3("0 0 M (",
				 t->text,
				 ") show\n");
			t = t->next;
		}
		bskip(parskip);
	}
	buffer_eob(fp);
	page_init[0] = '\0';
}

/* -- write_inside_title -- */
void write_inside_title(void)
{
	char t[201];

	bskip(cfmt.subtitlefont.size + 0.2 * CM);
	set_font(&cfmt.subtitlefont);

	strcpy(t, info.title[info.ntitle - 1]);
#ifdef DEBUG
	if (verbose > 15)
		printf("write inside title <%s>\n", t);
#endif

	if (cfmt.titlecaps)
		cap_str(t);
	PUT0(" (");
	put_str(t);
	if (cfmt.titleleft)
		PUT0(") 0 0 M show\n");
	else	PUT1(") %.1f 0 M cshow\n", cfmt.staffwidth / 2);
	bskip(cfmt.musicspace + 0.2 * CM);
}

/* -- write_heading -- */
void write_heading(void)
{
	float lwidth, down1, down2;
	int i, ncl;
	char t[201];

	lwidth = cfmt.staffwidth;

	/* write the titles */
	for (i = 0; i < info.ntitle; i++) {
		if (i == 0) {
			bskip(cfmt.titlespace + cfmt.titlefont.size);
			set_font(&cfmt.titlefont);
		} else {
			bskip(cfmt.subtitlespace + cfmt.subtitlefont.size);
			set_font(&cfmt.subtitlefont);
		}
		PUT0("(");
		if (i == 0 && cfmt.withxrefs)
			PUT1("%s. ", info.xref);
		strcpy(t, info.title[i]);
		if (cfmt.titlecaps)
			cap_str(t);
		put_str(t);
		if (cfmt.titleleft)
			PUT0(") 0 0 M show\n");
		else	PUT1(") %.1f 0 M cshow\n", lwidth / 2);
	}

	/* write composer, origin */
	if (info.ncomp > 0 || info.orig) {
		set_font(&cfmt.composerfont);
		bskip(cfmt.composerspace);
		ncl = info.ncomp;
		if (info.orig && ncl < 1)
			ncl = 1;
		for (i = 0; i < ncl; i++) {
			bskip(cfmt.composerfont.size);
			PUT1("%.1f 0 M (", lwidth);
			if (info.comp[i])
				put_str(info.comp[i]);
			if (info.orig && i == ncl - 1)
				put_str3(" (",
					 info.orig,
					 ")");
			PUT0(") lshow\n");
		}
		down1 = cfmt.composerspace + cfmt.musicspace
			+ ncl * cfmt.composerfont.size;
	} else {
		bskip(cfmt.composerfont.size + cfmt.composerspace);
		down1 = cfmt.composerspace + cfmt.musicspace + cfmt.composerfont.size;
	}
	bskip(cfmt.musicspace);

	/* decide whether we need extra shift for parts */
	down2 = cfmt.composerspace + cfmt.musicspace;
	if (info.parts)
		down2 += cfmt.partsspace + cfmt.partsfont.size;
	if (down2 > down1)
		bskip(down2 - down1);

	/* write parts */
	if (info.parts) {
		bskip(-cfmt.partsspace);
		set_font(&cfmt.partsfont);
		put_str3("0 0 M (",
			 info.parts,
			 ") show\n");
		bskip(cfmt.partsspace);
	}
}

/* -- write_parts -- */
void write_parts(void)
{
	if (!info.parts)
		return;
	bskip(cfmt.partsfont.size);
	set_font(&cfmt.partsfont);
	PUT0("0 0 M (");
	put_str(info.parts);
	PUT1(") show%s\n", cfmt.partsbox ? "b" : "");
	bskip(cfmt.partsspace);
}
