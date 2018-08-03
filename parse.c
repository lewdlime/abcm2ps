/*
 * Parsing functions.
 *
 * This file is part of abcm2ps.
 *
 * Copyright (C) 1998-2018 Jean-François Moine
 * Adapted from abc2ps, Copyright (C) 1996,1997 Michael Methfessel
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <regex.h>

#include "abcm2ps.h"

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

int nstaff;				/* (0..MAXSTAFF-1) */
struct SYMBOL *tsfirst;			/* first symbol in the time sorted list */

struct VOICE_S voice_tb[MAXVOICE];	/* voice table */
struct VOICE_S *first_voice;		/* first voice */
struct SYSTEM *cursys;			/* current system */
static struct SYSTEM *parsys;		/* current system while parsing */

struct FORMAT dfmt;			/* current global format */
int nbar;				/* current measure number */

struct map *maps;			/* note mappings */

static struct voice_opt_s *voice_opts, *tune_voice_opts;
static struct tune_opt_s *tune_opts, *cur_tune_opts;
static struct brk_s *brks;
static struct symsel_s clip_start, clip_end;

static INFO info_glob;			/* global info definitions */
static char *deco_glob[256];		/* global decoration table */
static struct map *maps_glob;		/* save note maps */

static int over_time;			/* voice overlay start time */
static int over_mxtime;			/* voice overlay max time */
static short over_bar;			/* voice overlay in a measure */
static short over_voice;		/* main voice in voice overlay */
static int staves_found;		/* time of the last %%staves */
static int abc2win;
static int capo;			// capo indication

float multicol_start;			/* (for multicol) */
static float multicol_max;
static float lmarg, rmarg;

static void get_clef(struct SYMBOL *s);
static struct SYMBOL *get_info(struct SYMBOL *s);
static void get_key(struct SYMBOL *s);
static void get_meter(struct SYMBOL *s);
static void get_voice(struct SYMBOL *s);
static void get_note(struct SYMBOL *s);
static struct SYMBOL *process_pscomment(struct SYMBOL *s);
static void ps_def(struct SYMBOL *s, char *p, char use);
static void set_tblt(struct VOICE_S *p_voice);
static void set_tuplet(struct SYMBOL *s);

/* -- weight of the symbols -- */
static char w_tb[NSYMTYPES] = {	/* !! index = symbol type !! */
	0,
	9,	/* 1- note / rest */
	3,	/* 2- space */
	2,	/* 3- bar */
	1,	/* 4- clef */
	6,	/* 5- timesig */
	5,	/* 6- keysig */
	0,	/* 7- tempo */
	0,	/* 8- staves */
	9,	/* 9- mrest */
	0,	/* 10- part */
	3,	/* 11- grace */
	0,	/* 12- fmtchg */
	8,	/* 13- tuplet */
	7,	/* 14- stbrk */
	7	/* 15- custos */
};

/* key signature transposition tables */
static signed char cde2fcg[7] = {0, 2, 4, -1, 1, 3, 5};
static char cgd2cde[7] = {0, 4, 1, 5, 2, 6, 3};

/* -- link a ABC symbol into the current voice -- */
static void sym_link(struct SYMBOL *s, int type)
{
	struct VOICE_S *p_voice = curvoice;

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
	if (s->prev) {
		s->fn = s->prev->fn;
		s->linenum = s->prev->linenum;
		s->colnum = s->prev->colnum;
	}
	return s;
}

/* -- expand a multi-rest into single rests and measure bars -- */
static void mrest_expand(struct SYMBOL *s)
{
	struct VOICE_S *p_voice;
	struct SYMBOL *s2, *next;
	struct decos dc;
	int nb, dt;

	nb = s->u.bar.len;
	dt = s->dur / nb;

	/* change the multi-rest (type bar) to a single rest */
	memcpy(&dc, &s->u.bar.dc, sizeof dc);
	memset(&s->u.note, 0, sizeof s->u.note);
	s->type = NOTEREST;
	s->abc_type = ABC_T_REST;
//	s->nhd = 0;
	s->dur = s->u.note.notes[0].len = dt;
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
		s2->abc_type = ABC_T_BAR;
		s2->u.bar.type = B_SINGLE;
		s2 = sym_add(p_voice, NOTEREST);
		s2->abc_type = ABC_T_REST;
		s2->flags = s->flags;
		s2->dur = s2->u.note.notes[0].len = dt;
		s2->head = H_FULL;
		s2->nflags = -2;
		p_voice->time += dt;
	}
	s2->next = next;
	if (next)
		next->prev = s2;

	/* copy the mrest decorations to the last rest */
	memcpy(&s2->u.note.dc, &dc, sizeof s2->u.note.dc);
}

/* -- sort all symbols by time and vertical sequence -- */
static void sort_all(void)
{
	struct SYSTEM *sy;
	struct SYMBOL *s, *prev, *s2;
	struct VOICE_S *p_voice;
	int fl, voice, time, w, wmin, multi, mrest_time;
	int nb, r, set_sy, new_sy;	// nv
	struct SYMBOL *vtb[MAXVOICE];
	signed char vn[MAXVOICE];	/* voice indexed by range */

/*	memset(vtb, 0, sizeof vtb); */
	mrest_time = -1;
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next)
		vtb[p_voice - voice_tb] = p_voice->sym;

	/* initialize the voice order */
	sy = cursys;
	set_sy = 1;
	new_sy = 0;
	prev = NULL;
	fl = 1;				/* (have gcc happy) */
	multi = -1;			/* (have gcc happy) */
	for (;;) {
		if (set_sy) {
		    fl = 1;			// start a new sequence
		    if (!new_sy) {
			set_sy = 0;
			multi = -1;
			memset(vn, -1, sizeof vn);
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
		}

		/* search the min time and symbol weight */
		wmin = time = (unsigned) ~0 >> 1;	/* max int */
//		nv = nb = 0;
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
//				nb = 0;
			} else if (w < wmin) {
				wmin = w;
//				nb = 0;
			}
#if 0
			if (!(s->sflags & S_SECOND)) {
				nv++;
				if (s->type == BAR)
					nb++;
			}
#endif
			if (s->type == MREST) {
				if (s->u.bar.len == 1)
					mrest_expand(s);
				else if (multi > 0)
					mrest_time = time;
			}
		}
		if (wmin > 127)
			break;					/* done */

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
					mrest_time = -1; /* some note or rest */
					break;
				}
				if (nb == 0) {
					nb = s->u.bar.len;
				} else if (nb != s->u.bar.len) {
					mrest_time = -1; /* different duration */
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
			if (!s || s->time != time
			 || w_tb[s->type] != wmin)
				continue;
			if (s->type == STAVES) {	// change STAVES to a flag
				sy = sy->next;
				set_sy = new_sy = 1;
				if (s->prev)
					s->prev->next = s->next;
				else
					voice_tb[voice].sym = s->next;
				if (s->next)
					s->next->prev = s->prev;
			} else {
				if (fl) {
					fl = 0;
					s->sflags |= S_SEQST;
				}
				if (new_sy) {
					new_sy = 0;
					s->sflags |= S_NEW_SY;
				}
				s->ts_prev = prev;
				if (prev) {
					prev->ts_next = s;
//fixme: bad error when the 1st voice is second
//					if (s->type == BAR
//					 && (s->sflags & S_SECOND)
//					 && prev->type != BAR
//					 && !(s->flags & ABC_F_INVIS))
//						error(1, s, "Bad measure bar");
				} else {
					tsfirst = s;
				}
				prev = s;
			}
			vtb[voice] = s->next;
		}
		fl = wmin;		/* start a new sequence if some space */
	}

	if (!prev)
		return;

	/* if no bar or format_change at end of tune, add a dummy symbol */
	if ((prev->type != BAR && prev->type != FMTCHG)
	 || new_sy) {
		p_voice = &voice_tb[prev->voice];
		p_voice->last_sym = prev;
		s = sym_add(p_voice, FMTCHG);
		s->aux = -1;
		s->time = prev->time + prev->dur;
		s->sflags = S_SEQST;
		if (new_sy)
			s->sflags |= S_NEW_SY;
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
	while (s) {
		if (s->type == TEMPO)
			return;			/* already a tempo */
		s = s->next;
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
	struct SYMBOL *s, *s2, *s3, *ns;

	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
//8.7.0 - for fmt at end of music line
//		if (p_voice->ignore)
//			continue;
		p_voice->ignore = 0;
		for (s = p_voice->sym; s; s = s->next) {
			if (s->time >= staves_found)
				break;
		}
		ns = NULL;
		for ( ; s; s = s->next) {
			switch (s->type) {
#if 0 // test
			case KEYSIG:	/* remove the empty key signatures */
				if (s->u.key.empty) {
					if (s->prev)
						s->prev->next = s->next;
					else
						p_voice->sym = s->next;
					if (s->next)
						s->next->prev = s->prev;
					continue;
				}
				break;
#endif
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
				continue;
			case MREST:		/* don't shift P: and Q: */
				if (!ns)
					continue;
				s2 = (struct SYMBOL *) getarena(sizeof *s);
				memset(s2, 0, sizeof *s2);
				s2->type = SPACE;
				s2->u.note.notes[1].len = -1;
				s2->flags = ABC_F_INVIS;
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
			if (s->flags & ABC_F_GRACE) {
				if (!ns)
					ns = s;
				while (!(s->flags & ABC_F_GR_END))
					s = s->next;
				s2 = (struct SYMBOL *) getarena(sizeof *s);
				memcpy(s2, s, sizeof *s2);
				s2->abc_type = 0;
				s2->type = GRACE;
				s2->dur = 0;
				s2->next = s->next;
				if (s2->next) {
					s2->next->prev = s2;
					if (cfmt.graceword) {
						for (s3 = s2->next; s3; s3 = s3->next) {
							switch (s3->type) {
							case SPACE:
								continue;
							case NOTEREST:
								s2->ly = s3->ly;
								s3->ly = NULL;
							default:
								break;
							}
							break;
						}
					}
				} else {
					p_voice->last_sym = s2;
				}
				s2->prev = s;
				s->next = s2;
				s = s2;

				// with w_tb[BAR] = 2,
				// the grace notes go after the bar
				// if before a bar, change the grace time
				if (s->next && s->next->type == BAR)
					s->time--;
			}
			if (!ns)
				continue;
			s->extra = ns;
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
			s->aux = -1;		/* nothing */
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
			if (s->type == STAVES)
				continue;
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
		}
		for (staff = 0; staff < MAXSTAFF; staff++) {
			new_sy->staff[staff].stafflines = "|||||";
			new_sy->staff[staff].staffscale = 1;
		}
		cursys = new_sy;
	} else {
		for (voice = 0; voice < MAXVOICE; voice++) {

			// update the previous system
//			if (parsys->voice[voice].range < 0
//			 || parsys->voice[voice].second)
//				continue;
			staff = parsys->voice[voice].staff;
			if (voice_tb[voice].stafflines)
				parsys->staff[staff].stafflines =
						voice_tb[voice].stafflines;
			if (voice_tb[voice].staffscale != 0)
				parsys->staff[staff].staffscale =
						voice_tb[voice].staffscale;
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

/* -- initialize the voices and staves -- */
/* this routine is called when starting the generation */
static void system_init(void)
{
	voice_compress();
	voice_dup();
	sort_all();			/* define the time / vertical sequences */
//	if (!tsfirst)
//		return;
//	parsys->nstaff = nstaff;	/* save the number of staves */
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
		 && s->aux >= symsel->bar)
			break;
	}
	if (!s)
		return NULL;
	if (symsel->seq != 0) {
		int seq;

		seq = symsel->seq;
		for (s = s->ts_next; s; s = s->ts_next) {
			if (s->type == BAR
			 && s->aux == symsel->bar) {
				if (--seq == 0)
					break;
			}
		}
		if (!s)
			return NULL;
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
			if (s->sflags & S_NEW_SY)
				sy = sy->next;
			switch (s2->type) {
			case CLEF:
				voice_tb[s2->voice].s_clef = s2;
				break;
			case KEYSIG:
				memcpy(&voice_tb[s2->voice].key, &s2->u.key,
					sizeof voice_tb[0].key);
				break;
			case TIMESIG:
				memcpy(&voice_tb[s2->voice].meter, &s2->u.meter,
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
	struct SYMBOL *s, *s2, *s3;
	int bar_time, wmeasure, tim;
	int bar_num, bar_rep;

	wmeasure = voice_tb[cursys->top_voice].meter.wmeasure;
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
			if (s->aux) {
				nbar = s->aux;		/* (%%setbarnb) */
				break;
			}
			if (s->u.bar.repeat_bar
			 && s->text
			 && !cfmt.contbarnb) {
				if (s->text[0] == '1') {
					bar_rep = nbar;
				} else {
					nbar = bar_rep; /* restart bar numbering */
					s->aux = nbar;
				}
			}
			break;
		}
		break;
	}

	/* set the measure number on the top bars
	 * and move the clefs before the measure bars */
	bar_time = s->time + wmeasure;	/* for incomplete measure at start of tune */
	bar_num = nbar;
	for ( ; s; s = s->ts_next) {
		switch (s->type) {
		case CLEF:
			if (s->sflags & S_NEW_SY)
				break;
			for (s2 = s->ts_prev; s2; s2 = s2->ts_prev) {
				if (s2->sflags & S_NEW_SY) {
					s2 = NULL;
					break;
				}
				switch (s2->type) {
				case BAR:
					if (s2->sflags & S_SEQST)
						break;
					continue;
				case MREST:
				case NOTEREST:
				case SPACE:
				case STBRK:
				case TUPLET:
					s2 = NULL;
					break;
				default:
					continue;
				}
				break;
			}
			if (!s2)
				break;

			/* move the clef */
			s->next->prev = s->prev;
			s->prev->next = s->next;
			s->ts_next->ts_prev = s->ts_prev;
			s->ts_prev->ts_next = s->ts_next;
			s->next = s2;
			s->prev = s2->prev;
			s->prev->next = s;
			s2->prev = s;
			s->ts_next = s2;
			s->ts_prev = s2->ts_prev;
			s->ts_prev->ts_next = s;
			s2->ts_prev = s;
//			if (s->sflags & S_NEW_SY) {
//				s->sflags &= ~S_NEW_SY;
//				s->ts_next->sflags |= S_NEW_SY;
//			}
			s3 = s->extra;
			if (s3) {
				if (s->ts_next->extra) {
					while (s3->next)
						s3 = s3->next;
					s3->next = s->ts_next->extra;
					s->ts_next->extra = s->extra;
				} else {
					s->ts_next->extra = s3;
				}
				s->extra = NULL;
			}
			s = s2;
			break;
		case TIMESIG:
			wmeasure = s->u.meter.wmeasure;
			if (s->time < bar_time)
				bar_time = s->time + wmeasure;
			break;
		case MREST:
			bar_num += s->u.bar.len - 1;
			while (s->ts_next
			    && s->ts_next->type != BAR)
				s = s->ts_next;
			break;
		case BAR:
//			if (s->flags & ABC_F_INVIS)
//				break;
			if (s->aux) {
				bar_num = s->aux;		/* (%%setbarnb) */
//				if (s->time < bar_time) {
//					s->aux = 0;
					break;
//				}
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
				 && s2->u.bar.repeat_bar
				 && s2->text
				 && !cfmt.contbarnb) {
					if (s2->text[0] == '1')
						bar_rep = bar_num;
					else		/* restart bar numbering */
						bar_num = bar_rep;
					break;
				}
				s2 = s2->next;
			} while (s2 && s2->time == tim);
			s->aux = bar_num;
			bar_time = s->time + wmeasure;
			break;
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
	int old_lvl, voice;
	struct VOICE_S *p_voice;

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
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		voice = p_voice - voice_tb;
		p_voice->sym = p_voice->last_sym = NULL;
		p_voice->time = 0;
		p_voice->have_ly = 0;
		p_voice->staff = cursys->voice[voice].staff;
		p_voice->second = cursys->voice[voice].second;
		p_voice->s_clef->time = 0;
		p_voice->lyric_start = NULL;
	}
	staves_found = 0;		// (for voice compress/dup)
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
		buffer_eob(0);
}

/*
 * for transpose purpose, check if a pitch is already in the measure or
 * if it is tied from a previous note, and return the associated accidental
 */
static int acc_same_pitch(int pitch)
{
	struct SYMBOL *s = curvoice->last_sym->prev;
	int i, time;

	// the overlaid voices may have no measure bars
//	if (curvoice->id[0] == '&')
//		s = voice_tb[curvoice->mvoice].last_sym;

	if (!s)
		return -1;

	time = s->time;

	for (; s; s = s->prev) {
		switch (s->abc_type) {
		case ABC_T_BAR:
			if (s->time < time)
				return -1;	/* no same pitch */
			for (;;) {
				s = s->prev;
				if (!s)
					return -1;
				if (s->abc_type == ABC_T_NOTE) {
					if (s->time + s->dur == time)
						break;
					return -1;
				}
				if (s->time < time)
					return -1;
			}
			for (i = 0; i <= s->nhd; i++) {
				if (s->u.note.notes[i].pit == pitch
				 && s->u.note.notes[i].ti1)
					return s->u.note.notes[i].acc;
			}
			return -1;
		case ABC_T_NOTE:
			for (i = 0; i <= s->nhd; i++) {
				if (s->u.note.notes[i].pit == pitch)
					return s->u.note.notes[i].acc;
			}
			break;
		}
	}
	return -1;
}

/* transpose a note / chord */
static void note_transpose(struct SYMBOL *s)
{
	int i, j, m, n, d, a, dp, i1, i2, i3, i4, sf_old;
	static const signed char acc1[6] = {0, 1, 0, -1, 2, -2};
	static const char acc2[5] = {A_DF, A_FT, A_NT, A_SH, A_DS};

	m = s->nhd;
	sf_old = curvoice->okey.sf;
	i2 = curvoice->ckey.sf - sf_old;
	dp = cgd2cde[(i2 + 4 * 7) % 7];
	if (curvoice->transpose < 0
	 && dp != 0)
		dp -= 7;
	dp += curvoice->transpose / 3 / 12 * 7;
	for (i = 0; i <= m; i++) {

		/* pitch */
		n = s->u.note.notes[i].pit;
		s->u.note.notes[i].pit += dp;
		s->pits[i] += dp;

		/* accidental */
		i1 = cde2fcg[(n + 5 + 16 * 7) % 7];	/* fcgdaeb */
		a = s->u.note.notes[i].acc & 0x07;
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

		i1 = ((i3 + 1 + 21) / 7 + 2 - 3 + 32 * 5) % 5;
		a = acc2[(unsigned) i1];
		if (s->u.note.notes[i].acc != 0) {
			;
		} else if (curvoice->ckey.empty) {	/* key none */
			if (a == A_NT
			 || acc_same_pitch(s->u.note.notes[i].pit) >= 0)
				continue;
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
		i1 = s->u.note.notes[i].acc & 0x07;
		i4 = s->u.note.notes[i].acc >> 3;
		if (i4 != 0				/* microtone */
		 && i1 != a) {				/* different accidental type */
			if (s->u.note.microscale) {
				n = i4;
				d = s->u.note.microscale;
			} else {
				n = parse.micro_tb[i4];
				d = ((n & 0xff) + 1) * 2;
				n = (n >> 8) + 1;
			}
//fixme: double sharps/flats ?*/
//fixme: does not work in all cases (tied notes, previous accidental)
			switch (a) {
			case A_NT:
				if (n >= d / 2) {
					n -= d / 2;
					a = i1;
				} else {
					a = i1 == A_SH ? A_FT : A_SH;
				}
				break;
			case A_DS:
				if (n >= d / 2) {
					s->u.note.notes[i].pit += 1;
					s->pits[i] += 1;
					n -= d / 2;
				} else {
					n += d / 2;
				}
				a = i1;
				break;
			case A_DF:
				if (n >= d / 2) {
					s->u.note.notes[i].pit -= 1;
					s->pits[i] -= 1;
					n -= d / 2;
				} else {
					n += d / 2;
				}
				a = i1;
				break;
			}
			if (s->u.note.microscale) {
				i4 = n;
			} else {
				d = d / 2 - 1 + ((n - 1) << 8);
				for (i4 = 1; i4 < MAXMICRO; i4++) {
					if (parse.micro_tb[i4] == d)
						break;
					if (parse.micro_tb[i4] == 0) {
						parse.micro_tb[i4] = d;
						break;
					}
				}
				if (i4 == MAXMICRO) {
					error(1, s, "Too many microtone accidentals");
					i4 = 0;
				}
			}
		}
		s->u.note.notes[i].acc = (i4 << 3) | a;
	}
}

/* transpose a guitar chord */
static void gch_tr1(struct SYMBOL *s, int i, int i2)
{
	char *p = &s->text[i],
		*q = p + 1,
		*new_txt;
	int l, latin;
	int n, a, i1, i3, i4;
	static const char note_names[] = "CDEFGAB";
	static const char *latin_names[7] =
			{ "Do", "Ré", "Mi", "Fa", "Sol", "La", "Si" };
	static const char *acc_name[5] = {"bb", "b", "", "#", "##"};

	/* main chord */
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
			latin++;
			n = 0;		/* Do */
			break;
		}
		n = 1;
		break;
	case 'F':
		if (p[1] == 'a')
			latin++;	/* Fa */
		n = 3;
		break;
	case 'L':
		latin++;		/* La */
		n = 5;
		break;
	case 'M':
		latin++;		/* Mi */
		n = 2;
		break;
	case 'R':
		latin++;
		if (p[1] != 'e')
			latin++;	/* Ré */
		n = 1;			/* Re */
		break;
	case 'S':
		latin++;
		if (p[1] == 'o') {
			latin++;
			n = 4;		/* Sol */
		} else {
			n = 6;		/* Si */
		}
		break;
	case '/':			// bass only
		latin--;
		break;
	default:
		return;
	}
	q += latin;

	/* allocate a new string */
	new_txt = getarena(strlen(s->text) + 6);
	l = p - s->text;
	memcpy(new_txt, s->text, l);
	s->text = new_txt;
	new_txt += l;
	p = q;

	if (latin >= 0) {			// if some chord
		a = 0;
		while (*p == '#') {
			a++;
			p++;
		}
		while (*p == 'b') {
			a--;
			p++;
		}
//		if (*p == '=')
//			p++;
		i3 = cde2fcg[n] + i2 + a * 7;
		i4 = cgd2cde[(unsigned) ((i3 + 16 * 7) % 7)];
		i1 = ((i3 + 1 + 21) / 7 + 2 - 3 + 32 * 5) % 5;
							/* accidental */
		if (latin == 0)
			*new_txt++ = note_names[i4];
		else
			new_txt += sprintf(new_txt, "%s", latin_names[i4]);
		new_txt += sprintf(new_txt, "%s", acc_name[i1]);
	}

	/* bass */
	while (*p != '\0' && *p != '\n' && *p != '/')	// skip 'm'/'dim'..
		*new_txt++ = *p++;
	if (*p == '/') {
		*new_txt++ = *p++;
//fixme: latin names not treated
		q = strchr(note_names, *p);
		if (q) {
			p++;
			n = q - note_names;
			if (*p == '#') {
				a = 1;
				p++;
			} else if (*p == 'b') {
				a = -1;
				p++;
			} else {
				a = 0;
			}
			i3 = cde2fcg[n] + i2 + a * 7;
			i4 = cgd2cde[(unsigned) ((i3 + 16 * 7) % 7)];
			i1 = ((i3 + 1 + 21) / 7 + 2 - 3 + 32 * 5) % 5;
			*new_txt++ = note_names[i4];
			new_txt += sprintf(new_txt, "%s", acc_name[i1]);
		}
	}
	strcpy(new_txt, p);
}

static void gch_capo(struct SYMBOL *s)
{
	char *p = s->text, *q, *r;
	int i, l, li = 0;
	static const char *capo_txt = "  (capo: %d)";
	static signed char cap_trans[] =
		{0, 5, -2, 3, -4, 1, -6, -1, 4, -3, 2, -5};

	// search the chord symbols
	for (;;) {
		if (!strchr("^_<>@", *p))
			break;
		p = strchr(p, '\n');
		if (!p)
			return;
		p++;
	}

	// add a capo chord symbol
	i = p - s->text;
	q = strchr(p + 1, '\n');
	if (q)
		l = q - p;
	else
		l = strlen(p);
	if (!capo) {
		capo = 1;
		li = strlen(capo_txt);
	}
	r = (char *) getarena(strlen(s->text) + l + li + 1);
	i += l;
	strncpy(r, s->text, i);		// annotations + chord symbol
	r[i++] = '\n';
	strncpy(r + i, p, l);		// capo
	if (li) {
		sprintf(r + i + l, capo_txt, cfmt.capo);
		l += li;
	}
	if (q)
		strcpy(r + i + l, q);	// ending annotations
	s->text = r;
	gch_tr1(s, i, cap_trans[cfmt.capo % 12]);
}

static void gch_transpose(struct SYMBOL *s)
{
	int in_ch = 0;
	int i2 = curvoice->ckey.sf - curvoice->okey.sf;
	char *o = s->text, *p = o;

	// search the chord symbols
	for (;;) {
		if (in_ch || !strchr("^_<>@", *p)) {
			gch_tr1(s, p - s->text, i2);
			p = s->text + (p - o);
			o = s->text;
			for (p++; *p; p++) {
				if (strchr("\t;\n", *p))
					break;
			}
			if (!*p)
				break;
			switch (*p) {
			case '\t':
				in_ch = 1;
				break;
			case ';':
				in_ch = !strchr("^_<>@", p[1]);
				break;
			default:
				in_ch = 0;
				break;
			}
		} else {
			p = strchr(p, '\n');
			if (!p)
				break;
		}
		p++;
	}
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
	if (cfmt.capo)
		gch_capo(s);

	/* split the guitar chords / annotations
	 * and initialize their vertical offsets */
	gch_place = s->posit.gch == SL_BELOW ? -1 : 1;
	h_gch = cfmt.font_tb[cfmt.gcf].size;
	h_ann = cfmt.font_tb[cfmt.anf].size;
	y_above = y_below = y_left = y_right = 0;
	box = cfmt.gchordbox;
	p = s->text;
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
			 && gch != s->gch
			 && s->gch->type == '@') {	/* if not 1st line */
				gch->x = (gch - 1)->x;
				gch->y = (gch - 1)->y - h_ann;
			}
			break;
		}
		gch->idx = p - s->text;
		for (;;) {
			switch (*p) {
			default:
				p++;
				continue;
			case '\\':
				p++;
				if (*p == 'n') {
					p[-1] = '\0';
					break;		/* sep = 'n' */
				}
				p++;
				continue;
			case '&':			/* skip "&xxx;" */
				for (;;) {
					switch (*p) {
					default:
						p++;
						continue;
					case ';':
						p++;
					case '\0':
					case '\n':
					case '\\':
						break;
					}
					break;
				}
				continue;
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
		p = s->text + gch->idx;
		q = p;
		for (; *p != '\0'; p++) {
			switch (*p) {
			case '#':
			case 'b':
			case '=':
				if (p == q	/* 1st char or after a slash */
				 || (p != q + 1	/* or invert '\' behaviour */
				  && p[-1] == '\\'))
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
				if (p[-1] == '\\') {
					p--;
					l = strlen(p);
					memmove(p, p + 1, l);
				}
				break;
			case ' ':
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
		p = s->text + gch->idx;
		str_font(gch->font);
		w = tex_str(p);
		gch->w = w; // + 4;
		switch (gch->type) {
		case '_':			/* below */
			xspc = w * GCHPRE;
			if (xspc > 8)
				xspc = 8;
			gch->x = -xspc;
			y_below -= h_ann;
			gch->y = y_below;
			break;
		case '^':			/* above */
			xspc = w * GCHPRE;
			if (xspc > 8)
				xspc = 8;
			gch->x = -xspc;
			y_above -= h_ann;
			gch->y = y_above;
			break;
		default:			/* guitar chord */
			xspc = w * GCHPRE;
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
	    && (s->abc_type != ABC_T_NOTE
	     || (s->flags & ABC_F_GRACE)))
		s = s->next;
	return s;
}

/* -- parse lyric (vocal) lines (w:) -- */
static struct SYMBOL *get_lyric(struct SYMBOL *s)
{
	struct SYMBOL *s1, *s2;
	char word[128], *p, *q;
	int ln, cont;
	struct FONTSPEC *f;

	curvoice->have_ly = curvoice->posit.voc != SL_HIDDEN;

	if (curvoice->ignore) {
		for (;;) {
			if (!s->abc_next)
				return s;
			switch (s->abc_next->abc_type) {
			case ABC_T_PSCOM:
				s = process_pscomment(s->abc_next);
				continue;
			case ABC_T_INFO:
				if (s->abc_next->text[0] == 'w'
				 || s->abc_next->text[0] == '+') {
					s = s->abc_next;
					continue;
				}
				break;
			}
			return s;
		}
	}

	f = &cfmt.font_tb[cfmt.vof];
	str_font(cfmt.vof);			/* (for tex_str) */

	/* treat all w: lines */
	cont = 0;
	ln = -1;
	s2 = s1 = NULL;				// have gcc happy
	for (;;) {
		if (!cont) {
			if (ln >= MAXLY- 1) {
				error(1, s, "Too many lyric lines");
				ln--;
			}
			ln++;
			s2 = s1;
			s1 = curvoice->lyric_start;
			if (!s1)
				s1 = curvoice->sym;
			else
				s1 = s1->next;
			if (!s1) {
				error(1, s, "w: without music");
				return s;
			}
		} else {
			cont = 0;
		}

		/* scan the lyric line */
		p = &s->text[2];
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
				while (s1 && s1->type != BAR) {
					s2 = s1;
					s1 = s1->next;
				}
				if (!s1) {
					error(1, s2,
						"Not enough bar lines for lyric line");
					goto ly_next;
				}
				s2 = s1;
				s1 = s1->next;
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
				word[0] = '\0';
				p++;
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
			if (s1) {				/* for error */
				s2 = s1;
				s1 = next_lyric_note(s1);
			}
			if (!s1) {
				if (!s2)
					s2 = s;
				error(1, s2, "Too many words in lyric line");
				goto ly_next;
			}
			if (word[0] != '\0'
			 && s1->posit.voc != SL_HIDDEN) {
				struct lyl *lyl;
				float w;

				if (!s1->ly) {
					s1->ly = (struct lyrics *) getarena(sizeof (struct lyrics));
					memset(s1->ly, 0, sizeof (struct lyrics));
				}

				/* handle the font change at start of text */
				q = word;
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
				w = tex_str(q);
				q = tex_buf;
				lyl = (struct lyl *) getarena(sizeof *s1->ly->lyl[0]
							- sizeof s1->ly->lyl[0]->t
							+ strlen(q) + 1);
				s1->ly->lyl[ln] = lyl;
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
			s2 = s1;
			s1 = s1->next;
		}

		/* loop if more lyrics */
ly_next:
		for (;;) {
			if (!s->abc_next)
				goto ly_upd;
			switch (s->abc_next->abc_type) {
			case ABC_T_PSCOM:
				s = process_pscomment(s->abc_next);
				f = &cfmt.font_tb[cfmt.vof];	/* may have changed */
				str_font(cfmt.vof);
				continue;
			case ABC_T_INFO:
				if (s->abc_next->text[0] != 'w'
				 && s->abc_next->text[0] != '+')
					goto ly_upd;
				s = s->abc_next;
				if (s->text[0] == '+')
					cont = 1;
				if (!cont) {
					s1 = next_lyric_note(s1);
					if (s1) {
						error(1, s1,
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
//fixme: no error with abc-2.1
	if (next_lyric_note(s1))
		error(0, s1, "Not enough words for lyric line");
	// fill the w: with 'blank syllabes'
	curvoice->lyric_start = curvoice->last_sym;
	return s;
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
static char txt_no_note[] = "No note in voice overlay";

	/* treat the end of overlay */
	p_voice = curvoice;
	if (p_voice->ignore)
		return;
	if (s->abc_type == ABC_T_BAR
	 || s->u.v_over.type == V_OVER_E)  {
		if (!p_voice->last_sym) {
			error(1, s, txt_no_note);
			return;
		}
		p_voice->last_sym->sflags |= S_BEAM_END;
		over_bar = 0;
		if (over_time < 0) {
			error(1, s, "Erroneous end of voice overlap");
			return;
		}
		if (p_voice->time != over_mxtime)
			error(1, s, tx_wrong_dur);
		curvoice = &voice_tb[over_voice];
		over_mxtime = 0;
		over_voice = -1;
		over_time = -1;
		return;
	}

	/* treat the full overlay start */
	if (s->u.v_over.type == V_OVER_S) {
		over_voice = p_voice - voice_tb;
		over_time = p_voice->time;
		return;
	}

	/* (here is treated a new overlay - '&') */
	/* create the extra voice if not done yet */
	if (!p_voice->last_sym) {
		error(1, s, txt_no_note);
		return;
	}
	p_voice->last_sym->sflags |= S_BEAM_END;
	voice2 = s->u.v_over.voice;
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
		p_voice2->staff = p_voice->staff;
		p_voice2->cstaff = p_voice->cstaff;
		p_voice2->color = p_voice->color;
		p_voice2->map_name = p_voice->map_name;
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
//	p_voice2->cstaff = p_voice2->staff = parsys->voice[voice2].staff
//			= parsys->voice[voice].staff;
//	if ((voice3 = p_voice2->clone) >= 0) {
//		p_voice3 = &voice_tb[voice3];
//		p_voice3->cstaff = p_voice3->staff
//				= parsys->voice[voice3].staff
//				= parsys->voice[p_voice->clone].staff;
//	}

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
		if (over_mxtime == 0)
			over_mxtime = p_voice->time;
		else if (p_voice->time != over_mxtime)
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
	p = s->text + 7;
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
			if (brace && !parenth && !(flags & (OPEN_BRACE | OPEN_BRACE2)))
				flags |= FL_VOICE;
			break;
		case '+':
			flags |= MASTER_VOICE;
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
			for ( ; *p != '\0'; p++) {
				switch (*p) {
				case ' ':
				case '\t':
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
					flags &= ~FL_VOICE;
					flags_st >>= 8;
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
					continue;
				case '|':
					flags |= STOP_BAR;
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
	struct SYMBOL *s2;
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
//		p_voice = curvoice;
		if (parsys->voice[curvoice - voice_tb].range < 0) {
			for (voice = 0; voice < MAXVOICE; voice++) {
				if (parsys->voice[voice].range >= 0) {
					curvoice = &voice_tb[voice];
					break;
				}
			}
/*fixme: should check if voice < MAXVOICE*/
		}
		curvoice->time = maxtime;

		// put the staves before a measure bar (see draw_bar())
		s2 = curvoice->last_sym;
		if (s2 && s2->type == BAR && s2->time == maxtime) {
			curvoice->last_sym = s2->prev;
			if (!curvoice->last_sym)
				curvoice->sym = NULL;
			sym_link(s, STAVES);
			s->next = s2;
			s2->prev = s;
			curvoice->last_sym = s2;
		} else {
			sym_link(s, STAVES); // link the staves in the current voice
		}
		s->state = ABC_S_HEAD; /* (output PS sequences immediately) */
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

	/* create the 'clone' voices */
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
			p_voice2->tblts[0] = p_voice2->tblts[1] = NULL;
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
	if (s->text[3] == 't') {		/* if %%staves */
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
			p_voice2 = p_voice;
			while (i < MAXVOICE) {
				i++;
				p_staff++;
				voice = p_staff->voice;
				p_voice = &voice_tb[voice];
				if (p_staff->flags & MASTER_VOICE) {
					p_voice2->second = 1;
					p_voice2 = p_voice;
				} else {
					p_voice->second = 1;
				}
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
	parsys->nstaff = nstaff = staff;

	/* change the behaviour of '|' in %%score */
	if (s->text[3] == 'c') {		/* if %%score */
		for (staff = 0; staff < nstaff; staff++)
			parsys->staff[staff].flags ^= STOP_BAR;
	}

	for (voice = 0; voice < MAXVOICE; voice++) {
		p_voice = &voice_tb[voice];
		parsys->voice[voice].second = p_voice->second;
		staff = p_voice->staff;
		if (staff > 0)
			p_voice->norepbra
				= !(parsys->staff[staff - 1].flags & STOP_BAR);
		if (p_voice->floating && staff == nstaff)
			p_voice->floating = 0;
	}
	curvoice = &voice_tb[parsys->top_voice];
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
		p_voice->lyric_start = NULL;
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

	p = trim_title(p, NULL);

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
static char *tune_header_rebuild(struct SYMBOL *s)
{
	struct SYMBOL *s2;
	char *header, *p;
	int len;

	len = 0;
	s2 = s;
	for (;;) {
		if (s2->abc_type == ABC_T_INFO) {
			len += strlen(s2->text) + 1;
			if (s2->text[0] == 'K')
				break;
		}
		s2 = s2->abc_next;
	}
	header = malloc(len + 1);
	p = header;
	for (;;) {
		if (s->abc_type == ABC_T_INFO) {
			strcpy(p, s->text);
			p += strlen(p);
			*p++ = '\n';
			if (s->text[0] == 'K')
				break;
		}
		s = s->abc_next;
	}
	*p++ = '\0';
	return header;
}

/* apply the options to the current tune */
static void tune_filter(struct SYMBOL *s)
{
	struct tune_opt_s *opt;
	struct SYMBOL *s1, *s2;
	regex_t r;
	char *header, *p;
	int ret;

	header = tune_header_rebuild(s);
	for (opt = tune_opts; opt; opt = opt->next) {
		struct SYMBOL *last_staves;

		p = &opt->s->text[2 + 5];	/* "%%tune RE" */
		while (isspace((unsigned char) *p))
			p++;

		ret = regcomp(&r, p, REG_EXTENDED | REG_NEWLINE | REG_NOSUB);
		if (ret)
			continue;
		ret = regexec(&r, header, 0, NULL, 0);
		regfree(&r);
		if (ret)
			continue;

		/* apply the options */
		cur_tune_opts = opt;
		last_staves = s->abc_next;
		for (s1 = opt->s->next; s1; s1 = s1->next) {

			/* replace the next %%staves/%%score */
			if (s1->abc_type == ABC_T_PSCOM
			 && (strncmp(&s1->text[2], "staves", 6) == 0
			  || strncmp(&s1->text[2], "score", 5) == 0)) {
				while (last_staves) {
					if (last_staves->abc_type == ABC_T_PSCOM
					 && (strncmp(&last_staves->text[2],
								"staves", 6) == 0
					  || strncmp(&last_staves->text[2],
								 "score", 5) == 0)) {
						last_staves->text = s1->text;
						last_staves = last_staves->abc_next;
						break;
					}
					last_staves = last_staves->abc_next;
				}
				continue;
			}
			s2 = (struct SYMBOL *) getarena(sizeof *s2);
			memcpy(s2, s1, sizeof *s2);
			process_pscomment(s2);
		}
		cur_tune_opts = NULL;
		tune_voice_opts = opt->voice_opts;	// for %%voice
//fixme: what if many %%tune's with %%voice inside?
	}
	free(header);
}

/* apply the options of the current voice */
static void voice_filter(void)
{
	struct voice_opt_s *opt;
	struct SYMBOL *s;
	regex_t r;
	int pass, ret;
	char *p;

	/* scan the global, then the tune options */
	pass = 0;
	opt = voice_opts;
	for (;;) {
		if (!opt) {
			if (pass != 0)
				break;
			opt = tune_voice_opts;
			if (!opt)
				break;
			pass++;
		}
		p = &opt->s->text[2 + 6];	/* "%%voice RE" */
		while (isspace((unsigned char) *p))
			p++;

		ret = regcomp(&r, p, REG_EXTENDED | REG_NOSUB);
		if (ret)
			goto next_voice;
		ret = regexec(&r, curvoice->id, 0, NULL, 0);
		if (ret && curvoice->nm)
			ret = regexec(&r, curvoice->nm, 0, NULL, 0);
		regfree(&r);
		if (ret)
			goto next_voice;

		/* apply the options */
		for (s = opt->s->next; s; s = s->next) {
			struct SYMBOL *s2;

			s2 = (struct SYMBOL *) getarena(sizeof *s2);
			memcpy(s2, s, sizeof *s2);
			process_pscomment(s2);
		}
next_voice:
		opt = opt->next;
	}
}

/* -- check if a pseudo-comment may be in the tune header -- */
static int check_header(struct SYMBOL *s)
{
	switch (s->text[2]) {
	case 'E':
		if (strncmp(s->text + 2, "EPS", 3) == 0)
			return 0;
		break;
	case 'm':
		if (strncmp(s->text + 2, "multicol", 8) == 0)
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
		switch (p_voice->key.instr) {
		case 0:
			if (!pipeformat) {
//				p_voice->transpose = cfmt.transpose;
				break;
			}
			//fall thru
		case K_HP:
		case K_Hp:
			if (p_voice->posit.std == 0)
				p_voice->posit.std = SL_BELOW;
			break;
		}
//		if (p_voice->key.empty)
//			p_voice->key.sf = 0;
		if (!cfmt.autoclef
		 && p_voice->s_clef
		 && (p_voice->s_clef->sflags & S_CLEF_AUTO)) {
			p_voice->s_clef->u.clef.type = TREBLE;
			p_voice->s_clef->sflags &= ~S_CLEF_AUTO;
		}
	}

	/* switch to the 1st voice */
	curvoice = &voice_tb[parsys->top_voice];
}

/* -- get the global definitions after the first K: or middle-tune T:'s -- */
static struct SYMBOL *get_global_def(struct SYMBOL *s)
{
	struct SYMBOL *s2;

	for (;;) {
		s2 = s->abc_next;
		if (!s2)
			break;
		switch (s2->abc_type) {
		case ABC_T_INFO:
			switch (s2->text[0]) {
			case 'K':
				s = s2;
				s->state = ABC_S_HEAD;
				get_key(s);
				continue;
			case 'I':
			case 'M':
			case 'Q':
				s = s2;
				s->state = ABC_S_HEAD;
				s = get_info(s);
				continue;
			}
			break;
		case ABC_T_PSCOM:
			if (!check_header(s2))
				break;
			s = s2;
			s->state = ABC_S_HEAD;
			s = process_pscomment(s);
			continue;
		}
		break;
	}
	set_global_def();
	return s;
}

/* save the global note maps */
static void save_maps(void)
{
	struct map *omap, *map;
	struct note_map *onotes, *notes;

	omap = maps;
	if (!omap) {
		maps_glob = NULL;
		return;
	}
	maps_glob = map = getarena(sizeof *maps_glob);
	for (;;) {
		memcpy(map, omap, sizeof *map);
		onotes = omap->notes;
		if (onotes) {
			map->notes = notes = getarena(sizeof *notes);
			for (;;) {
				memcpy(notes, onotes, sizeof *notes);
				onotes = onotes->next;
				if (!onotes)
					break;
				notes->next = getarena(sizeof *notes);
				notes = notes->next;
			}
		}
		omap = omap->next;
		if (!omap)
			break;
		map->next = getarena(sizeof *map);
		map = map->next;
	}
}

/* -- identify info line, store in proper place	-- */
static struct SYMBOL *get_info(struct SYMBOL *s)
{
	struct SYMBOL *s2;
	struct VOICE_S *p_voice;
	char *p;
	char info_type;
	int old_lvl;
	static char *state_txt[] = {"global", "header", "tune"};

	/* change arena to global or tune */
	old_lvl = lvlarena(s->state != ABC_S_GLOBAL);

	info_type = s->text[0];
	switch (info_type) {
	case 'd':
		break;
	case 'I':
		s = process_pscomment(s);	/* same as pseudo-comment */
		break;
	case 'K':
		get_key(s);
		if (s->state != ABC_S_HEAD)
			break;
		info['K' - 'A'] = s;		/* first K:, end of tune header */
		tunenum++;

		if (!epsf) {
//			if (!cfmt.oneperpage)
//				use_buffer = cfmt.splittune != 1;
			bskip(cfmt.topspace);
		}
		a2b("%% --- xref %s\n", &info['X' - 'A']->text[2]); // (for index)
		write_heading();
		block_put();

		/* information for index
		 * (pdfmark must be after title show for Adobe Distiller) */
		s2 = info['T' - 'A'];
		p = &s2->text[2];
		if (*p != '\0') {
			a2b("%% --- font ");
			outft = -1;
			set_font(TITLEFONT);		/* font in comment */
			a2b("\n");
			outft = -1;
		}
		if (cfmt.pdfmark) {
			if (*p != '\0')
				put_pdfmark(p);
			if (cfmt.pdfmark > 1) {
				for (s2 = s2->next; s2; s2 = s2->next) {
					p = &s2->text[2];
					if (*p != '\0')
						put_pdfmark(p);
				}
			}
		}

		nbar = cfmt.measurefirst;	/* measure numbering */
		over_voice = -1;
		over_time = -1;
		over_bar = 0;
		capo = 0;
		reset_gen();

		s = get_global_def(s);

		if (!(cfmt.fields[0] & (1 << ('Q' - 'A'))))
			info['Q' - 'A'] = NULL;

		/* apply the filter for the voice '1' */
		voice_filter();

		/* activate the default tablature if not yet done */
		if (!first_voice->tblts[0])
			set_tblt(first_voice);
		break;
	case 'L':
		switch (s->state) {
		case ABC_S_HEAD: {
			int i, auto_len;

			auto_len = s->u.length.base_length < 0;

			for (i = MAXVOICE, p_voice = voice_tb;
			     --i >= 0;
			     p_voice++)
				p_voice->auto_len = auto_len;
			break;
		    }
		case ABC_S_TUNE:
			curvoice->auto_len = s->u.length.base_length < 0;
			break;
		}
		break;
	case 'M':
		get_meter(s);
		break;
	case 'P': {
		struct VOICE_S *curvoice_sav;

		if (s->state != ABC_S_TUNE) {
			info['P' - 'A'] = s;
			break;
		}

		if (!(cfmt.fields[0] & (1 << ('P' - 'A'))))
			break;

		/*
		 * If not in the main voice, then,
		 * if the voices are synchronized and no P: yet in the main voice,
		 * the misplaced P: goes into the main voice.
		 */ 
		p_voice = &voice_tb[parsys->top_voice];
		if (curvoice != p_voice) {
			if (curvoice->time != p_voice->time)
				break;
			if (p_voice->last_sym && p_voice->last_sym->type == PART)
				break;		// already a P:
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
		if (s->state != ABC_S_TUNE) {
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
		if (s->state == ABC_S_GLOBAL)
			break;
		if (s->state == ABC_S_HEAD)		/* in tune header */
			goto addinfo;
		gen_ly(1);				/* in tune */
		p = &s->text[2];
		if (*p != '\0') {
			write_title(s);
			a2b("%% --- + (%s) ---\n", p);
			if (cfmt.pdfmark)
				put_pdfmark(p);
		}
		voice_init();
		reset_gen();		/* (display the time signature) */
		s = get_global_def(s);
		break;
	case 'U':
		deco[s->u.user.symbol] = parse.deco_tb[s->u.user.value - 128];
		break;
	case 'u':
		break;
	case 'V':
		get_voice(s);

		/* handle here the possible clef which could be replaced
		 * in case of filter */
		if (s->abc_next && s->abc_next->abc_type == ABC_T_CLEF) {
			s = s->abc_next;
			get_clef(s);
		}
		if (s->state == ABC_S_TUNE
		 && !curvoice->last_sym
		 && curvoice->time == 0)
			voice_filter();
		break;
	case 'w':
		if (s->state != ABC_S_TUNE)
			break;
		if (!(cfmt.fields[1] & (1 << ('w' - 'a')))) {
			while (s->abc_next) {
				if (s->abc_next->abc_type != ABC_T_INFO
				 || s->abc_next->text[0] != '+')
					break;
				s = s->abc_next;
			}
			break;
		}
		s = get_lyric(s);
		break;
	case 'W':
		if (s->state == ABC_S_GLOBAL
		 || !(cfmt.fields[0] & (1 << ('W' - 'A'))))
			break;
		goto addinfo;
	case 'X':
		if (!epsf) {
			buffer_eob(0);	/* flush stuff left from %% lines */
			write_buffer();
//fixme: 8.6.2
			if (cfmt.oneperpage)
				close_page();
//			else if (in_page)
			else
				use_buffer = cfmt.splittune != 1;
		}

		memcpy(&dfmt, &cfmt, sizeof dfmt); /* save global values */
		memcpy(&info_glob, &info, sizeof info_glob);
		memcpy(deco_glob, deco, sizeof deco_glob);
		save_maps();
		info['X' - 'A'] = s;
		if (tune_opts)
			tune_filter(s);
		break;
	default:
		if (info_type >= 'A' && info_type <= 'Z') {
			struct SYMBOL *prev;

			if (s->state == ABC_S_TUNE)
				break;
addinfo:
			prev = info[info_type - 'A'];
			if (!prev
			 || (prev->state == ABC_S_GLOBAL
			  && s->state != ABC_S_GLOBAL)) {
				info[info_type - 'A'] = s;
			} else {
				while (prev->next)
					prev = prev->next;
				prev->next = s;
			}
			while (s->abc_next
			    && s->abc_next->abc_type == ABC_T_INFO
			    && s->abc_next->text[0] == '+') {
				prev = s;
				s = s->abc_next;
				prev->next = s;
			}
			s->prev = prev;
			break;
		}
		if (s->state != ABC_S_GLOBAL)
			error(1, s, "%s info '%c:' not treated",
				state_txt[(int) s->state], info_type);
		break;
	}
	lvlarena(old_lvl);
	return s;
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
	if (flags >= 0) {
		head = H_FULL;
	} else switch (flags) {
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

	/* the bar time is correct if there is multi-rests */
	if (s2->type == MREST
	 || s2->type == BAR)		/* in second voice */
		return;
	while (s2->type != BAR && s2->prev)
		s2 = s2->prev;
	time = s2->time;
	auto_time = curvoice->time - time;

	/* remove the invisible rest at start of tune */
	if (time == 0) {
		while (s2 && s2->dur == 0)
			s2 = s2->next;
		if (s2 && s2->abc_type == ABC_T_REST
		 && (s2->flags & ABC_F_INVIS)) {
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
	if (curvoice->wmeasure == auto_time)
		return;				/* already good duration */

	for (; s2; s2 = s2->next) {
		int i, head, dots, nflags;

		s2->time = time;
		if (s2->dur == 0
		 || (s2->flags & ABC_F_GRACE))
			continue;
		s2->dur = s2->dur * curvoice->wmeasure / auto_time;
		time += s2->dur;
		if (s2->type != NOTEREST)
			continue;
		for (i = 0; i <= s2->nhd; i++)
			s2->u.note.notes[i].len = s2->u.note.notes[i].len
					 * curvoice->wmeasure / auto_time;
		identify_note(s2, s2->u.note.notes[0].len,
				&head, &dots, &nflags);
		s2->head = head;
		s2->dots = dots;
		s2->nflags = nflags;
		if (s2->nflags <= -2)
			s2->flags |= ABC_F_STEMLESS;
		else
			s2->flags &= ~ABC_F_STEMLESS;
	}
	curvoice->time = s->time = time;
}

/* -- measure bar -- */
static void get_bar(struct SYMBOL *s)
{
	int bar_type;
	struct SYMBOL *s2;

	if (s->u.bar.repeat_bar
	 && curvoice->norepbra
	 && !curvoice->second)
		s->sflags |= S_NOREPBRA;
	if (curvoice->auto_len)
		adjust_dur(s);

	bar_type = s->u.bar.type;
	s2 = curvoice->last_sym;
	if (s2 && s2->type == SPACE) {
		s2->time--;		// keep the space at the right place
	} else if (s2 && s2->type == BAR) {

		/* remove the invisible repeat bars when no shift is needed */
		if (bar_type == B_OBRA
		 && !s2->text
		 && (curvoice == &voice_tb[parsys->top_voice]
		  || (parsys->staff[curvoice->staff - 1].flags & STOP_BAR)
		  || (s->sflags & S_NOREPBRA))) {
			s2->text = s->text;
			s2->u.bar.repeat_bar = s->u.bar.repeat_bar;
			s2->flags |= s->flags & (ABC_F_RBSTART | ABC_F_RBSTOP);
			s2->sflags |= s->sflags
					& (S_NOREPBRA | S_RBSTART | S_RBSTOP);
			s = s2;
			goto gch_build;
		}

		/* merge back-to-back repeat bars */
		if (bar_type == B_LREP && !s->text) {
			if (s2->u.bar.type == B_RREP) {
				s2->u.bar.type = B_DREP;
				s2->flags |= ABC_F_RBSTOP;
				s2->sflags |= S_RBSTOP;
				return;
			}
			if (s2->u.bar.type == B_DOUBLE) {
				s2->u.bar.type = (B_SINGLE << 8) | B_LREP;
				s2->flags |= ABC_F_RBSTOP;
				s2->sflags |= S_RBSTOP;
				return;
			}
		}
	}

	/* link the bar in the voice */
	/* the bar must appear before a key signature */
	if (s2 && s2->type == KEYSIG
	 && (!s2->prev || s2->prev->type != BAR)) {
		curvoice->last_sym = s2->prev;
		if (!curvoice->last_sym)
			curvoice->sym = NULL;
		sym_link(s, BAR);
		s->next = s2;
		s2->prev = s;
		curvoice->last_sym = s2;
	} else {
		sym_link(s, BAR);
	}
	s->staff = curvoice->staff;	/* original staff */

	/* set some flags */
	switch (bar_type) {
	case B_OBRA:
	case (B_OBRA << 4) + B_CBRA:
		s->flags |= ABC_F_INVIS;
		break;
	case (B_COL << 8) + (B_BAR << 4) + B_COL:
	case (B_COL << 12) + (B_BAR << 8) + (B_BAR << 4) + B_COL:
		bar_type = (B_COL << 4) + B_COL;	/* :|: and :||: -> :: */
		s->u.bar.type = bar_type;
		break;
	case (B_BAR << 4) + B_BAR:
		if (!cfmt.rbdbstop)
			break;
	case (B_OBRA << 4) + B_BAR:
	case (B_BAR << 4) + B_CBRA:
		s->flags |= ABC_F_RBSTOP;
		s->sflags |= S_RBSTOP;
		break;
	}

	if (s->u.bar.dc.n > 0)
		deco_cnv(&s->u.bar.dc, s, NULL); /* convert the decorations */

	/* build the gch */
gch_build:
	if (s->text) {
		if (!s->u.bar.repeat_bar) {
			gch_build(s);	/* build the guitar chords */
		} else {
			s->gch = getarena(sizeof *s->gch * 2);
			memset(s->gch, 0, sizeof *s->gch * 2);
			s->gch->type = 'r';
			s->gch->font = REPEATFONT;
			str_font(REPEATFONT);
			s->gch->w = tex_str(s->text);
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
void do_tune(void)
{
	struct VOICE_S *p_voice;
	struct SYMBOL *s, *s1, *s2;
	int i;

	/* initialize */
	lvlarena(0);
	nstaff = 0;
	staves_found = -1;
	for (i = 0; i < MAXVOICE; i++) {
		p_voice = &voice_tb[i];
		s1 = (struct SYMBOL *) getarena(sizeof *s1);
		memset(s1, 0, sizeof *s1);
		s1->type = CLEF;
		s1->voice = i;
		if (cfmt.autoclef) {
			s1->u.clef.type = AUTOCLEF;
			s1->sflags = S_CLEF_AUTO;
		} else {
			s1->u.clef.type = TREBLE;
		}
		s1->u.clef.line = 2;		/* treble clef on 2nd line */
		p_voice->s_clef = s1;
		p_voice->meter.wmeasure = 1;	// M:none
		p_voice->wmeasure = 1;
		p_voice->scale = 1;
		p_voice->clone = -1;
		p_voice->over = -1;
		p_voice->posit = cfmt.posit;
		p_voice->stafflines = NULL;
//		p_voice->staffscale = 0;
	}
	curvoice = first_voice = voice_tb;
	reset_deco();
	abc2win = 0;
	clip_start.bar = -1;
	clip_end.bar = (short unsigned) ~0 >> 1;

	parsys = NULL;
	system_new();			/* create the 1st staff system */
	parsys->top_voice = parsys->voice[0].range = 0;	/* implicit voice */

	if (!epsf) {
//fixme: 8.6.2
#if 1
// fixme: should already be 0
		use_buffer = 0;
#else
		if (cfmt.oneperpage) {
			use_buffer = 0;
			close_page();
		} else {
			if (in_page)		// ??
				use_buffer = cfmt.splittune != 1;
		}
#endif
	} else {
		use_buffer = 1;
		marg_init();
	}

	/* set the duration of all notes/rests
	 *	(this is needed for tuplets and the feathered beams) */
	for (s = parse.first_sym; s; s = s->abc_next) {
		switch (s->abc_type) {
		case ABC_T_EOLN:
			if (s->u.eoln.type == 2)
				abc2win = 1;
			break;
		case ABC_T_NOTE:
		case ABC_T_REST:
			s->dur = s->u.note.notes[0].len;
			break;
		}
	}

	if (voice_tb[0].id[0] == '\0') {	/* single voice */
		voice_tb[0].id[0] = '1';	/* implicit V:1 */
		voice_tb[0].id[1] = '\0';
	}

	/* scan the tune */
	for (s = parse.first_sym; s; s = s->abc_next) {
		if (s->flags & ABC_F_LYRIC_START)
			curvoice->lyric_start = curvoice->last_sym;
		switch (s->abc_type) {
		case ABC_T_INFO:
			s = get_info(s);
			break;
		case ABC_T_PSCOM:
			s = process_pscomment(s);
			break;
		case ABC_T_NOTE:
		case ABC_T_REST:
			if (curvoice->space
			 && !(s->flags & ABC_F_GRACE)) {
				curvoice->space = 0;
				s->flags |= ABC_F_SPACE;
			}
			get_note(s);
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
			 || (s->flags & ABC_F_SPACE))
				curvoice->space = 1;
			if (cfmt.continueall || cfmt.barsperstaff
			 || s->u.eoln.type == 1)	/* if '\' */
				continue;
			if (s->u.eoln.type == 0		/* if normal eoln */
			 && abc2win
			 && parse.abc_vers != (2 << 16))
				continue;
			if (parsys->voice[curvoice - voice_tb].range == 0
			 && curvoice->last_sym)
				curvoice->last_sym->sflags |= S_EOLN;
			if (!cfmt.alignbars)
				continue;		/* normal */

			/* align bars */
			while (s->abc_next) {		/* treat the lyrics */
				if (s->abc_next->abc_type != ABC_T_INFO)
					break;
				switch (s->abc_next->text[0]) {
				case 'w':
					s = get_info(s->abc_next);
					continue;
				case 'd':
				case 's':
					s = s->abc_next;
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
			buffer_eob(0);
			curvoice = &voice_tb[0];
			continue;
		case ABC_T_MREST: {
			int dur;

			dur = curvoice->wmeasure * s->u.bar.len;
			if (curvoice->second) {
				curvoice->time += dur;
				break;
			}
			sym_link(s, MREST);
			s->dur = dur;
			curvoice->time += dur;
			if (s->text)
				gch_build(s);	/* build the guitar chords */
			if (s->u.bar.dc.n > 0)
				deco_cnv(&s->u.bar.dc, s, NULL);
			break;
		    }
		case ABC_T_MREP: {
			int n;

			s2 = curvoice->last_sym;
			if (!s2 || s2->type != BAR) {
				error(1, s,
				      "No bar before measure repeat");
				break;
			}
			if (curvoice->ignore)
				break;
			n = s->u.bar.len;
			if (curvoice->second) {
				curvoice->time += curvoice->wmeasure * n;
				break;
			}
			s2 = sym_add(curvoice, NOTEREST);
			s2->abc_type = ABC_T_REST;
			s2->flags |= ABC_F_INVIS;
			s2->dur = curvoice->wmeasure;
			curvoice->time += s2->dur;
			if (n == 1) {
				s->abc_next->u.bar.len = n; /* <n> in the next bar */
				break;
			}
			while (--n > 0) {
				s2 = sym_add(curvoice, BAR);
				s2->u.bar.type = B_SINGLE;
				if (n == s->u.bar.len - 1)
					s2->u.bar.len = s->u.bar.len;
				s2 = sym_add(curvoice, NOTEREST);
				s2->abc_type = ABC_T_REST;
				s2->flags |= ABC_F_INVIS;
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
	buffer_eob(1);
	if (epsf) {
		write_eps();
	} else {
		write_buffer();
//		if (!cfmt.oneperpage && in_page)
//			use_buffer = cfmt.splittune != 1;
	}

	if (info['X' - 'A']) {
		memcpy(&cfmt, &dfmt, sizeof cfmt); /* restore global values */
		memcpy(&info, &info_glob, sizeof info);
		memcpy(deco, deco_glob, sizeof deco);
		maps = maps_glob;
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
}

/* check if a K: or M: may go to the tune key and time signatures */
static int is_tune_sig(void)
{
	struct SYMBOL *s;

	if (!curvoice->sym)
		return 1;
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
	struct SYMBOL *s2;
	struct VOICE_S *p_voice;
	int voice;

	p_voice = curvoice;
	s->type = CLEF;
	if (s->abc_prev->abc_type == ABC_T_INFO) {
		switch (s->abc_prev->text[0]) {
		case 'K':
			if (s->abc_prev->state != ABC_S_HEAD)
				break;
			for (voice = 0; voice < MAXVOICE; voice++) {
				voice_tb[voice].s_clef = s;
				if (s->u.clef.type == PERC)
					voice_tb[voice].perc = 1;
			}
			return;
		case 'V':	/* clef relative to a voice definition in the header */
			p_voice = &voice_tb[(int) s->abc_prev->u.voice.voice];
			curvoice = p_voice;
			break;
		}
	}

	if (is_tune_sig()) {
		p_voice->s_clef = s;
	} else {				/* clef change */

#if 0
		sym_link(s, CLEF);
#else
		/* the clef must appear before a key signature or a bar */
		s2 = p_voice->last_sym;
		if (s2 && s2->prev
		 && s2->time == curvoice->time		// if no time skip
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
			p_voice->last_sym = s3->prev;
			sym_link(s, CLEF);
			s->next = s3;
			s3->prev = s;
			p_voice->last_sym = s2;
		} else {
			sym_link(s, CLEF);
		}
#endif
		s->aux = 1;			/* small clef */
	}
	p_voice->perc = s->u.clef.type == PERC;
	if (s->u.clef.type == AUTOCLEF)
		s->sflags |= S_CLEF_AUTO;
}

/* -- treat %%clef -- */
static void clef_def(struct SYMBOL *s)
{
	char *p;
	int clef, clef_line;
	char str[80];

	clef = -1;
	clef_line = 2;
	p = &s->text[2 + 5];		/* skip %%clef */
	while (isspace((unsigned char) *p))
		p++;

	/* clef name */
	switch (*p) {
	case '\"':			/* user clef name */
		p = get_str(str, p, sizeof str);
		s->u.clef.name = (char *) getarena(strlen(str) + 1);
		strcpy(s->u.clef.name, str);
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
		} else if (strncmp(p, "auto", 4) == 0) {
			clef = AUTOCLEF;
			s->sflags |= S_CLEF_AUTO;
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
			s->u.clef.invis = 1;
			s->flags |= ABC_F_INVIS;
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
			s->u.clef.transpose = -7;
		case '+':
			s->u.clef.octave = 1;
			break;
		case '_':
			s->u.clef.transpose = 7;
		case '-':
			s->u.clef.octave = -1;
			break;
		}
	}

	/* handle the clef */
	s->abc_type = ABC_T_CLEF;
	s->u.clef.type = clef;
	s->u.clef.line = clef_line;
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
static void set_k_acc(struct SYMBOL *s)
{
	int i, j, nacc;
	char accs[8], pits[8];
	static char sharp_tb[8] = {26, 23, 27, 24, 21, 25, 22};
	static char flat_tb[8] = {22, 25, 21, 24, 20, 23, 26};

	if (s->u.key.sf > 0) {
		for (nacc = 0; nacc < s->u.key.sf; nacc++) {
			accs[nacc] = A_SH;
			pits[nacc] = sharp_tb[nacc];
		}
	} else {
		for (nacc = 0; nacc < -s->u.key.sf; nacc++) {
			accs[nacc] = A_FT;
			pits[nacc] = flat_tb[nacc];
		}
	}
	for (i = 0; i < s->u.key.nacc; i++) {
		for (j = 0; j < nacc; j++) {
//			if ((pits[j] - s->u.key.pits[i]) % 7 == 0) {
			if (pits[j] == s->u.key.pits[i]) {
				accs[j] = s->u.key.accs[i];
				break;
			}
		}
		if (j == nacc) {
			if (nacc >= sizeof accs) {
				error(1, s, "Too many accidentals");
			} else {
				accs[j] = s->u.key.accs[i];
				pits[j] = s->u.key.pits[i];
				nacc++;
			}
		}
	}
	for (i = 0; i < nacc; i++) {
		s->u.key.accs[i] = accs[i];
		s->u.key.pits[i] = pits[i];
	}
	s->u.key.nacc = nacc;
}

/* -- get a key signature definition (K:) -- */
static void get_key(struct SYMBOL *s)
{
	struct VOICE_S *p_voice;
	struct SYMBOL *s2;
	struct key_s okey;			/* original key */
	int i;
// int delta;

	if (s->u.key.octave != NO_OCTAVE)
		curvoice->octave = s->u.key.octave;
	if (s->u.key.cue > 0)
		curvoice->scale = 0.7;
	else if (s->u.key.cue < 0)
		curvoice->scale = 1;
	if (s->u.key.stafflines)
		curvoice->stafflines = s->u.key.stafflines;
	if (s->u.key.staffscale != 0)
		curvoice->staffscale = s->u.key.staffscale;

	if (s->u.key.empty == 1)		/* clef only */
		return;

	if (s->u.key.sf != 0
	 && !s->u.key.exp
	 && s->u.key.nacc != 0)
		set_k_acc(s);

	memcpy(&okey, &s->u.key, sizeof okey);
	if (s->state == ABC_S_HEAD) {		/* if first K: (start of tune) */
		for (i = MAXVOICE, p_voice = voice_tb;
		     --i >= 0;
		     p_voice++)
			p_voice->transpose = cfmt.transpose;
//		curvoice->transpose = cfmt.transpose;
	}
	if (curvoice->transpose != 0) {
		key_transpose(&s->u.key);

#if 0
		/* transpose explicit accidentals */
//fixme: not correct - transpose adds or removes accidentals...
		if (s->u.key.nacc > 0) {
			struct VOICE_S voice, *voice_sav;
			struct SYMBOL note;

			memset(&voice, 0, sizeof voice);
			voice.transpose = curvoice->transpose;
			memcpy(&voice.ckey, &s->u.key, sizeof voice.ckey);
			voice.ckey.empty = 2;
			voice.ckey.nacc = 0;
			memset(&note, 0, sizeof note);
--fixme
			memcpy(note.u.note.pits, voice.ckey.pits,
					sizeof note.u.note.pits);
			memcpy(note.u.note.accs, voice.ckey.accs,
					sizeof note.u.note.accs);
			note.nhd = s->u.key.nacc;
			voice_sav = curvoice;
			curvoice = &voice;
			note_transpose(&note);
			memcpy(s->u.key.pits, note.u.note.pits,
					sizeof s->u.key.pits);
			memcpy(s->u.key.accs, note.u.note.accs,
					sizeof s->u.key.accs);
			curvoice = voice_sav;
		}
#endif
	}

	// calculate the tonic delta
//	s->u.key.key_delta = (cgd2cde[(s->u.key.sf + 7) % 7] + 14 + s->u.key.mode) % 7;
	s->u.key.key_delta = (cgd2cde[(s->u.key.sf + 7) % 7] + 14) % 7;

	if (s->state == ABC_S_HEAD) {	/* start of tune */
		for (i = MAXVOICE, p_voice = voice_tb;
		     --i >= 0;
		     p_voice++) {
			memcpy(&p_voice->key, &s->u.key,
						sizeof p_voice->key);
			memcpy(&p_voice->ckey, &s->u.key,
						sizeof p_voice->ckey);
			memcpy(&p_voice->okey, &okey,
						sizeof p_voice->okey);
			if (p_voice->key.empty)
				p_voice->key.sf = 0;
			if (s->u.key.octave != NO_OCTAVE)
				p_voice->octave = s->u.key.octave;
			if (s->u.key.stafflines)
				p_voice->stafflines = s->u.key.stafflines;
			if (s->u.key.staffscale != 0)
				p_voice->staffscale = s->u.key.staffscale;
//fixme: update parsys->voice[voice].stafflines = stafflines; ?
		}
		return;
	}

	/* ABC_S_TUNE (K: cannot be ABC_S_GLOBAL) */
	if (is_tune_sig()) {

		/* define the starting key signature */
		memcpy(&curvoice->key, &s->u.key,
					sizeof curvoice->key);
		memcpy(&curvoice->ckey, &s->u.key,
					sizeof curvoice->ckey);
		memcpy(&curvoice->okey, &okey,
					sizeof curvoice->okey);
		switch (curvoice->key.instr) {
		case 0:
			if (!pipeformat) {
//				curvoice->transpose = cfmt.transpose;
				break;
			}
			//fall thru
		case K_HP:
		case K_Hp:
			if (curvoice->posit.std == 0)
				curvoice->posit.std = SL_BELOW;
			break;
		}
		if (curvoice->key.empty)
			curvoice->key.sf = 0;
		return;
	}

	/* key signature change */
	if ((!s->abc_next
	  || s->abc_next->abc_type != ABC_T_CLEF)	/* if not explicit clef */
	 && curvoice->ckey.sf == s->u.key.sf	/* and same key */
	 && curvoice->ckey.nacc == 0
	 && s->u.key.nacc == 0
	 && curvoice->ckey.empty == s->u.key.empty
	 && cfmt.keywarn)			/* (if not key warning,
						 *  keep all key signatures) */
		return;				/* ignore */

	if (!curvoice->ckey.empty)
		s->aux = curvoice->ckey.sf;	/* previous key signature */
	memcpy(&curvoice->ckey, &s->u.key,
				sizeof curvoice->ckey);
	memcpy(&curvoice->okey, &okey,
				sizeof curvoice->okey);
	if (s->u.key.empty)
		s->u.key.sf = 0;

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

	switch (s->state) {
	case ABC_S_GLOBAL:
		/*fixme: keep the values and apply to all tunes?? */
		break;
	case ABC_S_HEAD:
		for (i = MAXVOICE, p_voice = voice_tb;
		     --i >= 0;
		     p_voice++) {
			memcpy(&p_voice->meter, &s->u.meter,
			       sizeof p_voice->meter);
			p_voice->wmeasure = s->u.meter.wmeasure;
		}
		break;
	case ABC_S_TUNE:
		curvoice->wmeasure = s->u.meter.wmeasure;
		if (is_tune_sig()) {
			memcpy(&curvoice->meter, &s->u.meter,
				       sizeof curvoice->meter);
			reset_gen();	/* (display the time signature) */
			break;
		}
		if (s->u.meter.nmeter == 0)
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

	voice = s->u.voice.voice;
	p_voice = &voice_tb[voice];
	if (parsys->voice[voice].range < 0) {
		if (cfmt.alignbars) {
			error(1, s, "V: does not work with %%%%alignbars");
		}
		if (staves_found < 0) {
			if (!s->u.voice.merge) {
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
			parsys->nstaff = nstaff;
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
	if (s->u.voice.fname != 0) {
		p_voice->nm = s->u.voice.fname;
		p_voice->new_name = 1;
	}
	if (s->u.voice.nname != 0)
		p_voice->snm = s->u.voice.nname;
	if (s->u.voice.octave != NO_OCTAVE)
		p_voice->octave = s->u.voice.octave;
	switch (s->u.voice.dyn) {
	case 1:
		p_voice->posit.dyn = SL_ABOVE;
		p_voice->posit.vol = SL_ABOVE;
		break;
	case -1:
		p_voice->posit.dyn = SL_BELOW;
		p_voice->posit.vol = SL_BELOW;
		break;
	}
	switch (s->u.voice.lyrics) {
	case 1:
		p_voice->posit.voc = SL_ABOVE;
		break;
	case -1:
		p_voice->posit.voc = SL_BELOW;
		break;
	}
	switch (s->u.voice.gchord) {
	case 1:
		p_voice->posit.gch = SL_ABOVE;
		break;
	case -1:
		p_voice->posit.gch = SL_BELOW;
		break;
	}
	switch (s->u.voice.stem) {
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
	switch (s->u.voice.gstem) {
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
	if (s->u.voice.scale != 0)
		p_voice->scale = s->u.voice.scale;
	else if (s->u.voice.cue > 0)
		p_voice->scale = 0.7;
	else if (s->u.voice.cue < 0)
		p_voice->scale = 1;
	if (s->u.voice.stafflines)
		p_voice->stafflines = s->u.voice.stafflines;
	if (s->u.voice.staffscale != 0)
		p_voice->staffscale = s->u.voice.staffscale;
	if (!p_voice->combine)
		p_voice->combine = cfmt.combinevoices;

	set_tblt(p_voice);

	/* if in tune, switch to this voice */
	if (s->state == ABC_S_TUNE)
		curvoice = p_voice;
}

/* sort the notes of the chord by pitch (lowest first) */
void sort_pitch(struct SYMBOL *s)
{
	int i, nx, k;
	struct note v_note;
	unsigned char new_order[MAXHD], inv_order[MAXHD];

	for (i = 0; i <= s->nhd; i++)
		new_order[i] = i;
	for (;;) {
		nx = 0;
		for (i = 1; i <= s->nhd; i++) {
			if (s->u.note.notes[i].pit >= s->u.note.notes[i - 1].pit)
				continue;
			memcpy(&v_note, &s->u.note.notes[i],
					sizeof v_note);
			memcpy(&s->u.note.notes[i], &s->u.note.notes[i - 1],
					sizeof v_note);
			memcpy(&s->u.note.notes[i - 1], &v_note,
					sizeof v_note);
			k = s->pits[i];
			s->pits[i] = s->pits[i - 1];
			s->pits[i - 1] = k;
			k = new_order[i];
			new_order[i] = new_order[i - 1];
			new_order[i - 1] = k;
			nx++;
		}
		if (nx == 0)
			break;
	}

	/* change the indexes of the note head decorations */
	if (s->nhd > 0) {
		for (i = 0; i <= s->nhd; i++)
			inv_order[new_order[i]] = i;
		for (i = 0; i <= s->u.note.dc.n; i++) {
			k = s->u.note.dc.tm[i].m;
			if (k >= 0)
				s->u.note.dc.tm[i].m = inv_order[k];
		}
	}
}

// set the map of the notes
static void set_map(struct SYMBOL *s)
{
	struct map *map;
	struct note_map *note_map;
	struct note *note;
	int m, delta;

	for (map = maps; map; map = map->next) {
		if (strcmp(map->name, curvoice->map_name) == 0)
			break;
	}
	if (!map)
		return;			// !?

	// loop on the note maps, then on the notes of the chord
	delta = curvoice->ckey.key_delta;
	for (m = 0; m <= s->nhd; m++) {
		note = &s->u.note.notes[m];
		for (note_map = map->notes; note_map; note_map = note_map->next) {
			switch (note_map->type) {
			case MAP_ONE:
				if (note->pit == note_map->pit
				 && note->acc == note_map->acc)
					break;
				continue;
			case MAP_OCT:
				if ((note->pit - note_map->pit + 28 ) % 7 == 0
				 && note->acc == note_map->acc)
					break;
				continue;
			case MAP_KEY:
				if ((note->pit + 28 - delta - note_map->pit) % 7 == 0)
					break;
				continue;
			default: // MAP_ALL
				break;
			}
			note->head = note_map->heads;
			note->color = note_map->color;
			if (note_map->print_pit != -128) {
				note->pit = note_map->print_pit;
				s->pits[m] = note->pit;
				note->acc = note_map->print_acc;
			}
			break;
		}
	}
}

/* -- note or rest -- */
static void get_note(struct SYMBOL *s)
{
	struct SYMBOL *prev;
	int i, m, delta;

	prev = curvoice->last_sym;
	m = s->nhd;

	/* insert the note/rest in the voice */
	sym_link(s,  s->u.note.notes[0].len != 0 ? NOTEREST : SPACE);
	if (!(s->flags & ABC_F_GRACE))
		curvoice->time += s->dur;

	if (curvoice->octave) {
		delta = curvoice->octave * 7;
		for (i = 0; i <= m; i++) {
			s->u.note.notes[i].pit += delta;
			s->pits[i] += delta;
		}
	}

	/* convert the decorations
	 * (!beam-accel! and !beam-rall! may change the note duration)
	 * (!8va(! may change ottava)
	 */
	if (s->u.note.dc.n > 0)
		deco_cnv(&s->u.note.dc, s, prev);

	if (curvoice->ottava) {
		delta = curvoice->ottava;
		for (i = 0; i <= m; i++)
			s->pits[i] += delta;
	}
	s->combine = curvoice->combine;
	s->color = curvoice->color;

	if (curvoice->perc)
		s->sflags |= S_PERC;
	else if (s->abc_type == ABC_T_NOTE
	      && curvoice->transpose != 0)
		note_transpose(s);

	if (!(s->flags & ABC_F_GRACE)) {
		switch (curvoice->posit.std) {
		case SL_ABOVE: s->stem = 1; break;
		case SL_BELOW: s->stem = -1; break;
		case SL_HIDDEN: s->flags |= ABC_F_STEMLESS;; break;
		}
	} else {			/* grace note - adjust its duration */
		int div;

		if (curvoice->key.instr != K_HP
		 && curvoice->key.instr != K_Hp
		 && !pipeformat) {
			div = 2;
			if (!prev
			 || !(prev->flags & ABC_F_GRACE)) {
				if (s->flags & ABC_F_GR_END)
					div = 1;	/* one grace note */
			}
		} else {
			div = 4;			/* bagpipe */
		}
		for (i = 0; i <= m; i++)
			s->u.note.notes[i].len /= div;
		s->dur /= div;
		switch (curvoice->posit.gsd) {
		case SL_ABOVE: s->stem = 1; break;
		case SL_BELOW: s->stem = -1; break;
		case SL_HIDDEN:	s->stem = 2; break;	/* opposite */
		}
	}

	s->nohdi1 = s->nohdi2 = -1;

	/* change the figure of whole measure rests */
	if (s->abc_type == ABC_T_REST) {
		if (s->dur == curvoice->wmeasure) {
			if (s->dur < BASE_LEN * 2)
				s->u.note.notes[0].len = BASE_LEN;
			else if (s->dur < BASE_LEN * 4)
				s->u.note.notes[0].len = BASE_LEN * 2;
			else
				s->u.note.notes[0].len = BASE_LEN * 4;
		}
	} else {

		/* sort the notes of the chord by pitch (lowest first) */
		if (!(s->flags & ABC_F_GRACE)
		 && curvoice->map_name)
			set_map(s);
		sort_pitch(s);
	}

	/* get the max head type, number of dots and number of flags */
	if (!curvoice->auto_len || (s->flags & ABC_F_GRACE)) {
		int head, dots, nflags, l;

		if ((l = s->u.note.notes[0].len) != 0) {
			identify_note(s, l, &head, &dots, &nflags);
			s->head = head;
			s->dots = dots;
			s->nflags = nflags;
			for (i = 1; i <= m; i++) {
				if (s->u.note.notes[i].len == l)
					continue;
				identify_note(s, s->u.note.notes[i].len,
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
		s->flags |= ABC_F_STEMLESS;

	if (s->sflags & (S_TREM1 | S_TREM2)) {
		if (s->nflags > 0)
			s->nflags += s->aux;
		else
			s->nflags = s->aux;
		if ((s->sflags & S_TREM2)
		 && (s->sflags & S_BEAM_END)) {		/* if 2nd note - see deco.c */
			prev->head = s->head;
			prev->aux = s->aux;
			prev->nflags = s->nflags;
			prev->flags |= (s->flags & ABC_F_STEMLESS);
		}
	}

	for (i = 0; i <= m; i++) {
		if (s->u.note.notes[i].sl1 != 0)
			s->sflags |= S_SL1;
		if (s->u.note.notes[i].sl2 != 0)
			s->sflags |= S_SL2;
		if (s->u.note.notes[i].ti1 != 0)
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
	if (s->text)
		gch_build(s);
}

static char *get_val(char *p, float *v)
{
	char tmp[32], *r = tmp;

	while (isspace((unsigned char) *p))
		p++;
	while ((isdigit((unsigned char) *p) && r < &tmp[32 - 1])
	    || *p == '-' || *p == '.')
		*r++ = *p++;
	*r = '\0';
	sscanf(tmp, "%f", v);
	return p;
}

// parse <path .../> from %%beginsvg and convert to Postscript
static void parse_path(char *p, char *q, char *id, int idsz)
{
	struct SYMBOL *s;
	char *buf, *r, *t, *op = NULL, *width, *scale, *trans;
	int i, fill, npar = 0;
	float x1, y1, x, y;
char *rmax;

	r = strstr(p, "class=\"");
	if (!r || r > q)
		return;
	r += 7;
	fill = strncmp(r, "fill", 4) == 0;
	width = strstr(p, "stroke-width:");
	scale = strstr(p, "scale(");
	if (scale && scale > q)
		scale = NULL;
	trans = strstr(p, "translate(");
	if (trans && trans > q)
		trans = NULL;
	for (;;) {
		p = strstr(p, "d=\"");
		if (!p)
			return;
		if (isspace((unsigned char) p[-1]))	// (check not 'id=..")
			break;
		p += 3;
	}
	i = (int) (q - p) * 4 + 200;		// estimated PS buffer size
	if (i > TEX_BUF_SZ)
		buf = malloc(i);
	else
		buf = tex_buf;
rmax=buf + i;
	r = buf;
	*r++ = '/';
	idsz -= 5;
	strncpy(r, id + 4, idsz);
	r += idsz;
	strcpy(r, "{gsave T ");
	r += strlen(r);
	if (scale || trans) {
		if (scale) {
			scale += 6;		// "scale("
			t = get_val(scale, &x1);
			if (*t == ',')
				t = get_val(t + 1, &y1);
			else
				y1 = x1;
		}
		if (trans) {
			trans += 10;		// "translate("
			t = get_val(trans, &x) + 1; //","
			t = get_val(t, &y);
		}
		if (!scale)
			r += sprintf(r, "%.2f %.2f T ", x, -y);
		else if (!trans)
			r += sprintf(r, "%.2f %.2f scale ", x1, y1);
		else if (scale > trans)
			r += sprintf(r, "%.2f %.2f T %.2f %.2f scale ",
					x, -y, x1, y1);
		else
			r += sprintf(r, "%.2f %.2f scale %.2f %.2f T ",
					x1, y1, x, -y);
	}
	strcpy(r, "0 0 M\n");
	r += strlen(r);
	if (width && width < q) {
		*r++ = ' ';
		width += 13;
		while (isdigit(*width) || *width == '.')
			*r++ = *width++;
		*r++ = ' ';
		*r++ = 'S';
		*r++ = 'L';
		*r++ = 'W';
	}
	p += 3;
	for (;;) {
		if (*p == '\0' || *p == '"')
			break;
		switch (*p++) {
		default:
			if ((isdigit((unsigned char) p[-1]))
			 || p[-1] == '-' || p[-1] == '.') {
				if (!npar)
					continue;
				p--;			// same op
				break;
			}
			continue;
		case 'M':
			op = "M";
			npar = 2;
			break;
		case 'm':
			op = "RM";
			npar = 2;
			break;
		case 'L':
			op = "L";
			npar = 2;
			break;
		case 'l':
			op = "RL";
			npar = 2;
			break;
		case 'H':
			op = "H";
			npar = 1;
			break;
		case 'h':
			op = "h";
			npar = 1;
			break;
		case 'V':
			op = "V";
			npar = 1;
			break;
		case 'v':
			*r++ = ' ';
			*r++ = '0';
			op = "RL";
			npar = 1;
			break;
		case 'z':
			op = "closepath";
			npar = 0;
			break;
		case 'C':
			op = "C";
			npar = 6;
			break;
		case 'c':
			op = "RC";
			npar = 6;
			break;
//		case 'A':
//			op = "arc";
//			break;
//		case 'a':
//			op = "arc";
//			break;
		case 'q':
			op = "RC";
			npar = 2;
			p = get_val(p, &x1);
			p = get_val(p, &y1);
			t = get_val(p, &x);
			t = get_val(t, &y);
			r += sprintf(r, " %.2f %.2f %.2f %.2f",
					x1*2/3, -y1*2/3,
					x1+(x-x1)*2/3, -y1-(y-y1)*2/3);
			break;
		case 't':
			op = "RC";
			npar = 2;
			x1 = x - x1;
			y1 = y - y1;
			t = get_val(p, &x);
			t = get_val(t, &y);
			r += sprintf(r, " %.2f %.2f %.2f %.2f",
					x1*2/3, -y1*2/3,
					x1+(x-x1)*2/3, -y1-(y-y1)*2/3);
			break;
		}
		*r++ = ' ';
		for (i = 0; i < npar; i++) {
			while (isspace((unsigned char) *p))
				p++;
			if (i & 1) {		// y is inverted
				if (*p == '-')
					p++;
				else if (*p != '0' || p[1] != ' ')
					*r++ = '-';
			}
			while ((isdigit((unsigned char) *p))
			    || *p == '-' || *p == '.')
				*r++ = *p++;
			*r++ = ' ';
		}
		if (*op == 'h') {
			*r++ = '0';
			*r++ = ' ';
			op = "RL";
		}
		strcpy(r, op);
		r += strlen(r);
if (r + 30 > rmax) bug("Buffer overflow in SVG to PS", 1);
	}
	strcpy(r, fill ? " fill" : " stroke");
	r += strlen(r);
	strcpy(r, "\ngrestore}!");
	r += strlen(r);

	s = getarena(sizeof(struct SYMBOL));
	memset(s, 0, sizeof(struct SYMBOL));
	s->text = getarena(strlen(buf) + 1);
	strcpy(s->text, buf);
	ps_def(s, s->text, 'p');
	if (buf != tex_buf)
		free(buf);
}

// parse <defs> .. </defs> from %%beginsvg
static void parse_defs(char *p, char *q)
{
	char *id, *r;
	int idsz;

	for (;;) {
		id = strstr(p, "id=\"");
		if (!id || id > q)
			return;
		r = strchr(id + 4, '"');
		if (!r)
			return;
		idsz = r + 1 - id;

		// if SVG output, mark the id as defined
		if (svg || epsf > 1) {
			svg_def_id(id, idsz);
			p = r;
			continue;
		}

		// convert SVG to PS
		p = id;
		while (*p != '<')
			p--;
		if (strncmp(p, "<path ", 6) == 0) {
			r = strstr(p, "/>");
			parse_path(p + 6, r, id, idsz);
			if (!r)
				break;
			p = r + 2;
			continue;
		}
		break;
	}
}

// extract the SVG defs from %%beginsvg and
//	convert to PostScript when PS output
//	move to the SVG glyphs when SVG output
static void svg_ps(char *p)
{
	char *q;

	for (;;) {
		q = strstr(p, "<defs>");
		if (!q)
			break;
		p = strstr(q, "</defs>");
		if (!p) {
			error(1, NULL, "No </defs> in %%beginsvg");
			break;
		}
		parse_defs(q + 6, p);
	}
}

/* -- treat a postscript or SVG definition -- */
static void ps_def(struct SYMBOL *s,
			char *p,
			char use)	/* cf user_ps_add() */
{
	if (!svg && epsf <= 1) {		/* if PS output */
		if (secure
//		 || use == 'g'		// SVG
		 || use == 's')		// PS for SVG
			return;
	} else {				/* if SVG output */
		if (use == 'p'		// PS for PS
		 || (use == 'g'		// SVG
		  && file_initialized > 0))
			return;
	}
	if (s->abc_prev)
		s->state = s->abc_prev->state;
	if (s->state == ABC_S_TUNE) {
		if (use == 'g')		// SVG
			return;
		sym_link(s, FMTCHG);
		s->aux = PSSEQ;
		s->text = p;
//		s->flags |= ABC_F_INVIS;
		return;
	}
	if (use == 'g') {			// SVG
		svg_ps(p);
		if (!svg && epsf <= 1)
			return;
	}
	if (file_initialized > 0 || mbf != outbuf)
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

// get a color
static int get_color(char *p)
{
	int i, color;
	static const struct {
		char *name;
		int color;
	} col_tb[] = {
		{ "aqua",	0x00ffff },
		{ "black",	0x000000 },
		{ "blue",	0x0000ff },
		{ "fuchsia",	0xff00ff },
		{ "gray",	0x808080 },
		{ "green",	0x008000 },
		{ "lime",	0x00ff00 },
		{ "maroon",	0x800000 },
		{ "navy",	0x000080 },
		{ "olive",	0x808000 },
		{ "purple",	0x800080 },
		{ "red",	0xff0000 },
		{ "silver",	0xc0c0c0 },
		{ "teal",	0x008080 },
		{ "white",	0xffffff },
		{ "yellow",	0xffff00 },
	};

	if (*p == '#') {
		if (sscanf(p, "#%06x", &color) != 1
		 || (unsigned) color > 0x00ffffff)
			return -1;
		return color;
	}
	for (i = sizeof col_tb / sizeof col_tb[0]; --i >= 0; ) {
		if (strncasecmp(p, col_tb[i].name,
				strlen(col_tb[i].name)) == 0)
			break;
	}
	if (i < 0)
		return -1;
	return col_tb[i].color;
}
/* get a transposition */
static int get_transpose(char *p)
{
	int val, pit1, pit2, acc;
	static int pit_st[7] = {0, 2, 4, 5, 7, 9, 11};

	if (isdigit(*p) || *p == '-' || *p == '+') {
		sscanf(p, "%d", &val);
		val *= 3;
		switch (p[strlen(p) - 1]) {
		default:
			return val;
		case '#':
			val++;
			break;
		case 'b':
			val += 2;
			break;
		}
		if (val > 0)
			return val;
		return val - 3;
	}

	// by music interval
	p = parse_acc_pit(p, &pit1, &acc);
	if (acc < 0) {
		error(1, NULL, "  in %%%%transpose");
		return 0;
	}
	pit1 += 126 - 2;    // for value > 0 and 'C' % 7 == 0
	pit1 = (pit1 / 7) * 12 + pit_st[pit1 % 7];
	switch (acc) {
	case A_DS:
		pit1 += 2;
		break;
	case A_SH:
		pit1++;
		break;
	case A_FT:
		pit1--;
		break;
	case A_DF:
		pit1 -= 2;
		break;
	}
	p = parse_acc_pit(p, &pit2, &acc);
	if (acc < 0) {
		error(1, NULL, "  in %%%%transpose");
		return 0;
	}
	pit2 += 126 - 2;
	pit2 = (pit2 / 7) * 12 + pit_st[pit2 % 7];
	switch (acc) {
	case A_DS:
		pit2 += 2;
		break;
	case A_SH:
		pit2++;
		break;
	case A_FT:
		pit2--;
		break;
	case A_DF:
		pit2 -= 2;
		break;
	}

	val = (pit2 - pit1) * 3;
	switch (acc) {
	default:
		return val;
	case A_DS:
	case A_SH:
		val++;
		break;
	case A_FT:
	case A_DF:
		val += 2;
		break;
	}
	if (val > 0)
		return val;
	return val - 3;
}

// create a note mapping
// %%map map_name note [print [heads]] [param]*
static void get_map(char *p)
{
	struct map *map;
	struct note_map *note_map;
	char *name, *q;
	int l, type, pit, acc;

	if (*p == '\0')
		return;

	/* map name */
	name = p;
	while (!isspace((unsigned char) *p) && *p != '\0')
		p++;
	l = p - name;

	/* base note */
	while (isspace((unsigned char) *p))
		p++;
	if (*p == '*') {
		type = MAP_ALL;
		p++;
	} else if (strncmp(p, "octave,", 7) == 0) {
		type = MAP_OCT;
		p += 7;
	} else if (strncmp(p, "key,", 4) == 0) {
		type = MAP_KEY;
		p += 4;
	} else if (strncmp(p, "all", 3) == 0) {
		type = MAP_ALL;
		while (!isspace((unsigned char) *p) && *p != '\0')
			p++;
	} else {
		type = MAP_ONE;
	}
	if (type != MAP_ALL) {
		p = parse_acc_pit(p, &pit, &acc);
		if (acc < 0)			// if error
			pit = acc = 0;
		if (type == MAP_OCT || type == MAP_KEY) {
			pit %= 7;
			if (type == MAP_KEY)
				acc = A_NULL;
		}
	} else {
		pit = acc = 0;
	}

	// get/create the map
	for (map = maps; map; map = map->next) {
		if (strncmp(name, map->name, l) == 0)
			break;
	}
	if (!map) {
		map = getarena(sizeof *map);
		map->next = maps;
		maps = map;
		map->name = getarena(l + 1);
		strncpy(map->name, name, l);
		map->name[l] = '\0';
		map->notes = NULL;
	}
	for (note_map = map->notes; note_map; note_map = note_map->next) {
		if (note_map->type == type
		 && note_map->pit == pit
		 && note_map->acc == acc)
			break;
	}
	if (!note_map) {
		note_map = getarena(sizeof *note_map);
		memset(note_map, 0, sizeof *note_map);
		note_map->next = map->notes;
		map->notes = note_map;
		note_map->type = type;
		note_map->pit = pit;
		note_map->acc = acc;
		note_map->print_pit = -128;
		note_map->color = -1;
	}

	/* try the optional 'print' and 'heads' parameters */
	while (isspace((unsigned char) *p))
		p++;
	if (*p == '\0')
		return;
	q = p;
	while (!isspace((unsigned char) *q) && *q != '\0') {
		if (*q == '=')
			break;
		q++;
	}
	if (isspace((unsigned char) *q) || *q == '\0') {
		if (*p != '*') {
			p = parse_acc_pit(p, &pit, &acc);
			if (acc >= 0) {
				note_map->print_pit = pit;
				note_map->print_acc = acc;
			}
			if (*p == '\0')
				return;
		}
		p = q;
		while (isspace((unsigned char) *p))
			p++;
		if (*p == '\0')
			return;
		q = p;
		while (!isspace((unsigned char) *q) && *q != '\0') {
			if (*q == '=')
				break;
			q++;
		}
		if (isspace((unsigned char) *q) || *q == '\0') {
			name = p;
			p = q;
			l = p - name;
			note_map->heads = getarena(l + 1);
			strncpy(note_map->heads, name, l);
			note_map->heads[l] = '\0';
		}
	}

	/* loop on the parameters */
	for (;;) {
		while (isspace((unsigned char) *p))
			p++;
		if (*p == '\0')
			break;
		if (strncmp(p, "heads=", 6) == 0) {
			p += 6;
			name = p;
			while (!isspace((unsigned char) *p) && *p != '\0')
				p++;
			l = p - name;
			note_map->heads = getarena(l + 1);
			strncpy(note_map->heads, name, l);
			note_map->heads[l] = '\0';
		} else if (strncmp(p, "print=", 6) == 0) {
			p += 6;
			p = parse_acc_pit(p, &pit, &acc);
			if (acc >= 0) {
				note_map->print_pit = pit;
				note_map->print_acc = acc;
			}
		} else if (strncmp(p, "color=", 6) == 0) {
			int color;

			color = get_color(p + 6);
			if (color < 0) {
				error(1, NULL, "Bad color in %%%%map");
				return;
			}
			note_map->color = color;
		}
		while (!isspace((unsigned char) *p) && *p != '\0')
			p++;
	}
}

/* -- process a pseudo-comment (%% or I:) -- */
static struct SYMBOL *process_pscomment(struct SYMBOL *s)
{
	char w[32], *p, *q;
	int lock, voice;
	float h1;

	p = s->text + 2;		/* skip '%%' */
	q = p + strlen(p) - 5;
	lock = strncmp(q, " lock", 5) == 0;
	if (lock)
		*q = '\0'; 
	p = get_str(w, p, sizeof w);
	if (s->state == ABC_S_HEAD
	 && !check_header(s)) {
		error(1, s, "Cannot have %%%%%s in tune header", w);
		return s;
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
			p = s->text + 2 + 7;
			while (*p != '\0' && *p != '\n')
				p++;
			if (*p == '\0')
				return s;		/* empty */
			ps_def(s, p + 1, use);
			return s;
		}
		if (strcmp(w, "begintext") == 0) {
			int job;

			if (s->state == ABC_S_TUNE) {
				gen_ly(1);
			} else if (s->state == ABC_S_GLOBAL) {
				if (epsf || !in_fname)
					return s;
			}
			p = s->text + 2 + 9;
			while (*p == ' ' || *p == '\t')
				p++;
			if (*p != '\n') {
				job = get_textopt(p);
				while (*p != '\0' && *p != '\n')
					p++;
				if (*p == '\0')
					return s;	/* empty */
			} else {
				job = cfmt.textoption;
			}
			if (job != T_SKIP) {
				p++;
				write_text(w, p, job);
			}
			return s;
		}
		if (strcmp(w, "break") == 0) {
			struct brk_s *brk;

			if (s->state != ABC_S_HEAD) {
				error(1, s, "%%%%%s ignored", w);
				return s;
			}
			if (*p == '\0')
				return s;
			for (;;) {
				brk = malloc(sizeof *brk);
				p = get_symsel(&brk->symsel, p);
				if (!p) {
					error(1, s, "Bad selection in %%%%%s", w);
					return s;
				}
				brk->next = brks;
				brks = brk;
				if (*p != ',' && *p != ' ')
					break;
				p++;
			}
			return s;
		}
		break;
	case 'c':
		if (strcmp(w, "center") == 0)
			goto center;
		if (strcmp(w, "clef") == 0) {
			if (s->state != ABC_S_GLOBAL)
				clef_def(s);
			return s;
		}
		if (strcmp(w, "clip") == 0) {
			if (!cur_tune_opts) {
				error(1, s, "%%%%%s not in %%%%tune sequence", w);
				return s;
			}

			/* %%clip <symbol selection> "-" <symbol selection> */
			if (*p != '-') {
				p = get_symsel(&clip_start, p);
				if (!p) {
					error(1, s, "Bad start in %%%%%s", w);
					return s;
				}
				if (*p != '-') {
					error(1, s, "Lack of '-' in %%%%%s", w);
					return s;
				}
			}
			p++;
			p = get_symsel(&clip_end, p);
			if (!p) {
				error(1, s, "Bad end in %%%%%s", w);
				return s;
			}
			if (clip_start.bar < 0)
				clip_start.bar = 0;
			if (clip_end.bar < clip_start.bar
			 || (clip_end.bar == clip_start.bar
			  && clip_end.time <= clip_start.time)) {
				clip_end.bar = (short unsigned) ~0 >> 1;
			}
			return s;
		}
		break;
	case 'd':
		if (strcmp(w, "deco") == 0) {
			deco_add(p);
			return s;
		}
		if (strcmp(w, "dynamic") == 0) {
			set_voice_param(curvoice, s->state, w, p);
			return s;
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
				return s;
			get_str(line, p, sizeof line);
			if ((fp = open_file(line, "eps", fn)) == NULL) {
				error(1, s, "No such file: %s", line);
				return s;
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
				error(1, s, "No bounding box in '%s'", fn);
				return s;
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
			buffer_eob(0);
			return s;
		}
		break;
	case 'g':
		if (strcmp(w, "gchord") == 0
		 || strcmp(w, "gstemdir") == 0) {
			set_voice_param(curvoice, s->state, w, p);
			return s;
		}
		if (strcmp(w, "glyph") == 0) {
			if (!svg && epsf <= 1)
				glyph_add(p);
			return s;
		}
		break;
	case 'm':
		if (strcmp(w, "map") == 0) {
			get_map(p);
			return s;
		}
		if (strcmp(w, "maxsysstaffsep") == 0) {
			if (s->state != ABC_S_TUNE)
				break;
			parsys->voice[curvoice - voice_tb].maxsep = scan_u(p, 0);
			return s;
		}
		if (strcmp(w, "multicol") == 0) {
			float bposy;

			generate();
			if (strncmp(p, "start", 5) == 0) {
				if (!in_page)
					a2b("%%\n");	/* initialize the output */
				buffer_eob(0);
				bposy = get_bposy();
				multicol_max = multicol_start = bposy;
				lmarg = cfmt.leftmargin;
				rmarg = cfmt.rightmargin;
			} else if (strncmp(p, "new", 3) == 0) {
				if (multicol_start == 0) {
					error(1, s,
					      "%%%%%s new without start", w);
				} else {
					buffer_eob(0);
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
					      "%%%%%s end without start", w);
				} else {
					buffer_eob(0);
					bposy = get_bposy();
					if (bposy > multicol_max)
						bskip((bposy - multicol_max)
								/ cfmt.scale);
					else
						a2b("%%\n");	/* force write_buffer */
					cfmt.leftmargin = lmarg;
					cfmt.rightmargin = rmarg;
					multicol_start = 0;
					buffer_eob(0);
					if (!info['X' - 'A']
					 && !epsf)
						write_buffer();
				}
			} else {
				error(1, s,
				      "Unknown keyword '%s' in %%%%%s", p, w);
			}
			return s;
		}
		break;
	case 'n':
		if (strcmp(w, "newpage") == 0) {
			if (epsf || !in_fname)
				return s;
			if (s->state == ABC_S_TUNE)
				generate();
			buffer_eob(0);
			write_buffer();
//			use_buffer = 0;
			if (isdigit((unsigned char) *p))
				pagenum = atoi(p);
			close_page();
			if (s->state == ABC_S_TUNE)
				bskip(cfmt.topspace);
			return s;
		}
		break;
	case 'p':
		if (strcmp(w, "pos") == 0) {	// %%pos <type> <position>
			p = get_str(w, p, sizeof w);
			set_voice_param(curvoice, s->state, w, p);
			return s;
		}
		if (strcmp(w, "ps") == 0
		 || strcmp(w, "postscript") == 0) {
			ps_def(s, p, 'b');
			return s;
		}
		break;
	case 'o':
		if (strcmp(w, "ornament") == 0) {
			set_voice_param(curvoice, s->state, w, p);
			return s;
		}
		break;
	case 'r':
		if (strcmp(w, "repbra") == 0) {
			if (s->state != ABC_S_TUNE)
				return s;
			curvoice->norepbra = strchr("0FfNn", *p)
						|| *p == '\0';
			return s;
		}
		if (strcmp(w, "repeat") == 0) {
			int n, k;

			if (s->state != ABC_S_TUNE)
				return s;
			if (!curvoice->last_sym) {
				error(1, s,
				      "%%%s cannot start a tune", w);
				return s;
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
					      "Incorrect 1st value in %%%%%s", w);
					return s;
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
						      "Incorrect 2nd value in %%%%%s", w);
						return s;
					}
				}
			}
			s->aux = REPEAT;
			if (curvoice->last_sym->type == BAR)
				s->doty = n;
			else
				s->doty = -n;
			sym_link(s, FMTCHG);
			s->nohdi1 = k;
			s->text = NULL;
			return s;
		}
		break;
	case 's':
		if (strcmp(w, "setbarnb") == 0) {
			if (s->state == ABC_S_TUNE) {
				struct SYMBOL *s2;
				int n;

				n = atoi(p);
				for (s2 = s->abc_next; s2; s2 = s2->abc_next) {
					if (s2->abc_type == ABC_T_BAR) {
						s2->aux = n;
						break;
					}
				}
				return s;
			}
			strcpy(w, "measurefirst");
			break;
		}
		if (strcmp(w, "sep") == 0) {
			float h2, len, lwidth;

			if (s->state == ABC_S_TUNE) {
				gen_ly(0);
			} else if (s->state == ABC_S_GLOBAL) {
				if (epsf || !in_fname)
					return s;
			}
			lwidth = (cfmt.landscape ? cfmt.pageheight : cfmt.pagewidth)
				- cfmt.leftmargin - cfmt.rightmargin;
			h1 = h2 = len = 0;
			if (*p != '\0') {
				h1 = scan_u(p, 0);
				while (*p != '\0' && !isspace((unsigned char) *p))
					p++;
				while (isspace((unsigned char) *p))
					p++;
			}
			if (*p != '\0') {
				h2 = scan_u(p, 0);
				while (*p != '\0' && !isspace((unsigned char) *p))
					p++;
				while (isspace((unsigned char) *p))
					p++;
			}
			if (*p != '\0')
				len = scan_u(p, 0);
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
			buffer_eob(0);
			return s;
		}
		if (strcmp(w, "staff") == 0) {
			int staff;

			if (s->state != ABC_S_TUNE)
				return s;
			if (*p == '+')
				staff = curvoice->cstaff + atoi(p + 1);
			else if (*p == '-')
				staff = curvoice->cstaff - atoi(p + 1);
			else
				staff = atoi(p) - 1;
			if ((unsigned) staff > (unsigned) nstaff) {
				error(1, s, "Bad staff in %%%%%s", w);
				return s;
			}
			curvoice->floating = 0;
			curvoice->cstaff = staff;
			return s;
		}
		if (strcmp(w, "staffbreak") == 0) {
			if (s->state != ABC_S_TUNE)
				return s;
			if (isdigit(*p)) {
				s->xmx = scan_u(p, 0);
				if (s->xmx < 0) {
					error(1, s, "Bad value in %%%%%s", w);
					return s;
				}
				if (p[strlen(p) - 1] == 'f')
					s->doty = 1;
			} else {
				s->xmx = 0.5 CM;
				if (*p == 'f')
					s->doty = 1;
			}
			sym_link(s, STBRK);
			return s;
		}
		if (strcmp(w, "stafflines") == 0) {
			if (isdigit((unsigned char) *p)) {
				switch (atoi(p)) {
				case 0: p = "..."; break;
				case 1: p = "..|"; break;
				case 2: p = ".||"; break;
				case 3: p = ".|||"; break;
				case 4: p = "||||"; break;
				case 5: p = "|||||"; break;
				case 6: p = "||||||"; break;
				case 7: p = "|||||||"; break;
				case 8: p = "||||||||"; break;
				default:
					error(1, s, "Bad number of lines");
					break;
				}
			} else {
				int l;

				l = strlen(p);
				q = p;
				p = getarena(l + 1);
				strcpy(p, q);
			}
			if (s->state != ABC_S_TUNE) {
				for (voice = 0; voice < MAXVOICE; voice++)
					voice_tb[voice].stafflines = p;
			} else {
				curvoice->stafflines = p;
			}
			return s;
		}
		if (strcmp(w, "staffscale") == 0) {
			char *q;
			float scale;

			scale = strtod(p, &q);
			if (scale < 0.3 || scale > 2
			 || (*q != '\0' && *q != ' ')) {
				error(1, s, "Bad value in %%%%%s", w);
				return s;
			}
			if (s->state != ABC_S_TUNE) {
				for (voice = 0; voice < MAXVOICE; voice++)
					voice_tb[voice].staffscale = scale;
			} else {
				curvoice->staffscale = scale;
			}
			return s;
		}
		if (strcmp(w, "staves") == 0
		 || strcmp(w, "score") == 0) {
			if (s->state == ABC_S_GLOBAL)
				return s;
			get_staves(s);
			return s;
		}
		if (strcmp(w, "stemdir") == 0) {
			set_voice_param(curvoice, s->state, w, p);
			return s;
		}
		if (strcmp(w, "sysstaffsep") == 0) {
			if (s->state != ABC_S_TUNE)
				break;
			parsys->voice[curvoice - voice_tb].sep = scan_u(p, 0);
			return s;
		}
		break;
	case 't':
		if (strcmp(w, "text") == 0) {
			int job;

center:
			if (s->state == ABC_S_TUNE) {
				gen_ly(1);
			} else if (s->state == ABC_S_GLOBAL) {
				if (epsf || !in_fname)
					return s;
			}
			if (w[0] == 'c') {
				job = T_CENTER;
			} else {
				job = cfmt.textoption;
				switch(job) {
				case T_SKIP:
					return s;
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
			return s;
		}
		if (strcmp(w, "tablature") == 0) {
			struct tblt_s *tblt;
			int i, j;

			tblt = tblt_parse(p);
			if (tblt == 0)
				return s;

			switch (s->state) {
			case ABC_S_TUNE:
			case ABC_S_HEAD:
				for (i = 0; i < ncmdtblt; i++) {
					if (cmdtblts[i].active)
						continue;
					j = cmdtblts[i].index;
					if (j < 0 || tblts[j] == tblt)
						return s;
				}
				/* !! 2 tblts per voice !! */
				if (curvoice->tblts[0] == tblt
				 || curvoice->tblts[1] == tblt)
					break;
				if (curvoice->tblts[1]) {
					error(1, s,
						"Too many tablatures for voice %s",
						curvoice->id);
					break;
				}
				if (!curvoice->tblts[0])
					curvoice->tblts[0] = tblt;
				else
					curvoice->tblts[1] = tblt;
				break;
			}
			return s;
		}
		if (strcmp(w, "transpose") == 0) {
			struct VOICE_S *p_voice;
			struct SYMBOL *s2;
			int i, val;

			val = get_transpose(p);
			switch (s->state) {
			case ABC_S_GLOBAL:
				cfmt.transpose = val;
				return s;
			case ABC_S_HEAD: {
				cfmt.transpose += val;
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
				return s;
			    }
			}
			curvoice->transpose = cfmt.transpose + val;
			s2 = curvoice->sym;
			if (!s2) {
				memcpy(&curvoice->key, &curvoice->okey,
					sizeof curvoice->key);
				key_transpose(&curvoice->key);
				memcpy(&curvoice->ckey, &curvoice->key,
					sizeof curvoice->ckey);
				if (curvoice->key.empty)
					curvoice->key.sf = 0;
				return s;
			}
			for (;;) {
				if (s2->type == KEYSIG)
					break;
				if (s2->time == curvoice->time) {
					s2 = s2->prev;
					if (s2)
						continue;
				}
				s2 = s;
				s2->abc_type = ABC_T_INFO;
				s2->text = (char *) getarena(2);
				s2->text[0] = 'K';
				s2->text[1] = '\0';
				sym_link(s2, KEYSIG);
//				if (!curvoice->ckey.empty)
//					s2->aux = curvoice->ckey.sf;
				s2->aux = curvoice->key.sf;
				break;
			}
			memcpy(&s2->u.key, &curvoice->okey,
						sizeof s2->u.key);
			key_transpose(&s2->u.key);
			memcpy(&curvoice->ckey, &s2->u.key,
						sizeof curvoice->ckey);
			if (curvoice->key.empty)
				s2->u.key.sf = 0;
			return s;
		}
		if (strcmp(w, "tune") == 0) {
			struct SYMBOL *s2, *s3;
			struct tune_opt_s *opt, *opt2;

			if (s->state != ABC_S_GLOBAL) {
				error(1, s, "%%%%%s ignored", w);
				return s;
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
				return s;
			}

			if (strcmp(p, "end") == 0)
				return s;	/* end of previous %%tune */

			/* search the end of the tune options */
			s2 = s;
			for (;;) {
				s3 = s2->abc_next;
				if (!s3)
					break;
				if (s3->abc_type != ABC_T_NULL
				 && (s3->abc_type != ABC_T_PSCOM
				  || strncmp(&s3->text[2], "tune ", 5) == 0))
					break;
				s2 = s3;
			}

			/* search if already a same %%tune */
			opt2 = NULL;
			for (opt = tune_opts; opt; opt = opt->next) {
				if (strcmp(opt->s->text, s->text) == 0)
					break;
				opt2 = opt;
			}

			if (opt) {
				free_voice_opt(opt->voice_opts);
				if (s2 == s) {			/* no option */
					if (!opt2)
						tune_opts = opt->next;
					else
						opt2->next = opt->next;
					free(opt);
					return s;
				}
				opt->voice_opts = NULL;
			} else {
				if (s2 == s)			/* no option */
					return s;
				opt = malloc(sizeof *opt);
				memset(opt, 0, sizeof *opt);
				opt->next = tune_opts;
				tune_opts = opt;
			}

			/* link the options */
			opt->s = s3 = s;
			cur_tune_opts = opt;
			s = s->abc_next;
			for (;;) {
				if (s->abc_type != ABC_T_PSCOM)
					continue;
				if (strncmp(&s->text[2], "voice ", 6) == 0) {
					s = process_pscomment(s);
				} else {
					s->state = ABC_S_HEAD;

					/* !! no reverse link !! */
					s3->next = s;
					s3 = s;
				}
				if (s == s2)
					break;
				s = s->abc_next;
			}
			cur_tune_opts = NULL;
			return s;
		}
		break;
	case 'u':
		if (strcmp(w, "user") == 0) {
			deco[s->u.user.symbol] = parse.deco_tb[s->u.user.value - 128];
			return s;
		}
		break;
	case 'v':
		if (strcmp(w, "vocal") == 0) {
			set_voice_param(curvoice, s->state, w, p);
			return s;
		}
		if (strcmp(w, "voice") == 0) {
			struct SYMBOL *s2, *s3;
			struct voice_opt_s *opt, *opt2;

			if (s->state != ABC_S_GLOBAL) {
				error(1, s, "%%%%voice ignored");
				return s;
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
				return s;
			}

			if (strcmp(p, "end") == 0)
				return s;	/* end of previous %%voice */

			if (cur_tune_opts)
				opt = cur_tune_opts->voice_opts;
			else
				opt = voice_opts;

			/* search the end of the voice options */
			s2 = s;
			for (;;) {
				s3 = s2->abc_next;
				if (!s3)
					break;
				if (s3->abc_type != ABC_T_NULL
				 && (s3->abc_type != ABC_T_PSCOM
				  || strncmp(&s3->text[2], "score ", 6) == 0
				  || strncmp(&s3->text[2], "staves ", 7) == 0
				  || strncmp(&s3->text[2], "tune ", 5) == 0
				  || strncmp(&s3->text[2], "voice ", 6) == 0))
					break;
				s2 = s3;
			}

			/* if already the same %%voice
			 * remove the options */
			opt2 = NULL;
			for ( ; opt; opt = opt->next) {
				if (strcmp(opt->s->text, s->text) == 0) {
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
			if (s2 == s)		/* no option */
				return s;
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
			opt->s = s3 = s;
			for ( ; s != s2; s = s->abc_next) {
				if (s->abc_next->abc_type != ABC_T_PSCOM)
					continue;
				s->abc_next->state = ABC_S_TUNE;
				s3->next = s->abc_next;
				s3 = s3->next;
			}
			return s;
		}
		if (strcmp(w, "voicecolor") == 0) {
			int color;

			if (!curvoice)
				return s;

			color = get_color(p);
			if (color < 0)
				error(1, s, "Bad color in %%%%voicecolor");
			else
				curvoice->color = color;
			return s;
		}
		if (strcmp(w, "voicecombine") == 0) {
			int combine;

			if (sscanf(p, "%d", &combine) != 1) {
				error(1, s, "Bad value in %%%%voicecombine");
				return s;
			}
			switch (s->state) {
			case ABC_S_GLOBAL:
				cfmt.combinevoices = combine;
				break;
			case ABC_S_HEAD:
				for (voice = 0; voice < MAXVOICE; voice++)
					voice_tb[voice].combine = combine;
				break;
			default:
				curvoice->combine = combine;
				break;
			}
			return s;
		}
		if (strcmp(w, "voicemap") == 0) {
			if (s->state != ABC_S_TUNE) {
				for (voice = 0; voice < MAXVOICE; voice++)
					voice_tb[voice].map_name = p;
			} else {
				curvoice->map_name = p;
			}
			return s;
		}
		if (strcmp(w, "voicescale") == 0) {
			char *q;
			float scale;

			scale = strtod(p, &q);
			if (scale < 0.6 || scale > 1.5
			 || (*q != '\0' && *q != ' ')) {
				error(1, s, "Bad %%%%voicescale value");
				return s;
			}
			if (s->state != ABC_S_TUNE) {
				for (voice = 0; voice < MAXVOICE; voice++)
					voice_tb[voice].scale = scale;
			} else {
				curvoice->scale = scale;
			}
			return s;
		}
		if (strcmp(w, "volume") == 0) {
			set_voice_param(curvoice, s->state, w, p);
			return s;
		}
		if (strcmp(w, "vskip") == 0) {
			if (s->state == ABC_S_TUNE) {
				gen_ly(0);
			} else if (s->state == ABC_S_GLOBAL) {
				if (epsf || !in_fname)
					return s;
			}
			bskip(scan_u(p, 0));
			buffer_eob(0);
			return s;
		}
		break;
	}
	if (s->state == ABC_S_TUNE) {
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
		first_voice = curvoice = voice_tb;
		for (i = 0; i < cfmt.alignbars; i++) {
			voice_tb[i].staff = voice_tb[i].cstaff = i;
			voice_tb[i].next = &voice_tb[i + 1];
			parsys->staff[i].flags |= STOP_BAR;
			parsys->voice[i].staff = i;
			parsys->voice[i].range = i;
		}
		i--;
		voice_tb[i].next = NULL;
		parsys->nstaff = nstaff = i;
	}
	return s;
}

/* -- set the duration of notes/rests in a tuplet -- */
/*fixme: KO if voice change*/
/*fixme: KO if in a grace sequence*/
static void set_tuplet(struct SYMBOL *t)
{
	struct SYMBOL *s, *s1;
	int l, r, lplet, grace;

	r = t->u.tuplet.r_plet;
	grace = t->flags & ABC_F_GRACE;

	l = 0;
	for (s = t->abc_next; s; s = s->abc_next) {
		if (s->abc_type == ABC_T_TUPLET) {
			struct SYMBOL *s2;
			int l2, r2;

			r2 = s->u.tuplet.r_plet;
			l2 = 0;
			for (s2 = s->abc_next; s2; s2 = s2->abc_next) {
				switch (s2->abc_type) {
				case ABC_T_NOTE:
				case ABC_T_REST:
					break;
				case ABC_T_EOLN:
					if (s2->u.eoln.type != 1) {
						error(1, t,
							"End of line found inside a nested tuplet");
						return;
					}
					continue;
				default:
					continue;
				}
				if (s2->u.note.notes[0].len == 0)
					continue;
				if (grace ^ (s2->flags & ABC_F_GRACE))
					continue;
				s1 = s2;
				l2 += s1->dur;
				if (--r2 <= 0)
					break;
			}
			l2 = l2 * s->u.tuplet.q_plet / s->u.tuplet.p_plet;
			s->aux = l2;
			l += l2;
			r -= s->u.tuplet.r_plet;
			if (r == 0)
				break;
			if (r < 0) {
				error(1, t, "Bad nested tuplet");
				break;
			}
			s = s2;
			continue;
		}
		switch (s->abc_type) {
		case ABC_T_NOTE:
		case ABC_T_REST:
			break;
		case ABC_T_EOLN:
			if (s->u.eoln.type != 1) {
				error(1, t, "End of line found inside a tuplet");
				return;
			}
			continue;
		default:
			continue;
		}
		if (s->u.note.notes[0].len == 0)	/* space ('y') */
			continue;
		if (grace ^ (s->flags & ABC_F_GRACE))
			continue;
		s1 = s;
		l += s->dur;
		if (--r <= 0)
			break;
	}
	if (!s) {
		error(1, t, "End of tune found inside a tuplet");
		return;
	}
	if (t->aux != 0)		/* if nested tuplet */
		lplet = t->aux;
	else
		lplet = (l * t->u.tuplet.q_plet) / t->u.tuplet.p_plet;
	r = t->u.tuplet.r_plet;
	for (s = t->abc_next; s; s = s->abc_next) {
		int olddur;

		if (s->abc_type == ABC_T_TUPLET) {
			int r2;

			r2 = s->u.tuplet.r_plet;
			s1 = s;
			olddur = s->aux;
			s1->aux = (olddur * lplet) / l;
			l -= olddur;
			lplet -= s1->aux;
			r -= r2;
			for (;;) {
				s = s->abc_next;
				if (s->abc_type != ABC_T_NOTE
				 && s->abc_type != ABC_T_REST)
					continue;
				if (s->u.note.notes[0].len == 0)
					continue;
				if (grace ^ (s->flags & ABC_F_GRACE))
					continue;
				if (--r2 <= 0)
					break;
			}
			if (r <= 0)
				goto done;
			continue;
		}
		if (s->abc_type != ABC_T_NOTE && s->abc_type != ABC_T_REST)
			continue;
		if (s->u.note.notes[0].len == 0)
			continue;
		s->sflags |= S_IN_TUPLET;
		if (grace ^ (s->flags & ABC_F_GRACE))
			continue;
		s1 = s;
		olddur = s->dur;
		s1->dur = (olddur * lplet) / l;
		if (--r <= 0)
			break;
		l -= olddur;
		lplet -= s1->dur;
	}
done:
	if (grace) {
		error(1, t, "Tuplets in grace note sequence not yet treated");
	} else {
		sym_link(t, TUPLET);
		t->aux = cfmt.tuplets;
	}
}
