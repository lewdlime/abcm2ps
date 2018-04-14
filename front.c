/*
 * ABC front-end parser
 *
 * This file is part of abcm2ps.
 *
 * Copyright (C) 2011-2017 Jean-François Moine (http://moinejf.free.fr)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <regex.h>

#ifdef WIN32
#define strncasecmp _strnicmp
#define strdup _strdup
#endif

#include "abcm2ps.h"

static unsigned char *dst;
static int offset, size;
static unsigned char *selection;
static int latin, skip;
static char prefix[4] = {'%'};
static int state;

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
static unsigned char ring[] = "AÅaåUŮuůeœ";
static unsigned char macron[] = "AĀDĐEĒHĦIĪOŌTŦUŪaādđeēhħiīoōtŧuū"; /* and stroke! */
static unsigned char slash[] = "OØoøDĐdđLŁlł";
static unsigned char ogonek[] = "AĄEĘIĮUŲaąeęiįuų";
static unsigned char caron[] = "LĽSŠTŤZŽlľsštťzžCČEĚDĎNŇRŘcčeědďnňrř";
static unsigned char breve[] = "AĂaăEĔeĕGĞgğIĬiĭOŎoŏUŬuŭ";
static unsigned char hungumlaut[] = "OŐUŰoőuű";
static unsigned char dot[] = "ZŻzżIİiıCĊcċGĠgġEĖeė";
/* the items of this table are 4 bytes long */
static unsigned char ligature[] = "AAÅaaåAEÆaeæccçcCÇDHÐdhðngŋOEŒssßTHÞthþ";

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
		if (!dst)
			dst = malloc(size);
		else
			dst = realloc(dst, size);
		if (!dst) {
			fprintf(stderr, "Out of memory - abort\n");
			exit(EXIT_FAILURE);
		}
	}
	memcpy(dst + offset, s, sz);
	offset += sz;
}

/* add text to the output buffer translating
 * the escape sequences and the non utf-8 characters */
static void txt_add_cnv(unsigned char *s, int sz, int comment)
{
	unsigned char *p, c, tmp[4];
	int in_string = 0;

	p = s;
	while (sz > 0) {
		switch (*p) {
		case '"':
			if (comment)
				in_string = !in_string;
			break;
		case '%':
			if (in_string || !comment)
				break;
			while (--p >= s) {	// start of comment
				if (*p != ' ' && *p != '\t')
					break;
			}
			p++;
			goto done;
		case '\\':
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
			if (sz > 0) {
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
done:
	if (p != s)
		txt_add(s, (int) (p - s));
}

static void txt_add_eos(char *fname, int linenum)
{
	static unsigned char eos = '\0';

	/* special case for continuation lines in ABC version 2.0 */
	if (parse.abc_vers == (2 << 16)
	 && offset > 0
	 && dst[offset - 1] == '\\') {
		offset--;
		return;
	}
	txt_add(&eos, 1);
	abc_parse((char *) dst, fname, linenum);
	offset = 0;
}

/* get the ABC version */
static void get_vers(char *p)
{
	int i, j, k;

	i = j = k = 0;
	if (sscanf(p, "%d.%d.%d", &i, &j, &k) != 3)
		if (sscanf(p, "%d.%d", &i, &j) != 2)
			sscanf(p, "%d", &i);
	parse.abc_vers = (i << 16) + (j << 8) + k;
}

/* check if the current tune is to be selected */
static int tune_select(unsigned char *s)
{
	regex_t r;
	unsigned char *p, *sel;
	int ret;

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

	ret = p - s;
	if (ret >= TEX_BUF_SZ - 1) {
		fprintf(stderr, "Tune header too big for %%%%select\n");
		return 0;
	}
	memcpy(tex_buf, s, ret);
	tex_buf[ret] = '\0';

	ret = regcomp(&r, (char *) sel,
				REG_EXTENDED | REG_NEWLINE | REG_NOSUB);
	if (ret)
		return 0;
	ret = regexec(&r, tex_buf, 0, NULL, 0);
	regfree(&r);
	return !ret;
}

/* -- front end parser -- */
void frontend(unsigned char *s,
		int ftype,
		char *fname,
		int linenum)
{
	unsigned char *p, *q, c, *begin_end, sep;
	int i, l, str_cnv_p, histo, end_len;
	char prefix_sav[4];
	int latin_sav = 0;		/* have C compiler happy */

	begin_end = NULL;
	end_len = 0;
	histo = 0;
//	state = 0;

	if (ftype == FE_ABC
	 && strncmp((char *) s, "%abc-", 5) == 0) {
		get_vers((char *) s + 5);
		while (*s != '\0'
		    && *s != '\r'
		    && *s != '\n')
			s++;
		if (*s != '\0') {
			s++;
			if (s[-1] == '\r' && *s == '\n')
				s++;
		}
		linenum++;
	}

	/* if unknown encoding, check if latin1 or utf-8 */
	if (ftype == FE_ABC
	 && parse.abc_vers >= ((2 << 16) | (1 << 8))) {	// if ABC version >= 2.1
		latin = 0;				// always UTF-8
	} else {
		for (p = s; *p != '\0'; p++) {
			c = *p;
			if (c == '\\') {
				if (!isdigit(p[1]))
					continue;
				if ((p[1] == '0' || p[1] == '2')
				 && p[2] == '0')	/* accidental */
					continue;
				latin = 1;
				break;
			}
			if (c < 0x80)
				continue;
			if (c >= 0xc2) {
				if ((p[1] & 0xc0) == 0x80) {
					latin = 0;
					break;
				}
			}
			latin = 1;
			break;
		}
	}
	latin_sav = latin;		/* (have gcc happy) */

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
			 || *p == '%'
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
		linenum++;

		if (skip) {
			if (l != 0)
				goto ignore;
			skip = 0;
		}
		if (begin_end) {
			if (ftype == FE_FMT) {
				if (strncmp((char *) s, "end", 3) == 0
				 && strncmp((char *) s + 3,
						(char *) begin_end, end_len) == 0) {
					begin_end = NULL;
					goto next_eol;
				}
				if (*s == '%')
					goto ignore;		/* comment */
				goto next;
			}
			if (*s == '%' && strchr(prefix, s[1])) {
				q = s + 2;
				while (*q == ' ' || *q == '\t')
					q++;
				if (strncmp((char *) q, "end", 3) == 0
				 && strncmp((char *) q + 3,
						(char *) begin_end, end_len) == 0) {
					begin_end = NULL;
					goto next_eol;
				}
			}
			if (strncmp("ps", (char *) begin_end, end_len) == 0) {
				if (*s == '%')
					goto ignore;		/* comment */
			} else {
				if (*s == '%' && strchr(prefix, s[1])) {
					s += 2;
					l -= 2;
				}
			}
			goto next;
		}

		while (l > 0 && isspace(s[l - 1]))
			l--;

		if (l == 0) {			/* empty line */
			if (ftype == FE_FMT)
				goto next_eol;
			switch (state) {
			default:
				goto ignore;
			case 1:
				fprintf(stderr,
					"Line %d: Empty line in tune header - K:C added\n",
					linenum);
				txt_add((unsigned char *) "K:C", 3);
				txt_add_eos(fname, linenum);
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
			if ((s[1] == ':' && isalpha(*s))
			 || (*s == '%' && strchr(prefix, s[1]))) {
				histo = 0;
			} else {
				if (*s != '+' || s[1] != ':')
					txt_add((unsigned char *) "+:", 2);
				goto next;
			}
		}

		/* special case 'space* "%" ' */
		if (*s == ' ' || *s == '\t') {
			q = s;
			do {
				q++;
			} while (*q == ' ' || *q == '\t');
			if (*q == '%')
				goto ignore;
		}

		if (ftype == FE_PS) {
			if (*s == '%')
				goto ignore;
			goto next;
		}

		/* treat the pseudo-comments */
		if (ftype == FE_FMT) {
			if (*s == '%')
				goto ignore;
			goto pscom;
		}
		if (*s == 'I' && s[1] == ':') {
			s += 2;
			l -= 2;
			while (*s == ' ' || *s == '\t') {
				s++;
				l--;
			}
			if (l <= 0)
				goto ignore;
			txt_add((unsigned char *) "%%", 2);
			goto pcinfo;
		}
		if (*s == '%') {
			if (!strchr(prefix, s[1]))	/* pure comment */
				goto ignore;
			s += 2;
			l -= 2;
			if (strncmp((char *) s, "abc ", 4) == 0) {
				s += 4;
				l -= 4;
				goto info;
			}
			if (strncmp((char *) s, "abcm2ps ", 8) == 0
			 || strncmp((char *) s, "ss-pref ", 8) == 0) {
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
				goto ignore;
			}
			if (strncmp((char *) s, "abc-version ", 12) == 0) {
				get_vers((char *) s + 12);
				goto ignore;
			}
pscom:
			while (*s == ' ' || *s == '\t') {
				s++;
				l--;
			}
			if (l <= 0)
				goto ignore;
			txt_add((unsigned char *) "%%", 2);
			if (strncmp((char *) s, "begin", 5) == 0) {
				q = begin_end = s + 5;
				while (!isspace(*q))
					q++;
				end_len = q - begin_end;
				goto next;
			}
pcinfo:
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
			if (strncmp((char *) s, "format ", 7) == 0
			  || strncmp((char *) s, "abc-include ", 12) == 0) {
				int skip_sav;

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
				    && *q != '\r')
					q++;
				while (q[-1] == ' ')
					q--;
				sep = *q;
				*q = '\0';
				skip_sav = skip;
//fixme: pb when different encoding in included file: != behaviour .fmt or .abc...
//				latin_sav = latin;
				offset = 0;
				include_file(s);
//				latin = latin_sav;
				skip = skip_sav;
				*q = sep;
				goto ignore;
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
					sep = *q;
					*q = '\0';
					selection = (unsigned char *) strdup((char *) s);
					*q = sep;
				}
				offset = 0;
				goto ignore;
			}
			goto next;
		}

		/* treat the information fields */
info:
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
						linenum);
					txt_add((unsigned char *) "K:C", 3);
					txt_add_eos(fname, linenum);
					txt_add_eos(fname, linenum);	/* empty line */
					break;
				case 2:
					txt_add_eos(fname, linenum);	/* no empty line - minor error */
					break;
				}
				if (selection) {
					skip = !tune_select(s);
					if (skip)
						goto ignore;
				}
				state = 1;
				strcpy(prefix_sav, prefix);
				latin_sav = latin;
				break;
			case 'U':
				break;
			case 'H':
				histo = 1;
				break;
			default:
				if (state == 0			/* if global */
				 && strchr("dKPQsVWw", *s) != NULL)
					goto ignore;
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
			goto ignore;
next:
		if (str_cnv_p)
			txt_add_cnv(s, l, !begin_end);
		else
			txt_add(s, l);
		if (begin_end)
			txt_add((unsigned char *) "\n", 1);
		else
next_eol:
			txt_add_eos(fname, linenum);
ignore:
		s = p;
	}
	if (begin_end)
		fprintf(stderr,
			"Line %d: No %%%%end after %%%%begin\n",
			linenum);
	if (ftype == FE_FMT)
		return;
	if (state == 1)
		fprintf(stderr,
			"Line %d: Unexpected EOF in header definition\n",
			linenum);
	abc_eof();
}
