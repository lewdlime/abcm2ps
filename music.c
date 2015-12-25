/*
 * Music generator.
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
#include <string.h>
#include <time.h>
#include <ctype.h>

#include "abcparse.h"
#include "abc2ps.h"

struct SYMBOL *tsnext;		/* next line when cut */
float realwidth;		/* real staff width while generating */

static int insert_meter;	/* flag to insert time signature */
static float alfa_last, beta_last;	/* for last short short line.. */

#define AT_LEAST(a,b)  do { float tmp = b; if(a<tmp) a=tmp; } while (0)

/* width of notes indexed by log2(note_length) */
float space_tb[NFLAGS_SZ] = {
	7, 10, 14.15, 20, 28.3,
	40,				/* crotchet */
	56.6, 80, 113, 150
};
float dot_space = 1.2;			/* space factor when dots */

/* upper and lower space needed by rests */
static struct {
	char u, l;
} rest_sp[NFLAGS_SZ] = {
	{16, 31},
	{16, 25},
	{16, 19},
	{10, 19},
	{10, 13},
	{10, 13},			/* crotchet */
	{7, 7},
	{10, 4},
	{10, 7},
	{10, 13}
};

/* -- decide whether to shift heads to other side of stem on chords -- */
/* also position accidentals to avoid too much overlap */
/* this routine is called only once per tune */
static void set_head_directions(struct SYMBOL *s)
{
	int i, i1, i2, i3, n, sig, d, shift;
	int p1, p2, p3, ps, m, nac;
	float dx, dx1, dx2, dx3, shmin, shmax;
	unsigned char ax_tb[MAXHD], ac_tb[MAXHD];
	static float dx_tb[4] = {
		9, 10, 12, 14
	};
	/* distance for no overlap - index: [prev acc][cur acc] */
	static char dt_tb[4][4] = {
		{5, 5, 5, 5},		/* dble sharp */
		{5, 6, 6, 6},		/* sharp */
		{5, 6, 5, 6},		/* natural */
		{5, 5, 5, 5}		/* flat */
	};

	/* special case when single note */
	if ((n = s->nhd) == 0) {
		if (s->as.u.note.accs[0] != 0) {
			dx = dx_tb[s->head];
			if (s->as.u.note.grace)
				dx *= 0.7;
			s->shac[0] = dx;
		}
		return;
	}

	/* set the head shifts */
	dx = dx_tb[s->head] * 0.78;
	if (s->as.u.note.grace)
		dx *= 0.7;
	i1 = 1;
	i2 = n + 1;
	sig = s->stem;
	if (sig < 0) {
		dx = -dx;
		i1 = n - 1;
		i2 = -1;
	}
	shift = 0;
	nac = 0;
	for (i = i1; i != i2; i += sig) {
		d = s->pits[i] - s->pits[i - sig];
		if (d < 0)
			d = -d;
		if (d > 3 || (d >= 2 && s->head < H_SQUARE))
			shift = 0;
		else {
			shift = !shift;
			if (shift) {
				s->shhd[i] = dx;
				nac++;
			}
		}
	}
	if (nac != 0 && sig > 0)
		s->xmx = dx;		/* shift the dots */

	/* set the accidental shifts */
	nac = 0;
	for (i = n; i >= 0; i--) {	/* from top to bottom */
		if ((i1 = s->as.u.note.accs[i]) != 0) {
			ax_tb[nac++] = i;
			if (i1 & 0xf8)
				i1 = A_SH;	/* micro-tone same as sharp */
			else if (i1 == A_DF)
				i1 = A_FT;	/* dble flat same as flat */
			else if (i1 == A_DS)
				i1 = 0;		/* (max -> 0) */
			ac_tb[i] = i1;
		}
	}
	if (nac == 0)			/* no accidental */
		return;
	dx = dx_tb[s->head];
	m = n;
	p2 = i2 = 0;			/* (compiler warning) */
	dx2 = 0;
	ps = 255;
	for (i = 0; i < nac; i++) {
		i1 = ax_tb[i];
		p1 = s->pits[i1];
		if (m >= 0) {		/* see if any head shift */
			if (ps - s->pits[i1] >= 4) {
				for (m--; m >= 0; m--) {
					if (s->shhd[m] < 0) {
						ps = s->pits[m];
						break;
					}
				}
			}
		}
		dx1 = dx;
		if (m >= 0 && s->shhd[m] < 0
		    && ps - p1 < 4 && ps - p1 > -4)
			dx1 -= s->shhd[m];
		if (s->as.u.note.grace)
			dx1 *= 0.7;
		if (i == 0) {	/* no other shift for the 1st accidental */
			s->shac[i1] = dx1;
			i2 = i1;
			p2 = p1;
			dx2 = dx1;
			continue;
		}
		d = dt_tb[ac_tb[i2]][ac_tb[i1]];
		if (p2 - p1 < d) {		/* if possible overlap */
			if (s->as.u.note.accs[i1] & 0xf8) {	/* microtonal */
				shmin = 6.5;
				shmax = 9;
			} else {
				shmin = 4.5;
				shmax = 7;
			}
			if (s->as.u.note.grace) {
				shmin *= 0.7;
				shmax *= 0.7;
			}
			if (i >= 2) {
				i3 = ax_tb[i - 2];
				p3 = s->pits[i3];
				d = dt_tb[ac_tb[i3]][ac_tb[i1]];
				if (p3 - p1 < d) {
					dx3 = s->shac[i3];
					if (p3 - p1 >= 4
					    && (ac_tb[i3] != A_SH || ac_tb[i1] != A_SH)) {
						if (dx1 > dx3 - shmin && dx1 < dx3 + shmin)
							dx1 = dx3 + shmin;
					} else {
						if (dx1 > dx3 - shmax && dx1 < dx3 + shmax)
							dx1 = dx3 + shmax;
					}
				}
			}
			if (p2 - p1 >= 4
			    && (ac_tb[i2] != A_SH || ac_tb[i1] != A_SH)) {
				if (dx1 > dx2 - shmin && dx1 < dx2 + shmin) {
					if (dx1 + shmin < dx2 + shmin)
						s->shac[i2] = dx1 + shmin;
					else	dx1 = dx2 + shmin;
				}
			} else {
				if (dx1 > dx2 - shmax && dx1 < dx2 + shmax) {
					if (dx1 + shmax < dx2 + shmax)
						s->shac[i2] = dx1 + shmax;
					else	dx1 = dx2 + shmax;
				}
			}
		}
		s->shac[i1] = dx1;
		i2 = i1;
		p2 = p1;
		dx2 = dx1;
	}
}

/* -- delete a symbol -- */
static struct SYMBOL *delsym(struct SYMBOL *s)
{
	if (s->next != 0)
		s->next->prev = s->prev;
	if (s->prev != 0)
		s->prev->next = s->next;
	if (s->ts_next != 0)
		s->ts_next->ts_prev = s->ts_prev;
	if (s->ts_prev != 0)
		s->ts_prev->ts_next = s->ts_next;
	if (tsnext == s)
		tsnext = s->ts_next;
	return s->next;
}

/* -- check if voice combine may occur -- */
static int may_combine(struct SYMBOL *s)
{
	struct SYMBOL *s2;
	int nhd2;

	if ((s2 = s->ts_next) == 0)
		return 0;
	if (s2->voice == s->voice
	    || s2->staff != s->staff
	    || s2->time != s->time
	    || s2->len != s->len
	    || s2->ly != 0
	    || ((s2->sflags ^ s->sflags) & S_WORD_ST)
	    || s2->as.u.note.word_end != s->as.u.note.word_end
	    || (s2->sflags & (S_SL1 | S_SL2))
	    || s2->as.u.note.slur_st != 0
	    || s2->as.u.note.slur_end != 0)
		return 0;
	nhd2 = s2->nhd;
	if (s->nhd + nhd2 + 1 >= MAXHD)
		return 0;
	if (!cfmt.comball && s->pits[0] <= s2->pits[nhd2] + 1)
		return 0;
	if (s2->as.u.note.dc.n != 0) {
		if (s2->as.u.note.dc.h != s2->as.u.note.dc.h
		    || memcmp(&s->as.u.note.dc, &s2->as.u.note.dc,
				sizeof s->as.u.note.dc) != 0)
			return 0;
	}
	return 1;
}

/* -- combine 2 voices -- */
static void do_combine(struct SYMBOL *s)
{
	struct SYMBOL *s2;
	int nhd, nhd2;

	nhd = s->nhd;
	s2 = s->ts_next;
	nhd2 = s2->nhd;
	if (s->type != s2->type) {	/* if note and rest */
		if (s2->type == REST) {
			delsym(s2);	/* remove the rest */
			return;
		}
		s->type = NOTE;		/* copy the note into the rest */
		nhd = -1;
		s->pits[0] = 127;
	}

	/* combine the voices */
	if (s->pits[0] >= s2->pits[nhd2]) {	/* standard case */
		if (s->pits[0] == s2->pits[nhd2])
			nhd2--;
		memcpy(&s->pits[nhd2 + 1], s->pits,
			sizeof s->pits[0] * (nhd + 1));
		memcpy(s->pits, s2->pits,
			sizeof s->pits[0] * (nhd2 + 1));

#define COMBINEV(f)\
    memcpy(&s->as.u.note.f[nhd2 + 1], s->as.u.note.f,\
	sizeof s->as.u.note.f[0] * (nhd + 1));\
    memcpy(s->as.u.note.f, s2->as.u.note.f,\
	sizeof s->as.u.note.f[0] * (nhd2 + 1))

		COMBINEV(pits);
		COMBINEV(lens);
		COMBINEV(accs);
		COMBINEV(sl1);
		COMBINEV(sl2);
		COMBINEV(ti1);
		COMBINEV(decs);
#undef COMBINEV
		nhd += nhd2 + 1;
		s->nhd = nhd;
		s->ymn = 3 * (s->pits[0] - 18) - 2;
/*fixme:should recalculate yav*/
	} else {				/* voice inverted */
/*fixme:KO if imbricated chords*/
		memcpy(&s->pits[nhd + 1], s2->pits,
			sizeof s->pits[0] * (nhd2 + 1));

#define COMBINEV(f)\
    memcpy(&s->as.u.note.f[nhd + 1], s2->as.u.note.f,\
	sizeof s->as.u.note.f[0] * (nhd2 + 1));\

		COMBINEV(pits);
		COMBINEV(lens);
		COMBINEV(accs);
		COMBINEV(sl1);
		COMBINEV(sl2);
		COMBINEV(ti1);
		COMBINEV(decs);
#undef COMBINEV
		nhd += nhd2 + 1;
		s->nhd = nhd;
/*fixme:should recalculate yav*/
		s->ymx = 3 * (s->pits[nhd] - 18) + 2;
	}
	if (s->as.u.note.ti1[0] == SL_AUTO)	/* force the tie directions */
		s->as.u.note.ti1[0] = SL_BELOW;
	if (s->as.u.note.ti1[nhd] == SL_AUTO)
		s->as.u.note.ti1[nhd] = SL_ABOVE;

	delsym(s2);			/* remove the next symbol */
}

/* -- try to combine voices */
static void combine_voices(void)
{
	struct SYMBOL *s, *s2;
	int r;

	for (s = first_voice->sym; s != 0; s = s->ts_next) {
		switch (s->type) {
		case TUPLET:
			if ((s2 = s->ts_next) == 0)
				break;
			r = s->as.u.tuplet.r_plet;
			if (s2->type != TUPLET
			    || s2->as.u.tuplet.r_plet != r
			    || s2->as.u.tuplet.p_plet != s->as.u.tuplet.p_plet
			    || s2->as.u.tuplet.q_plet != s->as.u.tuplet.q_plet)
				continue;
			s2 = s;
			for (;;) {
				s2 = s2->next;
				while (s2->type != NOTE && s2->type != REST)
					s2 = s2->next;
				if (!may_combine(s2))
					break;
				if (--r <= 0)
					break;
			}
			if (r > 0)
				continue;
			delsym(s->ts_next);	/* remove the tuplet */
			r = s->as.u.tuplet.r_plet;
			s2 = s;
			for (;;) {
				s2 = s2->next;
				while (s2->type != NOTE && s2->type != REST)
					s2 = s2->next;
				do_combine(s2);
				if (--r <= 0)
					break;
			}
			continue;
		default:
			continue;
		case NOTE:
		case REST:
			break;
		}
		if (s->ts_next == 0)
			break;
		if ((s->sflags & S_IN_TUPLET) || s->prev->type == TUPLET)
			continue;
		if (!(s->sflags & S_WORD_ST))
			continue;
		if (!s->as.u.note.word_end) {
			s2 = s;
			for (;;) {
				if (!may_combine(s2)) {
					s2 = 0;
					break;
				}
				if (s2->as.u.note.word_end)
					break;
				do {
					s2 = s2->next;
				} while (s2->type != NOTE && s2->type != REST);
			}
			if (s2 == 0)
				continue;
			s2 = s;
			for (;;) {
				do_combine(s2);
				if (s2->as.u.note.word_end)
					break;
				do {
					s2 = s2->next;
				} while (s2->type != NOTE && s2->type != REST);
			}
		}
		if (may_combine(s))
			do_combine(s);
	}
}

/* -- insert a clef change (treble or bass) before a symbol -- */
static void insert_clef(struct SYMBOL *s,
			int clef_type)
{
	struct VOICE_S *p_voice;
	struct SYMBOL *s2, *new_s;
	int staff, time, seq;

	/* search the main voice of the staff */
	staff = s->staff;
	p_voice = &voice_tb[s->voice];
	time = s->time;
	seq = s->seq;
	s2 = s;
	if (p_voice->second) {
		for (p_voice = first_voice; ; p_voice = p_voice->next)
			if (p_voice->staff == staff
			    /* && !p_voice->second */ )
				break;
		while (s2->ts_prev->time == time
		       && s2->ts_prev->seq == seq)
			s2 = s2->ts_prev;
		for (; s2 != 0; s2 = s2->ts_next) {
			if (s2->voice == p_voice - voice_tb)
				break;
		}
		if (s2 == 0) {
			for (s2 = p_voice->sym; s2->next != 0; s2 = s2->next)
				;
		}
	}

	/* create the symbol */
	p_voice->last_symbol = s2->prev;
	new_s = add_sym(p_voice, CLEF);
	new_s->next = s2;
	s2->prev = new_s;

	new_s->as.u.clef.type = clef_type;
	new_s->as.u.clef.line = clef_type == TREBLE ? 2 : 4;
	new_s->as.u.clef.stafflines = -1;
	new_s->staff = staff;
	new_s->u = 1;		/* small clef */

	/* link in time */
	while (s->ts_prev->time == time
	       && s->ts_prev->seq == seq)
		s = s->ts_prev;
	new_s->ts_prev = s->ts_prev;
	new_s->ts_prev->ts_next = new_s;
	new_s->ts_next = s;
	s->ts_prev = new_s;
	new_s->time = time;
}

/* -- define the clef for a staff -- */
/* this function is called only once for the whole tune */
static void set_clef(int staff)
{
	struct SYMBOL *s, *last_chg;
	int clef_type, min, max, time;

	/* get the max and min pitches */
	min = max = 16;			/* 'C' */
	for (s = first_voice->sym; s != 0; s = s->ts_next) {
		if (s->staff != staff || s->type != NOTE)
			continue;
		if (s->pits[0] < min)
			min = s->pits[0];
		else if (s->pits[s->nhd] > max)
			max = s->pits[s->nhd];
	}

	staff_tb[staff].clef.type = TREBLE;
	staff_tb[staff].clef.line = 2;
	if (min >= 13)			/* all upper than 'G,' --> treble clef */
		return;
	if (max <= 19) {		/* all lower than 'F' --> bass clef */
		staff_tb[staff].clef.type = BASS;
		staff_tb[staff].clef.line = 4;
		return;
	}

	/* set clef changes */
	clef_type = TREBLE;
	last_chg = 0;
	for (s = first_voice->sym; s != 0; s = s->ts_next) {
		struct SYMBOL *s2, *s3, *s4;

		if (s->staff != staff || s->type != NOTE)
			continue;

		/* check if a clef change may occur */
		time = s->time;
		if (clef_type == TREBLE) {
			if (s->pits[0] > 12		/* F, */
			    || s->pits[s->nhd] > 20)	/* G */
				continue;
			s2 = s->ts_prev;
			if (s2->time == time
			    && s2->staff == staff
			    && s2->type == NOTE
			    && s2->pits[0] >= 19)	/* F */
				continue;
			s2 = s->ts_next;
			if (s2 != 0
			    && s2->staff == staff
			    && s2->time == time
			    && s2->type == NOTE
			    && s2->pits[0] >= 19)	/* F */
				continue;
		} else {
			if (s->pits[0] < 12		/* F, */
			    || s->pits[s->nhd] < 20)	/* G */
				continue;
			s2 = s->ts_prev;
			if (s2->time == time
			    && s2->staff == staff
			    && s2->type == NOTE
			    && s2->pits[0] <= 13)	/* G, */
				continue;
			s2 = s->ts_next;
			if (s2 != 0
			    && s2->staff == staff
			    && s2->time == time
			    && s2->type == NOTE
			    && s2->pits[0] <= 13)	/* G, */
				continue;
		}

		/* go backwards and search where to insert a clef change */
#if 1 /*fixme:test*/
		s3 = s;
#else
		if (!voice_tb[s->voice].second
		    && voice_tb[s->voice].staff == staff)
			s3 = s;
		else	s3 = 0;
#endif
		time = last_chg == 0 ? 0 : last_chg->time;
		for (s2 = s->ts_prev; s2 != last_chg; s2 = s2->ts_prev) {
			if (s2->time <= time)
				break;
			if (s2->staff != staff)
				continue;
			if (s2->type == BAR) {
#if 0 /*fixme:test*/
				if (voice_tb[s2->voice].second
				    || voice_tb[s2->voice].staff != staff)
					continue;
#endif
				s3 = s2;
				break;
			}
#if 1
			if (s2->type != NOTE)
#else
			if (s2->len == 0)	/* neither note nor rest */
#endif
				continue;

			/* exit loop if a clef change cannot occur */
			if (s2->type == NOTE) {
				if (clef_type == TREBLE) {
					if (s2->pits[0] >= 19)		/* F */
						break;
				} else {
					if (s2->pits[s2->nhd] <= 13)	/* G, */
						break;
				}
			}

#if 1 /*fixme:test*/
#if 1
			/* have a 2nd choice on beam start */
			if ((s3->sflags & S_WORD_ST) == 0)
				s3 = s2;
#else
			/* have a 2nd choice on beam start */
			if ((s2->sflags & S_WORD_ST)
			    || (s3->sflags & S_WORD_ST) == 0)
				s3 = s2;
#endif
#else
			/* have a 2nd choice if word starts on the main voice */
			if (!voice_tb[s2->voice].second
			    && voice_tb[s2->voice].staff == staff) {
				if ((s2->sflags & S_WORD_ST)
				    || s3 == 0
				    || (s3->sflags & S_WORD_ST) == 0)
					s3 = s2;
			}
#endif
		}
		s2 = last_chg;
		last_chg = s;

		/* if first change, see if any note before */
		if (s2 == 0) {
#if 1 /*fixme:test*/
			s4 = s3;
#else
			if ((s4 = s3) == 0)
				s4 = s;
#endif
			for (s4 = s4->ts_prev; s4 != 0; s4 = s4->ts_prev) {
				if (s4->staff != staff)
					continue;
				if (s4->type == NOTE)
					break;
			}

			/* if no note, change the clef of the staff */
			if (s4 == 0) {
				if (clef_type == TREBLE) {
					clef_type = BASS;
					staff_tb[staff].clef.line = 4;
				} else {
					clef_type = TREBLE;
					staff_tb[staff].clef.line = 2;
				}
				staff_tb[staff].clef.type = clef_type;
				continue;
			}
		}

		/* no change possible if no insert point */
#if 1 /*fixme:test*/
		    else if (s3->time == s2->time)
#else
		if (s3 == 0 || s3 == s2)
#endif
			continue;

		/* insert a clef change */
		clef_type = clef_type == TREBLE ? BASS : TREBLE;
		insert_clef(s3, clef_type);
	}
}

/* -- sort the symbols by time -- */
/* this function is called only once for the whole tune */
static void def_tssym(void)
{
	struct SYMBOL *s, *t, *prev_sym;
	int time, bars, seq, fl;
	struct VOICE_S *p_voice;

	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		p_voice->s_anc = 0;
		p_voice->selected = 0;
	}

	/* sort the symbol by time */
	prev_sym = 0;
	s = 0;		/* compiler warning */
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		s = p_voice->sym;
		s->ts_prev = prev_sym;
		if (prev_sym != 0)
			prev_sym->ts_next = s;
		prev_sym = s;
		p_voice->s_anc = s->next;
	}
	bars = 0;			/* (for errors) */
	for (;;) {

		/* search the closest next time/sequence */
		time = (unsigned) ~0 >> 1;		/* max int */
		seq = -1;
		for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
			if ((s = p_voice->s_anc) == 0
			    || s->time > time)
				continue;
			if (s->time < time) {
				time = s->time;
				seq = s->seq;
			} else if (s->seq < seq)
				seq = s->seq;
		}
		if (seq < 0)
			break;		/* echu (finished) */

		/* warn about incorrect number of notes / measures */
		fl = 0;
		for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
			if ((s = p_voice->s_anc) != 0
			    && s->time == time
			    && s->seq == seq) {
				p_voice->selected = 1;
				switch (s->type) {
				case BAR:
					if (s->as.u.bar.type != B_INVIS)
						fl |= 1;
					break;
				case MREST:
					fl |= 2;
					break;
				}
				
			} else	p_voice->selected = 0;
		}

		if (fl & 1) {		/* if any bar */
			int ko = 0;

			bars++;
			for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
				if ((s = p_voice->s_anc) == 0)
					continue;
				if (s->time != time) {	/* bad time */
					if (s->prev == 0
					    || s->prev->time + s->prev->len
						!=  s->time)
						continue;	/* normal time skip */
					error(1, s,
					      "Bad bar at measure %d for voice %s",
					      bars, p_voice->name);
					ko = 1;
					break;
				}
			}
			if (ko) {
				for (p_voice = first_voice;
				     p_voice;
				     p_voice = p_voice->next) {
					if ((t = p_voice->s_anc) == 0
					    || t->type != BAR)
						continue;
					time = s->time + s->len;
					for (; t != 0; t = t->next) {
						t->time = time;
						time += t->len;
					}
				}
				bars--;
				continue;
			}
		}
		if ((fl & 2)		/* if any mrest alone in a staff */
		    && first_voice->next != 0) {	/* and many voices */
			fl = 0;
			for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
				if (!p_voice->selected)
					continue;
				if ((s = p_voice->s_anc) == 0)
					continue;
				if (s->type != MREST) {
					fl = 1;
					break;
				}
			}
			if (fl) {	/* if not only multi measure rests */
				for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
					int i, dt, time2;

					if ((s = p_voice->s_anc) == 0
					    || s->type != MREST)
						continue;
					p_voice->last_symbol = s->prev;
					i = s->as.u.bar.len;
					dt = s->len / i;
					time2 = s->time;
					t = add_sym(p_voice, REST);
					t->as.type = ABC_T_REST;
					t->as.linenum = s->as.linenum;
					t->as.colnum = s->as.colnum;
					t->len = t->as.u.note.lens[0] = dt;
					t->head = H_FULL;
					t->nflags = -2;
					t->time = time2;
					if (s->as.u.bar.dc.n > 0)
						memcpy(&t->as.u.note.dc,
							&s->as.u.bar.dc,
							sizeof t->as.u.note.dc);
					p_voice->s_anc = t;
					time2 += dt;
					while (--i > 0) {
						t = add_sym(p_voice, BAR);
						t->as.type = ABC_T_BAR;
						t->as.u.bar.type = B_SINGLE;
						t->as.linenum = s->as.linenum;
						t->as.colnum = s->as.colnum;
						t->time = time2;
						t = add_sym(p_voice, REST);
						t->as.type = ABC_T_REST;
						t->as.linenum = s->as.linenum;
						t->as.colnum = s->as.colnum;
						t->len = t->as.u.note.lens[0] = dt;
						t->head = H_FULL;
						t->nflags = -2;
						t->time = time2;
						time2 += dt;
					}
					t = p_voice->last_symbol;
					if ((t->next = s->next) != 0)
						s->next->prev = t;
				}
			}
		}

		/* set the time linkage */
		for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
			if (!p_voice->selected)
				continue;
			s = p_voice->s_anc;
			s->ts_prev = prev_sym;
			prev_sym->ts_next = s;
			prev_sym = s;
			p_voice->s_anc = s->next;
		}
	}
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
		for (s = p_voice->sym; s != 0; s = s->next) {
			signed char up, down;

			if (s->type != NOTE) {
				if (staff_chg)
					s->staff++;
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
			for (s1 = s->ts_prev; s1 != 0; s1 = s1->ts_prev) {
				if (s1->staff != staff
				    || s1->voice == s->voice)
					break;
/*fixme:test again*/
if (s1->type == NOTE)
/*fixme:test*/
				if (/*s1->type == NOTE
				    &&*/ s1->pits[0] < up)
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
			for (s1 = s->ts_next; s1 != 0; s1 = s1->ts_next) {
				if (s1->staff != staff + 1
				    || s1->voice == s->voice)
					break;
/*fixme:test again*/
if (s1->type == NOTE)
/*fixme:test*/
				if (/*s1->type == NOTE
				    &&*/ s1->pits[s1->nhd] > down)
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

/* -- set the y values of some symbols -- */
static void set_yval(struct SYMBOL *s)
{
	switch (s->type) {
	case CLEF:
		if (voice_tb[s->voice].second
		    || s->as.u.clef.type < 0) {
#ifndef YSTEP
			s->dc_top = s->dc_bot =
#endif
			s->ymx = s->ymn = 12;
			break;
		}
		switch (s->as.u.clef.type) {
		default:			/* treble / perc */
			s->y = -2 * 6;
			s->ymx = 24 + 12;
			s->ymn = -12;
			break;
		case ALTO:
			s->y = -3 * 6;
			s->ymx = 24 + 5;
			s->ymn = -4;
			break;
		case BASS:
			s->y = -4 * 6;
			s->ymx = 24 + 5;
			s->ymn = -3;
			break;
		}
		s->y += s->as.u.clef.line * 6;
		if (s->y > 0)
			s->ymx += s->y;
		if (s->as.u.clef.octave > 0)
			s->ymx += 12;
		if (s->y < 0)
			s->ymn += s->y;
		if (s->as.u.clef.octave < 0)
			s->ymn -= 12;
#ifndef YSTEP
		s->dc_top = s->ymx;
		s->dc_bot = s->ymn;
#endif
		break;
	default:
		s->ymx = 24 + 2;
		s->ymn = -2;
#ifndef YSTEP
		s->dc_top = 24 + 2;
		s->dc_bot = -2;
#endif
		break;
	}
}

/* -- set the pitch of the notes according to the clefs -- */
/* also set the vertical offset of the symbols */
/* it supposes that the first symbol of each voice is the clef */
/* this function is called only once per tune */
static void set_pitch(void)
{
	struct SYMBOL *s;
	int staff;
	char staff_clef[MAXSTAFF];

	for (s = first_voice->sym; s != 0; s = s->ts_next) {
		struct SYMBOL *g;
		int delta, np, m, pav;

		staff = s->staff;
		switch (s->type) {
		case CLEF:
			set_yval(s);
			if (voice_tb[s->voice].second
			    || s->as.u.clef.type < 0)
				continue;
			switch (s->as.u.clef.type) {
			default:		/* treble / perc */
				delta = 0 - 2 * 2;
				break;
			case ALTO:
				delta = 6 - 3 * 2;
				break;
			case BASS:
				delta = 12 - 4 * 2;
				break;
			}
			staff_clef[staff] = delta + s->as.u.clef.line * 2;
			continue;
		default:
			set_yval(s);
			if ((g = s->grace) == 0)
				continue;
			delta = staff_clef[staff];
			for (; g != 0; g = g->next) {
				if (delta != 0) {
					for (m = g->nhd; m >= 0; m--)
						g->pits[m] += delta;
				}
				g->ymn = 3 * (g->pits[0] - 18) - 2;
				g->ymx = 3 * (g->pits[g->nhd] - 18) + 2;
			}
			continue;
		case MREST:
			s->ymx = 24 + 15;
			s->ymn = -2;
#ifndef YSTEP
			s->dc_top = 24 + 15;
			s->dc_bot = -2;
#endif
			continue;
		case REST:
			s->y = 12;
			s->ymx = 12 + 8;
			s->ymn = 12 - 8;
#ifndef YSTEP
			s->dc_top = 12 + 8;
			s->dc_bot = 12 - 8;
#endif
			continue;
		case NOTE:
			break;
		}
		np = s->nhd;
		delta = staff_clef[staff];
		if (delta != 0) {
			for (m = np; m >= 0; m--)
				s->pits[m] += delta;
		}
		pav = 0;
		for (m = np; m >= 0; m--)
			pav += s->pits[m];
		s->yav = 3 * pav / (np + 1) - 3 * 18;
		s->ymx = 3 * (s->pits[np] - 18) + 2;
		s->ymn = 3 * (s->pits[0] - 18) - 2;
#ifndef YSTEP
		s->dc_top = s->ymx;
		s->dc_bot = s->ymn;
#endif
	}
}

/* -- set the stem direction when multi-voices -- */
/* and adjust the vertical offset of the rests */
/* this function is called only once per tune */
static void set_multi(void)
{
	struct SYMBOL *s, *t;
	int i, j, staff, rvoice, voice;
	struct {
		int nvoice;
		struct {
			int voice;
			short ymn;
			short ymx;
		} st[4];		/* (no more than 4 voices per staff) */
	} stb[MAXSTAFF];
	struct {
		short range;
		signed char st1, st2;
	} vtb[MAXVOICE];
	struct VOICE_S *p_voice;

	for (p_voice = first_voice, rvoice = 0;
	     p_voice != 0;
	     p_voice = p_voice->next, rvoice++)
		vtb[p_voice - voice_tb].range = rvoice;

	s = first_voice->sym;
	while (s != 0) {
		for (staff = nstaff; staff >= 0; staff--) {
			stb[staff].nvoice = -1;
			for (i = 4; --i >= 0; ) {
				stb[staff].st[i].voice = -1;
				stb[staff].st[i].ymx = 0;
				stb[staff].st[i].ymn = 24;
			}
		}
		for (i = 0; i < MAXVOICE; i++)
			vtb[i].st1 = vtb[i].st2 = -1;

		/* go to the next bar and get the max/min offsets */
/*fixme: the stem height is not calculated yet*/
		for (t = s;
		     t != 0 && t->type != BAR;
		     t = t->ts_next) {
			if (t->len == 0		/* not a note or a rest */
			    || t->as.u.note.invis)
				continue;
			staff = t->staff;
			voice = t->voice;
			if (vtb[voice].st1 < 0)
				vtb[voice].st1 = staff;
			else if (vtb[voice].st1 == staff)
				;
			else {
				if (staff > vtb[voice].st1) {
					if (staff > vtb[voice].st2)
						vtb[voice].st2 = staff;
				} else {
					if (vtb[voice].st1 > vtb[voice].st2)
						vtb[voice].st2 = vtb[voice].st1;
					vtb[voice].st1 = staff;
				}
			}
			rvoice = vtb[voice].range;
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
						break;
					}
				}
				stb[staff].st[i].voice = rvoice;
			}
			if (t->type != NOTE)
				continue;
			if (t->ymx > stb[staff].st[i].ymx)
				stb[staff].st[i].ymx = t->ymx;
			if (t->ymn < stb[staff].st[i].ymn)
				stb[staff].st[i].ymn = t->ymn;
			if (t->sflags & S_XSTEM) {
				if (t->ts_prev->staff != staff - 1
				    || t->ts_prev->type != NOTE) {
					error(1, s, "Bad +xstem+");
					t->sflags &= ~S_XSTEM;
/*fixme:nflags KO*/
				} else {
					t->ts_prev->multi = 1;
					t->multi = 1;
					t->as.u.note.stemless = 1;
				}
			}
		}

		for ( ;
		     s != 0 && s->type != BAR;
		     s = s->ts_next) {
			int us, ls;

			if (s->len == 0)	/* if not note or rest */
				continue;
			staff = s->staff;
			voice = s->voice;
			if (s->multi == 0 && vtb[voice].st2 >= 0) {
				if (staff == vtb[voice].st1)
					s->multi = -1;
				else if (staff == vtb[voice].st2)
					s->multi = 1;
			}
			if (stb[staff].nvoice <= 0) { /* voice alone on the staff */
				if (s->multi != 0)
					continue;
/*fixme:could be done in set_float()*/
				p_voice = &voice_tb[voice];
				if (p_voice->floating) {
					if (s->staff == p_voice->staff)
						s->multi = -1;
					else	s->multi = 1;
				}
				continue;
			}
			rvoice = vtb[voice].range;
			for (i = stb[staff].nvoice; i >= 0; i--) {
				if (stb[staff].st[i].voice == rvoice)
					break;
			}
			if (i < 0)
				continue;		/* voice ignored */
			if (s->multi == 0) {
				if (i == stb[staff].nvoice)
					s->multi = -1;	/* last voice */
				else {
					s->multi = 1;	/* first voice(s) */

					/* if 3 voices, and vertical space enough,
					 * have stems down for the middle voice */
					if (i != 0
					    && i + 1 == stb[staff].nvoice) {
						if (stb[staff].st[i].ymn - cfmt.stemheight
						    > stb[staff].st[i + 1].ymx)
							s->multi = -1;

						/* special case for unisson */
						if (s->ts_prev->time == s->time
						    && s->ts_prev->staff == s->staff
						    && s->pits[s->nhd] == s->ts_prev->pits[0]
						    && (s->sflags & S_WORD_ST)
						    && s->as.u.note.word_end
						    && ((t = s->ts_next) == 0
							|| t->staff != s->staff
							|| t->time != s->time))
							s->multi = -1;
					}
				}
			}
			if (s->type != REST || s->len == 0
			    || s->as.u.note.invis)
				continue;

			/* set the rest vertical offset */
			/* (if visible and invisible rests on the same staff,
			 *  set as if 1 rest only) */
			us = rest_sp[C_XFLAGS - s->nflags].u;
			ls = rest_sp[C_XFLAGS - s->nflags].l;

			if (i == 0) {			/* first voice */
				int alone, ymx;

				alone = 1;
				for (t = s->ts_next; t != 0; t = t->ts_next) {
					if (t->staff != s->staff
					    || t->time != s->time)
						break;
					if (t->type != REST
					    || !t->as.u.note.invis) {
						alone = 0;
						break;
					}
				}
/*fix 4.12.20*/
				if (alone)
					continue;
				ymx = stb[staff].st[1].ymx;
				for (j = 2; j <= stb[staff].nvoice; j++) {
					if (ymx < stb[staff].st[j].ymx)
						ymx = stb[staff].st[j].ymx;
				}
				s->y = (ymx + ls) / 6 * 6;
				if (s->y < 12)
					s->y = 12;
			} else if (i == stb[staff].nvoice) { /* last voice */
				int ymn;

				ymn = stb[staff].st[i - 1].ymn;
				for (j = i - 2; j >= 0; j--) {
					if (ymn > stb[staff].st[j].ymn)
						ymn = stb[staff].st[j].ymn;
				}
				s->y = (ymn - us + 48) / 6 * 6 - 48;
				if (s->y > 12)
					s->y = 12;
			} else {		/* middle voice */
/*fixme: may be too high*/
				s->y = (stb[staff].st[i - 1].ymn
					+ stb[staff].st[i + 1].ymx)
					/ 12 * 6;
				if (((t = s->ts_next) != 0
				     && t->staff == staff
				     && t->time == s->time
				     && t->len != 0
				     && !t->as.u.note.invis
				     && t->ymx > s->y - ls)
				    || (s->ts_prev->staff == staff
					&& s->ts_prev->time == s->time
					&& s->ts_prev->len != 0
					&& !s->ts_prev->as.u.note.invis
					&& s->ts_prev->ymn < s->y + us))
					s->shhd[0] = 10;
					s->xmx = 10;
			}
			s->ymx = s->y + us;
			if (s->ymx > stb[staff].st[i].ymx)
				stb[staff].st[i].ymx = s->ymx;
			s->ymn = s->y - ls;
			if (s->ymn < stb[staff].st[i].ymn)
				stb[staff].st[i].ymn = s->ymn;
#ifndef YSTEP
			s->dc_top = s->ymx;
			s->dc_bot = s->ymn;
#endif
		}

		while (s != 0 && s->type == BAR)
			s = s->ts_next;
	}
}

/* -- set the staves and stems when multivoice -- */
/* this function is called only once per tune */
static void set_global(void)
{
	int staff;
	struct SYMBOL *s;
	struct VOICE_S *p_voice;

#ifndef CLEF_TRANSPOSE
	int old_behaviour, done;

	/* adjust the pitches if old abc2ps behaviour of clef definition */
	old_behaviour = done = 0;
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		int max, min;

		if (!p_voice->forced_clef
		    || p_voice->clef.type == PERC)
			continue;

		/* search if any pitch is too high for the clef */
		max = 100;
		min = -100;
		for (s = p_voice->sym; s != 0; s = s->next) {
			switch (s->type) {
			case CLEF:
				if (s->as.u.clef.type < 0)
					continue;
				if (!s->as.u.clef.check_pitch) {
					max = 100;
					min = -100;
					continue;
				}
				switch (s->as.u.clef.type) {
				case TREBLE:
				case PERC:
					max = 100;
					min = -100;
					break;
				case ALTO:
					max = 25;	/* e */
					min = 14;	/* G, */
					break;
				case BASS:
					max = 21;	/* A */
					min = 10;	/* C, */
					break;
				}
				continue;
			default:
				continue;
			case NOTE:
				if (s->pits[0] < min) {
					done = 1;
					break;		/* new behaviour */
				}
				if (s->pits[s->nhd] <= max)
					continue;
				old_behaviour = 1;
				done = 1;
				break;
			}
			break;
		}
		if (done)
			break;
	}
	if (old_behaviour) {
		for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
			int delta;

			if (!p_voice->forced_clef
			    || p_voice->clef.type == PERC)
				continue;
			delta = 0;
			for (s = p_voice->sym; s != 0; s = s->next) {
				struct SYMBOL *g;
				int i;

				switch (s->type) {
				case CLEF:
					if (s->as.u.clef.type < 0)
						continue;
					if (!s->as.u.clef.check_pitch)
						delta = 0;
					else switch (s->as.u.clef.type) {
						default: delta = 0; break;
						case ALTO: delta = -7; break;
						case BASS: delta = -14; break;
					}
				default:
					continue;
				case NOTE:
				case GRACE:
					if (delta == 0)
						continue;
					break;
				}
				if (s->type == NOTE) {
					for (i = s->nhd; i >= 0; i--)
						s->pits[i] += delta;
				} else {
					for (g = s->grace; g != 0; g = g->next) {
						for (i = g->nhd; i >= 0; i--)
							g->pits[i] += delta;
					}
				}
			}
		}
	}
#endif

	/* set a pitch for all symbols, the start/end of words
	 * and the sequence number of undrawn symbols */
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		int pitch, start_flag;
		struct SYMBOL *sym, *lastnote;

		sym = p_voice->sym;
		pitch = 22;				/* 'B' - if no note! */
		for (s = sym; s != 0; s = s->next) {
			if (s->type == NOTE) {
				pitch = s->pits[0];
				break;
			}
		}
		while (sym != s) {
			sym->pits[0] = pitch;
			sym = sym->next;
		}
		start_flag = 1;
		lastnote = 0;
		for (s = sym; s != 0; s = s->next) {
			switch (s->type) {
			default:
				if ((s->sflags & S_EOLN) == 0)
					break;
				/* fall thru */
			case BAR:
			case MREST:
			case MREP:
				if (lastnote != 0
				    && !(s->sflags & S_BEAM_ON)) {
					lastnote->as.u.note.word_end = 1;
					start_flag = 1;
					lastnote = 0;
				}
				if (s->type == BAR
				    && s->next == 0
				    && s->prev->type == NOTE
				    && s->prev->len >= BREVE)
					s->prev->head = H_SQUARE;
				break;
			case NOTE:
			case REST:
				if (s->sflags & S_TREM)
					break;
				if (s->nflags <= 0 && s->len > 0) {
					if (lastnote != 0) {
						lastnote->as.u.note.word_end = 1;
						lastnote = 0;
					}
					s->as.u.note.word_end = start_flag = 1;
					s->sflags |= S_WORD_ST;
				} else if (s->type == NOTE) {
					if (start_flag)
						s->sflags |= S_WORD_ST;
					if (s->sflags & S_EOLN)
						s->as.u.note.word_end = 1;
					start_flag = s->as.u.note.word_end;
					lastnote = s;
				} else if (s->as.u.note.word_end
					   || (s->sflags & S_EOLN)) {
					if (lastnote != 0) {
						lastnote->as.u.note.word_end = 1;
						lastnote = 0;
					}
					s->as.u.note.word_end = 0;
					start_flag = 1;
				}
				break;
			}
			if (s->type == NOTE) {
				pitch = s->pits[0];
				if (s->prev->type != NOTE) {
					s->prev->pits[0] = (s->prev->pits[0]
							    + pitch) / 2;
				}
			} else	s->pits[0] = pitch;
		}
		if (lastnote != 0)
			lastnote->as.u.note.word_end = 1;
	}

	/* sort the symbols by time */
	def_tssym();

	/* align the undrawn symbols with the previous ones */
	for (s = first_voice->sym; s != 0; s = s->ts_next) {
		struct SYMBOL *s2;
		int seq;

		s2 = 0;
		switch (s->type) {
		case FMTCHG:
			if (s->u == STBRK || s->xmx != 0)
				goto setsq;
		case TEMPO:
		case PART:
		case STAVES:
		case TUPLET:
		case WHISTLE:
			if (s2 == 0)
				s2 = s;
			break;
		default:
setsq:
			if (s2 == 0)
				break;
			seq = s->seq;
			do {
				s2->seq = seq;
				s2 = s2->ts_next;
			} while (s2 != s);
			s2 = 0;
			break;
		}
	}

	/* set the staff of the floating voices */
	set_float();

	/* set the clefs */
	if (cfmt.autoclef) {
		for (p_voice = first_voice; p_voice; p_voice = p_voice->next)
			if (p_voice->forced_clef)
				staff_tb[p_voice->staff].forced_clef = 1;
		for (staff = 0; staff <= nstaff; staff++) {
			if (!staff_tb[staff].forced_clef)
				set_clef(staff);
		}
	}

	/* set the starting clefs and adjust the note pitches */
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next)
		memcpy(&p_voice->sym->as.u.clef,
		       &staff_tb[p_voice->staff].clef,
		       sizeof p_voice->sym->as.u.clef);
	set_pitch();
}

/* -- return the left indentation of the staves -- */
static float set_indent(int first_line)
{
	int staff;
	float w, maxw;
	struct VOICE_S *p_voice;
	char *p, *q;

	maxw = 0;
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		p = first_line ? p_voice->nm : p_voice->snm;
		if (p == 0)
			continue;
		str_font(&cfmt.voicefont);
		for (;;) {
			if ((q = strstr(p, "\\n")) != 0)
				*q = '\0';
			w = tex_str(p);
			if (w > maxw)
				maxw = w;
			if (q == 0)
				break;
			*q = '\\';
			p = q + 2;
		}
	}

	/* when no name, indent the first line if requested */
	if (maxw == 0)
		return first_line ? cfmt.indent : 0;

	maxw += 4 * cwid(' ') * cfmt.voicefont.swfac;
	for (staff = 0; staff <= nstaff; staff++) {
		if (staff_tb[staff].brace
		    || staff_tb[staff].bracket) {
			maxw += 10;
			break;
		}
	}
	return maxw;
}

/* -- set the y offset of the staves and return the whole height -- */
static float set_staff(void)
{
	struct SYMBOL *s;
	int staff, any_part, any_tempo;
	float y, staffsep, dy, maxsep, mbot, scale;
	struct {
		float mtop;
#ifdef YSTEP
		int not_empty;
#else
		float x;
		float ctop;
		float cbot;
#endif
	} delta_tb[MAXSTAFF], *p_delta;

	/* compute the distance between the staves */
	memset(delta_tb, 0, sizeof delta_tb);
	mbot = 0;
#ifdef YSTEP
	for (s = first_voice->sym; s != 0; s = s->ts_next) {
		switch (s->type) {
		case CLEF:
		case KEYSIG:
		case TIMESIG:
		case TEMPO:
		case PART:
		case STAVES:
		case TUPLET:
		case FMTCHG:
		case WHISTLE:
			continue;
		}
		delta_tb[s->staff].not_empty = 1;
	}
/*fixme: should handle the staff scales*/
	{
		int i;
		float v;

		if (delta_tb[0].not_empty) {
			p_delta = &delta_tb[0];
			for (i = 0; i < YSTEP; i++) {
				v = staff_tb[0].top[i];
				if (p_delta->mtop < v)
					p_delta->mtop = v;
			}
		} else	staff_tb[0].empty = 1;
		for (staff = 1; staff <= nstaff; staff++) {
			if (!delta_tb[staff].not_empty) {
				staff_tb[staff].empty = 1;
				continue;
			}
			p_delta = &delta_tb[staff];
			for (i = 0; i < YSTEP; i++) {
				v = staff_tb[staff].top[i] - staff_tb[staff - 1].bot[i];
				if (p_delta->mtop < v)
					p_delta->mtop = v;
			}
		}
		for (staff = nstaff; staff >= 0; staff--) {
			if (!staff_tb[staff].empty) {
				p_delta = &delta_tb[staff];
				for (i = 0; i < YSTEP; i++) {
					v = staff_tb[staff].bot[i];
					if (mbot > v)
						mbot = v;
				}
				break;
			}
		}
	}

	any_part = any_tempo = 0;
	for (s = first_voice->sym->next; s != 0; s = s->next) {
		switch (s->type) {
		case PART:
			any_part = 1;
			break;
		case TEMPO:
			any_tempo = 1;
			break;
		default:
			continue;
		}
		if (any_part && any_tempo)
			break;
	}
#else
	any_part = any_tempo = 0;
	for (s = first_voice->sym->next;	/* skip the clefs */
	     s != 0; s = s->ts_next) {
/*fixme: bad! better have a flag in the voice*/
		if (s->voice == first_voice - voice_tb) {
			switch (s->type) {
			case PART:
				any_part = 1;
				continue;
			case TEMPO: 
				any_tempo = 1;
				continue;
			}
		}
		switch (s->type) {
		case KEYSIG:		/* ?? */
		case TIMESIG:		/* ?? */
		case TEMPO:
		case PART:
		case STAVES:
		case TUPLET:
		case FMTCHG:
		case WHISTLE:
			continue;
		}
		staff = s->staff;
		p_delta = &delta_tb[staff];
		if (s->x > p_delta->x) {
			p_delta->x = s->x + 8;
			p_delta->ctop = s->dc_top;
			p_delta->cbot = s->dc_bot;
		} else {
			if (s->dc_top > p_delta->ctop)
				p_delta->ctop = s->dc_top;
			if (s->dc_bot < p_delta->cbot)
				p_delta->cbot = s->dc_bot;
		}

		/* adjust above the staff */
		if (staff == 0) {
			if (p_delta->mtop < p_delta->ctop)
				p_delta->mtop = p_delta->ctop;
		} else {
			dy = p_delta->ctop - p_delta[-1].cbot;
			if (p_delta->mtop < dy)
				p_delta->mtop = dy;
		}

		/* adjust below the last staff */
		if (staff == nstaff) {
			if (mbot > p_delta->cbot)
				mbot = p_delta->cbot;
		}
	}
#endif

	/* draw the parts and tempo indications if any */
	if (any_part || any_tempo) {
		dy = delta_tb[0].mtop * staff_tb[0].clef.staffscale;
		if (dy == 0)		/* first staff not displayed */
			dy = 24 + 14;
		dy = draw_partempo(dy, any_part, any_tempo);
	} else	dy = 0;

	/* set the staff offsets */
	staffsep = cfmt.staffsep * 0.5 + 24;
	maxsep = cfmt.maxstaffsep * 0.5 + 24;
	y = 0;
	for (staff = 0, p_delta = delta_tb;
	     staff <= nstaff;
	     staff++, p_delta++) {
		scale = staff_tb[staff].clef.staffscale;
		dy += p_delta->mtop * scale;
#ifdef YSTEP
		if (staff_tb[staff].empty)
			;
#else
		if (p_delta->mtop == 0)
			staff_tb[staff].empty = 1;
#endif
		else if (dy < staffsep)
			dy = staffsep;
		else if (dy > maxsep)
			dy = maxsep;
		y += dy;
		staff_tb[staff].y = -y;
		PUT2("/y%d{%.1f add}def\n", staff, -y);
		if (scale != 1) {
			PUT3("/scst%d{gsave 0 %.2f translate %.2f dup scale}def\n",
			     staff, -y, scale);
		}
		if (staff == 0) {
			staffsep = cfmt.sysstaffsep + 24;
			maxsep = cfmt.maxsysstaffsep + 24;
		}
		dy = 0;
	}
	if (mbot == 0) {
		for (staff = nstaff; staff >= 0; staff--) {
			if (!staff_tb[staff].empty)
				break;
		}
		if (staff < 0)		/* no symbol in this system */
			return y;
	}
	dy = -(mbot * staff_tb[nstaff].clef.staffscale);
	staffsep = cfmt.staffsep * 0.5;
	if (dy < staffsep)
		dy = staffsep;
	maxsep = cfmt.maxstaffsep * 0.5 + 24;
	if (dy > maxsep)
		dy = maxsep;
	y += dy;
	if (y > cfmt.maxstaffsep)
		y = cfmt.maxstaffsep;
	return y;
}

/* -- decide on beams and on stem directions -- */
/* this routine is called only once per tune */
static void set_beams(struct SYMBOL *sym)
{
	struct SYMBOL *s, *t, *prev_note;
	int beam, laststem, lasty;
#if 0
	int bar_time = 0;
#endif

	beam = 0;
	laststem = -1;
	lasty = 0;
	prev_note = 0;
	for (s = sym; s != 0; s = s->next) {
		if (s->type != NOTE) {
			struct SYMBOL *g;

			if ((g = s->grace) != 0) {
				if (s->stem == 0
				    && (s->stem = s->multi) == 0)
					s->stem = 1;
				for (; g != 0; g = g->next)
					g->multi = g->stem = s->stem;
			}
#if 0
			  else if (s->type == BAR)
				bar_time = s->time;
#endif
			continue;
		}

#if 0
		/* set beam breaks on demi-semi-quaver sequences */
		if (beam && cfmt.halfbeam
		    && s->len <= SEMIQUAVER / 2
		    && prev_note->len <= SEMIQUAVER / 2
		    && (s->time - bar_time) % QUAVER == 0)
			prev_note->sflags |= S_BEAM_BREAK;
#endif

		if (s->stem == 0		/* if not explicitly set */
		    && (s->stem = s->multi) == 0) { /* and alone on the staff */

			/* notes in a beam have the same stem direction */
			if (beam)
				s->stem = laststem;
			else if ((s->sflags & S_WORD_ST)
				   && !s->as.u.note.word_end) { /* start of beam */
				int avg, n;

				avg = s->yav;
				n = 12;
				for (t = s->next; t != 0; t = t->next) {
					if (t->type == NOTE) {
						if (t->multi != 0) {
							avg = n - t->multi;
							break;
						}
						avg += t->yav;
						n += 12;
					}
					if (t->as.u.note.word_end)
						break;
				}
				if (avg < n)
					laststem = 1;
				else if (avg > n || cfmt.bstemdown)
					laststem = -1;
				beam = 1;
				s->stem = laststem;
			} else {
				s->stem = s->yav >= 12 ? -1 : 1;
				if (s->yav == 12
				    && !cfmt.bstemdown) {
					int dy;

					dy = s->yav - lasty;
					if (dy > -7 && dy < 7)
						s->stem = laststem;
				}
			}
		} else {			/* stem set by set_multi */
			if ((s->sflags & S_WORD_ST)
			    && !s->as.u.note.word_end)	/* start of beam */
				beam = 1;
		}
		if (s->as.u.note.word_end)
			beam = 0;
		laststem = s->stem;
		lasty = s->yav;
		prev_note = s;
	}
}

/* -- set the x offset of the grace notes -- */
static float set_graceoffs(struct SYMBOL *s)
{
	struct SYMBOL *g, *next;
	int m;
	float xx;

	xx = 0;
	g = s->grace;
	g->sflags |= S_WORD_ST;
	for ( ; ; g = g->next) {
		set_head_directions(g);
		for (m = g->nhd; m >= 0; m--) {
			if (g->as.u.note.accs[m]) {
				xx += 4;
				if (g->as.u.note.accs[m] & 0xf8)
					xx += 2;	/*4.12.22*/
				break;
			}
		}
		g->x = xx;

		if (g->nflags <= 0) {
			g->sflags |= S_WORD_ST;
			g->as.u.note.word_end = 1;
		}
		next = g->next;
		if (next == 0) {
			g->as.u.note.word_end = 1;
			break;
		}
		if (next->nflags <= 0)
			g->as.u.note.word_end = 1;
		if (g->as.u.note.word_end) {
			next->sflags |= S_WORD_ST;
			xx += GSPACE / 4;
		}
		if (g->nflags <= 0)
			xx += GSPACE / 4;
		if (g->y > next->y + 8)
			xx -= 1.6;
		xx += GSPACE;
	}

	/* return the whole width */
	return xx;
}

/* -- shift the notes when voices overlap -- */
/* this routine is called only once per tune */
static void set_overlap(void)
{
	struct SYMBOL *s, *s1, *s2;
	int d, m, nhd2, sd1, sd2;
	float d1, d2, dy1, dy2, noteshift;

/*fixme: the accidentals are not fully treated.. */
/*fixme: problems when stems are inverted*/
	for (s = first_voice->sym; s != 0; s = s->ts_next) {
		if (s->type != NOTE
		    || s->as.u.note.invis)
			continue;

		if ((s2 = s->ts_next) == 0)
			break;
		if (s->staff != s2->staff
		    || s->time != s2->time)
			continue;
		if (s2->type != NOTE) {
			if (s2->type != REST)
				continue;
			if ((s2 = s2->ts_next) == 0)
				break;
			if (s2->type != NOTE
			    || s->staff != s2->staff
			    || s->time != s2->time)
				continue;
		}
		if (s2->as.u.note.invis)
			continue;

		nhd2 = s2->nhd;

		/* align the accidentals when bigger than SEMIBREVE */
		if (s->head >= H_OVAL) {
			if (s2->head < H_OVAL)
				for (m = nhd2; m >= 0; m--)
					s2->shac[m] += 3;
		} else {
			if (s2->head >= H_OVAL)
				for (m = s->nhd; m >= 0; m--)
					s->shac[m] += 3;
		}

		if (s2->multi < 0)		/* if lowest voice */
			s2->doty = -3;		/* shift down the dots */
		sd1 = sd2 = 0;
		d1 = d2 = dy1 = 0;
		d = s->pits[0] - s2->pits[nhd2];
		s1 = s;

		/* shift the accidentals */
/*fixme: not finished... */
		if (d != 0 && d >= -5 && d <= 5
		    && s1->as.u.note.accs[0]
		    && s2->as.u.note.accs[nhd2]) {
			noteshift = (d <= -4 || d >= 4) ? 4.5 : 7;
			if (s1->as.u.note.accs[0] & 0xf8)
				noteshift += 2;		/*4.12.23*/
			if (s2->shac[nhd2] < s1->shac[0] + noteshift)
				s2->shac[nhd2] = s1->shac[0] + noteshift;
		}

		/* if there may be a stem clash, force a shift */
		if (d != 0 && (s1->stem < 0 || s2->stem >= 0)) {
			if (s1->ymn > s2->ymx
			    || s1->ymx < s2->ymn)
				continue;
			if (d > 0 || s1->stem != s2->stem) {
				s1 = s2;
				s2 = s;
				nhd2 = s2->nhd;
				d = -d;
			}
		}
		switch (d) {
		case 0: {			/* unisson */
			int l1, l2;

			/* special case when different accidentals */
			if (s1->as.u.note.accs[0] != s2->as.u.note.accs[nhd2]) {
				d2 = 9;
				if (s2->as.u.note.accs[nhd2] != 0) {
					d2 += 8;
					if (s2->as.u.note.accs[nhd2] & 0xf8)
						d2 += 2;	/*4.12.23*/
				}
				if (s1->dots)
					d2 += 6;
				for (m = nhd2; m >= 0; m--) {
					s2->shhd[m] += d2;
					s2->shac[m] -= d2;
				}
				s2->xmx += d2;
				s2->doty = -3;
				continue;
			}
			if ((l1 = s1->len) >= SEMIBREVE)
				break;
			if ((l2 = s2->len) >= SEMIBREVE)
				break;
			if (cfmt.shifthnote
			    && (l1 >= MINIM || l2 >= MINIM))
				break;
			if (s1->stem == s2->stem
			    || s1->as.u.note.stemless
			    || s2->as.u.note.stemless)
				break;
			s2->sflags |= S_NO_HEAD;	/* same head */
			s2->as.u.note.accs[nhd2] = 0;
/*			d2 = s1->shhd[0]; */
			dy2 = 0;
			if (l1 == l2)
				goto do_shift;		/* no shift */
			if (l1 < l2) {
				l1 = l2;
				l2 = s1->len;
			}
			if (l1 < MINIM) {
				if (s1->stem < 0) {
					if (s1->dots > 0)
						dy1 = -3;
				} else if (s2->dots > 0) {
					s2->sflags &= ~S_NO_HEAD;
					s1->sflags |= S_NO_HEAD;
					s2->as.u.note.accs[nhd2] =
						s1->as.u.note.accs[0];
					s1->as.u.note.accs[0] = 0;
					dy2 = -3;
				}
				goto do_shift;
			}
			if (l2 < CROTCHET) {	/* (l1 >= MINIM) */
				if (s2->len >= MINIM) {
					s2->sflags &= ~S_NO_HEAD;
					s1->sflags |= S_NO_HEAD;
					s2->as.u.note.accs[nhd2] =
						s1->as.u.note.accs[0];
					s1->as.u.note.accs[0] = 0;
					dy2 = -3;
				}
				goto do_shift;
			}
			s2->sflags &= ~S_NO_HEAD;
			break;
		    }
		case 1:
			break;
		default:
			if ((s1->head == H_SQUARE
			     || s2->head == H_SQUARE)
			    && (d >= -3 && d <= 3))
				break;
			if (d > 0)
				continue;
			if (d < -1
			    && s1->head == H_OVAL
			    && s2->head == H_OVAL)
				continue;
			break;
		}
		if (s1->len >= BREVE
		    || s2->len >= BREVE)
			noteshift = 13;
		else if (s1->len >= SEMIBREVE
			 || s2->len >= SEMIBREVE)
			noteshift = 10;
		else	noteshift = 7.8;
/*fixme: treat the accidentals*/
/*fixme: treat the chord shifts*/
/*fixme: treat the previous shifts (3 voices or more)*/
		/* the dot of the 2nd voice should be lower */
		dy2 = -3;

		/* if the 1st voice is below the 2nd one,
		 * and there is no chord overlap,
		 * shift the 1st voice */
		if (d < 0) {
			if (s1->stem > 0 && s1->pits[s1->nhd] >= s2->pits[0]) {
				d2 = noteshift + 2; /* have room for the stem */
				goto do_shift;
			}
			d1 = noteshift;
			if (d > -7
			    && s2->len < CROTCHET
			    && (s2->sflags & S_WORD_ST)
			    && s2->as.u.note.word_end) {
/*fixme: have higher stem??*/
				if (s2->stem >= 0)
					d1 += 1.5; /* have room for the flag */
			} else if (s1->pits[s1->nhd] < s2->pits[0] - 1)
				d1 *= 0.5;
/*fixme: check if overlap in chord*/

			/* and shift the dot of the 2nd voice if any */
			if (s2->dots > 0)
				sd2 = 1;

			/* the dot of the 1st voice must be lower */
			dy1 = -3;
			dy2 = 0;

		/* if the upper note is SEMIBREVE or higher, shift it */
		} else if (s1->len >= SEMIBREVE
			   && s1->len > s2->len)
			d1 = noteshift;

		/* else shift the 2nd voice */
		else {
			d2 = noteshift;
			if (s1->dots > 0
			    && (d != 1 || (s1->pits[0] & 1)))
				sd1 = 1;	/* and the dot of the 1st voice */
		}

		/* do the shift, and update the width */
	do_shift:
		if (s1->stem > 0) {
			d1 -= s1->shhd[0];
			d2 += s1->shhd[0];
		} else {
			d1 -= s1->shhd[s1->nhd];
			d2 += s1->shhd[s1->nhd];
		}
		if (s2->stem > 0) {
			d1 += s2->shhd[0];
			d2 -= s2->shhd[0];
		} else {
			d1 += s2->shhd[nhd2];
			d2 -= s2->shhd[nhd2];
		}
		if (d1 > 0) {
/*fix 4.12.25*/
			if (s2->dots && sd2 == 0)	/* room for the dots */
				d1 += 8 + 3.5 * (s2->dots - 1);
			for (m = s1->nhd; m >= 0; m--)
				s1->shhd[m] += d1;
			if (sd2 != 0)
				s2->xmx = s1->xmx + d1;
			s1->xmx += d1;
		}
		if (d2 > 0) {
/*fix 4.12.25*/
			if (s1->dots && sd1 == 0)	/* room for the dots */
				d2 += 8 + 3.5 * (s1->dots - 1);
			for (m = nhd2; m >= 0; m--) {
				s2->shhd[m] += d2;
				if (s2->as.u.note.accs[m] != 0
				    && s2->pits[m] < s1->pits[0] - 4)
					s2->shac[m] -= d2;
			}
			if (sd1 != 0)
				s1->xmx = s2->xmx + d2;
			s2->xmx += d2;
		}
		s1->doty = dy1;
		s2->doty = dy2;
	}
}

/* -- set the stem lengths -- */
/* this routine is called only once per tune */
static void set_stems(void)
{
	struct SYMBOL *s, *g;
	float slen, scale;
	int ymn, ymx;

	for (s = first_voice->sym; s != 0; s = s->ts_next) {
		if (s->type != NOTE) {
			int ymin, ymax;

			if ((g = s->grace) == 0)
				continue;
			ymin = ymax = 12;
			for (; g != 0; g = g->next) {
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
#ifndef YSTEP
				g->dc_top = ymx;
				g->dc_bot = ymn;
#endif
			}
			s->ymx = ymax;
			s->ymn = ymin;
#ifndef YSTEP
			s->dc_top = s->ymx;
			s->dc_bot = s->ymn;
#endif
			continue;
		}

		/* shift notes in chords (need stem direction to do this) */
		set_head_directions(s);

		/* set height of stem end, without considering beaming for now */
		slen = cfmt.stemheight;
		switch (s->nflags) {
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
		} else	ymx = ymn;
		if (s->as.u.note.stemless) {
			if (s->stem >= 0) {
				s->y = ymn;
				s->ys = (float) ymx;
			} else {
				s->ys = (float) ymn;
				s->y = ymx;
			}
			if (s->nflags == -4)	/* if longa */
				ymn -= 6;
			s->ymx = ymx + 4;
			s->ymn = ymn - 4;
#ifndef YSTEP
			s->dc_top = s->ymx;
			s->dc_bot = s->ymn;
#endif
		} else if (s->stem >= 0) {
			if (s->nflags >= 2)
				slen -= 1;
			if (s->pits[s->nhd] > 26
			    && (s->nflags <= 0
				|| !((s->sflags & S_WORD_ST)
				     && s->as.u.note.word_end))) {
				slen -= 2;
				if (s->pits[s->nhd] > 28)
					slen -= 2;
			}
			s->y = ymn;
			if (s->as.u.note.ti1[0] != 0)
/*fixme
 *			    || s->as.u.note.ti2[0] != 0) */
				ymn -= 3;
			s->ymn = ymn - 4;
#ifndef YSTEP
			s->dc_bot = s->ymn;
#endif
			s->ys = ymx + slen;
			if (s->ys < 12)
				s->ys = 12;
			s->ymx = (int) (s->ys + 2.5);
#ifndef YSTEP
			s->dc_top = s->ys + 2;
#endif
		} else {			/* stem down */
			if (s->pits[0] < 18
			    && (s->nflags <= 0
				|| !((s->sflags & S_WORD_ST)
				     && s->as.u.note.word_end))) {
				slen -= 2;
				if (s->pits[0] < 16)
					slen -= 2;
			}
			s->ys = ymn - slen;
			if (s->ys > 12)
				s->ys = 12;
			s->ymn = (int) (s->ys - 2.5);
#ifndef YSTEP
			s->dc_bot = s->ys - 2;
#endif
			s->y = ymx;
			if (s->as.u.note.ti1[s->nhd] != 0)
/*fixme
 *			    || s->as.u.note.ti2[s->nhd] != 0)*/
				ymx += 3;
			s->ymx = ymx + 4;
#ifndef YSTEP
			s->dc_top = s->ymx;
#endif
		}
	}
}

/* -- set width and space of a symbol -- */
/* This routine sets the minimal left and right widths wl,wr
 * so that successive symbols are still separated when
 * no extra glue is put between them. It also sets the prefered
 * spacings pl,pr for good output. */
static void set_width(struct SYMBOL *s)
{
	struct SYMBOL *s2, *k;
	int i, m;
	float xx, w, wlnote, wlw;

	switch (s->type) {
	case NOTE:
	case REST:

		/* set the note widths */
		switch (s->head) {
		case H_SQUARE:
			wlnote = 8;
			break;
		case H_OVAL:
			wlnote = 6;
			break;
		case H_EMPTY:
			wlnote = 5;
			break;
		default:
			wlnote = 4.5;
			break;
		}
		s->wr = wlnote;

		/* room for shifted heads and accidental signs */
		if (s->xmx > 0)
			s->wr += s->xmx + 4;
		s2 = s->prev;
		while (s2->type == TUPLET)
			s2 = s2->prev;
		if (s2->type == BAR)
			wlnote += 3;
		for (m = 0; m <= s->nhd; m++) {
			xx = s->shhd[m];
			if (xx < 0)
				AT_LEAST(wlnote, -xx + 5);
			if (s->as.u.note.accs[m]) {	/* 4.12.22*/
				AT_LEAST(wlnote, s->shac[m]
					 + ((s->as.u.note.accs[m] & 0xf8)
					    ? 6.5 : 4.5));
			}
		}
		if (s2->type == BAR)
			wlnote -= 3;

		/* room for the decorations */
		if (s->as.u.note.dc.n > 0)
			wlnote += deco_width(s);

		/* space for flag if stem goes up on standalone note */
		if ((s->sflags & S_WORD_ST)
		    && s->as.u.note.word_end
		    && s->stem > 0 && s->nflags > 0)
			AT_LEAST(s->wr, s->xmx + 12);

		/* leave room for dots and set their offset */
		if (s->dots > 0) {

			/* standalone with up-stem and flags */
			if (s->nflags > 0 && s->stem > 0
			    && s->xmx == 0 && s->doty == 0
			    && (s->sflags & S_WORD_ST)
			    && s->as.u.note.word_end
			    && !(s->y % 6))
				s->xmx = DOTSHIFT;
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

		wlw = wlnote;

		/* extra space when up stem - down stem */
		if (s2->type == NOTE) {
			if (s2->stem > 0 && s->stem < 0)
				AT_LEAST(wlw, 7);

			/* make sure helper lines don't overlap */
			if ((s->y > 27 && s2->y > 27)
			    || (s->y < -3 && s2->y < -3))
				AT_LEAST(wlw, 6);
/* 4.12.22*/
			/* have ties wide enough */
			if (s2->sflags & S_TI1)
				AT_LEAST(wlw, 14);
		}

		/* leave room for guitar chord */
		/* !! this sequence is tied to draw_gchord() !! */
		if (s->as.text != 0) {
			float lspc, rspc;
			char *p, *q, sep, antype;

			str_font(&cfmt.font_tb[s->gcf]);
			lspc = rspc = cwid(' ') * cfmt.font_tb[s->gcf].swfac;
			p = s->as.text;
			antype = '\0';
			sep = '\n';
			for (;;) {
				if (sep == '\n') {
					if (*p != '\0' && strchr("^_<>@", *p) != 0)
						antype = *p++;
					else	antype = '\0';
				}
				for (q = p; ; q++) {
					if (*q == '\\') {
						q++;
						if (*q == '\\' || *q == ';')
							continue;
					}
					if (*q == '\0' || *q == ';' || *q == '\n') {
						sep = *q;
						*q = '\0';
						break;
					}
				}
				w = tex_str(p);
				switch (antype) {
				case '^':		/* above */
				case '_':		/* below */
				default: {		/* default = above */
					float wl;

					wl = w * GCHPRE;
					if (wl > 8)
						wl = 8;
					if (wl > lspc)
						lspc = wl;
					w -= wl;
					if (w > rspc)
						rspc = w;
					break;
				    }
				case '<':		/* left */
					w += wlnote;
					if (w > lspc)
						lspc = w;
					break;
				case '>':		/* right */
					w += s->wr;
					if (w > rspc)
						rspc = w;
					break;
				case '@':		/* absolute */
					break;
				}
				if (sep == '\0')
					break;
				*q = sep;
				p = q + 1;
			}
/*fixme: pb when '<' only*/
			if (s2->as.text != 0)
				AT_LEAST(wlw, lspc);
/*fixme: pb when '>' only*/
			for (k = s->next; k != 0; k = k->next) {
				switch (k->type) {
				default:
					continue;
				case NOTE:
				case REST:
				case BAR:
					if (k->as.text != 0)
						AT_LEAST(s->wr, rspc);
					break;
				}
				break;
			}
		}

		/* leave room for vocals under note */
		/* related to draw_lyrics() */
		if (s->ly) {
			struct lyrics *ly = s->ly;
			struct lyl *lyl;
			float align = 0;

			for (i = 0; i < MAXLY; i++) {
				float swfac, shift;
				char *p;

				if ((lyl = ly->lyl[i]) == 0)
					continue;
				p = lyl->t;
				w = lyl->w;
				swfac = lyl->f->swfac;
				xx = w + 2 * cwid(' ') * swfac;
				if (isdigit((unsigned char) *p)
				    || p[1] == ':'
				    || p[1] == '(' || p[1] == ')') {
					float sz;

					if (p[1] == '(')
						sz = cwid((unsigned char) p[1]);
					else {
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
				} else if (*p == '\x02' || *p == '\x03')
					shift = 0;
				else {
					shift = xx * VOCPRE;
					if (shift > 20)
						shift = 20;
				}
				lyl->s = shift;
				AT_LEAST(wlw, shift);
				xx -= shift;
				shift = 2 * cwid(' ') * swfac;
				for (k = s->next; k != 0; k = k -> next) {
					switch (k->type) {
					case NOTE:
					case REST:
						if (k->ly == 0
						    || k->ly->lyl[i] == 0)
							xx -= 9;
						else if (k->ly->lyl[i]->t[0] == '\x02'
							 || k->ly->lyl[i]->t[0] == '\x03')
							xx -= shift;
						else	break;
						if (xx <= 0)
							break;
						continue;
					case CLEF:
					case TIMESIG:
					case KEYSIG:
						xx -= 10;
						continue;
					case TEMPO:
					case PART:
					case TUPLET:
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
		}

		/* set the natural space */
		if (s->len != 0) {
			if (!(s->sflags & S_TREM))
				i = s->nflags;
			else {
				switch (s->head) {
				case H_SQUARE: i = -3; break;
				case H_OVAL: i = -2; break;
				case H_EMPTY: i = -1; break;
				default: i = 0; break;
				}
			}
			xx = space_tb[C_XFLAGS - i];
			if (s->dots)
				xx *= dot_space;
			s->pl = (1 - bnnp) * xx;
#if 0
			if (s->xmx > 0)
				xx *= 1.2;
#endif
			s->pr = bnnp * xx;
		} else {			/* space ('y') */
			if (s->as.u.note.lens[1] < 0)
				xx = 10;
			else	xx = (float) (s->as.u.note.lens[1] / 2);
			s->wl = s->wr = xx;
			s->pl = s->pr = xx * 1.4;
		}

		/* reduce right space when not followed by a note */
		for (k = s->next; k != 0; k = k->next) {
			switch (k->type) {
			case PART:
			case TEMPO:
				continue;
			default:
				s->pr *= 0.8;
				break;
			case NOTE:
			case REST:
			case TUPLET:
				break;
			}
			break;
		}

		/* squeeze notes a bit if big jump in pitch */
		if (s->type == NOTE
		    && s2->type == NOTE) {
			int dy;
			float fac;

			dy = s->y - s2->y;
			if (dy < 0)
				dy =- dy;
			fac = 1. - 0.01 * dy;
			if (fac < 0.9)
				fac = 0.9;
			s2->pr *= fac;

			/* stretch / shrink when opposite stem directions */
			if (s2->stem > 0 && s->stem < 0)
				s2->pr *= 1.1;
			else if (s2->stem < 0 && s->stem > 0)
				s2->pr *= 0.9;
		}

		/* if preceeded by a grace note sequence, adjust */
		if (s2->type == GRACE) {
			s->wl = wlnote - 4.5;
			s->pl = 0;
		} else	s->wl = wlw;
		break;
	case BAR:
		{
			int bar_type;

			w = 5;
			bar_type = s->as.u.bar.type;
			switch (bar_type) {
			case B_OBRA:
			case (B_OBRA << 4) + B_CBRA:
				w = 0;		/* invisible */
				break;
			case (B_BAR << 4) + B_COL:
			case (B_COL << 4) + B_BAR:
				w += 3 + 3 + 5;
				break;
			case (B_COL << 4) + B_COL:
				w += 5 + 3 + 3 + 3 + 5;
				break;
			default:
				for (;;) {
					switch (bar_type & 0x0f) {
					case B_OBRA:
					case B_CBRA:
						w += 3;
						break;
					case B_COL:
						w += 2;
					}
					bar_type >>= 4;
					if (bar_type == 0)
						break;
					w += 3;
				}
				break;
			}
		}
		if (w != 0) {
			s->wl = w;
			s->pl = w + 5;
			if (s->next != 0
			    && s->next->type != TIMESIG
			    && s->next->type != KEYSIG) {
				s->wr = 6;
				s->pr = 8;
			} else {
				s->wr = 5;
				s->pr = 5.5;
			}
			s->shhd[0] = (w - 5) * -0.5;
		}
		if (s->as.u.bar.dc.n > 0)
			s->wl += deco_width(s);

		/* have room for the repeat numbers / guitar chord */
		if (s->as.text == 0)
			break;
		{
			struct FONTSPEC *f;

			f = s->as.u.bar.repeat_bar ?
				&cfmt.repeatfont : &cfmt.font_tb[s->gcf];
			str_font(f);
			xx = tex_str(s->as.text)
				+ cwid(' ') * f->swfac * 1.5;
		}
		if (!s->as.u.bar.repeat_bar) {
			if (s->prev->as.text != 0) {
				float spc;

				spc = xx * GCHPRE;
				if (spc > 8)
					spc = 8;
				AT_LEAST(s->wl, spc);
				s->pl = s->wl;
				xx -= spc;
			}
		}
		for (s2 = s->next; s2 != 0; s2 = s2->next) {
			switch (s2->type) {
			case PART:
			case TEMPO:
			case TUPLET:
			case GRACE:
				continue;
			case NOTE:
			case REST:
				if (s2->as.text != 0) {
					AT_LEAST(s->wr, xx);
					s->pr = s->wr;
				}
				break;
			default:
				break;
			}
			break;
		}
		break;
	case CLEF:
		if (s->as.u.clef.type < 0)
			break;
		if (!s->as.u.clef.invis) {
			s->wl = 12;
			s->wr = s->u ? 10 : 16;
		} else if (!s->u) {
			s->wl = 6;
			s->wr = 6;
		}
		break;
	case KEYSIG: {
		int n1, n2;
		int esp = 4;

		if (s->as.u.key.nacc == 0) {
			n1 = s->as.u.key.sf;	/* new key sig */
			n2 = s->u;		/* old key */
			if (n1 * n2 > 0) {
				if (n1 < 0 /*|| n2 < 0*/) {
					n1 = -n1;
					n2 = -n2;
				}
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

			n1 = s->as.u.key.nacc;
			last_acc = s->as.u.key.accs[0];
			for (i = 1; i < n1; i++) {
				if (s->as.u.key.accs[i] != last_acc) {
					last_acc = s->as.u.key.accs[i];
					esp += 3;
				}
			}
		}
		s->wl = 3;
		if (n1 > 0) {
			s->wr = (float) (5 * n1 + esp);
		} else if (s->next != 0 && s->next->type != TIMESIG)
			s->wr = 3;
		break;
	}
	case TIMESIG:
		/* !!tied to draw_timesig()!! */
		w = 2;
		for (i = 0; i < s->as.u.meter.nmeter; i++) {
			int l;

			l = strlen(s->as.u.meter.meter[i].top);
			if (l > sizeof s->as.u.meter.meter[i].top)
				l = sizeof s->as.u.meter.meter[i].top;
			if (s->as.u.meter.meter[i].bot[0] != '\0') {
				int l2;

				l2 = strlen(s->as.u.meter.meter[i].bot);
				if (l2 > sizeof s->as.u.meter.meter[i].bot)
					l2 = sizeof s->as.u.meter.meter[i].bot;
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
		s->pl = s->wl + 16;
		s->pr = s->wr + 16;
		break;
	case MREP:
		if (s->as.u.bar.len == 1) {
			s->wr = s->wl = 16 / 2 + 8;
			s->pr = s->pl = s->wr + 8;
		} else	{
			s2 = s->prev->prev;	/* invisible rest (see parse) */
			s->wl = s2->wl;
			s->wr = s2->wr;
			s->pl = s2->pl;
			s->pr = s2->pr;
		}
		break;
	case GRACE:
		s->wl = set_graceoffs(s) + GSPACE * 0.8;
		s->prev->pr -= s->wl - 10;
		w = GSPACE0;
		if ((s2 = s->next) != 0
		    && s2->type == NOTE) {
			struct SYMBOL *g;

			g = s->grace;
			while (g->next != 0)
				g = g->next;
			if (g->y >= (float) (3 * (s2->pits[s2->nhd] - 18)))
				w -= 1;	/* above, a bit closer */
			else if ((g->sflags & S_WORD_ST)
				 && g->y < (float) (3 * (s2->pits[0] - 18) - 7))
				w += 2;	/* below with flag, a bit further */
		}
		s->wr = w;
		break;
	case FMTCHG:
		if (s->u != STBRK || (s->wl = s->xmx) == 0)
			break;		/* no space */
		if (s->next == 0 || s->next->type != CLEF)
			s->wr = 8;
		else {
			s->wr = 2;
			s->next->u = 0;	/* big clef */
		}
		s->pl = s->wl + 4;
		s->pr = s->wr + 4;
		break;
	case TEMPO:
	case PART:
	case TUPLET:		/* no space */
	case WHISTLE:
		break;
	default:
		bug("Cannot set width for symbol", 1);
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
		if ((s = s->prev) == 0)
			return;
	}
	if (s->type != BAR)
		return;

	if (s->as.u.bar.repeat_bar) {
		p_voice->bar_start = B_INVIS;
		p_voice->bar_text = s->as.text;
		p_voice->bar_repeat = 1;
		s->as.text = 0;
		s->as.u.bar.repeat_bar = 0;
	}
	bar_type = s->as.u.bar.type;
	if (bar_type == B_COL)			/* ':' */
		return;
	if ((bar_type & 0x0f) != B_COL)		/* if not left repeat bar */
		return;
	if (!(s->sflags & S_RRBAR)) {		/* 'xx:' (not ':xx:') */
		p_voice->bar_start = bar_type;
		if (s->prev != 0 && s->prev->type == BAR) {
			s->prev->next = 0;
			if (s->ts_prev != 0)
				s->ts_prev->ts_next = s->ts_next;
			if (s->ts_next != 0)
				s->ts_next->ts_prev = s->ts_prev;
		} else	s->as.u.bar.type = B_BAR;
		return;
	}
	if (bar_type == B_DREP) {		/* '::' */
		s->as.u.bar.type = B_RREP;
		p_voice->bar_start = B_LREP;
		return;
	}
	for (i = 0; bar_type != 0; i++)
		bar_type >>= 4;
	bar_type = s->as.u.bar.type;
	s->as.u.bar.type = bar_type >> ((i / 2) * 4);
	i = ((i + 1) / 2 * 4);
	p_voice->bar_start = bar_type & ((1 << i) - 1);
}

/* -- set the end of a piece of tune -- */
/* tsnext becomes the beginning of the next line */
static void set_piece(struct SYMBOL *s)
{
	struct VOICE_S *p_voice;
	int type;

	/* if last line, do nothing */
	if ((tsnext = s->ts_next) == 0)
		return;

	/* if clef key or time signature change on the next line,
	 * put them at the end of the current line */
	for (;;) {
		type = tsnext->type;
		switch (type) {
		case TIMESIG:
			if (!cfmt.timewarn)
				break;
		case CLEF:
		case KEYSIG:
			for (s = tsnext; s->ts_next != 0; s = s->ts_next)
				if (s->ts_next->type != type)
					break;
			if ((tsnext = s->ts_next) == 0)
				return;
			if (type == TIMESIG)
				insert_meter = 1;
			continue;
		case FMTCHG:		/* remove the staffbreaks */
			if (tsnext->u != STBRK)
				break;
			if (tsnext->next != 0)
				tsnext->next->prev = tsnext->prev;
			if (tsnext->prev != 0)
				tsnext->prev->next = tsnext->next;
			if ((tsnext = tsnext->ts_next) == 0)
				return;
			tsnext->ts_prev = s;
			if (tsnext->type == FMTCHG)
				continue;
			break;
		}
		break;
	}
	s->ts_next = 0;

	/* set end of voices */
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		int voice;

		voice = p_voice - voice_tb;
		for (s = tsnext->ts_prev; s != 0; s = s->ts_prev) {
			struct SYMBOL *s2;

			if (s->voice != voice)
				continue;

			/* set the word end / start */
			for (s2 = s; s2 != 0; s2 = s2->prev) {
				if (s2->type == NOTE) {
					s2->as.u.note.word_end = 1;
					break;
				}
				if (s2->type == BAR)
					break;
			}
			for (s2 = s->next; s2 != 0; s2 = s2->next) {
				if (s2->type == NOTE) {
					s2->sflags |= S_WORD_ST;
					break;
				}
				if (s2->type == BAR)
					break;
			}
			s->next = 0;
			check_bar(s);
			break;
		}
	}
}

/* -- set the horizontal offsets -- */
static void set_xoffset(struct SYMBOL *s,
			struct SYMBOL *last)
{
	struct SYMBOL *prev, *s3;
	struct VOICE_S *p_voice;
	float shrink, space, stretch;
	/* whistle space at start of line !! see /tw_head !! */
	int i;
	static unsigned char pitwh[12] =
		{28, 54, 28, 54, 28, 28, 54, 28, 54, 28, 54, 28};

	set_width(s);

	prev = s->prev;
	if (prev->time == s->time && prev->seq == s->seq) {
		s->wr = prev->wr;	/* same space as previous symbol */
		s->pr = prev->pr;
		s->x = prev->x;
		s->shrink = prev->shrink;
		s->stretch = prev->stretch;
		return;
	}
	shrink = prev->wr + s->wl;
	space = prev->pr + s->pl;
	stretch = 0;

	switch (s->type) {
	case CLEF:
		if (prev->len == 0)
			break;
		space = prev->pl - 5;
		break;
	case NOTE:
	case REST:
		if (prev->type == GRACE)
			break;
		if (prev->type == BAR)
			space *= 1.5;
		if ((s->sflags & S_WORD_ST) == 0) /* reduce spacing within a beam */
			space *= fnnp;
		if (s->sflags & S_IN_TUPLET)	/* reduce spacing in n-plet */
			space *= gnnp;
		stretch = s->pl;
		break;
	case BAR:
	case MREST:
	case MREP:
		stretch = s->pl * 0.4;
		break;
	}
	s->shrink = prev->shrink + shrink;
	s->x = prev->x + space;

	switch (prev->type) {
	case NOTE:
	case REST:
	case GRACE:
		if (last->type != GRACE)
			stretch += prev->pr;
		break;
	case BAR:
	case MREST:
	case MREP:
		stretch += prev->pr * 0.4;
		break;
	case WHISTLE:
		for (i = 0; i < nwhistle; i++)
			if (whistle_tb[i].voice == s->voice)
				break;
		shrink = pitwh[whistle_tb[i].pitch % 12];
		if (s->shrink < shrink)
			s->shrink = shrink;
		break;
	}
	s->stretch = stretch;

	/* if the duration of the previous symbol is greater than
	   the duration of the last symbol, adjust */
	if (prev->time != last->time) {
		if (s->len != 0 || s->type == TUPLET) {
			int len;

			if ((len = prev->len) == 0)	/* (start of line) */
				len = s->time - prev->time;
#if 1
			s->x = last->x
				+ (space - 5) * (s->time - last->time) / len
				+ 5;
#else
			if ((s->sflags & S_IN_TUPLET)
			    || (last->sflags & S_IN_TUPLET))
				s->x = last->x
					+ (space - 5) * (s->time - last->time) / len
					+ 5;
			else {
				int head, dots, nflags;
				identify_note(s, len, &head, &dots, &nflags);
				s->x = last->x + space_tb[C_XFLAGS - nflags];
/*fixme: add the dots*/
			}
#endif
/*fixme: should check if at the same height*/
#if 1
			shrink = last->shrink + 5;
#else
			shrink = last->shrink + s->wl;
#endif
			if (s->shrink < shrink)
				s->shrink = shrink;
		}
	}

	/* shift if clash with a previous symbol */
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		s3 = p_voice->s_anc;
		if (s3->staff != s->staff || s3->voice == s->voice)
			continue;
		if (s->shrink < s3->shrink + s3->wr + s->wl
		    && (s3->type != NOTE
			|| !s3->as.u.note.invis)) {
			shrink = s3->shrink;
			if (s3->ly != 0
			    || (s->ymn <= s3->ymx
				&& s->ymx >= s3->ymn))
				shrink += s3->wr + s->wl;
			else if (s3->wr > s->wl)
				shrink += s3->wr;
			else	shrink += s->wl;
			if (s->shrink < shrink)
				s->shrink = shrink;
		}
	}
}

/* -- Set the characteristics of the glue between symbols, then
 *	position the symbols along the staff. If staff is overfull,
 *	only does symbols which fit in. -- */
static void set_sym_glue(float width)
{
	float alfa0, beta0, alfa, beta;
	int voice, time, seq, some_grace;
	float space, shrink, stretch;
	float new_shrink, new_space, new_stretch;
	struct SYMBOL *s;

	alfa0 = ALFA_X;			/* max shrink and stretch */
	beta0 = BETA_X;
	if (cfmt.continueall) {
		alfa0 = cfmt.maxshrink;
		beta0 = BETA_C;
	}

	/* set the offsets of the first symbols (clefs - one for each voice) */
	s = first_voice->sym;
	time = s->time;
	seq = s->seq;
	while (s->time == time
	       && s->seq == seq) {
		set_width(s);
		s->x = s->shrink = s->stretch = s->wl;
		voice_tb[s->voice].s_anc = s;
		if ((s = s->ts_next) == 0)
			return;		/* no symbol */
	}
	space = shrink = stretch
		= new_space = new_shrink
		= s->ts_prev->x;

	/* then loop over the symbols */
	for (;;) {
		struct SYMBOL *s2, *s3, *s4;
		struct VOICE_S *p_voice;

		/* get the notes at this time, set spacing
		 * and get the min shrinking */
		new_stretch = 0;
		time = s->time;
		seq = s->seq;
		s4 = s;
		s2 = s;
		for (;;) {

			/* set the x offsets of the symbol */
			set_xoffset(s2, s->ts_prev);

			/* keep the symbol with larger space */
			if (s2->shrink > new_shrink)
				new_shrink = s2->shrink;
			if (s2->x > new_space)
				new_space = s2->x;
			if (s2->stretch > new_stretch)
				new_stretch = s2->stretch;

			if ((s2 = s2->ts_next) == 0
			    || s2->time != time || s2->seq != seq)
				break;
		}

		/* make sure that space >= shrink */
		if (new_space < space + new_shrink - shrink)
			new_space = space + new_shrink - shrink;

		/* set the horizontal offsets */
		stretch += new_space - space + new_stretch;
		shrink = new_shrink;
		space = new_space;

		/* adjust spacing and advance */
		s4 = s;				/* (for overfull) */
		for ( ; s != s2; s = s->ts_next) {
			s->x = space;
			s->shrink = shrink;
			s->stretch = stretch;
			voice_tb[s->voice].s_anc = s;

			/* remove some double bars */
			if (s->next != 0
			    && s->next->type == BAR
			    && s->next->next != 0
			    && s->next->next->type == BAR
			    && !s->next->next->as.u.bar.repeat_bar
			    && (s->next->as.text == 0
				|| s->next->next->as.text == 0)
			    && (s->next->as.u.bar.dc.n == 0
				|| s->next->next->as.u.bar.dc.n == 0)) {
				s3 = 0;
				if ((s->next->as.u.bar.type == B_SINGLE
				     || s->next->as.u.bar.type == B_DOUBLE)
				    && (s->next->next->as.u.bar.type & 0xf0)) {
					s3 = s->next->next;
					if (s->next->as.u.bar.dc.n != 0)
						memcpy(&s3->as.u.bar.dc,
							&s->next->as.u.bar.dc,
							sizeof s3->as.u.bar.dc);
					memcpy(&s->next->as.u.bar,
						&s3->as.u.bar,
						sizeof s->next->as.u.bar);
					if (s3->as.text != 0)
						s->next->as.text = s3->as.text;
				}
#if 0
				if ((s->next->as.u.bar.type & 0xf0)
				    && (s->next->next->as.u.bar.type == B_SINGLE
					|| s->next->next->as.u.bar.type == B_DOUBLE))
					s3 = s->next->next;
#endif
				if (s3 != 0) {
					if (s3 == s2)
						s2 = s3->ts_next;
					delsym(s3);
				}
			}
		}

		if (s == 0)
			break;

		/* check the total width */
		if (cfmt.continueall) {
			if (space <= width)
				continue;
			if ((space - width) / (space - shrink) < alfa0)
				continue;
		} else if (shrink < width)
			continue;

		/* may have a clef, key/time sig or staffbreak at EOL */
		switch (s->type) {
		case FMTCHG:
			if (s->u != STBRK)
				break;
		case CLEF:
		case KEYSIG:
		case TIMESIG:
			continue;
		}
		s = s4->ts_prev;
		if (!cfmt.continueall)
			error(0, s, "Line overfull");

		/* go back to the previous bar, if any */
		for (s2 = s; s2 != 0; s2 = s2->ts_prev) {
			if ((s2->type == BAR
			     && s2->as.u.bar.type != B_INVIS)
#if 1
			    )
#else
			    || s2->type == KEYSIG)
#endif
				break;
		}

		/* (should have some note) */
		if (s2 != 0
		    && s2->time > first_voice->sym->time)
			s = s2;
		else if (s->time <= first_voice->sym->time) {
			error(1, s, "No symbol in the music line");
			break;
		}

		/* don't cut in a grace note sequence if followed by note */
		if (s->type == GRACE
		    && s->next != 0
		    && s->next->type == NOTE)
			s = s->prev;

		/* restore the linkages */
		if (tsnext != 0) {
			tsnext->ts_prev->ts_next = tsnext;
			for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
				voice = p_voice - voice_tb;
				for (s2 = tsnext; s2 != 0; s2 = s2->ts_next) {
					if (s2->voice == voice) {
						if (s2->prev != 0)
							s2->prev->next = s2;
						break;
					}
				}
			}
		}
		set_piece(s);
		break;
	}

	/* search the last symbol and check if any grace note */
	some_grace = 0;
	for (s = first_voice->sym; s->ts_next != 0; s = s->ts_next)
		if (s->type == GRACE)
			some_grace = 1;

	/* get the total space from the last effective symbol */
	while (s->type == TEMPO
	       || s->type == STAVES)
		s = s->ts_prev;
	space = s->x;
	stretch = s->stretch;
	shrink = s->shrink;

	/* if the last symbol is not a bar, add some extra space */
	if (s->type != BAR) {
		if (s->pr < s->wr)
			s->pr = s->wr;
		shrink += s->wr + 3;
		space += s->pr + 5;
		stretch += (s->pr + 5) * 1.4;
	}

	/* set the glue, calculate final symbol positions */
	alfa = beta = 0;
	if (space >= width) {
		alfa = (space - width) / (space - shrink);
#if 0
		if (alfa > alfa0)
			alfa = alfa0;
#endif
	} else {
		beta = (width - space) / (stretch - space);
		if (beta > beta0) {
			if (!cfmt.continueall) {
				error(0, s,
				      "Line underfull (%.0fpt of %.0fpt)",
					beta0 * stretch + (1 - beta0) * space,
					width);
			}
			if (!cfmt.stretchstaff)
				beta = 0;
		}
		if (!cfmt.stretchlast
		    && tsnext == 0	/* if last line of tune */
		    && beta >= beta_last) {
			alfa = alfa_last; /* shrink underfull last line same as previous */
			beta = beta_last;
		}
	}
	alfa_last = alfa;
	beta_last = beta;
	realwidth = alfa * shrink + beta * stretch + (1 - alfa - beta) * space;
	if (alfa != 0) {
		if (alfa <= 1) {
			for (s = first_voice->sym; s != 0; s = s->ts_next)
				s->x = alfa * s->shrink + (1 - alfa) * s->x;
		} else {
#if 0
			error(0, s,
			      "Line too much shrunk (%.0fpt of %.0fpt)",
				shrink, width);
#endif
			alfa = width / shrink;
			for (s = first_voice->sym; s != 0; s = s->ts_next)
				s->x = alfa * s->shrink;
			realwidth = width;
		}
	} else {
		for (s = first_voice->sym; s != 0; s = s->ts_next)
			s->x = beta * s->stretch + (1 - beta) * s->x;
	}

	/* set the x offsets of the grace notes */
	if (some_grace) {
		for (s = first_voice->sym; s != 0; s = s->ts_next) {
			struct SYMBOL *g;

			if ((g = s->grace) == 0)
				continue;
			for ( ; g->next != 0; g = g->next)
				;
			space = s->x - g->x;
			for (g = s->grace; g != 0; g = g->next)
				g->x += space;
		}
	}
}

/* -- find one line to output -- */
static void find_piece(void)
{
	struct SYMBOL *s;
	int number, time, seq, i, voice;

	if (!cfmt.continueall) {
		voice = first_voice - voice_tb;
		if ((number = cfmt.barsperstaff) == 0) {

			/* find the first end-of-line */
			for (s = first_voice->sym; /*s != 0*/; s = s->ts_next) {
				if (s->sflags & S_EOLN && s->voice == voice)
					break;
				if (s->ts_next == 0) {
					/* when '\' at end of line and 'P:' */
/*					bug("no eoln in piece", 0); */
					break;
				}
			}
		} else {

			/* count the measures */
			for (s = first_voice->sym; s->ts_next != 0; s = s->ts_next)
				if (s->len > 0)		/* if note or rest */
					break;
			for ( ; s->ts_next != 0; s = s->ts_next) {
				if (s->type != BAR
				    || s->as.u.bar.type == B_INVIS)
					continue;
				if (s->prev->type == BAR)
					continue;
				if (s->voice == voice
				    && --number <= 0)
					break;
			}
		}

		/* cut at the last symbol of the sequence */
		if (s->len > 0) {	/* note or rest: cut on end time */
			time = s->time + s->len;
			for (; s->ts_next != 0; s = s->ts_next)
				if (s->ts_next->time >= time)
					break;
		} else {		/* other symbol: cut at end of sequence */
			seq = s->seq;
			for (; s->ts_next != 0; s = s->ts_next)
				if (s->ts_next->seq != seq)
					break;
		}
		set_piece(s);
	} else	tsnext = 0;

	for (i = nstaff; i >= 0; i--) {
		staff_tb[i].empty = 0;
		staff_tb[i].y = 0;
	}
}

/* -- init symbol list with clef, meter, key -- */
static void init_music_line(struct VOICE_S *p_voice)
{
	struct SYMBOL *s, *sym;

	sym = p_voice->sym;
	p_voice->sym = 0;

	/* output the first postscript sequences */
	if (sym != 0) {
		while (sym->type == FMTCHG
		       && sym->u == PSSEQ) {
			PUT1("%s\n", sym->as.text);
			if ((sym = delsym(sym)) == 0)
				break;
		}
	}

	/* add clef */
	if (sym != 0 && sym->type == CLEF
	    && !p_voice->second && p_voice->staff == sym->staff) {
		int stafflines;
		float staffscale;

		if ((stafflines = sym->as.u.clef.stafflines) < 0)
			stafflines = staff_tb[p_voice->staff].clef.stafflines;
		if ((staffscale = sym->as.u.clef.staffscale) == 0)
			staffscale = staff_tb[p_voice->staff].clef.staffscale;
		if (sym->as.u.clef.type >= 0)
			memcpy(&staff_tb[p_voice->staff].clef,
				&sym->as.u.clef,
				sizeof sym->as.u.clef);
		staff_tb[p_voice->staff].clef.stafflines = stafflines;
		staff_tb[p_voice->staff].clef.staffscale = staffscale;
		sym = delsym(sym);
	}
	s = add_sym(p_voice, CLEF);
	s->seq = 7;			/*4.12.20*/
	memcpy(&p_voice->clef, &staff_tb[p_voice->staff].clef,
	       sizeof p_voice->clef);
	memcpy(&s->as.u.clef, &p_voice->clef, sizeof s->as.u.clef);

	if (!p_voice->second) {

		/* add keysig */
		if (sym != 0 && sym->type == KEYSIG) {
			memcpy(&p_voice->key, &sym->as.u.key,
				 sizeof sym->as.u.key);
			sym = delsym(sym);
		}
		if (p_voice->key.sf != 0 || p_voice->key.nacc != 0) {
			s = add_sym(p_voice, KEYSIG);
			memcpy(&s->as.u.key, &p_voice->key, sizeof s->as.u.key);
			if (s->as.u.key.bagpipe && s->as.u.key.sf == 2)	/* K:Hp */
				s->u = 3;			/* --> G natural */
		}

		/* add time signature if needed */
		if (insert_meter
		    && p_voice->meter.nmeter != 0) {	/* != M:none */
			s = add_sym(p_voice, TIMESIG);
			memcpy(&s->as.u.meter, &p_voice->meter,
			       sizeof s->as.u.meter);
		}

		/* add tempo if any */
		if (info.tempo) {
			s = info.tempo;
			memset((&s->as) + 1, 0,
			       sizeof (struct SYMBOL) - sizeof (struct abcsym));
			p_voice->last_symbol->next = s;
			s->prev = p_voice->last_symbol;
			p_voice->last_symbol = s;
			s->voice = p_voice - voice_tb;
			s->staff = p_voice->staff;
			s->type = TEMPO;
#if 0 /*fix 4.12.19*/
			s->seq = s->prev->seq;	/* same x offset as prev sym */
#else
			s->seq = s->prev->seq + 1;	/*??*/
#endif
			info.tempo = 0;
		}

		/* add bar if needed */
		if (p_voice->bar_start != 0) {
			int i;

			i = 4;
			if (p_voice->bar_text == 0	/* if repeat continuation */
			    && p_voice->bar_start == B_OBRA) {
				for (s = sym; s != 0; s = s->next) {	/* search the end of repeat */
					if (s->type == BAR) {
						if ((s->as.u.bar.type & 0xf0)	/* if complex bar */
						    || s->as.u.bar.type == B_CBRA
						    || s->as.u.bar.repeat_bar)
							break;
						if (--i < 0)
							break;
					}
				}
				if (s == 0 || sym == 0)
					i = -1;
				if (i >= 0 && sym->time == s->time)
					i = -1;		/* no note */
			}
			if (i >= 0) {
				s = add_sym(p_voice, BAR);
				s->as.u.bar.type = p_voice->bar_start;
				s->as.text = p_voice->bar_text;
				s->as.u.bar.repeat_bar = p_voice->bar_repeat;
			}
			p_voice->bar_start = 0;
			p_voice->bar_repeat = 0;
			p_voice->bar_text = 0;
		}
	}
/*fixme: should be before the first note*/
	if (p_voice->whistle)
		add_sym(p_voice, WHISTLE);

	s = p_voice->last_symbol;
	if ((s->next = sym) != 0)
		sym->prev = s;
}

/* -- initialize a new line -- */
static void cut_symbols(void)
{
	struct VOICE_S *p_voice;
	struct SYMBOL *s, *s1;

	clrarena(3);

	/* set start of voices */
	if ((s1 = tsnext) == 0)
		return;
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		int voice;

		p_voice->sym = 0;		/* may have no symbol */
		voice = p_voice - voice_tb;
		for (s = s1; s != 0; s = s->ts_next) {
			if (s->voice == voice) {
				p_voice->sym = s;
				s->prev = 0;
				break;
			}
		}
	}

	/* add the first symbols of the line */
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		p_voice->time = s1->time;
		init_music_line(p_voice);

	/* insert the new symbols into the time sorted list */
		p_voice->s_anc = p_voice->sym;
	}
	insert_meter = 0;

	s = 0;
	for (;;) {
		int done;

		done = 1;
		for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
			s1 = p_voice->s_anc;
			if (s1 == 0 || s1->ts_prev != 0)
				continue;
			done = 0;		/* new symbol */
			if (s == 0)
				first_voice->sym = s1;
			else	s->ts_next = s1;
			s1->ts_prev = s;
			s = s1;
			set_yval(s);
			p_voice->s_anc = s->next;
		}
		if (done)
			break;
	}
	s->ts_next = tsnext;
	if (tsnext != 0)
		tsnext->ts_prev = s;
}

/* -- output for parsed symbol list -- */
void output_music(void)
{
	struct VOICE_S *p_voice;
	int voice, first_line;
	float lwidth;

	for (p_voice = first_voice; p_voice; p_voice = p_voice->next)
		if (p_voice->sym != 0)
			break;
	if (p_voice == 0)
		return;		/* no symbol at all */

	lvlarena(2);

	voice_dup();	/* duplicate the voices appearing in many staves */

	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		p_voice->time = 0;
		p_voice->cstaff = p_voice->staff;
		if (!p_voice->second)
			memcpy(&staff_tb[ p_voice->staff].clef,
				&p_voice->clef,
				sizeof p_voice->clef);
		init_music_line(p_voice);
	}
	first_line = insert_meter;
	insert_meter = 0;

	alfa_last = 0.1;
	beta_last = 0;

	check_buffer();	/* dump buffer if not enough space for a music line */

	set_global();			/* set global characteristics */
	if (first_voice->next != 0) {	/* if many voices */
		if (cfmt.combinevoices)
			combine_voices();
		set_multi();		/* set the stems direction in 'multi' */
	}
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next)
		set_beams(p_voice->sym);	/* decide on beams */
	set_stems();			/* set the stem lengths */
	if (first_voice->next != 0)	/* when multi-voices */
		set_overlap();		/* shift the notes on voice overlap */

	clrarena(3);
	lvlarena(3);
	lwidth = ((cfmt.landscape ? cfmt.pageheight : cfmt.pagewidth)
		- cfmt.leftmargin - cfmt.rightmargin)
			/ cfmt.scale;
	if (lwidth < 1 CM) {
		error(1, 0, "Bad page width %.1f", lwidth);
		lwidth = 20 CM;
	}
	for (;;) {		/* loop over pieces of line for output */
		float indent, line_height;

		find_piece();

		indent = set_indent(first_line);
		set_sym_glue(lwidth - indent);
		if (indent != 0)
			PUT1("%.2f 0 T\n", indent); /* do indentation */
		PUT0("/dlsym{\n");
		draw_sym_near();
		PUT0("}def\n");
		line_height = set_staff();
		draw_vname(first_line, indent);
		PUT0("dlsym\n");
		for (p_voice = first_voice; p_voice; p_voice = p_voice->next)
			draw_symbols(p_voice);
		draw_all_deco();
		bskip(line_height);
		if (nwhistle != 0)
			draw_whistle();
		if (indent != 0)
			PUT1("%.2f 0 T\n", -indent);
		buffer_eob();

		cut_symbols();
		if (tsnext == 0)
			break;
		first_line = 0;
	}
	lvlarena(2);

	/* reset the parser */
	for (voice = MAXVOICE; --voice >= 0; ) {
		voice_tb[voice].sym = 0;
		voice_tb[voice].time = 0;
		voice_tb[voice].have_ly = 0;
	}
}

/* -- reset the generator -- */
void reset_gen(void)
{
	insert_meter = 1;
}
