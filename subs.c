/*
 * Low-level utilities.
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
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <ctype.h>

#include "abcparse.h"
#include "abc2ps.h" 

static float twidth;		/* text width for %%begintext..%%endtext */

/* width of characters according to the encoding */
/* these are the widths for Times-Roman, extracted from the 'a2ps' package */
static short ISOLatin1_w[256] = {
	  0,  0,  0,  0,  0,  0,  0,  0, /* \002: hyphen in lyrics */
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
	  0,500,500,500,  0,  0,  0,  0, /* \201..\203: sharp, flat and natural signs */
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
static short ISOLatin2_w[96] = {
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
static short ISOLatin3_w[96] = {
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
static short ISOLatin4_w[96] = {
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
static short ISOLatin5_w[96] = {
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
static short ISOLatin6_w[96] = {
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
	ISOLatin2_w - 160,
	ISOLatin3_w - 160,
	ISOLatin4_w - 160,
	ISOLatin5_w - 160,
	ISOLatin6_w - 160
};

/* escaped character table */
/* adapted from the 'recode' package - first index is 128 + 32 */
static char ISOLatin1_c[] =
	"NS!!CtPdCuYeBBSE':Co-a<<NO--Rg'-DG+-2S3S''MyPI.M',1S-o>>141234?I"
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
	"NS!!CtPdCuYeBBSE':Co-a<<NO--Rg'-DG+-2S3S''MyPI.M',1S-o>>141234?I"
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

static struct text {
	struct text *next;
	float textw;
	char text[2];
} *text_tb[TEXT_MAX];

/* -- print message for internal error and maybe stop -- */
void bug(char *msg,
	 int fatal)
{
	error(1, 0, "This cannot happen!\n"
	       "Internal error: %s.\n", msg);
	if (fatal) {
		fprintf(stderr, "Emergency stop.\n\n");
		exit(3);
	}
	fprintf(stderr, "Trying to continue...\n");
}

/* -- print an error message -- */
void error(int sev,	/* 0: warning, 1: error */
	   int linenum,
	   char *fmt, ...)
{
	va_list args;
static char *t;

	if (t != info.title[0]) {
		t = info.title[0];
		fprintf(stderr, "   - In tune `%s':\n", t);
	}
	fprintf(stderr,
		"%s in line %d: ", sev == 0 ? "Warning" : "Error", linenum);
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
}

/* -- return random float between x1 and x2 -- */
float ranf(float x1,
	   float x2)
{
static int first = 1;

	if (first) {
		srand(time(0));
		first = 0;
	}
	return x1 + (x2 - x1) * (float) (rand() & 0x7fff) / 32768.;
}

/* -- read a number with a unit -- */
float scan_u(char *str)
{
	float a;
	int nch;

	if (sscanf(str, "%f%n", &a, &nch) == 1) {
		if (str[nch] == '\0' || str[nch] == ' ')
			return a * PT;
		if (!strncasecmp(str + nch, "cm", 2))
			return a * CM;
		if (!strncasecmp(str + nch, "in", 2))
			return a * IN;
		if (!strncasecmp(str + nch, "pt", 2))
			return a * PT;
	}
	error(1, 0, "\n++++ Unknown unit value \"%s\"", str);
	return 20 * PT;
}

/* -- capitalize a string -- */
void cap_str(unsigned char *p)
{
	while (*p != '\0') {
#if 1
/* pb with toupper - works with ASCII only */
		unsigned char c;

		c = *p;
		if ((c >= 'a' && c <= 'z')
		    || (c >= 0xe0 && c <= 0xfe))
			*p = c & ~0x20;
#else
		*p = toupper(*p);
#endif
		p++;
	}
}

/*  miscellaneous subroutines  */

/* -- return the character width -- */
float cwid(unsigned char c)
{
	short *w;

	if (c < 160)		/* (0xa0) */
		w = ISOLatin1_w;
	else	w = cw_tb[cfmt.encoding];
	return (float) w[c] / 1000.;
}

/* -- change string taking care of some tex-style codes -- */
/* Puts \ in front of ( and ) in case brackets are not balanced,
 * interprets all ISOLatin1..6 escape sequences as defined in rfc1345.
 * Returns the length of the string as finally given out on paper.
 * Also returns an estimate of the string width... */
void tex_str(char *d,
	     char *s,
	     int maxlen,
	     float *wid)
{
	float w;
	char c1, c2;
	char *p_enc, *p;
	int i;

	w = 0;
	maxlen--;		/* have room for EOS */
	p_enc = esc_tb[cfmt.encoding];
	while ((c1 = *s) != '\0') {
		switch (c1) {
		case '\\':			/* backslash sequences */
			s++;
			if ((c1 = *s) == '\0')
				break;
			if (c1 == ' ')
				goto addchar1;
			if (c1 == '\\' || (c2 = s[1]) == '\0') {
				if (--maxlen <= 0)
					break;
				*d++ = '\\';
				goto addchar1;
			}
			/* treat escape with octal value */
			if ((unsigned) (c1 - '0') <= 3
			    && (unsigned) (c2 - '0') <= 7
			    && (unsigned) (s[2] - '0') <= 7) {
				if ((maxlen -= 4) <= 0)
					break;
				*d++ = '\\';
				*d++ = c1;
				*d++ = c2;
				*d++ = s[2];
				c1 = ((c1 - '0') << 6) + ((c2 - '0') << 3) + s[2] - '0';
				w += cwid(c1);
				s += 2;
				break;
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
			goto addchar1;
		case '{':
		case '}':
			break;
		case '(':
		case ')':			/* ( ) becomes \( \) */
			if (--maxlen <= 0)
				break;
			*d++ = '\\';
			/* fall thru */
		default:		/* other characters: pass through */
		addchar1:
			if (--maxlen <= 0)
				break;
			*d++ = c1;
			w += cwid(c1);
		}
		s++;
	}
	*d = '\0';
	if (wid)
		*wid = w;
}

/* -- output a string in postscript -- */
static void put_str(char *str)
{
	char s[801];

	tex_str(s, str, sizeof s, 0);
	PUT1("%s", s);
}

/* -- output a string in postscript with head and tail -- */
static void put_str3(char *head,
		     char *str,
		     char *tail)
{
	PUT0(head);
	put_str(str);
	PUT0(tail);
}

/* -- set font and memorize in case of page break -- */
static void set_font_init(struct FONTSPEC *font)
{
	set_font(font);
	sprintf(page_init, "%.1f F%d ", font->size, font->fnum);
}

/* -- add_to_text_block -- */
void add_to_text_block(char *s,
		       int job)
{
	float lw;
	char buf[256];

	tex_str(buf, s, sizeof buf, &lw);

	/* if first line, set the fonts */
	if (twidth == 0)
		set_font_init(&cfmt.textfont);

	/* follow lines */
	if (job == OBEYLINES || job == OBEYCENTER) {
		bskip(cfmt.textfont.size * cfmt.lineskipfac);
		if (buf[0] != '\0') {
			if (job == OBEYLINES)
				PUT1("0 0 M (%s) show\n", buf);
			else	{
				PUT2("%.1f 0 M (%s) cshow\n",
				     ((cfmt.landscape ? cfmt.pageheight : cfmt.pagewidth)
					- cfmt.leftmargin - cfmt.rightmargin)
					* 0.5 / cfmt.scale,
				     buf);
			}
		}
		buffer_eob();
		twidth += lw;
		return;
	}

	/* fill or justify lines */
	if (twidth == 0) {		/* if first line */
		float baseskip;

		baseskip = cfmt.textfont.size * cfmt.lineskipfac;
		PUT1("/LF {0 %.1f RM}!\n", -baseskip);
		bskip(baseskip);
		PUT0("0 0 M (");
	}
	PUT1("%s ", buf);
	twidth += lw;
}

/* -- write_text_block -- */
void write_text_block(int job,
		      int abc_state)
{
	if (twidth == 0)
		return;

	if (job == T_FILL || job == T_JUSTIFY) {
		int ntline, nbreak;
		float textwidth, ftline, swfac, baseskip;
		float lwidth;

		baseskip = cfmt.textfont.size * cfmt.lineskipfac;
		lwidth = ((cfmt.landscape ? cfmt.pageheight : cfmt.pagewidth)
			- cfmt.leftmargin - cfmt.rightmargin) / cfmt.scale;

		/* estimate text widths.. ok for T-R, wild guess for other fonts */
		swfac = cfmt.textfont.swfac;

		PUT2(") %.1f P%d\n",
		     lwidth, job == T_FILL ? 1 : 2);

		/* estimate the skip:
		 * 1- (total textwidth)/(available width) */
		textwidth = twidth * swfac * cfmt.textfont.size;
		ftline = textwidth / lwidth;
		/* 2- assume some chars lost at each line end */
		nbreak = ftline;
		textwidth += 5 * nbreak * cwid('a') * swfac * cfmt.textfont.size;
		ftline = textwidth / lwidth;
		ntline = ftline + 1.0;
		bskip((ntline - 1) * baseskip);
	}
	bskip(cfmt.textfont.size * cfmt.parskipfac);
	buffer_eob();

	/* next line to allow pagebreak after each paragraph */
	if (!epsf && abc_state != ABC_S_TUNE && multicol_start == 0)
		write_buffer();
	page_init[0] = '\0';

	twidth = 0;
}

/* -- clear_text -- */
void clear_text(void)
{
	int i;

	for (i = TEXT_MAX; --i >= 0;) {
		if (i != TEXT_PS)
			text_tb[i] = 0;
	}
}

/* -- add_text -- */
void add_text(char *s,
	      int type)
{
	struct text *t, *r;

	t = (struct text *) getarena(sizeof (struct text) - 2
				     + strlen(s) + 1);
	strcpy(t->text, s);
	t->textw = cwid('a') * strlen(s);
	t->next = 0;
	if ((r = text_tb[type]) == 0)
		text_tb[type] = t;
	else {
		while (r->next != 0)
			r = r->next;
		r->next = t;
	}
}

/* -- output a line of words after tune -- */
static void put_wline(unsigned char *p,
		      float x,
		      int right)
{
	unsigned char *q, *r, sep;

	r = 0;
	q = p;
	if (isdigit(*p) || p[1] == '.') {
		while (*p != '\0') {
			p++;
			if (*p == ' '
			    || p[-1] == ':'
			    || p[-1] == '.')
				break;
		}
		r = p;
		while (*p == ' ')
			p++;
	}

	/* on the left side, permit page break at empty lines or stanza start */
	if (!right
	   && (*p == '\0' || r != 0))
		buffer_eob();

	if (r != 0) {
		sep = *r;
		*r = '\0';
		PUT1("%.2f 0 M(", x);
		put_str(q);
		PUT0(")lshow\n");
		*r = sep;
	}
	if (*p != '\0') {
		PUT1("%.2f 0 M(", x + 5);
		put_str(p);
		PUT0(")show\n");
	}
}

/* -- put_words -- */
void put_words(void)
{
	struct text *t, *u, *t_end;
	int n, have_text;
	float middle, max2col;

	if ((u = text_tb[TEXT_W]) == 0)
		return;

	set_font_init(&cfmt.wordsfont);

	/* see if we may have 2 columns */
	middle = 0.5 * ((cfmt.landscape ? cfmt.pageheight : cfmt.pagewidth)
		- cfmt.leftmargin - cfmt.rightmargin) / cfmt.scale;
	max2col = (middle - 45.) / (cfmt.wordsfont.swfac * cfmt.wordsfont.size);
	n = 0;
	have_text = 0;
	for (t = u; t != 0; t = t->next) {
		if (t->textw > max2col) {
			n = 0;
			break;
		}
		if (t->text[0] == '\0') {
			if (have_text) {
				n++;
				have_text = 0;
			}
		} else	have_text = 1;
	}
	if (n > 0) {
		int i;

		n++;
		n /= 2;
		i = n;
		have_text = 0;
		for (;;) {
			if (u->text[0] == '\0') {
				if (have_text
				    && --i <= 0)
					break;
				have_text = 0;
			} else	have_text = 1;
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
			put_wline(t->text, 45., 0);
			t = t->next;
			if (t == t_end)
				t = 0;
		}
		if (u != 0) {
			put_wline(u->text, 20. + middle, 1);
			if (u->text[0] == '\0') {
				if (--n == 0) {
					if (t != 0)
						n++;
					else if (u->next != 0) {

						/* center the last words */
/*fixme: should compute the width average.. */
						middle *= 0.6;
					}
				}
			}
			u = u->next;
		}
	}

	buffer_eob();
	page_init[0] = '\0';
}

/* -- put_text -- */
static void put_text(int type,
		     char *str)
{
	struct text *t;
	char buf[256];

	if ((t = text_tb[type]) == 0)
		return;

	tex_str(buf, t->text, sizeof buf, 0);
	bskip(cfmt.textfont.size * cfmt.lineskipfac);
	PUT2("0 0 M (%s %s) show\n", str, buf);
	while ((t = t->next) != 0) {
		bskip(cfmt.textfont.size * cfmt.lineskipfac);
		tex_str(buf, t->text, sizeof buf, 0);
		PUT1("20 0 M (%s) show\n", buf);
	}
	bskip(cfmt.textfont.size * cfmt.lineskipfac);
	buffer_eob();
}

/* -- put_history -- */
void put_history(void)
{
	struct text *t;
	float baseskip, parskip;

	set_font_init(&cfmt.textfont);
	baseskip = cfmt.textfont.size * cfmt.lineskipfac;
	parskip = cfmt.textfont.size * cfmt.parskipfac;

	bskip(cfmt.textspace);

	if (info.rhyth && !cfmt.infoline) {
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

	put_text(TEXT_D, "Discography: ");
	put_text(TEXT_N, "Notes: ");
	put_text(TEXT_Z, "Transcription: ");

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
	buffer_eob();
	page_init[0] = '\0';
}

/* -- write a title -- */
void write_title(int i)
{
	char t[200];

	if (i == 0) {
		bskip(cfmt.titlespace + cfmt.titlefont.size);
		set_font(&cfmt.titlefont);
	} else {
		bskip(cfmt.subtitlespace + cfmt.subtitlefont.size);
		set_font(&cfmt.subtitlefont);
	}

	strncpy(t, info.title[i], sizeof t - 1);
	t[sizeof t - 1] = '\0';

	if (cfmt.titlecaps)
		cap_str(t);
	PUT0(" (");
	if (i == 0 && cfmt.withxrefs)
		PUT1("%s.  ", info.xref);
	put_str(t);
	if (cfmt.titleleft)
		PUT0(") 0 0 M show\n");
	else	PUT1(") %.1f 0 M cshow\n",
		     0.5 * ((cfmt.landscape ? cfmt.pageheight : cfmt.pagewidth)
		     - cfmt.leftmargin - cfmt.rightmargin) / cfmt.scale);
}

/* -- write_heading -- */
void write_heading(void)
{
	float lwidth, down1, down2;
	int i, ncl;
	char *rhythm;

	lwidth = ((cfmt.landscape ? cfmt.pageheight : cfmt.pagewidth)
		- cfmt.leftmargin - cfmt.rightmargin) / cfmt.scale;

	/* titles */
	for (i = 0; i < info.ntitle; i++)
		write_title(i);

	/* rhythm, composer, origin */
	down1 = cfmt.composerspace + cfmt.composerfont.size;
	rhythm = (first_voice->key.bagpipe && !cfmt.infoline) ? info.rhyth : 0;
	if (rhythm) {
		set_font(&cfmt.composerfont);
		PUT2("0 -%.1f M (%s) show\n",
		     cfmt.composerspace + cfmt.composerfont.size,
		     info.rhyth);
		down1 -= cfmt.composerfont.size;
	}
	if ((ncl = info.ncomp) > 0 || info.orig) {
		set_font(&cfmt.composerfont);
		bskip(cfmt.composerspace);
		if (ncl == 0)
			ncl = 1;
		for (i = 0; i < ncl; i++) {
			bskip(cfmt.composerfont.size);
			PUT1("%.1f 0 M (", lwidth);
			if (info.comp[i])
				put_str(info.comp[i]);
			if (info.orig && i == ncl - 1)
				put_str3(" \\(",
					 info.orig,
					 "\\)");
			PUT0(") lshow\n");
		}
		down1 += cfmt.composerfont.size * (ncl - 1);

		rhythm = rhythm ? 0 : info.rhyth;
		if ((rhythm || info.area) && cfmt.infoline) {

			/* if only one of rhythm or area then do not use ()'s
			 * otherwise output 'rhythm (area)' */
			set_font(&cfmt.infofont);
			bskip(cfmt.infofont.size + cfmt.infospace);
			PUT1("%.1f 0 M (", lwidth);
			if (rhythm) {
				PUT1("%s", rhythm);
				if (info.area)
					PUT1(" \\(%s\\)", info.area);
			} else	PUT1("%s", info.area);
			PUT0(") lshow\n");
			down1 += cfmt.infofont.size + cfmt.infospace;
		}
		down2 = 0;
	} else {
		down2 = cfmt.composerspace + cfmt.composerfont.size;
	}

	/* parts */
	if (info.parts && cfmt.printparts) {
		down1 = cfmt.partsspace + cfmt.partsfont.size - down1;
		if (down1 > 0)
			down2 += down1;
		if (down2 > 0.01)
			bskip(down2);
		PUT2("%.1f F%d ", cfmt.partsfont.size, cfmt.partsfont.fnum);
		put_str3("0 0 M (", info.parts, ") show\n");
		down2 = 0;
	}
	bskip(down2 + cfmt.musicspace);
}

/* -- output the user defined postscript sequences -- */
void write_user_ps(void)
{
	struct text *t;

	t = text_tb[TEXT_PS];
	while (t != 0) {
		fprintf(fout, "%s\n", t->text);
		t = t->next;
	}
}
