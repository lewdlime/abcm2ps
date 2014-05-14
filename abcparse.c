/*
 * Generic ABC parser.
 *
 * Copyright (C) 1998-2014 Jean-François Moine
 * Adapted from abc2ps, Copyright (C) 1996, 1997 Michael Methfessel
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
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335  USA
 */

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

/* global values */
char *deco_tb[128];		/* decoration names */
int severity;			/* error severity */

static int abc_vers;		/* abc version */
static short abc_state;		/* parse state */
static short ulen;		/* unit note length set by M: or L: */
static short meter;		/* upper value of time sig for n-plets */
static unsigned char microscale; /* current microtone scale */
static signed char vover;	/* voice overlay (1: single bar, -1: multi-bar */
static char lyric_started;	/* lyric started */
static char *gchord;		/* guitar chord */
static struct deco dc;		/* decorations */
static struct abcsym *deco_start; /* 1st note of the line for d: / s: */
static struct abcsym *deco_cont; /* current symbol when d: / s: continuation */
static unsigned short *p_micro;	/* ptr to the microtone table of the tune */

#define VOICE_NAME_SZ 64	/* max size of a voice name */

static char *file;		/* remaining abc file */
static char *abc_fn;		/* current source file name */
static int linenum;		/* current source line number */
static int colnum;		/* current source column number */
static char *abc_line;		/* line being parsed */
static struct abcsym *last_sym;	/* last symbol for errors */

static short nvoice;		/* number of voices (0..n-1) */
static struct {			/* voice table and current pointer */
	char id[VOICE_ID_SZ];		/* voice ID */
	struct abcsym *last_note;	/* last note or rest */
	short ulen;			/* unit note length */
	unsigned char microscale;	/* microtone scale */
	unsigned char mvoice;		/* main voice when voice overlay */
} voice_tb[MAXVOICE], *curvoice;

/* char table for note line parsing */
#define CHAR_BAD 0
#define CHAR_IGN 1
#define CHAR_NOTE 2
#define CHAR_GR_ST 3
#define CHAR_DECO 4
#define CHAR_GCHORD 5
#define CHAR_BSLASH 6
#define CHAR_OBRA 7
#define CHAR_BAR 8
#define CHAR_OPAR 9
#define CHAR_VOV 10
#define CHAR_SPAC 11
#define CHAR_MINUS 12
#define CHAR_CPAR 13
#define CHAR_BRHY 14
#define CHAR_DECOS 15
#define CHAR_SLASH 16
#define CHAR_GR_EN 17
#define CHAR_LINEBREAK 18
static char char_tb[256] = {
	0, 0, 0, 0, 0, 0, 0, 0,				/* 00 - 07 */
	0, CHAR_SPAC, CHAR_LINEBREAK, 0, 0, 0, 0, 0,	/* 08 - 0f */
	0, 0, 0, 0, 0, 0, 0, 0,				/* 10 - 17 */
	0, 0, 0, 0, 0, 0, 0, 0,				/* 18 - 1f */
	CHAR_SPAC, CHAR_DECOS, CHAR_GCHORD, CHAR_BAD,	/* (sp) ! " # */
	CHAR_BAD, CHAR_BAD, CHAR_VOV, CHAR_BAD, 	/* $ % & ' */
	CHAR_OPAR, CHAR_CPAR, CHAR_BAD, CHAR_DECOS, 	/* ( ) * + */
	CHAR_BAD, CHAR_MINUS, CHAR_DECO, CHAR_SLASH, 	/* , - . / */
	CHAR_BAD, CHAR_BAD, CHAR_BAD, CHAR_BAD, 	/* 0 1 2 3 */
	CHAR_BAD, CHAR_BAD, CHAR_BAD, CHAR_BAD, 	/* 4 5 6 7 */
	CHAR_BAD, CHAR_BAD, CHAR_BAR, CHAR_BAD, 	/* 8 9 : ; */
	CHAR_BRHY, CHAR_NOTE, CHAR_BRHY, CHAR_BAD, 	/* < = > ? */
	CHAR_BAD, CHAR_NOTE, CHAR_NOTE, CHAR_NOTE, 	/* @ A B C */
	CHAR_NOTE, CHAR_NOTE, CHAR_NOTE, CHAR_NOTE, 	/* D E F G */
	CHAR_DECO, CHAR_DECO, CHAR_DECO, CHAR_DECO, 	/* H I J K */
	CHAR_DECO, CHAR_DECO, CHAR_DECO, CHAR_DECO, 	/* L M N O */
	CHAR_DECO, CHAR_DECO, CHAR_DECO, CHAR_DECO, 	/* P Q R S */
	CHAR_DECO, CHAR_DECO, CHAR_DECO, CHAR_DECO, 	/* T U V W */
	CHAR_NOTE, CHAR_DECO, CHAR_NOTE, CHAR_OBRA, 	/* X Y Z [ */
	CHAR_BSLASH, CHAR_BAR, CHAR_NOTE, CHAR_NOTE, 	/* \ ] ^ _ */
	CHAR_IGN, CHAR_NOTE, CHAR_NOTE, CHAR_NOTE, 	/* ` a b c */
	CHAR_NOTE, CHAR_NOTE, CHAR_NOTE, CHAR_NOTE, 	/* d e f g */
	CHAR_DECO, CHAR_DECO, CHAR_DECO, CHAR_DECO, 	/* h i j k */
	CHAR_DECO, CHAR_DECO, CHAR_DECO, CHAR_DECO, 	/* l m n o */
	CHAR_DECO, CHAR_DECO, CHAR_DECO, CHAR_DECO, 	/* p q r s */
	CHAR_DECO, CHAR_DECO, CHAR_DECO, CHAR_DECO, 	/* t u v w */
	CHAR_NOTE, CHAR_NOTE, CHAR_NOTE, CHAR_GR_ST, 	/* x y z { */
	CHAR_BAR, CHAR_GR_EN, CHAR_DECO, CHAR_BAD, 	/* | } ~ (del) */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* 80 - 8f */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* 90 - 9f */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* a0 - af */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* b0 - bf */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* c0 - cf */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* d0 - df */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* e0 - ef */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* f0 - ff */
};

static const char all_notes[] = "CDEFGABcdefgab";

static char *get_line(void);
static char *parse_len(char *p,
			int *p_len);
static char *parse_basic_note(char *p,
			      int *pitch,
			      int *length,
			      int *accidental,
			      int *stemless);
static int parse_info(struct abctune *t,
		       char *p,
		       char *comment);
static char *parse_gchord(char *p);
static int parse_line(struct abctune *t,
		      char *p);
static char *parse_note(struct abctune *t,
			char *p,
			int flags);
static void syntax(char *msg, char *q);
static void vover_new(void);

/* -- abcMIDI like errors -- */
static void print_error(char *s, int col)
{
	if (col >= 0)
		fprintf(stderr, "%s:%d.%d: error: %s\n", abc_fn, linenum, col, s);
	else
		fprintf(stderr, "%s:%d: error: %s\n", abc_fn, linenum, s);
}

/* -- delete an ABC symbol -- */
void abc_delete(struct abcsym *as)
{
	switch (as->type) {
	case ABC_T_INFO:
		switch (as->text[0]) {
		case 'Q':
			if (as->u.tempo.str1)
				free_f(as->u.tempo.str1);
			if (as->u.tempo.value)
				free_f(as->u.tempo.value);
			if (as->u.tempo.str2)
				free_f(as->u.tempo.str2);
			break;
		case 'V':
			if (as->u.voice.fname)
				free_f(as->u.voice.fname);
			if (as->u.voice.nname)
				free_f(as->u.voice.nname);
			break;
		}
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
		as->tune->last_sym = as->prev;
	free_f(as);
}

/* -- free the memory areas of all tunes -- */
void abc_free(struct abctune *t)
{
	struct abctune *tn;
	struct abcsym *s, *sn;

	if (!free_f)
		return;
	for (;;) {
		if (!t)
			break;
		s = t->first_sym;

		/* free the associated symbols */
		for (;;) {
			sn = s->next;
			abc_delete(s);
			if ((s = sn) == NULL)
				break;
		}

		/* free the tune */
		tn = t->next;
		free_f(t);
		t = tn;
	}
}

/* -- initialize the parser -- */
void abc_init(void *alloc_f_api(int size),
	      void free_f_api(void *ptr),
	      void level_f_api(int level),
	      int client_sz_api,
	      int keep_comment_api)
{
	if (alloc_f) {
		fprintf(stderr, "abc_init already initialized\n");
		return;
	}
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
	struct abctune *t;
	char *p;

	/* initialize */
	file = file_api;
	if (level_f)
		level_f(abc_state != ABC_S_GLOBAL);
	abc_state = ABC_S_TUNE;
	linenum = 0;
	abc_fn = "internal";
	t = s->tune;
	t->last_sym = s;

	/* scan till end of description */
	for (;;) {
		if ((p = get_line()) == NULL)
			break;			/* done */
		if (*p == '\0')
			break;			/* blank line --> done */
/*fixme-insert: don't accept X:*/
		/* parse the music line */
		if (parse_line(t, p))
			break;
	}
}

/* -- new symbol -- */
struct abcsym *abc_new(struct abctune *t,
		       char *text,
		       char *comment)
{
	struct abcsym *s;

	s = alloc_f(sizeof *s + client_sz);
	memset(s, 0, sizeof *s + client_sz);
	s->tune = t;
	if (text) {
		s->text = alloc_f(strlen(text) + 1);
		strcpy(s->text, text);
	}
	if (comment) {
		s->comment = alloc_f(strlen(comment) + 1);
		strcpy(s->comment, comment);
	}
	if (!t->last_sym) {
		t->first_sym = s;
	} else {
		if ((s->next = t->last_sym->next) != NULL)
			s->next->prev = s;
		t->last_sym->next = s;
		s->prev = t->last_sym;
	}
	last_sym = t->last_sym = s;
	s->state = abc_state;
	s->fn = abc_fn;
	s->linenum = linenum;
	s->colnum = colnum;
	return s;
}

/* get the ABC version */
static void get_vers(char *p)
{
	int i, j,k;

	i = j = k = 0;
	if (sscanf(p, "%d.%d.%d", &i, &j, &k) != 3)
		if (sscanf(p, "%d.%d", &i, &j) != 2)
			sscanf(p, "%d", &i);
	abc_vers = (i << 16) + (j << 8) + k;
}

/* -- parse an ABC file -- */
struct abctune *abc_parse(char *file_api)
{
	struct abctune *first_tune = NULL;
	struct abctune *t, *last_tune;
	/* saved global variables */
	int g_abc_vers, g_ulen, g_microscale;
	char *p;
	char g_char_tb[128];

	/* initialize */
	file = file_api;
	t = NULL;
	abc_state = ABC_S_GLOBAL;
	if (level_f)
		level_f(0);
	abc_fn = "internal";
	linenum = 0;
	last_tune = NULL;
	g_abc_vers = g_ulen = g_microscale = 0;	/* (have gcc happy) */

	/* scan till end of file */
	for (;;) {
		if ((p = get_line()) == NULL) {
			if (abc_state == ABC_S_HEAD) {
				syntax("Unexpected EOF in header definition",
					p);
				severity = 1;
			}
			if (t)
				t->abc_vers = abc_vers;
			if (abc_state != ABC_S_GLOBAL) {
				abc_vers = g_abc_vers;
				ulen = g_ulen;
				microscale = g_microscale;
				memcpy(char_tb, g_char_tb, sizeof g_char_tb);
			}
			break;			/* done */
		}

		/* start a new tune if not done */
		if (!t) {
			if (*p == '\0')
				continue;
			t = alloc_f(sizeof *t);
			memset(t, 0 , sizeof *t);
			if (!last_tune)
				first_tune = t;
			else
				last_tune->next = t;
			last_tune = t;
			p_micro = t->micro_tb;
			meter = 0;
		}

		/* parse the music line */
		switch (parse_line(t, p)) {
		case 2:				/* start of tune */
			g_abc_vers = abc_vers;
			g_ulen = ulen;
			g_microscale = microscale;
			memcpy(g_char_tb, char_tb, sizeof g_char_tb);
			break;
		case 1:				/* end of tune */
			t->abc_vers = abc_vers;
			abc_state = ABC_S_GLOBAL;
			t = NULL;
			abc_vers = g_abc_vers;
			ulen = g_ulen;
			microscale = g_microscale;
			memcpy(char_tb, g_char_tb, sizeof g_char_tb);
			if (level_f)
				level_f(0);
			if (dc.n > 0)
				syntax("Decoration without symbol", 0);
			dc.n = dc.h = dc.s = 0;
			break;
		}
	}
	return first_tune;
}

/* -- cut off after % and remove trailing blanks -- */
static char *decomment_line(char *p)
{
	char *q, c, *comment = NULL;

	q = p;
	for (;;) {
		c = *p;
		if (c == '\0')
			break;
		if (c == '\\') {
			p++;
			if (*p == '\0')
				return 0;
			p++;
			continue;
		}
		if (c == '%') {
			if (keep_comment) {
				comment = p + 1;
				break;
			}
			break;
		}
		if (c == '"') {
			for (;;) {
				p++;
				if (*p == '\0')
					break;
				if (*p == '"' && p[-1] != '\\') {
					p++;
					break;
				}
			}
		} else {
			p++;
		}
	}

	/* remove the trailing blanks */
	while (p > q) {
		c = *--p;
		if (!isspace((unsigned char) c)) {
			p[1] = '\0';
			break;
		}
	}
	return comment;
}

/* -- treat the broken rhythm '>' and '<' -- */
static void broken_rhythm(struct note *note,
			  int num)	/* >0: do dot, <0: do half */
{
	int l, m, n;

	num *= 2;
	if (num > 0) {
		if (num == 6)
			num = 8;
		n = num * 2 - 1;
		for (m = 0; m <= note->nhd; m++)
			note->lens[m] = (note->lens[m] * n) / num;
	} else {
		n = -num;
		if (n == 6)
			n = 8;
		for (m = 0; m <= note->nhd; m++)
			note->lens[m] /= n;
	}
	l = note->lens[0];
	for (m = 1; m <= note->nhd; m++)
		if (note->lens[m] < l)
			l = note->lens[m];
}

/* -- check for the '!' as end of line (ABC2Win) -- */
static int check_nl(char *p)
{
	while (*p != '\0') {
		switch (*p++) {
		case '!':
			return 0;
		case '|':
		case '[':
		case ':':
		case ']':
		case ' ':
		case '\t':
			return 1;
		}
	}
	return 1;
}

/* -- parse extra K: or V: definitions (clef, octave and microscale  -- */
static char *parse_extra(char *p,
			char **p_name,
			char **p_middle,
			char **p_lines,
			char **p_scale,
			char **p_octave)
{
	for (;;) {
		if (strncmp(p, "clef=", 5) == 0
		 || strncmp(p, "bass", 4) == 0
		 || strncmp(p, "treble", 6) == 0
		 || strncmp(p, "alto", 4) == 0
		 || strncmp(p, "tenor", 5) == 0
		 || strncmp(p, "perc", 4) == 0) {
			if (*p_name)
				syntax("Double clef name", p);
			*p_name = p;
		} else if (strncmp(p, "microscale=", 11) == 0) {
			int i;

			i = atoi(p + 11);
			if (i < 4 || i > 256)
				syntax("Invalid value in microscale=", p);
			else
				microscale = i;
		} else if (strncmp(p, "middle=", 7) == 0
			|| strncmp(p, "m=", 2) == 0) {
			if (*p_middle)
				syntax("Double clef middle", p);
			*p_middle = p + (p[1] == '=' ? 2 : 7);
		} else if (strncmp(p, "octave=", 7) == 0) {
			if (*p_octave)
				syntax("Double octave=", p);
			*p_octave = p + 7;
		} else if (strncmp(p, "stafflines=", 11) == 0) {
			if (*p_lines)
				syntax("Double stafflines", p);
			*p_lines = p + 11;
		} else if (strncmp(p, "staffscale=", 11) == 0) {
			if (*p_scale)
				syntax("Double staffscale", p);
			*p_scale = p + 11;
		} else if (strncmp(p, "transpose=", 10) == 0
			|| strncmp(p, "t=", 2) == 0) {
			;		/* ignored */
		} else {
			break;
		}
		while (!isspace((unsigned char) *p) && *p != '\0')
			p++;
		while (isspace((unsigned char) *p))
			p++;
		if (*p == '\0')
			break;
	}
	return p;
}

/* -- parse a decoration 'xxx<decosep>' -- */
static char *get_deco(char *p,
		      unsigned char *p_deco)
{
	char *q, sep, **t;
	unsigned i, l;

	*p_deco = 0;
	q = p;
	sep = q[-1];
	if (char_tb[(unsigned char) sep] == CHAR_DECOS) {
		if (sep == '+') {
			if (*p == '+' && p[1] == '+')
				p++;		/* special case "+++" */
		}
	} else {
		sep = '\0';			/* Barfly U: */
	}
	while (*p != sep) {
		if (*p == '\0') {
			syntax("Decoration not terminated", q);
			return p;
		}
		p++;
	}
	l = p - q;
	if (*p == sep)
		p++;
	for (i = 1, t = &deco_tb[1];
	     *t && i < 128;
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
	} else {
		syntax("Too many decoration types", q);
	}
	return p;
}

/* -- parse a list of accidentals (K:) -- */
static char *parse_acc(char *p,
			struct abcsym *s)
{
	int pit, len, acc, nostem;
	unsigned nacc;

	if (s->u.key.empty == 2)
		syntax("cannot have 'none' and a list of accidentals", p);
	nacc = 0;
	for (;;) {
		if (nacc >= sizeof s->u.key.pits) {
			syntax("Too many accidentals", p);
			break;
		}
		p = parse_basic_note(p, &pit, &len, &acc, &nostem);
		s->u.key.pits[nacc] = pit;
		s->u.key.accs[nacc++] = acc;
		while (isspace((unsigned char) *p))
			p++;
		if (*p == '\0')
			break;
		if (*p != '^' && *p != '_' && *p != '=')
			break;
	}
	s->u.key.microscale = microscale;
	if (s->u.key.empty != 2)
		s->u.key.nacc = nacc;
	return p;
}

/* -- parse a clef (K: or V:) -- */
static void parse_clef(struct abcsym *s,
			char *name,
			char *middle,
			char *lines,
			char *scale)
{
	int clef = -1;
	int transpose = 0;
	int clef_line = 2;
	char *warn = NULL;
	char str[80];

	str[0] = '\0';
	if (name && strncmp(name, "clef=", 5) == 0) {
		name += 5;
		switch (*name) {
		case '\"':
			name = get_str(str, name, sizeof str);
			s->u.clef.name = alloc_f(strlen(str) + 1);
			strcpy(s->u.clef.name, str);
			clef = TREBLE;
			break;
		case 'g':
			warn = name;
			transpose = -7;
		case 'G':
			clef = TREBLE;
			break;
		case 'f':
			warn = name;
			transpose = -14;
			clef = BASS;
			clef_line = 4;
			break;
		case 'F':
			if (name[1] == ',')	/* abc2.1.1 clef=F == clef=F, */
				transpose = -7;
			clef = BASS;
			clef_line = 4;
			break;
		case 'c':
			warn = name;
			transpose = -7;
		case 'C':
			clef = ALTO;
			clef_line = 3;
			break;
		case 'P':
			clef = PERC;
			break;
		}
		if (clef >= 0) {
			name++;
			if (*name == ',' || *name== '\'')
				warn = name;
			while (*name == ',') {
				transpose += 7;
				name++;
			}
			while (*name == '\'') {
				transpose -= 7;
				name++;
			}
		}
	}
	if (name && clef < 0) {
		if (!strncmp(name, "bass", 4)) {
			clef = BASS;
			clef_line = 4;
			s->u.clef.check_pitch = 1;
			name += 4;
		} else if (!strncmp(name, "treble", 6)) {
			clef = TREBLE;
			name += 6;
		} else if (!strncmp(name, "alto", 4)
			|| !strncmp(name, "tenor", 5)) {
			clef = ALTO;
			clef_line = *name == 'a' ? 3 : 4;
			s->u.clef.check_pitch = 1;
			if (*name == 'a')
				name += 4;
			else
				name += 5;
		} else if (!strncmp(name, "perc", 4)) {
			clef = PERC;
			name += 4;
		} else if (strncmp(name, "none", 4) == 0) {
			clef = TREBLE;
			s->u.clef.invis = 1;
			s->flags |= ABC_F_INVIS;
			name += 4;
		} else {
			syntax("Unknown clef", name);
		}
	}

	if (clef >= 0) {
		switch (*name) {
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
			clef_line = *name++ - '0';
			break;
		}
		if (name[1] == '8') {
			switch (*name) {
			case '^':
				transpose -= 7;
			case '+':
				s->u.clef.octave = 1;
				break;
			case '_':
				transpose += 7;
			case '-':
				s->u.clef.octave = -1;
				break;
			}
		}
	}

	if (middle) {
		int pit, len, acc, nostem, l;
		static const char line_tb[7] =
			{ALTO, TREBLE, ALTO, BASS, ALTO, BASS, ALTO};

		warn = middle;
		/* 'middle=<note pitch>' */
		parse_basic_note(middle, &pit, &len, &acc, &nostem);

		if (clef < 0)
			clef = line_tb[(pit + 7) % 7];
	
		switch (clef) {
		default:
			l = 20 + 4;
			break;
		case ALTO:
			l = 16 + 4;
			break;
		case BASS:
			l = 12 + 4;
			break;
		}
		clef_line = (l - pit + 28) % 7;
		if (clef_line & 1) {
			syntax("Bad 'middle' value for the clef", middle);
			pit++;
		}
		clef_line = clef_line / 2 + 1;

		transpose = l - (clef_line - 1) * 2 - pit;
		s->u.clef.check_pitch = 0;
	}

	s->u.clef.type = clef;
	s->u.clef.line = clef_line;
	s->u.clef.transpose = transpose;
	s->u.clef.stafflines = -1;
	s->u.clef.staffscale = 0;
	if (lines) {
		int l;

		l = atoi(lines);
		if ((unsigned) l < 10)
			s->u.clef.stafflines = l;
		else
			syntax("Bad value of stafflines", lines);
	}
	if (scale) {
		float sc;

		sc = atof(scale);
		if (sc >= 0.5 && sc <= 3)
			s->u.clef.staffscale = sc;
		else
			syntax("Bad value of staffscale", scale);
	}
	if (warn) {
		int sev_sav;

		sev_sav = severity;
		syntax("Warning: Deprecated or non-standard item", warn);
		severity = sev_sav;
	}
}

/* get the octave= value */
static int parse_octave(char *p)
{
	int oct;

	if (p) {
		oct = 1;
		if (*p == '-') {
			oct = -1;
			p++;
		}
		if (*p >= '0' && *p <= '4')
			return oct * (*p - '0');
		syntax("Bad octave value", p);
	}
	return NO_OCTAVE;
}

/* -- parse a 'K:' -- */
static void parse_key(char *p,
		      struct abcsym *s)
{
	int sf, mode;
	char *clef_name, *clef_middle, *clef_lines, *clef_scale;
	char *p_octave;

	if (*p == '\0') {
		s->u.key.empty = 2;
		return;
	}
	if (strncasecmp(p, "none", 4) == 0) {
		s->u.key.empty = 2;
		p += 4;
		while (isspace((unsigned char) *p))
			p++;
		if (*p == '\0')
			return;
	}
	clef_name = clef_middle = clef_lines = clef_scale = NULL;
	p_octave = NULL;
	p = parse_extra(p, &clef_name, &clef_middle, &clef_lines,
			&clef_scale, &p_octave);
	sf = 0;
	mode = MAJOR;
	switch (*p++) {
	case 'F': sf = -1; break;
	case 'B': sf++;
	case 'E': sf++;
	case 'A': sf++;
	case 'D': sf++;
	case 'G': sf++;
	case 'C': break;
	case 'H':
		if (*p == 'P') {
			mode = BAGPIPE;
			p++;
		} else if (*p == 'p') {
			mode = BAGPIPE + 1;
			sf = 2;
			p++;
		} else {
			syntax("Unknown bagpipe-like key", p);
		}
		break;
	case '^':
	case '_':
	case '=':
		p--;			/* explicit accidentals */
		break;
	case '\0':
		if (s->u.key.empty == 0)
			s->u.key.empty = 1;
		p--;
		break;
	default:
		p--;
		if (s->u.key.empty != 2)
			syntax("Key not recognized", p);
		break;
	}
	if (*p == '#') {
		sf += 7;
		p++;
	} else if (*p == 'b') {
		sf -= 7;
		p++;
	}

	while (*p != '\0') {
		while (isspace((unsigned char) *p))
			p++;
		if (*p == '\0')
			break;
		p = parse_extra(p, &clef_name, &clef_middle, &clef_lines,
				&clef_scale, &p_octave);
		if (*p == '\0')
			break;
		switch (*p) {
		case 'a':
		case 'A':
			if (strncasecmp(p, "aeo", 3) == 0) {
				sf -= 3;
				mode = 5;
				break;
			}
			goto unk;
		case 'd':
		case 'D':
			if (strncasecmp(p, "dor", 3) == 0) {
				sf -= 2;
				mode = 1;
				break;
			}
			goto unk;
		case 'e':
		case 'E':
			if (strncasecmp(p, "exp", 3) == 0) {
				s->u.key.exp = 1;
				break;
			}
			goto unk;
		case 'i':
		case 'I':
			if (strncasecmp(p, "ion", 3) == 0) {
				mode = 0;
				break;
			}
			goto unk;
		case 'l':
		case 'L':
			if (strncasecmp(p, "loc", 3) == 0) {
				sf -= 5;
				mode = 6;
				break;
			}
			if (strncasecmp(p, "lyd", 3) == 0) {
				sf += 1;
				mode = 3;
				break;
			}
			goto unk;
		case 'm':
		case 'M':
			if (strncasecmp(p, "maj", 3) == 0)
				break;
			if (strncasecmp(p, "mix", 3) == 0) {
				sf -= 1;
				mode = 4;
				break;
			}
			if (strncasecmp(p, "min", 3) == 0
			 || !isalpha((unsigned char) p[1])) { /* 'm' alone */
				sf -= 3;
				mode = MINOR;
				break;
			}
			goto unk;
		case 'p':
		case 'P':
			if (strncasecmp(p, "phr", 3) == 0) {
				sf -= 4;
				mode = 2;
				break;
			}
			goto unk;
		case '^':
		case '_':
		case '=':
			p = parse_acc(p, s);	/* explicit accidentals */
			continue;
		default:
		unk:
			syntax("Unknown token in key specifier", p);
			while (!isspace((unsigned char) *p) && *p != '\0')
				p++;
			continue;
		}
		while (isalpha((unsigned char) *p))
			p++;
	}

	if (sf > 7 || sf < -7) {
		syntax("Too many sharps/flats", p);
		if (sf > 0)
			sf -= 12;
		else
			sf += 12;
	}
	s->u.key.sf = sf;
	s->u.key.mode = mode;
	s->u.key.octave = parse_octave(p_octave);

	if (clef_name || clef_middle || clef_lines || clef_scale) {
		s = abc_new(s->tune, NULL, NULL);
		s->type = ABC_T_CLEF;
		parse_clef(s, clef_name, clef_middle, clef_lines,
				clef_scale);
	}
}

/* -- set default length from 'L:' -- */
static char *get_len(char *p,
		     struct abcsym *s)
{
	int l1, l2, d;
	char *error_txt = NULL;

	if (strcmp(p, "auto") == 0) {		/* L:auto */
		s->u.length.base_length = -1;
		return error_txt;
	}
	l1 = 0;
	l2 = 1;
	if (sscanf(p, "%d /%d ", &l1, &l2) != 2
	 || l1 == 0) {
		s->u.length.base_length = ulen ? ulen : BASE_LEN / 8;
		return "Bad unit note length: unchanged";
	}

	if (l2 == 0) {
		error_txt = "Bad length divisor, set to 4";
		l2 = 4;
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
static char *get_line(void)
{
	int l;
	char *p, *q;

	p = file;
	if (*p == '\0')
		return 0;
	abc_line = p;
	linenum++;

	/* handle %%begin .. %%end */
	if (strncmp(p, "%%begin", 7) == 0) {
		for (;;) {
			while (*p != '\0'
			    && *p != '\n')
				p++;
			if (*p == '\0') {
				syntax("No %%end after %%begin", 0);
				break;
			}
			linenum++;
			p++;
			if (strncmp(p, "%%end", 5) == 0) {
				p[-1] = '\0';
				while (*p != '\0'
				    && *p != '\n')
					p++;
				if (*p != '\0')
					*p++ = '\0';
				break;
			}
		}
		file = p;
		return abc_line;
	}

	/* set the end of line and
	 * memorize the beginning of the next line */
	while (*p != '\0'
	    && *p != '\n')
		p++;
	if (*p != '\0')
		*p++ = '\0';
	file = p;

	/* special case for continuation lines in ABC version 2.0 */
	if (abc_vers == (2 << 16)) {
		p = abc_line;
		for (;;) {
			while (*p != '\0') {
				if (*p == '"') {
					for (;;) {
						p++;
						if (*p == '\0')
							break;
						if (*p == '"' && p[-1] != '\\') {
							p++;
							break;
						}
					}
					continue;
				}
				if (*p == '\\'
				 && p[1] != '\\'
				 && p[1] != '%') {
					q = p + 1;
					while (isspace((unsigned char) *q))
						q++;
					if (*q == '\0' || *q == '%')
						break;
					p = q;
					continue;
				}
				p++;
			}
			if (*p == '\0')
				break;

			/* continuation found */
			q = file;
			if (*q == '\0')
				break;
			linenum++;
			while (*q != '\0'
			    && *q != '\n')
				q++;
			l = q - file;
			memmove(p, file, l);	/* concatenate */
			p[l] = '\0';
			if (*q == '\0')
				break;
			file = q + 1;
		}
	}
	return abc_line;
}

/* -- parse a 'M:' -- */
static char *parse_meter(char *p,
			 struct abcsym *s)
{
	int m1, m2, d, wmeasure, nm, in_parenth;
	unsigned i;
	char *q;
static char *top_err = "Cannot identify meter top";

	if (*p == '\0')
		return "Empty meter string";
	nm = 0;
	in_parenth = 0;
	wmeasure = 0;
	m1 = 0;
	if (strncmp(p, "none", 4) == 0) {
		p += 4;				/* no meter */
	} else while (*p != '\0') {
		if (*p == '=')
			break;
		if (nm >= MAX_MEASURE)
			return "Too many values in M:";
		switch (*p) {
		case 'C':
			s->u.meter.meter[nm].top[0] = *p++;
			if (*p == '|')
				s->u.meter.meter[nm].top[1] = *p++;
			m1 = 4;
			m2 = 4;
			break;
		case 'c':
		case 'o':
			if (*p == 'c')
				m1 = 4;
			else
				m1 = 3;
			m2 = 4;
			s->u.meter.meter[nm].top[0] = *p++;
			if (*p == '.')
				s->u.meter.meter[nm].top[1] = *p++;
			break;
		case '(':
			if (p[1] == '(') {	/* "M:5/4 ((2+3)/4)" */
				in_parenth = 1;
				s->u.meter.meter[nm++].top[0] = *p++;
			}
			q = p + 1;
			while (*q != '\0') {
				if (*q == ')' || *q == '/')
					break;
				q++;
			}
			if (*q == ')' && q[1] == '/') {	/* "M:5/4 (2+3)/4" */
				p++;		/* remove the parenthesis */
				continue;
			}			/* "M:5 (2+3)" */
			/* fall thru */
		case ')':
			in_parenth = *p == '(';
			s->u.meter.meter[nm++].top[0] = *p++;
			continue;
		default:
			if (sscanf(p, "%d", &m1) != 1
			 || m1 <= 0)
				return top_err;
			i = 0;
			m2 = 2;			/* default when no bottom value */
			for (;;) {
				while (isdigit((unsigned char) *p)
				    && i < sizeof s->u.meter.meter[0].top)
					s->u.meter.meter[nm].top[i++] = *p++;
				if (*p == ')') {
					if (p[1] != '/')
						break;
					p++;
				}
				if (*p == '/') {
					p++;
					if (sscanf(p, "%d", &m2) != 1
					 || m2 <= 0)
						return "Cannot identify meter bottom";
					i = 0;
					while (isdigit((unsigned char) *p)
					    && i < sizeof s->u.meter.meter[0].bot)
						s->u.meter.meter[nm].bot[i++] = *p++;
					break;
				}
				if (*p != ' ' && *p != '+')
					break;
				if (*p == '\0' || p[1] == '(')	/* "M:5 (2/4+3/4)" */
					break;
				if (i < sizeof s->u.meter.meter[0].top)
					s->u.meter.meter[nm].top[i++] = *p++;
				if (sscanf(p, "%d", &d) != 1
				 || d <= 0)
					return top_err;
				if (p[-1] == ' ') {
					if (d > m1)
						m1 = d;
				} else {
					m1 += d;
				}
			}
			break;
		}
		if (!in_parenth)
			wmeasure += m1 * BASE_LEN / m2;
		nm++;
		if (*p == ' ')
			p++;
		else if (*p == '+')
			s->u.meter.meter[nm++].top[0] = *p++;
	}
	meter = m1;
	if (*p == '=') {
		if (sscanf(++p, "%d/%d", &m1, &m2) != 2
		 || m1 <= 0
		 || m2 <= 0)
			return "Cannot identify meter explicit duration";
		wmeasure = m1 * BASE_LEN / m2;
		s->u.meter.expdur = 1;
	}
	s->u.meter.wmeasure = wmeasure;
	s->u.meter.nmeter = nm;

	/* if in the header, change the unit note length */
	if (abc_state == ABC_S_HEAD && ulen == 0) {
		if (wmeasure >= BASE_LEN * 3 / 4
		 || wmeasure == 0)
			ulen = BASE_LEN / 8;
		else
			ulen = BASE_LEN / 16;
	}
	return 0;
}

/* -- get a possibly quoted string -- */
char *get_str(char *d,		/* destination */
	      char *s,		/* source */
	      int maxlen)	/* max length */
{
	char c;

	maxlen--;		/* have place for the EOS */
	while (isspace((unsigned char) *s))
		s++;
	if (*s == '"') {
		s++;
		while ((c = *s) != '\0') {
			if (c == '"') {
				s++;
				break;
			}
			if (c == '\\') {
				if (--maxlen > 0)
					*d++ = c;
				c = *++s;
			}
			if (--maxlen > 0)
				*d++ = c;
			s++;
		}
	} else {
		while ((c = *s) != '\0') {
			if (isspace((unsigned char) c))
				break;
			if (--maxlen > 0)
				*d++ = c;
			s++;
		}
	}
	*d = '\0';
	while (isspace((unsigned char) *s))
		s++;
	return s;
}

/* -- parse a tempo (Q:) -- */
static char *parse_tempo(char *p,
			 struct abcsym *s)
{
	int l;
	char *q, str[80];

	/* string before */
	if (*p == '"') {
		p = get_str(str, p, sizeof str);
		s->u.tempo.str1 = alloc_f(strlen(str) + 1);
		strcpy(s->u.tempo.str1, str);
	}

	/* beat */
	if (*p == 'C' || *p == 'c'
	 || *p == 'L' || *p == 'l') {
		int len;

		p++;
		if (*p != '=')
			goto inval;
		p = parse_len(p + 1, &len);
		if (len <= 0)
			goto inval;
		s->u.tempo.length[0] = len * ulen / BASE_LEN;
		while (isspace((unsigned char) *p))
			p++;
		if (abc_vers >= (2 << 16))
			syntax("Deprecated Q: value", p);
	} else if (isdigit((unsigned char) *p) && strchr(p, '/') != 0) {
		unsigned i;

		i = 0;
		while (isdigit((unsigned char) *p)) {
			int top, bot, n;

			if (sscanf(p, "%d /%d%n", &top, &bot, &n) != 2
			 || bot <= 0)
				goto inval;
			l = (BASE_LEN * top) / bot;
			if (l <= 0
			 || i >= sizeof s->u.tempo.length
					/ sizeof s->u.tempo.length[0])
				goto inval;
			s->u.tempo.length[i++] = l;
			p += n;
			while (isspace((unsigned char) *p))
				p++;
		}
	}

	/* tempo value ('Q:beat=value' or 'Q:value') */
	if (*p == '=') {
		p++;
		while (isspace((unsigned char) *p))
			p++;
	}
	if (*p != '\0' && *p != '"') {
		q = p;
		while (*p != '"' && *p != '\0')
			p++;
		while (isspace((unsigned char) p[-1]))
			p--;
		l = p - q;
		s->u.tempo.value = alloc_f(l + 1);
		strncpy(s->u.tempo.value, q, l);
		s->u.tempo.value[l] = '\0';
		while (isspace((unsigned char) *p))
			p++;
	}

	/* string after */
	if (*p == '"') {
		p = get_str(str, p, sizeof str);
		s->u.tempo.str2 = alloc_f(strlen(str) + 1);
		strcpy(s->u.tempo.str2, str);
	}

	if (!s->u.tempo.str1 && !s->u.tempo.str2
	 && s->u.tempo.length[0] == 0) {
		if (s->u.tempo.value == 0)
			return "Empty tempo";
		if (abc_vers >= (2 << 16))
			syntax("Deprecated Q: value", p);
	}
	return 0;
inval:
	return "Invalid tempo";
}

/* -- get a user defined symbol (U:) -- */
static char *get_user(char *p,
		      struct abcsym *s)
{
	unsigned char c;
	char *value;

	c = (unsigned char) *p++;
	if (c == '\\') {
		c = (unsigned char) *p++;
		switch (c) {
		case 'n':
			c = '\n';
			break;
		case 't':
			c = '\t';
			break;
		}
	}
	switch (char_tb[c]) {
	default:
		return "Bad decoration character";
	case CHAR_DECO:
		break;
	case CHAR_BAD:
	case CHAR_IGN:
	case CHAR_SPAC:
	case CHAR_DECOS:
	case CHAR_LINEBREAK:
		char_tb[c] = CHAR_DECO;
		break;
	}
	s->u.user.symbol = c;

	/* skip '=' */
	while (isspace((unsigned char) *p) || *p == '=')
		p++;
	if (char_tb[(unsigned char) *p] == CHAR_DECOS)
		p++;
/*fixme: 'U: <char> = "text"' is not treated */
	get_deco(p, &s->u.user.value);

	/* treat special pseudo decorations */
	value = deco_tb[s->u.user.value - 128];
	if (strcmp(value, "beambreak") == 0)
		char_tb[c] = CHAR_SPAC;
	else if (strcmp(value, "ignore") == 0)
		char_tb[c] = CHAR_IGN;
	else if (strcmp(value, "nil") == 0
	      || strcmp(value, "none") == 0)
		char_tb[c] = CHAR_BAD;
	else
		return 0;
	s->u.user.value	= 0;		/* not a decoration */
	return 0;
}

/* -- parse the voice parameters (V:) -- */
static char *parse_voice(char *p,
			 struct abcsym *s)
{
	int voice;
	char *error_txt = NULL;
	char name[VOICE_NAME_SZ];
	char *clef_name, *clef_middle, *clef_lines, *clef_scale;
	char *p_octave;
	signed char *p_stem;
static struct kw_s {
	char *name;
	short len;
	short index;
} kw_tb[] = {
	{"name=", 5, 0},
	{"nm=", 3, 0},
	{"subname=", 8, 1},
	{"sname=", 6, 1},
	{"snm=", 4, 1},
	{"merge", 5, 2},
	{"up", 2, 3},
	{"down", 4, 4},
	{"stem=", 5, 5},
	{"gstem=", 6, 6},
	{"auto", 4, 7},
	{"dyn=", 4, 8},
	{"lyrics=", 7, 9},
	{"scale=", 6, 10},
	{"gchord=", 7, 11},
	{0}
};
	struct kw_s *kw;

	/* save the parameters of the previous voice */
	curvoice->ulen = ulen;
	curvoice->microscale = microscale;

	if (voice_tb[0].id[0] == '\0') {
		switch (s->prev->type) {
		case ABC_T_EOLN:
		case ABC_T_NOTE:
		case ABC_T_REST:
		case ABC_T_BAR:
			/* the previous voice was implicit (after K:) */
			voice_tb[0].id[0] = '1';
			break;
		}
	}
	{
		char *id, sep;

		id = p;
		while (isalnum((unsigned char) *p) || *p == '_')
			p++;
		sep = *p;
		*p = '\0';
		if (voice_tb[0].id[0] == '\0') {
			voice = 0;			/* first voice */
		} else {
			for (voice = 0; voice <= nvoice; voice++) {
				if (strcmp(id, voice_tb[voice].id) == 0)
					goto found;
			}
			if (voice >= MAXVOICE) {
				syntax("Too many voices", id);
				voice--;
			}
		}
		nvoice = voice;
		strncpy(voice_tb[voice].id, id, sizeof voice_tb[voice].id - 1);
		voice_tb[voice].mvoice = voice;
	found:
		strcpy(s->u.voice.id, voice_tb[voice].id);
		*p = sep;
	}
	curvoice = &voice_tb[voice];
	s->u.voice.voice = voice;

	/* if in tune, set the voice parameters */
	if (abc_state == ABC_S_TUNE) {
		ulen = curvoice->ulen;
		microscale = curvoice->microscale;
	}

	/* parse the other parameters */
	clef_name = clef_middle = clef_lines = clef_scale = NULL;
	p_octave = NULL;
	p_stem = &s->u.voice.stem;
	for (;;) {
		while (isspace((unsigned char) *p))
			p++;
		if (*p == '\0')
			break;
		p = parse_extra(p, &clef_name, &clef_middle, &clef_lines,
				&clef_scale, &p_octave);
		if (*p == '\0')
			break;
		for (kw = kw_tb; kw->name; kw++) {
			if (strncmp(p, kw->name, kw->len) == 0)
				break;
		}
		if (!kw->name) {
			while (!isspace((unsigned char) *p) && *p != '\0')
				p++;	/* ignore unknown keywords */
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
		case 2:			/* merge */
			s->u.voice.merge = 1;
			break;
		case 3:			/* up */
			*p_stem = 1;
			break;
		case 4:			/* down */
			*p_stem = -1;
			break;
		case 5:			/* stem= */
			p_stem = &s->u.voice.stem;
			break;
		case 6:			/* gstem= */
			p_stem = &s->u.voice.gstem;
			break;
		case 7:			/* auto */
			*p_stem = 2;
			break;
		case 8:			/* dyn= */
			p_stem = &s->u.voice.dyn;
			break;
		case 9:			/* lyrics= */
			p_stem = &s->u.voice.lyrics;
			break;
		case 10: {		/* scale= */
			float sc;

			sc = atof(p);
			if (sc >= 0.5 && sc <= 2)
				s->u.voice.scale = sc;
			else
				error_txt = "Bad value for voice scale";
			while (!isspace((unsigned char) *p) && *p != '\0')
				p++;
			break;
		    }
		case 11:		/* gchord= */
			p_stem = &s->u.voice.gchord;
			break;
		}
	}

	s->u.voice.octave = parse_octave(p_octave);

	if (clef_name || clef_middle || clef_lines || clef_scale) {
		s = abc_new(s->tune, NULL, NULL);
		s->type = ABC_T_CLEF;
		parse_clef(s, clef_name, clef_middle, clef_lines,
				clef_scale);
	}
	return error_txt;
}

/* -- parse a bar -- */
static char *parse_bar(struct abctune *t,
		       char *p)
{
	struct abcsym *s;
	int bar_type;
	char repeat_value[32];

	p--;
	bar_type = 0;
	for (;;) {
		switch (*p++) {
		case '|':
			bar_type <<= 4;
			bar_type |= B_BAR;
			continue;
		case '[':
			bar_type <<= 4;
			bar_type |= B_OBRA;
			continue;
		case ']':
			bar_type <<= 4;
			bar_type |= B_CBRA;
			continue;
		case ':':
			bar_type <<= 4;
			bar_type |= B_COL;
			continue;
		default:
			break;
		}
		break;
	}
	p--;

	/* if the last element is '[', it may start
	 * a chord, an embedded header or an other bar */
	if ((bar_type & 0x0f) == B_OBRA && bar_type != B_OBRA
	 && *p != ' ') {
		bar_type >>= 4;
		p--;
	}

	if (bar_type == (B_OBRA << 8) + (B_BAR << 4) + B_CBRA)	/* [|] */
		bar_type = (B_OBRA << 4) + B_CBRA;		/* [] */

/*	curvoice->last_note = NULL; */
	if (vover > 0) {
		curvoice = &voice_tb[curvoice->mvoice];
		vover = 0;
	}
	s = abc_new(t, gchord, NULL);
	if (gchord) {
		if (free_f)
			free_f(gchord);
		gchord = NULL;
	}
	s->type = ABC_T_BAR;
	s->u.bar.type = bar_type;

	if (dc.n > 0) {
		memcpy(&s->u.bar.dc, &dc, sizeof s->u.bar.dc);
		dc.n = dc.h = dc.s = 0;
	}
	if (!isdigit((unsigned char) *p)	/* if not a repeat bar */
	 && (*p != '"' || p[-1] != '['))	/* ('["' only) */
		return p;

	if (*p == '"') {
		p = get_str(repeat_value, p, sizeof repeat_value);
	} else {
		char *q;

		q = repeat_value;
		while (isdigit((unsigned char) *p)
		    || *p == ','
		    || *p == '-'
		    || (*p == '.' && isdigit((unsigned char) p[1]))) {
			if (q < &repeat_value[sizeof repeat_value - 1])
				*q++ = *p++;
			else
				p++;
		}
		*q = '\0';
	}
	if (bar_type != B_OBRA
	 || s->text) {
		s = abc_new(t, repeat_value, NULL);
		s->type = ABC_T_BAR;
		s->u.bar.type = B_OBRA;
	} else {
		s->text = alloc_f(strlen(repeat_value) + 1);
		strcpy(s->text, repeat_value);
	}
	s->u.bar.repeat_bar = 1;
	return p;
}

/* -- parse note or rest with pitch and length -- */
/* in case of error, 'accidental' is set to -1 */
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
		p++;
		if (*p == '^') {
			p++;
			acc = A_DS;
		} else {
			acc = A_SH;
		}
		break;
	case '=':
		p++;
		acc = A_NT;
		break;
	case '_':
		p++;
		if (*p == '_') {
			p++;
			acc = A_DF;
		} else {
			acc = A_FT;
		}
		break;
	}

	/* look for microtone value */
	if (acc != 0
	 && (isdigit((unsigned char) *p)
	  || (*p == '/' && microscale == 0))) {
		int n, d;
		char *q;

		n = d = 1;
		if (*p != '/') {
			n = strtol(p, &q, 10);
			p = q;
		}
		if (*p == '/') {
			p++;
			if (!isdigit((unsigned char) *p)) {
				d = 2;
			} else {
				d = strtol(p, &q, 10);
				p = q;
			}
		}
		if (microscale == 0) {
			d--;
			d += (n - 1) << 8;	/* short [ (n-1) | (d-1) ] */
			for (n = 1; n < MAXMICRO; n++) {
				if (p_micro[n] == d)
					break;
				if (p_micro[n] == 0) {
					p_micro[n] = d;
					break;
				}
			}
			if (n == MAXMICRO) {
				syntax("Too many microtone accidentals", p);
				n = 0;
			}
		}
		acc += (n << 3);
	}

	/* get the pitch */
	{
		char *p_n;

		p_n = strchr(all_notes, *p);
		if (!p_n) {
			syntax(acc ? "Missing note after accidental"
				   : "Not a note", p);
			acc = -1;
			if (*p == '\0')
				p--;
		} else {
			pit = p_n - all_notes + 16;
		}
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

	*pitch = pit;
	*length = len;
	*accidental = acc;
	*stemless = nostem;
	return p;
}

/* -- parse the decorations of notes and bars -- */
char *parse_deco(char *p,
		 struct deco *deco)
{
	int n;
	unsigned char c, d;

	n = deco->n;
	for (;;) {
		c = (unsigned char) *p++;
		if (char_tb[c] != CHAR_DECO && char_tb[c] != CHAR_DECOS)
			break;
		if (char_tb[c] == CHAR_DECOS) {
			p = get_deco(p, &d);
			c = d;
		}
		if (n >= MAXDC)
			syntax("Too many decorations for the note", p);
		else if (c != 0)
			deco->t[n++] = c;
	}
	deco->n = n;
	return p - 1;
}

/* -- parse a decoration line (d: or s:) -- */
static char *parse_decoline(char *p)
{
	struct abcsym *is;
	unsigned char d;
	int n;

	if ((is = deco_cont) == NULL)
		is = deco_start;
	else
		deco_cont = NULL;

	/* scan the decoration line */
	while (*p != '\0') {
		while (isspace((unsigned char) *p))
			p++;
		if (*p == '\0')
			break;
		switch (*p) {
		case '|':
			while (is && (is->type != ABC_T_BAR
					|| is->u.bar.type == B_OBRA))
				is = is->next;
			if (!is) {
				syntax("Not enough bar lines for deco line", p);
				return NULL;
			}
			is = is->next;
			p++;
			continue;
		case '*':
			while (is && is->type != ABC_T_NOTE)
				is = is->next;
			if (!is) {
				syntax("Not enough notes for deco line", p);
				return NULL;
			}
			is = is->next;
			p++;
			continue;
		case '\\':
			if (p[1] == '\0') {
				if (!is)
					return "Not enough notes for deco line";
				deco_cont = is;
				return NULL;
			}
			syntax("'\\' ignored", p);
			p++;
			continue;
		case '"':
			p = parse_gchord(p + 1);
			break;
		default:
			if (char_tb[(unsigned char) *p] == CHAR_DECOS)
				p = get_deco(p + 1, &d);
			else
				d = (unsigned char) *p++;
			break;
		}

		/* store the decoration / gchord/annotation in the next note */
		while (is && (is->type != ABC_T_NOTE
				|| (is->flags & ABC_F_GRACE)))
			is = is->next;
		if (!is)
			return "Not enough notes for deco line";

		if (gchord) {
			if (is->text) {
				char *gch;

				n = strlen(is->text);
				gch = alloc_f(n + strlen(gchord) + 2);
				strcpy(gch, is->text);
				gch[n] = '\n';
				strcpy(gch + n + 1, gchord);
				if (free_f) {
					free_f(gchord);
					free_f(is->text);
				}
				gchord = gch;
			}
			is->text = gchord;
			gchord = NULL;
		} else {
			n = is->u.note.dc.n;
			if (n >= MAXDC) {
				syntax("Too many decorations for the note", p);
			} else if (d != 0) {
				is->u.note.dc.t[n] = d;
				is->u.note.dc.n = n + 1;
			}
		}
		is = is->next;
	}
	return NULL;
}

/* -- parse a guitar chord / annotation -- */
static char *parse_gchord(char *p)
{
	char *q;
	int l, l2;

	q = p;
	while (*p != '"') {
		if (*p == '\\')
			p++;
		if (*p == '\0') {
			syntax("No end of guitar chord", p);
			break;
		}
		p++;
	}
	l = p - q;
	if (gchord) {
		char *gch;

		/* many guitar chords: concatenate with '\n' */
		l2 = strlen(gchord);
		gch = alloc_f(l2 + 1 + l + 1);
		strcpy(gch, gchord);
		gch[l2++] = '\n';
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
	return p;
}

/* -- parse a note length -- */
static char *parse_len(char *p,
			int *p_len)
{
	int len, fac;
	char *q;

	len = BASE_LEN;
	if (isdigit((unsigned char) *p)) {
		len *= strtol(p, &q, 10);
		if (len <= 0) {
			syntax("Bad length", p);
			len = BASE_LEN;
		}
		p = q;
	}
	fac = 1;
	while (*p == '/') {
		p++;
		if (isdigit((unsigned char) *p)) {
			fac *= strtol(p, &q, 10);
			if (fac == 0) {
				syntax("Bad length divisor", p - 1);
				fac = 1;
			}
			p = q;
		} else {
			fac *= 2;
		}
	}
	if (len % fac)
		syntax("Bad length divisor", p - 1);
	len /= fac;
	*p_len = len;
	return p;
}

/* -- parse a ABC line -- */
/* return 1 on end of tune, and 2 on start of new tune */
static int parse_line(struct abctune *t,
		      char *p)
{
	struct abcsym *s;
	char *comment, *q, c;
	struct abcsym *last_note_sav = NULL;
	struct deco dc_sav;
	int i, flags, flags_sav = 0, slur;
	static char qtb[10] = {0, 1, 3, 2, 3, 0, 2, 0, 3, 0};

	colnum = 0;
	switch (*p) {
	case '\0':			/* blank line */
		switch (abc_state) {
		case ABC_S_GLOBAL:
		case ABC_S_HEAD:	/*fixme: may have blank lines in headers?*/
			if (keep_comment || abc_state == ABC_S_GLOBAL) {
				if (t->last_sym
				 && t->last_sym->type != ABC_T_NULL) {
					s = abc_new(t, NULL, NULL);
					s->type = ABC_T_NULL;
				}
			}
			return 0;
		}
		return 1;
	case '%':
		if (p[1] == '@') {		/* file:line_number - see front.c */
			void *fn;
			int ln;

			if (sscanf(p + 2, "%p:%d", &fn, &ln) != 2)
				return 0;
			linenum = ln;
			abc_fn = fn;
			if (strncmp(file, "%abc-", 5) == 0)
				get_vers(file + 5);
			return 0;
		}
		if (p[1] == '%') {
			if (strncmp(p + 2, "begin", 5) != 0)
				comment = decomment_line(p + 2);
			else
				comment = NULL;
			s = abc_new(t, p, comment);
			s->type = ABC_T_PSCOM;
			p += 2;				/* skip '%%' */
			if (strncasecmp(p, "abc-version ", 12) == 0) {
				get_vers(p + 12);
				return 0;
			}
			if (strncasecmp(p, "decoration ", 11) == 0) {
				p += 11;
				while (isspace((unsigned char) *p))
					p++;
				switch (*p) {
				case '!':
					char_tb['!'] = CHAR_DECOS;
					char_tb['+'] = CHAR_BAD;
					break;
				case '+':
					char_tb['+'] = CHAR_DECOS;
					char_tb['!'] = CHAR_BAD;
					break;
				}
				return 0;
			}
			if (strncasecmp(p, "linebreak ", 10) == 0) {
				for (i = 0; i < sizeof char_tb; i++) {
					if (char_tb[i] == CHAR_LINEBREAK)
						char_tb[i] = i != '!' ?
								CHAR_BAD :
								CHAR_DECOS;
				}
				p += 10;
				for (;;) {
					while (isspace((unsigned char) *p))
						p++;
					if (*p == '\0')
						break;
					switch (*p) {
					case '!':
					case '$':
					case '*':
					case ';':
					case '?':
					case '@':
						char_tb[(unsigned char) *p++]
								= CHAR_LINEBREAK;
						break;
					case '<':
						if (strcmp(p, "<none>") == 0)
							return 0;
						if (strcmp(p, "<EOL>") == 0) {
							char_tb['\n'] = CHAR_LINEBREAK;
							p += 5;
							break;
						}
						/* fall thru */
					default:
						if (strcmp(p, "lock") != 0)
							syntax("Invalid character in %%%%linebreak",
								p);
						return 0;
					}
				}
				return 0;
			}
			if (strncasecmp(p, "user ", 5) == 0) {
				p += 5;
				while (isspace((unsigned char) *p))
					p++;
				get_user(p, s);
				return 0;
			}
			return 0;
		}
		/* fall thru */
	case '\\':				/* abc2mtex specific lines */
		if (keep_comment) {
			s = abc_new(t, p, NULL);
			s->type = ABC_T_NULL;
		}
		return 0;			/* skip */
	}
	comment = decomment_line(p);

	/* header fields */
	if (p[1] == ':'
	 && *p != '|' && *p != ':') {		/* not '|:' nor '::' */
		int new_tune;

		new_tune = parse_info(t, p, comment);

		/* handle BarFly voice definition */
		/* 'V:n <note line ending with a bar>' */
		if (*p != 'V'
		 || abc_state != ABC_S_TUNE)
			return new_tune;		/* (normal return) */
		c = p[strlen(p) - 1];
		if (c != '|' && c != ']')
			return new_tune;
		while (!isspace((unsigned char) *p) && *p != '\0')
			p++;
		while (isspace((unsigned char) *p))
			p++;
	}
	if (abc_state != ABC_S_TUNE) {
		if (keep_comment) {
			s = abc_new(t, p, comment);
			s->type = ABC_T_NULL;
		}
		return 0;
	}

	flags = 0;

	if (abc_vers <= (2 << 16))
		lyric_started = 0;
	deco_start = deco_cont = NULL;
	slur = 0;
	while (*p != '\0') {
		colnum = p - abc_line;
		switch (char_tb[(unsigned char) *p++]) {
		case CHAR_GCHORD:			/* " */
			if (flags & ABC_F_GRACE)
				goto bad_char;
			p = parse_gchord(p);
			break;
		case CHAR_GR_ST:		/* '{' */
			if (flags & ABC_F_GRACE)
				goto bad_char;
			last_note_sav = curvoice->last_note;
			curvoice->last_note = NULL;
			memcpy(&dc_sav, &dc, sizeof dc);
			dc.n = dc.h = dc.s = 0;
			flags_sav = flags;
			flags = ABC_F_GRACE;
			if (*p == '/') {
				flags |= ABC_F_SAPPO;
				p++;
			}
			break;
		case CHAR_GR_EN:		/* '}' */
			if (!(flags & ABC_F_GRACE))
				goto bad_char;
			t->last_sym->flags |= ABC_F_GR_END;
			curvoice->last_note = last_note_sav;
			memcpy(&dc, &dc_sav, sizeof dc);
			flags = flags_sav;
			break;
		case CHAR_DECOS:
			if (p[-1] == '!'
			 && char_tb['\n'] == CHAR_LINEBREAK
			 && check_nl(p)) {
				s = abc_new(t, NULL, NULL);	/* abc2win EOL */
				s->type = ABC_T_EOLN;
				s->u.eoln.type = 2;
				break;
			}
			/* fall thru */
		case CHAR_DECO:
			if (p[-1] == '.') {
				if (*p == '(' || *p == '-' || *p == ')')
					break;
				if (*p == '|') {
					p = parse_bar(t, p + 1);
					t->last_sym->u.bar.dotted = 1;
					break;
				}
			}
			p = parse_deco(p - 1, &dc);
			dc.h = dc.s = dc.n;
			break;
		case CHAR_LINEBREAK:
			s = abc_new(t, NULL, NULL);
			s->type = ABC_T_EOLN;
//			s->u.eoln.type = 0;
			break;
		case CHAR_NOTE:
			p = parse_note(t, p - 1, flags);
			flags &= ABC_F_GRACE;
			t->last_sym->u.note.slur_st = slur;
			slur = 0;
			if (t->last_sym->u.note.lens[0] > 0)	/* if not space */
				curvoice->last_note = t->last_sym;
			break;
		case CHAR_SLASH:		/* '/' */
			if (flags & ABC_F_GRACE)
				goto bad_char;
			q = p;
			while (*q == '/')
				q++;
			if (char_tb[(unsigned char) *q] != CHAR_BAR)
				goto bad_char;
			s = abc_new(t, NULL, NULL);
			s->type = ABC_T_MREP;
			s->u.bar.type = 0;
			s->u.bar.len = q - p + 1;
			syntax("Non standard measure repeat syntax", p - 1);
			p = q;
			break;
		case CHAR_BSLASH:		/* '\\' */
			if (*p == '\0')
				break;
			syntax("'\\' ignored", p - 1);
			break;
		case CHAR_OBRA:			/* '[' */
			if (*p == '|' || *p == ']' || *p == ':'
			 || isdigit((unsigned char) *p) || *p == '"'
			 || *p == ' ') {
				if (flags & ABC_F_GRACE)
					goto bad_char;
				p = parse_bar(t, p);
				break;
			}
			if (p[1] != ':') {
				p = parse_note(t, p - 1, flags); /* chord */
				flags &= ABC_F_GRACE;
				t->last_sym->u.note.slur_st = slur;
				slur = 0;
				curvoice->last_note = t->last_sym;
				break;
			}

			/* embedded information field */
#if 0
/*fixme:OK for [I:staff n], ?? for other headers*/
			if (flags & ABC_F_GRACE)
				goto bad_char;
#endif
			while (p[2] == ' ') {		/* remove the spaces */
				p[2] = ':';
				p[1] = *p;
				p++;
			}
			c = ']';
			q = p;
			while (*p != '\0' && *p != c)
				p++;
			if (*p == '\0') {
				syntax("Escape sequence [..] not closed", q);
				c = '\0';
			} else {
				*p = '\0';
			}
			parse_info(t, q, NULL);
			*p = c;
			if (c != '\0')
				p++;
			break;
		case CHAR_BAR:			/* '|', ':' or ']' */
			if (flags & ABC_F_GRACE)
				goto bad_char;
			p = parse_bar(t, p);
			break;
		case CHAR_OPAR:			/* '(' */
			if (isdigit((unsigned char) *p)) {
				int pplet, qplet, rplet;

				pplet = strtol(p, &q, 10);
				if (pplet <= 1) {
					syntax("Invalid 'p' in tuplet", p);
					pplet = 0;
				}
				p = q;
				if ((unsigned) pplet < sizeof qtb / sizeof qtb[0])
					qplet = qtb[pplet];
				else
					qplet = qtb[0];
				rplet = pplet;
				if (*p == ':') {
					p++;
					if (isdigit((unsigned char) *p)) {
						qplet = strtol(p, &q, 10);
						p = q;
					}
					if (*p == ':') {
						p++;
						if (isdigit((unsigned char) *p)) {
							rplet = strtol(p, &q, 10);
							p = q;
						}
					}
				}
				if (pplet == 0)
					break;
				if (rplet < 1) {
					syntax("Invalid 'r' in tuplet", p);
					break;
				}
				if (qplet == 0)
					qplet = meter % 3 == 0 ? 3 : 2;
				s = abc_new(t, NULL, NULL);
				s->type = ABC_T_TUPLET;
				s->u.tuplet.p_plet = pplet;
				s->u.tuplet.q_plet = qplet;
				s->u.tuplet.r_plet = rplet;
				s->flags |= flags;
				break;
			}
			if (*p == '&') {
				if (flags & ABC_F_GRACE)
					goto bad_char;
				p++;
				if (vover != 0) {
					syntax("Nested voice overlay", p - 1);
					break;
				}
				s = abc_new(t, NULL, NULL);
				s->type = ABC_T_V_OVER;
				s->u.v_over.type = V_OVER_S;
				s->u.v_over.voice = curvoice - voice_tb;
				vover = -1;		/* multi-bars */
				break;
			}
			slur <<= 3;
			if (p[-2] == '.')
				slur |= SL_DOTTED;
			switch (*p) {
			case '\'':
				slur += SL_ABOVE;
				p++;
				break;
			case ',':
				slur += SL_BELOW;
				p++;
				break;
			default:
				slur += SL_AUTO;
				break;
			}
			break;
		case CHAR_CPAR:			/* ')' */
			switch (t->last_sym->type) {
			case ABC_T_NOTE:
			case ABC_T_REST:
				break;
			default:
				goto bad_char;
			}
			t->last_sym->u.note.slur_end++;
			break;
		case CHAR_VOV:			/* '&' */
			if (flags & ABC_F_GRACE)
				goto bad_char;
			if (*p != ')'
			 || vover == 0) {		/*??*/
				if (curvoice->last_note == 0) {
					syntax("Bad start of voice overlay", p);
					break;
				}
				s = abc_new(t, NULL, NULL);
				s->type = ABC_T_V_OVER;
				/*s->u.v_over.type = V_OVER_V; */
				vover_new();
				s->u.v_over.voice = curvoice - voice_tb;
				if (vover == 0)
					vover = 1;	/* single bar */
				break;
			}
			p++;
			vover = 0;
			s = abc_new(t, NULL, NULL);
			s->type = ABC_T_V_OVER;
			s->u.v_over.type = V_OVER_E;
			s->u.v_over.voice = curvoice->mvoice;
			curvoice->last_note = NULL;	/* ?? */
			curvoice = &voice_tb[curvoice->mvoice];
			break;
		case CHAR_SPAC:			/* ' ' and '\t' */
			flags |= ABC_F_SPACE;
			break;
		case CHAR_MINUS: {		/* '-' */
			int tie_pos;

			if (!curvoice->last_note
			 || curvoice->last_note->type != ABC_T_NOTE)
				goto bad_char;
			if (p[-2] == '.')
				tie_pos = SL_DOTTED;
			else
				tie_pos = 0;
			switch (*p) {
			case '\'':
				tie_pos += SL_ABOVE;
				p++;
				break;
			case ',':
				tie_pos += SL_BELOW;
				p++;
				break;
			default:
				tie_pos += SL_AUTO;
				break;
			}
			for (i = 0; i <= curvoice->last_note->u.note.nhd; i++) {
				if (curvoice->last_note->u.note.ti1[i] == 0)
					curvoice->last_note->u.note.ti1[i] = tie_pos;
				else if (curvoice->last_note->u.note.nhd == 0)
					syntax("Too many ties", p);
			}
			break;
		    }
		case CHAR_BRHY:			/* '>' and '<' */
			if (!curvoice->last_note)
				goto bad_char;
			i = 1;
			while (*p == p[-1]) {
				i++;
				p++;
			}
			if (i > 3) {
				syntax("Bad broken rhythm", p - 1);
				i = 3;
			}
			if (p[-1] == '<')
				i = -i;
			broken_rhythm(&curvoice->last_note->u.note, i);
			curvoice->last_note->u.note.brhythm = i;
			break;
		case CHAR_IGN:			/* '*' & '`' */
			break;
		default:
		bad_char:
			syntax((flags & ABC_F_GRACE)
					? "Bad character in grace note sequence"
					: "Bad character",
				p - 1);
			break;
		}
	}

/*fixme: may we have grace notes across lines?*/
	if (flags & ABC_F_GRACE) {
		syntax("EOLN in grace note sequence", p - 1);
		if (curvoice->last_note)
			curvoice->last_note->flags |= ABC_F_GR_END;
		curvoice->last_note = last_note_sav;
		memcpy(&dc, &dc_sav, sizeof dc);
	}

	/* add eoln */
	s = abc_new(t, NULL, NULL);
	s->type = ABC_T_EOLN;
	if (flags & ABC_F_SPACE)
		s->flags |= ABC_F_SPACE;
	if (p[-1] == '\\'
	 || char_tb['\n'] != CHAR_LINEBREAK)
		s->u.eoln.type = 1;		/* no break */
	return 0;
}

/* -- parse a note or a rest -- */
static char *parse_note(struct abctune *t,
			char *p,
			int flags)
{
	struct abcsym *s;
	char *q;
	int pit, len, acc, nostem, chord, j, m;

	if (flags & ABC_F_GRACE) {	/* in a grace note sequence */
		s = abc_new(t, NULL, NULL);
	} else {
		s = abc_new(t, gchord, NULL);
		if (gchord) {
			if (free_f)
				free_f(gchord);
			gchord = NULL;
		}
	}
	s->type = ABC_T_NOTE;
	s->flags |= flags;

	if (*p != 'X' && *p != 'Z'
	 && !(flags & ABC_F_GRACE)) {
		if (!lyric_started) {
			lyric_started = 1;
			s->flags |= ABC_F_LYRIC_START;
		}
		if (!deco_start)
			deco_start = s;
	}
	chord = 0;

	/* rest */
	switch (*p) {
	case 'X':
		s->flags |= ABC_F_INVIS;
	case 'Z':			/* multi-rest */
		s->type = ABC_T_MREST;
		p++;
		len = 1;
		if (isdigit((unsigned char) *p)) {
			len = strtol(p, &q, 10);
			if (len == 0 && len > 100) {
				syntax("Bad number of measures", p);
				len = 1;
			}
			p = q;
		}
		s->u.bar.type = 0;
		s->u.bar.len = len;
		goto add_deco;
	case 'y':			/* space (BarFly) */
		s->type = ABC_T_REST;
		s->flags |= ABC_F_INVIS;
		p++;
		if (isdigit((unsigned char) *p)) {	/* number of points */
			s->u.note.lens[1] = strtol(p, &q, 10);
			p = q;
		} else {
			s->u.note.lens[1] = -1;
		}
		goto add_deco;
	case 'x':			/* invisible rest */
		s->flags |= ABC_F_INVIS;
		/* fall thru */
	case 'z':
		s->type = ABC_T_REST;
		p = parse_len(p + 1, &len);
		s->u.note.lens[0] = len * ulen / BASE_LEN;
		goto do_brhythm;
	case '[':			/* '[..]' = chord */
		chord = 1;
		p++;
		break;
	}

	q = p;

	/* get pitch, length and possible accidental */
	m = 0;
	nostem = 0;
	for (;;) {
		int tmp;

		if (chord) {
			if (m >= MAXHD) {
				syntax("Too many notes in chord", p);
				m--;
			}
			tmp = 0;
			if (*p == '.') {
				tmp = SL_DOTTED;
				p++;
			}
			if (*p == '(') {
				p++;
				switch (*p) {
				case '\'':
					tmp += SL_ABOVE;
					p++;
					break;
				case ',':
					tmp += SL_BELOW;
					p++;
					break;
				default:
					tmp += SL_AUTO;
					break;
				}
				s->u.note.sl1[m] = (s->u.note.sl1[m] << 3)
							+ tmp;
			}
		}
		tmp = dc.n;
		p = parse_deco(p, &dc);		/* note head decorations */
		if (dc.n != tmp) {
			if (dc.n - tmp >= 8) {
				syntax("Too many decorations on this head", p);
				tmp = dc.n - 7;
			}
			s->u.note.decs[m] = (tmp << 3) + dc.n - tmp;
			dc.s = dc.n;
		}
		p = parse_basic_note(p, &pit, &len, &acc, &tmp);
		if (!(flags & ABC_F_GRACE))
			len = len * ulen / BASE_LEN;
		else
			len /= 8;		/* for grace note alone */

		s->u.note.pits[m] = pit;
		s->u.note.lens[m] = len;
		s->u.note.accs[m] = acc;
		nostem |= tmp;

		if (chord) {
			for (;;) {
				if (*p == '.') {
					if (p[1] != '-')
						break;
					p++;
				}
				if (*p == '-') {
					switch (p[1]) {
					case '\'':
						s->u.note.ti1[m] = SL_ABOVE;
						p++;
						break;
					case ',':
						s->u.note.ti1[m] = SL_BELOW;
						p++;
						break;
					default:
						s->u.note.ti1[m] = SL_AUTO;
						break;
					}
				} else if (*p == ')') {
					s->u.note.sl2[m]++;
				} else {
					break;
				}
				p++;
			}
		}
		if (acc >= 0)			/* if no error */
			m++;			/* normal case */

		if (!chord)
			break;
		if (*p == ']') {
			p++;
			if (*p == '0') {
				nostem |= 1;
				p++;
			}
			if (*p == '/' || isdigit((unsigned char) *p)) {
				p = parse_len(p, &len);
				s->u.note.chlen = len;
				for (j = 0; j < m; j++) {
					tmp = len * s->u.note.lens[j];
					s->u.note.lens[j] = tmp / BASE_LEN;
				}
			}
			break;
		}
		if (*p == '\0') {
			syntax("Chord not closed", q);
			break;
		}
	}
	if (nostem)
		s->flags |= ABC_F_STEMLESS;

	if (m == 0) {			/* if no note (or error) */
		if ((t->last_sym = s->prev) == NULL) {
			t->first_sym = NULL;
		} else {
			s->prev->next = NULL;
			s->prev->flags |= (s->flags & ABC_F_ERROR);
		}
		return p;
	}
	s->u.note.microscale = microscale;
	s->u.note.nhd = m - 1;
do_brhythm:
	if (curvoice->last_note
	 && curvoice->last_note->u.note.brhythm != 0)
		broken_rhythm(&s->u.note,
			      -curvoice->last_note->u.note.brhythm);
add_deco:
	if (dc.n > 0) {
		memcpy(s->type != ABC_T_MREST ? &s->u.note.dc
				: &s->u.bar.dc,
			&dc, sizeof dc);
		dc.n = dc.h = dc.s = 0;
	}
	return p;
}

/* -- parse an information field -- */
/* return 2 on start of new tune */
static int parse_info(struct abctune *t,
			char *p,
			char *comment)
{
	struct abcsym *s;
	char info_type = *p;
	char *error_txt = NULL;

	s = abc_new(t, p, comment);
	s->type = ABC_T_INFO;

	p += 2;

	switch (info_type) {
	case 'd':
	case 's':
		if (abc_state == ABC_S_GLOBAL)
			break;
		if (!deco_start) {
			error_txt = "Erroneous 'd:'/'s:'";
			break;
		}
		error_txt = parse_decoline(p);
		break;
	case 'K':
		if (abc_state == ABC_S_GLOBAL)
			break;
		parse_key(p, s);
		if (abc_state == ABC_S_HEAD) {
			int i;

			abc_state = ABC_S_TUNE;
			if (ulen == 0)
				ulen = BASE_LEN / 8;
			for (i = MAXVOICE; --i >= 0; )
				voice_tb[i].ulen = ulen;
			lyric_started = 0;
		}
		break;
	case 'L':
		error_txt = get_len(p, s);
		if (s->u.length.base_length > 0)
			ulen = s->u.length.base_length;
		break;
	case 'M':
		error_txt = parse_meter(p, s);
		break;
	case 'Q':
		error_txt = parse_tempo(p, s);
		break;
	case 'U':
		error_txt = get_user(p, s);
		break;
	case 'V':
		if (abc_state == ABC_S_GLOBAL)
			break;
		error_txt = parse_voice(p, s);
		break;
	case 'X':
		memset(voice_tb, 0, sizeof voice_tb);
		nvoice = 0;
		curvoice = &voice_tb[0];
		abc_state = ABC_S_HEAD;
		if (level_f)
			level_f(1);
		return 2;
	}
	if (error_txt)
		syntax(error_txt, p);
	return 0;
}

/* -- print a syntax error message -- */
static void syntax(char *msg,
		   char *q)
{
	int n, len, m1, m2, pp;
	int maxcol = 73;

	severity = 1;
	n = q - abc_line;
	len = strlen(abc_line);
	if ((unsigned) n > (unsigned) len)
		n = -1;
	print_error(msg, n);
	if (n < 0) {
		if (q && *q != '\0')
			fprintf(stderr, " (near '%s')\n", q);
		return;
	}
	m1 = 0;
	m2 = --len;
	if (m2 > maxcol) {
		if (n < maxcol) {
			m2 = maxcol;
		} else {
			m1 = n - 20;
			m2 = m1 + maxcol;
			if (m2 > len)
				m2 = len;
		}
	}

	fprintf(stderr, "%4d ", linenum);
	pp = 6;
	if (m1 > 0) {
		fprintf(stderr, "...");
		pp += 3;
	}
	fprintf(stderr, "%*s", m2 - m1, &abc_line[m1]);
	if (m2 < len)
		fprintf(stderr, "...");
	fprintf(stderr, "\n");

	if ((unsigned) n < 200)
		fprintf(stderr, "%*s\n", n + pp - m1, "^");

	if (last_sym)
		last_sym->flags |= ABC_F_ERROR;
}

/* -- switch to a new voice overlay -- */
static void vover_new(void)
{
	int voice, mvoice;

	mvoice = curvoice->mvoice;
	for (voice = curvoice - voice_tb + 1; voice <= nvoice; voice++)
		if (voice_tb[voice].mvoice == mvoice)
			break;
	if (voice > nvoice) {
		if (nvoice >= MAXVOICE) {
			syntax("Too many voices", 0);
			return;
		}
		nvoice = voice;
		voice_tb[voice].id[0] = '&';
		voice_tb[voice].mvoice = mvoice;
	}
	voice_tb[voice].ulen = curvoice->ulen;
	voice_tb[voice].microscale = curvoice->microscale;
	curvoice = &voice_tb[voice];
}
