/*
 * Low-level utilities.
 *
 * This file is part of abcm2ps.
 *
 * Copyright (C) 1998-2008 Jean-François Moine
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

char tex_buf[TEX_BUF_SZ];	/* result of tex_str() */
int outft = -1;			/* last font in the output file */

static char *strop;		/* current string output operation */
static float strlw;		/* line width */
static float strtw = -1;	/* current text width */
static int strns;		/* number of spaces (justify) */
static int curft;		/* current (wanted) font */
static int defft;		/* default font */
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
	error(1, 0, "Internal error: %s.", msg);
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

	if (t != info['T' - 'A']) {
		char *p;

		t = info['T' - 'A'];
		p = &t->as.text[2];
		while (isspace((unsigned char) *p))
			p++;
		fprintf(stderr, "   - In tune `%s':\n", p);
	}
	fprintf(stderr, sev == 0 ? "Warning " : "Error ");
	if (s != 0) {
		fprintf(stderr, "in line %d.%d",
			s->as.linenum, s->as.colnum);
		if (showerror) {
			s->as.flags |= ABC_F_ERROR;
			showerror++;
		}
	}
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

/* -- change string taking care of some tex-style codes -- */
/* Puts \ in front of ( and ) in case brackets are not balanced,
 * interprets all ISOLatin1..6 escape sequences as defined in rfc1345.
 * Returns an estimated width of the string. */
float tex_str(char *s)
{
	char *d, c1, c2, *p_enc, *p;
	int maxlen, i;
	float w, swfac;

	w = 0;
	d = tex_buf;
	maxlen = sizeof tex_buf - 1;		/* have room for EOS */
	if ((i = curft) <= 0)
		i = defft;
	swfac = cfmt.font_tb[i].swfac;
	i = font_enc[cfmt.font_tb[i].fnum];
	if ((unsigned) i >= sizeof esc_tb / sizeof esc_tb[0])
		i = 0;
	p_enc = esc_tb[i];
	while ((c1 = *s++) != '\0') {
		switch (c1) {
		case '\\':			/* backslash sequences */
			if (*s == '\0')
				continue;
			c1 = *s++;
			if (c1 == ' ')
				break;
			if (c1 == 't') {
				c1 = '\t';
				break;
			}
			if (c1 == '\\' || (c2 = *s) == '\0') {
				if (--maxlen <= 0)
					break;
				*d++ = '\\';
				break;
			}
			/* treat escape with octal value */
			if ((unsigned) (c1 - '0') <= 3
			    && (unsigned) (c2 - '0') <= 7
			    && (unsigned) (s[1] - '0') <= 7) {
				c1 = ((c1 - '0') << 6) + ((c2 - '0') << 3) + s[1] - '0';
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
			if (i < 0)
				c1 = s[-1];
			break;
		case '$':
			if (isdigit((unsigned char) *s)
			    && (unsigned) (*s - '0') < FONT_UMAX) {
				i = *s - '0';
				if (i == 0)
					i = defft;
				swfac = cfmt.font_tb[i].swfac;
				i = cfmt.font_tb[i].fnum;
				i = font_enc[i];
				if ((unsigned) i >= sizeof esc_tb / sizeof esc_tb[0])
					i = 0;
				p_enc = esc_tb[i];
				if (--maxlen <= 0)
					break;
				*d++ = c1;
				c1 = *s++;
				goto addchar_nowidth;
			}
			if (*s == '$') {
				if (--maxlen <= 0)
					break;
				*d++ = c1;
				s++;
			}
			break;
		case '(':
		case ')':			/* ( ) becomes \( \) */
			if (--maxlen <= 0)
				break;
			*d++ = '\\';
			break;
		}
		w += cwid((unsigned char) c1) * swfac;
	addchar_nowidth:
		if (--maxlen <= 0)
			break;
		*d++ = c1;
	}
	*d = '\0';
	return w;
}

/* -- set the default font of a string -- */
void str_font(int ft)
{
	curft = defft = ft;
}

/* -- get the current default font -- */
int get_str_font(void)
{
	return defft;
}

/* -- output one string -- */
static void str_ft_out1(char *p, int l)
{
	if (curft != outft) {
		if (strtx) {
			PUT1(")%s ", strop);
			strtx = 0;
		}
		if (curft == 0)
			curft = defft;
		set_font(curft);
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
			    && (unsigned) (p[1] - '0') < FONT_UMAX) {
				if (p > q)
					str_ft_out1(q, p - q);
				if (curft != p[1] - '0') {
					curft = p[1] - '0';
					if (curft == 0)
						curft = defft;
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
	if (curft <= 0)		/* first call */
		curft = defft;

	/* special case when font change at start of text */
/*---fixme: authorize 2 chars?*/
	if (*p == '$' && isdigit((unsigned char) p[1])
	    && (unsigned) (p[1] - '0') < FONT_UMAX) {
		if (curft != p[1] - '0') {
			curft = p[1] - '0';
			if (curft == 0)
				curft = defft;
		}
		p += 2;
	}

	/* direct output if no font change */
	if (strchr(p, '$') == 0) {
		char *op;

		set_font(curft);
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
		outft = -1;
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
	char buf[256], *p, *q;

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
		p = trim_title(p, s1 == info['T' - 'A']);
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
		str_font(TEXTFONT);
		strlw = ((cfmt.landscape ? cfmt.pageheight : cfmt.pagewidth)
			- cfmt.leftmargin - cfmt.rightmargin) / cfmt.scale;
	}

	if (curft > 0)
		f = &cfmt.font_tb[curft];
	else	f = &cfmt.font_tb[defft];
	baseskip = f->size * cfmt.lineskipfac;

	/* follow lines */
	if (job == T_LEFT || job == T_CENTER || job == T_RIGHT) {
		if (*s != '\0') {
			bskip(baseskip);
			if (job == T_LEFT) {
				PUT0("0 0 M ");
				put_str(s, A_LEFT);
			} else if (job == T_CENTER) {
				PUT1("%.1f 0 M ", strlw * 0.5);
				put_str(s, A_CENTER);
			} else {
				PUT1("%.1f 0 M ", strlw);
				put_str(s, A_RIGHT);
			}
		} else {
			bskip(baseskip * 0.5);
			buffer_eob();
		}
		strtw = 0;
		return;
	}

	/* fill or justify lines */
	if (strtw < 0) {		/* if first line */
		curft = defft;
		bskip(baseskip);
		PUT0("0 0 M ");
		if (job == T_FILL)
			strop = "show";
		else {
			PUT0("/str{");
			outft = -1;
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
		bskip(f->size * cfmt.lineskipfac * 1.5);
		buffer_eob();
		PUT0("0 0 M ");
		if (job == T_JUSTIFY) {
			PUT0("/str{");
			outft = -1;
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
			bskip(cfmt.font_tb[curft].size * cfmt.lineskipfac);
			PUT0("0 0 M ");
			if (job == T_JUSTIFY) {
				PUT0("/str{");
				outft = -1;
			}
			strtw = 0;
		}
		if (strtw != 0) {
			str_ft_out(" ", 0);
			strtw += cwid(' ') * cfmt.font_tb[curft].swfac;
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
	bskip(cfmt.font_tb[TEXTFONT].size * cfmt.parskipfac);
	buffer_eob();

	/* next line to allow pagebreak after each paragraph */
	if (!epsf && abc_state != ABC_S_TUNE)
		write_buffer();
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
	    && (unsigned) (p[1] - '0') < FONT_UMAX) {
		if (curft != p[1] - '0') {
			curft = p[1] - '0';
			if (curft == 0)
				curft = defft;
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
	   && (*p == '\0' || r != 0))
		buffer_eob();

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

	str_font(WORDSFONT);

	/* see if we may have 2 columns */
	middle = 0.5 * ((cfmt.landscape ? cfmt.pageheight : cfmt.pagewidth)
		- cfmt.leftmargin - cfmt.rightmargin) / cfmt.scale;
	max2col = (int) ((middle - 45.) / (cwid('a') * cfmt.font_tb[WORDSFONT].swfac));
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
		bskip(cfmt.lineskipfac * cfmt.font_tb[WORDSFONT].size);
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
	buffer_eob();
}

/* -- output history -- */
void put_history(void)
{
	struct SYMBOL *s, *s2;
	float h;

	bskip(cfmt.textspace);
	str_font(HISTORYFONT);
	for (s = info['I' - 'A']; s != 0; s = s->next) {
		if ((s2 = info[s->as.text[0] - 'A']) == 0)
			continue;
		get_str(tex_buf, &s->as.text[1], 256);
		h = cfmt.font_tb[HISTORYFONT].size * cfmt.lineskipfac;
		set_font(HISTORYFONT);
		PUT1("0 0 M(%s)show ", tex_buf);
		for (;;) {
			put_inf(s2);
			if ((s2 = s2->next) == 0)
				break;
			bskip(h);
			PUT0("50 0 M ");
		}
		bskip(h * 1.2);
		buffer_eob();
	}
}

/* -- move trailing "The" to front, set to uppercase letters or add xref -- */
static char *trim_title(char *p, int first)
{
	char *b, *q;
static char buf[STRL1];

	q = 0;
	if (cfmt.titletrim) {
		q = strrchr(p, ',');
		if (q != 0) {
			if (q[1] != ' ' || !isupper(q[2])
			    || strchr(q + 2, ' ') != 0)
				q = 0;
		}
	}
	if (q == 0 && !cfmt.titlecaps && !(first && cfmt.withxrefs))
		return p;		/* keep the title as it is */
	b = buf;
	if (first && cfmt.withxrefs) {
		char *r;

		r = &info['X' - 'A']->as.text[2];
		if (strlen(p) + strlen(r) + 3 >= STRL1) {
			error(1, 0, "Title or X: too long");
			return p;
		}
		b += sprintf(b, "%s.  ", r);
	} else {
		if (strlen(p) >= STRL1) {
			error(1, 0, "Title too long");
			return p;
		}
	}
	if (q != 0)
		sprintf(b, "%s %.*s", q + 2, (int) (q - p), p);
	else
		strcpy(b, p);
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
	if (*p == '\0')
		return;
	p = trim_title(p, s == info['T' - 'A']);
	if (s == info['T' - 'A']) {
		bskip(cfmt.titlespace + cfmt.font_tb[TITLEFONT].size);
		set_font(TITLEFONT);
	} else {
		bskip(cfmt.subtitlespace + cfmt.font_tb[SUBTITLEFONT].size);
		set_font(SUBTITLEFONT);
	}
	if (cfmt.titleleft)
		PUT0("0 0 M(");
	else	PUT1("%.1f 0 M(",
		     0.5 * ((cfmt.landscape ? cfmt.pageheight : cfmt.pagewidth)
		     - cfmt.leftmargin - cfmt.rightmargin) / cfmt.scale);
	tex_str(p);
	PUT2("%s)show%s\n", tex_buf, cfmt.titleleft ? "" : "c");
}

/* -- write heading with format -- */
static void write_headform(float lwidth)
{
	char *p, *q;
	struct SYMBOL *s;
	struct FONTSPEC *f;
	int align, i, j;
	float x, y, xa[3], ya[3], sz, yb[3];
	char inf_nb[26];
	INFO inf_s;
	char inf_ft[26];
	float inf_sz[26];
	char fmt[64];

	memset(inf_nb, 0, sizeof inf_nb);
	memset(inf_ft, HISTORYFONT, sizeof inf_ft);
	inf_ft['A' - 'A'] = INFOFONT;
	inf_ft['C' - 'A'] = COMPOSERFONT;
	inf_ft['O' - 'A'] = COMPOSERFONT;
	inf_ft['P' - 'A'] = PARTSFONT;
	inf_ft['Q' - 'A'] = TEMPOFONT;
	inf_ft['R' - 'A'] = INFOFONT;
	inf_ft['T' - 'A'] = TITLEFONT;
	inf_ft['X' - 'A'] = TITLEFONT;
	memcpy(inf_s, info, sizeof inf_s);
	memset(inf_sz, 0, sizeof inf_sz);
	inf_sz['A' - 'A'] = cfmt.infospace;
	inf_sz['C' - 'A'] = cfmt.composerspace;
	inf_sz['O' - 'A'] = cfmt.composerspace;
	inf_sz['R' - 'A'] = cfmt.infospace;
	p = cfmt.titleformat;
	j = 0;
	for (;;) {
		while (isspace((unsigned char) *p))
			p++;
		if (*p == '\0')
			break;
		i = *p - 'A';
		if ((unsigned) i < 26) {
			inf_nb[i]++;
			switch (p[1]) {
			default:
				align = A_CENTER;
				break;
			case '1':
				align = A_RIGHT;
				p++;
				break;
			case '-':
				align = A_LEFT;
				p++;
				break;
			}
			if (j < sizeof fmt - 4) {
				fmt[j++] = i;
				fmt[j++] = align;
			}
		} else if (*p == ',') {
			if (j < sizeof fmt - 3)
				fmt[j++] = 126;		/* next line */
		} else if (*p == '+') {
			if (j > 0 && fmt[j - 1] < 125
			    && j < sizeof fmt - 3)
				fmt[j++] = 125;		/* concatenate */
/*new fixme: add free text "..." ?*/
		}
		p++;
	}
	fmt[j++] = 126;			/* newline */
	fmt[j] = 127;			/* end of format */

	ya[0] = ya[1] = ya[2] = cfmt.titlespace;;
	xa[0] = 0;
	xa[1] = lwidth * 0.5;
	xa[2] = lwidth;

	p = fmt;
	for (;;) {
		yb[0] = yb[1] = yb[2] = y = 0;
		q = p;
		for (;;) {
			i = *q++;
			if (i >= 126)		/* if newline */
				break;
			align = *q++;
			if (yb[align + 1] != 0)
				continue;
			s = inf_s[i];
			if (s == 0 || inf_nb[i] == 0)
				continue;
			j = inf_ft[i];
			f = &cfmt.font_tb[j];
			sz = f->size * 1.1 + inf_sz[i];
			if (y < sz)
				y = sz;
			yb[align + 1] = sz;
/*fixme:should count the height of the concatenated field*/
			if (*q == 125)
				q++;
		}
		for (i = 0; i < 3; i++)
			ya[i] += y - yb[i];
		for (;;) {
			i = *p++;
			if (i >= 126)		/* if newline */
				break;
			align = *p++;
			s = inf_s[i];
			if (s == 0 || inf_nb[i] == 0)
				continue;
			j = inf_ft[i];
			str_font(j);
			x = xa[align + 1];
			f = &cfmt.font_tb[j];
			sz = f->size * 1.1 + inf_sz[i];
			y = ya[align + 1] + sz;
			PUT2("%.1f %.1f M ", x, -y);
			if (*p == 125) {	/* concatenate */
			    p++;
/*fixme: do it work with different fields*/
			    if (*p == i && p[1] == align
				&& s->next != 0) {
				char buf[256], *r;

				q = s->as.text;
				if (q[1] == ':')
					q += 2;
				while (isspace((unsigned char) *q))
					q++;
				if (i == 'T' - 'A')
					q = trim_title(q, s == inf_s['T' - 'A']);
				strncpy(buf, q, sizeof buf - 1);
				buf[sizeof buf - 1] = '\0';
				j = strlen(buf);
				if (j < sizeof buf - 1) {
					buf[j] = ' ';
					buf[j + 1] = '\0';
				}
				s = s->next;
				q = s->as.text;
				if (q[1] == ':')
					q += 2;
				while (isspace((unsigned char) *q))
					q++;
				if (s->as.text[0] == 'T' && s->as.text[1] == ':')
					q = trim_title(q, 0);
				r = buf + strlen(buf);
				strncpy(r, q, buf + sizeof buf - r - 1);
				tex_str(buf);
				str_out(tex_buf, align);
				PUT0("\n");
				inf_nb[i]--;
				p += 2;
			    }
			} else if (i == 'Q' - 'A') {	/* special case for tempo */
				if (align != A_LEFT) {
					float w;

					w = tempo_width(s);
					if (align == A_CENTER)
						PUT1("-%.1f 0 RM ", w * 0.5);
					else	PUT1("-%.1f 0 RM ", w);
				}
				write_tempo(s, 0, 0.75);
			} else	put_inf2r(s, 0, align);
			if (inf_s[i] == info['T' - 'A']) {
				inf_ft[i] = SUBTITLEFONT;
				str_font(SUBTITLEFONT);
				f = &cfmt.font_tb[SUBTITLEFONT];
				inf_sz[i] = cfmt.subtitlespace;
				sz = f->size * 1.1 + inf_sz[i];
			}
			s = s->next;
			if (inf_nb[i] == 1) {
				while (s != 0) {
					y += sz;
					PUT2("%.1f %.1f M ", x, -y);
					put_inf2r(s, 0, align);
					s = s->next;
				}
			}
			inf_s[i] = s;
			inf_nb[i]--;
			ya[align + 1] = y;
		}
		if (ya[1] > ya[0])
			ya[0] = ya[1];
		if (ya[2] > ya[0])
			ya[0] = ya[2];
		if (*p == 127) {
			bskip(ya[0]);
			break;
		}
		ya[1] = ya[2] = ya[0];
	}
}

/* -- output the tune heading -- */
void write_heading(struct abctune *t)
{
	struct SYMBOL *s, *rhythm, *area, *author;
	float lwidth, down1, down2;

	lwidth = ((cfmt.landscape ? cfmt.pageheight : cfmt.pagewidth)
		- cfmt.leftmargin - cfmt.rightmargin) / cfmt.scale;

	if (cfmt.titleformat != 0) {
		write_headform(lwidth);
		bskip(cfmt.musicspace);
		return;
	}

	/* titles */
	for (s = info['T' - 'A']; s != 0; s = s->next)
		write_title(s);

	/* rhythm, composer, origin */
	down1 = cfmt.composerspace + cfmt.font_tb[COMPOSERFONT].size;
	rhythm = (first_voice->key.bagpipe && !cfmt.infoline) ? info['R' - 'A'] : 0;
	if (rhythm) {
		str_font(COMPOSERFONT);
		PUT1("0 %.1f M ",
		     -(cfmt.composerspace + cfmt.font_tb[COMPOSERFONT].size));
		put_inf(rhythm);
		down1 -= cfmt.font_tb[COMPOSERFONT].size;
	}
	area = author = 0;
	if (t->abc_vers < 2)
		area = info['A' - 'A'];
	else	author = info['A' - 'A'];
	if ((s = info['C' - 'A']) != 0 || info['O' - 'A'] || author != 0) {
		float xcomp;
		int align;

		str_font(COMPOSERFONT);
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
				bskip(cfmt.font_tb[COMPOSERFONT].size);
				down2 += cfmt.font_tb[COMPOSERFONT].size;
				PUT0("0 0 M ");
				put_inf(author);
				if ((author = author->next) == 0)
					break;
			}
		}
		if ((s = info['C' - 'A']) != 0 || info['O' - 'A']) {
			if (cfmt.aligncomposer >= 0
			    && down1 != down2)
				bskip(down1 - down2);
			for (;;) {
				bskip(cfmt.font_tb[COMPOSERFONT].size);
				PUT1("%.1f 0 M ", xcomp);
				put_inf2r(s,
					  (s == 0 || s->next == 0) ? info['O' - 'A'] : 0,
					  align);
				if (s == 0)
					break;
				if ((s = s->next) == 0)
					break;
				down1 += cfmt.font_tb[COMPOSERFONT].size;
			}
			if (down2 > down1)
				bskip(down2 - down1);
		}

		rhythm = rhythm ? 0 : info['R' - 'A'];
		if ((rhythm || area) && cfmt.infoline) {

			/* if only one of rhythm or area then do not use ()'s
			 * otherwise output 'rhythm (area)' */
			str_font(INFOFONT);
			bskip(cfmt.font_tb[INFOFONT].size + cfmt.infospace);
			PUT1("%.1f 0 M ", lwidth);
			put_inf2r(rhythm, area, A_RIGHT);
			down1 += cfmt.font_tb[INFOFONT].size + cfmt.infospace;
		}
		down2 = 0;
	} else	down2 = cfmt.composerspace + cfmt.font_tb[COMPOSERFONT].size;

	/* parts */
	if (info['P' - 'A']) {
		down1 = cfmt.partsspace + cfmt.font_tb[PARTSFONT].size - down1;
		if (down1 > 0)
			down2 += down1;
		if (down2 > 0.01)
			bskip(down2);
		str_font(PARTSFONT);
		PUT0("0 0 M ");
		put_inf(info['P' - 'A']);
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

	for (t = user_ps; t != 0; t = t->next) {
		if (t->text[0] == '\001') {	/* PS file */
			FILE *f;
			char line[BSIZE];

			if ((f = fopen(&t->text[1], "r")) == 0) {
				error(1, 0, "Cannot open PS file '%s'",
					&t->text[1]);
			} else {
				while (fgets(line, sizeof line, f))	/* copy the file */
					fwrite(line, 1, strlen(line), fout);
				fclose(f);
			}
		} else {
			fprintf(fout, "%s\n", t->text);
		}
	}
}
