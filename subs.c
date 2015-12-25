/*
 * Low-level utilities.
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
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <ctype.h>

#include "abcparse.h"
#include "abc2ps.h" 

char tex_buf[512];		/* result of tex_str() */
struct FONTSPEC *font_init;	/* font in case of page break */
int strcf;			/* current string font */

static char *strop;		/* current string output operation */
static float strlw;		/* line width */
static float strtw = -1;	/* current text width */
static int strns;		/* number of spaces (justify) */
static int strfc;		/* force a font change before bext */
static int strtx;		/* PostScript text outputing */

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

static struct u_ps {
	struct u_ps *next;
	char text[2];
} *user_ps;

static char *trim_title(char *p, int first);

/* -- print message for internal error and maybe stop -- */
void bug(char *msg, int fatal)
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
	   struct SYMBOL *s,
	   char *fmt, ...)
{
	va_list args;
static struct SYMBOL *t;

	if (t != info.title) {
		char *p;

		t = info.title;
		p = &t->as.text[2];
		while (isspace((unsigned char) *p))
			p++;
		fprintf(stderr, "   - In tune `%s':\n", p);
	}
	fprintf(stderr, sev == 0 ? "Warning " : "Error ");
	if (s != 0)
		fprintf(stderr, "in line %d.%d",
			s->as.linenum, s->as.colnum);
	fprintf(stderr, ": ");
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
}

/* -- read a number with a unit -- */
float scan_u(char *str)
{
	float a;
	int nch;

	if (sscanf(str, "%f%n", &a, &nch) == 1) {
		if (str[nch] == '\0' || str[nch] == ' ')
			return a PT;
		if (!strncasecmp(str + nch, "cm", 2))
			return a CM;
		if (!strncasecmp(str + nch, "in", 2))
			return a IN;
		if (!strncasecmp(str + nch, "pt", 2))
			return a PT;
	}
	error(1, 0, "\n++++ Unknown unit value \"%s\"", str);
	return 20 PT;
}

/* -- capitalize a string -- */
static void cap_str(char *p)
{
	while (*p != '\0') {
#if 1
/* pb with toupper - works with ASCII only */
		unsigned char c;

		c = (unsigned char) *p;
		if ((c >= 'a' && c <= 'z')
		    || (c >= 0xe0 && c <= 0xfe))
			*p = c & ~0x20;
#else
		*p = toupper((unsigned char) *p);
#endif
		p++;
	}
}

/* -- return the character width -- */
float cwid(unsigned char c)
{
	short *w;
	unsigned enc;

	if ((enc = cfmt.encoding) >= sizeof cw_tb / sizeof cw_tb[0])
		enc = 0;
	if (c < 160)		/* (0xa0) */
		w = ISOLatin1_w;
	else	w = cw_tb[enc];
	return (float) w[c] / 1000.;
}

/* -- memorize the current font in case of page break -- */
static void set_font_init(void)
{
	if (strcf < 0)
		return;
	font_init = &cfmt.font_tb[strcf];
}

/* -- change string taking care of some tex-style codes -- */
/* Puts \ in front of ( and ) in case brackets are not balanced,
 * interprets all ISOLatin1..6 escape sequences as defined in rfc1345.
 * Returns an estimate of the string width. */
float tex_str(char *s)
{
	char *d, c1, c2, *p_enc, *p;
	int maxlen, i;
	float w, swfac;

	w = 0;
	d = tex_buf;
	maxlen = sizeof tex_buf - 1;		/* have room for EOS */
	if ((i = strcf) < 0)
		i = 0;
	swfac = cfmt.font_tb[i].swfac;
	i = font_enc[cfmt.font_tb[i].fnum];
	if ((unsigned) i >= sizeof esc_tb / sizeof esc_tb[0])
		i = 0;
	p_enc = esc_tb[i];
	while ((c1 = *s++) != '\0') {
		switch (c1) {
		case '\\':			/* backslash sequences */
			if ((c1 = *s++) == '\0')
				break;
			if (c1 == ' ')
				goto addchar1;
			if (c1 == '\\' || (c2 = *s) == '\0') {
				if (--maxlen <= 0)
					break;
				*d++ = '\\';
				goto addchar1;
			}
			/* treat escape with octal value */
			if ((unsigned) (c1 - '0') <= 3
			    && (unsigned) (c2 - '0') <= 7
			    && (unsigned) (s[1] - '0') <= 7) {
				if ((maxlen -= 4) <= 0)
					break;
				*d++ = '\\';
				*d++ = c1;
				*d++ = c2;
				*d++ = s[1];
				c1 = ((c1 - '0') << 6) + ((c2 - '0') << 3) + s[1] - '0';
				w += cwid((unsigned char) c1) * swfac;
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
#if 0
		case '{':
		case '}':
			break;
#endif
		case '$':
			if (isdigit((unsigned char) *s)
			    && (unsigned) (*s - '0') < DFONT_MIN) {
				i = *s - '0';
				swfac = cfmt.font_tb[i].swfac;
				i = cfmt.font_tb[i].fnum;
				i = font_enc[i];
				if ((unsigned) i >= sizeof esc_tb / sizeof esc_tb[0])
					i = 0;
				p_enc = esc_tb[i];
			} else if (*s == '$') {
				if (--maxlen <= 0)
					break;
				*d++ = c1;
				w += cwid((unsigned char) c1) * swfac;
				s++;
			}
			goto addchar1;
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
			w += cwid((unsigned char) c1) * swfac;
			break;
		}
	}
	*d = '\0';
	return w;
}

/* -- set the default font of a string -- */
void str_font(struct FONTSPEC *font)
{
	memcpy(&cfmt.font_tb[0], font, sizeof cfmt.font_tb[0]);
	strcf = -1;
}

/* -- output one string -- */
static void str_ft_out1(char *p, int l)
{
	if (strfc) {
		strfc = 0;
		if (strtx) {
			PUT1(")%s ", strop);
			strtx = 0;
		}
		set_font(&cfmt.font_tb[strcf]);
		if (font_init != 0)
			set_font_init();
	}
	if (!strtx) {
		PUT0("(");
		strtx = 1;
	}
	PUT2("%.*s", l, p);
}

/* -- output a string and the font changes -- */
static void str_ft_out(char *p, int end)
{
	char *q;

	q = p;
	while (*p != '\0') {
		if (*p == '$') {
			if (isdigit((unsigned char) p[1])
			    && (unsigned) (p[1] - '0') < DFONT_MIN) {
				if (p > q)
					str_ft_out1(q, p - q);
				if (strcf != p[1] - '0') {
					strcf = p[1] - '0';
					strfc = 1;
				}
				p += 2;
				q = p;
				continue;
			}
			if (p[1] == '$') {
				str_ft_out1(q, p - q);
				q = ++p;
			}
		}
		p++;
	}
	if (p > q)
		str_ft_out1(q, p - q);
	if (end && strtx) {
		PUT1(")%s ", strop);
		strtx = 0;
	}
}

/* -- output a string, handling the font changes -- */
void str_out(char *p, int action)
{
	if (strcf < 0) {		/* first call */
		strcf = 0;
		strfc = 1;
	}

	/* special case when font change at start of text */
	if (*p == '$' && isdigit((unsigned char) p[1])
	    && (unsigned) (p[1] - '0') < DFONT_MIN) {
		if (strcf != p[1] - '0') {
			strcf = p[1] - '0';
			strfc = 1;
		}
		p += 2;
	}

	/* direct output if no font change */
	if (strchr(p, '$') == 0) {
		char *op;

		if (strfc) {
			set_font(&cfmt.font_tb[strcf]);
			strfc = 0;
		}
		if (action == A_CENTER)
			op = "c";
		else if (action == A_RIGHT)
			op = "r";
		else	op = "";
		PUT2("(%s)show%s ", p, op);
		return;
	}

	/* if not left aligned, build a PS function */
	if (action == A_LEFT)
		strop = "show";
	else {
		PUT0("/str{");
		strfc = 1;
		strop = "strop";
	}

	str_ft_out(p, 1);		/* output the string */

	/* if not left aligned, call the PS function */
	if (action == A_LEFT)
		return;
	PUT0("}def\n"
		"/strop/strw load def/w 0 def str w ");
	if (action == A_CENTER)
		PUT0("0.5 mul ");
	PUT0("neg 0 RM/strop/show load def str ");
}

/* -- output a string with TeX translation -- */
void put_str(char *str, int action)
{
	tex_str(str);
	str_out(tex_buf, action);
	PUT0("\n");
}

/* -- output a header information -- */
static void put_inf(struct SYMBOL *s)
{
	char *p;

	p = s->as.text;
	if (p[1] == ':')
		p += 2;
	while (isspace((unsigned char) *p))
		p++;
	put_str(p, A_LEFT);
}

/* -- output a header format '111 (222)' -- */
static void put_inf2r(struct SYMBOL *s1,
		      struct SYMBOL *s2,
		      int action)
{
	char buf[512], *p, *q;

	if (s1 == 0) {
		s1 = s2;
		s2 = 0;
	}
	p = s1->as.text;
	if (p[1] == ':')
		p += 2;
	while (isspace((unsigned char) *p))
		p++;
	if (s1->as.text[0] == 'T' && s1->as.text[1] == ':')
		p = trim_title(p, s1 == info.title);
	if (s2 != 0) {
		buf[sizeof buf - 1] = '\0';
		strncpy(buf, p, sizeof buf - 1);
		q = buf + strlen(buf);
		if (q < buf + sizeof buf - 4) {
			*q++ = ' ';
			*q++ = '(';
			p = s2->as.text;
			if (p[1] == ':')
				p += 2;
			while (isspace((unsigned char) *p))
				p++;
			strncpy(q, p, buf + sizeof buf - 2 - q);
			q += strlen(q);
			*q++ = ')';
			*q = '\0';
		}
		p = buf;
	}
	put_str(p, action);
}

/* -- add text to a block -- */
void add_to_text_block(char *s, int job)
{
	float baseskip, lw;
	char *p, sep;
	struct FONTSPEC *f;

	/* if first line, set the fonts */
	if (strtw < 0) {
		font_init = &cfmt.textfont;
		str_font(&cfmt.textfont);
		strlw = ((cfmt.landscape ? cfmt.pageheight : cfmt.pagewidth)
			- cfmt.leftmargin - cfmt.rightmargin) / cfmt.scale;
	}

	if (strcf >= 0)
		f = &cfmt.font_tb[strcf];
	else	f = &cfmt.font_tb[0];
	baseskip = f->size * cfmt.lineskipfac;

	/* follow lines */
	if (job == OBEYLINES || job == OBEYCENTER) {
		if (*s != '\0') {
			bskip(baseskip);
			if (job == OBEYLINES) {
				PUT0("0 0 M ");
				put_str(s, A_LEFT);
			} else	{
				PUT1("%.1f 0 M ", strlw * 0.5);
				put_str(s, A_CENTER);
			}
		} else	bskip(baseskip * 0.5);
		buffer_eob();
		strtw = 0;
		return;
	}

	/* fill or justify lines */
	if (strtw < 0) {		/* if first line */
		strcf = 0;
		strfc = 1;
		bskip(baseskip);
		PUT0("0 0 M ");
		if (job == T_FILL)
			strop = "show";
		else {
			PUT0("/str{");
			strop = "strop";
		}
		strns = 0;
		strtw = 0;
	}

	if (*s == '\0') {			/* empty line */
		if (strtx) {
			PUT1(")%s", strop);
			strtx = 0;
		}
		if (job == T_JUSTIFY)
			PUT0("}def\n"
				"/strop/show load def str\n");
		else	PUT0("\n");
		font_init = f;
		bskip(f->size * cfmt.lineskipfac * 1.5);
		buffer_eob();
		PUT0("0 0 M ");
		if (job == T_JUSTIFY) {
			PUT0("/str{");
			strfc = 1;
		}
		strns = 0;
		strtw = 0;
		return;
	}

	p = s;
	for (;;) {
		while (*p != ' ' && *p != '\0')
			p++;
		sep = *p;
		*p = '\0';
		lw = tex_str(s);
		if (strtw + lw > strlw) {
			if (strtx) {
				PUT1(")%s ", strop);
				strtx = 0;
			}
			if (job == T_JUSTIFY) {
				if (strns == 0)
					strns = 1;
				PUT2("}def\n"
					"/strop/strw load def/w 0 def str"
					"/w %.1f w sub %d div def"
					"/strop/jshow load def str ",
					strlw, strns);
				strns = 0;
			}
			bskip(cfmt.font_tb[strcf].size * cfmt.lineskipfac);
			PUT0("0 0 M ");
			if (job == T_JUSTIFY) {
				PUT0("/str{");
				strfc = 1;
			}
			strtw = 0;
		}
		if (strtw != 0) {
			str_ft_out(" ", 0);
			strtw += cwid(' ') * cfmt.font_tb[strcf].swfac;
			strns++;
		}
		str_ft_out(tex_buf, 0);
		strtw += lw;
		*p = sep;
		while (*p == ' ')
			p++;
		if (*p == '\0')
			break;
		s = p;
	}
}

/* -- write a text block -- */
void write_text_block(int job, int abc_state)
{
	if (strtw < 0)
		return;

	if (strtx) {
		PUT1(")%s", strop);
		strtx = 0;
	}
	if (job == T_JUSTIFY)
		PUT0("}def\n"
			"/strop/show load def str\n");
	else if (job == T_FILL)
		PUT0("\n");
	bskip(cfmt.textfont.size * cfmt.parskipfac);
	buffer_eob();

	/* next line to allow pagebreak after each paragraph */
	if (!epsf && abc_state != ABC_S_TUNE)
		write_buffer();
	font_init = 0;
	strtw = -1;
}

/* -- output a line of words after tune -- */
static int put_wline(char *p,
		     float x,
		     int right)
{
	char *q, *r, sep;

	while (isspace((unsigned char) *p))
		p++;
	if (*p == '$' && isdigit((unsigned char) p[1])
	    && (unsigned) (p[1] - '0') < DFONT_MIN) {
		if (strcf != p[1] - '0') {
			strcf = p[1] - '0';
			strfc = 1;
		}
		p += 2;
	}
	r = 0;
	q = p;
	if (isdigit((unsigned char) *p) || p[1] == '.') {
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
	   && (*p == '\0' || r != 0)) {
		set_font_init();
		buffer_eob();
	}

	if (r != 0) {
		sep = *r;
		*r = '\0';
		PUT1("%.1f 0 M ", x);
		put_str(q,  A_RIGHT);
		*r = sep;
	}
	if (*p != '\0') {
		PUT1("%.1f 0 M ", x + 5);
		put_str(p, A_LEFT);
	}
	return *p == '\0' && r == 0;
}

/* -- output the words after tune -- */
void put_words(struct SYMBOL *words)
{
	struct SYMBOL *s, *s_end, *s2;
	char *p;
	int i, n, have_text, max2col;
	float middle;

	str_font(&cfmt.wordsfont);

	/* see if we may have 2 columns */
	middle = 0.5 * ((cfmt.landscape ? cfmt.pageheight : cfmt.pagewidth)
		- cfmt.leftmargin - cfmt.rightmargin) / cfmt.scale;
	max2col = (int) ((middle - 45.) / (cwid('a') * cfmt.wordsfont.swfac));
	n = 0;
	have_text = 0;
	for (s = words; s != 0; s = s->next) {
		p = &s->as.text[2];
		while (isspace((unsigned char) *p))
			p++;
		if (strlen(p) > max2col) {
			n = 0;
			break;
		}
		if (*p == '\0') {
			if (have_text) {
				n++;
				have_text = 0;
			}
		} else	have_text = 1;
	}
	if (n > 0) {
		n++;
		n /= 2;
		i = n;
		have_text = 0;
		s_end = words;
		for (;;) {
			p = &s_end->as.text[2];
			while (isspace((unsigned char) *p))
				p++;
			if (*p == '\0') {
				if (have_text && --i <= 0)
					break;
				have_text = 0;
			} else	have_text = 1;
			s_end = s_end->next;
		}
		s2 = s_end->next;
	} else {
		s_end = 0;
		s2 = 0;
	}

	/* output the text */
	bskip(cfmt.wordsspace);
	for (s = words; s != 0 || s2 != 0; ) {
		bskip(cfmt.lineskipfac * cfmt.wordsfont.size);
		if (s != 0) {
			put_wline(&s->as.text[2], 45., 0);
			s = s->next;
			if (s == s_end)
				s = 0;
		}
		if (s2 != 0) {
			if (put_wline(&s2->as.text[2], 20. + middle, 1)) {
				if (--n == 0) {
					if (s != 0)
						n++;
					else if (s2->next != 0) {

						/* center the last words */
/*fixme: should compute the width average.. */
						middle *= 0.6;
					}
				}
			}
			s2 = s2->next;
		}
	}
	set_font_init();
	buffer_eob();
	font_init = 0;
}

/* -- output history lines -- */
static void put_text(char *head,
		     struct SYMBOL *s)
{
	float h;

	if (s == 0)
		return;
	if (strcf != 0) {
		set_font(&cfmt.historyfont);
		strcf = 0;
		set_font_init();
	}
	h = font_init->size * cfmt.lineskipfac;
	bskip(h * 1.2);
	PUT1("w%s ", head);
	bskip(h);
	do {
		PUT0("50 0 M ");
		put_inf(s);
		bskip(h);
	} while ((s = s->next) != 0);
	buffer_eob();
}

/* -- output history -- */
void put_history(void)
{
	bskip(cfmt.textspace);
	str_font(&cfmt.historyfont);
	if (!cfmt.infoline)
		put_text("rhythm", info.rhythm);
	put_text("book", info.book);
	put_text("source", info.src);
	put_text("disco", info.disco);
	put_text("notes", info.notes);
	put_text("trans", info.trans);
	put_text("histo", info.histo);
	font_init = 0;
}

/* -- move trailing "The" to front, set to uppercase letters or add xref -- */
static char *trim_title(char *p, int first)
{
	char *b, *q;
	int l, mxl, trim;
static char buf[256];

	l = strlen(p);
	q = p + l - 3;
	trim = strcmp(q, "The") == 0;
	if (!trim && !cfmt.titlecaps && !(first && cfmt.withxrefs))
		return p;
	b = buf;
	mxl = sizeof buf - 1;
	if (first && cfmt.withxrefs) {
		l = sprintf(b, "%s.  ", info.xref);
		b += l;
		mxl -= l;
	}
	if (trim) {
		q--;
		while (isspace((unsigned char) *q))
			q--;
		if (*q == ',') {
			strcpy(b, "The ");
			b += 4;
			mxl -= 4;
			l = q - p;
			if (l > mxl)
				l = mxl;
			strncpy(b, p, l);
			b[l] = '\0';
		} else	trim = 0;
	}
	if (!trim) {
		strncpy(b, p, mxl);
		b[mxl] = '\0';
	}
	if (cfmt.titlecaps)
		cap_str(buf);
	return buf;
}

/* -- write a title -- */
void write_title(struct SYMBOL *s)
{
	char *p;

	p = &s->as.text[2];
	while (isspace((unsigned char) *p))
		p++;
	if (*p == '\0' || strcmp(p, "(notitle)") == 0)
		return;
	p = trim_title(p, s == info.title);
	if (s == info.title) {
		bskip(cfmt.titlespace + cfmt.titlefont.size);
		set_font(&cfmt.titlefont);
	} else {
		bskip(cfmt.subtitlespace + cfmt.subtitlefont.size);
		set_font(&cfmt.subtitlefont);
	}
	if (cfmt.titleleft)
		PUT0("0 0 M(");
	else	PUT1("%.1f 0 M(",
		     0.5 * ((cfmt.landscape ? cfmt.pageheight : cfmt.pagewidth)
		     - cfmt.leftmargin - cfmt.rightmargin) / cfmt.scale);
	tex_str(p);
	PUT2("%s)show%s\n", tex_buf, cfmt.titleleft ? "" : "c");
}

/* -- write_heading with format -- */
static void write_headform(float lwidth)
{
	char *p;
	struct SYMBOL *s, *title, xref_sym;
	struct FONTSPEC *f;
	int align;
	float x, y, yl, yc, yr;

	title = info.title;
	yl = yc = yr = 0;
	p = cfmt.titleformat;
	for (;;) {
		f = &cfmt.historyfont;
		while (isspace((unsigned char) *p))
			p++;
		switch (*p++) {
		case '\0':
			if (yc > yl)
				yl = yc;
			if (yr > yl)
				yl = yr;
			yc = yr = yl;
			bskip(yl);
			return;
		case 'A':
			s = info.area;
			f = &cfmt.infofont;
			break;
		case 'B':
			s = info.book;
			break;
		case 'C':
			s = info.comp;
			f = &cfmt.composerfont;
			break;
		case 'D':
			s = info.disco;
			break;
		case 'H':
			s = info.histo;
			break;
		case 'N':
			s = info.notes;
			break;
		case 'O':
			s = info.orig;
			f = &cfmt.composerfont;
			break;
		case 'P':
			s = info.parts;
			f = &cfmt.partsfont;
			break;
		case 'R':
			s = info.rhythm;
			f = &cfmt.infofont;
			break;
		case 'S':
			s = info.src;
			break;
		case 'T':
			s = title;
			f = (s == info.title) ? &cfmt.titlefont
					 : &cfmt.subtitlefont;
			break;
		case 'X':
			s = &xref_sym;
			s->as.text = info.xref;
			s->next = 0;
			f = &cfmt.titlefont;
			break;
		case 'Z':
			s = info.trans;
			break;
		case ',':
			if (yc > yl)
				yl = yc;
			if (yr > yl)
				yl = yr;
			yc = yr = yl;
			continue;
		default:
			continue;
		}
		if (s == 0)
			continue;
		switch (*p) {
		default:
			align = A_CENTER;
			x = lwidth * 0.5;
			y = yc;
			break;
		case '1':
			align = A_RIGHT;
			x = lwidth;
			y = yr;
			p++;
			break;
		case '-':
			align = A_LEFT;
			x = 0;
			y = yl;
			p++;
			break;
		}
		do {
			str_font(f);
			PUT2("%.1f %.1f M ", x, -y);
			put_inf2r(s, 0, align);
			y += f->size * 1.1;
			if (s == title) {
				title = title->next;
				break;
			}
			s = s->next;
		} while (s != 0);
		switch (align) {
		case A_LEFT:
			yl = y;
			break;
		case A_RIGHT:
			yr = y;
			break;
		default:
			yc = y;
			break;
		}
	}
	/*not reached*/
}

/* -- write_heading -- */
void write_heading(struct abctune *t)
{
	struct SYMBOL *s, *rhythm, *area, *author;
	float lwidth, down1, down2;

	lwidth = ((cfmt.landscape ? cfmt.pageheight : cfmt.pagewidth)
		- cfmt.leftmargin - cfmt.rightmargin) / cfmt.scale;

	if (cfmt.titleformat != 0) {
		write_headform(lwidth);
		return;
	}

	/* titles */
	for (s = info.title; s != 0; s = s->next)
		write_title(s);

	/* rhythm, composer, origin */
	down1 = cfmt.composerspace + cfmt.composerfont.size;
	rhythm = (first_voice->key.bagpipe && !cfmt.infoline) ? info.rhythm : 0;
	if (rhythm) {
		str_font(&cfmt.composerfont);
		PUT1("0 %.1f M ",
		     -(cfmt.composerspace + cfmt.composerfont.size));
		put_inf(rhythm);
		down1 -= cfmt.composerfont.size;
	}
	area = author = 0;
	if (t->abc_vers < 2)
		area = info.area;
	else	author = info.area;
	if ((s = info.comp) != 0 || info.orig || author != 0) {
		float xcomp;
		int align;

		str_font(&cfmt.composerfont);
		bskip(cfmt.composerspace);
		if (cfmt.aligncomposer < 0) {
			xcomp = 0;
			align = A_LEFT;
		} else if (cfmt.aligncomposer == 0) {
			xcomp = lwidth * 0.5;
			align = A_CENTER;
		} else {
			xcomp = lwidth;
			align = A_RIGHT;
		}
		down2 = down1;
		if (author != 0) {
			for (;;) {
				bskip(cfmt.composerfont.size);
				down2 += cfmt.composerfont.size;
				PUT0("0 0 M ");
				put_inf(author);
				if ((author = author->next) == 0)
					break;
			}
		}
		if ((s = info.comp) != 0 || info.orig) {
			if (cfmt.aligncomposer >= 0
			    && down1 != down2)
				bskip(down1 - down2);
			for (;;) {
				bskip(cfmt.composerfont.size);
				PUT1("%.1f 0 M ", xcomp);
				put_inf2r(s,
					  (s == 0 || s->next == 0) ? info.orig : 0,
					  align);
				if (s == 0)
					break;
				if ((s = s->next) == 0)
					break;
				down1 += cfmt.composerfont.size;
			}
			if (down2 > down1)
				bskip(down2 - down1);
		}

		rhythm = rhythm ? 0 : info.rhythm;
		if ((rhythm || area) && cfmt.infoline) {

			/* if only one of rhythm or area then do not use ()'s
			 * otherwise output 'rhythm (area)' */
			str_font(&cfmt.infofont);
			bskip(cfmt.infofont.size + cfmt.infospace);
			PUT1("%.1f 0 M ", lwidth);
			put_inf2r(rhythm, area, A_RIGHT);
			down1 += cfmt.infofont.size + cfmt.infospace;
		}
		down2 = 0;
	} else	down2 = cfmt.composerspace + cfmt.composerfont.size;

	/* parts */
	if (info.parts) {
		down1 = cfmt.partsspace + cfmt.partsfont.size - down1;
		if (down1 > 0)
			down2 += down1;
		if (down2 > 0.01)
			bskip(down2);
		str_font(&cfmt.partsfont);
		PUT0("0 0 M ");
		put_inf(info.parts);
		down2 = 0;
	}
	bskip(down2 + cfmt.musicspace);
}

/* -- memorize a PS line -- */
void user_ps_add(char *s)
{
	struct u_ps *t, *r;
	int l;

	l = strlen(s);
	t = (struct u_ps *) malloc(sizeof *user_ps - sizeof user_ps->text
				   + l + 1);
	strcpy(t->text, s);
	t->next = 0;
	if ((r = user_ps) == 0)
		user_ps = t;
	else {
		while (r->next != 0)
			r = r->next;
		r->next = t;
	}
}

/* -- output the user defined postscript sequences -- */
void user_ps_write(void)
{
	struct u_ps *t;

	for (t = user_ps; t != 0; t = t->next)
		fprintf(fout, "%s\n", t->text);
}
