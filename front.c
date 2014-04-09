/*
 * ABC front-end parser
 *
 * This file is part of abcm2ps.
 *
 * Copyright (C) 2011-2014 Jean-François Moine (http://moinejf.free.fr)
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
#include <string.h>
#include <ctype.h>

#ifdef WIN32
#define strncasecmp strnicmp
#endif

#include "front.h"
#include "slre.h"

static unsigned char *dst;
static int offset, size, keep_comments;
static void (*include_f)(unsigned char *fn);
static unsigned char *selection;
static int latin, skip;
static char prefix[4] = {'%'};

/*
 * translation table from the ABC draft version 2
 *	` grave
 *	' acute
 *	^ circumflex
 *	, cedilla
 *	" umlaut
 *	~ tilde
 *	o ring
 *	= macron or stroke
 *	/ slash
 *	; ogonek
 *	v caron
 *	u breve
 *	: long Hungarian umlaut
 *	. dot / dotless
 * else, ligatures
 *	ae ss ng
 */
/* !! each table item is 3 bytes: 1 char, 1 UTF-8 char on 2 bytes */
static unsigned char grave[] = "AÀEÈIÌOÒUÙaàeèiìoòuù";
static unsigned char acute[] = "AÁEÉIÍOÓUÚYÝaáeéiíoóuúyýSŚZŹsśzźRŔLĹCĆNŃrŕlĺcćnń";
static unsigned char circumflex[] = "AÂEÊIÎOÔUÛaâeêiîoôuûHĤJĴhĥjĵCĈGĜSŜcĉgĝsŝ";
static unsigned char cedilla[] = "CÇcçSŞsşTŢtţRŖLĻGĢrŗlļgģNŅKĶnņkķ";
static unsigned char umlaut[] = "AÄEËIÏOÖUÜYŸaäeëiïoöuüyÿ";
static unsigned char tilde[] = "AÃNÑOÕaãnñoõIĨiĩUŨuũ";
static unsigned char ring[] = "AÅaåUŮuů";
static unsigned char macron[] = "AĀDĐEĒHĦIĪOŌTŦUŪaādđeēhħiīoōtŧuū"; /* and stroke! */
static unsigned char slash[] = "OØoøDĐdđLŁlł";
static unsigned char ogonek[] = "AĄEĘIĮUŲaąeęiįuų";
static unsigned char caron[] = "LĽSŠTŤZŽlľsštťzžCČEĚDĎNŇRŘcčeědďnňrř";
static unsigned char breve[] = "AĂaăEĔeĕGĞgğIĬiĭOŎoŏUŬuŭ";
static unsigned char hungumlaut[] = "OŐUŰoőuű";
static unsigned char dot[] = "ZŻzżIİiıCĊcċGĠgġEĖeė";
/* the items of this table are 4 bytes long */
static unsigned char ligature[] = "AAÅaaåAEÆaeæccçcCÇDHÐdhðngŋOEŒoeœssßTHÞthþ";

/* latin conversion tables - range 0xa0 .. 0xff */
static unsigned char latin2[] = {
	" Ą˘Ł¤ĽŚ§¨ŠŞŤŹ­ŽŻ°ą˛ł´ľśˇ¸šşťź˝žż"
	"ŔÁÂĂÄĹĆÇČÉĘËĚÍÎĎĐŃŇÓÔŐÖ×ŘŮÚŰÜÝŢß"
	"ŕáâăäĺćçčéęëěíîďđńňóôőö÷řůúűüýţ˙"
};
static unsigned char latin3[] = {
	" Ħ˘£¤  Ĥ§¨İŞĞĴ­  Ż°ħ²³´µĥ·¸ışğĵ½  ż"
	"ÀÁÂ  ÄĊĈÇÈÉÊËÌÍÎÏ  ÑÒÓÔĠÖ×ĜÙÚÛÜŬŜß"
	"àáâ  äċĉçèéêëìíîï  ñòóôġö÷ĝùúûüŭŝ˙"
};
static unsigned char latin4[] = {
	" ĄĸŖ¤ĨĻ§¨ŠĒĢŦ­Ž¯°ą˛ŗ´ĩļˇ¸šēģŧŊžŋ"
	"ĀÁÂÃÄÅÆĮČÉĘËĖÍÎĪĐŅŌĶÔÕÖ×ØŲÚÛÜŨŪß"
	"āáâãäåæįčéęëėíîīđņōķôõö÷øųúûüũū˙"
};
static unsigned char latin5[] = {
	" ¡¢£¤¥¦§¨©ª«¬­®¯°±²³´µ¶·¸¹º»¼½¾¿"
	"ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏĞÑÒÓÔÕÖ×ØÙÚÛÜİŞß"
	"àáâãäåæçèéêëìíîïğñòóôõö÷øùúûüışÿ"
};
static unsigned char latin6[] = {
	" ĄĒĢĪĨĶ§ĻĐŠŦŽ­ŪŊ°ąēģīĩķ·ļđšŧž―ūŋ"
	"ĀÁÂÃÄÅÆĮČÉĘËĖÍÎÏÐŅŌÓÔÕÖŨØŲÚÛÜÝÞß"
	"āáâãäåæįčéęëėíîïðņōóôõöũøųúûüýþĸ"
};
static unsigned char *latin_tb[5] = {
	latin2, latin3, latin4, latin5, latin6
};

/* add text to the output buffer */
static void txt_add(unsigned char *s, int sz)
{
	if (skip)
		return;
	if (offset + sz > size) {
		size = (offset + sz + 8191) / 8192 * 8192;
		if (dst == 0)
			dst = malloc(size);
		else
			dst = realloc(dst, size);
		if (dst == 0) {
			fprintf(stderr, "Out of memory - abort\n");
			exit(EXIT_FAILURE);
		}
	}
	memcpy(dst + offset, s, sz);
	offset += sz;
}

/* add text to the output buffer translating
 * the escape sequences and the non utf-8 characters */
static void txt_add_cnv(unsigned char *s, int sz)
{
	unsigned char *p, c, tmp[4];

	p = s;
	while (sz > 0) {
		if (*p == '\\') {
			if (sz >= 4			/* \ooo */
			 && p[1] >= '0' && p[1] <= '3'
			 && p[2] >= '0' && p[2] <= '7'
			 && p[3] >= '0' && p[3] <= '7') {
				c = ((p[1] - '0') << 6)
					+ ((p[2] - '0') << 3)
					+ p[3] - '0';
				if (p != s)
					txt_add(s, (int) (p - s));
				p += 4;
				s = p;
				sz -= 4;
				switch (c) {	/* convert the accidentals */
				case 0x01:
				case 0x81:
					tmp[0] = 0xe2;
					tmp[1] = 0x99;
					tmp[2] = 0xaf;
					txt_add(tmp, 3);
					continue;
				case 0x02:
				case 0x82:
					tmp[0] = 0xe2;
					tmp[1] = 0x99;
					tmp[2] = 0xad;
					txt_add(tmp, 3);
					continue;
				case 0x03:
				case 0x83:
					tmp[0] = 0xe2;
					tmp[1] = 0x99;
					tmp[2] = 0xae;
					txt_add(tmp, 3);
					continue;
				case 0x04:
				case 0x84:
					tmp[0] = 0xf0;
					tmp[1] = 0x9d;
					tmp[2] = 0x84;
					tmp[3] = 0xaa;
					txt_add(tmp, 4);
					continue;
				case 0x05:
				case 0x85:
					tmp[0] = 0xf0;
					tmp[1] = 0x9d;
					tmp[2] = 0x84;
					tmp[3] = 0xab;
					txt_add(tmp, 4);
					continue;
				}
				if (c >= 0x80 && latin > 0)
					goto latin;
				tmp[0] = c;
				txt_add(tmp, 1);
				continue;
			}
			if (sz >= 6		/* \uxxxx */
			 && p[1] == 'u'
			 && isxdigit(p[2])
			 && isxdigit(p[3])
			 && isxdigit(p[4])
			 && isxdigit(p[5])) {
				int i, v;

				v = 0;
				for (i = 2; i < 6; i++) {
					v <<= 4;
					c = p[i];
					if (c <= '9')
						v += c - '0';
					else if (c <= 'F')
						v += c - 'A' + 10;
					else
						v += c - 'a' + 10;
				}
				if (p != s)
					txt_add(s, (int) (p - s));
				p += 6;
				sz -= 6;
				if ((v & 0xdc00) == 0xd800	/* surrogates */
				 && sz >= 6
				 && *p == '\\'
				 && p[1] == 'u'
				 && isxdigit(p[2])
				 && isxdigit(p[3])
				 && isxdigit(p[4])
				 && isxdigit(p[5])) {
					int v2;

					v = (v - 0xd7c0) << 10;
					v2 = 0;
					for (i = 2; i < 6; i++) {
						v2 <<= 4;
						c = p[i];
						if (c <= '9')
							v2 += c - '0';
						else if (c <= 'F')
							v2 += c - 'A' + 10;
						else
							v2 += c - 'a' + 10;
					}
					v2 -= 0xdc00;
					v += v2;
					p += 6;
					sz -= 6;
				}
//fixme: else error
				s = p;
				if (v < 0x80) {	/* convert to UTF-8 */
					tmp[0] = v;
					i = 1;
				} else if (v < 0x800) {
					tmp[0] = 0xc0 | (v >> 6);
					tmp[1] = 0x80 | (v & 0x3f);
					i = 2;
				} else if (v < 0x10000) {
					tmp[0] = 0xe0 | (v >> 12);
					tmp[1] = 0x80 | ((v >> 6) & 0x3f);
					tmp[2] = 0x80 | (v & 0x3f);
					i = 3;
				} else {
					tmp[0] = 0xf0 | (v >> 18);
					tmp[1] = 0x80 | ((v >> 12) & 0x3f);
					tmp[2] = 0x80 | ((v >> 6) & 0x3f);
					tmp[3] = 0x80 | (v & 0x3f);
					i = 4;
				}
				txt_add(tmp, i);
				continue;
			}
			if (sz >= 3) {
				unsigned char *q;

				switch (p[1]) {
				case '`': q = grave; break;
				case '\'': q = acute; break;
				case '^': q = circumflex; break;
				case ',': q = cedilla; break;
				case '"': q = umlaut; break;
				case '~': q = tilde; break;
				case 'o': q = ring; break;
				case '=': q = macron; break;
				case '/': q = slash; break;
				case ';': q = ogonek; break;
				case 'v': q = caron; break;
				case 'u': q = breve; break;
				case 'H':
				case ':': q = hungumlaut; break;
				case '.': q = dot; break;
				default:
					q = ligature;
					do {
						if (*q == p[1]
						 && q[1] == p[2])
							break;
						q += 4;
					} while (*q != '\0');
					if (*q != '\0') {
						if (p != s)
							txt_add(s, (int) (p - s));
						txt_add(q + 2, 2);
						p += 3;
						sz -= 3;
						s = p;
						continue;
					}
					q = 0;
					break;
				}
				if (q != 0) {
					do {
						if (*q == p[2])
							break;
						q += 3;
					} while (*q != '\0');
					if (*q != '\0') {
						if (p != s)
							txt_add(s, (int) (p - s));
						txt_add(q + 1, 2);
						p += 3;
						sz -= 3;
						s = p;
						continue;
					}
				}
			}
			p++;
			sz--;
			if (*p == '\\') {
				p++;
				sz--;
			}
			continue;
		}
		if (*p >= 0x80 && latin > 0) {
			if (p != s)
				txt_add(s, (int) (p - s));
			c = *p++;
			s = p;
			sz--;
latin:
			if (c < 0xa0 || latin == 1) {
				tmp[0] = 0xc0 | ((c >> 6) & 0x03);
				tmp[1] = 0x80 | (c & 0x3f);
				txt_add(tmp, 2);
			} else {
				unsigned char *q;

				q = latin_tb[latin - 2];
				txt_add(q + (c - 0xa0) * 2, 2);
			}
			continue;
		}
		p++;
		sz--;
	}
	if (p != s)
		txt_add(s, (int) (p - s));
}

static const unsigned char eol_chars[2] = {'\r', '\n'};
static void eol0(void)
{
	txt_add((unsigned char *) &eol_chars[1], 1);
}
static void eol1(void)
{
	txt_add((unsigned char *) &eol_chars[0], 1);
}
static void eol2(void)
{
	txt_add((unsigned char *) &eol_chars[0], 2);
}
static void (*eol_tb[3])(void) = {eol0, eol1, eol2};
static void (*txt_add_eol)(void);

/* add the line number */
static void add_lnum(int nline)
{
	unsigned char tmp[16];

	sprintf((char *) tmp, "%%@%d", nline);
	txt_add(tmp, strlen((char *) tmp));
}

/* check if the current tune is to be selected */
static int tune_select(unsigned char *s)
{
	struct slre slre;
	unsigned char *p, *sel;

	/* if there is a list of tune indexes,
	 * check the tune index */
	sel = selection;
	if (isdigit(*sel)) {
		int tune_number, cur_sel, end_sel, n;

		/* get the tune number ('s' points to X:) */
		tune_number = strtod((char *) s + 2, 0);

		/* search it in the number list */
		for (;;) {
			if (sscanf((char *) sel, "%d%n", &cur_sel, &n) != 1)
				break;
			sel += n;
			if (*sel == '-') {
				sel++;
				if (sscanf((char *) sel, "%d%n", &end_sel, &n) != 1)
					end_sel = ~0u >> 1;
				else
					sel += n;
			} else {
				end_sel = cur_sel;
			}
			if (tune_number >= cur_sel && tune_number <= end_sel)
				return 1;
			if (*sel != ',')
				break;
			sel++;
		}
		if (*sel == '\0')
			return 0;
	}

	for (p = s + 2; ; p++) {
		switch (*p) {
		case '\0':
			return 0;
		default:
			continue;
		case '\n':
		case '\r':
			break;
		}
		if (p[1] != 'K' || p[2] != ':')
			continue;
		p += 3;
		while (*p != '\n' && *p != '\r' && *p != '\0')
			p++;
		if (*p != '\0')
			p++;		/* keep the EOL for RE with '\s' */
		break;
	}
//fixme: should compile only one time
	if (!slre_compile(&slre, (char *) sel))
		return 0;
	return slre_match(&slre, (char *) s, p - s, 0);
}

/* -- init the front-end -- */
void front_init(int edit,	/* for edition - keep comments */
		int eol,	/* 0: \n, 1: \r, 2: \r\n */
		void include_api(unsigned char *fn))
{
	keep_comments = edit;
	txt_add_eol = eol_tb[eol];
	include_f = include_api;
	dst = 0;
	offset = 0;
	size = 0;
}

/* -- front end parser -- */
unsigned char *frontend(unsigned char *s,
			int ftype)
{
	unsigned char *p, *q, c, *begin_end;
	int i, l, state, str_cnv_p, histo, end_len, nline;
	char prefix_sav[4];
	int latin_sav;

	begin_end = 0;
	end_len = 0;
	histo = 0;
	state = 0;
	nline = 0;
	if (dst != 0)			/* if continuation */
		offset--;		/* restart before the EOL */

	add_lnum(0);
	txt_add_eol();

	/* if unknown encoding, check if latin1 or utf-8 */
	if (ftype == FE_ABC
	 && strncmp((char *) s, "%abc-2.1", 8) == 0) {
		latin = 0;
	} else {
		for (p = s; *p != '\0'; p++) {
			c = *p;
			if (c == '\\') {
				if (p[1] == '2') {
					if (p[2] == '0')	/* accidental */
						continue;
					c = 0x80;
				} else if (p[1] == '3') {
					c = 0xc0;
				}
			}
			if (c < 0x80)
				continue;
//fixme: problem when two octal values give a UTF-8 character
			if (c >= 0xc0) {
				if ((p[1] & 0xc0) == 0x80
				 || (p[1] == '\\' && p[2] == '2')) {
					latin = 0;
					break;
				 }
			}
			latin = 1;
			break;
		}
	}

	/* scan the file */
	skip = 0;
	while (*s != '\0') {

		/* get a line */
		str_cnv_p = 0;
		p = s;
		while (*p != '\0'
		    && *p != '\r'
		    && *p != '\n') {
			if (*p == '\\'
			 || (latin > 0 && *p >= 0x80))
				str_cnv_p = 1;
			p++;
		}
		l = p - s;
		if (*p != '\0') {
			p++;
			if (p[-1] == '\r' && *p == '\n')	/* (DOS) */
				p++;
		}
		nline++;

		if (begin_end) {
			if (ftype == FE_FMT) {
				if (strncmp((char *) s, "end", 3) == 0
				 && strncmp((char *) s + 3,
						(char *) begin_end, end_len) == 0) {
					begin_end = 0;
					txt_add((unsigned char *) "%%", 2);
					goto next;
				}
				if (*s == '%')
					goto next_eol;		/* comment */
				goto next;
			}
			if (*s == '%' && strchr(prefix, s[1])) {
				q = s + 2;
				while (*q == ' ' || *q == '\t')
					q++;
				if (strncmp((char *) q, "end", 3) == 0
				 && strncmp((char *) q + 3,
						(char *) begin_end, end_len) == 0) {
					begin_end = 0;
					txt_add((unsigned char *) "%%", 2);
					l -= q - s;
					s = q;
					goto next;
				}
			}
			if (strncmp("ps", (char *) begin_end, end_len) == 0) {
				if (*s == '%')
					goto next_eol;		/* comment */
			} else {
				if (*s == '%' && strchr(prefix, s[1])) {
					s += 2;
					l -= 2;
				}
			}
			goto next;
		}

		if (skip) {
			if (l != 0)
				goto next_eol;
			skip = 0;
			txt_add_eol();
			add_lnum(nline);
		}

		if (l == 0) {			/* empty line */
			switch (state) {
			case 1:
				fprintf(stderr,
					"Line %d: Empty line in tune header - K:C added\n",
					nline);
				txt_add((unsigned char *) "K:C", 3);
				txt_add_eol();
				txt_add_eol();
				add_lnum(nline);
				/* fall thru */
			case 2:
				state = 0;
				strcpy(prefix, prefix_sav);
				latin = latin_sav;
				break;
			}
			goto next_eol;
		}
		if (histo) {			/* H: continuation */
			if ((s[1] == ':'
			  && (isalpha(*s) || *s == '+'))
			 || (*s == '%' && strchr(prefix, s[1]))) {
				histo = 0;
			} else {
				txt_add((unsigned char *) "H:", 2);
				goto next;
			}
		}

		/* special case 'space* "%" ' */
		if (*s == ' ' || *s == '\t') {
			q = s;
			do {
				q++;
			} while (*q == ' ' || *q == '\t');
			if (*q == '%') {
				if (keep_comments)
					txt_add(q, l - (q - s));
				else if (state != 0)	/* inside tune */
					txt_add(q, 1);	/* keep a single '%' */
				goto next_eol;
			}
		}

		if (ftype == FE_PS) {
			if (*s == '%')
				goto next_eol;
			goto next;
		}

		/* treat the pseudo-comments */
		if (ftype == FE_FMT) {
			if (*s == '%')
				goto next_eol;
			goto pscom;
		}
		if (*s == 'I' && s[1] == ':') {
			s += 2;
			l -= 2;
			while (*s == ' ' || *s == '\t') {
				s++;
				l--;
			}
			txt_add((unsigned char *) "%%", 2);
			goto info;
		}
		if (*s == '%') {
			if (!strchr(prefix, s[1])) {		/* pure comment */
				if (keep_comments
				 || strncmp((char *) s, "%abc", 4) == 0)
					txt_add(s, l);
				else if (state != 0)		/* if not global */
					txt_add(s, 1);
				goto next_eol;
			}
			s += 2;
			l -= 2;
			if (strncmp((char *) s, "abcm2ps ", 8) == 0) {
				s += 8;
				l -= 8;
				while (*s == ' ' || *s == '\t') {
					s++;
					l--;
				}
				for (i = 0; i < sizeof prefix - 1; i++) {
					if (*s == ' ' || *s == '\t'
					 || --l < 0)
						break;
					prefix[i] = *s++;
				}
				if (i == 0)
					prefix[i++] = '%';
				prefix[i] = '\0';
				txt_add((unsigned char *) "%", 1);
				goto next_eol;
			}
pscom:
			while (*s == ' ' || *s == '\t') {
				s++;
				l--;
			}
			txt_add((unsigned char *) "%%", 2);
			if (strncmp((char *) s, "begin", 5) == 0) {
				q = begin_end = s + 5;
				while (!isspace(*q))
					q++;
				end_len = q - begin_end;
				goto next;
			}
info:
			if (strncmp((char *) s, "encoding ", 9) == 0
			 || strncmp((char *) s, "abc-charset ", 12) == 0) {
				if (*s == 'e')
					q = s + 9;
				else
					q = s + 12;
				while (*q == ' ' || *q == '\t')
					q++;
				if (strncasecmp((char *) q, "latin", 5) == 0) {
					q += 5;
				} else if (strncasecmp((char *) q, "iso-8859-", 9) == 0) {
					q += 9;
				} else if (strncasecmp((char *) q, "utf-8", 5) == 0
					|| strncasecmp((char *) q, "native", 6) == 0) {
					latin = 0;
					goto next;
				} else if (!isdigit(*q)) {
					goto next;	/* unknown charset */
				}
				switch (*q) {
				case '1':
					if (q[1] == '0')
						latin = 6;
					else
						latin = 1;
					break;
				case '2': latin = 2; break;
				case '3': latin = 3; break;
				case '4': latin = 4; break;
				case '5':
					if (q[-1] != '-')
						latin = 5;
					break;
				case '6':
					if (q[-1] != '-')
						latin = 6;
					break;
/*fixme: iso-8859 5..8 not treated */
				case '9': latin = 5; break;
				}
				goto next;
			}
			if (include_f
			 && (strncmp((char *) s, "format ", 7) == 0
			  || strncmp((char *) s, "abc-include ", 12) == 0)) {
				unsigned char sep;

				if (*s == 'f')
					s += 7;
				else
					s += 12;
				while (*s == ' ' || *s == '\t')
					s++;
				q = s;
				while (*q != '\0'
				    && *q != '%'
				    && *q != '\n'
				    && *q != '\n'
				    && *q != '\r')
					q++;
				while (q[-1] == ' ')
					q--;
				sep = *q;
				*q = '\0';
				offset--;		/* remove one % */
				dst[offset - 1] = '\0';	/* replace the other % by EOS */
				include_f(s);
				offset--;		/* remove the EOS */
				*q = sep;
				add_lnum(nline);
				goto next_eol;
			}
			if (strncmp((char *) s, "select", 6) == 0) {
				s += 6;
				if (*s == '\n') {	/* select clear */
					q = s;
				} else if (*s != ' ' && *s != '\t') {
					goto next;
				} else {
					while (*s == ' ' || *s == '\t')
						s++;
					q = s;
					while (*q != '\0'
					    && *q != '%'
					    && *q != '\n'
					    && *q != '\r')
						q++;
					while (q[-1] == ' ' || q[-1] == '\t')
						q--;
					if (strncmp((char *) q - 5, " lock", 5) == 0)
						q -= 5;
				}
				if (selection) {
					free(selection);
					selection = NULL;
				}
				if (q != s) {
					unsigned char sep;

					sep = *q;
					*q = '\0';
					selection = (unsigned char *) strdup((char *) s);
					*q = sep;
				}
				offset--;		/* remove one % */
				goto next_eol;
			}
			goto next;
		}
		if (begin_end)
			goto next;

		/* treat the information fields */
		if (s[1] == ':' && (isalpha(*s) || *s == '+')) {
			c = *s;
			switch (c) {
			case 'I':		/* treat as a pseudo-comment */
				s += 2;
				l -= 2;
				goto pscom;
			case 'X':
				switch (state) {
				case 1:
					fprintf(stderr,
						"Line %d: X: found in tune header - K:C added\n",
						nline);
					txt_add((unsigned char *) "K:C", 3);
					txt_add_eol();
					txt_add_eol();	/* empty line */
					add_lnum(nline);
					txt_add_eol();
					break;
				case 2:
					txt_add_eol();	/* no empty line - minor error */
					break;
				}
				if (selection)
					skip = !tune_select(s);
				if (!skip) {
					state = 1;
					strcpy(prefix_sav, prefix);
					latin_sav = latin;
				}
				break;
			case 'U':
				break;
			case 'H':
				histo = 1;
				break;
			default:
				if (state == 0			/* if global */
				 && strchr("dKPQsVWw", *s) != 0) {
					if (keep_comments)
						txt_add(s, l);
					goto next_eol;		/* ignore */
				}
				if (*s == 'K')
					state = 2;
				break;
			}
			txt_add(s, 2);
			s += 2;
			l -= 2;
			while (*s == ' ' || *s == '\t') {
				s++;
				l--;
			}
			str_cnv_p = 1;
			goto next;
		}

		/* treat the music lines */
		if (state == 0)				/* if not in tune */
			goto next_eol;			/* ignore */
		if (!str_cnv_p)
			goto next;
		str_cnv_p = 0;
		for (i = 0; i < l; i++) {
			if (s[i] != '"')
				continue;
			i++;
			txt_add(s, i);
			s += i;
			l -= i;
			for (i = 0; i < l; i++) {
				if (s[i] == '"' && s[i - 1] != '\\')
					break;
			}
//fixme: if i == l, no end of string - error
			txt_add_cnv(s, i);
			s += i;
			l -= i;
			i = 0;
		}
next:
		if (str_cnv_p)
			txt_add_cnv(s, l);
		else
			txt_add(s, l);
next_eol:
		txt_add_eol();
		s = p;
	}
	txt_add((unsigned char *) "", 1);			/* EOS */
	return dst;
}

#ifdef MAIN
static void usage(void)
{
	printf("ABC frontend\n"
		"Usage: abcmfe file\n");
	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	char *p, *file, *result;
	char *fname = 0;
	FILE *fin;
	size_t fsize;

	while (--argc > 0) {
		argv++;
		p = *argv;
		if (*p == '-')
			continue;
		fname = p;
	}
	if (fname == 0)
		usage();

	if ((fin = fopen(fname, "rb")) == 0) {
		fprintf(stderr, "Cannot open '%s'\n", fname);
		exit(EXIT_FAILURE);
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

	front_init(0, 0, 0);
	result = (char *) frontend((unsigned char *) file, 0);

	fputs(result, stdout);
	return EXIT_SUCCESS;
}
#endif
