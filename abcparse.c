/*++
 * This module parses the ABC syntax.
 *
 * Copyright (C) 1998-2002 Jean-François Moine
 * Adapted from abc2ps, Copyright (C) 1996, 1997  Michael Methfessel
 *
 * Contact: mailto:moinejf@free.fr
 * Original site: http://moinejf.free.fr/
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
 *
 * Questions:
 *	- what about different note lengths in chord?
 *	- does a 'K:' (or some other x:) inside a 'P:' change the
 *	  key for that part only?
 *
 * To do: see 'fixme-insert'
 *
 *--*/

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "abcparse.h"

/* interface */
static void *(*alloc_f)(int size);
static void (*free_f)(void *);
static void (*level_f)(int level);
static int client_sz;
static int keep_comment;

static int abc_state;		/* parse state */
static int gulen, ulen;		/* unit note length set by M: or L: */
static char *gchord;		/* guitar chord */
static int meter;		/* upper value of time sig for n-plets */
static struct deco dc;		/* decorations */
static struct grace gr;		/* grace notes */
static int lyric_started;	/* lyric started */
static struct abcsym *lyric_start;	/* 1st note of the line for d: */
static struct abcsym *lyric_cont;	/* current symbol when d: continuation */

#define VOICE_NAME_SZ 32	/* max size of a voice name */

static unsigned char *file;	/* remaining abc file */
static short linenum;		/* current line number */
static char *scratch_line;	/* parse line */
static int line_length;		/* current line length */

static short nvoice;		/* number of voices (0..n-1) */
static struct {			/* voice table and current pointer */
	char *name;			/* voice name */
	short ulen;			/* unit note length */
	signed char carryover;		/* for interpreting > and < chars */
	unsigned char ntinext;
	signed char tinext[MAXHD];	/* chord ties */
	unsigned char pplet, qplet, rplet; /* nplet - fixme: may be global?*/
	char slur;			/* number of slur start */
	signed char add_pitch;		/* key transpose */
} voice_tb[MAXVOICE], *curvoice;

/* char table for note line parsing */
#define CHAR_BAD 0
#define CHAR_IGN 1
#define CHAR_NOTE 2
#define CHAR_REST 3
#define CHAR_ACC 4
#define CHAR_GRACE 5
#define CHAR_DECO 6
#define CHAR_GCHORD 7
#define CHAR_BSLASH 8
#define CHAR_OBRA 9
#define CHAR_BAR 10
#define CHAR_COL 11
#define CHAR_OPAR 12
static char char_tb[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, CHAR_IGN, 0, 0, 0, 0, 0, 0,	/* 00 - 0f */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* 10 - 1f */
	CHAR_IGN, CHAR_DECO, CHAR_GCHORD, CHAR_BAD,	/* (sp) ! " # */
	CHAR_BAD, CHAR_BAD, CHAR_BAD, CHAR_BAD, 	/* $ % & ' */
	CHAR_OPAR, CHAR_BAD, CHAR_BAD, CHAR_BAD, 	/* ( ) * + */
	CHAR_BAD, CHAR_BAD, CHAR_DECO, CHAR_BAD, 	/* , - . / */
	CHAR_BAD, CHAR_BAD, CHAR_BAD, CHAR_BAD, 	/* 0 1 2 3 */
	CHAR_BAD, CHAR_BAD, CHAR_BAD, CHAR_BAD, 	/* 4 5 6 7 */
	CHAR_BAD, CHAR_BAD, CHAR_COL, CHAR_BAD, 	/* 8 9 : ; */
	CHAR_BAD, CHAR_ACC, CHAR_BAD, CHAR_BAD, 	/* < = > ? */
	CHAR_BAD, CHAR_NOTE, CHAR_NOTE, CHAR_NOTE, 	/* @ A B C */
	CHAR_NOTE, CHAR_NOTE, CHAR_NOTE, CHAR_NOTE, 	/* D E F G */
	CHAR_DECO, CHAR_DECO, CHAR_DECO, CHAR_DECO, 	/* H I J K */
	CHAR_DECO, CHAR_DECO, CHAR_DECO, CHAR_DECO, 	/* L M N O */
	CHAR_DECO, CHAR_DECO, CHAR_DECO, CHAR_DECO, 	/* P Q R S */
	CHAR_DECO, CHAR_DECO, CHAR_DECO, CHAR_DECO, 	/* T U V W */
	CHAR_DECO, CHAR_DECO, CHAR_REST, CHAR_OBRA, 	/* X Y Z [ */
	CHAR_BSLASH, CHAR_BAD, CHAR_ACC, CHAR_ACC, 	/* \ ] ^ _ */
	CHAR_BAD, CHAR_NOTE, CHAR_NOTE, CHAR_NOTE, 	/* ` a b c */
	CHAR_NOTE, CHAR_NOTE, CHAR_NOTE, CHAR_NOTE, 	/* d e f g */
	CHAR_DECO, CHAR_DECO, CHAR_DECO, CHAR_DECO, 	/* h i j k */
	CHAR_DECO, CHAR_DECO, CHAR_DECO, CHAR_DECO, 	/* l m n o */
	CHAR_DECO, CHAR_DECO, CHAR_DECO, CHAR_DECO, 	/* p q r s */
	CHAR_DECO, CHAR_DECO, CHAR_DECO, CHAR_DECO, 	/* t u v w */
	CHAR_REST, CHAR_REST, CHAR_REST, CHAR_GRACE, 	/* x y z { */
	CHAR_BAR, CHAR_BAD, CHAR_DECO, CHAR_BAD, 	/* | } ~ (del) */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* 80 - 8f */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* 90 - 9f */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* a0 - af */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* b0 - bf */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* c0 - cf */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* d0 - df */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* e0 - ef */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* f0 - ff */
};

static char all_notes[] = "CDEFGABcdefgab^=_";
char *deco_tb[128];

int severity;

static unsigned char *decomment_line(unsigned char *p);
static unsigned char *get_line(void);
static char *get_tempo(unsigned char *p,
		       struct abcsym *s);
static unsigned char *parse_len(unsigned char *p,
				int *p_len);
static unsigned char *parse_clef(unsigned char *p,
				 struct clef_s *p_clef);
static int parse_line(struct abctune *t,
		      unsigned char *p);
static unsigned char *parse_note(struct abctune *t,
				 unsigned char *p);
static void parse_header(struct abctune *t,
			 unsigned char *p,
			 char *comment);
static void syntax(char *msg, char *q);

/* -- delete an ABC symbol -- */
void abc_delete(struct abcsym *as)
{
	switch (as->type) {
	case ABC_T_INFO:
		switch (as->text[0]) {
		case 'M':
			if (as->u.meter.top)
				free_f(as->u.meter.top);
			break;
		case 'Q':
			if (as->u.tempo.str)
				free_f(as->u.tempo.str);
			break;
		case 'V':
			if (as->u.voice.name)
				free_f(as->u.voice.name);
			if (as->u.voice.fname)
				free_f(as->u.voice.fname);
			if (as->u.voice.nname)
				free_f(as->u.voice.nname);
			break;
		}
		break;
	case ABC_T_NOTE:
		if (as->u.note.gr)
			free_f(as->u.note.gr);
		break;
	}
	if (as->text)
		free_f(as->text);
	if (as->comment)
		free_f(as->comment);

	if (as->prev)
		as->prev->next = as->next;
	if (as->next)
		as->next->prev = as->prev;
	if (as->tune->first_sym == as)
		as->tune->first_sym = as->next;
	if (as->tune->last_sym == as)
		if ((as->tune->last_sym = as->prev) == 0)
	free_f(as);
}

/* -- free all tunes memory areas -- */
void abc_free(struct abctune *first_tune)
{
	struct abctune *t;

	if (!free_f)
		return;
	t = first_tune;
	for (;;) {
		struct abcsym *s;

		if (t == 0)
			break;
		s = t->first_sym;

		/* free the associated symbols */
		for (;;) {
			struct abcsym *n;

			n = s->next;
			abc_delete(s);
			if ((s = n) == 0)
				break;
		}

		/* free the tune */
		{
			struct abctune *n;

			n = t->next;
			free_f(t);
			t = n;
		}
	}
}

/* -- initialize the parser -- */
void abc_init(void *alloc_f_api(int size),
	      void free_f_api(void *ptr),
	      void level_f_api(int level),
	      int client_sz_api,
	      int keep_comment_api)
{
	alloc_f = alloc_f_api;
	free_f = free_f_api;
	level_f = level_f_api;
	client_sz = client_sz_api;
	keep_comment = keep_comment_api;
}

/* -- insert an ABC description -- */
void abc_insert(char *file_api,
		struct abcsym *s)
{
	unsigned char *p;
	struct abctune *t;

	/* initialize */
	file = file_api;
	if (level_f)
		level_f(abc_state != ABC_S_GLOBAL);
	abc_state = ABC_S_TUNE;
	linenum = 0;
	t = s->tune;
	t->last_sym = s;

	/* scan till end of description */
	for (;;) {
		if ((p = get_line()) == 0)
			break;			/* done */

		if (*p == '\0')
			return;			/* blank line --> done */

/*fixme-insert: don't accept X: nor T:*/
		/* parse the music line */
		if (!parse_line(t, p))
			return;
	}
}

/* -- new symbol -- */
struct abcsym *abc_new(struct abctune *t,
		       unsigned char *p,
		       unsigned char *comment)
{
	struct abcsym *s;

	s = alloc_f(sizeof *s + client_sz);
	memset(s, 0, sizeof *s + client_sz);
	s->tune = t;
	if (p != 0) {
		s->text = alloc_f(strlen(p) + 1);
		strcpy(s->text, p);
	}
	if (comment != 0) {
		s->comment = alloc_f(strlen(comment) + 1);
		strcpy(s->comment, comment);
	}
	if (t->last_sym == 0)
		t->first_sym = t->last_sym = s;
	else	{
		if ((s->next = t->last_sym->next) != 0)
			s->next->prev = s;
		t->last_sym->next = s;
		s->prev = t->last_sym;
		t->last_sym = s;
	}
	s->linenum = linenum;
	return s;
}

/* -- parse an ABC file -- */
struct abctune *abc_parse(char *file_api)
{
	unsigned char *p;
	struct abctune *first_tune = 0;
	struct abctune *t, *last_tune;

	/* initialize */
	file = file_api;
	t = 0;
	abc_state = ABC_S_GLOBAL;
	if (level_f)
		level_f(0);
	linenum = 0;
	last_tune = 0;
	gulen = 0;

	/* scan till end of file */
	for (;;) {
		if ((p = get_line()) == 0) {
			if (abc_state == ABC_S_HEAD) {
				printf("\n+++ unexpected EOF in header definition at line %d\n",
				       linenum);
				severity = 1;
			}
			break;			/* done */
		}

		/* start a new tune if not done */
		if (t == 0) {
			struct abctune *n;

			if (*p == '\0')
				continue;
			n = alloc_f(sizeof *n);
			memset(n, 0 , sizeof *n);
			if (last_tune == 0)
				first_tune = n;
			else	{
				last_tune->next = n;
				n->prev = last_tune;
			}
			last_tune = t = n;
			ulen = gulen;
		}

		/* parse the music line */
		if (!parse_line(t, p))
			t = 0;
	}
	return first_tune;
}

/* -- cut off after % and remove trailing blanks -- */
static unsigned char *decomment_line(unsigned char *p)
{
	int i;
	unsigned char c, *comment = 0;

	i = 0;
	for (;;) {
		if ((c = *p++) == '%') {
			if (p[-2] != '\\') {
				comment = p;
				c = '\0';
			}
		}
		if (c == '\0') {
			p--;
			break;
		}
		i++;
	}

	/* remove trailing blanks */
	while (--i > 0) {
		c = *--p;
		if (!isspace(c)) {
			p[1] = '\0';
			break;
		}
	}
	return keep_comment ? comment : 0;
}

/* -- define a voice by name -- */
/* the voice is created if it does not exist */
static char *def_voice(unsigned char *p,
		       int *p_voice)
{
	char *name;
	char sep;
	int voice;

	name = p;
	while (isalnum(*p) || *p == '_')
		p++;
	sep = *p;
	*p = '\0';

	if (voice_tb[0].name == 0)
		voice = 0;		/* first voice */
	else {
		for (voice = 0; voice <= nvoice; voice++) {
			if (strcmp(name, voice_tb[voice].name) == 0)
				goto done;
		}
		if (voice >= MAXVOICE) {
			syntax("Too many voices", name);
			voice--;
		}
	}
	nvoice = voice;
	voice_tb[voice].name = alloc_f(strlen(name) + 1);
	strcpy(voice_tb[voice].name, name);
done:
	*p_voice = voice;
	*p = sep;
	return p;
}

/* -- treat the broken rhythm '>' and '<' -- */
static void broken_rhythm(struct note *note,
			  int num)	/* >0: do dot, <0: do half */
{
	int m, n;

	num *= 2;
	if (num > 0) {
		n = num * 2 - 1;
		for (m = 0; m <= note->nhd; m++)
			note->lens[m] = (note->lens[m] * n) / num;
	} else {
		n = -num;
		for (m = 0; m <= note->nhd; m++)
			note->lens[m] /= n;
		if (note->lens[0] < BASE_LEN)
			note->stemless = 0;
	}
}

/* -- parse a decoration '[!]xxx[!]' -- */
static char *get_deco(unsigned char *p,
		      unsigned char *p_deco)
{
	unsigned char *q;
	char **t;
	int i, l;

	*p_deco = 0;
	/* we are after the '!' */
	q = p;
	while (*p != '!') {
		if (*p == '\0') {
			if (q[-1] != '!')
				break;
			syntax("Decoration not terminated", q);
			return p;
		}
		p++;
	}
	l = p - q;
	if (*p == '!')
		p++;
	for (i = 1, t = &deco_tb[1];
	     *t != 0 && i < 128;
	     i++, t++) {
		if (strlen(*t) == l
		    && strncmp(*t, q, l) == 0) {
			*p_deco = i + 128;
			return p;
		}
	}

	/* new decoration */
	if (i < 128) {
		if (level_f && abc_state != ABC_S_GLOBAL)
			level_f(0);
		*t = alloc_f(l + 1);
		if (level_f && abc_state != ABC_S_GLOBAL)
			level_f(1);
		memcpy(*t, q, l);
		(*t)[l] = '\0';
		*p_deco = i + 128;
	} else	syntax("Too many decoration types", q);
	return p;
}

/* -- parse a K: -- */
static void parse_key(unsigned char *p,
		      struct abcsym *s)
{
	int sf, j;
	char w[81];
	int bagpipe;
	struct clef_s clef;
	int minor = 0;

	/* check for clef alone */
	p = parse_clef(p, &clef);
	if (*p == '\0') {
		if (clef.type >= 0) {
			struct abcsym *s2;

			s2 = abc_new(s->tune, 0, 0);
			s2->type = ABC_T_CLEF;
			memcpy(&s2->u.clef, &clef, sizeof s2->u.clef);
			s->u.key.empty = 1;
		}
		return;
	}

	bagpipe = 0;
	sf = 0;
	switch (*p) {
	case 'F':
		sf = -1;
		break;
	case 'B':
		sf++;
	case 'E':
		sf++;
	case 'A':
		sf++;
	case 'D':
		sf++;
	case 'G':
		sf++;
	case 'C':
		break;
	case 'H':
		bagpipe = 1;
		p++;
		if (*p == 'P')
			;
		else if (*p == 'p')
			sf = 2;
		else	syntax("Unknown bagpipe-like key", p);
		break;
	default:
		syntax("Key not recognized", p);
		break;
	}
	p++;
	if (*p == '#') {
		sf += 7;
		p++;
	} else if (*p == 'b') {
		sf -= 7;
		p++;
	}

	/* loop over blank-delimited words: get the next token in lower case */
	for (;;) {
		while (isspace(*p))
			p++;
		if (*p == '\0')
			break;

		/* check for clef */
		p = parse_clef(p, &clef);
		if (clef.type >= 0) {
			struct abcsym *s2;

			s2 = abc_new(s->tune, 0, 0);
			s2->type = ABC_T_CLEF;
			memcpy(&s2->u.clef, &clef, sizeof s2->u.clef);
			continue;
		}

		j = 0;
		while (*p != '\0' && !isspace(*p)) {
			if (j < sizeof w - 1)
				w[j++] = tolower(*p);
			p++;
		}
		w[j] = '\0';

		/* now identify this word */

		/* check for mode specifier */
		if (strncmp(w, "mix", 3) == 0)
			sf -= 1;
		/* dorian mode on the second note (D in C scale) */
		else if (strncmp(w, "dor", 3) == 0)
			sf -= 2;
		/* phrygian mode on the third note (E in C scale) */
		else if (strncmp(w, "phr", 3) == 0)
			sf -= 4;
		/* lydian mode on the fourth note (F in C scale) */
		else if (strncmp(w, "lyd", 3) == 0)
			sf += 1;
		/* locrian mode on the seventh note (B in C scale) */
		else if (strncmp(w, "loc", 3) == 0)
			sf -= 5;
		/* major and ionian are the same keysig */
		else if (strncmp(w, "maj", 3) == 0)
			;
		else if (strncmp(w, "ion", 3) == 0)
			;
		/* aeolian, m, minor are the same keysig - sixth note (A in C scale) */
		else if (strncmp(w, "aeo", 3) == 0
			 || strncmp(w, "min", 3) == 0
			 || strcmp(w, "m") == 0) {
			sf -= 3;
			minor = 1;
		/* check for "+8" or "-8" */
		} else if (!strcmp(w, "+8"))
			curvoice->add_pitch += 7;
		else if (!strcmp(w, "-8"))
			curvoice->add_pitch -= 7;
		else	syntax("Unknown token in key specifier", p);

	}  /* end of loop over blank-delimited words */

	s->u.key.sf = sf;
	s->u.key.bagpipe = bagpipe;
	s->u.key.minor = minor;
}

/* -- set default length from 'L:' -- */
static char *get_len(unsigned char *p,
		     struct abcsym *s)
{
	int l1, l2, d;
	char *error_txt = 0;

	l1 = 0;
	l2 = 1;
	if (sscanf(p, "%d /%d ", &l1, &l2) != 2
	    || l1 == 0) {
		s->u.length.base_length = ulen ? ulen : BASE_LEN / 8;
		return "Bad unit note length: unchanged";
	}

	d = BASE_LEN / l2;
	if (d * l2 != BASE_LEN) {
		error_txt = "Length incompatible with BASE, using 1/8";
		d = BASE_LEN / 8;
	} else 	{
		d *= l1;
		if (l1 != 1
		    || (l2 & (l2 - 1))) {
			error_txt = "Incorrect unit note length, using 1/8";
			d = BASE_LEN / 8;
		}
	}
	s->u.length.base_length = d;
	return error_txt;
}

/* -- get a new line from the current file in memory -- */
static unsigned char *get_line(void)
{
	int l;
	static int scratch_length = 0;
	unsigned char *p;
	unsigned char *line;

	p = file;
	if (*p == '\0')
		return 0;
	line = p; 		/* (for syntax error) */

	/* memorize the beginning of the next line */
	while (*p != '\0'
	       && *p != '\r'
	       && *p != '\n') {
		p++;
	}
	l = p - line;
	if (*p != '\0')
		p++;
	/* solve PC-DOS */
	if (p[-1] == '\r' && *p == '\n')
		p++;
	file = p;

	linenum++;

	/* allocate space for the line */
	if (scratch_line != 0
	    && l >= scratch_length) {
		free(scratch_line);
		scratch_line = 0;
	}
	if (scratch_line == 0) {
		scratch_line = malloc(l + 1);
		scratch_length = l;
	}
	p = scratch_line;
	strncpy(p, line, l);
	p[l] = '\0';
	line_length = l;	/* for syntax error */

	/* skip starting blanks */
	while (isspace(*p))
		p++;
	return p;
}

/* -- treat a 'M:' -- */
static char *parse_meter(unsigned char *p,
			 struct abcsym *s)
{
	int m1, m2, m1a, m1b, d;
	int mflag;
	unsigned char *q;

	if (*p == '\0')
		return "Empty meter string";
	m1 = m2 = 4;
	mflag = 0;
	if (*p == 'C') {
		if (p[1] == '|') {
			m1 = 2;
			m2 = 2;
			mflag = 2;
		} else	mflag = 1;
	} else if (*p == 'N' || *p == 'n') {
		m1 = 0;			/* no meter */
	} else {
		m2 = m1a = m1b = 0;
		q = strchr(p, '/');
		if (!q) {
			if (sscanf(p, "%d", &m1) != 1)
				return "Cannot identify meter";
		} else {
			if (strchr(p, '+')) {
				sscanf(p, "%d+%d+%d/", &m1, &m1a, &m1b);
				m1 += m1a + m1b;
			} else {
				sscanf(p, "%d %d %d/", &m1, &m1a, &m1b);
				if (m1 > 30		/* '78/8' -> '7 8/8' */
				    && m1a == 0) {
					m1b = m1 % 10;
					m1a = (m1 / 10) % 10;
					m1 %= 10;
				}
				if (m1 < m1a)
					m1 = m1a;
				if (m1 < m1b)
					m1 = m1b;
			}

			q++;
			if (sscanf(q, "%d", &m2) != 1
			    || m1 * m2 == 0)
				return "Cannot identify meter";

			d = BASE_LEN / m2;
			if (d * m2 != BASE_LEN)
				return "Meter not recognized";
		}
	}

	s->u.meter.m1 = m1;
	s->u.meter.m2 = m2;
	s->u.meter.flag = mflag;
	meter = m1;			/* (for n-plets) */

	/* extract the top meter string */
	if (mflag == 0
	    && *p != '\0') {
		int l;

		q = strchr(p, '/');
		if (q)
			l = q - p;
		else	l = strlen(p);
		s->u.meter.top = alloc_f(l + 1);
		strncpy(s->u.meter.top, p, l);
		s->u.meter.top[l] = '\0';
	}

	/* if in the header, change the unit note length */
	if (abc_state == ABC_S_HEAD && ulen == 0) {
		if (4 * m1 < 3 * m2)
			ulen = BASE_LEN / 16;
		else	ulen = BASE_LEN / 8;
	}
	return 0;
}

/* -- treat %%staves -- */
static void get_staves(unsigned char *p,
		       struct abcsym *s)
{
	int voice;
	char flags, flags2;
	struct staff_s *staff;

	/* define the voices */
	flags = 0;
	staff = 0;
	voice = 0;
	while (*p != '\0') {
		switch (*p) {
		case ' ':
		case '\t':
			break;
		case '[':
			if (flags & (OPEN_BRACKET | OPEN_BRACE | OPEN_PARENTH))
				goto err;
			flags |= OPEN_BRACKET;
			staff = 0;
			break;
		case ']':
			if (staff == 0)
				goto err;
			staff->flags |= CLOSE_BRACKET;
			break;
		case '{':
			if (flags & (OPEN_BRACKET | OPEN_BRACE | OPEN_PARENTH))
				goto err;
			flags |= OPEN_BRACE;
			staff = 0;
			break;
		case '}':
			if (staff == 0)
				goto err;
			staff->flags |= CLOSE_BRACE;
			break;
		case '(':
			if (flags & OPEN_PARENTH)
				goto err;
			flags |= OPEN_PARENTH;
			staff = 0;
			break;
		case ')':
			if (staff == 0)
				goto err;
			staff->flags |= CLOSE_PARENTH;
			break;
		case '|':
			if (staff == 0)
				goto err;
			staff->flags |= STOP_BAR;
			break;
		default:
			if (!isalnum(*p) && *p != '_')
				goto err;
			{
				int v;

				p = def_voice(p, &v);
				staff = &s->u.staves[voice];
				voice++;
				staff->voice = v;
				staff->name = alloc_f(strlen(voice_tb[v].name) + 1);
				strcpy(staff->name, voice_tb[v].name);
			}
			staff->flags = flags;
			flags = 0;
			continue;
		}
		p++;
	}

	/* check for errors */
	flags = CLOSE_BRACKET | CLOSE_BRACE | CLOSE_PARENTH;	/* bad flags */
	flags2 = flags;
	for (voice = 0, staff = s->u.staves;
	     voice <= MAXVOICE && staff->name;
	     voice++, staff++) {
		if (staff->flags & flags)
			goto err;
		if (staff->flags & CLOSE_PARENTH)
			flags = flags2;
		if (staff->flags & OPEN_BRACKET) {
			flags &= ~CLOSE_BRACKET;
			flags |= OPEN_BRACKET | OPEN_BRACE;
		} else if (staff->flags & CLOSE_BRACKET) {
			flags &= ~(OPEN_BRACKET | OPEN_BRACE);
			flags |= CLOSE_BRACKET;
		} else if (staff->flags & OPEN_BRACE) {
			flags &= ~CLOSE_BRACE;
			flags |= OPEN_BRACKET | OPEN_BRACE;
		} else if (staff->flags & CLOSE_BRACE) {
			flags &= ~(OPEN_BRACKET | OPEN_BRACE);
			flags |= CLOSE_BRACE;
		}
		if (staff->flags & OPEN_PARENTH) {
			flags2 = flags;
			flags &= ~CLOSE_PARENTH;
		}
	}
	return;

err:
	syntax("%%%%staves error", p);
}

/* -- get a possibly quoted string -- */
char *get_str(unsigned char *d,		/* destination */
	      unsigned char *s,		/* source */
	      int maxlen)		/* max length */
{
	char sep, c;

	maxlen--;		/* have place for the EOS */
	while (isspace(*s))
		s++;

	if (*s == '"') {
		sep = '"';
		s++;
	} else	sep = ' ';
	while ((c = *s) != '\0') {
		if (c == sep
		    || (c == '\t' && sep == ' ')) {
			if (sep != ' ')
				s++;
			break;
		}
		if (c == '\\'
		   && (c == sep
		       || (c == '\t' && sep == ' '))) {
			s++;
			continue;
		}
		if (--maxlen > 0)
			*d++ = c;
		s++;
	}
	*d = '\0';
	while (isspace(*s))
		s++;
	return s;
}

/* -- get a tempo (Q:) -- */
static char *get_tempo(unsigned char *p,
		       struct abcsym *s)
{
	int len = ulen ? ulen : BASE_LEN / 8;
	int value = 0;
	int have_error = 0;

	/* get the string if any */
	if (*p == '"') {
		unsigned char *q;
		int l;

		q = ++p;
		while (*p != '"' && *p != '\0')
			p++;
		l = p - q;
		s->u.tempo.str = alloc_f(l + 1);
		strncpy(s->u.tempo.str, q, l);
		s->u.tempo.str[l] = '\0';
		if (*p == '"')
			p++;
		while (isspace(*p))
			p++;
	}

	/* get the tempo indication if specified */
	if (*p == 'C' || *p == 'c'
	    || *p == 'L' || *p == 'l') {
		p = parse_len(p + 1, &len);
		while (isspace(*p))
			p++;
		if (*p != '=')
			have_error++;
		else	value = atoi(++p);
	} else if (isdigit(*p)) {
		int top, bot;

		if (sscanf(p, "%d /%d =%d", &top, &bot, &value) == 3)
			len = (BASE_LEN * top) / bot;
		else	value = atoi(p);
	} else if (*p != '\0')
		have_error++;

	if (len <= 0 || value < 0)
		have_error++;
	if (have_error) {
		len = ulen ? ulen : BASE_LEN / 8;
		value = 0;
	}

	s->u.tempo.length = len;
	s->u.tempo.value = value;
	return have_error ? "Invalid tempo specifier" : 0;
}

/* -- get a user defined accent (U:) -- */
static char *get_user(unsigned char *p,
		      struct abcsym *s)
{
	if (char_tb[*p] != CHAR_DECO)
		return "Invalid decoration";
	s->u.user.symbol = *p++;

	/* '=' and '!' are not important */
	while (isspace(*p)
	       || *p == '=' || *p == '!')
		p++;
	get_deco(p, &s->u.user.value);
	return 0;
}

/* -- get the voice parameters (V:) -- */
static char *get_voice(unsigned char *p,
		       struct abcsym *s)
{
	int voice;
	char *error_txt = 0;
	char name[VOICE_NAME_SZ];
static struct kw_s {
	char *name;
	short len;
	short index;
} kw_tb[] = {
	{"name=", 5, 0},
	{"nm=", 3, 0},
	{"subname=", 7, 1},
	{"sname=", 6, 1},
	{"snm=", 4, 1},
	{"clef=", 5, 2},
	{"cl=", 3, 2},
	{0, 0, 0}
};
	struct kw_s *kw;

	/* save the unit note length of the previous voice */
	curvoice->ulen = ulen;

	if (voice_tb[0].name == 0) {
		switch (s->prev->type) {
		case ABC_T_EOLN:
		case ABC_T_NOTE:
		case ABC_T_REST:
		case ABC_T_BAR:
			/* the previous voice was implicit (after K:) */
			voice_tb[0].name = alloc_f(2);
			strcpy(voice_tb[0].name, "1");
			break;
		default:
			/* declaration of the first voice */
			nvoice = -1;
			break;
		}
	}
	{
		int voice2;

		p = def_voice(p, &voice2);
		voice = voice2;
	}
	curvoice = &voice_tb[voice];
	s->u.voice.voice = voice;
	s->u.voice.name = alloc_f(strlen(curvoice->name) + 1);
	strcpy(s->u.voice.name, curvoice->name);

	/* if in tune, set the unit note length */
	if (abc_state == ABC_S_TUNE
	    && curvoice->ulen != 0)
		ulen = curvoice->ulen;

	/* get other parameters */
	while (isspace(*p))
		p++;
	while (*p != '\0') {
		for (kw = kw_tb; kw->name; kw++) {
			if (strncmp(p, kw->name, kw->len) == 0)
				break;
		}
		if (!kw->name) {
			p++;		/* ignore unknown keywords */
			continue;
		}
		p += kw->len;
		switch (kw->index) {
		case 0:			/* name */
			p = get_str(name, p, VOICE_NAME_SZ);
			s->u.voice.fname = alloc_f(strlen(name) + 1);
			strcpy(s->u.voice.fname, name);
			break;
		case 1:			/* subname */
			p = get_str(name, p, VOICE_NAME_SZ);
			s->u.voice.nname = alloc_f(strlen(name) + 1);
			strcpy(s->u.voice.nname, name);
			break;
		case 2: {		/* clef */
			struct clef_s clef;

			p = parse_clef(p - 5, &clef);
			if (clef.type >= 0) {
				struct abcsym *s2;

				s2 = abc_new(s->tune, 0, 0);
				s2->type = ABC_T_CLEF;
				memcpy(&s2->u.clef, &clef, sizeof s2->u.clef);
			} else	error_txt = "Unknown clef";
			break;
		   }
		}
	}
	return error_txt;
}

/* -- sort the notes in a chord (lowest first) -- */
void note_sort(struct abcsym *s)
{
	int m = s->u.note.nhd;

	for (;;) {
		int i;
		int nx = 0;

		for (i = 1; i <= m; i++) {
			if (s->u.note.pits[i] < s->u.note.pits[i-1]) {
				int k;
#define xch(a, b) k = a; a = b; b = k
				xch(s->u.note.pits[i], s->u.note.pits[i-1]);
#if 0
				xch(s->u.note.lens[i], s->u.note.lens[i-1]);
#endif
				xch(s->u.note.accs[i], s->u.note.accs[i-1]);
				xch(s->u.note.sl1[i], s->u.note.sl1[i-1]);
				xch(s->u.note.sl2[i], s->u.note.sl2[i-1]);
				xch(s->u.note.ti1[i], s->u.note.ti1[i-1]);
				xch(s->u.note.ti2[i], s->u.note.ti2[i-1]);
#undef xch
				nx++;
			}
		}
		if (nx == 0)
			break;
	}
}

/* -- parse a bar -- */
static char *parse_bar(struct abctune *t,
		       unsigned char *p,
		       enum bar_type bar_type)
{
	struct abcsym *s;
	char *q;
	char repeat_value[16];

	s = abc_new(t, gchord, 0);
	if (gchord) {
		if (free_f)
			free_f(gchord);
		gchord = 0;
	}
	s->type = ABC_T_BAR;
	s->state = ABC_S_TUNE;
	s->u.bar.type = bar_type;

	if (dc.n > 0) {
		memcpy(&s->u.bar.dc, &dc, sizeof s->u.bar.dc);
		dc.n = 0;
	}
	if (!isdigit(*p)) {	/* no repeat */
		int n;

		if (*p != '/')
			return p;

		/* measure repeat */
		n = 0;
		while (*p == '/') {
			n++;
			p++;
		}
		s = abc_new(t, 0, 0);
		s->type = ABC_T_MREP;
		s->state = ABC_S_TUNE;
		s->u.note.lens[0] = n;
		return p;
	}

	q = repeat_value;
	while (isdigit(*p)
	       || *p == ','
	       || *p == '-') {
		if (q < &repeat_value[sizeof repeat_value - 1])
			*q++ = *p++;
		else	p++;
	}
	*q = '\0';
	if (bar_type != B_INVIS
	    || s->text != 0) {
		s = abc_new(t, repeat_value, 0);
		s->type = ABC_T_BAR;
		s->state = ABC_S_TUNE;
		s->u.bar.type = B_INVIS;
	} else {
		s->text = alloc_f(strlen(repeat_value) + 1);
		strcpy(s->text, repeat_value);
	}
	return p;
}

/* -- parse note or rest with pitch and length -- */
static char *parse_basic_note(char *p,
			      int *pitch,
			      int *length,
			      int *accidental,
			      int *stemless)
{
	int pit, len, acc, nostem;

	acc = pit = nostem = 0;

	/* look for accidental sign */
	switch (*p) {
	case '^':
		if (p[1] == '^') {
			acc = A_DS;
			p++;
		} else	acc = A_SH;
		break;
	case '=':
		acc = A_NT;
		break;
	case '_':
		if (p[1] == '_') {
			acc = A_DF;
			p++;
		} else	acc = A_FT;
		break;
	}
	if (acc)
		p++;
	{
		char *p_n;

		p_n = strchr(all_notes, *p);
		if (p_n == 0
		    || p_n - all_notes >= 14) {
			if (acc)
				syntax("Missing note after accidental",
				       p);
			else	syntax("Not a note", p);
			pit = 16 + 7;	/* 'c' */
		} else	pit = p_n - all_notes + 16;
		p++;
	}

	while (*p == '\'') {		/* eat up following ' chars */
		pit += 7;
		p++;
	}

	while (*p == ',') {		/* eat up following , chars */
		pit -= 7;
		p++;
	}

	if (*p == '0') {
		nostem = 1;
		p++;
	}

	p = parse_len(p, &len);
	len = len * ulen / BASE_LEN;

	*pitch = pit + curvoice->add_pitch;
	*length = len;
	*accidental = acc;
	*stemless = len >= BASE_LEN ? 1 : nostem;

	return p;
}

/* -- parse a clef (K: or V:) -- */
static unsigned char *parse_clef(unsigned char *p,
				 struct clef_s *p_clef)
{
	int clef = -1;
	int explicit_clef = 0;
	int transpose = 0;
	int clef_line = 0;

	memset(p_clef, 0, sizeof *p_clef);
	if (strncmp(p, "clef=", 5) == 0) {
		p += 5;
		explicit_clef = 1;
		switch (*p) {
		case 'g':
			transpose = -7;
		case 'G':
			clef = TREBLE;
			clef_line = 2;
			break;
		case 'f':
			transpose = 14;
			clef = BASS;
			clef_line = 4;
			break;
		case 'F':
			transpose = 7;
			clef = BASS;
			clef_line = 4;
			break;
		case 'c':
			transpose = 7;
		case 'C':
			clef_line = 3;
			clef = ALTO;
			break;
		}
		if (clef >= 0) {
			p++;
			while (*p == ',') {
				transpose -= 7;
				p++;
			}
			while (*p == '\'') {
				transpose += 7;
				p++;
			}
		}
	}
	if (clef < 0) {
		if (!strncmp(p, "bass", 4)) {
			clef = BASS;
			clef_line = 4;
#ifdef CLEF_TRANSPOSE
			transpose = 14;
#endif
			p += 4;
		} else if (!strncmp(p, "treble", 6)) {
			clef = TREBLE;
			clef_line = 2;
			p += 6;
		} else if (!strncmp(p, "alto", 4)) {
			clef = ALTO;
			clef_line = 3;
#ifdef CLEF_TRANSPOSE
			transpose = 7;
#endif
			p += 4;
		} else if (!strncmp(p, "tenor", 4)) {
			clef = ALTO;
			clef_line = 4;
#ifdef CLEF_TRANSPOSE
			transpose = 7;
#endif
			p += 5;
		} else	{
			p_clef->type = -1;
			return p;
		}
	}

	p_clef->type = clef;
	p_clef->transpose = transpose;
	curvoice->add_pitch = -transpose;
	switch (*p) {
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
		clef_line = *p++ - '0';
		break;
	}
	p_clef->line = clef_line;
	if (p[1] == '8') {
		if (*p == '-') {
			p_clef->octave = -1;
			p += 2;
		} else if (*p == '+') {
			p_clef->octave = 1;
			p += 2;
		}
	}
	while (isspace(*p))
		p++;

	return p;
}

/* -- parse for decoration on note/bar -- */
unsigned char *parse_deco(unsigned char *p,
			  struct deco *deco)
{
	int n;
	unsigned char c, d;

	n = deco->n;
	for (;;) {
		c = *p++;
		if (char_tb[c] != CHAR_DECO)
			break;

		d = (unsigned char) c;
		if (c == '!')
			p = get_deco(p, &d);
		if (n >= MAXDC)
			syntax("Too many decorations for the note", p);
		else	deco->t[n++] = d;
	}
	deco->n = n;
	return p - 1;
}

/* -- parse a decoration line (d:) -- */
static char *parse_decoline(unsigned char *p)
{
	struct abcsym *is;
	unsigned char d;
	int n;

	if ((is = lyric_cont) == 0)
		is = lyric_start;
	else	lyric_cont = 0;

	/* scan the decoration line */
	while (*p != '\0') {
		while (isspace(*p))
			p++;
		if (*p == '\0')
			break;
		switch (*p) {
		case '|':
			while (is != 0 && is->type != ABC_T_BAR)
				is = is->next;
			if (is == 0) {
				syntax("Not enough bar lines for lyric line", p);
				return 0;
			}
			is = is->next;
			p++;
			continue;
		case '*':
			while (is != 0 && is->type != ABC_T_NOTE)
				is = is->next;
			if (is == 0) {
				syntax("Not enough notes for decoration line", p);
				return 0;
			}
			is = is->next;
			p++;
			continue;
		case '\\':
			if (p[1] == '\0') {
				if (is == 0)
					return "Not enough notes for decoration line";
				lyric_cont = is;
				return 0;
			}
			syntax("'\\' ignored", p);
			p++;
			continue;
		case '!':
			p = get_deco(p + 1, &d);
			break;
		default:
			d = *p++;
			break;
		}

		/* store the decoration in the next note */
		while (is != 0 && is->type != ABC_T_NOTE)
			is = is->next;
		if (is == 0)
			return "Not enough notes for decoration line";

		n = is->u.note.dc.n;
		if (n >= MAXDC)
			syntax("Too many decorations for the note", p);
		is->u.note.dc.t[n] = d;
		is->u.note.dc.n = n + 1;
		is = is->next;
	}
#if 0
	while (is != 0 && is->type != ABC_T_NOTE)
		is = is->next;
	if (is != 0)
		return "Not enough words for lyric line";
#endif
	return 0;
}

/* -- parse extra characters after note -- */
static char *parse_extra(char *p,
			 struct note *note)
{
	int i;

	for (;;) {
		switch (*p++) {
		case ' ':
		case '\t':
			note->word_end = 1;
			break;
		case '-':		/* tie */
			for (i = 0; i <= note->nhd; i++)
				note->ti1[i] = 1;
			break;
		case ')':
			note->slur_end++;
#if 0
			/*fixme: count the nb of '(' and ')'*/
			syntax("Unexpected symbol", p - 1);
#endif
			break;
		case '>':
			i = 1;
			while (*p == '>') {
				i++;
				p++;
			}
			broken_rhythm(note, i);
			curvoice->carryover = -i;
			break;
		case '<':
			i = -1;
			while (*p == '<') {
				i--;
				p++;
			}
			broken_rhythm(note, i);
			curvoice->carryover = -i;
			break;
		case '\0':
			note->word_end = 1;
			/* fall thru */
		default:
			return p - 1;
		}
	}
	/*not reached*/
}

/* -- parse a grace note sequence -- */
static unsigned char *parse_grace(unsigned char *p)
{
	unsigned char *q;
	int pit, len, acc;
	int n = 0;

	q = p;
	if (*p == '/') {
		gr.sappo = 1;	/* short appoggiatura */
		p++;
	}
	while (*p != '}') {
		if (*p == '\0') {
			syntax("Unbalanced grace note sequence",
			       q);
			return p;
		}
		if (strchr(all_notes, *p) == 0) {
			syntax("Unexpected symbol in grace note sequence",
			       p);
			p++;
		} else	{
			int tmp;

			p = parse_basic_note(p,
					     &pit,
					     &len,
					     &acc,
					     &tmp);
			if (n >= MAXGR)
				syntax("Too many grace notes",
				       p);
			else {
				gr.p[n] = pit;
				gr.a[n] = acc;
				n++;
			}
		}
	}
	p++;
	gr.n = n;
	if (n != 1)
		gr.sappo = 0;
	return p;
}

/* -- parse a note length -- */
static unsigned char *parse_len(unsigned char *p,
				int *p_len)
{
	int len, fac;

	len = BASE_LEN;
	if (isdigit(*p)) {
		len *= strtol(p, 0, 10);
		while (isdigit(*p))
			p++;
	}
	fac = 1;
	while (*p == '/') {
		p++;
		if (isdigit(*p)) {
			fac *= strtol(p, 0, 10);
			while (isdigit(*p))
				p++;
		} else	fac *= 2;
		if (len % fac) {
			syntax("Bad length divisor", p - 1);
			break;
		}
	}
	len /= fac;
	*p_len = len;
	return p;
}

/* -- parse a music line -- */
/* return 0 at end of tune */
static int parse_line(struct abctune *t,
		      unsigned char *p)
{
	struct abcsym *s;
	unsigned char *comment;
	static char qtb[10] = {1, 1, 3, 2, 3, 0, 2, 0, 3, 0};

again:					/* for history */
	switch (*p) {
	case '\0':
		switch (abc_state) {
		case ABC_S_GLOBAL:
		case ABC_S_HEAD:	/*fixme: may have blank lines in headers*/
			if (keep_comment) {
				s = abc_new(t, 0, 0);
				s->type = ABC_T_NULL;
				s->state = abc_state;
			}
			return 1;
		}
		abc_state = ABC_S_GLOBAL;
		if (level_f)
			level_f(0);
		return 0;
	case '%':
		if (p[1] == '%') {
			int in_text_block;

			comment = decomment_line(p + 2);
			in_text_block = 0;
			for (;;) {
				s = abc_new(t, p, comment);
				s->type = ABC_T_PSCOM;
				s->state = abc_state;
				if (in_text_block) {
					if (strncmp(p, "%%endtext", 9) == 0)
						break;
				} else if (strncmp(p, "%%begintext", 11) == 0)
					in_text_block = 1;
				else	break;
				if ((p = get_line()) == 0) {
					syntax("EOF while parsing %%begintext pseudo-comment",
					       scratch_line);
					break;
				}
				comment = 0;
			}
			p += 2;				/* skip '%%' */
			if (strncasecmp(p, "fmt ", 4) == 0)
				p += 4;			/* skip 'fmt' */
			if (strncmp(p, "staves ", 7) == 0)
				get_staves(p + 7, s);
			return 1;
		}
		if (keep_comment) {
			s = abc_new(t, p, 0);
			s->type = ABC_T_NULL;
			s->state = abc_state;
		}
		return 1;		/* skip comments */
	}
	comment = decomment_line(p);

	/* header fields */
	if (p[1] == ':'
	    && *p != '|' && *p != ':') {	/* not '|:' nor '::' */
		parse_header(t, p, comment);
		if (*p == 'H') {

			/* wait for an other 'x:' or any '%%' */
			for (;;) {
				if ((p = get_line()) == 0)
					break;
				if (p[1] == ':'
				    || (p[1] == '%' && *p == '%'))
					goto again;
				if (abc_state == ABC_S_HEAD) {
					s = abc_new(t, p, 0);
					s->type = ABC_T_INFO2;
					s->state = abc_state;
				}
			}
		}

		/* handle BarFly voice definition */
		/* 'V:n <note line ending with a bar>' */
		if (*p != 'V'
		    || abc_state != ABC_S_TUNE
		    || p[strlen(p) - 1] != '|')
			return 1;
		while (!isspace(*p))
			p++;
		while (isspace(*p))
			p++;
	}
	if (abc_state != ABC_S_TUNE) {
		if (keep_comment) {
			s = abc_new(t, p, comment);
			s->type = ABC_T_NULL;
			s->state = abc_state;
		}
		return 1;
	}

	lyric_started = 0;
	lyric_start = lyric_cont = 0;
	while (*p != '\0') {
		switch (char_tb[*p++]) {
		case CHAR_GCHORD: {			/* " */
			unsigned char *q;
			int l;

			q = p;
			while (*p != '"') {
				if (*p == '\0') {
					syntax("EOL reached while parsing guitar chord",
					       q);
					break;
				}
				p++;
			}
			l = p - q;
			if (gchord) {
				int l2;
				char *gch;

				/* many guitar chord: concatenate with '\n' */
				l2 = strlen(gchord);
				gch = alloc_f(l2 + 2 + l + 1);
				strcpy(gch, gchord);
				gch[l2++] = '\\';
				gch[l2++] = 'n';
				strncpy(&gch[l2], q, l);
				gch[l2 + l] = '\0';
				if (free_f)
					free_f(gchord);
				gchord = gch;
			} else {
				gchord = alloc_f(l + 1);
				strncpy(gchord, q, l);
				gchord[l] = '\0';
			}
			if (*p != '\0')
				p++;
		}
			break;
		case CHAR_GRACE:		/* '{' */
			p = parse_grace(p);
			break;
		case CHAR_DECO:
			if (*p == '\0' && p[-1] == '!')
				break;		/* ignore abc2win EOL */
			p = parse_deco(p - 1, &dc);
			break;
		case CHAR_ACC:
		case CHAR_NOTE:
		case CHAR_REST:
			p = parse_note(t, p - 1);
			break;
		case CHAR_BSLASH:		/* '\' */
			if (*p == '\0')
				return 1;
			syntax("'\\' ignored",
				p - 1);
			break;
		case CHAR_OBRA: {		/* '[' */
			char *q;
			char sep_end;

			if (*p == '|') {
				if (p[1] != ']') {
					/* [| thick-thin bar */
					p = parse_bar(t,
						      p + 1,
						      B_THICK_THIN);
				} else {
					/* [|] invisible bar */
					p = parse_bar(t, p + 2, B_INVIS);
				}
				break;
			}
			if (*p == ']') {
				/* [] invisible bar */
				p = parse_bar(t, p + 1, B_INVIS);
				break;
			}
			if (isdigit(*p)) {
				/* [1 or [2 without a preceeding bar */
				p = parse_bar(t, p, B_INVIS);
				break;
			}
			if (p[1] != ':') {
				p = parse_note(t,
					       p - 1);	/* chord */
				break;
			}

			/* embedded header */
			sep_end = ']';
			q = p;
			while (*p != '\0' && *p != sep_end)
				p++;
			if (*p == '\0') {
				syntax("Escape sequence [..] not closed",
				       q);
				sep_end = '\0';
			} else	*p = '\0';
			abc_state = ABC_S_EMBED;
			parse_header(t, q, 0);
			abc_state = ABC_S_TUNE;
			*p++ = sep_end;
			break;
		}
		case CHAR_BAR: {		/* '|' */
			enum bar_type bar_type = B_SINGLE;

			switch (*p) {
			case '|':
				bar_type = B_DOUBLE;
				p++;
				break;
			case ':':
				bar_type = B_LREP;
				do {		/* ignore |:: */
					p++;
				} while (*p == ':');
				break;
			case ']':		/* code |] for fat end bar */
				bar_type = B_THIN_THICK;
				p++;
				break;
			}
			p = parse_bar(t, p, bar_type);
			break;
		}
		case CHAR_COL: {		/* ':' */
			enum bar_type bar_type = B_RREP;

			if (*p == ':') {
				do {		/* ignore ::| */
					p++;
				} while (*p == ':');
				if (*p == '|')
					p++;
				else	bar_type = B_DREP;
			} else if (*p == '|') {
				p++;
			} else	bar_type = B_DASH;
			p = parse_bar(t, p, bar_type);
			break;
		}
		case CHAR_OPAR:			/* '(' */
			if (isdigit(*p)) {
				curvoice->pplet = *p - '0';
				curvoice->qplet = qtb[curvoice->pplet];
				curvoice->rplet = curvoice->pplet;
				p++;
				if (*p == ':') {
					p++;
					if (isdigit(*p)) {
						curvoice->qplet = *p - '0';
						p++;
					}
					if (*p == ':') {
						p++;
						if (isdigit(*p)) {
							curvoice->rplet = *p - '0';
							p++;
						}
					}
				}
				if (curvoice->qplet == 0)
					curvoice->qplet = meter % 3 == 0
						? 3
						: 2;
			} else	curvoice->slur++;
			break;
		case CHAR_IGN:			/* ' ', '\t', '*' */
			break;
		default:
			syntax("Bad character",
				p - 1);
			break;
		}
	}

	/* add eoln */
	s = abc_new(t, p, 0);
	s->type = ABC_T_EOLN;
	s->state = abc_state;

	return 1;
}

/* -- parse a note -- */
static unsigned char *parse_note(struct abctune *t,
				 unsigned char *p)
{
	struct abcsym *s;
	unsigned char *q;
	int pit, len, acc, nostem;
	int chord, sl1, sl2;
	int j, m;

	s = abc_new(t, gchord, 0);
	s->type = ABC_T_NOTE;
	s->state = ABC_S_TUNE;
	if (gchord && free_f)
		free_f(gchord);
	gchord = 0;

	if (dc.n > 0) {
		memcpy(&s->u.note.dc, &dc, sizeof s->u.note.dc);
		dc.n = 0;
	}

	if (curvoice->pplet) {		/* start of n-plet */
		s->u.note.p_plet = curvoice->pplet;
		s->u.note.q_plet = curvoice->qplet;
		s->u.note.r_plet = curvoice->rplet;
		curvoice->pplet = 0;
	}

	/* rest */
	switch (*p) {
	case 'Z':	/* experimental */
		s->type = ABC_T_MREST;
		p++;
		len = 1;
		if (isdigit(*p)) {
			len = strtol(p, 0, 10);
			while (isdigit(*p))
				p++;
		}
		s->u.note.lens[0] = len;
		return p;
	case 'y':			/* space (BarFly) */
		s->type = ABC_T_REST;
		s->u.note.invis = 1;
		return p + 1;
	case 'x':			/* invisible rest */
		s->u.note.invis = 1;
		/* fall thru */
	case 'z':
		s->type = ABC_T_REST;
		p = parse_len(p + 1, &len);
		s->u.note.lens[0] = len * ulen / BASE_LEN;
		if (curvoice->carryover != 0) {
			broken_rhythm(&s->u.note, curvoice->carryover);
			curvoice->carryover = 0;
		}
		return parse_extra(p, &s->u.note);
	}

	if (!lyric_started) {
		lyric_started = 1;
		s->u.note.lyric_start = 1;
		lyric_start = s;
	}

	if (gr.n > 0) {
		s->u.note.gr = alloc_f(sizeof (struct grace));
		memcpy(s->u.note.gr, &gr, sizeof (struct grace));
		gr.n = 0;
	}

	chord = 0;
	q = p;
	if (*p == '[') {	/* accept only '[..]' for chord */
		chord = 1;
		p++;
	}

	/* get pitch and length */
	m = 0;
	sl1 = sl2 = 0;
	nostem = 0;
	for (;;) {
		int tmp;

		if (chord && *p == '(') {
			s->u.note.sl1[m] = ++sl1;
			p++;
		}
		p = parse_deco(p, &dc);	/* for extra decorations within chord */
		if (strchr(all_notes, *p) == 0) {
			syntax("Not a note", p);
			p++;
		} else {
			p = parse_basic_note(p,
					     &pit,
					     &len,
					     &acc,
					     &tmp);
			s->u.note.pits[m] = pit;
			s->u.note.lens[m] = len;
			s->u.note.accs[m] = acc;
			nostem |= tmp;

			for (j = 0; j < curvoice->ntinext; j++) {
				if (curvoice->tinext[j] == pit) {
					s->u.note.ti2[m] = 1;
					curvoice->tinext[j] = 0;
					break;
				}
			}

			if (chord) {
				if (*p == '-') {
					s->u.note.ti1[m] = 1;
					p++;
				}
				if (*p == ')') {
					s->u.note.sl2[m] = ++sl2;
					p++;
					if (*p == '-') {
						s->u.note.ti1[m] = 1;
						p++;
					}
				}
			}
			m++;
		}

		if (!chord)
			break;
		if (*p == ']') {
			p++;
			break;
		}
		if (*p == '\0') {
			if (chord)
				syntax("Chord not closed", q);
			return p;
		}
	}
	s->u.note.stemless = nostem;

	/* warn about the bad ties */
	for (j = 0; j < curvoice->ntinext; j++) {
		if (curvoice->tinext[j] != 0)
			syntax("Bad tie", p);
	}

	if (m == 0) {			/* if no note */
		if ((t->last_sym = s->prev) == 0)
			t->first_sym = 0;
		else	s->prev->next = 0;
		return p;
	}
	s->u.note.nhd = m - 1;
	note_sort(s);			/* sort the notes in chord */
	if (curvoice->carryover != 0) {	/* add carryover from previous > or < */
		broken_rhythm(&s->u.note, curvoice->carryover);
		curvoice->carryover = 0;
	}
	p = parse_extra(p, &s->u.note);
	s->u.note.slur_st += curvoice->slur;
	curvoice->slur = 0;

	/* mark the ties (found in extra) */
	curvoice->ntinext = 0;
	for (j = 0; j <= s->u.note.nhd; j++) {
		if (s->u.note.ti1[j]) {
			curvoice->tinext[curvoice->ntinext] = s->u.note.pits[j];
			curvoice->ntinext++;
		}
	}
	return p;
}

/* -- process a header -- */
static void parse_header(struct abctune *t,
			 unsigned char *p,
			 char *comment)
{
	struct abcsym *s;
	unsigned char header_type = *p;
	char *error_txt = 0;

	s = abc_new(t, p, comment);
	s->type = ABC_T_INFO;
	s->state = abc_state;

	p += 2;
	while (isspace(*p))
		p++;
	switch (header_type) {
	case 'd':
		if (lyric_start == 0) {
			error_txt = "Erroneous 'd:'";
			break;
		}
		error_txt = parse_decoline(p);
		break;
	case 'K':
		if (abc_state == ABC_S_GLOBAL)
			break;
		parse_key(p, s);
		if (abc_state == ABC_S_HEAD) {
			if (ulen == 0)
				ulen = BASE_LEN / 8;
			abc_state = ABC_S_TUNE;
		}
		break;
	case 'L':
		error_txt = get_len(p, s);
		ulen = s->u.length.base_length;
		if (abc_state == ABC_S_GLOBAL)
			gulen = ulen;
		break;
	case 'M':
		error_txt = parse_meter(p, s);
		break;
	case 'Q':
		error_txt = get_tempo(p, s);
		break;
	case 'U':
		error_txt = get_user(p, s);
		break;
	case 'V':
		if (abc_state == ABC_S_GLOBAL)
			break;
		error_txt = get_voice(p, s);
		break;
	case 'T':
		if (abc_state != ABC_S_GLOBAL)
			break;
		/* 'T:' may start a new tune without 'X:' */
		printf("++ Line %d: T: without X:\n", linenum);
		/* fall thru */
	case 'X':
		if (abc_state != ABC_S_GLOBAL) {
			error_txt = "Previous tune not closed properly";
			/*??maybe call end_tune if ABC_S_TUNE??*/
		}
		memset(voice_tb, 0, sizeof voice_tb);
		nvoice = 0;
		curvoice = &voice_tb[0];
		abc_state = ABC_S_HEAD;
		if (level_f)
			level_f(1);
		break;
	}
	if (error_txt != 0)
		syntax(error_txt, p);
}

/* -- sytax: print message for syntax errror -- */
static void syntax(char *msg,
		   char *q)
{
	int i, n, len, m1, m2, pp;
	int maxcol = 73;

	severity = 1;
	n = q - scratch_line;
	if (n < 0 || n > line_length) {
		if (q == 0)
			printf("\n%s\n", msg);
		else	printf("\n%s at '%s'\n", msg, q);
		return;
	}
	printf("\n++++ %s in line %d.%d\n", msg, linenum, n + 1);
	m1 = 0;
	m2 = len = line_length - 1;
	if (m2 > maxcol) {
		if (n < maxcol)
			m2 = maxcol;
		else {
			m1 = n - 10;
			m2 = m1 + maxcol;
			if (m2 > len)
				m2 = len;
		}
	}

	printf("%4d ", linenum);
	pp = 5;
	if (m1 > 0) {
		printf("...");
		pp += 3;
	}
	for (i = m1; i <= m2; i++)
		printf("%c", scratch_line[i]);
	if (m2 < len)
		printf("...");
	printf("\n");

	if (n >= 0 && n < 200) {
		for (i = 0; i < n + pp - m1; i++)
			printf(" ");
		printf("^\n");
	}
}
