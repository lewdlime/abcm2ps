/*
 * Parsing functions.
 *
 * This file is part of abcm2ps.
 *
 * Copyright (C) 1998-2013 Jean-François Moine
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
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include "abc2ps.h"
#include "slre.h"

/* options = external formatting */
struct symsel_s {			/* symbol selection */
	short bar;
	short time;
	char seq;
};
struct brk_s {				/* music line break */
	struct brk_s *next;
	struct symsel_s symsel;
};
struct voice_opt_s {			/* voice options */
	struct voice_opt_s *next;
	struct SYMBOL *s;		/* list of options (%%xxx) */
};
struct tune_opt_s {			/* tune options */
	struct tune_opt_s *next;
	struct voice_opt_s *voice_opts;
	struct SYMBOL *s;		/* list of options (%%xxx) */
};

struct STAFF_S staff_tb[MAXSTAFF];	/* staff table */
int nstaff;				/* (0..MAXSTAFF-1) */
struct SYMBOL *tsfirst;			/* first symbol in the time sorted list */

struct VOICE_S voice_tb[MAXVOICE];	/* voice table */
static struct VOICE_S *curvoice;	/* current voice while parsing */
struct VOICE_S *first_voice;		/* first voice */
struct SYSTEM *cursys;			/* current system */
static struct SYSTEM *parsys;		/* current system while parsing */

struct FORMAT dfmt;			/* current global format */
unsigned short *micro_tb;		/* ptr to the microtone table of the tune */
int nbar;				/* current measure number */

static struct voice_opt_s *voice_opts;
static struct tune_opt_s *tune_opts, *cur_tune_opts;
static struct brk_s *brks;
static struct symsel_s clip_start, clip_end;


static INFO info_glob;			/* global info definitions */

static int over_time;			/* voice overlay start time */
static int over_mxtime;			/* voice overlay max time */
static short over_bar;			/* voice overlay in a measure */
static short over_voice;		/* main voice in voice overlay */
static int staves_found;		/* time of the last %%staves */
static int abc2win;

float multicol_start;			/* (for multicol) */
static float multicol_max;
static float lmarg, rmarg;
static int mc_in_tune;			/* multicol started in tune */

static void get_clef(struct SYMBOL *s);
static struct abcsym *get_info(struct abcsym *as,
		     struct abctune *t);
static void get_key(struct SYMBOL *s);
static void get_meter(struct SYMBOL *s);
static void get_voice(struct SYMBOL *s);
static void get_note(struct SYMBOL *s);
static struct abcsym *process_pscomment(struct abcsym *as);
static void set_tblt(struct VOICE_S *p_voice);
static void set_tuplet(struct SYMBOL *s);
static void sym_link(struct SYMBOL *s, int type);

/* -- weight of the symbols -- */
static signed char w_tb[NSYMTYPES] = {	/* !! index = symbol type !! */
	0,
	9,	/* 1- note / rest */
	1,	/* 2- space */
	3,	/* 3- bar */
	2,	/* 4- clef */
	5,	/* 5- timesig */
	4,	/* 6- keysig */
	0,	/* 7- tempo */
	0,	/* 8- staves */
	9,	/* 9- mrest */
	0,	/* 10- part */
//	8,	/* 11- grace */
	1,	/* 11- grace */
	0,	/* 12- fmtchg */
	7,	/* 13- tuplet */
	6,	/* 14- stbrk */
	6	/* 15- custos */
};

/* key signature transposition tables */
static signed char cde2fcg[7] = {0, 2, 4, -1, 1, 3, 5};
static char cgd2cde[7] = {0, 4, 1, 5, 2, 6, 3};

/* -- expand a multi-rest into single rests and measure bars -- */
static void mrest_expand(struct SYMBOL *s)
{
	struct VOICE_S *p_voice;
	struct SYMBOL *s2, *next;
	struct deco dc;
	int nb, dt;

	nb = s->as.u.bar.len;
	dt = s->dur / nb;

	/* change the multi-rest (type bar) to a single rest */
	memcpy(&dc, &s->as.u.bar.dc, sizeof dc);
	memset(&s->as.u.note, 0, sizeof s->as.u.note);
	s->type = NOTEREST;
	s->as.type = ABC_T_REST;
	s->as.u.note.nhd = 0;
	s->dur = s->as.u.note.lens[0] = dt;
	s->head = H_FULL;
	s->nflags = -2;

	/* add the bar(s) and rest(s) */
	next = s->next;
	p_voice = &voice_tb[s->voice];
	p_voice->last_sym = s;
	p_voice->time = s->time + dt;
	p_voice->cstaff = s->staff;
	s2 = s;
	while (--nb > 0) {
		s2 = sym_add(p_voice, BAR);
		s2->as.type = ABC_T_BAR;
		s2->as.u.bar.type = B_SINGLE;
		s2->as.linenum = s->as.linenum;
		s2->as.colnum = s->as.colnum;
		s2 = sym_add(p_voice, NOTEREST);
		s2->as.type = ABC_T_REST;
		s2->as.flags = s->as.flags;
		s2->as.linenum = s->as.linenum;
		s2->as.colnum = s->as.colnum;
		s2->dur = s2->as.u.note.lens[0] = dt;
		s2->head = H_FULL;
		s2->nflags = -2;
		p_voice->time += dt;
	}
	s2->next = next;
	if (next)
		next->prev = s2;

	/* copy the mrest decorations to the last rest */
	memcpy(&s2->as.u.note.dc, &dc, sizeof s2->as.u.note.dc);
}

/* -- sort all symbols by time and vertical sequence -- */
static void sort_all(void)
{
	struct SYSTEM *sy;
	struct SYMBOL *s, *prev, *s2;
	struct VOICE_S *p_voice;
	int fl, voice, time, w, wmin, multi, mrest_time;
	int nv, nb, r, sysadv;
	struct SYMBOL *vtb[MAXVOICE];
	signed char vn[MAXVOICE];	/* voice indexed by range */

/*	memset(vtb, 0, sizeof vtb); */
	mrest_time = -1;
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next)
		vtb[p_voice - voice_tb] = s = p_voice->sym;

	/* initialize the voice order */
	sy = cursys;
	sysadv = 1;
	prev = NULL;
	fl = 1;				/* set start of sequence */
	multi = -1;			/* (have gcc happy) */
	for (;;) {
		if (sysadv) {
			sysadv = 0;
			memset(vn, -1, sizeof vn);
			multi = -1;
			for (p_voice = first_voice;
			     p_voice;
			     p_voice = p_voice->next) {
				voice = p_voice - voice_tb;
				r = sy->voice[voice].range;
				if (r < 0)
					continue;
				vn[r] = voice;
				multi++;
			}
		}

		/* search the min time and symbol weight */
		wmin = time = (unsigned) ~0 >> 1;	/* max int */
		nv = nb = 0;
		for (r = 0; r < MAXVOICE; r++) {
			voice = vn[r];
			if (voice < 0)
				break;
			s = vtb[voice];
			if (!s || s->time > time)
				continue;
			w = w_tb[s->type];
			if (s->time < time) {
				time = s->time;
				wmin = w;
				nb = 0;
			} else if (w < wmin) {
				wmin = w;
				nb = 0;
			}
			if (!(s->sflags & S_SECOND)) {
				nv++;
				if (s->type == BAR)
					nb++;
			}
			if (s->type == MREST) {
				if (s->as.u.bar.len == 1)
					mrest_expand(s);
				else if (multi)
					mrest_time = time;
			}
		}
		if (wmin > 127)
			break;			/* done */

#if 0
		/* align the measure bars */
		if (nb != 0 && nb != nv) {	/* if other symbol than bars */
			wmin = (unsigned) ~0 >> 1;
			for (r = 0; r < MAXVOICE; r++) {
				voice = vn[r];
				if (voice < 0)
					break;
				s = vtb[voice];
				if (!s || s->time > time
				 || s->type == BAR)
					continue;
				w = w_tb[s->type];
				if (w < wmin)
					wmin = w;
			}
			if (wmin > 127)
				wmin = w_tb[BAR];
		}
#endif

		/* if some multi-rest and many voices, expand */
		if (time == mrest_time) {
			nb = 0;
			for (r = 0; r < MAXVOICE; r++) {
				voice = vn[r];
				if (voice < 0)
					break;
				s = vtb[voice];
				if (!s || s->time != time)
					continue;
				w = w_tb[s->type];
				if (w != wmin)
					continue;
				if (s->type != MREST) {
					mrest_time = -1;	/* some note or rest */
					break;
				}
				if (nb == 0) {
					nb = s->as.u.bar.len;
				} else if (nb != s->as.u.bar.len) {
					mrest_time = -1;	/* different duration */
					break;
				}
			}
			if (mrest_time < 0) {
				for (r = 0; r < MAXVOICE; r++) {
					voice = vn[r];
					if (voice < 0)
						break;
					s = vtb[voice];
					if (s && s->type == MREST)
						mrest_expand(s);
				}
			}
		}

		/* link the vertical sequence */
		for (r = 0; r < MAXVOICE; r++) {
			voice = vn[r];
			if (voice < 0)
				break;
			s = vtb[voice];
			if (!s || s->time != time)
				continue;
			w = w_tb[s->type];
			if (w != wmin)
				continue;
			if (fl) {
				fl = 0;
				s->sflags |= S_SEQST;
			}
			s->ts_prev = prev;
			if (prev) {
				prev->ts_next = s;
				if (s->type == BAR
				 && (s->sflags & S_SECOND)
				 && prev->type != BAR
				 && !(s->as.flags & ABC_F_INVIS))
					error(1, s, "Bad measure bar");
			} else {
				tsfirst = s;
			}
			prev = s;
			vtb[voice] = s->next;
			if (s->type == STAVES) {
				sy = sy->next;
				sysadv = 1;
			}
		}
		fl = wmin;	/* start a new sequence if some space */
	}

	if (!prev)
		return;

	/* if no bar or format_change at end of tune, add a dummy symbol */
	if (prev->type != BAR && prev->type != FMTCHG) {
		p_voice = &voice_tb[prev->voice];
		p_voice->last_sym = prev;
		s = sym_add(p_voice, FMTCHG);
		s->u = -1;
		s->time = prev->time + prev->dur;
		s->sflags = S_SEQST;
		prev->ts_next = s;
		s->ts_prev = prev;
		for (;;) {
			prev->sflags &= ~S_EOLN;
			if (prev->sflags & S_SEQST)
				break;
			prev = prev->ts_prev;
		}
	}

	/* if Q: from tune header, put it at start of the music */
	s2 = info['Q' - 'A'];
	if (!s2)
		return;
	info['Q' - 'A'] = NULL;
	s = tsfirst->extra;
	if (s) {
		do {
			if (s->type == TEMPO)
				return;		/* already a tempo */
			s = s->next;
		} while (s);
	}
	s = tsfirst;
	s2->type = TEMPO;
	s2->voice = s->voice;
	s2->staff = s->staff;
	s2->time = s->time;
	if (s->extra) {
		s2->next = s->extra;
		s2->next->prev = s2;
	}
	s->extra = s2;
}

/* -- move the symbols with no width to the next symbol -- */
static void voice_compress(void)
{
	struct VOICE_S *p_voice;
	struct SYMBOL *s, *s2, *ns;
//	int sflags;

	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		if (p_voice->ignore)
			continue;
		for (s = p_voice->sym; s; s = s->next) {
			if (s->time >= staves_found)
				break;
		}
		ns = NULL;
//		sflags = 0;
		for ( ; s; s = s->next) {
			switch (s->type) {
			case KEYSIG:	/* remove the empty key signatures */
				if (s->as.u.key.empty) {
					if (s->prev)
						s->prev->next = s->next;
					else
						p_voice->sym = s->next;
					if (s->next)
						s->next->prev = s->prev;
					continue;
				}
				break;
			case FMTCHG:
				s2 = s->extra;
				if (s2) {	/* dummy format */
					if (!ns)
						ns = s2;
					if (s->prev) {
						s->prev->next = s2;
						s2->prev = s->prev;
					}
					if (!s->next) {
						ns = NULL;
						break;
					}
					while (s2->next)
						s2 = s2->next;
					s->next->prev = s2;
					s2->next = s->next;
				}
				/* fall thru */
			case TEMPO:
			case PART:
			case TUPLET:
				if (!ns)
					ns = s;
//				sflags |= s->sflags;
				continue;
			case MREST:		/* don't shift P: and Q: */
				if (!ns)
					continue;
				s2 = (struct SYMBOL *) getarena(sizeof *s);
				memset(s2, 0, sizeof *s2);
				s2->type = SPACE;
				s2->as.u.note.lens[1] = -1;
				s2->as.flags = ABC_F_INVIS;
				s2->voice = s->voice;
				s2->staff = s->staff;
				s2->time = s->time;
				s2->sflags = s->sflags;
				s2->next = s;
				s2->prev = s->prev;
				s2->prev->next = s2;
				s->prev = s2;
				s = s2;
				break;
			}
			if (s->as.flags & ABC_F_GRACE) {
				if (!ns)
					ns = s;
				while (!(s->as.flags & ABC_F_GR_END))
					s = s->next;
				s2 = (struct SYMBOL *) getarena(sizeof *s);
				memcpy(s2, s, sizeof *s2);
				s2->as.type = 0;
				s2->type = GRACE;
				s2->dur = 0;
				s2->next = s->next;
				if (s2->next)
					s2->next->prev = s2;
				else
					p_voice->last_sym = s2;
				s2->prev = s;
				s->next = s2;
				s = s2;
			}
			if (!ns)
				continue;
			s->extra = ns;
//			s->sflags |= (sflags & S_EOLN);
//			sflags = 0;
			s->prev->next = NULL;
			s->prev = ns->prev;
			if (s->prev)
				s->prev->next = s;
			else
				p_voice->sym = s;
			ns->prev = NULL;
			ns = NULL;
		}

		/* when symbols with no space at end of tune,
		 * add a dummy format */
		if (ns) {
			s = sym_add(p_voice, FMTCHG);
			s->u = -1;		/* nothing */
			s->extra = ns;
			s->prev->next = NULL;	/* unlink */
			s->prev = ns->prev;
			if (s->prev)
				s->prev->next = s;
			else
				p_voice->sym = s;
			ns->prev = NULL;
		}
	}
}

/* -- duplicate the voices as required -- */
static void voice_dup(void)
{
	struct VOICE_S *p_voice, *p_voice2;
	struct SYMBOL *s, *s2, *g, *g2;
	int voice;

	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		if ((voice = p_voice->clone) < 0)
			continue;
		p_voice->clone = -1;
		p_voice2 = &voice_tb[voice];
		for (s = p_voice->sym; s; s = s->next) {
//fixme: there may be other symbols before the %%staves at this same time
			if (s->time >= staves_found)
				break;
		}
		for ( ; s; s = s->next) {
			s2 = (struct SYMBOL *) getarena(sizeof *s2);
			memcpy(s2, s, sizeof *s2);
			s2->prev = p_voice2->last_sym;
			s2->next = NULL;
			if (p_voice2->sym)
				p_voice2->last_sym->next = s2;
			else
				p_voice2->sym = s2;
			p_voice2->last_sym = s2;
			s2->voice = voice;
			s2->staff = p_voice2->staff;
			if (p_voice2->second)
				s2->sflags |= S_SECOND;
			else
				s2->sflags &= ~S_SECOND;
			if (p_voice2->floating)
				s2->sflags |= S_FLOATING;
			else
				s2->sflags &= ~S_FLOATING;
			s2->ly = NULL;
			g = s2->extra;
			if (!g)
				continue;
			g2 = (struct SYMBOL *) getarena(sizeof *g2);
			memcpy(g2, g, sizeof *g2);
			s2->extra = g2;
			s2 = g2;
			s2->voice = voice;
			s2->staff = p_voice2->staff;
			for (g = g->next; g; g = g->next) {
				g2 = (struct SYMBOL *) getarena(sizeof *g2);
				memcpy(g2, g, sizeof *g2);
				s2->next = g2;
				g2->prev = s2;
				s2 = g2;
				s2->voice = voice;
				s2->staff = p_voice2->staff;
			}
		}
	}
}

/* -- create a new staff system -- */
static void system_new(void)
{
	struct SYSTEM *new_sy;
	int staff, voice;

	new_sy = (struct SYSTEM *) getarena(sizeof *new_sy);
	if (!parsys) {
		memset(new_sy, 0, sizeof *new_sy);
		for (voice = 0; voice < MAXVOICE; voice++) {
			new_sy->voice[voice].range = -1;
			new_sy->voice[voice].clef.line = 2;
			new_sy->voice[voice].clef.stafflines = 5;
			new_sy->voice[voice].clef.staffscale = 1;
		}
		cursys = new_sy;
	} else {
		for (voice = 0; voice < MAXVOICE; voice++) {
			if (parsys->voice[voice].range < 0
			 || parsys->voice[voice].second)
				continue;
			staff = parsys->voice[voice].staff;
			memcpy(&parsys->staff[staff].clef,
				&parsys->voice[voice].clef,
				sizeof parsys->staff[staff].clef);
		}
		memcpy(new_sy, parsys, sizeof *new_sy);
		for (voice = 0; voice < MAXVOICE; voice++) {
			new_sy->voice[voice].range = -1;
			new_sy->voice[voice].second = 0;
		}
		for (staff = 0; staff < MAXSTAFF; staff++)
			new_sy->staff[staff].flags = 0;
		parsys->next = new_sy;
	}
	parsys = new_sy;
}

/* -- set the staves -- */
static void staves_init(void)
{
	struct SYSTEM *sy, *new_sy;
	struct SYMBOL *s, *staves;
	int staff, voice;

	sy = cursys;
	for (voice = 0; voice < MAXVOICE; voice++) {
		if (sy->voice[voice].range < 0
		 || sy->voice[voice].second)
			continue;
		staff = sy->voice[voice].staff;
		memcpy(&sy->staff[staff].clef,
			&sy->voice[voice].clef,
			sizeof (struct clef_s));
		sy->staff[staff].sep = sy->voice[voice].sep;
		sy->staff[staff].maxsep = sy->voice[voice].maxsep;
	}
	staves = NULL;
	for (s = tsfirst; s; s = s->ts_next) {
		switch (s->type) {
		case STAVES:
			sy = sy->next;
			for (voice = 0; voice < MAXVOICE; voice++) {
				if (sy->voice[voice].range < 0
				 || sy->voice[voice].second)
					continue;
				staff = sy->voice[voice].staff;
				memcpy(&sy->staff[staff].clef,
					&sy->voice[voice].clef,
					sizeof (struct clef_s));
				sy->staff[staff].sep = sy->voice[voice].sep;
				sy->staff[staff].maxsep = sy->voice[voice].maxsep;
			}
			staves = s;
			continue;
		case CLEF:
			if (s->as.u.clef.type < 0)
				break;
			{
				int scale, lines;

				scale = sy->voice[s->voice].clef.staffscale;
				lines = sy->voice[s->voice].clef.stafflines;
				sy->voice[s->voice].clef = s->as.u.clef;
				sy->voice[s->voice].clef.staffscale = scale;
				sy->voice[s->voice].clef.stafflines = lines;
			}
			continue;	/* normal clef change */
		case KEYSIG:
		case TIMESIG:
		case TEMPO:
		case PART:
		case FMTCHG:
			continue;
		default:
			staves = NULL;
			continue;
		}

		/* CLEF with change of #lines or scale of staff */
		voice = s->voice;
		if (!staves) {
			staves = s;	/* create a new staff system */
			new_sy = (struct SYSTEM *) getarena(sizeof *new_sy);
			memcpy(new_sy, sy, sizeof *new_sy);
			for (voice = 0; voice < MAXVOICE; voice++) {
				if (new_sy->voice[voice].range < 0
				 || new_sy->voice[voice].second)
					continue;
				staff = new_sy->voice[voice].staff;
				memcpy(&new_sy->staff[staff].clef,
					&new_sy->voice[voice].clef,
					sizeof (struct clef_s));
			}
			new_sy->next = sy->next;
			sy->next = new_sy;
			sy = new_sy;
			s->type = STAVES;	/* and set the marker */
		} else {		/* remove the CLEF */
			if (s->prev)
				s->prev->next = s->next;
			else
				voice_tb[voice].sym = s->next;
			if (s->ts_next) {
				s->ts_next->ts_prev = s->ts_prev;
				if (s->sflags & S_SEQST)
					s->ts_next->sflags |= S_SEQST;
			}
			if (s->ts_prev)
				s->ts_prev->ts_next = s->ts_next;
			else
				tsfirst = s->ts_next;
			if (s->next)
				s->next->prev = s->prev;
		}
		staff = sy->voice[voice].staff;
		if (s->as.u.clef.stafflines >= 0)
			sy->voice[voice].clef.stafflines
				= sy->staff[staff].clef.stafflines
				= s->as.u.clef.stafflines;
		if (s->as.u.clef.staffscale != 0)
			sy->voice[voice].clef.staffscale
				= sy->staff[staff].clef.staffscale
				= s->as.u.clef.staffscale;
	}
}

/* -- initialize the voices and staves -- */
/* this routine is called when starting the generation */
static void system_init(void)
{
	voice_compress();
	voice_dup();
	sort_all();			/* define the time / vertical sequences */
	if (!tsfirst)
		return;
	parsys->nstaff = nstaff;	/* save the number of staves */
	staves_init();
}

/* go to a global (measure + time) */
static struct SYMBOL *go_global_time(struct SYMBOL *s,
				struct symsel_s *symsel)
{
	struct SYMBOL *s2;
	int bar_time;

	if (symsel->bar <= 1) {		/* special case: there is no measure 0/1 */
//	 && nbar == -1) {		/* see set_bar_num */
		if (symsel->bar == 0)
			goto chk_time;
		for (s2 = s; s2; s2 = s2->ts_next) {
			if (s2->type == BAR
			 && s2->time != 0)
				break;
		}
		if (s2->time < voice_tb[cursys->top_voice].meter.wmeasure)
			s = s2;
		goto chk_time;
	}
	for ( ; s; s = s->ts_next) {
		if (s->type == BAR
		 && s->u >= symsel->bar)
			break;
	}
	if (!s)
		return NULL;
	if (symsel->seq != 0) {
		int seq;

		seq = symsel->seq;
		for (s = s->ts_next; s; s = s->ts_next) {
			if (s->type == BAR
			 && s->u == symsel->bar) {
				if (--seq == 0)
					break;
			}
		}
		if (!s)
			return 0;
	}

chk_time:
	if (symsel->time == 0)
		return s;
	bar_time = s->time + symsel->time;
	while (s->time < bar_time) {
		s = s->ts_next;
		if (!s)
			return s;
	}
	do {
		s = s->ts_prev;		/* go back to the previous sequence */
	} while (!(s->sflags & S_SEQST));
	return s;
}

/* treat %%clip */
static void do_clip(void)
{
	struct SYMBOL *s, *s2;
	struct SYSTEM *sy;
	struct VOICE_S *p_voice;
	int voice;

	/* remove the beginning of the tune */
	s = tsfirst;
	if (clip_start.bar > 0
	 || clip_start.time > 0) {
		s = go_global_time(s, &clip_start);
		if (!s) {
			tsfirst = NULL;
			return;
		}

		/* update the start of voices */
		sy = cursys;
		for (s2 = tsfirst; s2 != s; s2 = s2->ts_next) {
			switch (s2->type) {
			case STAVES:
				sy = sy->next;
				break;
			case CLEF:
				memcpy(&voice_tb[s2->voice].clef, &s2->as.u.clef,
					sizeof voice_tb[0].clef);
				break;
			case KEYSIG:
				memcpy(&voice_tb[s2->voice].key, &s2->as.u.key,
					sizeof voice_tb[0].key);
				break;
			case TIMESIG:
				memcpy(&voice_tb[s2->voice].meter, &s2->as.u.meter,
					sizeof voice_tb[0].meter);
				break;
			}
		}
		cursys = sy;
		for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
			voice = p_voice - voice_tb;
			for (s2 = s; s2; s2 = s2->ts_next) {
				if (s2->voice == voice) {
					s2->prev = NULL;
					break;
				}
			}
			p_voice->sym = s2;
		}
		tsfirst = s;
		s->ts_prev = NULL;
	}

	/* remove the end of the tune */
	s = go_global_time(s, &clip_end);
	if (!s)
		return;

	/* keep the current sequence */
	do {
		s = s->ts_next;
		if (!s)
			return;
	} while (!(s->sflags & S_SEQST));

	/* cut the voices */
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		voice = p_voice - voice_tb;
		for (s2 = s->ts_prev; s2; s2 = s2->ts_prev) {
			if (s2->voice == voice) {
				s2->next = NULL;
				break;
			}
		}
		if (!s2)
			p_voice->sym = NULL;
	}
	s->ts_prev->ts_next = NULL;
}

/* -- set the bar numbers and treat %%clip / %%break -- */
static void set_bar_num(void)
{
	struct SYMBOL *s;
	int bar_time, wmeasure;
	int bar_num, bar_rep;

	wmeasure = voice_tb[cursys->top_voice].meter.wmeasure;
	if (wmeasure == 0)				/* if M:none */
		wmeasure = 1;
	bar_rep = nbar;

	/* don't count a bar at start of line */
	for (s = tsfirst; ; s = s->ts_next) {
		if (!s)
			return;
		switch (s->type) {
		case TIMESIG:
		case CLEF:
		case KEYSIG:
		case FMTCHG:
		case STBRK:
			continue;
		case BAR:
			if (s->u != 0) {
				nbar = s->u;		/* (%%setbarnb) */
				break;
			}
			if (s->as.u.bar.repeat_bar
			 && s->as.text
			 && cfmt.contbarnb == 0) {
				if (s->as.text[0] == '1') {
					bar_rep = nbar;
				} else {
					nbar = bar_rep; /* restart bar numbering */
					s->u = nbar;
				}
			}
			break;
		}
		break;
	}

	/* set the measure number on the top bars */
	bar_time = s->time + wmeasure;	/* for incomplete measure at start of tune */
	bar_num = nbar;
	for ( ; s; s = s->ts_next) {
		switch (s->type) {
		case TIMESIG:
			wmeasure = s->as.u.meter.wmeasure;
			if (wmeasure == 0)
				wmeasure = 1;
			if (s->time < bar_time)
				bar_time = s->time + wmeasure;
			break;
		case MREST:
			bar_num += s->as.u.bar.len - 1;
			while (s->ts_next
			    && s->ts_next->type != BAR)
				s = s->ts_next;
			break;
		case BAR: {
			struct SYMBOL *s2;
			int tim;

//			if (s->as.flags & ABC_F_INVIS)
//				break;
			if (s->u != 0) {
				bar_num = s->u;		/* (%%setbarnb) */
				if (s->time < bar_time) {
					s->u = 0;
					break;
				}
			} else {
				if (s->time < bar_time)	/* incomplete measure */
					break;
				bar_num++;
			}

			/* check if any repeat bar at this time */
			tim = s->time;
			s2 = s;
			do {
				if (s2->type == BAR
				 && s2->as.u.bar.repeat_bar
				 && s2->as.text
				 && cfmt.contbarnb == 0) {
					if (s2->as.text[0] == '1')
						bar_rep = bar_num;
					else		/* restart bar numbering */
						bar_num = bar_rep;
					break;
				}
				s2 = s2->next;
			} while (s2 && s2->time == tim);
			s->u = bar_num;
			bar_time = s->time + wmeasure;
			break;
		}
	    }
	}

	/* do the %%clip stuff */
	if (clip_start.bar >= 0) {
		if (bar_num <= clip_start.bar
		 || nbar > clip_end.bar) {
			tsfirst = NULL;
			return;
		}
		do_clip();
	}

	/* do the %%break stuff */
	{
		struct brk_s *brk;
		int nbar_min;

//		if (nbar == 1)
//			nbar = -1;	/* see go_global_time */
		nbar_min = nbar;
		if (nbar_min == 1)
			nbar_min = -1;
		for (brk = brks; brk; brk = brk->next) {
			if (brk->symsel.bar <= nbar_min
			 || brk->symsel.bar > bar_num)
				continue;
			s = go_global_time(tsfirst, &brk->symsel);
			if (s)
				s->sflags |= S_EOLN;
		}
	}
	if (cfmt.measurenb < 0)		/* if no display of measure bar */
		nbar = bar_num;		/* update in case of more music to come */
}

/* -- generate a piece of tune -- */
static void generate(void)
{
	int voice;
	int old_lvl;

	system_init();
	if (!tsfirst)
		return;				/* no symbol */
	set_bar_num();
	if (!tsfirst)
		return;				/* no more symbol */
	old_lvl = lvlarena(2);
	output_music();
	clrarena(2);				/* clear generation */
	lvlarena(old_lvl);

	/* reset the parser */
	for (voice = 0; voice < MAXVOICE; voice++) {
		voice_tb[voice].sym = voice_tb[voice].last_sym = NULL;
		voice_tb[voice].time = 0;
		voice_tb[voice].have_ly = 0;
		voice_tb[voice].staff = cursys->voice[voice].staff;
		voice_tb[voice].second = cursys->voice[voice].second;
	}
	if (staves_found > 0)
		staves_found = 0;
}

/* -- output the music and lyrics after tune -- */
static void gen_ly(int eob)
{
	generate();
	if (info['W' - 'A']) {
		put_words(info['W' - 'A']);
		info['W' - 'A'] = NULL;
	}
	if (eob)
		buffer_eob();
}

/* for transpose purpose, check if a pitch is already in the measure
 * in the current voice and return its accidental or (-1) */
static int acc_same_pitch(int pitch)
{
	struct SYMBOL *s;
	int i;

	for (s = curvoice->last_sym; s; s = s->prev) {
		switch (s->as.type) {
		case ABC_T_BAR:
			return -1;	/* no same pitch */
		default:
			continue;
		case ABC_T_NOTE:
			break;
		}

		for (i = 0; i <= s->nhd; i++) {
			if (s->as.u.note.pits[i] == pitch)
				return s->as.u.note.accs[i];
		}
	}
	return -1;
}

/* transpose a note / chord */
static void note_transpose(struct SYMBOL *s)
{
	int i, j, m, n, d, a, dp, i1, i2, i3, i4, sf_old;
	static signed char acc1[6] = {0, 1, 0, -1, 2, -2};
	static char acc2[5] = {A_DF, A_FT, A_NT, A_SH, A_DS};

	m = s->as.u.note.nhd;
	sf_old = curvoice->okey.sf;
	i2 = curvoice->ckey.sf - sf_old;
	dp = cgd2cde[(i2 + 4 * 7) % 7];
	if (curvoice->transpose < 0
	 && dp != 0)
		dp -= 7;
	dp += curvoice->transpose / 3 / 12 * 7;
	for (i = 0; i <= m; i++) {
		n = s->as.u.note.pits[i];
		s->as.u.note.pits[i] += dp;
		i1 = cde2fcg[(n + 5 + 16 * 7) % 7];	/* fcgdaeb */
		a = s->as.u.note.accs[i] & 0x07;
		if (a == 0) {
			if (curvoice->okey.nacc == 0) {
				if (sf_old > 0) {
					if (i1 < sf_old - 1)
						a = A_SH;
				} else if (sf_old < 0) {
					if (i1 >= sf_old + 6)
						a = A_FT;
				}
			} else {
				for (j = 0; j < curvoice->okey.nacc; j++) {
					if ((n + 16 * 7 - curvoice->okey.pits[j]) % 7
								== 0) {
						a = curvoice->okey.accs[j];
						break;
					}
				}
			}
		}
		i3 = i1 + i2 + acc1[a] * 7;

		/* accidental */
		i1 = ((i3 + 1 + 21) / 7 + 2 - 3 + 32 * 5) % 5;
		a = acc2[(unsigned) i1];
		if (curvoice->ckey.empty != 0) {	/* key none */
			int other_acc;

			other_acc = acc_same_pitch(s->as.u.note.pits[i]);
			switch (s->as.u.note.accs[i]) {
			case 0:
				if (other_acc >= 0 || a == A_NT)
					continue;
				break;
			case A_NT:
				break;
			default:
				if (other_acc < 0) {
					if (a == A_NT)
						a = 0;
				} else {
					if (a == other_acc)
						a = 0;
				}
				break;
			}
		} else if (s->as.u.note.accs[i] != 0) {
			;
		} else if (curvoice->ckey.nacc > 0) {	/* acc list */
			i4 = cgd2cde[(unsigned) ((i3 + 16 * 7) % 7)];
			for (j = 0; j < curvoice->ckey.nacc; j++) {
				if ((i4 + 16 * 7 - curvoice->ckey.pits[j]) % 7
							== 0)
					break;
			}
			if (j < curvoice->ckey.nacc)
				continue;
		} else {
			continue;
		}
		i1 = s->as.u.note.accs[i] & 0x07;
		i4 = s->as.u.note.accs[i] >> 3;
		if (i4 != 0				/* microtone */
		 && i1 != a) {				/* different accidental type */
			n = micro_tb[i4];
			d = (n & 0xff) + 1;
			n = (n >> 8) + 1;
			if (a == A_NT) {
/*fixme:should treat the double sharps/flats*/
				if (n >= d) {
					n -= d;
					a = i1;
				} else {
					n = d - n;
					if (i1 == A_SH)
						a = A_FT;
					else
						a = A_SH;
				}
			}
			d--;
			d += (n - 1) << 8;
			for (i4 = 1; i4 < MAXMICRO; i4++) {
				if (micro_tb[i4] == d)
					break;
				if (micro_tb[i4] == 0) {
					micro_tb[i4] = d;
					break;
				}
			}
			if (i4 == MAXMICRO) {
				error(1, s, "Too many microtone accidentals");
				i4 = 0;
			}
		}
		s->as.u.note.accs[i] = (i4 << 3) | a;
	}
}

/* transpose a guitar chord */
static void gch_transpose(struct SYMBOL *s)
{
	char *p, *q, *new_txt;
	int l, latin;
	int n, a, i1, i2, i3, i4;
	static const char note_names[] = "CDEFGAB";
	static const char *latin_names[7] =
			{ "Do", "Ré", "Mi", "Fa", "Sol", "La", "Si" };
	static const char *acc_name[5] = {"bb", "b", "", "#", "##"};

	p = s->as.text;

	/* skip the annotations */
	for (;;) {
		if (!strchr("^_<>@", *p))
			break;
		while (*p != '\0' && *p != '\n')
			p++;
		if (*p == '\0')
			return;
		p++;
	}

	/* main chord */
	q = p + 1;
	latin = 0;
	switch (*p) {
	case 'A':
	case 'B':
		n = *p - 'A' + 5;
		break;
	case 'C':
	case 'E':
	case 'G':
		n = *p - 'C';
		break;
	case 'D':
		if (p[1] == 'o') {
			latin = 1;
			n = 0;		/* Do */
			break;
		}
		n = 1;
		break;
	case 'F':
		if (p[1] == 'a')
			latin = 1;	/* Fa */
		n = 3;
		break;
	case 'L':
		latin = 1;		/* La */
		n = 6;
		break;
	case 'M':
		latin = 1;		/* Mi */
		n = 2;
		break;
	case 'R':
		latin = 1;
		if (p[1] != 'e')
			latin++;	/* Ré */
		n = 1;			/* Re */
		break;
	case 'S':
		latin = 1;
		if (p[1] == 'o') {
			latin++;
			n = 4;		/* Sol */
		} else {
			n = 6;		/* Si */
		}
		break;
	default:
		return;
	}
	q += latin;

	/* allocate a new string */
	new_txt = getarena(strlen(s->as.text) + 6);
	l = p - s->as.text;
	memcpy(new_txt, s->as.text, l);
	s->as.text = new_txt;
	new_txt += l;
	p = q;

	i2 = curvoice->ckey.sf - curvoice->okey.sf;
	i1 = cde2fcg[n];			/* fcgdaeb */
	a = 0;
	if (*p == '#') {
		a++;
		p++;
		if (*p == '#') {
			a++;
			p++;
		}
	} else if (*p == 'b') {
		a--;
		p++;
		if (*p == 'b') {
			a--;
			p++;
		}
	} else if (*p == '=') {
		p++;
	}
	i3 = i1 + i2 + a * 7;
	i4 = cgd2cde[(unsigned) ((i3 + 16 * 7) % 7)];
	if (!latin)
		*new_txt++ = note_names[i4];
	else
		new_txt += sprintf(new_txt, "%s", latin_names[i4]);
	i1 = ((i3 + 1 + 21) / 7 + 2 - 3 + 32 * 5) % 5;
						/* accidental */
	new_txt += sprintf(new_txt, "%s", acc_name[i1]);

	/* bass */
	while (*p != '\0' && *p != '\n' && *p != '/')
		*new_txt++ = *p++;
	if (*p == '/') {
		*new_txt++ = *p++;
//fixme: latin names not treated
		q = strchr(note_names, *p);
		if (q) {
			p++;
			n = q - note_names;
			i1 = cde2fcg[n];	/* fcgdaeb */
			if (*p == '#') {
				a = 1;
				p++;
			} else if (*p == 'b') {
				a = -1;
				p++;
			} else {
				a = 0;
			}
			i3 = i1 + i2 + a * 7;
			i4 = cgd2cde[(unsigned) ((i3 + 16 * 7) % 7)];
			*new_txt++ = note_names[i4];
			i1 = ((i3 + 1 + 21) / 7 + 2 - 3 + 32 * 5) % 5;
			new_txt += sprintf(new_txt, "%s", acc_name[i1]);
		}
	}
	strcpy(new_txt, p);
}

/* -- build the guitar chords / annotations -- */
static void gch_build(struct SYMBOL *s)
{
	struct gch *gch;
	char *p, *q, antype, sep;
	float w, h_ann, h_gch, y_above, y_below, y_left, y_right;
	float xspc;
	int l, ix, box, gch_place;

	if (s->posit.gch == SL_HIDDEN)
		return;
	s->gch = getarena(sizeof *s->gch * MAXGCH);
	memset(s->gch, 0, sizeof *s->gch * MAXGCH);

	if (curvoice->transpose != 0)
		gch_transpose(s);

	/* split the guitar chords / annotations
	 * and initialize their vertical offsets */
	gch_place = s->posit.gch == SL_BELOW ? -1 : 1;
	h_gch = cfmt.font_tb[cfmt.gcf].size;
	h_ann = cfmt.font_tb[cfmt.anf].size;
	y_above = y_below = y_left = y_right = 0;
	box = cfmt.gchordbox;
	p = s->as.text;
	gch = s->gch;
	sep = '\n';
	antype = 'g';			/* (compiler warning) */
	for (;;) {
		if (sep != 'n' && strchr("^_<>@", *p)) {
			gch->font = cfmt.anf;
			antype = *p++;
			if (antype == '@') {
				int n;
				float xo, yo;

				if (sscanf(p, "%f,%f%n", &xo, &yo, &n) != 2) {
					error(1, s, "Error in annotation \"@\"");
				} else {
					p += n;
					if (*p == ' ')
						p++;
					gch->x = xo;
					gch->y = yo;
				}
			}
		} else if (sep == '\n') {
			gch->font = cfmt.gcf;
			gch->box = box;
			antype = 'g';
		} else {
			gch->font = (gch - 1)->font;
			gch->box = (gch - 1)->box;
		}
		gch->type = antype;
		switch (antype) {
		default:				/* guitar chord */
			if (gch_place < 0)
				break;			/* below */
			y_above += h_gch;
			if (box)
				y_above += 2;
			break;
		case '^':				/* above */
			y_above += h_ann;
			break;
		case '_':				/* below */
			break;
		case '<':				/* left */
			y_left += h_ann * 0.5;
			break;
		case '>':				/* right */
			y_right += h_ann * 0.5;
			break;
		case '@':				/* absolute */
			if (gch->x == 0 && gch->y == 0
			 && gch != s->gch) {		/* if not 1st line */
				gch->x = (gch - 1)->x;
				gch->y = (gch - 1)->y - h_ann;
			}
			break;
		}
		gch->idx = p - s->as.text;
		for (;;) {
			switch (*p) {
			default:
				p++;
				continue;
			case '\\':
				p++;
				switch (*p) {
				case 'n':
					p[-1] = '\0';
					break;		/* sep = 'n' */
				case '\\':
				case ';':
					p++;
					continue;
				}
				break;
			case '\0':
			case ';':
			case '\n':
				break;
			}
			break;
		}
		sep = *p;
		if (sep == '\0')
			break;
		*p++ = '\0';
		gch++;
		if (gch - s->gch >= MAXGCH) {
			error(1, s, "Too many guitar chords / annotations");
			break;
		}
	}

	/* change the accidentals in the guitar chords */
	for (ix = 0, gch = s->gch; ix < MAXGCH; ix++, gch++) {
		if (gch->type == '\0')
			break;
		if (gch->type != 'g')
			continue;
		p = s->as.text + gch->idx;
		q = p;
		for (; *p != '\0'; p++) {
			switch (*p) {
			case '#':
			case 'b':
			case '=':
				if (p == q)	/* 1st char or after a slash */
					break;

				/* set the accidentals as unused utf-8 values
				 * (see subs.c) */
				switch (*p) {
				case '#':
					*p = 0x01;
					break;
				case 'b':
					*p = 0x02;
					break;
				default:
/*				case '=': */
					*p = 0x03;
					break;
				}
				break;
			case '\\':
				p++;
				switch (*p) {
				case '#':
					p[-1] = 0x01;
					goto move;
				case 'b':
					p[-1] = 0x02;
					goto move;
				case '=':
					p[-1] = 0x03;
				move:
					l = strlen(p);
					memmove(p, p + 1, l);
					p--;
					break;
				}
				break;
			case ' ':
				if (p != q)		//??
					break;
				/* fall thru */
			case '/':
				q = p + 1;
				break;
			}
		}
	}

	/* set the offsets and widths */
/*fixme:utf8*/
	for (ix = 0, gch = s->gch; ix < MAXGCH; ix++, gch++) {
		if (gch->type == '\0')
			break;
		if (gch->type == '@')
			continue;		/* no width */
		p = s->as.text + gch->idx;
		str_font(gch->font);
		w = tex_str(p);
		gch->w = w + 4;
		switch (gch->type) {
		case '_':			/* below */
			xspc = w;
			xspc *= GCHPRE;
			if (xspc > 8)
				xspc = 8;
			gch->x = -xspc;
			y_below -= h_ann;
			gch->y = y_below;
			break;
		case '^':			/* above */
			xspc = w;
			xspc *= GCHPRE;
			if (xspc > 8)
				xspc = 8;
			gch->x = -xspc;
			y_above -= h_ann;
			gch->y = y_above;
			break;
		default:			/* guitar chord */
			xspc = w;
			xspc *= GCHPRE;
			if (xspc > 8)
				xspc = 8;
			gch->x = -xspc;
			if (gch_place < 0) {	/* below */
				y_below -= h_gch;
				gch->y = y_below;
				if (box) {
					y_below -= 2;
					gch->y -= 1;
				}
			} else {
				y_above -= h_gch;
				gch->y = y_above;
				if (box) {
					y_above -= 2;
					gch->y -= 1;
				}
			}
			break;
		case '<':		/* left */
			gch->x = -(w + 6);
			y_left -= h_ann;
			gch->y = y_left;
			break;
		case '>':		/* right */
			gch->x = 6;
			y_right -= h_ann;
			gch->y = y_right;
			break;
		}
	}
}

/* get the note which will receive a lyric word */
static struct SYMBOL *next_lyric_note(struct SYMBOL *s)
{
	while (s
	    && (s->as.type != ABC_T_NOTE
	     || (s->as.flags & ABC_F_GRACE)))
		s = s->next;
	return s;
}

/* -- parse lyric (vocal) lines (w:) -- */
static struct abcsym *get_lyric(struct abcsym *as)
{
	struct SYMBOL *s;
	char word[128], *p, *q;
	int ln, cont;
	struct FONTSPEC *f;

	curvoice->have_ly = curvoice->posit.voc != SL_HIDDEN;

	f = &cfmt.font_tb[cfmt.vof];
	str_font(cfmt.vof);			/* (for tex_str) */

	/* treat all w: lines */
	s = curvoice->lyric_start;
	if (!s) {
		s = curvoice->sym;
		if (!s) {
			error(1, s, "w: without music");
			return as;
		}
		curvoice->lyric_start = s;
	}
	if (s->ly)			/* prev music line ended by a note */
		s = s->next;
	cont = 0;
	ln = -1;
	for (;;) {
		if (!cont) {
			if (ln >= MAXLY) {
				error(1, s, "Too many lyric lines");
				ln--;
			}
			ln++;
			s = curvoice->lyric_start;
		} else {
			cont = 0;
		}

		/* scan the lyric line */
		p = &as->text[2];
		while (*p != '\0') {
			while (isspace((unsigned char) *p))
				p++;
			if (*p == '\0')
				break;
			if (*p == '\\' && p[1] == '\0') {
				cont = 1;
				break;
			}
			switch (*p) {
			case '|':
				while (s && s->type != BAR)
					s = s->next;
				if (!s) {
					error(1, s,
						"Not enough bar lines for lyric line");
					goto ly_next;
				}
				s = s->next;
				p++;
				continue;
			case '-':
				word[0] = LY_HYPH;
				word[1] = '\0';
				p++;
				break;
			case '_':
				word[0] = LY_UNDER;
				word[1] = '\0';
				p++;
				break;
			case '*':
				word[0] = *p++;
				word[1] = '\0';
				break;
			default:
				q = word;
				for (;;) {
					unsigned char c;

					c = *p;
					switch (c) {
					case '\0':
					case ' ':
					case '\t':
					case '_':
					case '*':
					case '|':
						break;
					case '~':
						c = ' ';
						goto addch;
					case '-':
						c = LY_HYPH;
						goto addch;
					case '\\':
						if (p[1] == '\0')
							break;
						switch (p[1]) {
						case '~':
						case '_':
						case '*':
						case '|':
						case '-':
						case ' ':
						case '\\':
							c = *++p;
							break;
						}
						/* fall thru */
					default:
					addch:
						if (q < &word[sizeof word - 1])
							*q++ = c;
						p++;
						if (c == LY_HYPH)
							break;
						continue;
					}
					break;
				}
				*q = '\0';
				break;
			}

			/* store the word in the next note */
			s = next_lyric_note(s);
			if (!s) {
				error(1, s, "Too many words in lyric line");
				goto ly_next;
			}
			if (word[0] != '*'
			 && s->posit.voc != SL_HIDDEN) {
				struct lyl *lyl;
				float w;

				if (!s->ly) {
					s->ly = (struct lyrics *) getarena(sizeof (struct lyrics));
					memset(s->ly, 0, sizeof (struct lyrics));
				}
				w = tex_str(word);

				/* handle the font change at start of text */
				q = tex_buf;
				if (*q == '$' && isdigit((unsigned char) q[1])
				 && (unsigned) (q[1] - '0') < FONT_UMAX) {
					int ft;

					ft = q[1] - '0';
					if (ft == 0)
						ft = cfmt.vof;
					f = &cfmt.font_tb[ft];
					str_font(ft);
					q += 2;
				}
				lyl = (struct lyl *) getarena(sizeof *s->ly->lyl[0]
							    + strlen(q));
				s->ly->lyl[ln] = lyl;
				lyl->f = f;
				lyl->w = w;
				strcpy(lyl->t, q);

				/* handle the font changes inside the text */
				while (*q != '\0') {
					if (*q == '$' && isdigit((unsigned char) q[1])
					 && (unsigned) (q[1] - '0') < FONT_UMAX) {
						int ft;

						q++;
						ft = *q - '0';
						if (ft == 0)
							ft = cfmt.vof;
						f = &cfmt.font_tb[ft];
						str_font(ft);
					}
					q++;
				}
			}
			s = s->next;
		}

		/* loop if more lyrics */
ly_next:
		for (;;) {
			if (!as->next)
				goto ly_upd;
			switch (as->next->type) {
			case ABC_T_PSCOM:
				as = process_pscomment(as->next);
				f = &cfmt.font_tb[cfmt.vof];	/* may have changed */
				str_font(cfmt.vof);
				continue;
			case ABC_T_INFO:
				if (as->next->text[0] != 'w'
				 && as->next->text[0] != '+')
					goto ly_upd;
				as = as->next;
				if (as->text[0] == '+')
					cont = 1;
				if (!cont) {
					s = next_lyric_note(s);
					if (s) {
						error(1, s,
							"Not enough words for lyric line");
					}
				}
				break;			/* more lyric */
			default:
				goto ly_upd;
			}
			break;
		}
	}

	/* the next lyrics will go into the next notes */
ly_upd:
	s = next_lyric_note(s);
	if (s)
		error(1, s, "Not enough words for lyric line");
	curvoice->lyric_start = curvoice->last_sym;
	return as;
}

/* -- add a voice in the linked list -- */
static void voice_link(struct VOICE_S *p_voice)
{
	struct VOICE_S *p_voice2;

	p_voice2 = first_voice;
	for (;;) {
		if (p_voice2 == p_voice)
			return;
		if (!p_voice2->next)
			break;
		p_voice2 = p_voice2->next;
	}
	p_voice2->next = p_voice;
}

/* -- get a voice overlay -- */
static void get_over(struct SYMBOL *s)
{
	struct VOICE_S *p_voice, *p_voice2, *p_voice3;
	int range, voice, voice2, voice3;
static char tx_wrong_dur[] = "Wrong duration in voice overlay";

	/* treat the end of overlay */
	p_voice = curvoice;
	if (p_voice->ignore)
		return;
	if (s->as.type == ABC_T_BAR
	 || s->as.u.v_over.type == V_OVER_E)  {
		p_voice->last_sym->sflags |= S_BEAM_END;
		over_bar = 0;
		if (over_time < 0) {
			error(1, s, "Erroneous end of voice overlap");
			return;
		}
		if (p_voice->time != over_mxtime)
			error(1, s, tx_wrong_dur);
		curvoice = &voice_tb[over_voice];
		over_voice = -1;
		over_time = -1;
		return;
	}

	/* treat the full overlay start */
	if (s->as.u.v_over.type == V_OVER_S) {
		over_time = p_voice->time;
		return;
	}

	/* (here is treated a new overlay - '&') */
	/* create the extra voice if not done yet */
	p_voice->last_sym->sflags |= S_BEAM_END;
	voice2 = s->as.u.v_over.voice;
	p_voice2 = &voice_tb[voice2];
	if (parsys->voice[voice2].range < 0) {
		int clone;

		if (cfmt.abc2pscompat) {
			error(1, s, "Cannot have %%%%abc2pscompat");
			cfmt.abc2pscompat = 0;
		}
		clone = p_voice->clone >= 0;
		p_voice2->id[0] = '&';
		p_voice2->id[1] = '\0';
		p_voice2->second = 1;
		parsys->voice[voice2].second = 1;
		p_voice2->scale = p_voice->scale;
		p_voice2->octave = p_voice->octave;
		p_voice2->transpose = p_voice->transpose;
		memcpy(&p_voice2->key, &p_voice->key,
					sizeof p_voice2->key);
		memcpy(&p_voice2->ckey, &p_voice->ckey,
					sizeof p_voice2->ckey);
		memcpy(&p_voice2->okey, &p_voice->okey,
					sizeof p_voice2->okey);
		p_voice2->posit = p_voice->posit;
		range = parsys->voice[p_voice - voice_tb].range;
		for (voice = 0; voice < MAXVOICE; voice++) {
			if (parsys->voice[voice].range > range)
				parsys->voice[voice].range += clone + 1;
		}
		parsys->voice[voice2].range = range + 1;
		voice_link(p_voice2);
		if (clone) {
			for (voice3 = MAXVOICE; --voice3 >= 0; ) {
				if (parsys->voice[voice3].range < 0)
					break;
			}
			if (voice3 > 0) {
				p_voice3 = &voice_tb[voice3];
				strcpy(p_voice3->id, p_voice2->id);
				p_voice3->second = 1;
				parsys->voice[voice3].second = 1;
				p_voice3->scale = voice_tb[p_voice->clone].scale;
				parsys->voice[voice3].range = range + 2;
				voice_link(p_voice3);
				p_voice2->clone = voice3;
			} else {
				error(1, s,
				      "Too many voices for overlay cloning");
			}
		}
	}
	voice = p_voice - voice_tb;
	p_voice2->cstaff = p_voice2->staff = parsys->voice[voice2].staff
			= parsys->voice[voice].staff;
	if ((voice3 = p_voice2->clone) >= 0) {
		p_voice3 = &voice_tb[voice3];
		p_voice3->cstaff = p_voice3->staff
				= parsys->voice[voice3].staff
				= parsys->voice[p_voice->clone].staff;
	}

	if (over_time < 0) {			/* first '&' in a measure */
		int time;

		over_bar = 1;
		over_mxtime = p_voice->time;
		over_voice = voice;
		time = p_voice2->time;
		for (s = p_voice->last_sym; /*s*/; s = s->prev) {
			if (s->type == BAR
			 || s->time <= time)	/* (if start of tune) */
				break;
		}
		over_time = s->time;
	} else {
		if (over_voice < 0) {
			over_mxtime = p_voice->time;
			over_voice = voice;
		} else if (p_voice->time != over_mxtime)
			error(1, s, tx_wrong_dur);
	}
	p_voice2->time = over_time;
	curvoice = p_voice2;
}

struct staff_s {
	short voice;
	short flags;
};

/* -- parse %%staves / %%score -- */
static void parse_staves(struct SYMBOL *s,
			struct staff_s *staves)
{
	char *p;
	int voice, flags_st, brace, bracket, parenth, err;
	short flags;
	struct staff_s *p_staff;

	/* define the voices */
	err = 0;
	flags = 0;
	brace = bracket = parenth = 0;
	flags_st = 0;
	voice = 0;
	p = s->as.text + 7;
	while (*p != '\0' && !isspace((unsigned char) *p))
		p++;
	while (*p != '\0') {
		switch (*p) {
		case ' ':
		case '\t':
			break;
		case '[':
			if (parenth || brace + bracket >= 2) {
				error(1, s, "Misplaced '[' in %%%%staves");
				err = 1;
				break;
			}
			if (brace + bracket == 0)
				flags |= OPEN_BRACKET;
			else
				flags |= OPEN_BRACKET2;
			bracket++;
			flags_st <<= 8;
			flags_st |= OPEN_BRACKET;
			break;
		case '{':
			if (parenth || brace || bracket >= 2) {
				error(1, s, "Misplaced '{' in %%%%staves");
				err = 1;
				break;
			}
			if (bracket == 0)
				flags |= OPEN_BRACE;
			else
				flags |= OPEN_BRACE2;
			brace++;
			flags_st <<= 8;
			flags_st |= OPEN_BRACE;
			break;
		case '(':
			if (parenth) {
				error(1, s, "Misplaced '(' in %%%%staves");
				err = 1;
				break;
			}
			flags |= OPEN_PARENTH;
			parenth++;
			flags_st <<= 8;
			flags_st |= OPEN_PARENTH;
			break;
		case '*':
			flags |= FL_VOICE;
			break;
		default:
			if (!isalnum((unsigned char) *p) && *p != '_') {
				error(1, s, "Bad voice ID in %%%%staves");
				err = 1;
				break;
			}
			if (voice >= MAXVOICE) {
				error(1, s, "Too many voices in %%%%staves");
				err = 1;
				break;
			}
			{
				int i, v;
				char sep, *q;

				q = p;
				while (isalnum((unsigned char) *p) || *p == '_')
					p++;
				sep = *p;
				*p = '\0';

				/* search the voice in the voice table */
				v = -1;
				for (i = 0; i < MAXVOICE; i++) {
					if (strcmp(q, voice_tb[i].id) == 0) {
						v = i;
						break;
					}
				}
				if (v < 0) {
					error(1, s,
						"Voice '%s' of %%%%staves has no symbol",
						q);
					err = 1;
//					break;
					p_staff = staves;
				} else {
					p_staff = staves + voice++;
					p_staff->voice = v;
				}
				*p = sep;
			}
			for (;;) {
				switch (*p) {
				case ' ':
				case '\t':
					p++;
					continue;
				case ']':
					if (!(flags_st & OPEN_BRACKET)) {
						error(1, s,
							"Misplaced ']' in %%%%staves");
						err = 1;
						break;
					}
					bracket--;
					if (brace + bracket == 0)
						flags |= CLOSE_BRACKET;
					else
						flags |= CLOSE_BRACKET2;
					flags_st >>= 8;
					p++;
					continue;
				case '}':
					if (!(flags_st & OPEN_BRACE)) {
						error(1, s,
							"Misplaced '}' in %%%%staves");
						err = 1;
						break;
					}
					brace--;
					if (bracket == 0)
						flags |= CLOSE_BRACE;
					else
						flags |= CLOSE_BRACE2;
					flags_st >>= 8;
					p++;
					continue;
				case ')':
					if (!(flags_st & OPEN_PARENTH)) {
						error(1, s,
							"Misplaced ')' in %%%%staves");
						err = 1;
						break;
					}
					parenth--;
					flags |= CLOSE_PARENTH;
					flags_st >>= 8;
					p++;
					continue;
				case '|':
					flags |= STOP_BAR;
					p++;
					continue;
				}
				break;
			}
			p_staff->flags = flags;
			flags = 0;
			if (*p == '\0')
				break;
			continue;
		}
		if (*p == '\0')
			break;
		p++;
	}
	if (flags_st != 0) {
		error(1, s, "'}', ')' or ']' missing in %%%%staves");
		err = 1;
	}
	if (err) {
		int i;

		for (i = 0; i < voice; i++)
			staves[i].flags = 0;
	}
	if (voice < MAXVOICE)
		staves[voice].voice = -1;
}

/* -- get staves definition (%%staves / %%score) -- */
static void get_staves(struct SYMBOL *s)
{
	struct VOICE_S *p_voice, *p_voice2;
	struct staff_s *p_staff, staves[MAXVOICE];
	int i, flags, voice, staff, range, dup_voice, maxtime;

	voice_compress();
	voice_dup();

	/* create a new staff system */
	curvoice = p_voice = first_voice;
	maxtime = p_voice->time;
	flags = p_voice->sym != NULL;
	for (p_voice = p_voice->next; p_voice; p_voice = p_voice->next) {
		if (p_voice->time > maxtime)
			maxtime = p_voice->time;
		if (p_voice->sym)
			flags = 1;
	}
	if (flags == 0			/* if first %%staves */
	 || (maxtime == 0 && staves_found < 0)) {
		for (voice = 0; voice < MAXVOICE; voice++)
			parsys->voice[voice].range = -1;
	} else {

		/*
		 * create a new staff system and
		 * link the staves in a voice which is seen from
		 * the previous system - see sort_all
		 */
		p_voice = curvoice;
		if (parsys->voice[p_voice - voice_tb].range < 0) {
			for (voice = 0; voice < MAXVOICE; voice++) {
				if (parsys->voice[voice].range >= 0) {
					curvoice = &voice_tb[voice];
					break;
				}
			}
/*fixme: should check if voice < MAXVOICE*/
		}
		curvoice->time = maxtime;
		sym_link(s, STAVES);	/* link the staves in the current voice */
		s->as.state = ABC_S_HEAD; /* (output PS sequences immediately) */
		parsys->nstaff = nstaff;
		system_new();
	}
	staves_found = maxtime;

	memset(staves, 0, sizeof staves);
	parse_staves(s, staves);

	/* initialize the voices */
	for (voice = 0, p_voice = voice_tb;
	     voice < MAXVOICE;
	     voice++, p_voice++) {
		p_voice->second = 0;
		p_voice->floating = 0;
		p_voice->ignore = 0;
		p_voice->time = maxtime;
	}
	dup_voice = MAXVOICE;
	range = 0;
	p_staff = staves;
	parsys->top_voice = p_staff->voice;
	for (i = 0;
	     i < MAXVOICE && p_staff->voice >= 0;
	     i++, p_staff++) {
		voice = p_staff->voice;
		p_voice = &voice_tb[voice];
		if (parsys->voice[voice].range >= 0) {
			if (parsys->voice[dup_voice - 1].range >= 0) {
				error(1, s, "Too many voices for cloning");
				continue;
			}
			voice = --dup_voice;	/* duplicate the voice */
			p_voice2 = &voice_tb[voice];
			memcpy(p_voice2, p_voice, sizeof *p_voice2);
			p_voice2->next = NULL;
			p_voice2->sym = p_voice2->last_sym = NULL;
			p_voice2->tblts[0] = p_voice2->tblts[1] = 0;
			p_voice2->clone = -1;
			while (p_voice->clone > 0)
				p_voice = &voice_tb[p_voice->clone];
			p_voice->clone = voice;
			p_voice = p_voice2;
			p_staff->voice = voice;
		}
		parsys->voice[voice].range = range++;
		voice_link(p_voice);
	}

	/* change the behavior from %%staves to %%score */
	if (s->as.text[3] == 't') {		/* if %%staves */
		for (i = 0, p_staff = staves;
		     i < MAXVOICE - 2 && p_staff->voice >= 0;
		     i++, p_staff++) {
			flags = p_staff->flags;
			if (!(flags & (OPEN_BRACE | OPEN_BRACE2)))
				continue;
			if ((flags & (OPEN_BRACE | CLOSE_BRACE))
					== (OPEN_BRACE | CLOSE_BRACE)
			 || (flags & (OPEN_BRACE2 | CLOSE_BRACE2))
					== (OPEN_BRACE2 | CLOSE_BRACE2))
				continue;
			if (p_staff[1].flags != 0)
				continue;
			if ((flags & OPEN_PARENTH)
			 || (p_staff[2].flags & OPEN_PARENTH))
				continue;

			/* {a b c} --> {a *b c} */
			if (p_staff[2].flags & (CLOSE_BRACE | CLOSE_BRACE2)) {
				p_staff[1].flags |= FL_VOICE;

			/* {a b c d} --> {(a b) (c d)} */
			} else if (p_staff[2].flags == 0
				 && (p_staff[3].flags & (CLOSE_BRACE | CLOSE_BRACE2))) {
				p_staff->flags |= OPEN_PARENTH;
				p_staff[1].flags |= CLOSE_PARENTH;
				p_staff[2].flags |= OPEN_PARENTH;
				p_staff[3].flags |= CLOSE_PARENTH;
			}
		}
	}

	/* set the staff system */
	staff = -1;
	for (i = 0, p_staff = staves;
	     i < MAXVOICE && p_staff->voice >= 0;
	     i++, p_staff++) {
		flags = p_staff->flags;
		if ((flags & (OPEN_PARENTH | CLOSE_PARENTH))
				== (OPEN_PARENTH | CLOSE_PARENTH)) {
			flags &= ~(OPEN_PARENTH | CLOSE_PARENTH);
			p_staff->flags = flags;
		}
		voice = p_staff->voice;
		p_voice = &voice_tb[voice];
		if (flags & FL_VOICE) {
			p_voice->floating = 1;
			p_voice->second = 1;
		} else {
#if MAXSTAFF < MAXVOICE
			if (staff >= MAXSTAFF - 1) {
				error(1, s, "Too many staves");
			} else
#endif
				staff++;
			parsys->staff[staff].flags = 0;
		}
		p_voice->staff = p_voice->cstaff
				= parsys->voice[voice].staff = staff;
		parsys->staff[staff].flags |= flags;
		if (flags & OPEN_PARENTH) {
			while (i < MAXVOICE) {
				i++;
				p_staff++;
				voice = p_staff->voice;
				p_voice = &voice_tb[voice];
				p_voice->second = 1;
				p_voice->staff = p_voice->cstaff
						= parsys->voice[voice].staff
						= staff;
				if (p_staff->flags & CLOSE_PARENTH)
					break;
			}
			parsys->staff[staff].flags |= p_staff->flags;
		}
	}
	if (staff < 0)
		staff = 0;
	nstaff = staff;

	/* change the behaviour of '|' in %%score */
	if (s->as.text[3] == 'c') {		/* if %%score */
		for (staff = 0; staff <= nstaff; staff++)
			parsys->staff[staff].flags ^= STOP_BAR;
	}

	for (voice = 0; voice < MAXVOICE; voice++) {
		parsys->voice[voice].second = voice_tb[voice].second;
		staff = p_voice->staff;
		if (staff > 0)
			p_voice->norepbra
				= !(parsys->staff[staff - 1].flags & STOP_BAR);
	}
}

/* -- re-initialize all potential voices -- */
static void voice_init(void)
{
	struct VOICE_S *p_voice;
	int i;

	for (i = 0, p_voice = voice_tb;
	     i < MAXVOICE;
	     i++, p_voice++) {
		p_voice->sym = p_voice->last_sym = NULL;
		p_voice->bar_start = 0;
		p_voice->time = 0;
		p_voice->slur_st = 0;
		p_voice->hy_st = 0;
		p_voice->tie = 0;
		p_voice->rtie = 0;
	}
}

/* output a pdf mark */
static void put_pdfmark(char *p)
{
	unsigned char c, *q;
	int u;

	p = trim_title(p, 0);

	/* check if pure ASCII without '\', '(' nor ')'*/
	for (q = (unsigned char *) p; *q != '\0'; q++) {
		switch (*q) {
		case '\\':
		case '(':
		case ')':
			break;
		default:
			if (*q >= 0x80)
				break;
			continue;
		}
		break;
	}
	if (*q == '\0') {
		a2b("[/Title(%s)/OUT pdfmark\n", p);
		return;
	}

	/* build utf-8 mark */
	a2b("[/Title<FEFF");
	q = (unsigned char *) p;
	u = -1;
	while (*q != '\0') {
		c = *q++;
		if (c < 0x80) {
			if (u >= 0) {
				a2b("%04X", u);
				u = -1;
			}
			a2b("%04X", (int) c);
			continue;
		}
		if (c < 0xc0) {
			u = (u << 6) | (c & 0x3f);
			continue;
		}
		if (u >= 0) {
			a2b("%04X", u);
			u = -1;
		}
		if (c < 0xe0)
			u = c & 0x1f;
		else if (c < 0xf0)
			u = c & 0x0f;
		else
			u = c & 0x07;
	}
	if (u >= 0) {
		a2b("%04X", u);
		u = -1;
	}
	a2b(">/OUT pdfmark\n");
}

/* rebuild a tune header for %%tune filter */
static char *tune_header_rebuild(struct abcsym *as)
{
	struct abcsym *as2;
	char *header, *p;
	int len;

	len = 0;
	as2 = as;
	for (;;) {
		if (as2->type == ABC_T_INFO) {
			len += strlen(as2->text) + 1;
			if (as2->text[0] == 'K')
				break;
		}
		as2 = as2->next;
	}
	header = malloc(len + 1);
	p = header;
	for (;;) {
		if (as->type == ABC_T_INFO) {
			strcpy(p, as->text);
			p += strlen(p);
			*p++ = '\n';
			if (as->text[0] == 'K')
				break;
		}
		as = as->next;
	}
	*p++ = '\0';
	return header;
}

/* apply the options to the current tune */
static void tune_filter(struct abcsym *as)
{
	struct tune_opt_s *opt;
	struct SYMBOL *s, *s2;
	struct slre slre;
	char *header, *p;

	header = tune_header_rebuild(as);
	for (opt = tune_opts; opt; opt = opt->next) {
		struct abcsym *last_staves;

		p = &opt->s->as.text[2 + 5];	/* "%%tune RE" */
		while (isspace((unsigned char) *p))
			p++;
		if (!slre_compile(&slre, p))
			continue;
		if (!slre_match(&slre, header, strlen(header), 0))
			continue;

		/* apply the options */
		cur_tune_opts = opt;
		last_staves = as->next;
		for (s = opt->s->next; s; s = s->next) {
			struct abcsym *as2;

			/* replace the next %%staves/%%score */
			as2 = (struct abcsym *) s;
			if (as2->type == ABC_T_PSCOM
			 && (strncmp(&as2->text[2], "staves", 6) == 0
			  || strncmp(&as2->text[2], "score", 5) == 0)) {
				while (last_staves) {
					if (last_staves->type == ABC_T_PSCOM
					 && (strncmp(&last_staves->text[2],
								"staves", 6) == 0
					  || strncmp(&last_staves->text[2],
								 "score", 5) == 0)) {
						last_staves->text = as2->text;
						last_staves = last_staves->next;
						break;
					}
					last_staves = last_staves->next;
				}
				continue;
			}
			s2 = (struct SYMBOL *) getarena(sizeof *s2);
			memcpy(s2, s, sizeof *s2);
			process_pscomment((struct abcsym *) s2);
		}
		cur_tune_opts = NULL;
	}
	free(header);
}

/* apply the options of the current voice */
static void voice_filter(void)
{
	struct voice_opt_s *opt;
	struct SYMBOL *s;
	struct slre slre;
	int pass;
	char *p;

	/* scan the global, then the tune options */
	pass = 0;
	opt = voice_opts;
	for (;;) {
		if (!opt) {
			if (pass != 0)
				break;
			if (!cur_tune_opts)
				break;
			opt = cur_tune_opts->voice_opts;
			if (!opt)
				break;
			pass++;
		}
		p = &opt->s->as.text[2 + 6];	/* "%%voice RE" */
		while (isspace((unsigned char) *p))
			p++;
		if (!slre_compile(&slre, p))
			goto next_voice;
		if (!slre_match(&slre, curvoice->id, strlen(curvoice->id), 0)
		 && (curvoice->nm == 0
		  || !slre_match(&slre, curvoice->nm, strlen(curvoice->nm), 0)))
			goto next_voice;

		/* apply the options */
		for (s = opt->s->next; s; s = s->next) {
			struct SYMBOL *s2;

			s2 = (struct SYMBOL *) getarena(sizeof *s2);
			memcpy(s2, s, sizeof *s2);
			process_pscomment((struct abcsym *) s2);
		}
next_voice:
		opt = opt->next;
	}
}

/* -- check if a pseudo-comment may be in the tune header -- */
static int check_header(struct abcsym *as)
{
	switch (as->text[2]) {
	case 'E':
		if (strncmp(as->text + 3, "PS", 2) == 0)
			return 0;
		break;
	case 'm':
		if (strncmp(as->text + 3, "ulticol", 7) == 0)
			return 0;
		break;
	}
	return 1;
}

/* -- set the global definitions after the first K: or middle-tune T:'s -- */
static void set_global_def(void)
{
	struct VOICE_S *p_voice;
	int i;

	for (i = MAXVOICE, p_voice = voice_tb;
	     --i >= 0;
	     p_voice++) {
		if (p_voice->key.mode >= BAGPIPE
		 && p_voice->posit.std == 0)
			p_voice->posit.std = SL_BELOW;
		p_voice->transpose = cfmt.transpose;
		if (p_voice->key.empty)
			p_voice->key.sf = 0;
	}

	/* switch to the 1st voice */
	curvoice = &voice_tb[parsys->top_voice];
}

/* -- get the global definitions after the first K: or middle-tune T:'s -- */
static struct abcsym *get_global_def(struct abcsym *as,
				     struct abctune *t)
{
	struct abcsym *as2;

	for (;;) {
		as2 = as->next;
		if (!as2)
			break;
		switch (as2->type) {
		case ABC_T_INFO:
			switch (as2->text[0]) {
			case 'K':
				as = as2;
				as->state = ABC_S_HEAD;
				get_key((struct SYMBOL *) as);
				continue;
			case 'I':
			case 'M':
			case 'Q':
				as = as2;
				as->state = ABC_S_HEAD;
				as = get_info(as, t);
				continue;
			}
			break;
		case ABC_T_PSCOM:
			if (!check_header(as2))
				break;
			as = as2;
			as->state = ABC_S_HEAD;
			as = process_pscomment(as);
			continue;
		}
		break;
	}
	set_global_def();
	return as;
}

/* -- identify info line, store in proper place	-- */
static struct abcsym *get_info(struct abcsym *as,
		     struct abctune *t)
{
	struct SYMBOL *s, *s2;
	struct VOICE_S *p_voice;
	char *p;
	char info_type;
	int old_lvl;
	static char *state_txt[] = {"global", "header", "tune"};

	/* change arena to global or tune */
	old_lvl = lvlarena(as->state != ABC_S_GLOBAL);

	s = (struct SYMBOL *) as;
	info_type = as->text[0];
	switch (info_type) {
	case 'd':
		break;
	case 'I':
		as = process_pscomment(as);	/* same as pseudo-comment */
		break;
	case 'K':
		get_key(s);
		if (as->state != ABC_S_HEAD)
			break;
		info['K' - 'A'] = s;		/* first K:, end of tune header */
		tunenum++;

		if (!epsf) {
			if (!cfmt.oneperpage)
				use_buffer = !cfmt.splittune;
			bskip(cfmt.topspace);
		}
		write_heading(t);
		block_put();

		/* information for index
		 * (pdfmark must be after title show for Adobe Distiller) */
		s2 = info['T' - 'A'];
		p = &s2->as.text[2];
		if (*p != '\0') {
			a2b("%% --- %s (%s) ---\n"
				"%% --- font ",
				&info['X' - 'A']->as.text[2], p);
			outft = -1;
			set_font(TITLEFONT);		/* font in comment */
			a2b("\n");
			outft = -1;
			if (cfmt.pdfmark)
				put_pdfmark(p);
			for (s2 = s2->next; s2; s2 = s2->next) {
				p = &s2->as.text[2];
				a2b("%% --- + (%s) ---\n", p);
				if (cfmt.pdfmark > 1)
					put_pdfmark(p);
			}
		}

		nbar = cfmt.measurefirst;	/* measure numbering */
		over_voice = -1;
		over_time = -1;
		over_bar = 0;
		reset_gen();

		as = get_global_def(as, t);

		if (!(cfmt.fields[0] & (1 << ('Q' - 'A'))))
			info['Q' - 'A'] = NULL;

		/* apply the filter for the voice '1' */
		voice_filter();

		/* activate the default tablature if not yet done */
		if (first_voice->tblts[0] == 0)
			set_tblt(first_voice);
		break;
	case 'L':
		switch (as->state) {
		case ABC_S_HEAD: {
			int i, auto_len;

			auto_len = as->u.length.base_length < 0;

			for (i = MAXVOICE, p_voice = voice_tb;
			     --i >= 0;
			     p_voice++)
				p_voice->auto_len = auto_len;
			break;
		    }
		case ABC_S_TUNE:
			curvoice->auto_len = as->u.length.base_length < 0;
			break;
		}
		break;
	case 'M':
		get_meter(s);
		break;
	case 'P': {
		struct VOICE_S *curvoice_sav;

		if (as->state != ABC_S_TUNE) {
			info['P' - 'A'] = s;
			break;
		}

		if (!(cfmt.fields[0] & (1 << ('P' - 'A'))))
			break;
		p_voice = &voice_tb[parsys->top_voice];

		/* if not main voice and if voices are synchronized
		 * then, misplaced P: goes in the main voice */ 
		if (curvoice != p_voice
		 && curvoice->time == p_voice->time) {
			curvoice_sav = curvoice;
			curvoice = p_voice;
			sym_link(s, PART);
			curvoice = curvoice_sav;
			break;
		}
		sym_link(s, PART);
		break;
	    }
	case 'Q':
		if (!(cfmt.fields[0] & (1 << ('Q' - 'A'))))
			break;
		if (as->state != ABC_S_TUNE) {
			info['Q' - 'A'] = s;
			break;
		}
		if (curvoice != &voice_tb[parsys->top_voice])
			break;		/* tempo only for first voice */
		s2 = curvoice->last_sym;
		if (s2) {			/* keep last Q: */
			int tim;

			tim = s2->time;
			do {
				if (s2->type == TEMPO) {
					if (!s2->next)
						curvoice->last_sym = s2->prev;
					else
						s2->next->prev = s2->prev;
					if (!s2->prev)
						curvoice->sym = s2->next;
					else
						s2->prev->next = s2->next;
					break;
				}
				s2 = s2->prev;
			} while (s2 && s2->time == tim);
		}
		sym_link(s, TEMPO);
		break;
	case 'r':
	case 's':
		break;
	case 'T':
		if (as->state == ABC_S_GLOBAL)
			break;
		if (as->state == ABC_S_HEAD)		/* in tune header */
			goto addinfo;
		gen_ly(1);				/* in tune */
		p = &as->text[2];
		if (*p != '\0') {
			write_title(s);
			a2b("%% --- + (%s) ---\n", p);
			if (cfmt.pdfmark)
				put_pdfmark(p);
		}
		voice_init();
		reset_gen();		/* (display the time signature) */
		as = get_global_def(as, t);
		break;
	case 'U': {
		unsigned char *deco;

		deco = as->state == ABC_S_GLOBAL ? deco_glob : deco_tune;
		deco[as->u.user.symbol] = deco_intern(as->u.user.value);
		break;
	    }
	case 'u':
		break;
	case 'V':
		get_voice(s);

		/* handle here the possible clef which could be replaced
		 * in case of filter */
		if (as->next && as->next->type == ABC_T_CLEF) {
			as = as->next;
			get_clef((struct SYMBOL *) as);
		}
		if (as->state == ABC_S_TUNE
		 && !curvoice->last_sym
		 && curvoice->time == 0)
			voice_filter();
		break;
	case 'w':
		if (as->state != ABC_S_TUNE)
			break;
		if (!(cfmt.fields[1] & (1 << ('w' - 'a')))) {
			while (as->next) {
				if (as->next->type != ABC_T_INFO
				 || as->next->text[0] != '+')
					break;
				as = as->next;
			}
			break;
		}
		as = get_lyric(as);
		break;
	case 'W':
		if (as->state == ABC_S_GLOBAL)
			break;
		goto addinfo;
	case 'X':
		if (!epsf) {
			buffer_eob();	/* flush stuff left from %% lines */
			write_buffer();
		}
		dfmt = cfmt;		/* save format and info */
		memcpy(&info_glob, &info, sizeof info_glob);
		memcpy(&deco_tune, &deco_glob, sizeof deco_tune);
		info['X' - 'A'] = s;
		if (tune_opts)
			tune_filter(as);
		break;
	default:
		if (info_type >= 'A' && info_type <= 'Z') {
			struct SYMBOL *prev;

addinfo:
			if (!(cfmt.fields[0] & (1 << (info_type - 'A')))
			 && as->state != ABC_S_GLOBAL
			 && as->state != ABC_S_HEAD)
				break;
			prev = info[info_type - 'A'];
			if (!prev
			 || (prev->as.state == ABC_S_GLOBAL
			  && as->state != ABC_S_GLOBAL)) {
				info[info_type - 'A'] = s;
				break;
			}
			while (prev->next)
				prev = prev->next;
			prev->next = s;
			s->prev = prev;
			break;
		}
		if (as->state != ABC_S_GLOBAL)
			error(1, s, "%s info '%c:' not treated",
				state_txt[(int) as->state], info_type);
		break;
	}
	lvlarena(old_lvl);
	return as;
}

/* -- set head type, dots, flags for note -- */
void identify_note(struct SYMBOL *s,
		   int dur,
		   int *p_head,
		   int *p_dots,
		   int *p_flags)
{
	int head, dots, flags;

	if (dur % 12 != 0)
		error(1, s, "Invalid note duration");
	dur /= 12;			/* see BASE_LEN for values */
	if (dur == 0)
		error(1, s, "Note too short");
	for (flags = 5; dur != 0; dur >>= 1, flags--) {
		if (dur & 1)
			break;
	}
	dur >>= 1;
	switch (dur) {
	case 0: dots = 0; break;
	case 1: dots = 1; break;
	case 3: dots = 2; break;
	case 7: dots = 3; break;
	default:
		error(1, s, "Note too much dotted");
		dots = 3;
		break;
	}
	flags -= dots;
	if (flags >= 0)
		head = H_FULL;
	else switch (flags) {
	default:
		error(1, s, "Note too long");
		flags = -4;
		/* fall thru */
	case -4:
		head = H_SQUARE;
		break;
	case -3:
		head = cfmt.squarebreve ? H_SQUARE : H_OVAL;
		break;
	case -2:
		head = H_OVAL;
		break;
	case -1:
		head = H_EMPTY;
		break;
	}
	*p_head = head;
	*p_flags = flags;
	*p_dots = dots;
}

/* -- adjust the duration and time of symbols in a measure when L:auto -- */
static void adjust_dur(struct SYMBOL *s)
{
	struct SYMBOL *s2;
	int time, auto_time;

	/* search the start of the measure */
	s2 = curvoice->last_sym;
	if (!s2)
		return;
	while (s2->type != BAR && s2->prev)
		s2 = s2->prev;
	time = s2->time;
	auto_time = curvoice->time - time;

	/* remove the invisible rest at start of tune */
	if (time == 0) {
		while (s2 && s2->dur == 0)
			s2 = s2->next;
		if (s2 && s2->as.type == ABC_T_REST
		 && (s2->as.flags & ABC_F_INVIS)) {
			time += s2->dur * curvoice->wmeasure / auto_time;
			if (s2->prev)
				s2->prev->next = s2->next;
			else
				curvoice->sym = s2->next;
			if (s2->next)
				s2->next->prev = s2->prev;
			s2 = s2->next;
		}
	}
	while (s2) {
		if (s2->dur != 0) {
			s2->dur = s2->dur * curvoice->wmeasure / auto_time;
			s2->time = time;
			time += s2->dur;
			if (s2->type == NOTEREST) {
				int i, head, dots, nflags;

				for (i = 0; i <= s2->nhd; i++)
					s2->as.u.note.lens[i] = s2->as.u.note.lens[i]
						 * curvoice->wmeasure / auto_time;
				identify_note(s2, s2->as.u.note.lens[0],
						&head, &dots, &nflags);
				s2->head = head;
				s2->dots = dots;
				s2->nflags = nflags;
				if (s2->nflags <= -2)
					s2->as.flags |= ABC_F_STEMLESS;
				else
					s2->as.flags &= ~ABC_F_STEMLESS;
			}
		}
		s2 = s2->next;
	}
	curvoice->time = s->time = time;
}

/* -- measure bar -- */
static void get_bar(struct SYMBOL *s)
{
	int bar_type;
	struct SYMBOL *s2, *s3;

	if (curvoice->norepbra && s->as.u.bar.repeat_bar)
		s->sflags |= S_NOREPBRA;
	if (curvoice->auto_len)
		adjust_dur(s);

	bar_type = s->as.u.bar.type;
	s3 = NULL;
	s2 = curvoice->last_sym;
	if (s2) {

		/* remove the invisible repeat bars when no shift is needed */
		if (bar_type == B_OBRA
		 && (curvoice == &voice_tb[parsys->top_voice]
		  || (parsys->staff[curvoice->staff - 1].flags & STOP_BAR)
		  || (s->sflags & S_NOREPBRA))) {
			if (s2->type == BAR && !s2->as.text) {
				s2->as.text = s->as.text;
				s2->as.u.bar.repeat_bar = s->as.u.bar.repeat_bar;
				s2->sflags |= (s->sflags & S_NOREPBRA);
				s = s2;
				goto gch_build;
			}
		}

		/* merge back-to-back repeat bars */
		if (bar_type == B_LREP && !s->as.text) {
			if (s2->type == BAR
			 && s2->as.u.bar.type == B_RREP) {
				s2->as.u.bar.type = B_DREP;
				return;
			}
		}
	}

	/* link the bar in the voice */
	if (s3) {
		s2 = curvoice->last_sym;
		curvoice->last_sym = s3->prev;
		sym_link(s, BAR);
		s->next = s3;
		s3->prev = s;
		curvoice->last_sym = s2;
	} else {
		sym_link(s, BAR);
	}
	s->staff = curvoice->staff;	/* original staff */

	/* set some flags */
	switch (bar_type) {
	case B_OBRA:
/*	case B_CBRA:			thick bar or end of repeat braket */
	case (B_OBRA << 4) + B_CBRA:
		s->as.flags |= ABC_F_INVIS;
		break;
	case (B_COL << 8) + (B_BAR << 4) + B_COL:
	case (B_COL << 12) + (B_BAR << 8) + (B_BAR << 4) + B_COL:
		bar_type = (B_COL << 4) + B_COL;	/* :|: and :||: -> :: */
		s->as.u.bar.type = bar_type;
		break;
	}
	if ((bar_type & 0xf0) != 0) {
		do {
			bar_type >>= 4;
		} while ((bar_type & 0xf0) != 0);
		if (bar_type == B_COL)
			s->sflags |= S_RRBAR;
	}
	if (s->as.u.bar.dc.n > 0)
		deco_cnv(&s->as.u.bar.dc, s, 0); /* convert the decorations */

	/* build the gch */
gch_build:
	if (s->as.text) {
		if (!s->as.u.bar.repeat_bar) {
			gch_build(s);	/* build the guitar chords */
		} else {
			s->gch = getarena(sizeof *s->gch * 2);
			memset(s->gch, 0, sizeof *s->gch * 2);
			s->gch->type = 'r';
			s->gch->font = REPEATFONT;
			str_font(REPEATFONT);
			s->gch->w = tex_str(s->as.text);
			s->gch->x = 4 + 4;
		}
	}
}

/* -- activate the tablature from the command line '-T' -- */
static void set_tblt(struct VOICE_S *p_voice)
{
	struct tblt_s *tblt;
	int i;

	for (i = 0; i < ncmdtblt; i++) {
		if (!cmdtblts[i].active)
			continue;
		if (cmdtblts[i].vn[0] != '\0') {
			if (strcmp(cmdtblts[i].vn, p_voice->id) != 0
			 && (p_voice->nm == 0
			  || strcmp(cmdtblts[i].vn, p_voice->nm) != 0)
			 && (p_voice->snm == 0
			  || strcmp(cmdtblts[i].vn, p_voice->snm) != 0))
				continue;
		}
		tblt = tblts[cmdtblts[i].index];
		if (p_voice->tblts[0] == tblt
		 || p_voice->tblts[1] == tblt)
			continue;
		if (p_voice->tblts[0] == 0)
			p_voice->tblts[0] = tblt;
		else
			p_voice->tblts[1] = tblt;
	}
}

/* -- do a tune -- */
void do_tune(struct abctune *t)
{
	struct abcsym *as;
	struct SYMBOL *s, *s2;
	int i;

	/* initialize */
	lvlarena(0);
	nstaff = 0;
	staves_found = -1;
	memset(staff_tb, 0, sizeof staff_tb);
	memset(voice_tb, 0, sizeof voice_tb);
	for (i = 0; i < MAXVOICE; i++) {
		voice_tb[i].clef.line = 2;	/* treble clef on 2nd line */
		voice_tb[i].clef.stafflines = 5;
		voice_tb[i].clef.staffscale = 1;
		voice_tb[i].meter.nmeter = 1;
		voice_tb[i].meter.wmeasure = BASE_LEN;
		voice_tb[i].meter.meter[0].top[0] = '4';
		voice_tb[i].meter.meter[0].bot[0] = '4';
		voice_tb[i].wmeasure = BASE_LEN;
		voice_tb[i].scale = 1;
		voice_tb[i].clone = -1;
		voice_tb[i].over = -1;
	}
	curvoice = first_voice = voice_tb;
	micro_tb = t->micro_tb;		/* microtone values */
	abc2win = 0;
	clip_start.bar = -1;
	clip_end.bar = (short unsigned) ~0 >> 1;

	parsys = NULL;
	system_new();			/* create the 1st staff system */
	parsys->top_voice = parsys->voice[0].range = 0;	/* implicit voice */

	if (!epsf) {
		if (cfmt.oneperpage) {
			use_buffer = 0;
			close_page();
		} else {
			use_buffer = !cfmt.splittune;

		}
	} else {
		use_buffer = 1;
		marg_init();
	}

	/* set the duration of all notes/rests
	 *	(this is needed for tuplets and the feathered beams)
	 * and get the voice IDs */
	for (as = t->first_sym; as; as = as->next) {
		switch (as->type) {
		case ABC_T_EOLN:
			if (as->u.eoln.type == 2)
				abc2win = 1;
			break;
		case ABC_T_NOTE:
		case ABC_T_REST:
			s = (struct SYMBOL *) as;
			s->dur = s->as.u.note.lens[0];
			break;
		case ABC_T_INFO:
			if (as->text[0] == 'V') {
				int v;

				v = as->u.voice.voice;
				if (voice_tb[v].id[0] == '\0')
					strcpy(voice_tb[v].id, as->u.voice.id);
			}
			break;
		}
	}

	if (voice_tb[0].id[0] == '\0') {	/* single voice */
		voice_tb[0].id[0] = '1';	/* implicit V:1 */
		voice_tb[0].id[1] = '\0';
	}

	/* scan the tune */
	for (as = t->first_sym; as; as = as->next) {
		s = (struct SYMBOL *) as;
		switch (as->type) {
		case ABC_T_INFO:
			as = get_info(as, t);
			break;
		case ABC_T_PSCOM:
			as = process_pscomment(as);
			break;
		case ABC_T_NOTE:
		case ABC_T_REST:
			if (curvoice->space) {
				curvoice->space = 0;
				s->as.flags |= ABC_F_SPACE;
			}
			get_note(s);
			if (as->flags & ABC_F_LYRIC_START)
				curvoice->lyric_start = s;
			break;
		case ABC_T_BAR:
			if (over_bar)
				get_over(s);
			get_bar(s);
			break;
		case ABC_T_CLEF:
			get_clef(s);
			break;
		case ABC_T_EOLN:
			if (cfmt.breakoneoln
			 || (s->as.flags & ABC_F_SPACE))
				curvoice->space = 1;
			if (cfmt.continueall || cfmt.barsperstaff
			 || as->u.eoln.type == 1)	/* if '\' */
				continue;
			if (as->u.eoln.type == 0	/* if normal eoln */
			 && abc2win
			 && t->abc_vers != (2 << 16))
				continue;
			if (parsys->voice[curvoice - voice_tb].range == 0
			 && curvoice->last_sym)
				curvoice->last_sym->sflags |= S_EOLN;
			if (!cfmt.alignbars)
				continue;
			while (as->next) {		/* treat the lyrics */
				if (as->next->type != ABC_T_INFO)
					break;
				switch (as->next->text[0]) {
				case 'w':
					as = get_info(as->next, t);
					s = (struct SYMBOL *) as;
					continue;
				case 'd':
				case 's':
					as = as->next;
					s = (struct SYMBOL *) as;
					continue;
				}
				break;
			}
			i = (curvoice - voice_tb) + 1;
			if (i < cfmt.alignbars) {
				curvoice = &voice_tb[i];
				continue;
			}
			generate();
			buffer_eob();
			curvoice = &voice_tb[0];
			continue;
		case ABC_T_MREST: {
			int dur;

			dur = curvoice->wmeasure * as->u.bar.len;
			if (curvoice->second) {
				curvoice->time += dur;
				break;
			}
			sym_link(s, MREST);
			s->dur = dur;
			curvoice->time += dur;
			if (s->as.text)
				gch_build(s);	/* build the guitar chords */
			if (s->as.u.bar.dc.n > 0)
				deco_cnv(&s->as.u.bar.dc, s, 0);
			break;
		    }
		case ABC_T_MREP: {
			int n;

			if (!as->next || as->next->type != ABC_T_BAR) {
				error(1, s,
				      "Measure repeat not followed by a bar");
				break;
			}
			if (curvoice->ignore)
				break;
			n = as->u.bar.len;
			if (curvoice->second) {
				curvoice->time += curvoice->wmeasure * n;
				break;
			}
			s2 = sym_add(curvoice, NOTEREST);
			s2->as.type = ABC_T_REST;
			s2->as.linenum = as->linenum;
			s2->as.colnum = as->colnum;
			s2->as.flags |= ABC_F_INVIS;
			s2->dur = curvoice->wmeasure;
			curvoice->time += s2->dur;
			if (n == 1) {
				as->next->u.bar.len = n; /* <n> in the next bar */
				break;
			}
			while (--n > 0) {
				s2 = sym_add(curvoice, BAR);
				s2->as.linenum = as->linenum;
				s2->as.colnum = as->colnum;
				s2->as.u.bar.type = B_SINGLE;
				if (n == as->u.bar.len - 1)
					s2->as.u.bar.len = as->u.bar.len;
				s2 = sym_add(curvoice, NOTEREST);
				s2->as.type = ABC_T_REST;
				s2->as.linenum = as->linenum;
				s2->as.colnum = as->colnum;
				s2->as.flags |= ABC_F_INVIS;
				s2->dur = curvoice->wmeasure;
				curvoice->time += s2->dur;
			}
			break;
		    }
		case ABC_T_V_OVER:
			get_over(s);
			continue;
		case ABC_T_TUPLET:
			set_tuplet(s);
			break;
		default:
			continue;
		}
		if (s->type == 0)
			continue;
		if (curvoice->second)
			s->sflags |= S_SECOND;
		if (curvoice->floating)
			s->sflags |= S_FLOATING;
	}

	gen_ly(0);
	put_history();
	buffer_eob();
	if (epsf)
		write_eps();
	else
		write_buffer();

	if (info['X' - 'A']) {
		memcpy(&cfmt, &dfmt, sizeof cfmt); /* restore format and info */
		memcpy(&info, &info_glob, sizeof info);
		info['X' - 'A'] = NULL;
	}

	/* free the parsing resources */
	{
		struct brk_s *brk, *brk2;

		brk = brks;
		while (brk) {
			brk2 = brk->next;
			free(brk);
			brk = brk2;
		}
		brks = brk;		/* (NULL) */
	}
//	clrarena(2);			/* clear generation */
}

/* check if a K: or M: may go to the tune key and time signatures */
static int is_tune_sig(void)
{
	struct SYMBOL *s;

	if (curvoice->time != 0)
		return 0;		/* not at start of tune */
	for (s = curvoice->sym; s; s = s->next) {
		switch (s->type) {
		case TEMPO:
		case PART:
		case FMTCHG:
			break;
		default:
			return 0;
		}
	}
	return 1;
}

/* -- get a clef definition (in K: or V:) -- */
static void get_clef(struct SYMBOL *s)
{
	struct VOICE_S *p_voice;
	struct SYMBOL *s2;
	int stafflines, voice;
	float staffscale;

	p_voice = curvoice;
	if (s->as.prev->type == ABC_T_INFO) {
		switch (s->as.prev->text[0]) {
		case 'K':
			if (s->as.prev->state != ABC_S_HEAD)
				break;
			if (s->as.u.clef.type >= 0) {
				for (voice = 0; voice < MAXVOICE; voice++) {
					stafflines = parsys->voice[voice].clef.stafflines;
					staffscale = parsys->voice[voice].clef.staffscale;
					memcpy(&parsys->voice[voice].clef, &s->as.u.clef,
					       sizeof parsys->voice[voice].clef);
					parsys->voice[voice].clef.stafflines = stafflines;
					parsys->voice[voice].clef.staffscale = staffscale;
					voice_tb[voice].forced_clef = 1;
					if (s->as.u.clef.type == PERC)
						voice_tb[voice].perc = 1;
				}
			}
			if ((stafflines = s->as.u.clef.stafflines) >= 0) {
				for (voice = 0; voice < MAXVOICE; voice++)
					parsys->voice[voice].clef.stafflines = stafflines;
			}
			if ((staffscale = s->as.u.clef.staffscale) != 0) {
				for (voice = 0; voice < MAXVOICE; voice++)
					parsys->voice[voice].clef.staffscale = staffscale;
			}
			return;
		case 'V':	/* clef relative to a voice definition in the header */
			p_voice = &voice_tb[(int) s->as.prev->u.voice.voice];
			break;
		}
	}
	voice = p_voice - voice_tb;

	if (is_tune_sig()) {
		if ((stafflines = s->as.u.clef.stafflines) < 0)
			stafflines = parsys->voice[voice].clef.stafflines;
		if ((staffscale = s->as.u.clef.staffscale) == 0)
			staffscale = parsys->voice[voice].clef.staffscale;
		if (s->as.u.clef.type >= 0)
			memcpy(&parsys->voice[voice].clef,
				&s->as.u.clef,
				sizeof parsys->voice[voice].clef);
		parsys->voice[voice].clef.stafflines = stafflines;
		parsys->voice[voice].clef.staffscale = staffscale;
	} else {				/* clef change */
		if (s->as.u.clef.type < 0) {	/* if stafflines or staffscale only */
			sym_link(s, CLEF);	/* (will be changed to STAVES in staves_init) */
			return;
		}

		/* the clef must appear before a key signature or a bar */
		s2 = curvoice->last_sym;
		if (s2 && s2->prev
		 && (s2->type == KEYSIG || s2->type == BAR)) {
			struct SYMBOL *s3;

			for (s3 = s2; s3->prev; s3 = s3->prev) {
				switch (s3->prev->type) {
				case KEYSIG:
				case BAR:
					continue;
				}
				break;
			}
			curvoice->last_sym = s3->prev;
			sym_link(s, CLEF);
			s->next = s3;
			s3->prev = s;
			curvoice->last_sym = s2;
		} else {
			sym_link(s, CLEF);
		}
		s->u = 1;			/* small clef */

		/* if #lines or scale, add a clef which will be changed to STAVES
		 * in staves_init */
		if (s->as.u.clef.stafflines >= 0
		 || s->as.u.clef.staffscale != 0) {
			s2 = sym_add(curvoice, CLEF);
			s2->as.linenum = s->as.linenum;
			s2->as.colnum = s->as.colnum;
			s2->as.u.clef.type = -1;
			s2->as.u.clef.stafflines = s->as.u.clef.stafflines;
			s2->as.u.clef.staffscale = s->as.u.clef.staffscale;
		}
	}
	if (s->as.u.clef.type >= 0) {
		p_voice->forced_clef = 1;	/* don't change */
		p_voice->perc = s->as.u.clef.type == PERC;
	}
}

/* -- treat %%clef -- */
static void clef_def(struct SYMBOL *s)
{
	char *p;
	int clef, clef_line;
	char str[80];

	clef = -1;
	clef_line = 2;
	p = &s->as.text[2 + 5];		/* skip %%clef */
	while (isspace((unsigned char) *p))
		p++;

	/* clef name */
	switch (*p) {
	case '\"':			/* user clef name */
		p = get_str(str, p, sizeof str);
		s->as.u.clef.name = (char *) getarena(strlen(str) + 1);
		strcpy(s->as.u.clef.name, str);
		clef = TREBLE;
		break;
	case 'G':
		clef = TREBLE;
		p++;
		break;
	case 'F':
		clef = BASS;
		clef_line = 4;
		p++;
		break;
	case 'C':
		clef = ALTO;
		clef_line = 3;
		p++;
		break;
	case 'P':
		clef = PERC;
		p++;
		break;
	case 't':
		if (strncmp(p, "treble", 6) == 0) {
			clef = TREBLE;
			p += 6;
		}
		if (strncmp(p, "tenor", 5) == 0) {
			clef = ALTO;
			clef_line = 4;
			p += 5;
		}
		break;
	case 'a':
		if (strncmp(p, "alto", 4) == 0) {
			clef = ALTO;
			clef_line = 3;
			p += 4;
		}
		break;
	case 'b':
		if (strncmp(p, "bass", 4) == 0) {
			clef = BASS;
			clef_line = 4;
			p += 4;
		}
		break;
	case 'p':
		if (strncmp(p, "perc", 4) == 0) {
			clef = PERC;
			p += 4;
		}
		break;
	case 'n':
		if (strncmp(p, "none", 4) == 0) {
			clef = TREBLE;
			s->as.u.clef.invis = 1;
			s->as.flags |= ABC_F_INVIS;
			p += 4;
		}
		break;
	}
	if (clef < 0) {
		error(1, s, "Unknown clef '%s'", p);
		return;
	}

	/* clef line */
	switch (*p) {
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
		clef_line = *p++ - '0';
		break;
	}

	/* +/-/^/_8 */
	if (p[1] == '8') {
		switch (*p) {
		case '^':
			s->as.u.clef.transpose = -7;
		case '+':
			s->as.u.clef.octave = 1;
			break;
		case '_':
			s->as.u.clef.transpose = 7;
		case '-':
			s->as.u.clef.octave = -1;
			break;
		}
	}

	/* handle the clef */
	s->as.type = ABC_T_CLEF;
	s->as.u.clef.type = clef;
	s->as.u.clef.line = clef_line;
	s->as.u.clef.stafflines = -1;
	s->as.u.clef.staffscale = 0;
	get_clef(s);
}

/* transpose a key */
static void key_transpose(struct key_s *key)
{
	int t, sf;

	t = curvoice->transpose / 3;
	sf = (t & ~1) + (t & 1) * 7 + key->sf;
	switch ((curvoice->transpose + 210) % 3) {
	case 1:
		sf = (sf + 4 + 12 * 4) % 12 - 4;	/* more sharps */
		break;
	case 2:
		sf = (sf + 7 + 12 * 4) % 12 - 7;	/* more flats */
		break;
	default:
		sf = (sf + 5 + 12 * 4) % 12 - 5;	/* Db, F# or B */
		break;
	}
	key->sf = sf;
}

/* -- set the accidentals when K: with modified accidentals -- */
static void set_acc(struct SYMBOL *s)
{
	int i, j, nacc;
	char accs[8], pits[8];
	static char sharp_tb[8] = {26, 23, 27, 24, 21, 25, 22};
	static char flat_tb[8] = {22, 25, 21, 24, 20, 23, 26};

	if (s->as.u.key.sf > 0) {
		for (nacc = 0; nacc < s->as.u.key.sf; nacc++) {
			accs[nacc] = A_SH;
			pits[nacc] = sharp_tb[nacc];
		}
	} else {
		for (nacc = 0; nacc < -s->as.u.key.sf; nacc++) {
			accs[nacc] = A_FT;
			pits[nacc] = flat_tb[nacc];
		}
	}
	for (i = 0; i < s->as.u.key.nacc; i++) {
		for (j = 0; j < nacc; j++) {
			if ((pits[j] - s->as.u.key.pits[i]) % 7 == 0) {
				accs[j] = s->as.u.key.accs[i];
				break;
			}
		}
		if (j == nacc) {
			accs[j] = s->as.u.key.accs[i];
			pits[j] = s->as.u.key.pits[i];
			nacc++;		/* cannot overflow */
		}
	}
	for (i = 0; i < nacc; i++) {
		s->as.u.key.accs[i] = accs[i];
		s->as.u.key.pits[i] = pits[i];
	}
	s->as.u.key.nacc = nacc;
}

/* -- get a key signature definition (K:) -- */
static void get_key(struct SYMBOL *s)
{
	struct VOICE_S *p_voice;
	struct SYMBOL *s2;
	struct key_s okey;			/* original key */
	int i;

	if (s->as.u.key.octave != NO_OCTAVE)
		curvoice->octave = s->as.u.key.octave;
	if (s->as.u.key.empty == 1)		/* clef only */
		return;

	if (s->as.u.key.sf != 0
	 && !s->as.u.key.exp
	 && s->as.u.key.nacc != 0)
		set_acc(s);

	memcpy(&okey, &s->as.u.key, sizeof okey);
	if (s->as.state == ABC_S_HEAD)		/* if first K: (start of tune) */
		curvoice->transpose = cfmt.transpose;
	if (curvoice->transpose != 0) {
		key_transpose(&s->as.u.key);

#if 0
		/* transpose explicit accidentals */
//fixme: not correct - transpose adds or removes accidentals...
		if (s->as.u.key.nacc > 0) {
			struct VOICE_S voice, *voice_sav;
			struct SYMBOL note;

			memset(&voice, 0, sizeof voice);
			voice.transpose = curvoice->transpose;
			memcpy(&voice.ckey, &s->as.u.key, sizeof voice.ckey);
			voice.ckey.empty = 2;
			voice.ckey.nacc = 0;
			memset(&note, 0, sizeof note);
			memcpy(note.as.u.note.pits, voice.ckey.pits,
					sizeof note.as.u.note.pits);
			memcpy(note.as.u.note.accs, voice.ckey.accs,
					sizeof note.as.u.note.accs);
			note.as.u.note.nhd = s->as.u.key.nacc;
			voice_sav = curvoice;
			curvoice = &voice;
			note_transpose(&note);
			memcpy(s->as.u.key.pits, note.as.u.note.pits,
					sizeof s->as.u.key.pits);
			memcpy(s->as.u.key.accs, note.as.u.note.accs,
					sizeof s->as.u.key.accs);
			curvoice = voice_sav;
		}
#endif
	}

	if (s->as.state == ABC_S_HEAD) {	/* start of tune */
		for (i = MAXVOICE, p_voice = voice_tb;
		     --i >= 0;
		     p_voice++) {
			memcpy(&p_voice->key, &s->as.u.key,
						sizeof p_voice->key);
			memcpy(&p_voice->ckey, &s->as.u.key,
						sizeof p_voice->ckey);
			memcpy(&p_voice->okey, &okey,
						sizeof p_voice->okey);
			if (s->as.u.key.octave != NO_OCTAVE)
				p_voice->octave = s->as.u.key.octave;
		}
		return;
	}

	/* ABC_S_TUNE (K: cannot be ABC_S_GLOBAL) */
	if (is_tune_sig()) {

		/* define the starting key signature */
		memcpy(&curvoice->key, &s->as.u.key,
					sizeof curvoice->key);
		memcpy(&curvoice->ckey, &s->as.u.key,
					sizeof curvoice->ckey);
		memcpy(&curvoice->okey, &okey,
					sizeof curvoice->okey);
		if (curvoice->key.mode >= BAGPIPE
		 && curvoice->posit.std == 0)
			curvoice->posit.std = SL_BELOW;
		curvoice->transpose = cfmt.transpose;
		if (curvoice->key.empty)
			curvoice->key.sf = 0;
		return;
	}

	/* key signature change */
	if ((!s->as.next
	  || s->as.next->type != ABC_T_CLEF)	/* if not explicit clef */
	 && curvoice->ckey.sf == s->as.u.key.sf	/* and same key */
	 && curvoice->ckey.nacc == 0
	 && s->as.u.key.nacc == 0
	 && curvoice->ckey.empty == s->as.u.key.empty
	 && cfmt.keywarn)			/* (if not key warning,
						 *  keep all key signatures) */
		return;				/* ignore */

	if (!curvoice->ckey.empty)
		s->u = curvoice->ckey.sf;	/* previous key signature */
	memcpy(&curvoice->ckey, &s->as.u.key,
				sizeof curvoice->ckey);
	memcpy(&curvoice->okey, &okey,
				sizeof curvoice->okey);
	if (s->as.u.key.empty)
		s->as.u.key.sf = 0;

	/* the key signature must appear before a time signature */
	s2 = curvoice->last_sym;
	if (s2 && s2->type == TIMESIG) {
		curvoice->last_sym = s2->prev;
		if (!curvoice->last_sym)
			curvoice->sym = NULL;
		sym_link(s, KEYSIG);
		s->next = s2;
		s2->prev = s;
		curvoice->last_sym = s2;
	} else {
		sym_link(s, KEYSIG);
	}
}

/* -- set meter from M: -- */
static void get_meter(struct SYMBOL *s)
{
	struct VOICE_S *p_voice;
	int i;

	switch (s->as.state) {
	case ABC_S_GLOBAL:
		/*fixme: keep the values and apply to all tunes?? */
		break;
	case ABC_S_HEAD:
		for (i = MAXVOICE, p_voice = voice_tb;
		     --i >= 0;
		     p_voice++) {
			memcpy(&p_voice->meter, &s->as.u.meter,
			       sizeof p_voice->meter);
			p_voice->wmeasure = s->as.u.meter.wmeasure;
		}
		break;
	case ABC_S_TUNE:
		curvoice->wmeasure = s->as.u.meter.wmeasure;
		if (is_tune_sig()) {
			memcpy(&curvoice->meter, &s->as.u.meter,
				       sizeof curvoice->meter);
			reset_gen();	/* (display the time signature) */
			break;
		}
		if (s->as.u.meter.nmeter == 0)
			break;		/* M:none */
		sym_link(s, TIMESIG);
		break;
	}
}

/* -- treat a 'V:' -- */
static void get_voice(struct SYMBOL *s)
{
	struct VOICE_S *p_voice;
	int voice;

	voice = s->as.u.voice.voice;
	p_voice = &voice_tb[voice];
	if (parsys->voice[voice].range < 0) {
		if (cfmt.alignbars) {
			error(1, s, "V: does not work with %%%%alignbars");
		}
		if (staves_found < 0) {
			if (!s->as.u.voice.merge) {
#if MAXSTAFF < MAXVOICE
				if (nstaff >= MAXSTAFF - 1) {
					error(1, s, "Too many staves");
					return;
				}
#endif
				nstaff++;
			} else {
				p_voice->second = 1;
				parsys->voice[voice].second = 1;
			}
			p_voice->staff = p_voice->cstaff = nstaff;
			parsys->voice[voice].staff = nstaff;
			{
				int range, i;

				range = 0;
				for (i = 0; i < MAXVOICE; i++) {
					if (parsys->voice[i].range > range)
						range = parsys->voice[i].range;
				}
				parsys->voice[voice].range = range + 1;
				voice_link(p_voice);
			}
		} else {
			p_voice->ignore = 1;
			p_voice->staff = p_voice->cstaff = nstaff + 1;
		}
	}

	/* if something has changed, update */
	if (s->as.u.voice.octave != NO_OCTAVE)
		p_voice->octave = s->as.u.voice.octave;
	if (s->as.u.voice.fname != 0) {
		p_voice->nm = s->as.u.voice.fname;
		p_voice->new_name = 1;
	}
	if (s->as.u.voice.nname != 0)
		p_voice->snm = s->as.u.voice.nname;
	switch (s->as.u.voice.dyn) {
	case 1:
		p_voice->posit.dyn = SL_ABOVE;
		p_voice->posit.vol = SL_ABOVE;
		break;
	case -1:
		p_voice->posit.dyn = SL_BELOW;
		p_voice->posit.vol = SL_BELOW;
		break;
	}
	switch (s->as.u.voice.lyrics) {
	case 1:
		p_voice->posit.voc = SL_ABOVE;
		break;
	case -1:
		p_voice->posit.voc = SL_BELOW;
		break;
	}
	switch (s->as.u.voice.gchord) {
	case 1:
		p_voice->posit.gch = SL_ABOVE;
		break;
	case -1:
		p_voice->posit.gch = SL_BELOW;
		break;
	}
	switch (s->as.u.voice.stem) {
	case 1:
		p_voice->posit.std = SL_ABOVE;
		break;
	case -1:
		p_voice->posit.std = SL_BELOW;
		break;
	case 2:
		p_voice->posit.std = 0;		/* auto */
		break;
	}
	switch (s->as.u.voice.gstem) {
	case 1:
		p_voice->posit.gsd = SL_ABOVE;
		break;
	case -1:
		p_voice->posit.gsd = SL_BELOW;
		break;
	case 2:
		p_voice->posit.gsd = 0;		/* auto */
		break;
	}
	if (s->as.u.voice.scale != 0)
		p_voice->scale = s->as.u.voice.scale;

	set_tblt(p_voice);

	/* if in tune, switch to this voice */
	if (s->as.state == ABC_S_TUNE)
		curvoice = p_voice;
}

/* sort the notes of the chord by pitch (lowest first) */
void sort_pitch(struct SYMBOL *s, int combine)
{
	int i, nx, k;
	for (;;) {
		nx = 0;
		for (i = 1; i <= s->nhd; i++) {
			if (s->as.u.note.pits[i] >= s->as.u.note.pits[i-1])
				continue;
#define xch(f) \
	k = s->as.u.note.f[i]; \
	s->as.u.note.f[i] = s->as.u.note.f[i-1]; \
	s->as.u.note.f[i-1] = k
			xch(pits);
			xch(lens);
			xch(accs);
			xch(sl1);
			xch(sl2);
			xch(ti1);
			xch(decs);
#undef xch
			if (combine) {
				k = s->pits[i];
				s->pits[i] = s->pits[i-1];
				s->pits[i-1] = k;
			}
			nx++;
		}
		if (nx == 0)
			break;
	}
}

/* -- note or rest -- */
static void get_note(struct SYMBOL *s)
{
	struct SYMBOL *prev;
	int i, m;

	prev = curvoice->last_sym;
	s->nhd = m = s->as.u.note.nhd;

	if (curvoice->octave != 0) {
		for (i = 0; i <= m; i++)
			s->as.u.note.pits[i] += curvoice->octave * 7;
	}

	if (curvoice->perc)
		s->sflags |= S_PERC;
	else if (s->as.type == ABC_T_NOTE
	      && curvoice->transpose != 0)
		note_transpose(s);

	if (!(s->as.flags & ABC_F_GRACE)) {
		switch (curvoice->posit.std) {
		case SL_ABOVE: s->stem = 1; break;
		case SL_BELOW: s->stem = -1; break;
		}
	} else {			/* grace note - adjust its duration */
		int div;

		switch (curvoice->posit.gsd) {
		case SL_ABOVE: s->stem = 1; break;
		case SL_BELOW: s->stem = -1; break;
		}
		if (curvoice->key.mode < BAGPIPE) {
			div = 4;
			if (!prev
			 || !(prev->as.flags & ABC_F_GRACE)) {
				if (s->as.flags & ABC_F_GR_END)
					div = 2;	/* one grace note */
			}
		} else {
			div = 8;
		}
		for (i = 0; i <= m; i++)
			s->as.u.note.lens[i] /= div;
		s->dur /= div;
	}

	/* convert the decorations
	 * (!beam-accel! and !beam-rall! may change the note duration) */
	if (s->as.u.note.dc.n > 0)
		deco_cnv(&s->as.u.note.dc, s, prev);

	/* insert the note/rest in the voice */
	sym_link(s,  s->as.u.note.lens[0] != 0 ? NOTEREST : SPACE);
	if (!(s->as.flags & ABC_F_GRACE))
		curvoice->time += s->dur;
	s->nohdix = -1;

	/* change the figure of whole measure rests */
	if (s->as.type == ABC_T_REST) {
		if (s->dur == curvoice->wmeasure) {
			if (s->dur < BASE_LEN * 2)
				s->as.u.note.lens[0] = BASE_LEN;
			else if (s->dur < BASE_LEN * 4)
				s->as.u.note.lens[0] = BASE_LEN * 2;
			else
				s->as.u.note.lens[0] = BASE_LEN * 4;
		}
	} else {

		/* sort the notes of the chord by pitch (lowest first) */
		sort_pitch(s, 0);
	}

	memcpy(s->pits, s->as.u.note.pits, sizeof s->pits);

	/* get the max head type, number of dots and number of flags */
	{
		int head, dots, nflags, l;

		if ((l = s->as.u.note.lens[0]) != 0) {
			identify_note(s, l, &head, &dots, &nflags);
			s->head = head;
			s->dots = dots;
			s->nflags = nflags;
			for (i = 1; i <= m; i++) {
				if (s->as.u.note.lens[i] == l)
					continue;
				identify_note(s, s->as.u.note.lens[i],
						&head, &dots, &nflags);
				if (head > s->head)
					s->head = head;
				if (dots > s->dots)
					s->dots = dots;
				if (nflags > s->nflags)
					s->nflags = nflags;
			}
			if (s->sflags & S_XSTEM)
				s->nflags = 0;		/* word start+end */
		}
	}
	if (s->nflags <= -2)
		s->as.flags |= ABC_F_STEMLESS;

	if (s->sflags & (S_TREM1 | S_TREM2)) {
		if (s->nflags > 0)
			s->nflags += s->u;
		else
			s->nflags = s->u;
		if ((s->sflags & S_TREM2)
		 && (s->sflags & S_BEAM_END)) {		/* if 2nd note - see deco.c */
			prev->head = s->head;
			prev->u = s->u;
			prev->nflags = s->nflags;
			prev->as.flags |= (s->as.flags & ABC_F_STEMLESS);
		}
	}

	for (i = 0; i <= m; i++) {
		if (s->as.u.note.sl1[i] != 0)
			s->sflags |= S_SL1;
		if (s->as.u.note.sl2[i] != 0)
			s->sflags |= S_SL2;
		if (s->as.u.note.ti1[i] != 0)
			s->sflags |= S_TI1;
	}

	switch (cfmt.shiftunison) {
	case 0:
		break;
	case 1:
		s->sflags |= S_SHIFTUNISON_1;
		break;
	case 2:
		s->sflags |= S_SHIFTUNISON_2;
		break;
	default:
		s->sflags |= S_SHIFTUNISON_1 | S_SHIFTUNISON_2;
		break;
	}

	/* build the guitar chords */
	if (s->as.text)
		gch_build(s);
}

/* -- treat a postscript definition -- */
static void ps_def(struct SYMBOL *s,
			char *p,
			char use)	/* cf user_ps_add() */
{
	if (svg == 0 && epsf != 2) {	/* if PS output */
		if (secure
		 || use == 'g'
		 || use == 's')
			return;
	} else {			/* if SVG output */
		if (use == 'p'
		 || (use == 'g' && file_initialized))
			return;
	}
	if (s->as.prev)
		s->as.state = s->as.prev->state;
	if (s->as.state == ABC_S_TUNE) {
		if (use == 'g')
			return;
		sym_link(s, FMTCHG);
		s->u = PSSEQ;
		s->as.text = p;
		s->as.flags |= ABC_F_INVIS;
#if 0
		if (s->prev && (s->prev->sflags & S_EOLN)) {
			s->sflags |= S_EOLN;
			s->prev->sflags &= ~S_EOLN;
		}
#endif
		return;
	}
	if (file_initialized || mbf != outbuf)
		a2b("%s\n", p);
	else
		user_ps_add(p, use);
}

/* get a symbol selection */
/* measure_number [ ":" time_numerator "/" time_denominator ] */
static char *get_symsel(struct symsel_s *symsel, char *p)
{
	char *q;
	int tn, td, n;

	symsel->bar = strtod(p, &q);
	if (*q >= 'a' && *q <= 'z')
		symsel->seq = *q++ - 'a';
	else
		symsel->seq = 0;
	if (*q == ':') {
		if (sscanf(q + 1, "%d/%d%n", &tn, &td, &n) != 2
		 || td <= 0)
			return 0;
		symsel->time = BASE_LEN * tn / td;
		q += 1 + n;
	} else {
		symsel->time = 0;
	}
	return q;
}

/* free the voice options */
static void free_voice_opt(struct voice_opt_s *opt)
{
	struct voice_opt_s *opt2;

	while (opt) {
		opt2 = opt->next;
		free(opt);
		opt = opt2;
	}
}

/* -- process a pseudo-comment (%% or I:) -- */
static struct abcsym *process_pscomment(struct abcsym *as)
{
	char w[32], *p, *q;
	int lock;
	float h1;
	struct SYMBOL *s = (struct SYMBOL *) as;

	p = as->text + 2;		/* skip '%%' */
	q = p + strlen(p) - 5;
	lock = strncmp(q, " lock", 5) == 0;
	if (lock)
		*q = '\0'; 
	p = get_str(w, p, sizeof w);
	if (as->state == ABC_S_HEAD
	 && !check_header(as)) {
		error(1, s, "Cannot have %%%%%s in tune header", w);
		return as;
	}
	switch (w[0]) {
	case 'b':
		if (strcmp(w, "beginps") == 0
		 || strcmp(w, "beginsvg") == 0) {
			char use;

			if (w[5] == 'p') {
				if (strncmp(p, "svg", 3) == 0)
					use = 's';
				else if (strncmp(p, "nosvg", 5) == 0)
					use = 'p';
				else
					use = 'b';
			} else {
				use = 'g';
			}
			p = as->text + 2 + 7;
			while (*p != '\0' && *p != '\n')
				p++;
			if (*p == '\0')
				return as;		/* empty */
			ps_def(s, p + 1, use);
			return as;
		}
		if (strcmp(w, "begintext") == 0) {
			int job;

			if (as->state == ABC_S_TUNE) {
				gen_ly(1);
			} else if (as->state == ABC_S_GLOBAL) {
				if (epsf || in_fname == 0)
					return as;
			}
			p = as->text + 2 + 9;
			while (*p == ' ' || *p == '\t')
				p++;
			if (*p != '\n') {
				job = get_textopt(p);
				if (job < 0) {
					error(1, s,
						"Bad argument for begintext: %s", p);
					job = T_LEFT;
				}
				while (*p != '\0' && *p != '\n')
					p++;
				if (*p == '\0')
					return as;	/* empty */
			} else {
				job = cfmt.textoption;
			}
			if (job != T_SKIP) {
				p++;
				write_text(w, p, job);
			}
			return as;
		}
		if (strcmp(w, "break") == 0) {
			struct brk_s *brk;

			if (as->state != ABC_S_TUNE) {
				error(1, s, "%%%%%s ignored", w);
				return as;
			}
			if (*p == '\0')
				return as;
			for (;;) {
				brk = malloc(sizeof *brk);
				p = get_symsel(&brk->symsel, p);
				if (!p) {
					error(1, s, "Bad selection in %%%%%s", w);
					return as;
				}
				brk->next = brks;
				brks = brk;
				if (*p != ',' && *p != ' ')
					break;
				p++;
			}
			return as;
		}
		break;
	case 'c':
		if (strcmp(w, "center") == 0)
			goto center;
		if (strcmp(w, "clef") == 0) {
			if (as->state != ABC_S_GLOBAL)
				clef_def(s);
			return as;
		}
		if (strcmp(w, "clip") == 0) {
			if (!cur_tune_opts) {
				error(1, s, "No %%%%tune before %%%%%s", w);
				return as;
			}

			/* %%clip <symbol selection> "-" <symbol selection> */
			if (*p != '-') {
				p = get_symsel(&clip_start, p);
				if (!p) {
					error(1, s, "Bad start in %%%%%s", w);
					return as;
				}
				if (*p != '-') {
					error(1, s, "Lack of '-' in %%%%%s", w);
					return as;
				}
			}
			p++;
			p = get_symsel(&clip_end, p);
			if (!p) {
				error(1, s, "Bad end in %%%%%s", w);
				return as;
			}
			if (clip_start.bar < 0)
				clip_start.bar = 0;
			if (clip_end.bar < clip_start.bar
			 || (clip_end.bar == clip_start.bar
			  && clip_end.time <= clip_start.time)) {
				clip_end.bar = (short unsigned) ~0 >> 1;
			}
			return as;
		}
		break;
	case 'd':
		if (strcmp(w, "deco") == 0) {
			deco_add(p);
			return as;
		}
		if (strcmp(w, "dynamic") == 0) {
			set_voice_param(curvoice, as->state, w, p);
			return as;
		}
		break;
	case 'E':
		if (strcmp(w, "EPS") == 0) {
			float x1, y1, x2, y2;
			FILE *fp;
			char fn[STRL1], line[STRL1];

			gen_ly(1);
			if (secure
			 || cfmt.textoption == T_SKIP)
				return as;
			get_str(line, p, sizeof line);
			if ((fp = open_file(line, "eps", fn)) == 0) {
				error(1, s, "No such file: %s", line);
				return as;
			}

			/* get the bounding box */
			x1 = x2 = 0;
			while (fgets(line, sizeof line, fp)) {
				if (strncmp(line, "%%BoundingBox:", 14) == 0) {
					if (sscanf(&line[14], "%f %f %f %f",
						   &x1, &y1, &x2, &y2) == 4)
						break;
				}
			}
			fclose(fp);
			if (x1 == x2) {
				error(1, s,
				      "No bounding box in '%s'", fn);
				return as;
			}
			if (cfmt.textoption == T_CENTER
			 || cfmt.textoption == T_RIGHT) {
				float lw;

				lw = ((cfmt.landscape ? cfmt.pageheight : cfmt.pagewidth)
					- cfmt.leftmargin - cfmt.rightmargin) / cfmt.scale;
				if (cfmt.textoption == T_CENTER)
					x1 += (lw - (x2 - x1)) * 0.5;
				else
					x1 += lw - (x2 - x1);
			}
			a2b("\001");	/* include file (must be the first after eob) */
			bskip(y2 - y1);
			a2b("%.2f %.2f%%%s\n", x1, -y1, fn);
			buffer_eob();
			return as;
		}
		break;
	case 'g':
		if (strcmp(w, "gchord") == 0
		 || strcmp(w, "gstemdir") == 0) {
			set_voice_param(curvoice, as->state, w, p);
			return as;
		}
		if (strcmp(w, "glyph") == 0) {
			glyph_add(p);
			return as;
		}
		break;
	case 'm':
		if (strcmp(w, "maxsysstaffsep") == 0) {
			if (as->state != ABC_S_TUNE)
				break;
			parsys->voice[curvoice - voice_tb].maxsep = scan_u(p);
			return as;
		}
		if (strcmp(w, "measrep") == 0)
			goto irepeat;
		if (strcmp(w, "multicol") == 0) {
			float bposy;

			generate();
			if (strncmp(p, "start", 5) == 0) {
				if (!in_page)
					a2b("%%\n");	/* initialize the output */
				buffer_eob();
				bposy = get_bposy();
				multicol_max = multicol_start = bposy;
				lmarg = cfmt.leftmargin;
				rmarg = cfmt.rightmargin;
				mc_in_tune = info['X' - 'A'] != 0;
			} else if (strncmp(p, "new", 3) == 0) {
				if (multicol_start == 0) {
					error(1, s,
					      "%%%s new without start", w);
				} else {
					buffer_eob();
					bposy = get_bposy();
					if (bposy < multicol_start)
						bskip((bposy - multicol_start)
								/ cfmt.scale);
					if (bposy < multicol_max)
						multicol_max = bposy;
					cfmt.leftmargin = lmarg;
					cfmt.rightmargin = rmarg;
				}
			} else if (strncmp(p, "end", 3) == 0) {
				if (multicol_start == 0) {
					error(1, s,
					      "%%%s end without start", w);
				} else {
					buffer_eob();
					bposy = get_bposy();
					if (bposy > multicol_max)
						bskip((bposy - multicol_max)
								/ cfmt.scale);
					else
						a2b("%%\n");	/* force write_buffer */
					if (mc_in_tune == (info['X' - 'A'] != 0)) {
						cfmt.leftmargin = lmarg;
						cfmt.rightmargin = rmarg;
					} else if (!mc_in_tune) {
						dfmt.leftmargin = lmarg;
						dfmt.rightmargin = rmarg;
					}
					multicol_start = 0;
					buffer_eob();
					if (!info['X' - 'A']
					 && !epsf)
						write_buffer();
				}
			} else {
				error(1, s,
				      "Unknown keyword '%s' in %%%s", p, w);
			}
			return as;
		}
		break;
	case 'n':
		if (strcmp(w, "newpage") == 0) {
			if (epsf || in_fname == 0)
				return as;
			if (as->state == ABC_S_TUNE)
				generate();
			buffer_eob();
			write_buffer();
			use_buffer = 0;
			if (isdigit((unsigned char) *p))
				pagenum = atoi(p);
			close_page();
			return as;
		}
		break;
	case 'p':
		if (strcmp(w, "ps") == 0
		 || strcmp(w, "postscript") == 0) {
			ps_def(s, p, 'b');
			return as;
		}
		break;
	case 'o':
		if (strcmp(w, "ornament") == 0) {
			set_voice_param(curvoice, as->state, w, p);
			return as;
		}
		break;
	case 'r':
		if (strcmp(w, "repbra") == 0) {
			if (as->state != ABC_S_TUNE)
				return as;
			curvoice->norepbra = !atoi(p);
			return as;
		}
		if (strcmp(w, "repeat") == 0) {
			int n, k;

irepeat:
			if (!curvoice->last_sym) {
				error(1, s,
				      "%%%s cannot start a tune", w);
				return as;
			}
			if (*p == '\0') {
				n = 1;
				k = 1;
			} else {
				n = atoi(p);
				if (n < 1
				 || (curvoice->last_sym->type == BAR
					&& n > 2)) {
					error(1, s,
					      "Incorrect 1st value in %%%s", w);
					return as;
				}
				while (*p != '\0' && !isspace((unsigned char) *p))
					p++;
				while (isspace((unsigned char) *p))
					p++;
				if (*p == '\0') {
					k = 1;
				} else {
					k = atoi(p);
					if (k < 1) {
//					 || (curvoice->last_sym->type == BAR
//					  && n == 2
//					  && k > 1)) {
						error(1, s,
						      "Incorrect 2nd value in %%%s", w);
						return as;
					}
				}
			}
			s->u = REPEAT;
			if (curvoice->last_sym->type == BAR)
				s->doty = n;
			else
				s->doty = -n;
			sym_link(s, FMTCHG);
			s->nohdix = k;
			as->text = NULL;
			return as;
		}
		break;
	case 's':
		if (strcmp(w, "setbarnb") == 0) {
			if (as->state == ABC_S_TUNE) {
				struct abcsym *as2;
				int n;

				n = atoi(p);
				for (as2 = as->next; as2; as2 = as2->next) {
					if (as2->type == ABC_T_BAR) {
						s = (struct SYMBOL *) as2;
						s->u = n;
						break;
					}
				}
				return as;
			}
			strcpy(w, "measurefirst");
			break;
		}
		if (strcmp(w, "sep") == 0) {
			float h2, len, lwidth;

			if (as->state == ABC_S_TUNE) {
				gen_ly(0);
			} else if (as->state == ABC_S_GLOBAL) {
				if (epsf || in_fname == 0)
					return as;
			}
			lwidth = (cfmt.landscape ? cfmt.pageheight : cfmt.pagewidth)
				- cfmt.leftmargin - cfmt.rightmargin;
			h1 = h2 = len = 0;
			if (*p != '\0') {
				h1 = scan_u(p);
				while (*p != '\0' && !isspace((unsigned char) *p))
					p++;
				while (isspace((unsigned char) *p))
					p++;
			}
			if (*p != '\0') {
				h2 = scan_u(p);
				while (*p != '\0' && !isspace((unsigned char) *p))
					p++;
				while (isspace((unsigned char) *p))
					p++;
			}
			if (*p != '\0')
				len = scan_u(p);
			if (h1 < 1)
				h1 = 0.5 CM;
			if (h2 < 1)
				h2 = h1;
			if (len < 1)
				len = 3.0 CM;
			bskip(h1);
			a2b("%.1f %.1f sep0\n",
			     len / cfmt.scale,
			     (lwidth - len) * 0.5 / cfmt.scale);
			bskip(h2);
			buffer_eob();
			return as;
		}
		if (strcmp(w, "staff") == 0) {
			int staff;

			if (as->state != ABC_S_TUNE)
				return as;
			if (*p == '+')
				staff = curvoice->cstaff + atoi(p + 1);
			else if (*p == '-')
				staff = curvoice->cstaff - atoi(p + 1);
			else
				staff = atoi(p) - 1;
			if ((unsigned) staff > (unsigned) nstaff) {
				error(1, s, "Bad staff in %%%s", w);
				return as;
			}
			curvoice->floating = 0;
			curvoice->cstaff = staff;
			return as;
		}
		if (strcmp(w, "staffbreak") == 0) {
			if (as->state != ABC_S_TUNE)
				return as;
			if (isdigit(*p)) {
				s->xmx = scan_u(p);
				if (s->xmx <= 0) {
					error(1, s, "Bad value in %%%s", w);
					return as;
				}
				if (p[strlen(p) - 1] == 'f')
					s->doty = 1;
			} else {
				s->xmx = 0.5 CM;
				if (*p == 'f')
					s->doty = 1;
			}
			sym_link(s, STBRK);
			return as;
		}
		if (strcmp(w, "stafflines") == 0) {
			int voice, lines;

			lines = atoi(p);
			if ((unsigned) lines >= 10) {
				error(1, s, "Bad value in %%%s", w);
				return as;
			}
			if (as->state != ABC_S_TUNE) {
				for (voice = 0; voice < MAXVOICE; voice++)
					parsys->voice[voice].clef.stafflines = lines;
			} else {
				voice = curvoice - voice_tb;
				parsys->voice[voice].clef.stafflines = lines;
			}
			return as;
		}
		if (strcmp(w, "staffscale") == 0) {
			int voice;
			char *q;
			float scale;

			scale = strtod(p, &q);
			if (scale <= 0.5 || scale > 2
			 || (*q != '\0' && *q != ' ')) {
				error(1, s, "Bad value in %%%s", w);
				return as;
			}
			if (as->state != ABC_S_TUNE) {
				for (voice = 0; voice < MAXVOICE; voice++)
					parsys->voice[voice].clef.staffscale = scale;
			} else {
				voice = curvoice - voice_tb;
				parsys->voice[voice].clef.staffscale = scale;
			}
			return as;
		}
		if (strcmp(w, "staves") == 0
		 || strcmp(w, "score") == 0) {
			if (as->state == ABC_S_GLOBAL)
				return as;
			get_staves(s);
			return as;
		}
		if (strcmp(w, "stemdir") == 0) {
			set_voice_param(curvoice, as->state, w, p);
			return as;
		}
		if (strcmp(w, "sysstaffsep") == 0) {
			if (as->state != ABC_S_TUNE)
				break;
			parsys->voice[curvoice - voice_tb].sep = scan_u(p);
			return as;
		}
		break;
	case 't':
		if (strcmp(w, "text") == 0) {
			int job;

center:
			if (as->state == ABC_S_TUNE) {
				gen_ly(1);
			} else if (as->state == ABC_S_GLOBAL) {
				if (epsf || in_fname == 0)
					return as;
			}
			if (w[0] == 'c') {
				job = T_CENTER;
			} else {
				job = cfmt.textoption;
				switch(job) {
				case T_SKIP:
					return as;
				case T_LEFT:
				case T_RIGHT:
				case T_CENTER:
					break;
				default:
					job = T_LEFT;
					break;
				}
			}
			write_text(w, p, job);
			return as;
		}
		if (strcmp(w, "tablature") == 0) {
			struct tblt_s *tblt;
			int i, j;

			tblt = tblt_parse(p);
			if (tblt == 0)
				return as;

			switch (as->state) {
			case ABC_S_TUNE:
				for (i = 0; i < ncmdtblt; i++) {
					if (cmdtblts[i].active)
						continue;
					j = cmdtblts[i].index;
					if (j < 0 || tblts[j] == tblt)
						return as;
				}
				/* !! 2 tblts per voice !! */
				if (curvoice->tblts[0] == tblt
				 || curvoice->tblts[1] == tblt)
					break;
				if (curvoice->tblts[1] != 0) {
					error(1, s,
						"Too many tablatures for voice %s",
						curvoice->id);
					break;
				}
				if (curvoice->tblts[0] == 0)
					curvoice->tblts[0] = tblt;
				else
					curvoice->tblts[1] = tblt;
				break;
			}
			return as;
		}
		if (strcmp(w, "transpose") == 0) {
			struct SYMBOL *s2;

			interpret_fmt_line(w, p, 0);
			switch (as->state) {
			case ABC_S_GLOBAL:
				return as;
			case ABC_S_HEAD: {
				struct VOICE_S *p_voice;
				int i;

				for (i = MAXVOICE, p_voice = voice_tb;
				     --i >= 0;
				     p_voice++) {
					p_voice->transpose = cfmt.transpose;
					memcpy(&p_voice->key, &p_voice->okey,
						sizeof p_voice->key);
					key_transpose(&p_voice->key);
					memcpy(&p_voice->ckey, &p_voice->key,
						sizeof p_voice->ckey);
					if (p_voice->key.empty)
						p_voice->key.sf = 0;
				}
				return as;
			    }
			}
			curvoice->transpose = cfmt.transpose;
			s2 = curvoice->sym;
			if (!s2) {
				memcpy(&curvoice->key, &curvoice->okey,
					sizeof curvoice->key);
				key_transpose(&curvoice->key);
				memcpy(&curvoice->ckey, &curvoice->key,
					sizeof curvoice->ckey);
				if (curvoice->key.empty)
					curvoice->key.sf = 0;
				return as;
			}
			for ( ; s2; s2 = s2->prev)
				if (s2->type == KEYSIG)
					break;
			if (!s2 || s2->time != curvoice->time) {
				s = sym_add(curvoice, KEYSIG);
				s->as.type = ABC_T_INFO;
				s->as.text = (char *) getarena(2);
				s->as.text[0] = 'K';
				s->as.text[1] = '\0';
				s->u = curvoice->ckey.sf;
				s2 = s;
			}
			memcpy(&s2->as.u.key, &curvoice->okey,
						sizeof s2->as.u.key);
			key_transpose(&s2->as.u.key);
			memcpy(&curvoice->ckey, &s->as.u.key,
						sizeof curvoice->ckey);
			if (curvoice->key.empty)
				curvoice->key.sf = 0;
			return as;
		}
		if (strcmp(w, "tune") == 0) {
			struct abcsym *as2, *as3;
			struct tune_opt_s *opt, *opt2;

			if (as->state != ABC_S_GLOBAL) {
				error(1, s, "%%%%%s ignored", w);
				return as;
			}

			/* if void %%tune, remove all tune options */
			if (*p == '\0') {
				opt = tune_opts;
				while (opt) {
					free_voice_opt(opt->voice_opts);
					opt2 = opt->next;
					free(opt);
					opt = opt2;
				}
				tune_opts = NULL;
				return as;
			}

			if (strcmp(p, "end") == 0)
				return as;	/* end of previous %%tune */

			/* search the end of the tune options */
			as2 = as;
			for (;;) {
				as3 = as2->next;
				if (!as3)
					break;
				if (as3->type != ABC_T_NULL
				 && (as3->type != ABC_T_PSCOM
				  || strncmp(&as3->text[2], "tune ", 5) == 0))
					break;
				as2 = as3;
			}

			/* search if already a same %%tune */
			opt2 = NULL;
			for (opt = tune_opts; opt; opt = opt->next) {
				if (strcmp(opt->s->as.text, as->text) == 0)
					break;
				opt2 = opt;
			}

			if (opt) {
				free_voice_opt(opt->voice_opts);
				if (as2 == as) {	/* no option */
					if (!opt2)
						tune_opts = opt->next;
					else
						opt2->next = opt->next;
					free(opt);
					return as;
				}
				opt->voice_opts = NULL;
			} else {
				if (as2 == as)		/* no option */
					return as;
				opt = malloc(sizeof *opt);
				memset(opt, 0, sizeof *opt);
				opt->next = tune_opts;
				tune_opts = opt;
			}

			/* link the options */
			opt->s = s;
			cur_tune_opts = opt;
			for ( ; as != as2; as = as->next) {
				if (as->next->type != ABC_T_PSCOM)
					continue;
				if (strncmp(&as->next->text[2], "voice ", 6) == 0) {
					as = process_pscomment(as->next);
					if (as == as2)
						break;
					continue;
				}
				as->next->state = ABC_S_TUNE;

				/* !! no reverse link !! */
				s->next = (struct SYMBOL *) as->next;
				s = s->next;
			}
			cur_tune_opts = NULL;
			return as;
		}
		break;
	case 'u':
		if (strcmp(w, "user") == 0) {
			unsigned char *deco;

			deco = as->state == ABC_S_GLOBAL ? deco_glob : deco_tune;
			deco[as->u.user.symbol] = deco_intern(as->u.user.value);
			return as;
		}
		break;
	case 'v':
		if (strcmp(w, "vocal") == 0) {
			set_voice_param(curvoice, as->state, w, p);
			return as;
		}
		if (strcmp(w, "voice") == 0) {
			struct abcsym *as2;
			struct voice_opt_s *opt, *opt2;

			if (as->state != ABC_S_GLOBAL) {
				error(1, s, "%%%%voice ignored");
				return as;
			}

			/* if void %%voice, free all voice options */
			if (*p == '\0') {
				if (cur_tune_opts) {
					free_voice_opt(cur_tune_opts->voice_opts);
					cur_tune_opts->voice_opts = NULL;
				} else {
					free_voice_opt(voice_opts);
					voice_opts = NULL;
				}
				return as;
			}

			if (strcmp(p, "end") == 0)
				return as;	/* end of previous %%voice */

			if (cur_tune_opts)
				opt = cur_tune_opts->voice_opts;
			else
				opt = voice_opts;

			/* search the end of the voice options */
			as2 = as;
			for (;;) {
				struct abcsym *as3;

				as3 = as2->next;
				if (!as3)
					break;
				if (as3->type != ABC_T_NULL
				 && (as3->type != ABC_T_PSCOM
				  || strncmp(&as3->text[2], "score ", 6) == 0
				  || strncmp(&as3->text[2], "staves ", 7) == 0
				  || strncmp(&as3->text[2], "tune ", 5) == 0
				  || strncmp(&as3->text[2], "voice ", 6) == 0))
					break;
				as2 = as3;
			}

			/* if already the same %%voice
			 * remove the options */
			opt2 = NULL;
			for ( ; opt; opt = opt->next) {
				if (strcmp(opt->s->as.text, as->text) == 0) {
					if (!opt2) {
						if (cur_tune_opts)
							cur_tune_opts->voice_opts = NULL;
						else
							voice_opts = NULL;
					} else {
						opt2->next = opt->next;
					}
					free(opt);
					break;
				}
				opt2 = opt;
			}
			if (as2 == as)		/* no option */
				return as;
			opt = malloc(sizeof *opt + strlen(p));
			memset(opt, 0, sizeof *opt);
			if (cur_tune_opts) {
				opt->next = cur_tune_opts->voice_opts;
				cur_tune_opts->voice_opts = opt;
			} else {
				opt->next = voice_opts;
				voice_opts = opt;
			}

			/* link the options */
			opt->s = s;
			for ( ; as != as2; as = as->next) {
				if (as->next->type != ABC_T_PSCOM)
					continue;
				as->next->state = ABC_S_TUNE;
				s->next = (struct SYMBOL *) as->next;
				s = s->next;
			}
			return as->prev;
		}
		if (strcmp(w, "voicescale") == 0) {
			int voice;
			char *q;
			float scale;

			scale = strtod(p, &q);
			if (scale < 0.6 || scale > 1.5
			 || (*q != '\0' && *q != ' ')) {
				error(1, s, "Bad %%%%voicescale value");
				return as;
			}
			if (as->state != ABC_S_TUNE) {
				for (voice = 0; voice < MAXVOICE; voice++)
					voice_tb[voice].scale = scale;
			} else {
				curvoice->scale = scale;
			}
			return as;
		}
		if (strcmp(w, "volume") == 0) {
			set_voice_param(curvoice, as->state, w, p);
			return as;
		}
		if (strcmp(w, "vskip") == 0) {
			if (as->state == ABC_S_TUNE) {
				gen_ly(0);
			} else if (as->state == ABC_S_GLOBAL) {
				if (epsf || in_fname == 0)
					return as;
			}
			h1 = scan_u(p);
			bskip(h1);
			buffer_eob();
			return as;
		}
		break;
	}
	if (as->state == ABC_S_TUNE) {
		if (strcmp(w, "leftmargin") == 0
		 || strcmp(w, "rightmargin") == 0
		 || strcmp(w, "scale") == 0) {
			generate();
			block_put();
		}
	}
	interpret_fmt_line(w, p, lock);
	if (cfmt.alignbars && strcmp(w, "alignbars") == 0) {
		int i;

		generate();
		if ((unsigned) cfmt.alignbars > MAXSTAFF) {
			error(1, s, "Too big value in %%%%alignbars");
			cfmt.alignbars = MAXSTAFF;
		}
		if (staves_found >= 0)		/* (compatibility) */
			cfmt.alignbars = nstaff + 1;
		first_voice = curvoice = &voice_tb[0];
		for (i = 0; i < cfmt.alignbars; i++) {
			voice_tb[i].staff = voice_tb[i].cstaff = i;
			voice_tb[i].next = &voice_tb[i + 1];
			parsys->staff[i].flags |= STOP_BAR;
			parsys->voice[i].staff = i;
			parsys->voice[i].range = i;
		}
		voice_tb[i - 1].next = NULL;
		nstaff = i;
	}
	return as;
}

/* -- set the duration of notes/rests in a tuplet -- */
/*fixme: KO if voice change*/
/*fixme: KO if in a grace sequence*/
static void set_tuplet(struct SYMBOL *t)
{
	struct abcsym *as;
	struct SYMBOL *s;
	int l, r, lplet, grace;

	r = t->as.u.tuplet.r_plet;
	grace = t->as.flags & ABC_F_GRACE;

	l = 0;
	for (as = t->as.next; as; as = as->next) {
		if (as->type == ABC_T_TUPLET) {
			struct abcsym *as2;
			int l2, r2;

			r2 = as->u.tuplet.r_plet;
			l2 = 0;
			for (as2 = as->next; as2; as2 = as2->next) {
				switch (as2->type) {
				case ABC_T_NOTE:
				case ABC_T_REST:
					break;
				case ABC_T_EOLN:
					if (as2->u.eoln.type != 1) {
						error(1, t,
							"End of line found inside a nested tuplet");
						return;
					}
					continue;
				default:
					continue;
				}
				if (as2->u.note.lens[0] == 0)
					continue;
				if (grace ^ (as2->flags & ABC_F_GRACE))
					continue;
				s = (struct SYMBOL *) as2;
				l2 += s->dur;
				if (--r2 <= 0)
					break;
			}
			l2 = l2 * as->u.tuplet.q_plet
					/ as->u.tuplet.p_plet;
			((struct SYMBOL *) as)->u = l2;
			l += l2;
			r -= as->u.tuplet.r_plet;
			if (r == 0)
				break;
			if (r < 0) {
				error(1, t, "Bad nested tuplet");
				break;
			}
			as = as2;
			continue;
		}
		switch (as->type) {
		case ABC_T_NOTE:
		case ABC_T_REST:
			break;
		case ABC_T_EOLN:
			if (as->u.eoln.type != 1) {
				error(1, t, "End of line found inside a tuplet");
				return;
			}
			continue;
		default:
			continue;
		}
		if (as->u.note.lens[0] == 0)	/* space ('y') */
			continue;
		if (grace ^ (as->flags & ABC_F_GRACE))
			continue;
		s = (struct SYMBOL *) as;
		l += s->dur;
		if (--r <= 0)
			break;
	}
	if (!as) {
		error(1, t, "End of tune found inside a tuplet");
		return;
	}
	if (t->u != 0)		/* if nested tuplet */
		lplet = t->u;
	else
		lplet = (l * t->as.u.tuplet.q_plet) / t->as.u.tuplet.p_plet;
	r = t->as.u.tuplet.r_plet;
	for (as = t->as.next; as; as = as->next) {
		int olddur;

		if (as->type == ABC_T_TUPLET) {
			int r2;

			r2 = as->u.tuplet.r_plet;
			s = (struct SYMBOL *) as;
			olddur = s->u;
			s->u = (olddur * lplet) / l;
			l -= olddur;
			lplet -= s->u;
			r -= r2;
			for (;;) {
				as = as->next;
				if (as->type != ABC_T_NOTE
				 && as->type != ABC_T_REST)
					continue;
				if (as->u.note.lens[0] == 0)
					continue;
				if (grace ^ (as->flags & ABC_F_GRACE))
					continue;
				if (--r2 <= 0)
					break;
			}
			if (r <= 0)
				goto done;
			continue;
		}
		if (as->type != ABC_T_NOTE && as->type != ABC_T_REST)
			continue;
		if (as->u.note.lens[0] == 0)
			continue;
		if (grace ^ (as->flags & ABC_F_GRACE))
			continue;
		s = (struct SYMBOL *) as;
		s->sflags |= S_IN_TUPLET;
		olddur = s->dur;
		s->dur = (olddur * lplet) / l;
		if (--r <= 0)
			break;
		l -= olddur;
		lplet -= s->dur;
	}
done:
	if (grace) {
		error(1, t, "Tuplets in grace note sequence not yet treated");
	} else {
		sym_link(t, TUPLET);
		t->u = cfmt.tuplets;
	}
}

/* -- add a new symbol in a voice -- */
struct SYMBOL *sym_add(struct VOICE_S *p_voice, int type)
{
	struct SYMBOL *s;
	struct VOICE_S *p_voice2;

	s = (struct SYMBOL *) getarena(sizeof *s);
	memset(s, 0, sizeof *s);
	p_voice2 = curvoice;
	curvoice = p_voice;
	sym_link(s, type);
	curvoice = p_voice2;
	if (p_voice->second)
		s->sflags |= S_SECOND;
	if (p_voice->floating)
		s->sflags |= S_FLOATING;
	return s;
}

/* -- link a ABC symbol into a voice -- */
static void sym_link(struct SYMBOL *s, int type)
{
	struct VOICE_S *p_voice = curvoice;

/*	memset((&s->as) + 1, 0, sizeof (struct SYMBOL) - sizeof (struct abcsym)); */
	if (!p_voice->ignore) {
		s->prev = p_voice->last_sym;
		if (s->prev)
			p_voice->last_sym->next = s;
		else
			p_voice->sym = s;
		p_voice->last_sym = s;
//fixme:test bug
//	} else {
//		if (p_voice->sym)
//			p_voice->last_sym = p_voice->sym = s;
	}

	s->type = type;
	s->voice = p_voice - voice_tb;
	s->staff = p_voice->cstaff;
	s->time = p_voice->time;
	s->posit = p_voice->posit;
}
