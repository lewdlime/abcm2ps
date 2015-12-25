/*++
 * Generic ABC parser.
 *
 * Copyright (C) 1998-2004 Jean-François Moine
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
static int lyric_started;	/* lyric started */
static struct abcsym *lyric_start;	/* 1st note of the line for d: */
static struct abcsym *lyric_cont;	/* current symbol when d: continuation */
static int vover_bar;		/* in a simple voice overlay sequence */

#define VOICE_NAME_SZ 64	/* max size of a voice name */

static unsigned char *file;	/* remaining abc file */
static short linenum;		/* current line number */
static char *scratch_line;	/* parse line */
static int scratch_length = 0;	/* allocated length */
static int line_length;		/* current line length */

static short nvoice;		/* number of voices (0..n-1) */
static struct {			/* voice table and current pointer */
	char name[32];			/* voice name */
	struct abcsym *last_note;	/* last note or rest */
	struct abcsym *tie;		/* last note with starting ties */
	short ulen;			/* unit note length */
	char slur;			/* number of slur starts */
	unsigned char pplet, qplet, rplet; /* nplet - fixme: may be global?*/
	signed char add_pitch;		/* key transpose */
	unsigned char mvoice;		/* main voice when voice overlay */
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
#define CHAR_OPAR 11
#define CHAR_VOV 12
#define CHAR_VOVE 13
#define CHAR_SPAC 14
#define CHAR_MINUS 15
#define CHAR_CPAR 16
#define CHAR_BRHY 17
static char char_tb[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, CHAR_SPAC, 0, 0, 0, 0, 0, 0,	/* 00 - 0f */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* 10 - 1f */
	CHAR_SPAC, CHAR_DECO, CHAR_GCHORD, CHAR_BAD,	/* (sp) ! " # */
	CHAR_BAD, CHAR_BAD, CHAR_VOV, CHAR_BAD, 	/* $ % & ' */
	CHAR_OPAR, CHAR_CPAR, CHAR_IGN, CHAR_BAD, 	/* ( ) * + */
	CHAR_BAD, CHAR_MINUS, CHAR_DECO, CHAR_BAD, 	/* , - . / */
	CHAR_BAD, CHAR_BAD, CHAR_BAD, CHAR_BAD, 	/* 0 1 2 3 */
	CHAR_BAD, CHAR_BAD, CHAR_BAD, CHAR_BAD, 	/* 4 5 6 7 */
	CHAR_BAD, CHAR_BAD, CHAR_BAR, CHAR_BAD, 	/* 8 9 : ; */
	CHAR_BRHY, CHAR_ACC, CHAR_BRHY, CHAR_BAD, 	/* < = > ? */
	CHAR_BAD, CHAR_NOTE, CHAR_NOTE, CHAR_NOTE, 	/* @ A B C */
	CHAR_NOTE, CHAR_NOTE, CHAR_NOTE, CHAR_NOTE, 	/* D E F G */
	CHAR_DECO, CHAR_DECO, CHAR_DECO, CHAR_DECO, 	/* H I J K */
	CHAR_DECO, CHAR_DECO, CHAR_DECO, CHAR_DECO, 	/* L M N O */
	CHAR_DECO, CHAR_DECO, CHAR_DECO, CHAR_DECO, 	/* P Q R S */
	CHAR_DECO, CHAR_DECO, CHAR_DECO, CHAR_DECO, 	/* T U V W */
	CHAR_DECO, CHAR_DECO, CHAR_REST, CHAR_OBRA, 	/* X Y Z [ */
	CHAR_BSLASH, CHAR_BAR, CHAR_ACC, CHAR_ACC, 	/* \ ] ^ _ */
	CHAR_IGN, CHAR_NOTE, CHAR_NOTE, CHAR_NOTE, 	/* ` a b c */
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

static unsigned char *get_line(void);
static unsigned char *parse_len(unsigned char *p,
				int *p_len);
static char *parse_basic_note(char *p,
			      int *pitch,
			      int *length,
			      int *accidental,
			      int *stemless);
static void parse_header(struct abctune *t,
			 unsigned char *p,
			 char *comment);
static int parse_line(struct abctune *t,
		      unsigned char *p);
static unsigned char *parse_note(struct abctune *t,
				 unsigned char *p);
static void syntax(char *msg, char *q);
static void vover_new(void);

/* -- abcMIDI like errors -- */
static void print_error(char *s,
			int col)
{
	if (col >= 0)
		fprintf(stderr, "Error in line %d.%d: %s\n", linenum, col, s);
	else	fprintf(stderr, "Error in line %d: %s\n", linenum, s);
}

static void print_warning(char *s)
{
	fprintf(stderr, "Warning in line %d: %s\n", linenum, s);
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
			if (as->u.tempo.str2)
				free_f(as->u.tempo.str2);
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
	if (scratch_line != 0) {
		fprintf(stderr, "abc_init already initialized\n");
		return;
	}
	scratch_line = malloc(256 + 1);
	scratch_length = 256;
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
			break;			/* blank line --> done */

/*fixme-insert: don't accept X: nor T:*/
		/* parse the music line */
		if (!parse_line(t, p))
			break;
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
				print_error("unexpected EOF in header definition",
					    -1);
				severity = 1;
			}
			break;			/* done */
		}
		while (isspace(*p))		/* skip starting blanks */
			p++;

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
			if (i > 0 && p[-2] != '\\') {
				if (keep_comment)
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
	return comment;
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

	if (voice_tb[0].name[0] == '\0')
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
	strncpy(voice_tb[voice].name, name, sizeof voice_tb[voice].name - 1);
	voice_tb[voice].mvoice = voice;
done:
	*p_voice = voice;
	*p = sep;
	return p;
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
	note->len = l;
}

/* -- check for the '!' as end of line (ABC2Win) -- */
static int check_nl(unsigned char *p)
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

/* -- skip a clef definition -- */
static unsigned char *clef_skip(unsigned char *p,
				unsigned char **p_name,
				unsigned char **p_middle)
{
	for (;;) {
		if (strncmp(p, "clef=", 5) == 0
		    || strncmp(p, "bass", 4) == 0
		    || strncmp(p, "treble", 6) == 0
		    || strncmp(p, "alto", 4) == 0
		    || strncmp(p, "tenor", 5) == 0
		    || strncmp(p, "perc", 4) == 0
		    || strncmp(p, "none", 4) == 0) {
			if (*p_name != 0)
				syntax("Double clef name", p);
			*p_name = p;
		} else if (strncmp(p, "middle=", 7) == 0) {
			if (*p_middle != 0)
				syntax("Double clef middle", p);
			*p_middle = p;
		} else	break;
		while (!isspace(*p) && *p != '\0')
			p++;
		while (isspace(*p))
			p++;
		if (*p == '\0')
			break;
	}
	return p;
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

/* -- parse a list of accidentals (K:) -- */
static unsigned char *parse_acc(unsigned char *p,
				struct abcsym *s)
{
	int pit, len, acc, nostem, nacc;

	nacc = s->u.key.nacc;
	for (;;) {
		if (nacc >= sizeof s->u.key.pits) {
			syntax("Too many accidentals", 0);
			break;
		}
		p = parse_basic_note(p, &pit, &len, &acc, &nostem);
		s->u.key.pits[nacc] = pit - curvoice->add_pitch;
		s->u.key.accs[nacc++] = acc;
		if (*p == '\0')
			break;
		if (*p != '^' && *p != '_' && *p != '=')
			break;
	}
	s->u.key.nacc = nacc;
	return p;
}

/* -- parse a clef (K: or V:) -- */
static void parse_clef(struct clef_s *p_clef,
		       unsigned char *name,
		       unsigned char *middle)
{
	int clef = -1;
	int transpose = 0;
	int clef_line = 2;
	int clef_name;

	clef_name = 0;
	if (name != 0 && strncmp(name, "clef=", 5) == 0) {
		name += 5;
		clef_name = 1;
		switch (*name) {
		case 'g':
			transpose = -7;
		case 'G':
			clef = TREBLE;
			break;
		case 'f':
			transpose = -14;
			clef = BASS;
			clef_line = 4;
			break;
		case 'F':
			transpose = -7;
			clef = BASS;
			clef_line = 4;
			break;
		case 'c':
			transpose = -7;
		case 'C':
			clef = ALTO;
			clef_line = 3;
			break;
		case 'P':
			clef = PERC;
			break;
		default:
			clef_name = 0;
		}
		if (clef_name) {
			name++;
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
	if (name != 0 && !clef_name) {
		if (!strncmp(name, "bass", 4)) {
			clef = BASS;
			clef_line = 4;
#ifdef CLEF_TRANSPOSE
			transpose = -14;
#else
			p_clef->check_pitch = 1;
#endif
			name += 4;
			clef_name = 1;
		} else if (!strncmp(name, "treble", 6)) {
			clef = TREBLE;
			name += 6;
			clef_name = 1;
		} else if (!strncmp(name, "alto", 4)
			   || !strncmp(name, "tenor", 5)) {
			clef = ALTO;
			clef_line = *name == 'a' ? 3 : 4;
#ifdef CLEF_TRANSPOSE
			transpose = -7;
#else
			p_clef->check_pitch = 1;
#endif
			if (*name == 'a')
				name += 4;
			else	name += 5;
			clef_name = 1;
		} else if (!strncmp(name, "perc", 4)) {
			clef = PERC;
			name += 4;
		} else /*if (strncmp(name, "none", 4) == 0)*/ {
			clef = TREBLE;
			p_clef->invis = 1;
			name += 4;
		}

		if (clef_name) {
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
				if (*name == '-')
					p_clef->octave = -1;
				else if (*name == '+')
					p_clef->octave = 1;
			}
		}
	}

	if (middle != 0) {
		int pit, len, acc, nostem, l;

		/* 'middle=<note pitch>' */
		middle += 7;
		if (clef < 0)
			clef = TREBLE;
		curvoice->add_pitch = 0;
		parse_basic_note(middle, &pit, &len, &acc, &nostem);
		l = 20;
		switch (clef) {
		case ALTO:
			l = 16;
			break;
		case BASS:
			l = 12;
			break;
		}
		l = l - pit + 4 + 14;
		clef_line = (l % 7) / 2 + 1;
		transpose = l / 7 * 7 - 14;
#ifndef CLEF_TRANSPOSE
		p_clef->check_pitch = 0;
#endif
	}

	p_clef->type = clef;
	p_clef->line = clef_line;
	p_clef->transpose = transpose;
	curvoice->add_pitch = transpose;
}

/* -- parse a 'K:' -- */
static void parse_key(unsigned char *p,
		      struct abcsym *s)
{
	int sf;
	unsigned char *clef_name, *clef_middle;

	clef_name = 0;
	clef_middle = 0;
	p = clef_skip(p, &clef_name, &clef_middle);
	sf = 0;
	switch (*p++) {
	case 'F': sf = -1; break;
	case 'B': sf++;
	case 'E': sf++;
	case 'A': sf++;
	case 'D': sf++;
	case 'G': sf++;
	case 'C': break;
	case 'H':
		s->u.key.bagpipe = 1;
		if (*p == 'P')
			p++;
		else if (*p == 'p') {
			sf = 2;
			p++;
		} else	syntax("Unknown bagpipe-like key", p);
		break;
	case '^':
	case '_':
	case '=':
		p--;			/* explicit accidentals */
		break;
	case '\0':
		s->u.key.empty = 1;
		p--;
		break;
	default:
		syntax("Key not recognized", p);
		p--;
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
		while (isspace(*p))
			p++;
		if (*p == '\0')
			break;
		p = clef_skip(p, &clef_name, &clef_middle);
		if (*p == '\0')
			break;
		switch (*p) {
		case 'a':
		case 'A':
			if (strncasecmp(p, "aeo", 3) == 0) {
				sf -= 3;
				s->u.key.minor = 1;
			} else	goto unk;
			break;
		case 'd':
		case 'D':
			if (strncasecmp(p, "dor", 3) == 0)
				sf -= 2;
			else	goto unk;
			break;
		case 'i':
		case 'I':
			if (strncasecmp(p, "ion", 3) == 0)
				break;
			goto unk;
		case 'l':
		case 'L':
			if (strncasecmp(p, "loc", 3) == 0)
				sf -= 5;
			else if (strncasecmp(p, "lyd", 3) == 0)
				sf += 1;
			else	goto unk;
			break;
		case 'm':
		case 'M':
			if (strncasecmp(p, "maj", 3) == 0)
				;
			else if (strncasecmp(p, "mix", 3) == 0)
				sf -= 1;
			else if (strncasecmp(p, "min", 3) == 0
				 || !isalpha(p[1])) {		/* 'm' alone */
				sf -= 3;
				s->u.key.minor = 1;
			} else	goto unk;
			break;
		case 'o':
		case 'O':
			if (strncasecmp(p, "octave", 6) == 0) {	/* (abcMIDI) */
				p += 6;
				while (!isspace(*p) && *p != '\0')
					p++;
				continue;
			}
			goto unk;
		case 'p':
		case 'P':
			if (strncasecmp(p, "phr", 3) == 0)
				sf -= 4;
			else	goto unk;
			break;
		case '^':
		case '_':
		case '=':
			p = parse_acc(p, s);	/* explicit accidentals */
			continue;
		case '+':
		case '-':
			if (p[1] == '8') {
				/* "+8" / "-8" (fixme: not standard) */
				if (*p == '+')
					curvoice->add_pitch += 7;
				else	curvoice->add_pitch -= 7;
				p += 2;
				continue;
			}
			goto unk;
		default:
		unk:
			syntax("Unknown token in key specifier", p);
			while (!isspace(*p) && *p != '\0')
				p++;
			continue;
		}
		while (isalpha(*p))
			p++;
	}

	if (sf > 7 || sf < -7) {
		syntax("Too many sharps/flats", p);
		if (sf > 0)
			sf -= 12;
		else	sf += 12;
	}
	s->u.key.sf = sf;

	if (clef_name != 0 || clef_middle != 0) {
		s = abc_new(s->tune, 0, 0);
		s->type = ABC_T_CLEF;
		parse_clef(&s->u.clef, clef_name, clef_middle);
	}
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

	return p;
}

/* -- parse a 'M:' -- */
static char *parse_meter(unsigned char *p,
			 struct abcsym *s)
{
	int m1, m2, d, wmeasure;
	int nm, i;
	int in_parenth;

	if (*p == '\0')
		return "Empty meter string";
	nm = 0;
	in_parenth = 0;
	wmeasure = 0;
	if (*p == 'N' || *p == 'n')
		;				/* no meter */
	else if (*p == 'C') {
		s->u.meter.meter[0].top[0] = *p++;
		if (*p == '|')
			s->u.meter.meter[0].top[1] = *p++;
		wmeasure = 4 * BASE_LEN / 4;
		nm = 1;
	} else while (*p != '\0') {
		if (*p == '(' || *p == ')') {
			if (*p == '(')
				in_parenth = 1;
			else 	in_parenth = 0;
			s->u.meter.meter[nm].top[0] = *p++;
			nm++;
			continue;
		}
		if (sscanf(p, "%d", &m1) != 1
		    || m1 <= 0)
			return "Cannot identify meter top";
		i = 0;
		m2 = 2;			/* default when no bottom value */
		for (;;) {
			while (isdigit(*p)
			       && i < sizeof s->u.meter.meter[0].top)
				s->u.meter.meter[nm].top[i++] = *p++;
			if (*p == '/') {
				p++;
				if (sscanf(p, "%d", &m2) != 1
				    || m2 <= 0)
					return "Cannot identify meter bottom";
				i = 0;
				while (isdigit(*p)
				       && i < sizeof s->u.meter.meter[0].bot)
					s->u.meter.meter[nm].bot[i++] = *p++;
				break;
			}
			if (*p != ' ' && *p != '+')
				break;
			s->u.meter.meter[nm].top[i++] = *p++;
			if (sscanf(p, "%d", &d) != 1
			    || d <= 0)
				return "Cannot identify meter top";
			if (p[-1] == ' ') {
				if (d > m1)
					m1 = d;
			} else	m1 += d;
		}
		if (!in_parenth)
			wmeasure += m1 * BASE_LEN / m2;
		nm++;
		if (*p == ' ' || *p == '+') {
			s->u.meter.meter[nm].top[0] = *p++;
			nm++;
		}
	}

	s->u.meter.wmeasure = wmeasure;
	s->u.meter.nmeter = nm;

	/* if in the header, change the unit note length */
	if (abc_state == ABC_S_HEAD && ulen == 0) {
		if (wmeasure >= BASE_LEN * 3 / 4
		    || wmeasure == 0)
			ulen = BASE_LEN / 8;
		else	ulen = BASE_LEN / 16;
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
	if (flags != 0)
		goto err;

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

/* -- parse a tempo (Q:) -- */
static char *parse_tempo(unsigned char *p,
			 struct abcsym *s)
{
	int have_error = 0;
	unsigned char *q;
	int l;

	/* string before */
	if (*p == '"') {
		q = ++p;
		while (*p != '"' && *p != '\0')
			p++;
		l = p - q;
		s->u.tempo.str1 = alloc_f(l + 1);
		strncpy(s->u.tempo.str1, q, l);
		s->u.tempo.str1[l] = '\0';
		if (*p == '"')
			p++;
		while (isspace(*p))
			p++;
	}

	/* beat */
	if (*p == 'C' || *p == 'c'
	    || *p == 'L' || *p == 'l') {
		int len;

		p = parse_len(p + 1, &len);
		if (len <= 0)
			have_error++;
		else	s->u.tempo.length[0] = len;
		while (isspace(*p))
			p++;
	} else if (isdigit(*p) && strchr(p, '/') != 0) {
		int i;

		i = 0;
		while (isdigit(*p)) {
			int top, bot, n;

			if (sscanf(p, "%d /%d%n", &top, &bot, &n) != 2
			    || bot <= 0) {
				have_error++;
				break;
			}
			l = (BASE_LEN * top) / bot;
			if (l <= 0
			    || i >= sizeof s->u.tempo.length
					/ sizeof s->u.tempo.length[0])
				have_error++;
			else	s->u.tempo.length[i++] = l;
			p += n;
			while (isspace(*p))
				p++;
		}
	}

	/* tempo value ('Q:beat=value' or 'Q:value') */
	if (*p == '=')
		p++;
	if (isdigit(*p)) {
		int value;

		value = atoi(p);
		if (value < 0)
			have_error++;
		else	s->u.tempo.value = value;
		while (isdigit(*p) || isspace(*p))
			p++;
	}

	/* string after */
	if (*p == '"') {
		q = ++p;
		while (*p != '"' && *p != '\0')
			p++;
		l = p - q;
		s->u.tempo.str2 = alloc_f(l + 1);
		strncpy(s->u.tempo.str2, q, l);
		s->u.tempo.str2[l] = '\0';
	}

	return have_error ? "Invalid tempo" : 0;
}

/* -- get a user defined accent (U:) -- */
static char *get_user(unsigned char *p,
		      struct abcsym *s)
{
	if (char_tb[*p] != CHAR_DECO)	/* accept any character */
/*fixme: should be for the current tune only */
		char_tb[*p] = CHAR_DECO;
	s->u.user.symbol = *p++;

	/* '=' and '!' are not important */
	while (isspace(*p)
	       || *p == '=' || *p == '!')
		p++;
	get_deco(p, &s->u.user.value);
	return 0;
}

/* -- parse the voice parameters (V:) -- */
static char *parse_voice(unsigned char *p,
			 struct abcsym *s)
{
	int voice;
	char *error_txt = 0;
	char name[VOICE_NAME_SZ];
	unsigned char *clef_name, *clef_middle;
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
	{"merge", 5, 2},
	{"up", 2, 3},
	{"down", 4, 4},
	{0, 0, 0}
};
	struct kw_s *kw;

	/* save the unit note length of the previous voice */
	curvoice->ulen = ulen;

	if (voice_tb[0].name[0] == '\0') {
		switch (s->prev->type) {
		case ABC_T_EOLN:
		case ABC_T_NOTE:
		case ABC_T_REST:
		case ABC_T_BAR:
			/* the previous voice was implicit (after K:) */
			voice_tb[0].name[0] = '1';
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
	if (abc_state == ABC_S_TUNE || abc_state == ABC_S_EMBED)
		ulen = curvoice->ulen;

	/* parse the other parameters */
	clef_name = 0;
	clef_middle = 0;
	for (;;) {
		while (isspace(*p))
			p++;
		if (*p == '\0')
			break;
		p = clef_skip(p, &clef_name, &clef_middle);
		if (*p == '\0')
			break;
		for (kw = kw_tb; kw->name; kw++) {
			if (strncmp(p, kw->name, kw->len) == 0)
				break;
		}
		if (!kw->name) {
			while (!isspace(*p) && *p != '\0')
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
			s->u.voice.stem = 1;
			break;
		case 4:			/* down */
			s->u.voice.stem = -1;
			break;
		}
	}
	if (clef_name != 0 || clef_middle != 0) {
		s = abc_new(s->tune, 0, 0);
		s->type = ABC_T_CLEF;
		parse_clef(&s->u.clef, clef_name, clef_middle);
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
				xch(s->u.note.lens[i], s->u.note.lens[i-1]);
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
		       unsigned char *p)
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
	if ((bar_type & 0x0f) == B_OBRA && bar_type != B_OBRA) {
		bar_type >>= 4;		/* have an other bar for '[' */
		p--;
	}
	if (bar_type == (B_OBRA << 8) + (B_BAR << 4) + B_CBRA)	/* [|] */
		bar_type = (B_OBRA << 4) + B_CBRA;		/* [] */

/*	curvoice->last_note = 0; */
	if (vover_bar) {
		curvoice = &voice_tb[curvoice->mvoice];
		vover_bar = 0;
	}
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
	if (!isdigit(*p)		/* if not a repeat bar */
	    && (*p != '"' || p[-1] != '[')) {	/* ('["' only) */
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
		s->u.bar.type = 0;
		s->u.bar.len = n;
		return p;
	}

	if (*p == '"')
		p = get_str(repeat_value, p, sizeof repeat_value);
	else {
		char *q;

		q = repeat_value;
		while (isdigit(*p)
		       || *p == ','
		       || *p == '-'
		       || (*p == '.' && isdigit(p[1]))) {
			if (q < &repeat_value[sizeof repeat_value - 1])
				*q++ = *p++;
			else	p++;
		}
		*q = '\0';
	}
	if (repeat_value[0] != '1' || repeat_value[1] != '\0')
		curvoice->tie = 0;
	if (bar_type != B_OBRA
	    || s->text != 0) {
		s = abc_new(t, repeat_value, 0);
		s->type = ABC_T_BAR;
		s->state = ABC_S_TUNE;
		s->u.bar.type = B_OBRA;
	} else {
		s->text = alloc_f(strlen(repeat_value) + 1);
		strcpy(s->text, repeat_value);
	}
	s->u.bar.repeat_bar = 1;
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
		p++;
		if (*p == '^') {
			acc = A_DS;
			p++;
		} else	acc = A_SH;
		break;
	case '=':
		p++;
		acc = A_NT;
		break;
	case '_':
		p++;
		if (*p == '_') {
			acc = A_DF;
			p++;
		} else	acc = A_FT;
		break;
	}
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
	*stemless = nostem;

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
	return 0;
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
	struct abcsym *last_note_sav = 0;
	unsigned char *q, c;
	int i;
	char sappo = 0;
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
			comment = decomment_line(p + 2);
			s = abc_new(t, p, comment);
			s->type = ABC_T_PSCOM;
			s->state = abc_state;
			p += 2;				/* skip '%%' */
			if (strncasecmp(p, "fmt ", 4) == 0)
				p += 4;			/* skip 'fmt' */
			if (strncmp(p, "begintext", 9) == 0) {
				for (;;) {
					if ((p = get_line()) == 0) {
						syntax("EOF while parsing %%begintext pseudo-comment",
						       scratch_line);
						return 0;
					}
					s = abc_new(t, p, 0);
					s->type = ABC_T_PSCOM;
					s->state = abc_state;
					if (*p != '%' || p[1] != '%')
						continue;
					p += 2;
					if (strncasecmp(p, "fmt ", 4) == 0)
						p += 4;
					if (strncmp(p, "endtext", 7) == 0)
						return 1;
				}
				/* not reached */
			}
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
		    || abc_state != ABC_S_TUNE)
			return 1;
		c = p[strlen(p) - 1];
		if (c != '|' && c != ']')
			return 1;
		while (!isspace(*p) && *p != '\0')
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

	if (scratch_line[0] == ' ' && curvoice->last_note != 0)
		curvoice->last_note->u.note.word_end = 1;

	lyric_started = 0;
	lyric_start = lyric_cont = 0;
	while (*p != '\0') {
		switch (char_tb[*p++]) {
		case CHAR_GCHORD: {			/* " */
			int l;
			int more_gch;

		gch_continue:
			more_gch = 0;
			q = p;
			while (*p != '"') {
				if (*p == '\0') {
					more_gch = 1;
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
			else if (more_gch) {
				if ((p = get_line()) == 0) {
					syntax("EOF reached while parsing guitar chord",
					       q);
					return 0;
				}
				goto gch_continue;
			}
		}
			break;
		case CHAR_GRACE:		/* '{' or '}' */
			if (p[-1] == '{') {
				if (*p == '/') {
					sappo = 1;
					p++;
				}
				char_tb['{'] = CHAR_BAD;
				char_tb['}'] = CHAR_GRACE;
				last_note_sav = curvoice->last_note;
				curvoice->last_note = 0;
			} else {
				char_tb['{'] = CHAR_GRACE;
				char_tb['}'] = CHAR_BAD;
/*fixme:bad*/
				t->last_sym->u.note.word_end = 1;
				curvoice->last_note = last_note_sav;
			}
			break;
		case CHAR_DECO:
			if (p[-1] == '!' && check_nl(p)) {
				s = abc_new(t, 0, 0);	/* abc2win EOL */
				s->type = ABC_T_EOLN;
				s->state = abc_state;
				break;
			}
			if (p[-1] == '.' && *p == '|') {
				p = parse_bar(t, p + 1);
/*fixme: should have other dashed bars, as '.||' */
				t->last_sym->u.bar.type = B_COL;
				break;
			}
			p = parse_deco(p - 1, &dc);
			break;
		case CHAR_ACC:
		case CHAR_NOTE:
		case CHAR_REST:
			p = parse_note(t, p - 1);
			if (sappo) {
				t->last_sym->u.note.sappo = 1;
				sappo = 0;
			}
			curvoice->last_note = t->last_sym;
			break;
		case CHAR_BSLASH:		/* '\\' */
/*fixme: KO if in grace note sequence*/
			if (*p == '\0')
				return 1;
			syntax("'\\' ignored", p - 1);
			break;
		case CHAR_OBRA:			/* '[' */
			if (*p == '|' || *p == ']'
			    || isdigit(*p) || *p == '"') {
				p = parse_bar(t, p);
				break;
			}
			if (p[1] != ':') {
				p = parse_note(t, p - 1);	/* chord */
				if (sappo) {
					t->last_sym->u.note.sappo = 1;
					sappo = 0;
				}
				curvoice->last_note = t->last_sym;
				break;
			}

			/* embedded header */
			c = ']';
			q = p;
			while (*p != '\0' && *p != c)
				p++;
			if (*p == '\0') {
				syntax("Escape sequence [..] not closed",
				       q);
				c = '\0';
			} else	*p = '\0';
			abc_state = ABC_S_EMBED;
			parse_header(t, q, 0);
			abc_state = ABC_S_TUNE;
			*p++ = c;
			break;
		case CHAR_BAR:			/* '|', ':' or ']' */
			p = parse_bar(t, p);
			break;
		case CHAR_OPAR:			/* '(' */
			if (isdigit(*p)) {
				curvoice->pplet = *p - '0';
				if (curvoice->pplet <= 1) {
					syntax("Invalid 'p' in tuplet", p);
					curvoice->pplet = 0;
				}
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
							if (curvoice->pplet != 0)
								curvoice->rplet = *p - '0';
							p++;
						}
					}
				}
				if (curvoice->qplet == 0)
					curvoice->qplet = meter % 3 == 0
						? 3
						: 2;
			} else if (*p == '&') {
				s = abc_new(t, 0, 0);
				s->type = ABC_T_V_OVER;
				p++;
				if (*p == '&') {
					s->u.v_over.type = V_OVER_SD;
					p++;
				} else	s->u.v_over.type = V_OVER_SS;
				s->u.v_over.voice = curvoice - voice_tb;
				char_tb[')'] = CHAR_VOVE;
			} else	curvoice->slur++;
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
			if (*p != ')') {
				s = abc_new(t, 0, 0);
				s->type = ABC_T_V_OVER;
				if (*p == '&') {
					s->u.v_over.type = V_OVER_D;
					p++;
				} /*else s->u.v_over.type = V_OVER_S; */
				vover_new();
				s->u.v_over.voice = curvoice - voice_tb;
				if (char_tb[')'] != CHAR_VOVE)
					vover_bar = 1;
				break;
			}
			if (char_tb[')'] != CHAR_VOVE) {
				syntax("Bad end of voice overlay", p - 1);
				break;
			}
			p++;
			/* fall thru */
		case CHAR_VOVE:			/* ')' after '(&' */
			s = abc_new(t, 0, 0);
			s->type = ABC_T_V_OVER;
			s->u.v_over.type = V_OVER_E;
			s->u.v_over.voice = curvoice->mvoice;
			char_tb[')'] = CHAR_CPAR;
			if (curvoice->last_note != 0) {
				curvoice->last_note->u.note.word_end = 1;
				curvoice->last_note = 0;
			}
			curvoice = &voice_tb[curvoice->mvoice];
			break;
		case CHAR_SPAC:			/* ' ' and '\t' */
			if (curvoice->last_note != 0)
				curvoice->last_note->u.note.word_end = 1;
			break;
		case CHAR_MINUS:		/* '-' */
			if ((curvoice->tie = curvoice->last_note) == 0
			    || curvoice->tie->type != ABC_T_NOTE)
				goto bad_char;
			for (i = 0; i <= curvoice->tie->u.note.nhd; i++)
				curvoice->tie->u.note.ti1[i] = 1;
			break;
		case CHAR_BRHY:			/* '>' and '<' */
			if (curvoice->last_note == 0)
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
			syntax("Bad character", p - 1);
			break;
		}
	}

/*fixme: may we have grace notes across lines?*/
	if (char_tb['{'] == CHAR_BAD) {
		syntax("No end of grace note sequence", 0);
		char_tb['{'] = CHAR_GRACE;
		char_tb['}'] = CHAR_BAD;
		if (curvoice->last_note != 0)
			curvoice->last_note->u.note.word_end = 1;
		curvoice->last_note = last_note_sav;
	}

	/* add eoln */
	s = abc_new(t, 0, 0);
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
	int ntie;
	signed char tie_pit[MAXHD];

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

	if (char_tb['{'] == CHAR_BAD)	/* in a grace note sequence */
		s->u.note.grace = 1;
	else if (curvoice->rplet) {	/* start of n-plet */
		s->u.note.p_plet = curvoice->pplet;
		s->u.note.q_plet = curvoice->qplet;
		s->u.note.r_plet = curvoice->rplet;
		curvoice->rplet = 0;
	}

	/* rest */
	switch (*p) {
	case 'Z':			/* multi-rest */
		s->type = ABC_T_MREST;
		p++;
		len = 1;
		if (isdigit(*p)) {
			len = strtol(p, 0, 10);
			while (isdigit(*p))
				p++;
		}
		s->u.bar.type = 0;
		s->u.bar.len = len;
		return p;
	case 'y':			/* space (BarFly) */
		s->type = ABC_T_REST;
		s->u.note.invis = 1;
		s->u.note.slur_st += curvoice->slur;
		curvoice->slur = 0;
		return p + 1;
	case 'x':			/* invisible rest */
		s->u.note.invis = 1;
		/* fall thru */
	case 'z':
		s->type = ABC_T_REST;
		p = parse_len(p + 1, &len);
		s->u.note.len = s->u.note.lens[0] = len * ulen / BASE_LEN;
		if (curvoice->last_note != 0
		    && curvoice->last_note->u.note.brhythm != 0)
			broken_rhythm(&s->u.note,
				      -curvoice->last_note->u.note.brhythm);
		return p;
	}

	if (!s->u.note.grace && !lyric_started) {
		lyric_started = 1;
		s->u.note.lyric_start = 1;
		lyric_start = s;
	}

	chord = 0;
	q = p;
	if (*p == '[') {	/* accept only '[..]' for chord */
		chord = 1;
		p++;
	}

	/* prepare searching the end of ties */
	ntie = 0;
	if (curvoice->tie != 0) {
		for (m = 0; m <= curvoice->tie->u.note.nhd; m++) {
			if (curvoice->tie->u.note.ti1[m])
				tie_pit[ntie++] = curvoice->tie->u.note.pits[m];
		}
		curvoice->tie = 0;
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
			if (s->u.note.grace) {
				len = len * BASE_LEN / 4 / ulen;
				tmp = 0;
			}
			s->u.note.pits[m] = pit;
			s->u.note.lens[m] = len;
			s->u.note.accs[m] = acc;
			nostem |= tmp;

			for (j = 0; j < ntie; j++) {
				if (tie_pit[j] == pit) {
					s->u.note.ti2[m] = 1;
					tie_pit[j] = -128;
					break;
				}
			}

			if (chord) {
				if (*p == '-') {
					s->u.note.ti1[m] = 1;
					curvoice->tie = s;
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
			syntax("Chord not closed", q);
			return p;
		}
	}
	s->u.note.stemless = nostem;

	/* warn about the bad ties */
	if (char_tb['{'] != CHAR_BAD) {	/* if not in a grace note sequence */
		for (j = 0; j < ntie; j++) {
			if (tie_pit[j] != -128)
				syntax("Bad tie", p);
		}
	}

	if (m == 0) {			/* if no note */
		if ((t->last_sym = s->prev) == 0)
			t->first_sym = 0;
		else	s->prev->next = 0;
		return p;
	}
	s->u.note.nhd = m - 1;

	/* the chord length is the length of the first note */
	s->u.note.len = s->u.note.lens[0];

	note_sort(s);			/* sort the notes in chord */
	if (curvoice->last_note != 0
	    && curvoice->last_note->u.note.brhythm != 0)
		broken_rhythm(&s->u.note,
			      -curvoice->last_note->u.note.brhythm);
	s->u.note.slur_st += curvoice->slur;
	curvoice->slur = 0;

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
			int i;

			abc_state = ABC_S_TUNE;
			if (ulen == 0)
				ulen = BASE_LEN / 8;
			for (i = MAXVOICE; --i >= 0; )
				voice_tb[i].ulen = ulen;
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
	case 'T':
		if (abc_state != ABC_S_GLOBAL)
			break;
		/* 'T:' may start a new tune without 'X:' */
		print_warning("T: without X:");
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
	int n, len, m1, m2, pp;
	int maxcol = 73;

	severity = 1;
	n = q - scratch_line;
	if ((unsigned) n >= line_length)
		n = -1;
	print_error(msg, n);
	if (n < 0) {
		if (q != 0)
			fprintf(stderr, " (near '%s')\n", q);
		return;
	}
/*	printf("\n++++ %s in line %d.%d\n", msg, linenum, n + 1);	*/
	m1 = 0;
	m2 = len = line_length - 1;
	if (m2 > maxcol) {
		if (n < maxcol)
			m2 = maxcol;
		else {
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
	fprintf(stderr, "%*s", m2 - m1, &scratch_line[m1]);
	if (m2 < len)
		fprintf(stderr, "...");
	fprintf(stderr, "\n");

	if ((unsigned) n < 200)
		fprintf(stderr, "%*s\n", n + pp - m1, "^");
}

/* -- switch to a new voice overlay -- */
static void vover_new(void)
{
	int voice, mvoice;

	mvoice = curvoice - voice_tb;
	for (voice = mvoice + 1; voice <= nvoice; voice++)
		if (voice_tb[voice].mvoice == mvoice)
			break;
	if (voice > nvoice) {
		if (nvoice >= MAXVOICE) {
			syntax("Too many voices", 0);
			return;
		}
		nvoice = voice;
		voice_tb[voice].name[0] = '&';
		voice_tb[voice].mvoice = mvoice;
	}
	voice_tb[voice].ulen = curvoice->ulen;
	voice_tb[voice].add_pitch = curvoice->add_pitch;
	curvoice = &voice_tb[voice];
}
