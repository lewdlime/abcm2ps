/*
 * Music generator.
 *
 * This file is part of abcm2ps.
 *
 * Copyright (C) 1998-2017 Jean-François Moine
 * Adapted from abc2ps, Copyright (C) 1996,1997 Michael Methfessel
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "abcm2ps.h"

struct STAFF_S staff_tb[MAXSTAFF];	/* staff table */
struct SYMBOL *tsnext;		/* next line when cut */
float realwidth;		/* real staff width while generating */

static int insert_meter;	/* insert time signature (1) and indent 1st line (2) */
static float beta_last;		/* for last short short line.. */

#define AT_LEAST(a,b)  do { float tmp = b; if(a<tmp) a=tmp; } while (0)

/* width of notes indexed by log2(note_length) */
float space_tb[NFLAGS_SZ] = {
	7, 10, 14.15, 20, 28.3,
	40,				/* crotchet */
	56.6, 80, 113, 150
};
// width of note heads indexed by s->head
float hw_tb[] = {4.5, 5, 6, 8};
static int smallest_duration;

/* upper and lower space needed by rests */
static char rest_sp[NFLAGS_SZ][2] = {
	{18, 18},
	{12, 18},
	{12, 12},
	{8, 12},
	{6, 8},
	{10, 10},			/* crotchet */
	{6, 4},
	{10, 0},
	{10, 4},
	{10, 10}
};

/* set the head of the notes */
static float set_heads(struct SYMBOL *s)
{
	struct note *note;
	int i, m, n;
	char *p, *q, *r;
	float w, wmax;
	static float dx_tb[4] = {
		7, 8, 10, 13.3
	};

	n = s->nhd;
	wmax = -1;
	for (m = 0; m <= n; m++) {
		note = &s->u.note.notes[m];
		p = note->head;			/* list of heads from parsing */
		if (!p)
			continue;
		i = s->head;
		for (;;) {		// search the head for the duration
			q = strchr(p, ',');
			if (!q)
				break;
			if (--i < 0)
				break;
			p = q + 1;
		}
		if (!q)
			q = p + strlen(p);
		r = strchr(p, '/');
		if (r && r < q) {	// search the head for the stem direction
			if (s->stem >= 0)
				q = r;
			else
				p = r + 1;
		}
		r = strchr(p, ':');		// width separator
		if (r && r < q) {
			q = r;
			sscanf(r, ":%f", &w);
			if (w > wmax)
				wmax = w;
		}
		note->head = p;
		note->hlen = q - p;
	}
	if (wmax < 0) {
		wmax = dx_tb[s->head];
		if (s->dur >= BASE_LEN * 2 && s->head == H_OVAL)
			wmax = 13.8;
	}
	s->u.note.sdx = wmax / 2;		// stem offset
	return wmax;
}

/* -- decide whether to shift heads to other side of stem on chords -- */
/* and set the head of the notes */
/* this routine is called only once per tune for normal notes
 * it is called on setting symbol width for grace notes */
static void set_head_shift(struct SYMBOL *s)
{
	int i, i1, i2, n, sig, d, shift, ps;
	float dx, dx_max, dx_head;
//	unsigned char ax_tb[MAXHD], ac_tb[MAXHD];
	/* distance for no overlap - index: [prev acc][cur acc] */
//	static char dt_tb[4][4] = {
//		{5, 5, 5, 5},		/* dble sharp */
//		{5, 6, 6, 6},		/* sharp */
//		{5, 6, 5, 6},		/* natural */
//		{5, 5, 5, 5}		/* flat */
//	};

	/* set the heads of the notes with mapping */
	dx_head = set_heads(s) + 2;

	n = s->nhd;
	if (n == 0)
		return;			// single note

	/* set the head shifts */
	dx = dx_head * 0.78;
	if (s->flags & ABC_F_GRACE)
		dx *= 0.5;
	sig = s->stem;
	if (sig >= 0) {
		i1 = 1;
		i2 = n + 1;
		ps = s->pits[0];
	} else {
		dx = -dx;
		i1 = n - 1;
		i2 = -1;
		ps = s->pits[n];
	}
	shift = 0;
	dx_max = 0;
	for (i = i1; i != i2; i += sig) {
		d = s->pits[i] - ps;
		ps = s->pits[i];
		if (d == 0) {
			if (shift) {		/* unison on shifted note */
				float new_dx = s->u.note.notes[i].shhd =
					s->u.note.notes[i - sig].shhd + dx;
				if (dx_max < new_dx)
					dx_max = new_dx;
				continue;
			}
			if (i + sig != i2	/* second after unison */
//fixme: should handle many unisons after second
			 && ps + sig == s->pits[i + sig]) {
				s->u.note.notes[i].shhd = -dx;
				if (dx_max < -dx)
					dx_max = -dx;
				continue;
			}
		}
		if (d < 0)
			d = -d;
		if (d > 3 || (d >= 2 && s->head < H_SQUARE)) {
			shift = 0;
		} else {
			shift = !shift;
			if (shift) {
				s->u.note.notes[i].shhd = dx;
				if (dx_max < dx)
					dx_max = dx;
			}
		}
	}
	s->xmx = dx_max;				/* shift the dots */
}

// set the accidental shifts for a set of chords
static void acc_shift(struct note *notes[], int n, float dx_head)
{
	int i, i1, ps, p1, acc;
	float dx, dx1;

	// set the shifts from the head shifts
	for (i = n - 1; --i >= 0; ) {	// (no shift on top)
		dx = notes[i]->shhd;
		if (dx == 0 || dx > 0)
			continue;
		dx = dx_head - dx;
		ps = notes[i]->pit;
		for (i1 = n; --i1 >= 0; ) {
			if (!notes[i1]->acc)
				continue;
			p1 = notes[i1]->pit;
			if (p1 < ps - 3)
				break;
			if (p1 > ps + 3)
				continue;
			if (notes[i1]->shac < dx)
				notes[i1]->shac = dx;
		}
	}
	for (i = n; --i >= 0; ) {		// from top to bottom
		acc = notes[i]->acc;

		if (!acc)
			continue;
		dx = notes[i]->shac;
		if (dx == 0) {
			dx = notes[i]->shhd;
			if (dx < 0)
				dx = dx_head - dx;
			else
				dx = dx_head;
		}
		ps = notes[i]->pit;
		for (i1 = n; --i1 > i; ) {
			if (!notes[i1]->acc)
				continue;
			p1 = notes[i1]->pit;
			if (p1 >= ps + 4) {	// pitch far enough
				if (p1 > ps + 4) // if more than a fifth
					continue;
				switch (acc) {
				case A_NULL:
				case A_FT:
				case A_DF:
					continue;
				}
				switch (notes[i1]->acc) {
				case A_NULL:
				case A_FT:
				case A_DF:
					continue;
				}
			}
			if (dx > notes[i1]->shac - 6) {
				dx1 = notes[i1]->shac + 7;
				if (dx1 > dx)
					dx = dx1;
			}
		}
		notes[i]->shac = dx;
	}
}

/* set the horizontal shift of accidentals */
/* this routine is called only once per tune */
static void set_acc_shft(void)
{
	struct SYMBOL *s, *s2;
	int i, staff, t, acc, n, nx;
	float dx_head;
	struct note *notes[MAXHD * 4];	// (max = 4 voices per staff)
	struct note *nt;

	s = tsfirst;
	while (s) {
		if (s->abc_type != ABC_T_NOTE
		 || (s->flags & ABC_F_INVIS)) {
			s = s->ts_next;
			continue;
		}
		staff = s->staff;
		t = s->time;
		acc = 0;
		for (s2 = s; s2; s2 = s2->ts_next) {
			if (s2->time != t
			 || s2->abc_type != ABC_T_NOTE
			 || s2->staff != staff)
				break;
			if (acc)
				continue;
			for (i = 0; i <= s2->nhd; i++) {
				if (s2->u.note.notes[i].acc) {
					acc = 1;
					continue;
				}
			}
		}
		if (!acc) {
			s = s2;
			continue;
		}

		dx_head = set_heads(s) + 2;
		n = 0;
		for ( ; s != s2; s = s->ts_next) {
			for (i = 0; i <= s->nhd; i++)
				notes[n++] = &s->u.note.notes[i];
		}

		// sort the notes
		for (;;) {
			nx = 0;
			for (i = 1; i < n; i++) {
				if (notes[i]->pit >= notes[i - 1]->pit)
					continue;
				nt = notes[i];
				notes[i] = notes[i - 1];
				notes[i - 1] = nt;
				nx++;
			}
			if (nx == 0)
				break;
		}
		acc_shift(notes, n, dx_head);
	}
}

/* -- unlink a symbol -- */
void unlksym(struct SYMBOL *s)
{
//	if (!s->next) {
//		if (s->extra) {
//			s->type = FMTCHG;
//			s->aux = -1;
//			return;
//		}
//	} else {
	if (s->next) {
		s->next->prev = s->prev;
//		if (s->extra) {
//			struct SYMBOL *g;
//
//			g = s->next->extra;
//			if (!g) {
//				s->next->extra = s->extra;
//			} else {
//				for (; g->next; g = g->next)
//					;
//				g->next = s->extra;
//			}
//		}
	}
	if (s->prev)
		s->prev->next = s->next;
	else
		voice_tb[s->voice].sym = s->next;
	if (s->ts_next) {
		if (s->extra) {
			struct SYMBOL *g;

			g = s->ts_next->extra;
			if (!g) {
				s->ts_next->extra = s->extra;
			} else {
				for (; g->next; g = g->next)
					;
				g->next = s->extra;
			}
		}
		if ((s->sflags & S_SEQST)
		 && !(s->ts_next->sflags & S_SEQST)) {
			s->ts_next->sflags |= S_SEQST;
			s->ts_next->shrink = s->shrink;
			s->ts_next->space = s->space;
		}
		if (s->sflags & S_NEW_SY)
			s->ts_next->sflags |= S_NEW_SY;
		s->ts_next->ts_prev = s->ts_prev;
	}
	if (s->ts_prev)
		s->ts_prev->ts_next = s->ts_next;
	if (tsfirst == s)
		tsfirst = s->ts_next;
	if (tsnext == s)
		tsnext = s->ts_next;
}

/* -- check if voice combine may occur -- */
static int may_combine(struct SYMBOL *s)
{
	struct SYMBOL *s2;
	int nhd2;

	s2 = s->ts_next;
	if (!s2 || s2->type != NOTEREST)
		return 0;
	if (s2->voice == s->voice
	 || s2->staff != s->staff
	 || s2->time != s->time
	 || s2->dur != s->dur)
		return 0;
	if (s->combine <= 0
	 && s2->abc_type != s->abc_type)
		return 0;
	if (s->u.note.dc.n + s2->u.note.dc.n >= MAXDC)
		return 0;
//fixme: should check the double decorations
	if (s->gch && s2->gch)
		return 0;
	if (s->abc_type == ABC_T_REST) {
		if (s2->abc_type  == ABC_T_REST
		 && (s->flags & ABC_F_INVIS) && !(s2->flags & ABC_F_INVIS))
			return 0;
		return 1;
	}
	if (s2->ly
	 || (s2->sflags & (S_SL1 | S_SL2))
	 || s2->u.note.slur_st != 0
	 || s2->u.note.slur_end != 0)
		return 0;
	if ((s2->sflags ^ s->sflags) & (S_BEAM_ST | S_BEAM_END))
		return 0;
	nhd2 = s2->nhd;
	if (s->nhd + nhd2 + 1 >= MAXHD)
		return 0;
	if (s->combine <= 1
	 && s->pits[0] <= s2->pits[nhd2] + 1)
		return 0;
	return 1;
}

/* -- combine 2 voices -- */
static void do_combine(struct SYMBOL *s)
{
	struct SYMBOL *s2;
	int i, m, nhd, nhd2, type;

again:
	nhd = s->nhd;
	s2 = s->ts_next;
	s2->extra = NULL;
	if (s->abc_type != s2->abc_type) {	/* if note and rest */
		if (s2->abc_type != ABC_T_REST) {
			s2 = s;
			s = s2->ts_next;
		}
		goto delsym2;
	}
	if (s->abc_type == ABC_T_REST) {
		if ((s->flags & ABC_F_INVIS)
		 && !(s2->flags & ABC_F_INVIS))
			s->flags &= ~ABC_F_INVIS;
		goto delsym2;
	}

	/* combine the voices */
	nhd2 = s2->nhd + 1;
	memcpy(&s->u.note.notes[nhd + 1],
		s2->u.note.notes,
		sizeof s->u.note.notes[0] * nhd2);
	memcpy(&s->pits[nhd + 1], s2->pits,
		sizeof s->pits[0] * (nhd2 + 1));
	s->sflags |= s2->sflags & (S_SL1 | S_SL2 | S_TI1);

	nhd += nhd2;
	s->nhd = nhd;

	sort_pitch(s);			/* sort the notes by pitch */

	if (s->combine >= 3) {		// remove unison heads
		for (m = nhd; m > 0; m--) {
			if (s->u.note.notes[m].pit == s->u.note.notes[m - 1].pit
			 && s->u.note.notes[m].acc == s->u.note.notes[m - 1].acc) {
				memmove(&s->u.note.notes[m - 1],
					&s->u.note.notes[m],
					sizeof s->u.note.notes[0]);
				memmove(&s->pits[m - 1], &s->pits[m],
					sizeof s->pits[0]);
				s->nhd = --nhd;
			}
		}
	}

	s->ymx = 3 * (s->pits[nhd] - 18) + 4;
	s->ymn = 3 * (s->pits[0] - 18) - 4;

	/* force the tie directions */
	type = s->u.note.notes[0].ti1;
	if ((type & 0x0f) == SL_AUTO)
		s->u.note.notes[0].ti1 = SL_BELOW | (type & ~SL_DOTTED);
	type = s->u.note.notes[nhd].ti1;
	if ((type & 0x0f) == SL_AUTO)
		s->u.note.notes[nhd].ti1 = SL_ABOVE | (type & ~SL_DOTTED);
delsym2:
	if (s2->text && !s->text) {
		s->text = s2->text;
		s->gch = s2->gch;
	}
	if (s2->u.note.dc.n > 0) {	// update the added decorations
		int n;

		for (i = 0; i < s2->u.note.dc.n; i++) {
			if (s2->u.note.dc.tm[i].m >= 0)
				s2->u.note.dc.tm[i].m += nhd + 1;
		}
		n = s->u.note.dc.n;
		memcpy(&s->u.note.dc.tm[n],
			s2->u.note.dc.tm,
			sizeof s->u.note.dc.tm[0] * s2->u.note.dc.n);
		s->u.note.dc.n += s2->u.note.dc.n;
	}
	unlksym(s2);			/* remove the next symbol */

	/* there may be more voices */
	if (!(s->sflags & S_IN_TUPLET) && may_combine(s))
		goto again;
}

/* -- try to combine voices */
static void combine_voices(void)
{
	struct SYMBOL *s, *s2, *g;
	int i, r;

	for (s = tsfirst; s->ts_next; s = s->ts_next) {
		if (s->combine < 0)
			continue;
		if (s->combine == 0
		 && s->abc_type != ABC_T_REST)
			continue;
		if (s->sflags & S_IN_TUPLET) {
			g = s->extra;
			if (!g)
				continue;	/* tuplet already treated */
			r = 0;
			for ( ; g; g = g->next) {
				if (g->type == TUPLET
				 && g->u.tuplet.r_plet > r)
					r = g->u.tuplet.r_plet;
			}
			if (r == 0)
				continue;
			i = r;
			for (s2 = s; s2; s2 = s2->next) {
				if (!s2->ts_next)
					break;
				if (s2->type != NOTEREST)
					continue;
				if (!may_combine(s2))
					break;
				if (--i <= 0)
					break;
			}
			if (i > 0)
				continue;
			for (s2 = s; /*s2*/; s2 = s2->next) {
				if (s2->type != NOTEREST)
					continue;
				do_combine(s2);
				if (--r <= 0)
					break;
			}
			continue;
			
		}
		if (s->type != NOTEREST)
			continue;

		if (s->abc_type == ABC_T_NOTE) {
			if (!(s->sflags & S_BEAM_ST))
				continue;
			if (s->sflags & S_BEAM_END) {
				if (may_combine(s))
					do_combine(s);
				continue;
			}
		} else {
			if (may_combine(s))
				do_combine(s);
			continue;
		}
		s2 = s;
		for (;;) {
			if (!may_combine(s2)) {
				s2 = NULL;
				break;
			}
//fixme: may have rests in beam
			if (s2->sflags & S_BEAM_END)
				break;
			do {
				s2 = s2->next;
			} while (s2->type != NOTEREST);
		}
		if (!s2)
			continue;
		s2 = s;
		for (;;) {
			do_combine(s2);
//fixme: may have rests in beam
			if (s2->sflags & S_BEAM_END)
				break;
			do {
				s2 = s2->next;
			} while (s2->type != NOTEREST);
		}
	}
}

/* -- insert a clef change (treble or bass) before a symbol -- */
static struct SYMBOL *insert_clef(struct SYMBOL *s,
				int clef_type,
				int clef_line)
{
	struct VOICE_S *p_voice;
	struct SYMBOL *new_s;
	int staff;

	staff = s->staff;

	/* don't insert the clef between two bars */
	if (s->type == BAR && s->prev && s->prev->type == BAR
/*	 && s->time == s->prev->time */
			)
		s = s->prev;

	/* create the symbol */
	p_voice = &voice_tb[s->voice];
	p_voice->last_sym = s->prev;
	if (!p_voice->last_sym)
		p_voice->sym = NULL;
	p_voice->time = s->time;
	new_s = sym_add(p_voice, CLEF);
	new_s->next = s;
	s->prev = new_s;

	new_s->u.clef.type = clef_type;
	new_s->u.clef.line = clef_line;
	new_s->staff = staff;
	new_s->aux = 1;			/* small clef */
	new_s->sflags &= ~S_SECOND;

	/* link in time */
	while (!(s->sflags & S_SEQST))
		s = s->ts_prev;
//	if (!s->ts_prev || s->ts_prev->type != CLEF)
	if (s->ts_prev->type != CLEF)
		new_s->sflags |= S_SEQST;
	new_s->ts_prev = s->ts_prev;
//	if (new_s->ts_prev)
		new_s->ts_prev->ts_next = new_s;
//	else
//		tsfirst = new_s;
	new_s->ts_next = s;
	s->ts_prev = new_s;
	return new_s;
}

/* -- set the staff of the floating voices -- */
/* this function is called only once per tune */
static void set_float(void)
{
	struct VOICE_S *p_voice;
	int staff, staff_chg;
	struct SYMBOL *s, *s1;

	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		if (!p_voice->floating)
			continue;
		staff_chg = 0;
		staff = p_voice->staff;
		for (s = p_voice->sym; s; s = s->next) {
			signed char up, down;

			if (!s->dur) {
				if (staff_chg)
					s->staff++;
				continue;
			}
			if (!(s->sflags & S_FLOATING)) {
				staff_chg = 0;
				continue;
			}
			if (s->pits[0] >= 19) {		/* F */
				staff_chg = 0;
				continue;
			}
			if (s->pits[s->nhd] <= 12) {	/* F, */
				staff_chg = 1;
				s->staff++;
				continue;
			}
			up = 127;
			for (s1 = s->ts_prev; s1; s1 = s1->ts_prev) {
				if (s1->staff != staff
				 || s1->voice == s->voice)
					break;
#if 1
/*fixme:test again*/
				if (s1->abc_type == ABC_T_NOTE)
#endif
				    if (s1->pits[0] < up)
					up = s1->pits[0];
			}
			if (up == 127) {
				if (staff_chg)
					s->staff++;
				continue;
			}
			if (s->pits[s->nhd] > up - 3) {
				staff_chg = 0;
				continue;
			}
			down = -127;
			for (s1 = s->ts_next; s1; s1 = s1->ts_next) {
				if (s1->staff != staff + 1
				 || s1->voice == s->voice)
					break;
#if 1
/*fixme:test again*/
				if (s1->abc_type == ABC_T_NOTE)
#endif
				    if (s1->pits[s1->nhd] > down)
					down = s1->pits[s1->nhd];
			}
			if (down == -127) {
				if (staff_chg)
					s->staff++;
				continue;
			}
			if (s->pits[0] < down + 3) {
				staff_chg = 1;
				s->staff++;
				continue;
			}
			up -= s->pits[s->nhd];
			down = s->pits[0] - down;
			if (!staff_chg) {
				if (up < down + 3)
					continue;
				staff_chg = 1;
			} else {
				if (up < down - 3) {
					staff_chg = 0;
					continue;
				}
			}
			s->staff++;
		}
	}
}

/* -- set the x offset of the grace notes -- */
static float set_graceoffs(struct SYMBOL *s)
{
	struct SYMBOL *g, *next;
	int m;
	float xx, dx, gspleft, gspinside, gspright;
	struct note *notes[MAXHD];

	gspleft = (cfmt.gracespace >> 16) * 0.1;
	gspinside = ((cfmt.gracespace >> 8) & 0xff) * 0.1;
	gspright = (cfmt.gracespace & 0xff) * 0.1;
	xx = 0;
	for (g = s->extra; ; g = g->next) {
		if (g->type == NOTEREST)
			break;
	}
	g->sflags |= S_BEAM_ST;
	for ( ; ; g = g->next) {
		if (g->type != NOTEREST) {
			if (!g->next)
				break;
			continue;
		}
		set_head_shift(g);
		for (m = 0; m <= g->nhd; m++)
			notes[m] = &g->u.note.notes[m];
		acc_shift(notes, g->nhd + 1, 7);
		dx = 0;
		for (m = g->nhd; m >= 0; m--) {
			if (g->u.note.notes[m].shac > dx)
				dx = g->u.note.notes[m].shac;
		}
		xx += dx;
		g->x = xx;

		if (g->nflags <= 0)
			g->sflags |= S_BEAM_ST | S_BEAM_END;
		next = g->next;
		if (!next) {
			g->sflags |= S_BEAM_END;
			break;
		}
		if (next->nflags <= 0 || (next->flags & ABC_F_SPACE))
			g->sflags |= S_BEAM_END;
		if (g->sflags & S_BEAM_END) {
			next->sflags |= S_BEAM_ST;
			xx += gspinside / 4;
		}
		if (g->nflags <= 0)
			xx += gspinside / 4;
		if (g->y > next->y + 8)
			xx -= 1.5;
		xx += gspinside;
	}

	xx += gspleft + gspright;
	next = s->next;
	if (next
	 && next->abc_type == ABC_T_NOTE) {	/* if before a note */
		if (g->y >= 3 * (next->pits[next->nhd] - 18))
			xx -= 1;		/* above, a bit closer */
		else if ((g->sflags & S_BEAM_ST)
		      && g->y < 3 * (next->pits[0] - 18) - 7)
			xx += 2;	/* below with flag, a bit further */
	}

	/* return the whole width */
	return xx;
}

/* -- compute the width needed by the guitar chords / annotations -- */
static float gchord_width(struct SYMBOL *s,
			  float wlnote,
			  float wlw)
{
	struct SYMBOL *s2;
	struct gch *gch;
	int ix;
	float lspc, rspc, w, alspc, arspc;

	lspc = rspc = alspc = arspc = 0;
	for (ix = 0, gch = s->gch; ix < MAXGCH; ix++, gch++) {
		if (gch->type == '\0')
			break;
		switch (gch->type) {
		default: {		/* default = above */
			float wl;

			wl = -gch->x;
			if (wl > lspc)
				lspc = wl;
			w = gch->w + 2 - wl;
			if (w > rspc)
				rspc = w;
			break;
		    }
		case '<':		/* left */
			w = gch->w + wlnote;
			if (w > alspc)
				alspc = w;
			break;
		case '>':		/* right */
			w = gch->w + s->wr;
			if (w > arspc)
				arspc = w;
			break;
		}
	}

	/* adjust width for no clash */
	s2 = s->prev;
	if (s2) {
		if (s2->gch) {
			for (s2 = s->ts_prev; ; s2 = s2->ts_prev) {
				if (s2 == s->prev) {
					AT_LEAST(wlw, lspc);
					break;
				}
				if (s2->sflags & S_SEQST)
					lspc -= s2->shrink;
			}
		}
		if (alspc != 0)
			AT_LEAST(wlw, alspc);
	}
	s2 = s->next;
	if (s2) {
		if (s2->gch) {
			for (s2 = s->ts_next; ; s2 = s2->ts_next) {
				if (s2 == s->next) {
					AT_LEAST(s->wr, rspc);
					break;
				}
				if (s2->sflags & S_SEQST)
					rspc -= 8;
			}
		}
		if (arspc != 0)
			AT_LEAST(s->wr, arspc);
	}
	return wlw;
}

/* -- set the width needed by the lyrics -- */
static float ly_width(struct SYMBOL *s, float wlw)
{
	struct SYMBOL *k;
	struct lyrics *ly = s->ly;
	struct lyl *lyl;
	struct tblt_s *tblt;
	float align, xx, w;
	int i;

	/* check if the lyrics contain tablature definition */
	for (i = 0; i < 2; i++) {
		tblt = voice_tb[s->voice].tblts[i];
		if (!tblt)
			continue;
		if (tblt->pitch == 0) {		/* yes, no width */
			for (i = 0; i < MAXLY; i++) {
				if ((lyl = ly->lyl[i]) == NULL)
					continue;
				lyl->s = 0;
			}
			return wlw;
		}
	}

	align = 0;
	for (i = 0; i < MAXLY; i++) {
		float swfac, shift;
		char *p;

		lyl = ly->lyl[i];
		if (!lyl)
			continue;
		p = lyl->t;
		w = lyl->w;
		swfac = lyl->f->swfac;
		xx = w + 2 * cwid(' ') * swfac;
		if (s->type == GRACE) {			// %%graceword
			shift = s->wl;
		} else if ((isdigit((unsigned char) *p) && strlen(p) > 2)
		 || p[1] == ':'
		 || *p == '(' || *p == ')') {
			float sz;

			if (*p == '(') {
				sz = cwid((unsigned char) *p);
			} else {
				sz = 0;
				while (*p != '\0') {
/*fixme: KO when '\ooo'*/
					if (*p == '\\') {
						p++;
						continue;
					}
					sz += cwid((unsigned char) *p);
					if (*p == ' ')
						break;
					p++;
				}
			}
			sz *= swfac;
			shift = (w - sz + 2 * cwid(' ') * swfac)
				* VOCPRE;
			if (shift > 20)
				shift = 20;
			shift += sz;
			if (isdigit((unsigned char) lyl->t[0])) {
				if (shift > align)
					align = shift;
			}
		} else if (*p == LY_HYPH || *p == LY_UNDER) {
			shift = 0;
		} else {
			shift = xx * VOCPRE;
			if (shift > 20)
				shift = 20;
		}
		lyl->s = shift;
		AT_LEAST(wlw, shift);
		xx -= shift;
		shift = 2 * cwid(' ') * swfac;
		for (k = s->next; k; k = k->next) {
			switch (k->type) {
			case NOTEREST:
				if (!k->ly
				 || !k->ly->lyl[i])
					xx -= 9;
				else if (k->ly->lyl[i]->t[0] == LY_HYPH
				      || k->ly->lyl[i]->t[0] == LY_UNDER)
					xx -= shift;
				else
					break;
				if (xx <= 0)
					break;
				continue;
			case CLEF:
			case TIMESIG:
			case KEYSIG:
				xx -= 10;
				continue;
			default:
				xx -= 5;
				break;
			}
			break;
		}
		if (xx > s->wr)
			s->wr = xx;
		}
	if (align > 0) {
		for (i = 0; i < MAXLY; i++) {
			if ((lyl = ly->lyl[i]) == 0)
				continue;
			if (isdigit((unsigned char) lyl->t[0]))
				lyl->s = align;
		}
	}
	return wlw;
}

/* -- set the width of a symbol -- */
/* This routine sets the minimal left and right widths wl,wr
 * so that successive symbols are still separated when
 * no extra glue is put between them */
static void set_width(struct SYMBOL *s)
{
	struct SYMBOL *s2;
	int i, m;
	float xx, w, wlnote, wlw;

	switch (s->type) {
	case NOTEREST:

		/* set the note widths */
		s->wr = wlnote = hw_tb[s->head];

		/* room for shifted heads and accidental signs */
		if (s->xmx > 0)
			s->wr += s->xmx + 4;
		s2 = s->prev;
		if (s2) {
			switch (s2->type) {
			case BAR:
			case CLEF:
			case KEYSIG:
			case TIMESIG:
				wlnote += 3;
				break;
			}
		}
		for (m = 0; m <= s->nhd; m++) {
			xx = s->u.note.notes[m].shhd;
			if (xx < 0)
				AT_LEAST(wlnote, -xx + 5);
			if (s->u.note.notes[m].acc) {
				AT_LEAST(wlnote, s->u.note.notes[m].shac
					 + ((s->u.note.notes[m].acc & 0xf8)
					    ? 6.5 : 4.5));
			}
		}
		if (s2) {
			switch (s2->type) {
			case BAR:
			case CLEF:
			case KEYSIG:
			case TIMESIG:
				wlnote -= 3;
				break;
			}
		}

		/* room for the decorations */
		if (s->u.note.dc.n != 0)
			wlnote += deco_width(s);

		/* space for flag if stem goes up on standalone note */
		if ((s->sflags & (S_BEAM_ST | S_BEAM_END)) == (S_BEAM_ST | S_BEAM_END)
		 && s->stem > 0 && s->nflags > 0)
			AT_LEAST(s->wr, s->xmx + 9);	// was 12, then removed, then back again

		/* leave room for dots and set their offset */
		if (s->dots > 0) {
			switch (s->head) {
			case H_SQUARE:
			case H_OVAL:
				s->xmx += 2;
				break;
			case H_EMPTY:
				s->xmx += 1;
				break;
			}
			AT_LEAST(s->wr, s->xmx + 12);
			if (s->dots >= 2)
				s->wr += 3.5 * (s->dots - 1);
		}

		/* if a tremolo on 2 notes, have space for the small beam(s) */
		if ((s->sflags & (S_TREM2 | S_BEAM_END)) == (S_TREM2 | S_BEAM_END))
			AT_LEAST(wlnote, 20);

		wlw = wlnote;

		if (s2) {
			switch (s2->type) {
			case NOTEREST:	/* extra space when up stem - down stem */
				if (s2->abc_type == ABC_T_REST)
					break;
				if (s2->stem > 0 && s->stem < 0)
					AT_LEAST(wlw, 7);

				/* make sure helper lines don't overlap */
				if ((s->y > 27 && s2->y > 27)
				 || (s->y < -3 && s2->y < -3))
					AT_LEAST(wlw, 6);

				/* have ties wide enough */
				if (s2->sflags & S_TI1)
					AT_LEAST(wlw, 14);
				break;
			case CLEF:		/* extra space at start of line */
				if ((s2->sflags & S_SECOND)
				 || s2->aux)
					break;
				wlw += 8;
				break;
			case KEYSIG:
/*			case TIMESIG:	*/
				wlw += 4;
				break;
			}
		}

		/* leave room for guitar chord and annotations */
		if (s->gch)
			wlw = gchord_width(s, wlnote, wlw);

		/* leave room for vocals under note */
		/* related to draw_lyrics() */
		if (s->ly)
			wlw = ly_width(s, wlw);

		/* if preceeded by a grace note sequence, adjust */
		if (s2 && s2->type == GRACE)
			s->wl = wlnote - 4.5;
		else
			s->wl = wlw;
		break;
	case SPACE:
		xx = s->u.note.notes[0].shhd * 0.5;
		s->wr = xx;
		if (s->gch)
			xx = gchord_width(s, xx, xx);
		if (s->u.note.dc.n != 0)
			xx += deco_width(s);
		s->wl = xx;
		break;
	case BAR:
		if (s->sflags & S_NOREPBRA)
			break;
		if (!(s->flags & ABC_F_INVIS)) {
			int bar_type;

			bar_type = s->u.bar.type;
			switch (bar_type) {
			case B_BAR:
				w = 5 + 3;
				break;
			case (B_BAR << 4) + B_COL:
			case (B_COL << 4) + B_BAR:
				w = 5 + 3 + 3 + 5;
				break;
			case (B_COL << 4) + B_COL:
				w = 5 + 5 + 3 + 3 + 3 + 5;
				break;
			default:
				w = 5;
				for (;;) {
					switch (bar_type & 0x0f) {
					case B_OBRA:
					case B_CBRA:
						w += 3;
						break;
					case B_COL:
						w += 2;
						break;
					}
					bar_type >>= 4;
					if (bar_type == 0)
						break;
					w += 3;
				}
				break;
			}
			s->wl = w;
			if (s->next
			 && s->next->type != TIMESIG)
				s->wr = 8;
			else
				s->wr = 5;
//			s->shhd[0] = (w - 5) * -0.5;
		}
		if (s->u.bar.dc.n != 0)
			s->wl += deco_width(s);

		/* have room for the repeat numbers / guitar chord */
		if (s->gch
		 && strlen(s->text) < 4)
			s->wl = gchord_width(s, s->wl, s->wl);
		break;
	case CLEF:
		/* shift the clef to the left - see draw_symbols() */
//		if (!(s->flags & ABC_F_INVIS)) {
			s->wl = 12 + 10;
			s->wr = (s->aux ? 10 : 12) - 10;
//		}
		break;
	case KEYSIG: {
		int n1, n2, esp;

		s->wl = 3;
		esp = 4;
		if (s->u.key.nacc == 0) {
			n1 = s->u.key.sf;	/* new key sig */
			if (cfmt.cancelkey || n1 == 0)
				n2 = s->aux;	/* old key */
			else
				n2 = 0;
			if (n1 * n2 >= 0) {	/* if no natural */
				if (n1 < 0)
					n1 = -n1;
				if (n2 < 0)
					n2 = -n2;
				if (n2 > n1)
					n1 = n2;
			} else {
				n1 -= n2;
				if (n1 < 0)
					n1 = -n1;
				esp += 3;	/* see extra space in draw_keysig() */
			}
		} else {
			int last_acc;

			n1 = n2 = s->u.key.nacc;
			last_acc = s->u.key.accs[0];
			for (i = 1; i < n2; i++) {
				if (s->u.key.pits[i] > s->u.key.pits[i - 1] + 6
				 || s->u.key.pits[i] < s->u.key.pits[i - 1] - 6)
					n1--;		/* octave */
				else if (s->u.key.accs[i] != last_acc)
					esp += 3;
				last_acc = s->u.key.accs[i];
			}
		}
		s->wr = 5.5 * n1 + esp;
		break;
	    }
	case TIMESIG:
		/* !!tied to draw_timesig()!! */
		w = 0;
		for (i = 0; i < s->u.meter.nmeter; i++) {
			int l;

			l = sizeof s->u.meter.meter[i].top;
			if (s->u.meter.meter[i].top[l - 1] == '\0') {
				l = strlen(s->u.meter.meter[i].top);
				if (s->u.meter.meter[i].top[1] == '|'
				 || s->u.meter.meter[i].top[1] == '.')
					l--;		/* 'C|' */
			}
			if (s->u.meter.meter[i].bot[0] != '\0') {
				int l2;

				l2 = sizeof s->u.meter.meter[i].bot;
				if (s->u.meter.meter[i].bot[l2 - 1] == '\0')
					l2 = strlen(s->u.meter.meter[i].bot);
				if (l2 > l)
					l = l2;
			}
			w += 6.5 * l;
		}
		s->wl = w;
		s->wr = w + 7;
		break;
	case MREST:
		s->wl = 40 / 2 + 16;
		s->wr = 40 / 2 + 16;
		break;
	case GRACE:
		s->wl = set_graceoffs(s);
		if (s->ly)
			ly_width(s, 0);
		break;
	case STBRK:
		if (s->next && s->next->type == CLEF) {
			s->wr = 2;
			s->next->aux = 0;	/* big clef */
		} else {
			s->wr = 8;
		}
		s->wl = s->xmx;
		break;
#if 0
	case TEMPO:
	case PART:
	case TUPLET:
	case CUSTOS:
#endif
	case FMTCHG:		/* no space */
		break;
	default:
		bug("Cannot set width for symbol", 1);
	}
}

/* -- set the natural space -- */
static float set_space(struct SYMBOL *s)
{
	struct SYMBOL *s2;
	int i, len, l, stemdir, prev_time;
	float space;

//fixme: s->ts_prev never NULL ?
//	prev_time = !s->ts_prev ? s->time : s->ts_prev->time;
	prev_time = s->ts_prev->time;
	len = s->time - prev_time;		/* time skip */
	if (len == 0) {
		switch (s->type) {
		case MREST:
			return s->wl + 16;
/*fixme:do same thing at start of line*/
		case NOTEREST:
			if (s->ts_prev->type == BAR) {
				i = 2;
				if (s->nflags < -2)
					i = 0;
				return space_tb[i];
			}
			break;
		}
		return 0;
	}
	if (s->ts_prev->type == MREST)
		return s->ts_prev->wr + 16
				+ 3;		// (bar wl=5 wr=8)
	if (smallest_duration >= MINIM) {
		if (smallest_duration >= SEMIBREVE)
			len /= 4;
		else
			len /= 2;
	}
	if (len >= CROTCHET) {
		if (len < MINIM)
			i = 5;
		else if (len < SEMIBREVE)
			i = 6;
		else if (len < BREVE)
			i = 7;
		else if (len < BREVE * 2)
			i = 8;
		else
			i = 9;
	} else {
		if (len >= QUAVER)
			i = 4;
		else if (len >= SEMIQUAVER)
			i = 3;
		else if (len >= SEMIQUAVER / 2)
			i = 2;
		else if (len >= SEMIQUAVER / 4)
			i = 1;
		else
			i = 0;
	}
	l = len - ((SEMIQUAVER / 8) << i);
	space = space_tb[i];
	if (l != 0) {
		if (l < 0) {
			space = space_tb[0] * len / (SEMIQUAVER / 8);
		} else {
			if (i >= 9)
				i = 8;
			space += (space_tb[i + 1] - space_tb[i])
				* l / len;
		}
	}
	if (s->dur == 0) {
		if (s->type == BAR) {
			if (s->u.bar.type & 0xf0)
				space *= 0.8;	/* complex bar */
			else
				space *= 0.7;
		}
		return space;
	}

	/* reduce spacing within a beam */
	if (!(s->sflags & S_BEAM_ST))
		space *= fnnp;

	/* decrease spacing when stem down followed by stem up */
/*fixme:to be done later, after x computed in sym_glue*/
	if (s->abc_type == ABC_T_NOTE && s->nflags >= -1
	 && s->stem > 0) {
		stemdir = 1;
		for (s2 = s->ts_prev;
		     s2 && s2->time == prev_time;
		     s2 = s2->ts_prev) {
			if (s2->nflags < -1 || s2->stem > 0) {
				stemdir = 0;
				break;
			}
		}
		if (stemdir) {
			for (s2 = s->ts_next;
			     s2 && s2->time == s->time;
			     s2 = s2->ts_next) {
				if (s2->nflags < -1 || s2->stem < 0) {
					stemdir = 0;
					break;
				}
			}
			if (stemdir)
				space *= 0.9;
		}
	}
	return space;
}

/* -- set the width and space of all symbols -- */
/* this function is called once for the whole tune
 * then, once per music line up to the first sequence */
static void set_allsymwidth(struct SYMBOL *last_s)
{
#if 1
//	float space;
	float new_val, maxx;
	struct SYMBOL *s = tsfirst, *s2;
	float xa = 0;
	float xl[MAXSTAFF];

	memset(xl, 0, sizeof xl);

	/* loop on all symbols */
	while (1) {
		maxx = xa;
		s2 = s;
//		space = 0;
		do {
			set_width(s);
			new_val = xl[s->staff] + s->wl;
			if (new_val > maxx)
				maxx = new_val;

//			if (s->ts_prev) {
//				new_val = set_space(s);
//				if (space < new_val)
//					space = new_val;
//			}

			s = s->ts_next;
		} while (s != last_s && (s->sflags & S_SEQST) == 0);

		/* set the spaces at start of sequence */
		s2->shrink = maxx - xa;
		if (s2->ts_prev)
			s2->space = set_space(s2);

		if (s2->shrink == 0 && s2->space == 0 && s2->type == CLEF) {
			s2->sflags &= ~S_SEQST;		/* no space */
			s2->time = s2->ts_prev->time;
		}
		if (s == last_s)
			return;

		// update the min left space per staff
		xa = maxx;
		s = s2;
		do {
			if (xl[s->staff] < xa + s->wr)
				xl[s->staff] = xa + s->wr;
			s = s->ts_next;
		} while ((s->sflags & S_SEQST) == 0);
	}
	// not reached
#else
	struct VOICE_S *p_voice;
	struct SYMBOL *s, *s2, *s3;
	struct tblt_s *tblt;
	int i;
	float new_val, shrink, space;

	/* set the space of the starting symbols */
	new_val = 0;
	s = tsfirst;
	for (;;) {
		set_width(s);
		if (new_val < s->wl)
			new_val = s->wl;
		s = s->ts_next;
		if (s == last_s || (s->sflags & S_SEQST))
			break;
	}
	tsfirst->shrink = new_val;

	/* loop on all remaining symbols */
	while (s != last_s) {
		s2 = s;
		shrink = space = 0;
		do {
			int ymx1, ymn1, ymx2, ymn2;
			float wl;

			/* set the minimum space before and after the symbol */
			set_width(s2);

			/* calculate the minimum space before the symbol,
			 * looping in the previous time sequence */
			if (s2->type == BAR) {
				ymx1 = 50;
				ymn1 = -50;
			} else {
				ymx1 = s2->ymx;
				ymn1 = s2->ymn;
			}
			wl = s2->wl;
			new_val = 0;
			for (s3 = s->ts_prev; s3; s3 = s3->ts_prev) {
				if (new_val < s3->wr
				 && s3->type == NOTEREST
				 && s2->type == NOTEREST)
					new_val = s3->wr;
				if (s3->staff == s2->staff
				 && (!(s3->flags & ABC_F_INVIS)
				  || s3->voice == s2->voice)
				 && new_val < s3->wr + wl) {
					switch (s3->type) {
					case NOTEREST:
						if (s2->type == NOTEREST) {
							new_val = s3->wr + wl;
							break;
						}
						/* fall thru */
					default:
						ymx2 = s3->ymx;
						ymn2 = s3->ymn;
						if (ymn1 > ymx2
						 || ymx1 < ymn2)
							break;
						/* fall thru */
					case SPACE:
					case BAR:
					case CLEF:
					case TIMESIG:
					case KEYSIG:
						new_val = s3->wr + wl;
						break;
					}
				}
				if (s3->sflags & S_SEQST) {
					if (new_val != 0)
						break;
					wl -= s3->shrink;
					if (wl < 0)
						break;
				}
			}
			if (shrink < new_val)
				shrink = new_val;
			new_val = set_space(s2);
			if (space < new_val)
				space = new_val;
			if ((s2 = s2->ts_next) == last_s)
				break;
		} while (!(s2->sflags & S_SEQST));

		/* set the spaces at start of sequence */
		if (shrink == 0 && space == 0 && s->type == CLEF) {
			s->sflags &= ~S_SEQST;		/* no space */
			s->time = s->ts_prev->time;
		} else {
			s->shrink = shrink;
			s->space = space;
		}
		s = s2;
	}

	/* have room for the tablature header */
	space = 0;
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		for (i = 0; i < 2; i++) {
			if ((tblt = p_voice->tblts[i]) == NULL)
				continue;
			if (tblt->wh > space)
				space = tblt->wh;
		}
	}
	if (space == 0)
		return;
	shrink = 0;
	for (s = tsfirst; s != last_s; s = s->ts_next) {
		if (s->shrink != 0)
			shrink += s->shrink;
		if (s->abc_type == ABC_T_NOTE)
			break;
	}
	if (s != last_s && shrink < space) {
		while (!(s->sflags & S_SEQST))
			s = s->ts_prev;
		s->shrink += space - shrink;
	}
#endif
}

/* change a symbol into a rest */
static void to_rest(struct SYMBOL *s)
{
	s->type = NOTEREST;
	s->abc_type = ABC_T_REST;
	s->sflags &= S_NL | S_SEQST;
	s->doty = -1;
	s->u.note.dc.n = 0;
	s->gch = NULL;
	s->extra = NULL;
	s->u.note.slur_st = s->u.note.slur_end = 0;
/*fixme: should set many parameters for set_width*/
//	set_width(s);
}

/* -- set the repeat sequences / measures -- */
static void set_repeat(struct SYMBOL *g,	/* repeat format */
			struct SYMBOL *s)	/* first note */
{
	struct SYMBOL *s2, *s3;
	int i, j, n, dur, staff, voice;

	staff = s->staff;
	voice = s->voice;

	/* treat the sequence repeat */
	if ((n = g->doty) < 0) {		/* number of notes / measures */
		n = -n;
		i = n;				/* number of notes to repeat */
		for (s3 = s->prev; s3; s3 = s3->prev) {
			if (s3->dur == 0) {
				if (s3->type == BAR) {
					error(0, s3, "Bar in sequence to repeat");
					goto delrep;
				}
				continue;
			}
			if (--i <= 0)
				break;
		}
		if (!s3) {
			error(0, s, "Not enough symbols to repeat");
			goto delrep;
		}
		dur = s->time - s3->time;

		i = g->nohdi1 * n;	/* number of notes/rests to repeat */
		for (s2 = s; s2; s2 = s2->next) {
			if (s2->dur == 0) {
				if (s2->type == BAR) {
					error(0, s2, "Bar in repeat sequence");
					goto delrep;
				}
				continue;
			}
			if (--i <= 0)
				break;
		}
		if (!s2
		 || !s2->next) {		/* should have some symbol */
			error(0, s, "Not enough symbols after repeat sequence");
			goto delrep;
		}
		for (s2 = s->prev; s2 != s3; s2 = s2->prev) {
			if (s2->abc_type == ABC_T_NOTE) {
				s2->sflags |= S_BEAM_END;
				break;
			}
		}
		for (j = g->nohdi1; --j >= 0; ) {
			i = n;			/* number of notes/rests */
			if (s->dur != 0)
				i--;
			s2 = s->ts_next;
			while (i > 0) {
				if (s2->staff == staff) {
					s2->extra = NULL;
					unlksym(s2);
					if (s2->voice == voice
					 && s2->dur)
						i--;
				}
				s2 = s2->ts_next;
			}
			to_rest(s);
			s->dur = s->u.note.notes[0].len = dur;
//			s->sflags |= S_REPEAT | S_BEAM_ST;
			s->sflags |= S_REPEAT;
			set_width(s);
			if (s->sflags & S_SEQST)
				s->space = set_space(s);
			s->head = H_SQUARE;
			for (s = s2; s; s = s->ts_next) {
				if (s->staff == staff
				 && s->voice == voice
				 && s->dur)
					break;
			}
		}
		goto delrep;			/* done */
	}

	/* check the measure repeat */
	i = n;				/* number of measures to repeat */
	for (s2 = s->prev->prev ; s2; s2 = s2->prev) {
		if (s2->type == BAR
		 || s2->time == tsfirst->time) {
			if (--i <= 0)
				break;
		}
	}
	if (!s2) {
		error(0, s, "Not enough measures to repeat");
		goto delrep;
	}

	dur = s->time - s2->time;	/* repeat duration */

	if (n == 1)
		i = g->nohdi1;		/* repeat number */
	else
		i = n;			/* check only 2 measures */
	for (s2 = s; s2; s2 = s2->next) {
		if (s2->type == BAR) {
			if (--i <= 0)
				break;
		}
	}
	if (!s2) {
		error(0, s, "Not enough bars after repeat measure");
		goto delrep;
	}

	/* if many 'repeat 2 measures'
	 * insert a new %%repeat after the next bar */
	i = g->nohdi1;		/* repeat number */
	if (n == 2 && i > 1) {
		s2 = s2->next;
		if (!s2) {
			error(0, s, "Not enough bars after repeat measure");
			goto delrep;
		}
		g->nohdi1 = 1;
		s = (struct SYMBOL *) getarena(sizeof *s);
		memcpy(s, g, sizeof *s);
		s->next = s2->extra;
		if (s->next)
			s->next->prev = s;
		s->prev = NULL;
		s2->extra = s;
		s->nohdi1 = --i;
	}

	/* replace */
	dur /= n;
	if (n == 2) {			/* repeat 2 measures (once) */
		s3 = s;
		for (s2 = s->ts_next; ;s2 = s2->ts_next) {
			if (s2->staff != staff)
				continue;
			if (s2->voice == voice
			 && s2->type == BAR)
				break;
			s2->extra = NULL;
			unlksym(s2);
		}
		to_rest(s3);
		s3->dur = s3->u.note.notes[0].len = dur;
		s3->flags = ABC_F_INVIS;
		if (s3->sflags & S_SEQST)
			s3->space = set_space(s3);
		s2->u.bar.len = 2;
		if (s2->sflags & S_SEQST)
			s2->space = set_space(s2);
		s3 = s2->next;
		for (s2 = s3->ts_next; ;s2 = s2->ts_next) {
			if (s2->staff != staff)
				continue;
			if (s2->voice == voice
			 && s2->type == BAR)
				break;
			s2->extra = NULL;
			unlksym(s2);
		}
		to_rest(s3);
		s3->dur = s3->u.note.notes[0].len = dur;
		s3->flags = ABC_F_INVIS;
		set_width(s3);
		if (s3->sflags & S_SEQST)
			s3->space = set_space(s3);
		if (s2->sflags & S_SEQST)
			s2->space = set_space(s2);
		return;
	}

	/* repeat 1 measure */
	s3 = s;
	for (j = g->nohdi1; --j >= 0; ) {
		for (s2 = s3->ts_next; ; s2 = s2->ts_next) {
			if (s2->staff != staff)
				continue;
			if (s2->voice == voice
			 && s2->type == BAR)
				break;
			s2->extra = NULL;
			unlksym(s2);
		}
		to_rest(s3);
		s3->dur = s3->u.note.notes[0].len = dur;
//		s3->sflags |= S_REPEAT | S_BEAM_ST;
		s3->sflags |= S_REPEAT;
		if (s3->sflags & S_SEQST)
			s3->space = set_space(s3);
		if (s2->sflags & S_SEQST)
			s2->space = set_space(s2);
		if (g->nohdi1 == 1) {
			s3->doty = 1;
			break;
		}
		s3->doty = g->nohdi1 - j + 1; /* number to print above the repeat rest */
		s3 = s2->next;
	}
	return;

delrep:					/* remove the %%repeat */
	g->aux = -1;
}

/* add a custos before the symbol of the next line */
static void custos_add(struct SYMBOL *s)
{
	struct VOICE_S *p_voice;
	struct SYMBOL *new_s, *s2;
	int i;

	s2 = s;
	for (;;) {
		if (!s2)
			return;
		if (s2->abc_type == ABC_T_NOTE)
			break;
		s2 = s2->next;
	}

	p_voice = &voice_tb[s->voice];
	p_voice->last_sym = s->prev;
	if (!p_voice->last_sym)
		p_voice->sym = NULL;
	p_voice->time = s->time;
	new_s = sym_add(p_voice, CUSTOS);
	new_s->next = s;
	s->prev = new_s;
	new_s->ts_prev = s->ts_prev;
	new_s->ts_prev->ts_next = new_s;
	new_s->ts_next = s;
	s->ts_prev = new_s;

	new_s->sflags |= S_SEQST;
	new_s->wl = 8;
	new_s->wr = 4;
	new_s->shrink = s->shrink;
	if (new_s->shrink < 8 + 4)
		new_s->shrink = 8 + 4;
	new_s->space = s2->space;

	new_s->nhd = s2->nhd;
	for (i = 0; i <= new_s->nhd; i++) {
//		new_s->u.note.notes[i].pit = s->u.note.notes[i].pit;
		new_s->pits[i] = s2->pits[i];
		new_s->u.note.notes[i].len = CROTCHET;
	}
	new_s->flags = ABC_F_STEMLESS;
}

/* -- define the beginning of a new music line -- */
static struct SYMBOL *set_nl(struct SYMBOL *s)
{
	struct SYMBOL *s2, *extra, *new_sy;
	struct VOICE_S *p_voice;
	int done;

	/* if explicit EOLN, cut on the next symbol */
	if ((s->sflags & S_EOLN) && !cfmt.keywarn && !cfmt.timewarn) {
		s = s->next;
		if (!s)
			return s;

		while (!(s->sflags & S_SEQST))
			s = s->ts_prev;
		goto setnl;
	}

	/* if normal symbol, cut here */
	switch (s->type) {
	case CLEF:
	case BAR:
		break;
	case KEYSIG:
		if (cfmt.keywarn && !s->u.key.empty)
			break;
		goto normal;
	case TIMESIG:
		if (cfmt.timewarn)
			break;
		goto normal;
	case GRACE:			/* don't cut on a grace note */
		s = s->next;
		if (!s)
			return s;
		/* fall thru */
	default:
normal:
		/* cut on the next symbol */
		s = s->next;
		if (!s)
			return s;
		while (!(s->sflags & S_SEQST))
			s = s->ts_prev;
		goto setnl;
	}

	/* go back to handle the staff breaks at end of line */
	for (; s; s = s->ts_prev) {
		if (!(s->sflags & S_SEQST))
			continue;
		switch (s->type) {
		case CLEF:
		case KEYSIG:
		case TIMESIG:
			continue;
		}
		break;
	}
	done = 0;
	new_sy = extra = NULL;
	for ( ; ; s = s->ts_next) {
		if (!s)
			return s;
		if (s->sflags & S_NEW_SY)
			new_sy = s;
		if (!(s->sflags & S_SEQST))
			continue;
		if (done < 0)
			break;
		switch (s->type) {
		case BAR:
			if (done)
				goto cut_here;
			done = 1;
			break;
		case STBRK:
			if (s->doty == 0) {	/* if not forced */
				unlksym(s);	/* remove */
				break;
			}
			done = -1;	/* keep the next symbols on the next line */
			break;
		case TIMESIG:
			if (!cfmt.timewarn)
				goto cut_here;
			break;
		case CLEF:
			if (done)
				goto cut_here;
			break;
		case KEYSIG:
			if (!cfmt.keywarn || s->u.key.empty)
				goto cut_here;
			break;
		default:
			if (!done || (s->prev && s->prev->type == GRACE))
				break;
			goto cut_here;
		}
		if (s->extra) {
			if (!extra)
				extra = s;
			else
				error(0, s, "abcm2ps problem: "
					"Extra symbol may be misplaced");
		}
	}
cut_here:
	if (extra			/* extra symbol(s) to be moved */
	 && extra != s) {
		s2 = extra->extra;
		while (s2->next)
			s2 = s2->next;
		s2->next = s->extra;
		s->extra = extra->extra;
		extra->extra = NULL;
	}
	if (new_sy && s != new_sy) {
		new_sy->sflags &= ~S_NEW_SY;
		s->sflags |= S_NEW_SY;
	}
setnl:
	if (cfmt.custos && !first_voice->next) {
		custos_add(s);
	} else {
		s2 = s->ts_prev;
		switch (s2->type) {
		case BAR:
		case FMTCHG:
		case CLEF:
		case KEYSIG:
		case TIMESIG:
			break;
		default:			/* add an extra symbol at eol */
			p_voice = &voice_tb[s2->voice];
			p_voice->last_sym = s2;
			p_voice->time = s->time;
			s2 = s2->next;
			extra = sym_add(p_voice, FMTCHG);
			extra->next = s2;
			if (s2)
				s2->prev = extra;
			extra->ts_prev = extra->prev;
			extra->ts_prev->ts_next = extra;
			extra->ts_next = s;
			s->ts_prev = extra;
			extra->aux = -1;
			extra->sflags |= S_SEQST;
			extra->wl = 6;
//fixme: wr/shrink/space are not needed
			extra->wr = 6;
//			extra->shrink = extra->prev->wr + 6;
//			extra->space = extra->prev->space;
			extra->shrink = s->shrink;
			extra->space = s->space;
			if (s->x != 0)
				extra->x = s->x - 1;
// {	/* auto break */
//				for (s2 = s->ts_next; ; s2 = s2->ts_next) {
//					if (s2->x != 0) {
//						extra->x = s2->x - 1;
//						break;
//					}
//				}
//			}
			break;
		}
	}
	s->sflags |= S_NL;
	return s;
}

/* -- search where to cut the lines according to the staff width -- */
static struct SYMBOL *set_lines(struct SYMBOL *first,	/* first symbol */
				struct SYMBOL *last,	/* last symbol / 0 */
				float lwidth,		/* w - (clef & key sig) */
				float indent)		/* for start of tune */
{
	struct SYMBOL *s, *s2, *s3;
	float x, xmin, xmax, wwidth, shrink, space;
	int nlines, beam, bar_time;

	/* calculate the whole size of the piece of tune */
	wwidth = indent;
	for (s = first; s != last; s = s->ts_next) {
		if (!(s->sflags & S_SEQST))
			continue;
		s->x = wwidth;
		shrink = s->shrink;
		if ((space = s->space) < shrink)
			wwidth += shrink;
		else
			wwidth += shrink * cfmt.maxshrink
				+ space * (1 - cfmt.maxshrink);
	}

	/* loop on cutting the tune into music lines */
	s = first;
	for (;;) {
		nlines = wwidth / lwidth + 0.999;
		if (nlines <= 1) {
			if (last)
				last = set_nl(last);
			return last;
		}

		/* try to cut on a measure bar */
		s2 = first = s;
		xmin = s->x + wwidth / nlines * cfmt.breaklimit;
		xmax = s->x + lwidth;
		for ( ; s != last; s = s->ts_next) {
			x = s->x;
			if (x == 0)
				continue;
			if (x > xmax)
				break;
			if (s->type != BAR)
				continue;
			if (x > xmin)
				goto cut_here;
			s2 = s;			// keep the last bar
		}

		/* if a bar, cut here */
		if (s == last)
			return last;
		if (s->type == BAR)
			goto cut_here;

		bar_time = s2->time;

		/* try to avoid to cut a beam */
		beam = s2->type == NOTEREST &&
				(s2->sflags & (S_BEAM_ST | S_BEAM_END)) == 0 ? 1 : 0;
		s = s2;				/* restart from start or last bar */
		s2 = s3 = NULL;
		xmax -= 6;			// a FORMAT will be added
		for ( ; s != last; s = s->ts_next) {
			if ((s->sflags & (S_BEAM_ST | S_BEAM_END))
						== S_BEAM_ST) {
				beam++;
				continue;
			}
			if ((s->sflags & (S_BEAM_ST | S_BEAM_END))
						== S_BEAM_END)
				beam--;
			x = s->x;
			if (x < xmin)
				continue;
//--fixme
//			if (x + 2 * s->shrink >= xmax)
			if (x + s->shrink >= xmax)
				break;
			if (beam != 0)
				continue;
			s2 = s;
			if ((s->time - bar_time) % (CROTCHET / 2) == 0)
				s3 = s;
		}
		if (s3)
			s2 = s3;
		if (s2)
			s = s2;
		while (s->x == 0 || s->x + s->shrink * 2 >= xmax)
			s = s->ts_prev;
cut_here:
		if (s->sflags & S_NL) {		/* already set here - advance */
			error(0, s, "Line split problem - "
					"adjust maxshrink and/or breaklimit");
			nlines = 2;
			for (s = s->ts_next; s != last; s = s->ts_next) {
				if (s->x == 0)
					continue;
				if (--nlines <= 0)
					break;
			}
		}
		s = set_nl(s);
		if (!s
		 || (last && s->time >= last->time))
			break;
		wwidth -= s->x - first->x;
	}
	return s;
}

/* -- cut the tune into music lines -- */
static void cut_tune(float lwidth, float indent)
{
	struct VOICE_S *p_voice;
	struct SYMBOL *s, *s2;
	int i;
	float xmin;

	/* adjust the line width according to the starting clef
	 * and key signature */
/*fixme: may change in the tune*/
#if 1
	s = tsfirst;
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		i = p_voice - voice_tb;
		if (cursys->voice[i].range >= 0)
			break;
	}
	lwidth -= 12 + 10			// clef.wl
		+ 12 - 10			// clef.wr
		+ 3				// key.wl
		+ 3 + p_voice->key.sf * 5.5;	// key.wr
#else
	for (s = tsfirst; s; s = s->ts_next) {
		if (s->shrink == 0)
			continue;
		if (s->type != CLEF && s->type != KEYSIG)
			break;
		lwidth -= s->shrink;
	}
#endif
	if (cfmt.custos && !first_voice->next)
		lwidth -= 12;
	if (cfmt.continueall) {
		set_lines(s, NULL, lwidth, indent);
		return;
	}

	/* if asked, count the measures and set the EOLNs */
	if ((i = cfmt.barsperstaff) != 0) {
		s2 = s;
		for ( ; s; s = s->ts_next) {
			if (s->type != BAR
			 || s->aux == 0)
				continue;
			if (--i > 0)
				continue;
			s->sflags |= S_EOLN;
			i = cfmt.barsperstaff;
		}
		s = s2;
	}

	/* cut at explicit end of line, checking the line width */
	xmin = indent;
	s2 = s;
	for ( ; s; s = s->ts_next) {
		if (!(s->sflags & (S_SEQST | S_EOLN)))
			continue;
		xmin += s->shrink;
		if (xmin > lwidth) {
			if (cfmt.linewarn)
				error(0, s, "Line overfull (%.0fpt of %.0fpt)",
					xmin, lwidth);
//			for (s = s->ts_next; s; s = s->ts_next) {
			for ( ; s; s = s->ts_next) {
				if (s->sflags & S_EOLN)
					break;
			}
			s = s2 = set_lines(s2, s, lwidth, indent);
			if (!s)
				break;
			xmin = s->shrink;
			indent = 0;
			continue;
		}
		if (!(s->sflags & S_EOLN))
			continue;
		s2 = set_nl(s);
		s->sflags &= ~S_EOLN;
		s = s2;
		if (!s)
			break;
		xmin = s->shrink;
		indent = 0;
	}
}

/* -- set the y values of some symbols -- */
static void set_yval(struct SYMBOL *s)
{
//fixme: staff_tb is not yet loaded
//	int top, bot;
//	top = staff_tb[s->staff].topbar;
//	bot = staff_tb[s->staff].botbar;
	switch (s->type) {
	case CLEF:
		if ((s->sflags & S_SECOND)
		 || (s->flags & ABC_F_INVIS)) {
//			s->ymx = s->ymn = (top + bot) / 2;
			s->ymx = s->ymn = 12;
			break;
		}
		s->y = (s->u.clef.line - 1) * 6;
		switch (s->u.clef.type) {
		default:			/* treble / perc */
//			s->y = -2 * 6;
//			s->ymx = 24 + 12;
//			s->ymn = -9;
			s->ymx = s->y + 28;
			s->ymn = s->y - 14;
			break;
		case ALTO:
//			s->y = -3 * 6;
//			s->ymx = 24 + 2;
//			s->ymn = -1;
			s->ymx = s->y + 13;
			s->ymn = s->y - 11;
			break;
		case BASS:
//			s->y = -4 * 6;
//			s->ymx = 24 + 2;
//			s->ymn = 2;
			s->ymx = s->y + 7;
			s->ymn = s->y - 12;
			break;
		}
		if (s->aux) {			// small clef
			s->ymx -= 2;
			s->ymn += 2;
		}
		if (s->ymx < 26)
			s->ymx = 26;
		if (s->ymn > -1)
			s->ymn = -1;
//		s->y += s->u.clef.line * 6;
//		if (s->y > 0)
//			s->ymx += s->y;
//		else if (s->y < 0)
//			s->ymn += s->y;
		if (s->u.clef.octave > 0)
			s->ymx += 9;
		else if (s->u.clef.octave < 0)
			s->ymn -= 9;
		break;
	case KEYSIG:
		if (s->u.key.sf > 2)
			s->ymx = 24 + 10;
		else if (s->u.key.sf > 0)
			s->ymx = 24 + 6;
		else
			s->ymx = 24 + 2;
		s->ymn = -2;
		break;
	default:
//		s->ymx = top + 2;
		s->ymx = 24 + 2;
		s->ymn = -2;
		break;
	}
}

// set the clefs (treble or bass) in a 'auto clef' sequence
// return the starting clef type
static int set_auto_clef(int staff,
			struct SYMBOL *s_start,
			int clef_type_start)
{
	struct SYMBOL *s;
	struct SYMBOL *s_last, *s_last_chg;
	int clef_type, min, max, time;

	/* get the max and min pitches in the sequence */
	max = 12;				/* "F," */
	min = 20;				/* "G" */
	for (s = s_start; s; s = s->ts_next) {
		if ((s->sflags & S_NEW_SY) && s != s_start)
			break;
		if (s->staff != staff)
			continue;
		if (s->abc_type != ABC_T_NOTE) {
			if (s->type == CLEF) {
				if (s->u.clef.type != AUTOCLEF)
					break;
				unlksym(s);
			}
			continue;
		}
		if (s->pits[0] < min)
			min = s->pits[0];
		else if (s->pits[s->nhd] > max)
			max = s->pits[s->nhd];
	}

	if (min >= 19					/* upper than 'F' */
	 || (min >= 13 && clef_type_start != BASS))	/* or 'G,' */
		return TREBLE;
	if (max <= 13					/* lower than 'G,' */
	 || (max <= 19 && clef_type_start != TREBLE))	/* or 'F' */
		return BASS;

	/* set clef changes */
	if (clef_type_start == AUTOCLEF) {
		if ((max + min) / 2 >= 16)
			clef_type_start = TREBLE;
		else
			clef_type_start = BASS;
	}
	clef_type = clef_type_start;
	s_last = s;
	s_last_chg = NULL;
	for (s = s_start; s != s_last; s = s->ts_next) {
		struct SYMBOL *s2, *s3;	//, *s4;

		if ((s->sflags & S_NEW_SY) && s != s_start)
			break;
		if (s->staff != staff || s->abc_type != ABC_T_NOTE)
			continue;

		/* check if a clef change may occur */
		time = s->time;
		if (clef_type == TREBLE) {
			if (s->pits[0] > 12		/* F, */
			 || s->pits[s->nhd] > 20) {	/* G */
				if (s->pits[0] > 20)
					s_last_chg = s;
				continue;
			}
			s2 = s->ts_prev;
			if (s2
			 && s2->time == time
			 && s2->staff == staff
			 && s2->abc_type == ABC_T_NOTE
			 && s2->pits[0] >= 19)	/* F */
				continue;
			s2 = s->ts_next;
			if (s2
			 && s2->staff == staff
			 && s2->time == time
			 && s2->abc_type == ABC_T_NOTE
			 && s2->pits[0] >= 19)	/* F */
				continue;
		} else {
			if (s->pits[0] < 12		/* F, */
			 || s->pits[s->nhd] < 20) {	/* G */
				if (s->pits[s->nhd] < 12)
					s_last_chg = s;
				continue;
			}
			s2 = s->ts_prev;
			if (s2
			 && s2->time == time
			 && s2->staff == staff
			 && s2->abc_type == ABC_T_NOTE
			 && s2->pits[0] <= 13)	/* G, */
				continue;
			s2 = s->ts_next;
			if (s2
			 && s2->staff == staff
			 && s2->time == time
			 && s2->abc_type == ABC_T_NOTE
			 && s2->pits[0] <= 13)	/* G, */
				continue;
		}

		/* if first change, change the starting clef */
		if (!s_last_chg) {
			clef_type = clef_type_start =
					clef_type == TREBLE ? BASS : TREBLE;
			s_last_chg = s;
			continue;
		}

		/* go backwards and search where to insert a clef change */
		s3 = s;
		for (s2 = s->ts_prev; s2 != s_last_chg; s2 = s2->ts_prev) {
			if (s2->staff != staff)
				continue;
			if (s2->type == BAR
			 && s2->voice == s->voice) {
				s3 = s2;
				break;
			}
			if (s2->abc_type != ABC_T_NOTE)
				continue;

#if 0
			/* exit loop if a clef change cannot occur */
			if (clef_type == TREBLE) {
				if (s2->pits[0] >= 19)		/* F */
					break;
			} else {
				if (s2->pits[s2->nhd] <= 13)	/* G, */
					break;
			}
#endif

			/* have a 2nd choice on beam start */
			if ((s2->sflags & S_BEAM_ST)
			 && !voice_tb[s2->voice].second)
				s3 = s2;
		}

		/* no change possible if no insert point */
		if (s3->time == s_last_chg->time) {
			s_last_chg = s;
			continue;
		}
		s_last_chg = s;

		/* insert a clef change */
		clef_type = clef_type == TREBLE ? BASS : TREBLE;
		s2 = insert_clef(s3, clef_type, clef_type == TREBLE ? 2 : 4);
		s2->sflags |= S_CLEF_AUTO;
//		s3->prev->staff = staff;
	}
	return clef_type_start;
}

/* set the auto clefs */
/* this function is called once at start of tune generation */
/*
 * global variables:
 *	- staff_tb[staff].clef = clefs at start of line (here, start of tune)
 *				(created here, updated on clef draw)
 *	- voice_tb[voice].clef = clefs at end of generation
 *				(created on voice creation, updated here)
 */
static void set_clefs(void)
{
	struct SYSTEM *sy;
	struct VOICE_S *p_voice;
	struct SYMBOL *s, *s2, *g;
	int staff, voice, pitch, new_type, new_line, old_lvl;
	struct {
		struct SYMBOL *clef;
		short autoclef;
		short mid;
	} staff_clef[MAXSTAFF];

	old_lvl = lvlarena(1);			// keep the staff clefs

	// create the staff table
	memset(staff_tb, 0, sizeof staff_tb);
	for (staff = 0; staff <= nstaff; staff++) {
		staff_clef[staff].clef = NULL;
		staff_clef[staff].autoclef = 1;
	}

	// set the starting clefs of the staves
	sy = cursys;
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		voice = p_voice - voice_tb;
		if (sy->voice[voice].range < 0)
			continue;
		staff = sy->voice[voice].staff;
		if (!sy->voice[voice].second) {		// main voices
			if (p_voice->stafflines)
				sy->staff[staff].stafflines = p_voice->stafflines;
			if (p_voice->staffscale != 0)
				sy->staff[staff].staffscale = p_voice->staffscale;
			if (sy->voice[voice].sep)
				sy->staff[staff].sep = sy->voice[voice].sep;
			if (sy->voice[voice].maxsep)
				sy->staff[staff].maxsep = sy->voice[voice].maxsep;
		}
		s = p_voice->s_clef;
		if (!sy->voice[voice].second
		 && !(s->sflags & S_CLEF_AUTO))
			staff_clef[staff].autoclef = 0;
	}
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		voice = p_voice - voice_tb;
		if (sy->voice[voice].range < 0
		 || sy->voice[voice].second)		// main voices
			continue;
		staff = sy->voice[voice].staff;
		s = p_voice->s_clef;
		if (staff_clef[staff].autoclef) {
			s->u.clef.type = set_auto_clef(staff,
							tsfirst,
							s->u.clef.type);
			s->u.clef.line =
				s->u.clef.type == TREBLE ? 2 : 4;
		}
		staff_clef[staff].clef = staff_tb[staff].s_clef = s;
	}
	for (staff = 0; staff <= sy->nstaff; staff++)
		staff_clef[staff].mid = (strlen(sy->staff[staff].stafflines) - 1) * 3;

	for (s = tsfirst; s; s = s->ts_next) {
		for (g = s->extra ; g; g = g->next) {
			if (g->type == FMTCHG && g->aux == REPEAT) {
				set_repeat(g, s);
				break;
			}
		}

		// handle %%staves
		if (s->sflags & S_NEW_SY) {
			sy = sy->next;
			for (staff = 0; staff <= nstaff; staff++)
				staff_clef[staff].autoclef = 1;
			for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
				voice = p_voice - voice_tb;
				if (sy->voice[voice].range < 0)
					continue;
				staff = sy->voice[voice].staff;
				if (!sy->voice[voice].second) {
					if (p_voice->stafflines)
						sy->staff[staff].stafflines =
								p_voice->stafflines;
					if (p_voice->staffscale != 0)
						sy->staff[staff].staffscale =
								p_voice->staffscale;
					if (sy->voice[voice].sep)
						sy->staff[staff].sep =
								sy->voice[voice].sep;
					if (sy->voice[voice].maxsep)
						sy->staff[staff].maxsep =
								sy->voice[voice].maxsep;
				}
				s2 = p_voice->s_clef;
				if (!(s2->sflags & S_CLEF_AUTO))
					staff_clef[staff].autoclef = 0;
			}
			for (staff = 0; staff <= sy->nstaff; staff++)
				staff_clef[staff].mid =
					(strlen(sy->staff[staff].stafflines) - 1) * 3;
			for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
				voice = p_voice - voice_tb;
				if (sy->voice[voice].range < 0
				 || sy->voice[voice].second)
					continue;
				staff = sy->voice[voice].staff;
				s2 = p_voice->s_clef;
				if (s2->sflags & S_CLEF_AUTO) {
//fixme: the staff may have other voices with explicit clefs...
//					if (!staff_clef[staff].autoclef)
//						???
					new_type = set_auto_clef(staff, s,
						staff_clef[staff].clef ?
							staff_clef[staff].clef->u.clef.type :
							AUTOCLEF);
					new_line = new_type == TREBLE ? 2 : 4;
				} else {
					new_type = s2->u.clef.type;
					new_line = s2->u.clef.line;
				}
				if (!staff_clef[staff].clef) {	// new staff
					if (s2->sflags & S_CLEF_AUTO) {
						if (s2->u.clef.type != AUTOCLEF) {
							p_voice->s_clef =
								(struct SYMBOL *) getarena(sizeof *s);
							memcpy(p_voice->s_clef,
								s2,
								sizeof *p_voice->s_clef);
						}
						p_voice->s_clef->u.clef.type = new_type;
						p_voice->s_clef->u.clef.line = new_line;
					}
					staff_tb[staff].s_clef =
						staff_clef[staff].clef = p_voice->s_clef;
					continue;
				}
								// old staff
				if (new_type == staff_clef[staff].clef->u.clef.type
				 && new_line == staff_clef[staff].clef->u.clef.line)
					continue;
				g = s;
				while (g->voice != voice)
					g = g->ts_next;
				if (g->type != CLEF) {
					g = insert_clef(g, new_type, new_line);
					if (s2->sflags & S_CLEF_AUTO)
						g->sflags |= S_CLEF_AUTO;
				}
				staff_clef[staff].clef = p_voice->s_clef = g;
			}
		}
		if (s->type != CLEF) {
			s->mid = staff_clef[s->staff].mid;
			continue;
		}

		if (s->u.clef.type == AUTOCLEF) {
			s->u.clef.type = set_auto_clef(s->staff,
						s->ts_next,
						staff_clef[s->staff].clef->u.clef.type);
			s->u.clef.line = s->u.clef.type == TREBLE ? 2 : 4;
		}

		p_voice = &voice_tb[s->voice];
		p_voice->s_clef = s;
		if (s->sflags & S_SECOND) {
/*fixme:%%staves:can this happen?*/
//			if (!s->prev)
//				break;
			unlksym(s);
			continue;
		}
		staff = s->staff;
// may have been inserted on %%staves
//		if (s->sflags & S_CLEF_AUTO) {
//			unlksym(s);
//			continue;
//		}

		if (staff_tb[staff].s_clef) {
			if (s->u.clef.type == staff_clef[staff].clef->u.clef.type
			 && s->u.clef.line == staff_clef[staff].clef->u.clef.line
			 && !(s->sflags & S_NEW_SY)) {
//				unlksym(s);
				continue;
			}
		} else {

			// the voice moved to a new staff with a forced clef
			 staff_tb[staff].s_clef = s;
		}
		staff_clef[staff].clef = s;
	}

	/* set a pitch to the symbols of voices with no note */
	sy = cursys;
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		voice = p_voice - voice_tb;
		if (sy->voice[voice].range < 0)
			continue;
		s2 = p_voice->sym;
		if (!s2 || s2->pits[0] != 127)
			continue;
		staff = sy->voice[voice].staff;
		switch (staff_tb[staff].s_clef->u.clef.type) {
		default:
			pitch = 22;		/* 'B' */
			break;
		case ALTO:
			pitch = 16;		/* 'C' */
			break;
		case BASS:
			pitch = 10;		/* 'D,' */
			break;
		}
		for (s = s2; s; s = s->next)
			s->pits[0] = pitch;
	}

	lvlarena(old_lvl);
}

/* -- set the pitch of the notes according to the clefs -- */
/* also treat the auto clefs and set the vertical offset of the symbols */
/* this function is called only once per tune
 * then, once per music line up to the old sequence */
static void set_pitch(struct SYMBOL *last_s)
{
	struct SYMBOL *s, *g;
	int staff, delta, dur;
	signed char staff_delta[MAXSTAFF];
	static const signed char delta_tb[4] = {
		0 - 2 * 2,
		6 - 3 * 2,
		12 - 4 * 2,
		0 - 3 * 2
	};

	for (staff = 0; staff <= nstaff; staff++) {
		s = staff_tb[staff].s_clef;
		staff_delta[staff] = delta_tb[s->u.clef.type] +
				s->u.clef.line * 2 +
				s->u.clef.transpose;
	}

	dur = BASE_LEN;
	for (s = tsfirst; s != last_s; s = s->ts_next) {
		int np, m;

		staff = s->staff;
		switch (s->type) {
		case CLEF:
			staff_delta[staff] = delta_tb[s->u.clef.type] +
						s->u.clef.line * 2 +
						s->u.clef.transpose;
			set_yval(s);
			break;
		case GRACE:
			for (g = s->extra; g; g = g->next) {
				if (g->type != NOTEREST)
					continue;
				delta = staff_delta[g->staff];
				if (delta != 0
				 && voice_tb[s->voice].key.instr != K_DRUM) {
					for (m = g->nhd; m >= 0; m--)
						g->pits[m] += delta;
				}
				g->ymn = 3 * (g->pits[0] - 18) - 2;
				g->ymx = 3 * (g->pits[g->nhd] - 18) + 2;
			}
			set_yval(s);
			break;
		case KEYSIG:
			s->u.key.clef_delta =
				staff_delta[staff]; /* keep the current clef */
//			s->ymx = 24 + 10;
//			s->ymn = -2;
//			break;
			/* fall thru */
		default:
			set_yval(s);
			break;
		case MREST:
			if (s->flags & ABC_F_INVIS)
				break;
			s->ymx = 24 + 15;
			s->ymn = -2;
			break;
		case NOTEREST:
			if (s->abc_type != ABC_T_NOTE
			 && !first_voice->next) {
				s->y = 12;		/* rest single voice */
				s->ymx = 24;
				s->ymn = 0;
				break;
			}
			np = s->nhd;
			delta = staff_delta[staff];
			if (delta != 0
			 && voice_tb[s->voice].key.instr != K_DRUM) {
				for (m = np; m >= 0; m--)
					s->pits[m] += delta;
			}
			if (s->abc_type == ABC_T_NOTE) {
				s->ymx = 3 * (s->pits[np] - 18) + 4;
				s->ymn = 3 * (s->pits[0] - 18) - 4;
			} else {
				s->y = (s->pits[0] - 18) / 2 * 6;
				s->ymx = s->y + rest_sp[5 - s->nflags][0];
				s->ymn = s->y - rest_sp[5 - s->nflags][1];
			}
			if (s->dur < dur)
				dur = s->dur;
			break;
		}
	}
	smallest_duration = dur;
}

/* -- set the stem direction when multi-voices -- */
/* this function is called only once per tune */
static void set_stem_dir(void)
{
	struct SYSTEM *sy;
	struct SYMBOL *s, *t, *u;
	int i, staff, nst, rvoice, voice;
	struct {
		int nvoice;
		struct {
			int voice;
			short ymn;
			short ymx;
		} st[4];		/* (no more than 4 voices per staff) */
	} stb[MAXSTAFF];
	struct {
		signed char st1, st2;	/* (a voice cannot be on more than 2 staves) */
	} vtb[MAXVOICE];

	s = tsfirst;
	sy = cursys;
	nst = sy->nstaff;
	while (s) {
		for (staff = nst; staff >= 0; staff--) {
			stb[staff].nvoice = -1;
			for (i = 4; --i >= 0; ) {
				stb[staff].st[i].voice = -1;
				stb[staff].st[i].ymx = 0;
				stb[staff].st[i].ymn = 24;
			}
		}
		for (i = 0; i < MAXVOICE; i++)
			vtb[i].st1 = vtb[i].st2 = -1;

		/* get the max/min offsets in the delta time */
/*fixme: the stem height is not calculated yet*/
		for (u = s; u; u = u->ts_next) {
			if (u->type == BAR)
				break;
			if (u->sflags & S_NEW_SY) {
				if (u != s)
					break;
				sy = sy->next;
				for (staff = nst; staff <= sy->nstaff; staff++) {
					stb[staff].nvoice = -1;
					for (i = 4; --i >= 0; ) {
						stb[staff].st[i].voice = -1;
						stb[staff].st[i].ymx = 0;
						stb[staff].st[i].ymn = 24;
					}
				}
				nst = sy->nstaff;
			}
			if (u->type != NOTEREST
			 || (u->flags & ABC_F_INVIS))
				continue;
			staff = u->staff;
#if 1
/*fixme:test*/
if (staff > nst) {
	bug("set_stem_dir(): bad staff number\n", 1);
}
#endif
			voice = u->voice;
			if (vtb[voice].st1 < 0) {
				vtb[voice].st1 = staff;
			} else if (vtb[voice].st1 != staff) {
				if (staff > vtb[voice].st1) {
					if (staff > vtb[voice].st2)
						vtb[voice].st2 = staff;
				} else {
					if (vtb[voice].st1 > vtb[voice].st2)
						vtb[voice].st2 = vtb[voice].st1;
					vtb[voice].st1 = staff;
				}
			}
			rvoice = sy->voice[voice].range;
			for (i = stb[staff].nvoice; i >= 0; i--) {
				if (stb[staff].st[i].voice == rvoice)
					break;
			}
			if (i < 0) {
				if (++stb[staff].nvoice >= 4)
					bug("Too many voices per staff", 1);
				for (i = 0; i < stb[staff].nvoice; i++) {
					if (rvoice < stb[staff].st[i].voice) {
						memmove(&stb[staff].st[i + 1],
							&stb[staff].st[i],
							sizeof stb[staff].st[i]
								* (stb[staff].nvoice - i));
						stb[staff].st[i].ymx = 0;
						stb[staff].st[i].ymn = 24;
						break;
					}
				}
				stb[staff].st[i].voice = rvoice;
			}

			if (u->abc_type != ABC_T_NOTE)
				continue;
			if (u->ymx > stb[staff].st[i].ymx)
				stb[staff].st[i].ymx = u->ymx;
			if (u->ymn < stb[staff].st[i].ymn)
				stb[staff].st[i].ymn = u->ymn;
			if (u->sflags & S_XSTEM) {
				if (u->ts_prev->staff != staff - 1
				 || u->ts_prev->abc_type != ABC_T_NOTE) {
					error(1, s, "Bad !xstem!");
					u->sflags &= ~S_XSTEM;
/*fixme:nflags KO*/
				} else {
					u->ts_prev->multi = 1;
					u->multi = 1;
					u->flags |= ABC_F_STEMLESS;
				}
			}
		}

		for ( ; s != u; s = s->ts_next) {
			if (s->multi)
				continue;
			if (s->type != NOTEREST		/* if not note nor rest */
			 && s->type != GRACE)
				continue;
			staff = s->staff;
			voice = s->voice;
//			if (!s->multi && vtb[voice].st2 >= 0) {
			if (vtb[voice].st2 >= 0) {
				if (staff == vtb[voice].st1)
					s->multi = -1;
				else if (staff == vtb[voice].st2)
					s->multi = 1;
				continue;
			}
			if (stb[staff].nvoice <= 0) { /* voice alone on the staff */
//				if (s->multi)
//					continue;
/*fixme:could be done in set_float()*/
				if (s->sflags & S_FLOATING) {
					if (staff == voice_tb[voice].staff)
						s->multi = -1;
					else
						s->multi = 1;
				}
				continue;
			}
			rvoice = sy->voice[voice].range;
			for (i = stb[staff].nvoice; i >= 0; i--) {
				if (stb[staff].st[i].voice == rvoice)
					break;
			}
			if (i < 0)
				continue;		/* voice ignored */
			if (i == stb[staff].nvoice) {
				s->multi = -1;	/* last voice */
			} else {
				s->multi = 1;	/* first voice(s) */

				/* if 3 voices, and vertical space enough,
				 * have stems down for the middle voice */
				if (i != 0
				 && i + 1 == stb[staff].nvoice) {
					if (stb[staff].st[i].ymn - cfmt.stemheight
					    > stb[staff].st[i + 1].ymx)
						s->multi = -1;

					/* special case for unison */
					if (s->ts_prev
					 && s->ts_prev->time == s->time
					 && s->ts_prev->staff == s->staff
					 && s->pits[s->nhd] == s->ts_prev->pits[0]
					 && (s->sflags & (S_BEAM_ST | S_BEAM_END))
							== (S_BEAM_ST | S_BEAM_END)
					 && ((t = s->ts_next) == NULL
					  || t->staff != s->staff
					  || t->time != s->time))
						s->multi = -1;
				}
			}
		}

		while (s && s->type == BAR) {
			if (s->sflags & S_NEW_SY) {
				sy = sy->next;
				nst = sy->nstaff;
			}
			s = s->ts_next;
		}
	}
}

/* -- adjust the offset of the rests when many voices -- */
/* this function is called only once per tune */
static void set_rest_offset(void)
{
	struct SYSTEM *sy;
	struct SYMBOL *s, *s2;
	int nvoice, voice, end_time, not_alone, ymax, ymin,
		shift, dots;
	float dx;
	struct {
		struct SYMBOL *s;
		int staff;
		int end_time;
	} vtb[MAXVOICE], *v;

	memset(vtb, 0, sizeof vtb);
	
	sy = cursys;
	nvoice = 0;
	for (s = tsfirst; s; s = s->ts_next) {
		if (s->flags & ABC_F_INVIS)
			continue;
		if (s->sflags & S_NEW_SY)
			sy = sy->next;
		if (s->type != NOTEREST)
			continue;
		if (s->voice > nvoice)
			nvoice = s->voice;
		v = &vtb[s->voice];
		v->s = s;
		v->staff = s->staff;
		v->end_time = s->time + s->dur;
		if (s->abc_type != ABC_T_REST)
			continue;

		/* check if clash with previous symbols */
		ymin = -127;
		ymax = 127;
		not_alone = dots = 0;
		for (voice = 0, v = vtb; voice <= nvoice; voice++, v++) {
			s2 = v->s;
			if (!s2
			 || v->staff != s->staff
			 || voice == s->voice)
				continue;
			if (v->end_time <= s->time)
				continue;
			not_alone++;
			if (sy->voice[voice].range < sy->voice[s->voice].range) {
				if (s2->time == s->time) {
					if (s2->ymn < ymax) {
						ymax = s2->ymn;
						if (s2->dots)
							dots = 1;
					}
				} else {
					if (s2->y < ymax)
						ymax = s2->y;
				}
			} else {
				if (s2->time == s->time) {
					if (s2->ymx > ymin) {
						ymin = s2->ymx;
						if (s2->dots)
							dots = 1;
					}
				} else {
					if (s2->y > ymin)
						ymin = s2->y;
				}
			}
		}

		/* check if clash with next symbols */
		end_time = s->time + s->dur;
		for (s2 = s->ts_next; s2; s2 = s2->ts_next) {
			if (s2->time >= end_time)
				break;
			if (s2->staff != s->staff
			 || s2->type != NOTEREST
			 || (s2->flags & ABC_F_INVIS))
				continue;
			not_alone++;
			if (sy->voice[s2->voice].range < sy->voice[s->voice].range) {
				if (s2->time == s->time) {
					if (s2->ymn < ymax) {
						ymax = s2->ymn;
						if (s2->dots)
							dots = 1;
					}
				} else {
					if (s2->y < ymax)
						ymax = s2->y;
				}
			} else {
				if (s2->time == s->time) {
					if (s2->ymx > ymin) {
						ymin = s2->ymx;
						if (s2->dots)
							dots = 1;
					}
				} else {
					if (s2->y > ymin)
						ymin = s2->y;
				}
			}
		}
		shift = ymax - s->ymx;
		if (shift < 0) {
			shift = (-shift + 5) / 6 * 6;
			if (s->ymn - shift >= ymin) {
				s->y -= shift;
				s->ymx -= shift;
				s->ymn -= shift;
				continue;
			}
			dx = dots ? 15 : 10;
			s->u.note.notes[0].shhd = dx;
			s->xmx = dx;
			continue;
		}
		shift = ymin - s->ymn;
		if (shift > 0) {
			shift = (shift + 5) / 6 * 6;
			if (s->ymx + shift <= ymax) {
				s->y += shift;
				s->ymx += shift;
				s->ymn += shift;
				continue;
			}
			dx = dots ? 15 : 10;
			s->u.note.notes[0].shhd = dx;
			s->xmx = dx;
			continue;
		}
		if (!not_alone) {
			s->y = 12;
			s->ymx = 24;
			s->ymn = 0;
		}
	}
}

/* -- create a starting symbol -- */
static struct SYMBOL *sym_new(int type,
				struct VOICE_S *p_voice,
				struct SYMBOL *last_s)	/* same time */
{
	struct SYMBOL *s;

	s = (struct SYMBOL *) getarena(sizeof *s);
	memset(s, 0, sizeof *s);
	s->type = type;
	s->voice = p_voice - voice_tb;
	s->staff = p_voice->staff;
	s->time = last_s->time;

	s->next = p_voice->last_sym->next;
	if (s->next)
		s->next->prev = s;
	p_voice->last_sym->next = s;
	s->prev = p_voice->last_sym;
	p_voice->last_sym = s;

	s->ts_next = last_s;
	s->ts_prev = last_s->ts_prev;
	s->ts_prev->ts_next = s;
	if (!s->ts_prev || s->ts_prev->type != type)
		s->sflags |= S_SEQST;
	last_s->ts_prev = s;
	if (last_s->type == type && s->voice != last_s->voice) {
		last_s->sflags &= ~S_SEQST;
		last_s->shrink = 0;
	}
	s->fn = last_s->fn;
	s->linenum = last_s->linenum;
	s->colnum = last_s->colnum;
	return s;
}

/* -- init the symbols at start of a music line -- */
static void init_music_line(void)
{
	struct VOICE_S *p_voice;
	struct SYMBOL *s, *last_s;
	int voice, staff;

	/* initialize the voices */
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		voice = p_voice - voice_tb;
		p_voice->last_sym = p_voice->sym;
		if (cursys->voice[voice].range < 0)
			continue;
		p_voice->second = cursys->voice[voice].second;

		/* move the voice to a non empty staff */
		staff = cursys->voice[voice].staff;
		while (staff < nstaff && cursys->staff[staff].empty)
			staff++;
		p_voice->staff = staff;
	}

	/* add a clef at start of the main voices */
	last_s = tsfirst;
	while (last_s->type == CLEF) {		/* move the starting clefs */
		voice = last_s->voice;
		p_voice = &voice_tb[voice];
		if (cursys->voice[voice].range >= 0
		 && !cursys->voice[voice].second) {
			last_s->aux = 0;			/* normal clef */
			p_voice->last_sym = p_voice->sym = last_s;
		}
		last_s = last_s->ts_next;
	}
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		if (p_voice->sym && p_voice->sym->type == CLEF)
			continue;
		voice = p_voice - voice_tb;
		if (cursys->voice[voice].range < 0
		 || cursys->voice[voice].second)
			continue;
		staff = cursys->voice[voice].staff;
#if 0
		if (last_s->voice == voice && last_s->type == CLEF) {
			last_s->aux = 0;			/* normal clef */
#if 0
			if (cursys->staff[staff].clef.invis)
				s->flags |= ABC_F_INVIS;
#endif
			p_voice->last_sym = p_voice->sym = last_s;
			last_s = last_s->ts_next;
			continue;
		}
#endif
		if (!staff_tb[staff].s_clef)
			continue;			// no clef

		s = (struct SYMBOL *) getarena(sizeof *s);
		memset(s, 0, sizeof *s);
		memcpy(&s->u.clef, &staff_tb[staff].s_clef->u.clef,
				sizeof s->u.clef);
		s->type = CLEF;
		s->voice = voice;
		s->staff = staff;
		s->time = last_s->time;
		s->next = p_voice->sym;
		if (s->next) {
			s->next->prev = s;
			s->fn = s->next->fn;
			s->linenum = s->next->linenum;
			s->colnum = s->next->colnum;
		}
		p_voice->last_sym = p_voice->sym = s;
		s->ts_next = last_s;
		s->ts_prev = last_s->ts_prev;
		if (!s->ts_prev) {
			tsfirst = s;
			s->sflags |= S_SEQST;
		} else {
			s->ts_prev->ts_next = s;
		}
		last_s->ts_prev = s;
		if (last_s->type == CLEF)
			last_s->sflags &= ~S_SEQST;
//		if (cursys->voice[voice].second)
//			s->sflags |= S_SECOND;
		if (staff_tb[staff].s_clef->u.clef.invis
		 || cursys->staff[staff].empty)
			s->flags |= ABC_F_INVIS;
//		set_yval(s);
	}

	/* add keysig */
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		voice = p_voice - voice_tb;
		if (cursys->voice[voice].range < 0
		 || cursys->voice[voice].second
		 || cursys->staff[cursys->voice[voice].staff].empty)
			continue;
		if (last_s->voice == voice && last_s->type == KEYSIG) {
			p_voice->last_sym = last_s;
			last_s->aux = last_s->u.key.sf;	// no key cancel
			last_s = last_s->ts_next;
			continue;
		}
		if (p_voice->key.sf != 0 || p_voice->key.nacc != 0) {
			s = sym_new(KEYSIG, p_voice, last_s);
			memcpy(&s->u.key, &p_voice->key, sizeof s->u.key);
			if (s->u.key.instr == K_Hp)
				s->aux = 3;	/* "A" -> "D" => G natural */
//			set_yval(s);
		}
	}

	/* add time signature if needed */
	if (insert_meter & 1) {
		for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
			voice = p_voice - voice_tb;
			if (cursys->voice[voice].range < 0
			 || cursys->voice[voice].second
			 || cursys->staff[cursys->voice[voice].staff].empty
			 || p_voice->meter.nmeter == 0)		/* M:none */
				continue;
			if (last_s->voice == voice && last_s->type == TIMESIG) {
				p_voice->last_sym = last_s;
				last_s = last_s->ts_next;
				continue;
			}
			s = sym_new(TIMESIG, p_voice, last_s);
			memcpy(&s->u.meter, &p_voice->meter,
			       sizeof s->u.meter);
//			set_yval(s);
		}
		insert_meter &= ~1;		// no meter any more
	}

	/* add bar if needed (for repeat bracket) */
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		int bar_start;

		// if bar already, keep it in sequence
		voice = p_voice - voice_tb;
		if (last_s->voice == voice && last_s->type == BAR) {
			p_voice->last_sym = last_s;
			last_s = last_s->ts_next;
			continue;
		}

		bar_start = p_voice->bar_start;
		if (!bar_start)
			continue;
		p_voice->bar_start = 0;

		if (cursys->voice[voice].range < 0
		 || cursys->voice[voice].second
		 || cursys->staff[cursys->voice[voice].staff].empty)
			continue;

		s = sym_new(BAR, p_voice, last_s);
		s->u.bar.type = bar_start & 0x0fff;
		if (bar_start & 0x8000)
			s->flags |= ABC_F_INVIS;
		if (bar_start & 0x4000)
			s->sflags |= S_NOREPBRA;
		if (bar_start & 0x2000)
			s->flags |= ABC_F_RBSTART;
		if (bar_start & 0x1000)
			s->sflags |= S_RBSTART;
		s->text = p_voice->bar_text;
		s->gch = p_voice->bar_gch;
		if (p_voice->bar_repeat)
			s->u.bar.repeat_bar = p_voice->bar_repeat;

		p_voice->bar_repeat = 0;
		p_voice->bar_text = NULL;
		p_voice->bar_gch = NULL;
	}

	/* if initialization of a new music line, compute the spacing,
	 * including the first (old) sequence */
	set_pitch(last_s);
	s = last_s;
	if (s) {
		for ( ; s; s = s->ts_next)
			if (s->sflags & S_SEQST)
				break;
		if (s)
		    for (s = s->ts_next; s; s = s->ts_next)
			if (s->sflags & S_SEQST)
				break;
	}
	set_allsymwidth(s);	/* set the width of the added symbols */
}

/* -- set a pitch in all symbols and the start/stop of the beams -- */
static void set_words(struct VOICE_S *p_voice)
{
	int pitch, beam_start;
	struct SYMBOL *s, *s2, *lastnote;

	for (s = p_voice->sym; s; s = s->next) {
		if (s->abc_type == ABC_T_NOTE) {
			pitch = s->pits[0];
			break;
		}
	}
	if (!s)
		pitch = 127;			/* no note */
	beam_start = 1;
	lastnote = NULL;
	for (s = p_voice->sym; s; s = s->next) {
		switch (s->type) {
		default:
			if (s->flags & ABC_F_SPACE)
				beam_start = 1;
			break;
		case MREST:
			beam_start = 1;
			break;
		case BAR:
			if (!(s->sflags & S_BEAM_ON))
				beam_start = 1;

			/* change the last long note to the square note */
			if (!s->next && s->prev
			 && s->prev->abc_type == ABC_T_NOTE
			 && s->prev->dur >= BREVE)
				s->prev->head = H_SQUARE;
			break;
		case NOTEREST:
			if (s->sflags & S_TREM2)
				break;
			if (s->flags & ABC_F_SPACE)
				beam_start = 1;
			if (beam_start
			 || s->nflags - s->aux <= 0) {
				if (lastnote) {
					lastnote->sflags |= S_BEAM_END;
					lastnote = NULL;
				}
				if (s->nflags - s->aux <= 0) {
					s->sflags |= (S_BEAM_ST | S_BEAM_END);
				} else if (s->abc_type == ABC_T_NOTE) {
					s->sflags |= S_BEAM_ST;
					beam_start = 0;
				}
			}
			if (s->sflags & S_BEAM_END)
				beam_start = 1;
			if (s->abc_type == ABC_T_NOTE)
				lastnote = s;
			break;
		}
		if (s->abc_type == ABC_T_NOTE) {
			pitch = s->pits[0];
//			if (s->prev
//			 && s->prev->abc_type != ABC_T_NOTE) {
//				s->prev->pits[0] =
//					(s->prev->pits[0] + pitch)
//						/ 2;
			for (s2 = s->prev; s2; s2 = s2->prev) {
				if (s2->abc_type != ABC_T_REST)
					break;
				s2->pits[0] = pitch;
			}
		} else {
			s->pits[0] = pitch;
		}
	}
	if (lastnote)
		lastnote->sflags |= S_BEAM_END;
}

/* -- set the end of the repeat sequences -- */
static void set_rb(struct VOICE_S *p_voice)
{
	struct SYMBOL *s, *s2;
	int mx, n;

	s = p_voice->sym;
	while (s) {
		if (s->type != BAR || !(s->sflags & S_RBSTART)
		 || (s->sflags & S_NOREPBRA)) {
			s = s->next;
			continue;
		}

		mx = cfmt.rbmax;

		/* if 1st repeat sequence, compute the bracket length */
		if (s->text && s->text[0] == '1') {
			n = 0;
			s2 = NULL;
			for (s = s->next; s; s = s->next) {
				if (s->type != BAR)
					continue;
				n++;
				if (s->sflags & S_RBSTOP) {
					if (n <= cfmt.rbmax) {
						mx = n;
						s2 = NULL;
					}
					break;
				}
				if (n == cfmt.rbmin)
					s2 = s;
			}
			if (s2) {
				s2->sflags |= S_RBSTOP;
				mx = cfmt.rbmin;
			}
		}
		while (s) {

			/* check repbra shifts (:| | |2 in 2nd staves) */
			if (!(s->flags & ABC_F_RBSTART)) {
				s = s->next;
				if (!s)
					break;
				if (!(s->flags & ABC_F_RBSTART)) {
					s = s->next;
					if (!s)
						break;
					if (!(s->flags & ABC_F_RBSTART))
						break;
				}
			}
			n = 0;
			s2 = NULL;
			for (s = s->next; s; s = s->next) {
				if (s->type != BAR)
					continue;
				n++;
				if (s->sflags & S_RBSTOP)
					break;
				if (!s->next) {
					s->flags |= ABC_F_RBSTOP;
					s->sflags |= S_RBSTOP;
				} else if (n == mx) {
					s->sflags |= S_RBSTOP;
				}
			}
		}
	}
}

/* -- initialize the generator -- */
/* this function is called only once per tune  */
static void set_global(void)
{
	struct SYSTEM *sy;
	struct SYMBOL *s;
	struct VOICE_S *p_voice;
	int staff;
	static const signed char delpit[4] = {0, -7, -14, 0};

	/* get the max number of staves */
	sy = cursys;
	staff = cursys->nstaff;
	while ((sy = sy->next) != NULL) {
		if (sy->nstaff > staff)
			staff = sy->nstaff;
	}
	nstaff = staff;

	/* adjust the pitches if old abc2ps behaviour of clef definition */
	if (cfmt.abc2pscompat) {
		int i;

		for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
			int delta;
			struct SYMBOL *g;

			if (p_voice->octave != 0)
				continue;
#if 0
			/* (the clefs in the voice table are not yet initialized) */
//			i = p_voice->staff;
//			i = cursys->staff[i].clef.type;
			i = cursys->voice[p_voice - voice_tb].clef.type;
#else
			i = p_voice->s_clef->u.clef.type;
#endif
			if (i == PERC)
				continue;
			delta = delpit[i];
			for (s = p_voice->sym; s; s = s->next) {
				switch (s->type) {
				case CLEF:
					i = s->u.clef.type;
					if (!s->u.clef.check_pitch)
						i = 0;
					delta = delpit[i];
					break;
				case NOTEREST:
					if (delta == 0)
						break;
					if (s->abc_type == ABC_T_REST)
						break;
					for (i = s->nhd; i >= 0; i--)
						s->pits[i] += delta;
					break;
				case GRACE:
					if (delta == 0)
						break;
					for (g = s->extra; g; g = g->next) {
						if (g->type != NOTEREST)
							continue;
						for (i = g->nhd; i >= 0; i--)
							g->pits[i] += delta;
					}
					break;
				}
			}
		}
	}

	/* set the pitches, the words (beams) and the repeat brackets */
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		set_words(p_voice);
//		if (!p_voice->second && !p_voice->norepbra)
			set_rb(p_voice);
	}

	/* set the staff of the floating voices */
	set_float();

	// set the clefs and adjust the pitches of all symbols
	set_clefs();
	set_pitch(NULL);
}

/* -- return the left indentation of the staves -- */
static float set_indent(void)
{
	int staff, voice;
	float w, maxw;
	struct VOICE_S *p_voice;
	char *p, *q;

	maxw = 0;
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		voice = p_voice - voice_tb;
		if (cursys->voice[voice].range < 0)
			continue;
		staff = cursys->voice[voice].staff;
		if (cursys->staff[staff].empty)
			continue;
		if ((p = p_voice->new_name ? p_voice->nm : p_voice->snm) == NULL)
			continue;
		str_font(VOICEFONT);
		for (;;) {
			if ((q = strstr(p, "\\n")) != NULL)
				*q = '\0';
			w = tex_str(p);
			if (w > maxw)
				maxw = w;
			if (!q)
				break;
			*q = '\\';
			p = q + 2;
		}
	}

	if (maxw != 0) {
		w = 0;
//		for (staff = 0; staff <= nstaff; staff++) {
		for (staff = 0; staff <= cursys->nstaff; staff++) {
			if (cursys->staff[staff].flags
					& (OPEN_BRACE2 | OPEN_BRACKET2)) {
				w = 20;
				break;
			}
			if ((cursys->staff[staff].flags
					& (OPEN_BRACE | OPEN_BRACKET))
			 && w == 0)
				w = 10;
		}
		maxw += 4 * cwid(' ') * cfmt.font_tb[VOICEFONT].swfac + w;
	}
	if (insert_meter & 2)			/* if indent */
		maxw += cfmt.indent;
	return maxw;
}

/* -- decide on beams and on stem directions -- */
/* this routine is called only once per tune */
static void set_beams(struct SYMBOL *sym)
{
	struct SYSTEM *sy;
	struct SYMBOL *s, *t, *g, *s_opp;
	int n, m, beam, laststem, mid_p;

	beam = 0;
	laststem = -1;
	s_opp = NULL;
	sy = cursys;
	for (s = sym; s; s = s->next) {
		if (s->abc_type != ABC_T_NOTE) {
			switch (s->type) {
			default:
				continue;
			case STAVES:
				sy = sy->next;
				continue;
			case GRACE:
				break;
			}
			g = s->extra;
			while (g->abc_type != ABC_T_NOTE)
				g = g->next;
			if (g->stem == 2) {	/* opposite gstem direction */
				s_opp = s;
				continue;
			}
			if (s->stem == 0
			 && (s->stem = s->multi) == 0)
				s->stem = 1;
			for (; g; g = g->next) {
				g->stem = s->stem;
				g->multi = s->multi;
			}
			continue;
		}

		mid_p = s->mid / 3 + 18;

		if (s->stem == 0		/* if not explicitly set */
		 && (s->stem = s->multi) == 0) { /* and alone on the staff */

			/* notes in a beam have the same stem direction */
			if (beam) {
				s->stem = laststem;
			} else if ((s->sflags & (S_BEAM_ST | S_BEAM_END))
					== S_BEAM_ST) { /* start of beam */
			    int pu = s->pits[s->nhd],
				pd = s->pits[0];

				beam = 1;
				for (t = s->next; t; t = t->next) {
					if (t->abc_type != ABC_T_NOTE)
						continue;
					if (t->stem || t->multi) {
						s->stem = t->multi ? t->multi : t->stem;
						break;
					}
					if (t->pits[t->nhd] > pu)
						pu = t->pits[t->nhd];
					if (t->pits[0] < pd)
						pd = t->pits[0];
					if (t->sflags & S_BEAM_END)
						break;
				}
				if (t->sflags & S_BEAM_END) {
					mid_p *= 2;
					if (pu + pd < mid_p) {
						s->stem = 1;
					} else if (pu + pd > mid_p) {
						s->stem = -1;
					} else {
						if (cfmt.bstemdown)
							s->stem = -1;
					}
				}
				if (!s->stem)
					s->stem = laststem;
			} else {				// no beam
				n = s->pits[s->nhd] + s->pits[0];
				if (n == mid_p * 2) {
					n = 0;
					for (m = 0; m <= s->nhd; m++)
						n += s->pits[m];
					mid_p *= s->nhd + 1;
				} else {
					mid_p *= 2;
				}
				if (n < mid_p)
					s->stem = 1;
				else if (n > mid_p)
					s->stem = -1;
				else if (cfmt.bstemdown)
					s->stem = -1;
				else
					s->stem = laststem;
			}
		} else {			/* stem set by set_stem_dir */
			if ((s->sflags & (S_BEAM_ST | S_BEAM_END))
					== S_BEAM_ST) /* start of beam */
				beam = 1;
		}
		if (s->sflags & S_BEAM_END)
			beam = 0;
		laststem = s->stem;

		if (s_opp) {			/* opposite gstem direction */
			for (g = s_opp->extra; g; g = g->next)
				g->stem = -laststem;
			s_opp->stem = -laststem;
			s_opp = NULL;
		}
	}
}

/* handle unison in voice overlap */
static int same_head(struct SYMBOL *s1, struct SYMBOL *s2)
{
	int i1, i2, l1, l2, i11, i12, i21, i22;
	float sh1, sh2;

	if ((s1->sflags & (S_SHIFTUNISON_1 | S_SHIFTUNISON_2))
			== (S_SHIFTUNISON_1 | S_SHIFTUNISON_2))
		return 0;
	if ((l1 = s1->dur) >= SEMIBREVE)
		return 0;
	if ((l2 = s2->dur) >= SEMIBREVE)
		return 0;
	if (s1->flags & s2->flags & ABC_F_STEMLESS)
		return 0;
	if (s1->dots != s2->dots) {
//		if ((s1->sflags & (S_SHIFTUNISON_1 | S_SHIFTUNISON_2))
		if ((s1->sflags & S_SHIFTUNISON_1)
		 || s1->dots * s2->dots != 0)
			return 0;
	}
	if (s1->stem * s2->stem > 0)
		return 0;

	/* check if a common unison */
	i1 = i2 = 0;
	if (s1->pits[0] > s2->pits[0]) {
		if (s1->stem < 0)
			return 0;
		while (s2->pits[i2] != s1->pits[0]) {
			if (++i2 > s2->nhd)
				return 0;
		}
	} else if (s1->pits[0] < s2->pits[0]) {
		if (s2->stem < 0)
			return 0;
		while (s2->pits[0] != s1->pits[i1]) {
			if (++i1 > s1->nhd)
				return 0;
		}
	}
	if (s2->u.note.notes[i2].acc != s1->u.note.notes[i1].acc)
		return 0;
	i11 = i1;
	i21 = i2;
	sh1 = s1->u.note.notes[i1].shhd;
	sh2 = s2->u.note.notes[i2].shhd;
	do {
		i1++;
		i2++;
		if (i1 > s1->nhd) {
//			if (s1->pits[0] < s2->pits[0])
//				return 0;
			break;
		}
		if (i2 > s2->nhd) {
//			if (s1->pits[0] > s2->pits[0])
//				return 0;
			break;
		}
		if (s2->u.note.notes[i2].acc != s1->u.note.notes[i1].acc)
			return 0;
		if (sh1 < s1->u.note.notes[i1].shhd)
			sh1 = s1->u.note.notes[i1].shhd;
		if (sh2 < s2->u.note.notes[i2].shhd)
			sh2 = s2->u.note.notes[i2].shhd;
	} while (s2->pits[i2] == s1->pits[i1]);
	if (i1 <= s1->nhd) {
		if (i2 <= s2->nhd)
			return 0;
		if (s2->stem > 0)
			return 0;
	} else if (i2 <= s2->nhd) {
		if (s1->stem > 0)
			return 0;
	}
	i12 = i1;
	i22 = i2;

	if (l1 == l2)
		goto same_head;
	if (l1 < l2) {
		l1 = l2;
		l2 = s1->dur;
	}
	if (l1 < MINIM) {
		if (s2->dots > 0)
			goto head_2;
		if (s1->dots > 0)
			goto head_1;
		goto same_head;
	}
	if (l2 < CROTCHET) {	/* (l1 >= MINIM) */
//		if ((s1->sflags & S_SHIFTUNISON_2)
//		 || s1->dots != s2->dots)
		if (s1->sflags & S_SHIFTUNISON_2)
			return 0;
		if (s2->dur >= MINIM)
			goto head_2;
		goto head_1;
	}
	return 0;

same_head:
	if (voice_tb[s1->voice].scale < voice_tb[s2->voice].scale)
		goto head_2;
head_1:
//	s2->nohdi1 = i21;	/* keep heads of 1st voice */
//	s2->nohdi2 = i22;
	for (i2 = i21; i2 < i22; i2++) {
		s2->u.note.notes[i2].invisible = 1;
		s2->u.note.notes[i2].acc = 0;
	}
	for (i2 = 0; i2 <= s2->nhd; i2++)
		s2->u.note.notes[i2].shhd += sh1;
	return 1;
head_2:
//	s1->nohdi1 = i11;	/* keep heads of 2nd voice */
//	s1->nohdi2 = i12;
	for (i1 = i11; i1 < i12; i1++) {
		s1->u.note.notes[i1].invisible = 1;
		s1->u.note.notes[i1].acc = 0;
	}
	for (i1 = 0; i1 <= s1->nhd; i1++)
		s1->u.note.notes[i1].shhd += sh2;
	return 1;
}

/* width of notes for voice overlap - index = head */
static float w_note[] = {
	3.5, 3.7, 5, 7
};

/* handle unison with different accidentals */
static void unison_acc(struct SYMBOL *s1,
			struct SYMBOL *s2,
			int i1, int i2)
{
	int m;
	float d;

	if (s2->u.note.notes[i2].acc == 0) {
		d = w_note[s2->head] * 2 + s2->xmx + s1->u.note.notes[i1].shac + 2;
		if (s1->u.note.notes[i1].acc & 0xf8)
			d += 2;
		if (s2->dots)
			d += 6;
		for (m = 0; m <= s1->nhd; m++) {
			s1->u.note.notes[m].shhd += d;
			s1->u.note.notes[m].shac -= d;
		}
		s1->xmx += d;
	} else {
		d = w_note[s1->head] * 2 + s1->xmx + s2->u.note.notes[i2].shac + 2;
		if (s2->u.note.notes[i2].acc & 0xf8)
			d += 2;
		if (s1->dots)
			d += 6;
		for (m = 0; m <= s2->nhd; m++) {
			s2->u.note.notes[m].shhd += d;
			s2->u.note.notes[m].shac -= d;
		}
		s2->xmx += d;
	}
}

#define MAXPIT (48 * 2)

/* set the left space of a note/chord */
static void set_left(struct SYMBOL *s, float *left)
{
	int m, i, j;
	float w_base, w, shift;

	for (i = 0; i < MAXPIT; i++)
		left[i] = -100;

	/* stem */
	w = w_base = w_note[s->head];
	if (s->nflags > -2) {
		if (s->stem > 0) {
			w = -w;
			i = s->pits[0] * 2;
			j = ((int) ((s->ymx - 2) / 3) + 18) * 2;
		} else {
			i = ((int) ((s->ymn + 2) / 3) + 18) * 2;
			j = s->pits[s->nhd] * 2;
		}
		if (i < 0)
			i = 0;
		for (; i < MAXPIT && i <= j; i++)
			left[i] = w;
	}

	/* notes */
	if (s->stem > 0)
		shift = s->u.note.notes[0].shhd;	/* previous shift */
	else
		shift = s->u.note.notes[s->nhd].shhd;
	for (m = 0; m <= s->nhd; m++) {
		w = -s->u.note.notes[m].shhd + w_base + shift;
		i = s->pits[m] * 2;
		if (i < 0)
			i = 0;
		else if (i >= MAXPIT - 1)
			i = MAXPIT - 2;
		if (w > left[i])
			left[i] = w;
		if (s->head != H_SQUARE)
			w -= 1;
		if (w > left[i - 1])
			left[i - 1] = w;
		if (w > left[i + 1])
			left[i + 1] = w;
	}
}

/* set the right space of a note/chord */
static void set_right(struct SYMBOL *s, float *right)
{
	int m, i, j, k, flags;
	float w_base, w, shift;

	for (i = 0; i < MAXPIT; i++)
		right[i] = -100;

	/* stem and flags */
	flags = s->nflags > 0
	     && (s->sflags & (S_BEAM_ST | S_BEAM_END))
			== (S_BEAM_ST | S_BEAM_END);
	w = w_base = w_note[s->head];
	if (s->nflags > -2) {
		if (s->stem < 0) {
			w = -w;
			i = ((int) ((s->ymn + 2) / 3) + 18) * 2;
			j = s->pits[s->nhd] * 2;
			k = i + 4;
		} else {
			i = s->pits[0] * 2;
			j = ((int) ((s->ymx - 2) / 3) + 18) * 2;
			k = i;				// (have gcc happy)
		}
		if (i < 0)
			i = 0;
		for ( ; i < MAXPIT && i < j; i++)
			right[i] = w;
	}

	if (flags) {
		if (s->stem > 0) {
			if (s->xmx == 0)
				i = s->pits[s->nhd] * 2;
			else
				i = s->pits[0] * 2;
			i += 4;
			if (i < 0)
				i = 0;
			for (; i < MAXPIT && i <= j - 4; i++)
				right[i] = 11;
		} else {
			i = k;
			if (i < 0)
				i = 0;
			for (; i < MAXPIT && i <= s->pits[0] * 2 - 4; i++)
				right[i] = 3.5;
		}
	}

	/* notes */
	if (s->stem > 0)
		shift = s->u.note.notes[0].shhd;	/* previous shift */
	else
		shift = s->u.note.notes[s->nhd].shhd;
	for (m = 0; m <= s->nhd; m++) {
		w = s->u.note.notes[m].shhd + w_base - shift;
		i = s->pits[m] * 2;
		if (i < 0)
			i = 0;
		else if (i >= MAXPIT - 1)
			i = MAXPIT - 2;
		if (w > right[i])
			right[i] = w;
		if (s->head != H_SQUARE)
			w -= 1;
		if (w > right[i - 1])
			right[i - 1] = w;
		if (w > right[i + 1])
			right[i + 1] = w;
	}
}

/* -- shift the notes horizontally when voices overlap -- */
/* this routine is called only once per tune */
static void set_overlap(void)
{
	struct SYMBOL *s, *s1, *s2, *s3;
	int i, i1, i2, m, sd, t, dp;
	float d, d2, dr, dr2, dx;
	float left1[MAXPIT], right1[MAXPIT], left2[MAXPIT], right2[MAXPIT];
	float right3[MAXPIT], *pl, *pr;

	for (s = tsfirst; s; s = s->ts_next) {
		if (s->abc_type != ABC_T_NOTE
		 || (s->flags & ABC_F_INVIS))
			continue;

		/* treat the stem on two staves with different directions */
		if ((s->sflags & S_XSTEM)
		 && s->ts_prev->stem < 0) {
			s2 = s->ts_prev;
			for (m = 0; m <= s2->nhd; m++) {
				s2->u.note.notes[m].shhd += STEM_XOFF * 2;
				s2->u.note.notes[m].shac -= STEM_XOFF * 2;
			}
			s2->xmx += STEM_XOFF * 2;
		}

		/* search the next note at the same time on the same staff */
		s2 = s;
		for (;;) {
			s2 = s2->ts_next;
			if (!s2)
				break;
			if (s2->time != s->time) {
				s2 = NULL;
				break;
			}
			if (s2->abc_type == ABC_T_NOTE
			 && !(s2->flags & ABC_F_INVIS)
			 && s2->staff == s->staff)
				break;
		}
		if (!s2)
			continue;
		s1 = s;

		/* set the dot vertical offset */
		if (cursys->voice[s1->voice].range < cursys->voice[s2->voice].range)
			s2->doty = -3;
		else
			s1->doty = -3;

		/* no shift if no overlap */
		if (s1->ymn > s2->ymx
		 || s1->ymx < s2->ymn)
			continue;

		if (same_head(s1, s2))
			continue;

		/* compute the minimum space for 's1 s2' and 's2 s1' */
		set_right(s1, right1);
		set_left(s2, left2);

		s3 = s1->ts_prev;
		if (s3 && s3->time == s1->time
		 && s3->staff == s1->staff
		 && s3->abc_type == ABC_T_NOTE
		 && !(s3->flags & ABC_F_INVIS)) {
			set_right(s3, right3);
			for (i = 0; i < MAXPIT; i++) {
				if (right3[i] > right1[i])
					right1[i] = right3[i];
			}
		} else {
			s3 = NULL;
		}
		d = -100;
		for (i = 0; i < MAXPIT; i++) {
			if (left2[i] + right1[i] > d)
				d = left2[i] + right1[i];
		}
		if (d < -3) {			// no clash if no dots clash
			if (!s1->dots || !s2->dots
			 || s2->doty >= 0
			 || s1->stem > 0 || s2->stem < 0
			 || s1->pits[s1->nhd] + 2 != s2->pits[0]
			 || (s2->pits[0] & 1))
				continue;
		}

		set_right(s2, right2);
		set_left(s1, left1);
		if (s3) {
			set_left(s3, right3);
			for (i = 0; i < MAXPIT; i++) {
				if (right3[i] > left1[i])
					left1[i] = right3[i];
			}
		}
		d2 = dr = dr2 = -100;
		for (i = 0; i < MAXPIT; i++) {
			if (left1[i] + right2[i] > d2)
				d2 = left1[i] + right2[i];
			if (right1[i] > dr)
				dr = right1[i];
			if (right2[i] > dr2)
				dr2 = right2[i];
		}

		/* check for unison with different accidentals
		 * and clash of dots */
		t = 0;
		i1 = s1->nhd;
		i2 = s2->nhd;
		for (;;) {
			dp = s1->pits[i1] - s2->pits[i2];
			switch (dp) {
			case 0:
				if (s1->u.note.notes[i1].acc
						!= s2->u.note.notes[i2].acc) {
					t = -1;
					break;
				}
				if (s2->u.note.notes[i2].acc)
					s2->u.note.notes[i2].acc = 0;
				if (s1->dots && s2->dots
				 && (s1->pits[i1] & 1))
					t = 1;
				break;
			case -1:
//				if (s1->dots && s2->dots)
//					t = 1;
				if (s1->dots && s2->dots) {
					if (s1->pits[i1] & 1) {
						s1->doty = 0;
						s2->doty = 0;
					} else {
						s1->doty = -3;
						s2->doty = -3;
					}
				}
				break;
			case -2:
				if (s1->dots && s2->dots
				 && !(s1->pits[i1] & 1)) {
//					t = 1;
					s1->doty = 0;
					s2->doty = 0;
					break;
				}
				break;
			}
			if (t < 0)
				break;
			if (dp >= 0) {
				if (--i1 < 0)
					break;
			}
			if (dp <= 0) {
				if (--i2 < 0)
					break;
			}
		}
		
		if (t < 0) {		/* unison and different accidentals */
			unison_acc(s1, s2, i1, i2);
			continue;
		}

		sd = 0;
		pl = left2;
		pr = right2;
		if (s1->dots) {
			if (s2->dots) {
				if (!t)			/* if no dot clash */
					sd = 1;		/* align the dots */
			}
		} else if (s2->dots) {
			if (d2 + dr < d + dr2)
				sd = 1;			/* align the dots */
		}
		if (!s3 && d2 + dr < d + dr2) {
			s1 = s2;			/* invert the voices */
			s2 = s;
			d = d2;
			pl = left1;
			pr = right1;
			dr2 = dr;
		}
		d += 3;
		if (d < 0)
			d = 0;				// (not return!)

		/* handle the previous shift */
		m = s1->stem >= 0 ? 0 : s1->nhd;
		d += s1->u.note.notes[m].shhd;
		m = s2->stem >= 0 ? 0 : s2->nhd;
		d -= s2->u.note.notes[m].shhd;

		/*
		 * room for the dots
		 * - if the dots of v1 don't shift, adjust the shift of v2
		 * - otherwise, align the dots and shift them if clash
		 */
		if (s1->dots) {
			dx = 7.7 + s1->xmx +		// x 1st dot
				3.5 * s1->dots - 3.5 +	// x last dot
				3;			// some space
			if (!sd) {
				d2 = -100;
				for (i1 = 0; i1 <= s1->nhd; i1++) {
					i = s1->pits[i1];
					if (!(i & 1)) {
						if (s1->doty >= 0)
							i++;
						else
							i--;
					}
					i *= 2;
					if (i < 1)
						i = 1;
					else if (i >= MAXPIT - 1)
						i = MAXPIT - 2;
					if (pl[i] > d2)
						d2 = pl[i];
					if (pl[i - 1] + 1 > d2)
						d2 = pl[i - 1] + 1;
					if (pl[i + 1] + 1 > d2)
						d2 = pl[i + 1] + 1;
				}
				if (dx + d2 + 2 > d)
					d = dx + d2 + 2;
			} else {
				if (dx < d + dr2 + s2->xmx) {
					d2 = 0;
					for (i1 = 0; i1 <= s1->nhd; i1++) {
						i = s1->pits[i1];
						if (!(i & 1)) {
							if (s1->doty >= 0)
								i++;
							else
								i--;
						}
						i *= 2;
						if (i < 1)
							i = 1;
						else if (i >= MAXPIT - 1)
							i = MAXPIT - 2;
						if (pr[i] > d2)
							d2 = pr[i];
						if (pr[i - 1] + 1> d2)
							d2 = pr[i - 1] = 1;
						if (pr[i + 1] + 1 > d2)
							d2 = pr[i + 1] + 1;
					}
					if (d2 > 4.5
					 && 7.7 + s1->xmx + 2 < d + d2 + s2->xmx)
						s2->xmx = d2 + 3 - 7.7;
				}
			}
		}

		for (m = s2->nhd; m >= 0; m--) {
			s2->u.note.notes[m].shhd += d;
//			if (s2->u.note.accs[m] != 0
//			 && s2->pits[m] < s1->pits[0] - 4)
//				s2->shac[m] -= d;
		}
		s2->xmx += d;
		if (sd)
			s1->xmx = s2->xmx;	// align the dots
	}
}

/* -- set the stem lengths -- */
/* this routine is called only once per tune */
static void set_stems(void)
{
	struct SYSTEM *sy;
	struct SYMBOL *s, *s2, *g;
	float slen, scale;
	int ymn, ymx, nflags, mid;

	sy = cursys;
	for (s = tsfirst; s; s = s->ts_next) {
		if (s->abc_type != ABC_T_NOTE) {
			int ymin, ymax;

			switch (s->type) {
			default:
				continue;
			case STAVES:
				sy = sy->next;
				continue;
			case GRACE:
				break;
			}
			ymin = ymax = s->mid;
			for (g = s->extra; g; g = g->next) {
				if (g->type != NOTEREST)
					continue;
				slen = GSTEM;
				if (g->nflags > 1)
					slen += 1.2 * (g->nflags - 1);
				ymn = 3 * (g->pits[0] - 18);
				ymx = 3 * (g->pits[g->nhd] - 18);
				if (s->stem >= 0) {
					g->y = ymn;
					g->ys = ymx + slen;
					ymx = (int) (g->ys + 0.5);
				} else {
					g->y = ymx;
					g->ys = ymn - slen;
					ymn = (int) (g->ys - 0.5);
				}
				ymx += 2;
				ymn -= 2;
				if (ymn < ymin)
					ymin = ymn;
				else if (ymx > ymax)
					ymax = ymx;
				g->ymx = ymx;
				g->ymn = ymn;
			}
			s->ymx = ymax;
			s->ymn = ymin;
			continue;
		}

		/* shift notes in chords (need stem direction to do this) */
		set_head_shift(s);

		/* if start or end of beam, adjust the number of flags
		 * with the other end */
		nflags = s->nflags;
		if ((s->sflags & (S_BEAM_ST | S_BEAM_END)) == S_BEAM_ST) {
			if (s->sflags & S_FEATHERED_BEAM)
				nflags = ++s->nflags;
			for (s2 = s->next; /*s2*/; s2 = s2->next) {
				if (s2->abc_type == ABC_T_NOTE) {
					if (s->sflags & S_FEATHERED_BEAM)
						s2->nflags++;
					if (s2->sflags & S_BEAM_END)
						break;
				}
			}
/*			if (s2) */
			    if (s2->nflags > nflags)
				nflags = s2->nflags;
		} else if ((s->sflags & (S_BEAM_ST | S_BEAM_END)) == S_BEAM_END) {
			for (s2 = s->prev; /*s2*/; s2 = s2->prev) {
				if (s2->sflags & S_BEAM_ST)
					break;
			}
/*			if (s2) */
			    if (s2->nflags > nflags)
				nflags = s2->nflags;
		}

		/* set height of stem end */
		mid = s->mid;
		slen = cfmt.stemheight;
		switch (nflags) {
		case 2: slen += 2; break;
		case 3:	slen += 5; break;
		case 4:	slen += 10; break;
		case 5:	slen += 16; break;
		}
		if ((scale = voice_tb[s->voice].scale) != 1)
			slen *= (scale + 1) * 0.5;
		ymn = 3 * (s->pits[0] - 18);
		if (s->nhd > 0) {
			slen -= 2;
			ymx = 3 * (s->pits[s->nhd] - 18);
		} else {
			ymx = ymn;
		}
		if (s->aux != 0)
			slen += 2 * s->aux;		/* tremolo */
		if (s->flags & ABC_F_STEMLESS) {
			if (s->stem >= 0) {
				s->y = ymn;
				s->ys = ymx;
			} else {
				s->ys = ymn;
				s->y = ymx;
			}
			if (nflags == -4)		/* if longa */
				ymn -= 6;
			s->ymx = ymx + 4;
			s->ymn = ymn - 4;
		} else if (s->stem >= 0) {
			if (nflags >= 2)
				slen -= 1;
			if (s->pits[s->nhd] > 26
			 && (nflags <= 0
			  || (s->sflags & (S_BEAM_ST | S_BEAM_END))
					!= (S_BEAM_ST | S_BEAM_END))) {
				slen -= 2;
				if (s->pits[s->nhd] > 28)
					slen -= 2;
			}
			s->y = ymn;
			if (s->u.note.notes[0].ti1 != 0)
/*fixme
 *			 || s->u.note.ti2[0] != 0) */
				ymn -= 3;
			s->ymn = ymn - 4;
			s->ys = ymx + slen;
			if (s->ys < mid)
				s->ys = mid;
			s->ymx = (int) (s->ys + 2.5);
		} else {			/* stem down */
			if (s->pits[0] < 18
			 && (nflags <= 0
			  || (s->sflags & (S_BEAM_ST | S_BEAM_END))
					!= (S_BEAM_ST | S_BEAM_END))) {
				slen -= 2;
				if (s->pits[0] < 16)
					slen -= 2;
			}
			s->ys = ymn - slen;
			if (s->ys > mid)
				s->ys = mid;
			s->ymn = (int) (s->ys - 2.5);
			s->y = ymx;
/*fixme:the tie may be lower*/
			if (s->u.note.notes[s->nhd].ti1 != 0)
				ymx += 3;
			s->ymx = ymx + 4;
		}
	}
}

/* -- split up unsuitable bars at end of staff -- */
static void check_bar(struct SYMBOL *s)
{
	struct VOICE_S *p_voice;
	int bar_type, i;

	p_voice = &voice_tb[s->voice];

	/* search the last bar */
	while (s->type == CLEF || s->type == KEYSIG || s->type == TIMESIG) {
		if (s->type == TIMESIG
		 && s->time > p_voice->sym->time) /* if not empty voice */
			insert_meter |= 1;	  /* meter in the next line */
		if ((s = s->prev) == NULL)
			return;
	}
	if (s->type != BAR)
		return;

	if (s->u.bar.repeat_bar) {
		p_voice->bar_start = B_OBRA;
		p_voice->bar_text = s->text;
		p_voice->bar_gch = s->gch;
		p_voice->bar_repeat = 1;
		s->text = NULL;
		s->gch = NULL;
		s->u.bar.repeat_bar = 0;
		if (s->flags & ABC_F_INVIS)
			p_voice->bar_start |= 0x8000;
		if (s->sflags & S_NOREPBRA)
			p_voice->bar_start |= 0x4000;
		if (s->flags & ABC_F_RBSTART)
			p_voice->bar_start |= 0x2000;
		if (s->sflags & S_RBSTART)
			p_voice->bar_start |= 0x1000;
	}
	bar_type = s->u.bar.type;
	if (bar_type == B_COL)			/* ':' */
		return;
	if ((bar_type & 0x0f) != B_COL)		/* if not left repeat bar */
		return;
	if (!(s->sflags & S_RRBAR)) {		/* 'xx:' (not ':xx:') */
		if (bar_type == ((B_SINGLE << 8) | B_LREP)) {
			p_voice->bar_start = B_LREP;
			s->u.bar.type = B_DOUBLE;
			return;
		}
		p_voice->bar_start = bar_type & 0x0fff;
		if (s->flags & ABC_F_INVIS)
			p_voice->bar_start |= 0x8000;
		if (s->sflags & S_NOREPBRA)
			p_voice->bar_start |= 0x4000;
		if (s->prev && s->prev->type == BAR)
			unlksym(s);
		else
			s->u.bar.type = B_BAR;
		return;
	}
	if (bar_type == B_DREP) {		/* '::' */
		s->u.bar.type = B_RREP;
		p_voice->bar_start = B_LREP;
		if (s->flags & ABC_F_INVIS)
			p_voice->bar_start |= 0x8000;
		if (s->sflags & S_NOREPBRA)
			p_voice->bar_start |= 0x4000;
		if (s->flags & ABC_F_RBSTART)
			p_voice->bar_start |= 0x2000;
		if (s->sflags & S_RBSTART)
			p_voice->bar_start |= 0x1000;
		return;
	}
	for (i = 0; bar_type != 0; i++)
		bar_type >>= 4;
	bar_type = s->u.bar.type;
	s->u.bar.type = bar_type >> ((i / 2) * 4);
	i = ((i + 1) / 2 * 4);
	bar_type &= 0x0fff;
	p_voice->bar_start = bar_type & ((1 << i) - 1);
	if (s->flags & ABC_F_INVIS)
		p_voice->bar_start |= 0x8000;
	if (s->sflags & S_NOREPBRA)
		p_voice->bar_start |= 0x4000;
	if (s->flags & ABC_F_RBSTART)
		p_voice->bar_start |= 0x2000;
	if (s->sflags & S_RBSTART)
		p_voice->bar_start |= 0x1000;
}

/* -- move the symbols of an empty staff to the next one -- */
static void sym_staff_move(int staff)
//			struct SYMBOL *s,
//			struct SYSTEM *sy)
{
	struct SYMBOL *s;

//	for (;;) {
	for (s = tsfirst; s; s = s->ts_next) {
		if (s->sflags & S_NL)
			break;
		if (s->staff == staff
		 && s->type != CLEF) {
			s->staff++;
			s->flags |= ABC_F_INVIS;
		}
//		s = s->ts_next;
//		if (s == tsnext || s->sflags & S_NEW_SY)
//			break;
	}
}

/* -- adjust the empty flag of a brace system -- */
static void set_brace(struct SYSTEM *sy, char *empty, char *empty_gl)
{
	int staff, i, empty_fl;

	/* if a system brace has empty and non empty staves, keep all staves */
	for (staff = 0; staff <= nstaff; staff++) {
		if (!(sy->staff[staff].flags & (OPEN_BRACE | OPEN_BRACE2)))
			continue;
		empty_fl = 0;
		i = staff;
		while (staff <= nstaff) {
			empty_fl |= empty[staff] ? 1 : 2;
			if (cursys->staff[staff].flags & (CLOSE_BRACE | CLOSE_BRACE2))
				break;
			staff++;
		}
		if (empty_fl == 3) {	/* if empty and not empty staves */
			while (i <= staff) {
				empty[i] = 0;
				empty_gl[i++] = 0;
			}
		}
	}
}

/* -- define the start and end of a piece of tune -- */
/* tsnext becomes the beginning of the next line */
static void set_piece(void)
{
	struct SYSTEM *sy;
	struct SYMBOL *s;
	struct VOICE_S *p_voice;
	struct STAFF_S *p_staff;
	int staff;
	char empty[MAXSTAFF], empty_gl[MAXSTAFF];

	/* reset the staves */
	sy = cursys;
	for (staff = 0; staff <= nstaff; staff++) {
		p_staff = &staff_tb[staff];
		p_staff->y = 0;		/* staff system not computed */
		p_staff->stafflines = sy->staff[staff].stafflines;
		p_staff->staffscale = sy->staff[staff].staffscale;
	}

	/* search the next end of line,
	 * set the repeat measures, (remove some dble bars?)
	 * and flag the empty staves
	 */
	memset(empty, 1, sizeof empty);
	memset(empty_gl, 1, sizeof empty_gl);
	for (s = tsfirst; s; s = s->ts_next) {
		if (s->sflags & S_NL)
			break;
		if (s->sflags & S_NEW_SY) {
			set_brace(sy, empty, empty_gl);
			for (staff = 0; staff <= nstaff; staff++) {
				sy->staff[staff].empty = empty[staff];
				empty[staff] = 1;
			}
			sy = sy->next;
			for (staff = 0; staff <= sy->nstaff; staff++) {
				p_staff = &staff_tb[staff];
				p_staff->stafflines = sy->staff[staff].stafflines;
				if (!p_staff->stafflines)
					p_staff->stafflines = "|||||";
				p_staff->staffscale = sy->staff[staff].staffscale;
				if (p_staff->staffscale == 0)
					p_staff->staffscale = 1;
			}
		}
		if (!empty[s->staff])
			continue;
		switch (s->type) {
		case GRACE:
			empty_gl[s->staff] = empty[s->staff] = 0;
			break;
		case NOTEREST:
		case SPACE:
		case MREST:
			if (cfmt.staffnonote > 1) {
				empty_gl[s->staff] = empty[s->staff] = 0;
			} else if (!(s->flags & ABC_F_INVIS)) {
				if (s->abc_type == ABC_T_NOTE
				 || cfmt.staffnonote != 0)
					empty_gl[s->staff] = empty[s->staff] = 0;
			}
			break;
		}
	}
	tsnext = s;

	/* set the last empty staves */
	set_brace(sy, empty, empty_gl);
	for (staff = 0; staff <= nstaff; staff++)
		sy->staff[staff].empty = empty[staff];

	/* define the offsets of the measure bars */
	for (staff = 0; staff <= nstaff; staff++) {
		int i, l;
		char *stafflines;

		if (empty_gl[staff])
			continue;

		p_staff = &staff_tb[staff];
		stafflines = p_staff->stafflines;
		l = strlen(stafflines);
		p_staff->topbar = 6 * (l - 1);
		for (i = 0; i < l - 1; i++)
			if (stafflines[i] != '.')
				break;
		p_staff->botbar = i * 6;
		if (i >= l - 2) {		// 0, 1 or 2 lines
			p_staff->botbar -= 6;
			p_staff->topbar += 6;
		}
	}

	/* move the symbols of the empty staves to the next staff */
//	sy = cursys;
	for (staff = 0; staff < nstaff; staff++) {
#if 1
		if (empty_gl[staff])
			sym_staff_move(staff);
#else
		if (sy->staff[staff].empty)
			sym_staff_move(staff, tsfirst, sy);
	}
	if (sy->next) {
		for (s = tsfirst; s; s = s->ts_next) {
			if (s->sflags & S_NL)
				break;
			if (s->sflags & S_NEW_SY) {
				sy = sy->next;
				for (staff = 0; staff < nstaff; staff++) {
					if (sy->staff[staff].empty)
						sym_staff_move(staff, s, sy);
				}
				if (!sy->next)
					break;
			}
		}
#endif
	}

	/* initialize the music line */
	init_music_line();

	/* if last music line, nothing more to do */
	if (!tsnext)
		return;

	s = tsnext;
	s->sflags &= ~S_NL;
	s = s->ts_prev;
	s->ts_next = NULL;

	/* set the end of the voices */
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		int voice;

		voice = p_voice - voice_tb;
		for (s = tsnext->ts_prev; s; s = s->ts_prev) {
			if (s->voice == voice) {
				s->next = NULL;
				check_bar(s);
				break;
			}
		}
		if (!s)
			p_voice->sym = NULL;
	}
}

/* -- position the symbols along the staff -- */
static void set_sym_glue(float width)
{
	struct SYMBOL *s;
	float beta0, alfa, beta;
	int some_grace;
	float xmin, x, xmax, spafac;

	/* calculate the whole space of the symbols */
	some_grace = 0;
	xmin = x = xmax = 0;
	s = tsfirst;
	for (;;) {
		if (s->type == GRACE)
			some_grace = 1;
		if (s->sflags & S_SEQST) {
			float space;

			xmin += s->shrink;
			if ((space = s->space) < s->shrink)
				space = s->shrink;
			x += space;
			if (cfmt.stretchstaff)
				space *= 1.8;
			xmax += space;
		}
		if (!s->ts_next)
			break;
		s = s->ts_next;
	}

	/* set max shrink and stretch */
	if (!cfmt.continueall)
		beta0 = BETA_X;
	else
		beta0 = BETA_C;

	/* memorize the glue for the last music line */
	if (tsnext) {
		if (x - width >= 0) {
			beta_last = 0;
		} else {
			beta_last = (width - x) / (xmax - x);	/* stretch */
			if (beta_last > beta0) {
				if (cfmt.stretchstaff) {
					if (!cfmt.continueall
					 && cfmt.linewarn) {
						error(0, s,
						      "Line underfull (%.0fpt of %.0fpt)",
							beta0 * xmax + (1 - beta0) * x,
							width);
					}
				} else {
					width = x;
					beta_last = 0;
				}
			}
		}
	} else {			/* if last music line */
		if (x < width) {
			beta = (width - x) / (xmax - x);	/* stretch */
			if (beta >= beta_last) {
				beta = beta_last * xmax + (1 - beta_last) * x;

				/* shrink underfull last line same as previous */
				if (beta < width * (1. - cfmt.stretchlast))
					width = beta;
			}
		}
	}

	spafac = width / x;			/* space expansion factor */

	/* define the x offsets of all starting symbols */
	x = xmax = 0;
	s = tsfirst;
	for (;;) {
		if (s->sflags & S_SEQST) {
			float new_space;

			new_space = s->shrink;
			if (s->space != 0) {
				if (new_space < s->space * spafac)
					new_space = s->space * spafac;
				xmax += s->space * spafac * 1.8;
			}
			x += new_space;
			xmax += new_space;
			s->x = x;
			s->xmax = xmax;
		}
		if (!s->ts_next)
			break;
		s = s->ts_next;
	}

	/* if the last symbol is not a bar, add some extra space */
	switch (s->type) {
	case BAR:
	case FMTCHG:
		break;
	case CUSTOS:
		x += s->wr;
		xmin += s->wr;
		xmax += s->wr;
		break;
	default: {
		float min;

		min = s->wr;
		while (!(s->sflags & S_SEQST)) {
			s = s->ts_prev;
			if (s->wr > min)
				min = s->wr;
		}
		xmin += min + 3;
		if (tsnext && tsnext->space * 0.8 > s->wr + 4) {
			x += tsnext->space * 0.8 * spafac;
			xmax += tsnext->space * 0.8 * spafac * 1.8;
		} else {
#if 1
			x += min + 4;
			xmax += min + 4;
#else
/*fixme:should calculate the space according to the last symbol duration */
			x += (min + 4) * spafac;
			xmax += (min + 4) * spafac * 1.8;
#endif
		}
		break;
	    }
	}

	/* calculate the exact glue */
	if (x >= width) {
		beta = 0;
		if (x == xmin) {
			alfa = 1;			// no extra space
		} else {
			alfa = (x - width) / (x - xmin);	/* shrink */
			if (alfa > 1) {
				error(1, s,
				      "Line too much shrunk (%.0f/%0.fpt of %.0fpt)",
					xmin, x, width);
// uncomment for staff greater than music line
//				alfa = 1;
			}
		}
		realwidth = xmin * alfa + x * (1 - alfa);
	} else {
		alfa = 0;
		if (xmax > x)
			beta = (width - x) / (xmax - x);	/* stretch */
		else
			beta = 1;				/* (no note) */
		if (beta > beta0) {
			if (!cfmt.stretchstaff)
				beta = 0;
		}
		realwidth = xmax * beta + x * (1 - beta);
	}

	/* set the final x offsets */
	s = tsfirst;
	if (alfa != 0) {
		if (alfa < 1) {
			x = xmin = 0;
			for (; s; s = s->ts_next) {
				if (s->sflags & S_SEQST) {
					xmin += s->shrink * alfa;
					x = xmin + s->x * (1 - alfa);
				}
				s->x = x;
			}
		} else {
			alfa = realwidth / x;
			x = 0;
			for (; s; s = s->ts_next) {
				if (s->sflags & S_SEQST)
					x = s->x * alfa;
				s->x = x;
			}
		}
	} else {
		x = 0;
		for (; s; s = s->ts_next) {
			if (s->sflags & S_SEQST)
				x = s->xmax * beta + s->x * (1 - beta);
			s->x = x;
		}
	}

	/* set the x offsets of the grace notes */
	if (some_grace) {
		for (s = tsfirst; s; s = s->ts_next) {
			struct SYMBOL *g;

			if (s->type != GRACE)
				continue;
			x = s->x - s->wl + (cfmt.gracespace >> 16) * 0.1;
			for (g = s->extra; g; g = g->next)
				if (g->type == NOTEREST)
					g->x += x;
		}
	}
}

/* -- initialize a new music line -- */
static void new_music_line(void)
{
	struct VOICE_S *p_voice;
	struct SYMBOL *s;
	int voice;

	/* set the first symbol of each voice */
	tsfirst->ts_prev = NULL;
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		p_voice->sym = NULL;		/* may have no symbol */
		voice = p_voice - voice_tb;
		for (s = tsfirst; s; s = s->ts_next) {
			if (s->voice == voice) {
				p_voice->sym = s;
				s->prev = NULL;
				break;
			}
		}
	}

}

/* -- initialize the start of generation / new music line -- */
static void gen_init(void)
{
	struct SYMBOL *s;

	for (s = tsfirst ; s; s = s->ts_next) {
		if (s->extra) {
			output_ps(s, 0);
			if (!s->extra && s->type == FMTCHG) {
				unlksym(s);
				if (!tsfirst)
					return;
			}
		}
		if (s->sflags & S_NEW_SY) {
			s->sflags &= ~S_NEW_SY;
			cursys = cursys->next;
		}
		switch (s->type) {
		case CLEF:
		case KEYSIG:
		case TIMESIG:
			continue;
//		default:
//			break;		/* may be Q: */
		}
		return;
	}
	tsfirst = NULL;			/* no more notes */
}

/* -- show the errors -- */
static void error_show(void)
{
	struct SYMBOL *s;

	for (s = tsfirst; s; s = s->ts_next) {
		if (s->flags & ABC_F_ERROR) {
			putxy(s->x, staff_tb[s->staff].y + s->y);
			a2b("showerror\n");
		}
	}
}

/* -- delay output until the staves are defined (by draw_systems) -- */
static float delayed_output(float indent)
{
	float line_height;
	char *outbuf_sav, *mbf_sav, *tmpbuf;

	outbuf_sav = outbuf;
	mbf_sav = mbf;
	tmpbuf = malloc(outbufsz);
	if (!tmpbuf) {
		error(1, NULL, "Out of memory for delayed outbuf - abort");
		exit(EXIT_FAILURE);
	}
	mbf = outbuf = tmpbuf;
	*outbuf = '\0';
	outft = -1;
	draw_sym_near();
	outbuf = outbuf_sav;
	mbf = mbf_sav;
	outft = -1;
	line_height = draw_systems(indent);
	a2b("%s", tmpbuf);
	free(tmpbuf);
	return line_height;
}

/* -- generate the music -- */
void output_music(void)
{
	struct VOICE_S *p_voice;
	float lwidth, indent;

	/* set the staff system if any STAVES at start of the next line */
	gen_init();
	if (!tsfirst)
		return;
	check_buffer();
	set_global();			/* initialize the generator */
	if (first_voice->next) {	/* if many voices */
//		if (cfmt.combinevoices >= 0)
			combine_voices();
		set_stem_dir();		/* set the stems direction in 'multi' */
	}
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next)
		set_beams(p_voice->sym);	/* decide on beams */
	set_stems();			/* set the stem lengths */
	if (first_voice->next) {	/* when multi-voices */
		set_rest_offset();	/* set the vertical offset of rests */
		set_overlap();		/* shift the notes on voice overlap */
	}
	set_acc_shft();			// set the horizontal offset of accidentals
	set_allsymwidth(NULL);		/* set the width of all symbols */

	lwidth = ((cfmt.landscape ? cfmt.pageheight : cfmt.pagewidth)
		- cfmt.leftmargin - cfmt.rightmargin)
			/ cfmt.scale;
	if (lwidth < 50) {
		error(1, 0, "Bad page width %.1f", lwidth);
		lwidth = 10 CM;
	}
	indent = set_indent();
	cut_tune(lwidth, indent);
	beta_last = 0;
	for (;;) {			/* loop per music line */
		float line_height;

		set_piece();
		indent = set_indent();
		set_sym_glue(lwidth - indent);
		if (indent != 0)
			a2b("%.2f 0 T\n", indent); /* do indentation */
		line_height = delayed_output(indent);
		draw_all_symb();
		draw_all_deco();
		if (showerror)
			error_show();
		bskip(line_height);
		if (indent != 0) {
			a2b("%.2f 0 T\n", -indent);
			insert_meter &= ~2;	// no more indentation
		}
		tsfirst = tsnext;
		gen_init();
		if (!tsfirst)
			break;
		buffer_eob(0);
		new_music_line();
	}
	outft = -1;
}

/* -- reset the generator -- */
void reset_gen(void)
{
	if (cfmt.fields[0] & (1 << ('M' - 'A')))
		insert_meter = 3;	/* insert meter and indent */
	else
		insert_meter = 2;	/* indent only */
}
