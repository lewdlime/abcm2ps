/*
 * Music generator.
 *
 * This file is part of abcm2ps.
 *
 * Copyright (C) 1998-2014 Jean-François Moine
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

#include <string.h>
#include <ctype.h>

#include "abc2ps.h"

struct SYMBOL *tsnext;		/* next line when cut */
float realwidth;		/* real staff width while generating */

static int insert_meter;	/* insert time signature (1) and indent 1st line (2) */
static float alfa_last, beta_last;	/* for last short short line.. */

#define AT_LEAST(a,b)  do { float tmp = b; if(a<tmp) a=tmp; } while (0)

/* width of notes indexed by log2(note_length) */
float space_tb[NFLAGS_SZ] = {
	7, 10, 14.15, 20, 28.3,
	40,				/* crotchet */
	56.6, 80, 113, 150
};
static int smallest_duration;

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
	n = s->nhd;
	if (n == 0) {
		if (s->as.u.note.accs[0] != 0) {
			dx = dx_tb[s->head];
			if (s->as.flags & ABC_F_GRACE)
				dx *= 0.7;
			s->shac[0] = dx;
		}
		return;
	}

	/* set the head shifts */
	dx = dx_tb[s->head] * 0.78;
	if (s->as.flags & ABC_F_GRACE)
		dx *= 0.5;
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
		if (d == 0 && shift) {		/* unison on shifted note */
			s->shhd[i] = s->shhd[i - sig] + dx;
			continue;
		}
		if (d < 0)
			d = -d;
		if (d > 3 || (d >= 2 && s->head < H_SQUARE)) {
			shift = 0;
		} else {
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
		if (s->as.flags & ABC_F_GRACE)
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
			if (s->as.u.note.accs[i1] & 0xf8) { /* microtonal */
				shmin = 6.5;
				shmax = 9;
			} else {
				shmin = 4.5;
				shmax = 7;
			}
			if (s->as.flags & ABC_F_GRACE) {
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
						if (dx1 > dx3 - shmin && dx1
									< dx3 + shmin)
							dx1 = dx3 + shmin;
					} else {
						if (dx1 > dx3 - shmax && dx1
									< dx3 + shmax)
							dx1 = dx3 + shmax;
					}
				}
			}
			if (p2 - p1 >= 4
			 && (ac_tb[i2] != A_SH || ac_tb[i1] != A_SH)) {
				if (dx1 > dx2 - shmin && dx1 < dx2 + shmin) {
					if (dx1 + shmin < dx2 + shmin)
						s->shac[i2] = dx1 + shmin;
					else
						dx1 = dx2 + shmin;
				}
			} else {
				if (dx1 > dx2 - shmax && dx1 < dx2 + shmax) {
					if (dx1 + shmax < dx2 + shmax)
						s->shac[i2] = dx1 + shmax;
					else
						dx1 = dx2 + shmax;
				}
			}
		}
		s->shac[i1] = dx1;
		i2 = i1;
		p2 = p1;
		dx2 = dx1;
	}
}

/* -- unlink a symbol -- */
void unlksym(struct SYMBOL *s)
{
	if (!s->next) {
		if (s->extra) {
			s->type = FMTCHG;
			s->u = -1;
			return;
		}
	} else {
		s->next->prev = s->prev;
		if (s->extra) {
			struct SYMBOL *g;

			g = s->next->extra;
			if (!g) {
				s->next->extra = s->extra;
			} else {
				for (; g->next; g = g->next)
					;
				g->next = s->extra;
			}
		}
	}
	if (s->prev)
		s->prev->next = s->next;
	else
		voice_tb[s->voice].sym = s->next;
	if (s->ts_next) {
		if ((s->sflags & S_SEQST)
		 && !(s->ts_next->sflags & S_SEQST)) {
			s->ts_next->sflags |= S_SEQST;
			s->ts_next->shrink = s->shrink;
			s->ts_next->space = s->space;
		}
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
	if (cfmt.combinevoices <= 0
	 && s2->as.type != s->as.type)
		return 0;
	if (s2->as.u.note.dc.n != 0) {
		if (s2->as.u.note.dc.h != s2->as.u.note.dc.h
		 || memcmp(&s->as.u.note.dc, &s2->as.u.note.dc,
				sizeof s->as.u.note.dc) != 0)
			return 0;
	}
	if (s->gch && s2->gch)
		return 0;
	if (s->as.type == ABC_T_REST)
		return 1;
	if (s2->ly
	 || (s2->sflags & (S_SL1 | S_SL2))
	 || s2->as.u.note.slur_st != 0
	 || s2->as.u.note.slur_end != 0)
		return 0;
	if ((s2->sflags ^ s->sflags) & (S_BEAM_ST | S_BEAM_END))
		return 0;
	nhd2 = s2->nhd;
	if (s->nhd + nhd2 + 1 >= MAXHD)
		return 0;
	if (cfmt.combinevoices <= 1 && s->pits[0] <= s2->pits[nhd2] + 1)
		return 0;
	return 1;
}

/* -- combine 2 voices -- */
static void do_combine(struct SYMBOL *s)
{
	struct SYMBOL *s2;
	int nhd, nhd2, type;

again:
	nhd = s->nhd;
	s2 = s->ts_next;
	nhd2 = s2->nhd;
	s2->extra = NULL;
	if (s->as.type != s2->as.type) {	/* if note and rest */
		if (s2->as.type == ABC_T_REST)
			goto delsym2;
#if 1
		s2 = s;
		s = s2->ts_next;
		goto delsym2;
#else
		s->as.type = ABC_T_NOTE;	/* copy the note into the rest */
		nhd = -1;
//		s->pits[0] = 127;
		s->sflags |= s2->sflags & (S_BEAM_BR1 | S_BEAM_BR2 |
					S_OTHER_HEAD |
					S_TREM1 | S_TREM2 | S_XSTEM |
					S_BEAM_ON |
					S_SL1 | S_SL2 | S_TI1 |
					S_PERC | S_RBSTOP |
					S_FEATHERED_BEAM |
					S_SHIFTUNISON_1 | S_SHIFTUNISON_2);
#endif
	} else if (s->as.type == ABC_T_REST) {
		if ((s->as.flags & ABC_F_INVIS)
		 && !(s2->as.flags & ABC_F_INVIS))
			s->as.flags &= ~ABC_F_INVIS;
		goto delsym2;
	}

	/* combine the voices */
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
	s->ymx = 3 * (s->pits[nhd] - 18) + 4;

	sort_pitch(s, 1);		/* sort the notes by pitch */

	/* force the tie directions */
	type = s->as.u.note.ti1[0];
	if ((type & 0x03) == SL_AUTO)
		s->as.u.note.ti1[0] = SL_BELOW | (type & ~SL_DOTTED);
	type = s->as.u.note.ti1[nhd];
	if ((type & 0x03) == SL_AUTO)
		s->as.u.note.ti1[nhd] = SL_ABOVE | (type & ~SL_DOTTED);
delsym2:
	if (s2->as.text && !s->as.text) {
		s->as.text = s2->as.text;
		s->gch = s2->gch;
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
		if (cfmt.combinevoices == 0
		 && s->as.type != ABC_T_REST)
			continue;
		if (s->sflags & S_IN_TUPLET) {
			g = s->extra;
			if (!g)
				continue;	/* tuplet already treated */
			r = 0;
			for ( ; g; g = g->next) {
				if (g->type == TUPLET
				 && g->as.u.tuplet.r_plet > r)
					r = g->as.u.tuplet.r_plet;
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

		if (s->as.type == ABC_T_NOTE) {
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
#if 1
//fixme: may have rests in beam
			if (s2->sflags & S_BEAM_END)
#else
			if (s2->as.type == ABC_T_REST
			 || (s2->sflags & S_BEAM_END))
#endif
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
#if 1
//fixme: may have rests in beam
			if (s2->sflags & S_BEAM_END)
#else
			if (s2->as.type == ABC_T_REST
			 || (s2->sflags & S_BEAM_END))
#endif
				break;
			do {
				s2 = s2->next;
			} while (s2->type != NOTEREST);
		}
	}
}

/* -- insert a clef change (treble or bass) before a symbol -- */
static void insert_clef(struct SYMBOL *s,
			int clef_type)
{
	struct VOICE_S *p_voice;
	struct SYMBOL *new_s;

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

	new_s->as.u.clef.type = clef_type;
	new_s->as.u.clef.line = clef_type == TREBLE ? 2 : 4;
	new_s->staff = s->staff;
	new_s->u = 1;			/* small clef */
	new_s->sflags &= ~S_SECOND;

	/* link in time */
	while (!(s->sflags & S_SEQST))
		s = s->ts_prev;
	if (s->type == STAVES) {
		s = s->ts_next;
		s->sflags |= S_SEQST;
	} else if (!s->ts_prev || s->ts_prev->type != CLEF) {
		new_s->sflags |= S_SEQST;
	}
	new_s->ts_prev = s->ts_prev;
	if (new_s->ts_prev)
		new_s->ts_prev->ts_next = new_s;
	else
		tsfirst = new_s;
	new_s->ts_next = s;
	s->ts_prev = new_s;
}

/* -- define the clef for a staff with no explicit clef -- */
/* this function is called only once for the whole tune */
static void set_clef(int staff)
{
	struct SYSTEM *sy;
	struct SYMBOL *s, *last_chg;
	int clef_type, min, max, time;

	/* get the max and min pitches */
	min = max = 16;			/* 'C' */
	for (s = tsfirst; s; s = s->ts_next) {
		if (s->staff != staff || s->as.type != ABC_T_NOTE)
			continue;
		if (s->pits[0] < min)
			min = s->pits[0];
		else if (s->pits[s->nhd] > max)
			max = s->pits[s->nhd];
	}

	sy = cursys;
	if (min >= 13)			/* all upper than 'G,' --> treble clef */
		return;
	if (max <= 19) {		/* all lower than 'F' --> bass clef */
		do {
			sy->staff[staff].clef.type = BASS;
			sy->staff[staff].clef.line = 4;
			sy = sy->next;
		} while (sy);
		return;
	}

	/* set clef changes */
	clef_type = TREBLE;
	sy->staff[staff].clef.type = clef_type;
	sy->staff[staff].clef.line = 2;
	last_chg = NULL;
	for (s = tsfirst; s; s = s->ts_next) {
		struct SYMBOL *s2, *s3, *s4;

		if (s->staff != staff || s->as.type != ABC_T_NOTE) {
			if (s->type == STAVES) {
				sy = sy->next;	/* keep the starting clef */
				sy->staff[staff].clef.type = clef_type;
				sy->staff[staff].clef.line = clef_type == TREBLE ? 2 : 4;
				last_chg = s;
			}
			continue;
		}

		/* check if a clef change may occur */
		time = s->time;
		if (clef_type == TREBLE) {
			if (s->pits[0] > 12		/* F, */
			 || s->pits[s->nhd] > 20)	/* G */
				continue;
			s2 = s->ts_prev;
			if (s2
			 && s2->time == time
			 && s2->staff == staff
			 && s2->as.type == ABC_T_NOTE
			 && s2->pits[0] >= 19)	/* F */
				continue;
			s2 = s->ts_next;
			if (s2
			 && s2->staff == staff
			 && s2->time == time
			 && s2->as.type == ABC_T_NOTE
			 && s2->pits[0] >= 19)	/* F */
				continue;
		} else {
			if (s->pits[0] < 12		/* F, */
			 || s->pits[s->nhd] < 20)	/* G */
				continue;
			s2 = s->ts_prev;
			if (s2
			 && s2->time == time
			 && s2->staff == staff
			 && s2->as.type == ABC_T_NOTE
			 && s2->pits[0] <= 13)	/* G, */
				continue;
			s2 = s->ts_next;
			if (s2
			 && s2->staff == staff
			 && s2->time == time
			 && s2->as.type == ABC_T_NOTE
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
		else
			s3 = NULL;
#endif
#if 0
		time = !last_chg ? -1 : last_chg->time;
#endif
		for (s2 = s->ts_prev; s2 != last_chg; s2 = s2->ts_prev) {
#if 0
			if (s2->time <= time)
				break;
#endif
			if (s2->staff != staff)
				continue;
			if (s2->type == BAR
			 && s2->voice == s->voice) {
#if 0 /*fixme:test*/
				if (voice_tb[s2->voice].second
				 || voice_tb[s2->voice].staff != staff)
					continue;
#endif
				s3 = s2;
				break;
			}
#if 1
			if (s2->as.type != ABC_T_NOTE)
#else
			if (s2->type != NOTEREST)	/* neither note nor rest */
#endif
				continue;

			/* exit loop if a clef change cannot occur */
//			if (s2->as.type == ABC_T_NOTE) {
				if (clef_type == TREBLE) {
					if (s2->pits[0] >= 19)		/* F */
						break;
				} else {
					if (s2->pits[s2->nhd] <= 13)	/* G, */
						break;
				}
//			}

#if 1 /*fixme:test*/
			/* have a 2nd choice on beam start */
#if 1
#if 1
#if 1 /* double clef pb - clef change on 2nd voice */
			if ((s2->sflags & S_BEAM_ST)
			 && !voice_tb[s2->voice].second)
#else
			if (s2->sflags & S_BEAM_ST)
#endif
				s3 = s2;
#else
			if ((s3->sflags & S_BEAM_ST) == 0)
				s3 = s2;
#endif
#else
			if ((s2->sflags & S_BEAM_ST)
			 || (s3->sflags & S_BEAM_ST) == 0)
				s3 = s2;
#endif
#else
			/* have a 2nd choice if word starts on the main voice */
			if (!voice_tb[s2->voice].second
			 && voice_tb[s2->voice].staff == staff) {
				if ((s2->sflags & S_BEAM_ST)
				 || !s3
				 || !(s3->sflags & S_BEAM_ST))
					s3 = s2;
			}
#endif
		}
		s2 = last_chg;
		last_chg = s;

		/* if first change, see if any note before */
		if (!s2 || s2->type == STAVES) {
#if 1 /*fixme:test*/
			s4 = s3;
#else
			if ((s4 = s3) == NULL)
				s4 = s;
#endif
			for (s4 = s4->ts_prev; s4 != s2; s4 = s4->ts_prev) {
				if (s4->staff != staff)
					continue;
				if (s4->as.type == ABC_T_NOTE)
					break;
			}

			/* if no note, change the clef of the staff */
			if (s4 == s2) {
				if (clef_type == TREBLE) {
					clef_type = BASS;
					sy->staff[staff].clef.line = 4;
				} else {
					clef_type = TREBLE;
					sy->staff[staff].clef.line = 2;
				}
				sy->staff[staff].clef.type = clef_type;
				continue;
			}
		}

		/* no change possible if no insert point */
#if 1 /*fixme:test*/
		    else if (s3->time == s2->time)
#else
		if (!s3 || s3 == s2)
#endif
			continue;

		/* insert a clef change */
		clef_type = clef_type == TREBLE ? BASS : TREBLE;
		insert_clef(s3, clef_type);
		s3->prev->staff = staff;
	}

	/* keep the starting clef of the next staff systems */
	while ((sy = sy->next) != 0) {
		sy->staff[staff].clef.type = clef_type;
		sy->staff[staff].clef.line = clef_type == TREBLE ? 2 : 4;
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
		for (s = p_voice->sym; s; s = s->next) {
			signed char up, down;

			if (s->as.type != ABC_T_NOTE) {
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
				if (s1->as.type == ABC_T_NOTE)
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
				if (s1->as.type == ABC_T_NOTE)
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
	float xx, gspleft, gspinside, gspright;

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
		set_head_directions(g);
		for (m = g->nhd; m >= 0; m--) {
			if (g->as.u.note.accs[m]) {
				xx += 5;
				if (g->as.u.note.accs[m] & 0xf8)
					xx += 2;
				break;
			}
		}
		g->x = xx;

		if (g->nflags <= 0)
			g->sflags |= S_BEAM_ST | S_BEAM_END;
		next = g->next;
		if (!next) {
			g->sflags |= S_BEAM_END;
			break;
		}
		if (next->nflags <= 0 || (next->as.flags & ABC_F_SPACE))
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
	 && next->as.type == ABC_T_NOTE) {	/* if before a note */
		if (g->y >= (float) (3 * (next->pits[next->nhd] - 18)))
			xx -= 1;		/* above, a bit closer */
		else if ((g->sflags & S_BEAM_ST)
		      && g->y < (float) (3 * (next->pits[0] - 18) - 7))
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
	float lspc, rspc, w;

	lspc = rspc = 0;
	for (ix = 0, gch = s->gch; ix < MAXGCH; ix++, gch++) {
		if (gch->type == '\0')
			break;
		switch (gch->type) {
		default: {		/* default = above */
			float wl;

			wl = -gch->x;
			if (wl > lspc)
				lspc = wl;
			w = gch->w - wl;
			if (w > rspc)
				rspc = w;
			break;
		    }
		case '<':		/* left */
			w = gch->w + wlnote;
			if (w > lspc)
				lspc = w;
			break;
		case '>':		/* right */
			w = gch->w + s->wr;
			if (w > rspc)
				rspc = w;
			break;
		}
	}
#if 1
	/* adjust width for no clash */
	s2 = s->prev;
	if (s2 && s2->gch) {
		for (s2 = s->ts_prev; ; s2 = s2->ts_prev) {
			if (s2 == s->prev) {
				AT_LEAST(wlw, lspc);
				break;
			}
			if (s2->sflags & S_SEQST)
				lspc -= s2->shrink;
		}
	}
	s2 = s->next;
	if (s2 && s2->gch) {
		for (s2 = s->ts_next; ; s2 = s2->ts_next) {
			if (s2 == s->next) {
				AT_LEAST(s->wr, rspc);
				break;
			}
			if (s2->sflags & S_SEQST)
				rspc -= 8;
		}
	}
#else
	s2 = s->prev;
	if (s2 && s2->gch) {
		AT_LEAST(wlw, lspc);
/*fixme: pb when ">" only*/
	for (s2 = s->next; s2; s2 = s2->next) {
		switch (s2->type) {
		default:
			continue;
		case NOTEREST:
		case SPACE:
		case BAR:
			if (s2->as.text)
				AT_LEAST(s->wr, rspc);
			break;
		}
		break;
	}
#endif
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
				if ((lyl = ly->lyl[i]) == 0)
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

		if ((lyl = ly->lyl[i]) == 0)
			continue;
		p = lyl->t;
		w = lyl->w;
		swfac = lyl->f->swfac;
		xx = w + 2 * cwid(' ') * swfac;
		if (isdigit((unsigned char) *p)
		 || p[1] == ':'
//		 || p[1] == '(' || p[1] == ')') {
		 || *p == '(' || *p == ')') {
			float sz;

//			if (p[1] == '(')
			if (*p == '(') {
//				sz = cwid((unsigned char) p[1]);
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
			xx = s->shhd[m];
			if (xx < 0)
				AT_LEAST(wlnote, -xx + 5);
			if (s->as.u.note.accs[m]) {
				AT_LEAST(wlnote, s->shac[m]
					 + ((s->as.u.note.accs[m] & 0xf8)
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
		if (s->as.u.note.dc.n > 0)
			wlnote += deco_width(s);

		/* space for flag if stem goes up on standalone note */
		if ((s->sflags & (S_BEAM_ST | S_BEAM_END)) == (S_BEAM_ST | S_BEAM_END)
		 && s->stem > 0 && s->nflags > 0)
			AT_LEAST(s->wr, s->xmx + 12);

		/* leave room for dots and set their offset */
		if (s->dots > 0) {

			/* standalone with up-stem and flags */
			if (s->nflags > 0 && s->stem > 0
			 && s->xmx == 0 && s->doty == 0
			 && (s->sflags & (S_BEAM_ST | S_BEAM_END))
					== (S_BEAM_ST | S_BEAM_END)
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

		/* if a tremolo on 2 notes, have space for the small beam(s) */
		if ((s->sflags & (S_TREM2 | S_BEAM_END)) == (S_TREM2 | S_BEAM_END))
			AT_LEAST(wlnote, 20);

		wlw = wlnote;

		if (s2) {
			switch (s2->type) {
			case NOTEREST:	/* extra space when up stem - down stem */
				if (s2->as.type == ABC_T_REST)
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
				 || s2->u)
					break;
				wlw += 8;
				break;
			case KEYSIG:
/*			case TIMESIG:	*/
				wlw += 4;
				break;
			}
		}

		/* leave room for guitar chord */
		if (s->gch)
			wlw = gchord_width(s, wlnote, wlw);

		/* leave room for vocals under note */
		/* related to draw_lyrics() */
		if (s->ly)
			wlw = ly_width(s, wlw);
#if 0
/*new fixme*/
		/* reduce right space when not followed by a note */
		for (k = s->next; k; k = k->next) {
			switch (k->type) {
			default:
				s->pr *= 0.8;
				break;
			case NOTEREST:
				break;
			}
			break;
		}

		/* squeeze notes a bit if big jump in pitch */
		if (s->as.type == ABC_T_NOTE
		 && s2->as.type == ABC_T_NOTE) {
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
#endif
		/* if preceeded by a grace note sequence, adjust */
		if (s2 && s2->type == GRACE)
			s->wl = wlnote - 4.5;
		else
			s->wl = wlw;
		break;
	case SPACE:
		if (s->as.u.note.lens[1] < 0)
			xx = 10;
		else
			xx = (float) s->as.u.note.lens[1] * 0.5;
		s->wr = xx;
		if (s->gch)
			xx = gchord_width(s, xx, xx);
		if (s->as.u.note.dc.n > 0)
			xx += deco_width(s);
		s->wl = xx;
		break;
	case BAR:
		if (s->sflags & S_NOREPBRA)
			break;
		if (!(s->as.flags & ABC_F_INVIS)) {
			int bar_type;

			w = 5;
			bar_type = s->as.u.bar.type;
			switch (bar_type) {
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
			s->shhd[0] = (w - 5) * -0.5;
		}
		if (s->as.u.bar.dc.n > 0)
			s->wl += deco_width(s);

		/* have room for the repeat numbers / guitar chord */
		if (s->gch
		 && strlen(s->as.text) < 4)
			s->wl = gchord_width(s, s->wl, s->wl);
		break;
	case CLEF:
		/* shift the clef to the left - see draw_symbols() */
		if (!(s->as.flags & ABC_F_INVIS)) {
			s->wl = 12 + 10;
			s->wr = (s->u ? 10 : 12) - 10;
		}
		break;
	case KEYSIG: {
		int n1, n2, esp;

		s->wl = 3;
		esp = 4;
		if (s->as.u.key.nacc == 0) {
			n1 = s->as.u.key.sf;	/* new key sig */
			if (cfmt.cancelkey || n1 == 0)
				n2 = s->u;	/* old key */
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

			n1 = n2 = s->as.u.key.nacc;
			last_acc = s->as.u.key.accs[0];
			for (i = 1; i < n2; i++) {
				if (s->as.u.key.accs[i] != last_acc) {
					last_acc = s->as.u.key.accs[i];
					esp += 3;
				}
				if (s->as.u.key.pits[i] == s->as.u.key.pits[i - 1] + 7
				 || s->as.u.key.pits[i] == s->as.u.key.pits[i - 1] - 7)
					n1--;		/* octave */
			}
		}
		s->wr = (float) (5.5 * n1 + esp);
		break;
	    }
	case TIMESIG:
		/* !!tied to draw_timesig()!! */
		w = 0;
		for (i = 0; i < s->as.u.meter.nmeter; i++) {
			int l;

			l = sizeof s->as.u.meter.meter[i].top;
			if (s->as.u.meter.meter[i].top[l - 1] == '\0') {
				l = strlen(s->as.u.meter.meter[i].top);
				if (s->as.u.meter.meter[i].top[1] == '|'
				 || s->as.u.meter.meter[i].top[1] == '.')
					l--;		/* 'C|' */
			}
			if (s->as.u.meter.meter[i].bot[0] != '\0') {
				int l2;

				l2 = sizeof s->as.u.meter.meter[i].bot;
				if (s->as.u.meter.meter[i].bot[l2 - 1] == '\0')
					l2 = strlen(s->as.u.meter.meter[i].bot);
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
		break;
	case STBRK:
		if (s->next && s->next->type == CLEF) {
			s->wr = 2;
			s->next->u = 0;	/* big clef */
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
	case FMTCHG:
	case STAVES:		/* no space */
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

	prev_time = !s->ts_prev ? s->time : s->ts_prev->time;
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
	if (s->prev && s->prev->type == MREST)
		return s->prev->wr + 16;
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
			if (s->as.u.bar.type & 0xf0)
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
	if (s->as.type == ABC_T_NOTE && s->nflags >= -1
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
		if (!s || (s->sflags & S_SEQST))
			break;
	}
	tsfirst->shrink = new_val;

	/* loop on all remaining symbols */
	for (;;) {
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
				 && (!(s3->as.flags & ABC_F_INVIS)
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
		if ((s = s2) == last_s)
			break;
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
		if (s->as.type == ABC_T_NOTE)
			break;
	}
	if (s != last_s && shrink < space) {
		while (!(s->sflags & S_SEQST))
			s = s->ts_prev;
		s->shrink += space - shrink;
	}
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

		i = g->nohdix * n;	/* number of notes/rests to repeat */
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
			if (s2->as.type == ABC_T_NOTE) {
				s2->sflags |= S_BEAM_END;
				break;
			}
		}
		s3 = s;
		for (j = g->nohdix; --j >= 0; ) {
			i = n;			/* number of notes/rests */
			if (s3->dur != 0)
				i--;
			s2 = s3->ts_next;
			while (i > 0) {
				if (s2->staff != staff)
					continue;
				if (s2->voice == voice) {
					if (s2->dur != 0)
						i--;
				}
				s2->extra = NULL;
				unlksym(s2);
				s2 = s2->ts_next;
			}
			s3->type = NOTEREST;
			s3->as.type = ABC_T_REST;
			s3->dur = s3->as.u.note.lens[0]
				= s2->time - s3->time;
			s3->sflags &= S_NL | S_SEQST;
//			s3->sflags |= S_REPEAT | S_BEAM_ST;
			s3->sflags |= S_REPEAT;
			s3->as.u.note.slur_st = s3->as.u.note.slur_end = 0;
			s3->doty = -1;
			s3->extra = NULL;
			set_width(s3);
			if (s3->sflags & S_SEQST)
				s3->space = set_space(s3);
			s3->head = H_SQUARE;
			s3 = s2;
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
		i = g->nohdix;		/* repeat number */
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
	i = g->nohdix;		/* repeat number */
	if (n == 2 && i > 1) {
		s2 = s2->next;
		if (!s2) {
			error(0, s, "Not enough bars after repeat measure");
			goto delrep;
		}
		g->nohdix = 1;
		s = (struct SYMBOL *) getarena(sizeof *s);
		memcpy(s, g, sizeof *s);
		s->next = s2->extra;
		if (s->next)
			s->next->prev = s;
		s->prev = NULL;
		s2->extra = s;
		s->nohdix = --i;
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
		s3->type = NOTEREST;
		s3->as.type = ABC_T_REST;
		s3->dur = s3->as.u.note.lens[0] = dur;
		s3->as.flags = ABC_F_INVIS;
/*fixme: should set many parameters for set_width*/
//		set_width(s3);
		if (s3->sflags & S_SEQST)
			s3->space = set_space(s3);
		s2->as.u.bar.len = 2;
		if (s2->sflags & S_SEQST)
			s2->space = set_space(s2);
		s3 = s2->next;
		s2 = s3->next;
		for (;;) {
			if (s2->type == BAR || s2->type == CLEF)
				break;
			s2->extra = NULL;
			unlksym(s2);
			s2 = s2->next;
		}
		s3->type = NOTEREST;
		s3->as.type = ABC_T_REST;
		s3->dur = s3->as.u.note.lens[0] = dur;
		s3->as.flags = ABC_F_INVIS;
		set_width(s3);
		if (s3->sflags & S_SEQST)
			s3->space = set_space(s3);
		if (s2->sflags & S_SEQST)
			s2->space = set_space(s2);
		return;
	}

	/* repeat 1 measure */
	s3 = s;
	for (j = g->nohdix; --j >= 0; ) {
		for (s2 = s3->ts_next; ; s2 = s2->ts_next) {
			if (s2->staff != staff)
				continue;
			if (s2->voice == voice
			 && s2->type == BAR)
				break;
			s2->extra = NULL;
			unlksym(s2);
		}
		s3->type = NOTEREST;
		s3->as.type = ABC_T_REST;
		s3->dur = s3->as.u.note.lens[0] = dur;
		s3->sflags &= S_NL | S_SEQST;
//		s3->sflags |= S_REPEAT | S_BEAM_ST;
		s3->sflags |= S_REPEAT;
		s3->as.u.note.slur_st = s3->as.u.note.slur_end = 0;
		s3->extra = NULL;
/*fixme: should set many parameters for set_width*/
//		set_width(s3);
		if (s3->sflags & S_SEQST)
			s3->space = set_space(s3);
		if (s2->sflags & S_SEQST)
			s2->space = set_space(s2);
		if (g->nohdix == 1) {
			s3->doty = 1;
			break;
		}
		s3->doty = g->nohdix - j + 1;	/* number to print above the repeat rest */
		s3 = s2->next;
	}
	return;

delrep:					/* remove the %%repeat */
	g->u = -1;
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
		if (s2->as.type == ABC_T_NOTE)
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
	new_s->shrink = 8 + 4;

	new_s->nhd = s2->nhd;
//	memcpy(new_s->as.u.note.lens, s2->as.u.note.lens,
//			sizeof new_s->as.u.note.lens);
	memcpy(new_s->pits, s2->pits, sizeof new_s->pits);
	for (i = 0; i <= new_s->nhd; i++)
		new_s->as.u.note.lens[i] = CROTCHET;
	new_s->as.flags = ABC_F_STEMLESS;
}

/* -- define the beginning of a new music line -- */
static struct SYMBOL *set_nl(struct SYMBOL *s)
{
	struct SYMBOL *s2, *extra, *staves;
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
		if (cfmt.keywarn)
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
#if 1
		goto setnl;
#else
		switch (s->type) {
		case NOTEREST:
		case GRACE:
		case SPACE:
			goto setnl;
		}
		break;
#endif
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
	staves = extra = NULL;
	for ( ; ; s = s->ts_next) {
		if (!s)
			return NULL;
		if (!(s->sflags & S_SEQST))
			continue;
		if (done < 0)
			break;
		if (s->type == STAVES) {	/* case "| $ %%staves K: M:" */
			if (!s->ts_next)
				return NULL;
			staves = s;
			s = s->ts_next;
		}
		switch (s->type) {
		case BAR:
			if (done
			 || (s->u == 0		/* incomplete measure */
			  && s->next		/* not at end of tune */
			  && (s->as.u.bar.type & 0x0f) == B_COL
			  && !(s->sflags & S_RRBAR)))
						/* 'xx:' (not ':xx:') */
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
			if (!cfmt.keywarn)
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
	if (staves) {			/* move the %%staves to the next line */
		if (s != staves->ts_next) {
			unlksym(staves);
			staves->prev = s->prev;
			if (s->prev)
				staves->prev->next = staves;
			s->prev = staves;
			staves->next = s;
			staves->ts_prev = s->ts_prev;
			staves->ts_prev->ts_next = staves;
			s->ts_prev = staves;
			staves->ts_next = s;
			s->sflags &= ~S_SEQST;
		}
		s = staves;
	}
setnl:
	if (cfmt.custos && !first_voice->next) {
		custos_add(s);
	} else {
		switch (s->ts_prev->type) {
		case BAR:
		case FMTCHG:
		case CLEF:
		case KEYSIG:
		case TIMESIG:
			break;
		default:			/* add an extra symbol at eol */
			p_voice = &voice_tb[s->voice];
			p_voice->last_sym = s->prev;
//			if (!p_voice->last_sym)
//				p_voice->sym = NULL;
			p_voice->time = s->time;
			extra = sym_add(p_voice, FMTCHG);
			extra->next = s;
			s->prev = extra;
			extra->ts_prev = s->ts_prev;
			extra->ts_prev->ts_next = extra;
			extra->ts_next = s;
			s->ts_prev = extra;
			if (s->x != 0) {	/* auto break */
				for (s2 = s->ts_next; ; s2 = s2->ts_next) {
					if (s2->x != 0) {
						extra->x = s2->x - 1;
						break;
					}
				}
			}
			extra->u = -1;
			extra->sflags |= S_SEQST;
			extra->wl = 6;
			extra->shrink = extra->prev->wr + 6;
			extra->space = extra->prev->space;
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
	struct SYMBOL *s, *s2;
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
			if (s->type != BAR)
				continue;
			x = s->x;
			if (x == 0)
				continue;
			if (x < xmin) {
				s2 = s;
				continue;
			}
			if (x <= xmax)
				goto cut_here;
			break;
		}

		/* try to avoid to cut a beam */
		beam = 0;
		bar_time = s2->time;
		s = s2;				/* restart on the last bar */
		s2 = NULL;
		for ( ; s != last; s = s->ts_next) {
			x = s->x;
			if (x != 0 && x >= xmin) {
				if (x > xmax)
					break;
#if 0
				if (beam <= 0) {
					for (s = s->ts_prev ; ; s = s->ts_prev) {
						if (s->x != 0)
							break;
					}
					goto cut_here;
				}
#endif
				if (!s2)
					s2 = s;
			}
			if ((s->sflags & (S_BEAM_ST | S_BEAM_END))
						== S_BEAM_ST)
				beam++;
			else if ((s->sflags & (S_BEAM_ST | S_BEAM_END))
						== S_BEAM_END)
				beam--;
		}
		if (s2 && beam <= 0) {
			s = s2;
			goto cut_here;
		}

		/* cut on a crotchet */
		s = s2;
		if (!s) {
//fixme:test - this should not occur very often
			fprintf(stderr, "*** cut_tune limit 1!\n");
			s = s2 = first->ts_next;
		}
		for ( ; s != last; s = s->ts_next) {
			x = s->x;
			if (x == 0)
				continue;
			if ((s->time - bar_time) % CROTCHET == 0) {
				for (s = s->ts_prev ; ; s = s->ts_prev) {
					if (!s)
						return s;
					if (s->x != 0)
						break;
				}
				goto cut_here;
			}
			if (x > xmax)
				break;
		}
//fixme:test - may occur when symbol width > line width
		fprintf(stderr, "*** cut_tune limit 2!\n");
		s = s->next;
		if (!s)
			return s;
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
	struct SYMBOL *s, *s2;
	int i;
	float xmin;

	/* adjust the line width according to the starting clef
	 * and key signature */
/*fixme: may change in the tune*/
	for (s = tsfirst; ; s = s->ts_next) {
		if (s->shrink == 0)
			continue;
		if (s->type != CLEF && s->type != KEYSIG)
			break;
		lwidth -= s->shrink;
	}
	if (cfmt.custos && !first_voice->next)
		lwidth -= 12;
	if (cfmt.continueall) {
		set_lines(s, 0, lwidth, indent);
		return;
	}

	/* if asked, count the measures and set the EOLNs */
	if ((i = cfmt.barsperstaff) != 0) {
		s2 = s;
		for ( ; s; s = s->ts_next) {
			if (s->type != BAR
			 || s->u == 0)
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
			for (s = s->ts_next; s; s = s->ts_next) {
				if (s->sflags & S_EOLN)
					break;
			}
			s = s2 = set_lines(s2, s, lwidth, indent);
			if (!s)
				break;
			xmin = s->shrink;
			indent = 0;
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
		 || (s->as.flags & ABC_F_INVIS)) {
//			s->ymx = s->ymn = (top + bot) / 2;
			s->ymx = s->ymn = 12;
			break;
		}
		switch (s->as.u.clef.type) {
		default:			/* treble / perc */
			s->y = -2 * 6;
			s->ymx = 24 + 15;
			s->ymn = -11;
			break;
		case ALTO:
			s->y = -3 * 6;
			s->ymx = 24 + 6;
			s->ymn = -3;
			break;
		case BASS:
			s->y = -4 * 6;
			s->ymx = 24 + 6;
			s->ymn = -3;
			break;
		}
		if (s->u) {
			s->ymx -= 2;
			s->ymn += 2;
		}
		s->y += s->as.u.clef.line * 6;
		if (s->y > 0)
			s->ymx += s->y;
		else if (s->y < 0)
			s->ymn += s->y;
		if (s->as.u.clef.octave > 0)
			s->ymx += 9;
		else if (s->as.u.clef.octave < 0)
			s->ymn -= 9;
		break;
	case KEYSIG:
		if (s->as.u.key.sf > 2)
			s->ymx = 24 + 10;
		else if (s->as.u.key.sf > 0)
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

/* -- set the pitch of the notes according to the clefs -- */
/* also set the vertical offset of the symbols */
/* this function is called only once per tune
 * then, once per music line up to the old sequence */
static void set_pitch(struct SYMBOL *last_s)
{
	struct SYSTEM *sy;
	struct SYMBOL *s;
	int staff, delta, dur;
	signed char staff_clef[MAXSTAFF];
	static signed char delta_tb[4] = {
		0 - 2 * 2,
		6 - 3 * 2,
		12 - 4 * 2,
		0 - 2 * 2
	};

	sy = cursys;
	for (staff = 0; staff <= sy->nstaff; staff++) {
		staff_clef[staff] = delta_tb[sy->staff[staff].clef.type]
				+ sy->staff[staff].clef.transpose
				+ sy->staff[staff].clef.line * 2;
	}
	dur = BASE_LEN;
	for (s = tsfirst; s != last_s; s = s->ts_next) {
		struct SYMBOL *g;
		int np, m, pav;

		for (g = s->extra ; g; g = g->next) {
			if (g->type == FMTCHG && g->u == REPEAT) {
				set_repeat(g, s);
				break;
			}
		}
		staff = s->staff;
		switch (s->type) {
		case CLEF:
			if (s->sflags & S_SECOND) {
/*fixme:%%staves:can this happen?*/
				if (!s->prev)
					break;
				unlksym(s);
				break;
			}
			set_yval(s);
			staff_clef[staff] = delta_tb[s->as.u.clef.type]
						+ s->as.u.clef.transpose
						+ s->as.u.clef.line * 2;
			break;
		case STAVES:
			sy = sy->next;
			for (staff = 0; staff <= sy->nstaff; staff++) {
				staff_clef[staff] = delta_tb[sy->staff[staff].clef.type]
						+ sy->staff[staff].clef.transpose
						+ sy->staff[staff].clef.line * 2;
			}
			break;
		case GRACE:
			for (g = s->extra; g; g = g->next) {
				if (g->type != NOTEREST)
					continue;
				delta = staff_clef[g->staff];
				if (delta != 0) {
					for (m = g->nhd; m >= 0; m--)
						g->pits[m] += delta;
				}
				g->ymn = 3 * (g->pits[0] - 18) - 2;
				g->ymx = 3 * (g->pits[g->nhd] - 18) + 2;
			}
			set_yval(s);
			break;
		case KEYSIG:
			s->pits[0] = staff_clef[staff];	/* keep the current clef */
//			s->ymx = 24 + 10;
//			s->ymn = -2;
//			break;
			/* fall thru */
		default:
			set_yval(s);
			break;
		case MREST:
			if (s->as.flags & ABC_F_INVIS)
				break;
			s->ymx = 24 + 15;
			s->ymn = -2;
			break;
		case NOTEREST:
#if 1 /* test rest offset */
			if (s->as.type != ABC_T_NOTE
			 && !first_voice->next) {
#else
			if (s->as.type != ABC_T_NOTE) {
#endif
				s->y = 12;		/* rest */
				s->ymx = 12 + 8;
				s->ymn = 12 - 8;
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
			s->ymx = 3 * (s->pits[np] - 18) + 4;
			s->ymn = 3 * (s->pits[0] - 18) - 4;
// test rest offset
			if (s->as.type != ABC_T_NOTE)
				s->y = s->yav / 6 * 6;
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
		for (u = s;
		     u && u->type != BAR && u->type != STAVES;
		     u = u->ts_next) {
			if (u->type != NOTEREST
			 || (u->as.flags & ABC_F_INVIS))
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

			if (u->as.type != ABC_T_NOTE)
				continue;
			if (u->ymx > stb[staff].st[i].ymx)
				stb[staff].st[i].ymx = u->ymx;
			if (u->ymn < stb[staff].st[i].ymn)
				stb[staff].st[i].ymn = u->ymn;
			if (u->sflags & S_XSTEM) {
				if (u->ts_prev->staff != staff - 1
				 || u->ts_prev->as.type != ABC_T_NOTE) {
					error(1, s, "Bad +xstem+");
					u->sflags &= ~S_XSTEM;
/*fixme:nflags KO*/
				} else {
					u->ts_prev->multi = 1;
					u->multi = 1;
					u->as.flags |= ABC_F_STEMLESS;
				}
			}
		}

		for ( ; s != u; s = s->ts_next) {
			if (s->type != NOTEREST		/* if not note nor rest */
			 && s->type != GRACE)
				continue;
			staff = s->staff;
			voice = s->voice;
			if (!s->multi && vtb[voice].st2 >= 0) {
				if (staff == vtb[voice].st1)
					s->multi = -1;
				else if (staff == vtb[voice].st2)
					s->multi = 1;
			}
			if (stb[staff].nvoice <= 0) { /* voice alone on the staff */
				if (s->multi)
					continue;
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
			if (!s->multi) {
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
						if (s->ts_prev->time == s->time
						 && s->ts_prev->staff == s->staff
						 && s->pits[s->nhd] == s->ts_prev->pits[0]
						 && (s->sflags & (S_BEAM_ST | S_BEAM_END))
								== (S_BEAM_ST | S_BEAM_END)
						 && ((t = s->ts_next) == 0
						  || t->staff != s->staff
						  || t->time != s->time))
							s->multi = -1;
					}
				}
			}
		}

		while (s) {
			switch (s->type) {
			case STAVES:
				sy = sy->next;
				for (staff = nst + 1; staff <= sy->nstaff; staff++) {
					stb[staff].nvoice = -1;
					for (i = 4; --i >= 0; ) {
						stb[staff].st[i].voice = -1;
						stb[staff].st[i].ymx = 0;
						stb[staff].st[i].ymn = 24;
					}
				}
				nst = sy->nstaff;
				/*fall thru*/
			case BAR:
				s = s->ts_next;
				continue;
			}
			break;
		}
	}
}

/* -- shift a rest vertically or horizontally -- */
static void shift_rest(struct SYMBOL *s,	/* rest */
			struct SYMBOL *s2,	/* other note/rest */
			struct SYSTEM *sy)
{
	int y, us, ls, ymx, ymn;

	us = rest_sp[C_XFLAGS - s->nflags].u;
	ls = rest_sp[C_XFLAGS - s->nflags].l;

	/* check if clash */
	ymx = s->y + us;
	ymn = s->y - ls;
	if (ymx < s2->ymn
	 || ymn > s2->ymx)
		return;			/* no */

	/* decide to move the rest upper or lower
	 * according to the voice ranges */
	if (sy->voice[s->voice].range > sy->voice[s2->voice].range)
		ymx = s2->ymn;			/* lower */
	else
		ymx = s2->ymx;			/* upper */

	/* change the rest vertical offset */
	if (ymx >= s2->ymx) {
		y = (s2->ymx + ls + 3) / 6 * 6;
		if (y < 12)
			y = 12;
		if (s->y < y)
			s->y = y;
	} else {
		y = (s2->ymn - us - 3) / 6 * 6;
		if (y > 12)
			y = 12;
		if (s->y > y)
			s->y = y;
	}
	s->ymx = s->y + us;
	s->ymn = s->y - ls;
}

/* -- adjust the offset of the rests when many voices -- */
/* this function is called only once per tune */
static void set_rest_offset(void)
{
	struct SYSTEM *sy;
	struct SYMBOL *s, *s2, *prev;
	int nvoice, voice, end_time, not_alone;
	struct {
		struct SYMBOL *s;
		int staff;
		int end_time;
	} vtb[MAXVOICE], *v;

	memset(vtb, 0, sizeof vtb);
	
	sy = cursys;
	nvoice = 0;
	for (s = tsfirst; s; s = s->ts_next) {
		v = &vtb[s->voice];
		if (s->as.flags & ABC_F_INVIS)
			continue;
		switch (s->type) {
		case STAVES:
			sy = sy->next;
		default:
			continue;
		case NOTEREST:
			break;
		}
		if (s->voice > nvoice)
			nvoice = s->voice;
		v->s = s;
		v->staff = s->staff;
		v->end_time = s->time + s->dur;
		if (s->as.type != ABC_T_REST)
			continue;

		/* check if clash with previous symbols */
		not_alone = 0;
		prev = NULL;
		for (voice = 0, v = vtb; voice <= nvoice; voice++, v++) {
			if (!v->s
			 || v->staff != s->staff
			 || voice == s->voice)
				continue;
			if (v->end_time <= s->time)
				continue;
			not_alone++;
			shift_rest(s, v->s, sy);
			if (voice < s->voice && v->s->time == s->time)
				prev = v->s;
		}

		/* check if clash with next symbols */
		end_time = s->time + s->dur;
		for (s2 = s->ts_next; s2; s2 = s2->ts_next) {
			if (s2->time >= end_time)
				break;
			if (s2->staff != s->staff
			 || s2->type != NOTEREST
			 || (s2->as.flags & ABC_F_INVIS))
				continue;
			not_alone++;
			if (s2->as.type != ABC_T_REST) {
				shift_rest(s, s2, sy);

				/* shift to the right if same time and clash */
				if (prev
				 && s2->time == s->time
				 && (s2->ymx > s->ymn
				  || prev->ymn < s->ymx)) {
					int y;

					s->shhd[0] = 10;
					s->xmx = 10;
					y = (prev->ymn + s2->ymx) / 2;
					if (y < 12)
						y += 5;
					s->y = y / 6 * 6;
					s->ymx = s->y + 8;
					s->ymn = s->y - 8;
				}
			}
		}
		if (!not_alone) {
			s->y = 12;
			s->ymx = 12 + 8;
			s->ymn = 12 - 8;
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
	if (s->ts_prev->type != type)
		s->sflags |= S_SEQST;
	last_s->ts_prev = s;
	if (last_s->type == type && s->voice != last_s->voice) {
		last_s->sflags &= ~S_SEQST;
		last_s->shrink = 0;
	}
	s->as.fn = last_s->as.fn;
	s->as.linenum = last_s->as.linenum;
	s->as.colnum = last_s->as.colnum;
	return s;
}

/* -- init the symbols at start of a music line -- */
/* this routine is called when starting a tune generation,
 * and later for each new music line */
static void init_music_line(void)
{
	struct VOICE_S *p_voice;
	struct SYMBOL *s, *last_s;
	int voice, staff;

	/* initialize the voices */
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		voice = p_voice - voice_tb;
		if (cursys->voice[voice].range < 0)
			continue;
		p_voice->second = cursys->voice[voice].second;
		staff = cursys->voice[voice].staff;
		while (staff < nstaff && cursys->staff[staff].empty)
			staff++;
		p_voice->staff = staff;
		s = p_voice->sym;
		if (!s)
			continue;
		if (s->type == CLEF) {		/* move the clefs and keysig's */
			if (!p_voice->second) {
				cursys->staff[staff].clef.type
					= s->as.u.clef.type;
				cursys->staff[staff].clef.line
					= s->as.u.clef.line;
				cursys->staff[staff].clef.octave
					= s->as.u.clef.octave;
				cursys->staff[staff].clef.invis
					= s->as.u.clef.invis;
			}
			s = s->next;
			if (!s)
				continue;
		}
		if (s->type == KEYSIG)
			memcpy(&p_voice->key, &s->as.u.key,
				sizeof s->as.u.key);
	}

	/* add a clef at start of each voice */
	last_s = tsfirst;
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		voice = p_voice - voice_tb;
		if (cursys->voice[voice].range < 0)
			continue;
		staff = cursys->voice[voice].staff;
		if (last_s->voice == voice && last_s->type == CLEF) {
			last_s->u = 0;		/* normal clef */
#if 0
			if (cursys->staff[staff].clef.invis)
				s->as.flags |= ABC_F_INVIS;
#endif
			p_voice->last_sym = last_s;
			last_s = last_s->ts_next;
		} else {
			s = (struct SYMBOL *) getarena(sizeof *s);
			memset(s, 0, sizeof *s);
			s->type = CLEF;
			s->voice = voice;
			s->staff = staff;
			s->time = last_s->time;
			s->next = p_voice->sym;
			if (s->next) {
				s->next->prev = s;
				s->as.fn = s->next->as.fn;
				s->as.linenum = s->next->as.linenum;
				s->as.colnum = s->next->as.colnum;
			}
			p_voice->sym = s;
			p_voice->last_sym = s;
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
			memcpy(&s->as.u.clef,
				&cursys->staff[staff].clef,
				sizeof s->as.u.clef);
			if (cursys->voice[voice].second)
				s->sflags |= S_SECOND;
			if (cursys->staff[staff].clef.invis
			 || cursys->staff[staff].empty)
				s->as.flags |= ABC_F_INVIS;
			set_yval(s);
		}
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
			last_s = last_s->ts_next;
			continue;
		}
		if (p_voice->key.sf != 0 || p_voice->key.nacc != 0) {
			s = sym_new(KEYSIG, p_voice, last_s);
			memcpy(&s->as.u.key, &p_voice->key, sizeof s->as.u.key);
			if (s->as.u.key.mode == BAGPIPE + 1)
				s->u = 3;	/* K:Hp --> G natural */
			set_yval(s);
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
			memcpy(&s->as.u.meter, &p_voice->meter,
			       sizeof s->as.u.meter);
			set_yval(s);
		}
	}

	/* add bar if needed */
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		int i;

		voice = p_voice - voice_tb;
		if (cursys->voice[voice].range < 0
		 || cursys->voice[voice].second
		 || cursys->staff[cursys->voice[voice].staff].empty
		 || p_voice->bar_start == 0)
			continue;
		i = 2;
		if (!p_voice->bar_text		/* if repeat continuation */
		 && p_voice->bar_start == B_OBRA) {
			for (s = p_voice->last_sym;
			     s;
			     s = s->next) {	/* search the end of repeat */
				if (s->sflags & S_RBSTOP) {
					i = -1;
					break;
				}
				if (s->type != BAR)
					continue;
				if ((s->as.u.bar.type & 0xf0)	/* if complex bar */
				 || s->as.u.bar.type == B_CBRA
				 || s->as.u.bar.repeat_bar)
					break;
				if (--i < 0)
					break;
			}
			if (!s)
				i = -1;
			if (i >= 0 && p_voice->last_sym->time == s->time)
				i = -1;		/* no note */
		}
		if (i >= 0) {
			s = sym_new(BAR, p_voice, last_s);
			s->as.u.bar.type = p_voice->bar_start & 0x3fff;
			if (p_voice->bar_start & 0x8000)
				s->as.flags |= ABC_F_INVIS;
			if (p_voice->bar_start & 0x4000)
				s->sflags |= S_NOREPBRA;
			s->as.text = p_voice->bar_text;
			s->gch = p_voice->bar_gch;
			s->as.u.bar.repeat_bar = p_voice->bar_repeat;
			set_yval(s);
		}
		p_voice->bar_start = 0;
		p_voice->bar_repeat = 0;
		p_voice->bar_text = 0;
		p_voice->bar_gch = 0;
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
	while ((sy = sy->next) != 0) {
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

			if (!p_voice->forced_clef
			 || p_voice->octave != 0)
				continue;
#if 1
			/* (the clefs in the voice table are not yet initialized) */
			i = p_voice->staff;
			i = cursys->staff[i].clef.type;
#else
			i = p_voice->clef.type;
#endif
			if (i == PERC)
				continue;
			delta = delpit[i];
			for (s = p_voice->sym; s; s = s->next) {
				switch (s->type) {
				case CLEF:
					i = s->as.u.clef.type;
					if (!s->as.u.clef.check_pitch)
						i = 0;
					delta = delpit[i];
					break;
				case NOTEREST:
					if (delta == 0)
						break;
					if (s->as.type == ABC_T_REST)
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

	/* set a pitch for all symbols and the start/stop of words (beams) */
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		int pitch, start_flag;
		struct SYMBOL *sym, *lastnote;

		sym = p_voice->sym;
		for (s = sym; s; s = s->next) {
			if (s->as.type == ABC_T_NOTE) {
				pitch = s->pits[0];
				break;
			}
		}
		if (!s)
			pitch = 127;			/* no note */
		start_flag = 1;
		lastnote = 0;
		for (s = sym; s; s = s->next) {
			switch (s->type) {
			default:
				if (s->as.flags & ABC_F_SPACE)
					start_flag = 1;
				break;
			case MREST:
				start_flag = 1;
				break;
			case BAR:
				if (!(s->sflags & S_BEAM_ON))
					start_flag = 1;
				if (!s->next
				 && s->prev->as.type == ABC_T_NOTE
				 && s->prev->dur >= BREVE)
					s->prev->head = H_SQUARE;
				break;
			case NOTEREST:
				if (s->sflags & S_TREM2)
					break;
				if (s->as.flags & ABC_F_SPACE)
					start_flag = 1;
				if (start_flag
				 || s->nflags - s->u <= 0) {
					if (lastnote) {
						lastnote->sflags |= S_BEAM_END;
						lastnote = NULL;
					}
					if (s->nflags - s->u <= 0) {
						s->sflags |= (S_BEAM_ST | S_BEAM_END);
					} else if (s->as.type == ABC_T_NOTE) {
						s->sflags |= S_BEAM_ST;
						start_flag = 0;
					}
				}
				if (s->sflags & S_BEAM_END)
					start_flag = 1;
				if (s->as.type == ABC_T_NOTE)
					lastnote = s;
				break;
			}
			if (s->as.type == ABC_T_NOTE) {
				pitch = s->pits[0];
				if (s->prev
				 && s->prev->as.type != ABC_T_NOTE) {
					s->prev->pits[0] = (s->prev->pits[0]
							    + pitch) / 2;
				}
			} else {
				s->pits[0] = pitch;
			}
		}
		if (lastnote)
			lastnote->sflags |= S_BEAM_END;
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

	/* set a pitch to the symbols of voices with no note */
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		int pitch;
		struct SYMBOL *sym;

		sym = p_voice->sym;
		if (!sym || sym->pits[0] != 127)
			continue;
		switch (cursys->staff[sym->staff].clef.type) {
		default:
		case TREBLE:
			pitch = 22;		/* 'B' */
			break;
		case ALTO:
			pitch = 16;		/* 'C' */
			break;
		case BASS:
			pitch = 10;		/* 'D,' */
			break;
		}
		for (s = sym; s; s = s->next)
			s->pits[0] = pitch;
	}
	set_pitch(NULL);		/* adjust the note pitches */
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
		for (staff = 0; staff <= nstaff; staff++) {
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
	struct SYMBOL *s, *t, *g, *s_opp;
	int beam, laststem, lasty;

	beam = 0;
	laststem = -1;
	lasty = 0;
	s_opp = NULL;
	for (s = sym; s; s = s->next) {
		if (s->as.type != ABC_T_NOTE) {
			if (s->type != GRACE)
				continue;
			g = s->extra;
			while (g->as.type != ABC_T_NOTE)
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

		if (s->stem == 0		/* if not explicitly set */
		 && (s->stem = s->multi) == 0) { /* and alone on the staff */

			/* notes in a beam have the same stem direction */
			if (beam) {
				s->stem = laststem;
			} else if ((s->sflags & (S_BEAM_ST | S_BEAM_END))
					== S_BEAM_ST) { /* start of beam */
				int avg, n;

				avg = s->yav;
				n = 12;
				for (t = s->next; t; t = t->next) {
					if (t->as.type == ABC_T_NOTE) {
						if (t->multi) {
							avg = n - t->multi;
							break;
						}
						avg += t->yav;
						n += 12;
					}
					if (t->sflags & S_BEAM_END)
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
				if (s->yav == 12	/* note on middle line */
				 && !cfmt.bstemdown) {
					int dy;

					if (!s->prev || s->prev->type == BAR) {
						for (t = s->next; t; t = t->next) {
							if (t->as.type == ABC_T_NOTE
							 || t->type == BAR)
								break;
						}
						if (t && t->as.type == ABC_T_NOTE
						 && t->yav < 12)
							s->stem = 1;
					} else {
						dy = s->yav - lasty;
						if (dy > -7 && dy < 7)
							s->stem = laststem;
					}
				}
			}
		} else {			/* stem set by set_stem_dir */
			if ((s->sflags & (S_BEAM_ST | S_BEAM_END))
					== S_BEAM_ST) /* start of beam */
				beam = 1;
		}
		if (s->sflags & S_BEAM_END)
			beam = 0;
		laststem = s->stem;
		lasty = s->yav;

		if (s_opp) {			/* opposite gstem direction */
			for (g = s_opp->extra; g; g = g->next)
				g->stem = -laststem;
			s_opp->stem = -laststem;
			s_opp = NULL;
		}
	}
}

/* -- shift the notes horizontally when voices overlap -- */
/* this routine is called only once per tune */
static void set_overlap(void)
{
	struct SYMBOL *s, *s1, *s2;
	int d, i1, i2, m, sd1, sd2, t;
	float d1, d2, dy1, dy2, noteshift;

	for (s = tsfirst; s; s = s->ts_next) {
		if (s->as.type != ABC_T_NOTE
		 || (s->as.flags & ABC_F_INVIS))
			continue;

		/* treat the stem on two staves with different directions */
		if ((s->sflags & S_XSTEM)
		 && s->ts_prev->stem < 0) {
			s2 = s->ts_prev;
			for (m = 0; m <= s2->nhd; m++) {
				s2->shhd[m] += STEM_XOFF * 2;
				s2->shac[m] -= STEM_XOFF * 2;
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
			if (s2->as.type == ABC_T_NOTE
			 && !(s2->as.flags & ABC_F_INVIS)
			 && s2->staff == s->staff)
				break;
		}
		if (!s2)
			continue;

		sd1 = sd2 = 0;
		d1 = d2 = dy1 = dy2 = 0;
		s1 = s;

		/* set the smallest interval type */
		t = 0;		/* t: interval types
				 *	0: >= 4
				 *	1: third or fourth
				 *	2: second
				 *	4: unison
				 *	-1: unison and different accidentals */
		{
			int dp;

			i1 = s1->nhd;
			i2 = s2->nhd;
			for (;;) {
				dp = s1->pits[i1] - s2->pits[i2];
				switch (dp) {
				case 0:
					if (s1->as.u.note.accs[i1] != s2->as.u.note.accs[i2])
						t = -1;
					else
						t |= 4;
					break;
				case 1:
				case -1:
					t |= 2;
					break;
				case 2:
				case -2:
				case 3:
				case -3:
					t |= 1;
					break;
				}
				if (t < 0)
					break;
				if (dp >= 0) {
					i1--;
					if (i1 < 0)
						break;
				}
				if (dp <= 0) {
					i2--;
					if (i2 < 0)
						break;
				}
			}
		}
		if (s1->dur >= BREVE || s2->dur >= BREVE) {
			noteshift = 13;
		} else {

			/* if the 2nd voice is far enough, don't shift it */
			if (t == 0 && s1->pits[0] > s2->pits[s2->nhd]
			 && s1->stem > 0 && s2->stem < 0)
				continue;
			if (s1->dur >= SEMIBREVE || s2->dur >= SEMIBREVE) {
				noteshift = 10;
			} else {
				if (t == 1 && s1->pits[0] > s2->pits[s2->nhd]
				 && s1->stem > 0 &&  s2->stem < 0) {
					if (s1->shac[0] < 8)
						s1->shac[0] += 5;
					if (s1->as.u.note.accs[0] != 0
					 && s2->as.u.note.accs[s2->nhd] != 0)
						s2->shac[s2->nhd] += 10;
					continue;
				}
				noteshift = 7.8;
			}
		}

		/* if unison and different accidentals */
		if (t < 0) {
			if (s2->as.u.note.accs[i2] == 0) {
				d1 = noteshift + s2->xmx + s1->shac[i1];
				if (s1->as.u.note.accs[i1] & 0xf8)
					d1 += 2;
				if (s2->dots)
					d1 += 6;
				for (m = 0; m <= s1->nhd; m++) {
					s1->shhd[m] += d1;
					s1->shac[m] -= d1;
				}
				s1->xmx += d1;
			} else {
				d2 = noteshift + s1->xmx + s2->shac[i2];
				if (s2->as.u.note.accs[i2] & 0xf8)
					d2 += 2;
				if (s1->dots)
					d2 += 6;
				for (m = 0; m <= s2->nhd; m++) {
					s2->shhd[m] += d2;
					s2->shac[m] -= d2;
				}
				s2->xmx += d2;
			}
			s2->doty = -3;
			continue;
		}

		if (s1->stem * s2->stem > 0) {	/* if same stem direction */
			d2 = noteshift + 2;	/* shift the 2nd voice */
			if (s1->dur < CROTCHET
			 && (s1->sflags & (S_BEAM_ST | S_BEAM_END))
					== (S_BEAM_ST | S_BEAM_END)) { /* if a flag */
				if (s1->stem > 0) {
					if (3 * (s1->pits[s1->nhd] - 18) > s2->ymx) {
						d2 *= 0.5;
						sd1 = -1;
					} else if (s1->pits[s1->nhd] <= s2->pits[s2->nhd]) {
						d2 += noteshift;
					}
				}
			} else {			/* no flag */
				if (s1->pits[0] > s2->pits[s2->nhd] + 1) {
					d2 *= 0.5;
					sd1 = -1;
				}
			}
		} else if (s->stem < 0) {	/* if stem inverted, */
			s1 = s2;		/* invert the voices */
			s2 = s;
		}

		d = s1->pits[0] - s2->pits[s2->nhd];
		if (d >= 0)
			dy2 = -3;	/* the dot of the 2nd voice shall be lower */

		if (s1->head == H_SQUARE || s2->head == H_SQUARE) {
			if (s1->ymn >= s2->ymx + 4
			 || s1->ymx <= s2->ymn - 4) {
				d2 = 0;
				goto do_shift;
			}
			if (s1->stem * s2->stem > 0)	/* if same stem direction */
				goto do_shift;
		} else {
			if (s1->ymn >= s2->ymx - 2
			 || s1->ymx <= s2->ymn + 2) {
				d2 = 0;
				goto do_shift;
			}
			if (s1->stem * s2->stem > 0)	/* if same stem direction */
				goto do_shift;
			if (d >= 2)
				goto do_shift;
		}
		/* (here, voice 1 stem up and voice 2 stem down) */

		/* if unison */
		if (t >= 4) {
			int l1, l2;

			if ((s1->sflags & (S_SHIFTUNISON_1 | S_SHIFTUNISON_2))
					== (S_SHIFTUNISON_1 | S_SHIFTUNISON_2))
				goto uni_shift;
			if ((l1 = s1->dur) >= SEMIBREVE)
				goto uni_shift;
			if ((l2 = s2->dur) >= SEMIBREVE)
				goto uni_shift;
			if (s1->as.flags & s2->as.flags & ABC_F_STEMLESS)
				goto uni_shift;
			if (s1->dots != s2->dots) {
				if ((s1->sflags & (S_SHIFTUNISON_1 | S_SHIFTUNISON_2))
				 || s1->dots * s2->dots != 0)
					goto uni_shift;
			}
			i2 = 0;
			while (i2 <= s2->nhd && s2->pits[i2] != s1->pits[0])
				i2++;
			if (i2 > s2->nhd)
				goto uni_shift;
			i1 = 0;
			while (i1 < s1->nhd && i1 + i2 < s2->nhd
			    && s2->pits[i1 + i2 + 1] == s1->pits[i1 + 1])
				i1++;
			if (i1 + i2 != s2->nhd)
				goto uni_shift;
			if (l1 == l2)
				goto same_head;
			if (l1 < l2) {
				l1 = l2;
				l2 = s1->dur;
			}
			if (l1 < MINIM) {
				if (s2->dots > 0) {
					dy2 = -3;
					goto head_2;
				}
				if (s1->dots > 0)
					goto head_1;
				goto same_head;
			}
			if (l2 < CROTCHET) {	/* (l1 >= MINIM) */
				if ((s1->sflags & S_SHIFTUNISON_2)
				 || s1->dots != s2->dots)
					goto uni_shift;
				if (s2->dur >= MINIM) {
					dy2 = -3;
					goto head_2;
				}
				goto head_1;
			}
			goto uni_shift;
		same_head:
			if (voice_tb[s1->voice].scale < voice_tb[s2->voice].scale)
				goto head_2;
		head_1:
			s2->nohdix = i2;	/* keep heads of 1st voice */
			for (; i2 <= s2->nhd; i2++)
				s2->as.u.note.accs[i2] = 0;
			goto do_shift;
		head_2:
			s1->nohdix = i1;	/* keep heads of 2nd voice */
			for (; i1 >= 0; i1--)
				s1->as.u.note.accs[i1] = 0;
			goto do_shift;
		}

		if (d == -1
		 && (s1->nhd == 0 || s1->pits[1] > s2->pits[s2->nhd])
		 && (s2->nhd == 0 || s1->pits[0] > s2->pits[s2->nhd - 1])) {
			if (!(s->as.flags & ABC_F_STEMLESS)) {
				d1 = noteshift;
				if (s2->dots && s1->dots == s2->dots) {
					sd2 = 1;
					dy1 = -3;
				}
			} else {
				d2 = noteshift;
			}
			goto do_shift;
		}

		if (t == 1) {			/* if third or fourth only */
			if (s1->head != H_SQUARE
			 && s2->head != H_SQUARE)
				t = 0;
		}
		if (t == 0) {			/* if small overlap */
			if (s1->dur < SEMIBREVE
			 && s2->dur < SEMIBREVE) {
				if (s2->dur < CROTCHET
				 && (s2->sflags & (S_BEAM_ST | S_BEAM_END))
						== (S_BEAM_ST | S_BEAM_END) /* if flag */
				 && s1->pits[0] < s2->pits[0]
				 && 3 * (s1->pits[s1->nhd] - 18) > s2->ymn)
					d1 = noteshift;
				else
					d1 = noteshift * 0.3;	// (was 0.6)
				if (s2->dots)
					sd2 = 1;
			} else {
				d2 = noteshift + 1.5;
				if (s1->dots)
					sd1 = 1;
			}
			goto do_shift;
		}

	uni_shift:
		if (t >= 2) {				/* if close or unison */
			if (s1->dots != s2->dots) {
				if (s1->dots > s2->dots) /* shift the voice with more dots */
					d1 = noteshift;
				else
					d2 = noteshift;
/*fixme:if second, see if dots may be distinguished?*/
			} else if (d == 1) {
				d2 = noteshift;
				if (s1->dots)
					sd1 = 1;
			} else {
				if (t >= 4		/* if unison */
				 && s1->stem >= 0)
					d2 = noteshift;
				else
					d1 = noteshift;
			}
			if (t >= 4) {			/* if unison */
				if (d1 != 0)
					d1 += 1.5;
				else
					d2 += 1.5;
			}
			goto do_shift;
		}

		/* if the upper note is SEMIBREVE or higher, shift it */
		if (s1->dur >= SEMIBREVE
		 && s1->dur > s2->dur) {
			d1 = noteshift;

		/* else shift the 2nd voice */
		} else {
			d2 = noteshift;
			if (s1->dots > 0
			 && (d != 1 || (s1->pits[0] & 1)))
/*fixme: d always != 1 ?*/
				sd1 = 1;	/* and the dot of the 1st voice */
		}

		/* do the shift, and update the width */
	do_shift:

		/* shift the accidentals */
		for (i1 = 0; i1 <= s1->nhd; i1++) {
			int dp;
			float shft;

			if (s1->as.u.note.accs[i1] == 0)
				continue;
			for (i2 = 0; i2 <= s2->nhd; i2++) {
				dp = s1->pits[i1] - s2->pits[i2];
				if (dp > 5 || dp < -5)
					continue;
				if (s2->as.u.note.accs[i2] == 0) {
					if (s2->shhd[i2] < 0
					 && dp == 3) {
						s1->shac[i1] = 9 + 7;
					}
					continue;
				}
				if (dp == 0) {
					s2->as.u.note.accs[i2] = 0;
					continue;
				}
				shft = (dp <= -4 || dp >= 4) ? 4.5 : 7;
				if (dp > 0) {
					if (s1->as.u.note.accs[i1] & 0xf8)
						shft += 2;
					if (s2->shac[i2] < s1->shac[i1] + shft
					 && s2->shac[i2] > s1->shac[i1] - shft)
						s2->shac[i2] = s1->shac[i1] + shft;
				} else {
					if (s2->as.u.note.accs[i2] & 0xf8)
						shft += 2;
					if (s1->shac[i1] < s2->shac[i2] + shft
					 && s1->shac[i1] > s2->shac[i2] - shft)
						s1->shac[i1] = s2->shac[i2] + shft;
				}
			}
		}

		/* handle the previous shift */
		m = s1->stem >= 0 ? 0 : s1->nhd;
		d1 -= s1->shhd[m];
		d2 += s1->shhd[m];
		m = s2->stem >= 0 ? 0 : s2->nhd;
		d1 += s2->shhd[m];
		d2 -= s2->shhd[m];

		if (d1 > 0) {			/* shift the 1st voice */
			if (s2->dots && sd2 == 0)	/* room for the dots */
				d1 += 8 + 3.5 * (s2->dots - 1);
			for (m = s1->nhd; m >= 0; m--)
				s1->shhd[m] += d1;
			s1->xmx += d1;
			if (sd2 != 0)
				s2->xmx = s1->xmx;
		}
		if (d2 > 0) {			/* shift the 2nd voice */
			if (s1->dots && sd1 == 0)	/* room for the dots */
				d2 += 8 + 3.5 * (s1->dots - 1);
			for (m = s2->nhd; m >= 0; m--) {
				s2->shhd[m] += d2;
				if (s2->as.u.note.accs[m] != 0
				 && s2->pits[m] < s1->pits[0] - 4)
					s2->shac[m] -= d2;
			}
			s2->xmx += d2;
			if (sd1 > 0)
				s1->xmx = s2->xmx;
		}
		s1->doty = dy1;
		s2->doty = dy2;
	}
}

/* -- set the stem lengths -- */
/* this routine is called only once per tune */
static void set_stems(void)
{
	struct SYMBOL *s, *s2, *g;
	float slen, scale;
	int ymn, ymx, nflags;

	for (s = tsfirst; s; s = s->ts_next) {
		if (s->as.type != ABC_T_NOTE) {
			int ymin, ymax;

			if (s->type != GRACE)
				continue;
			ymin = ymax = 12;
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
		set_head_directions(s);

		/* if start or end of beam, adjust the number of flags
		 * with the other end */
		nflags = s->nflags;
		if ((s->sflags & (S_BEAM_ST | S_BEAM_END)) == S_BEAM_ST) {
			if (s->sflags & S_FEATHERED_BEAM)
				nflags = ++s->nflags;
			for (s2 = s->next; /*s2*/; s2 = s2->next) {
				if (s2->as.type == ABC_T_NOTE) {
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
		if (s->u != 0)
			slen += 2 * s->u;		/* tremolo */
		if (s->as.flags & ABC_F_STEMLESS) {
			if (s->stem >= 0) {
				s->y = ymn;
				s->ys = (float) ymx;
			} else {
				s->ys = (float) ymn;
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
			if (s->as.u.note.ti1[0] != 0)
/*fixme
 *			 || s->as.u.note.ti2[0] != 0) */
				ymn -= 3;
			s->ymn = ymn - 4;
			s->ys = ymx + slen;
			if (s->ys < 12)
				s->ys = 12;
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
			if (s->ys > 12)
				s->ys = 12;
			s->ymn = (int) (s->ys - 2.5);
			s->y = ymx;
/*fixme:the tie may be lower*/
			if (s->as.u.note.ti1[s->nhd] != 0)
/*fixme
 *			 || s->as.u.note.ti2[s->nhd] != 0)*/
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
		 && s->time > p_voice->sym->time)	/* if not empty voice */
			insert_meter |= 1;	/* meter in the next line */
		if ((s = s->prev) == 0)
			return;
	}
	if (s->type != BAR)
		return;

	if (s->as.u.bar.repeat_bar) {
		p_voice->bar_start = B_OBRA;
		p_voice->bar_text = s->as.text;
		p_voice->bar_gch = s->gch;
		p_voice->bar_repeat = 1;
		s->as.text = NULL;
		s->gch = NULL;
		s->as.u.bar.repeat_bar = 0;
		if (s->as.flags & ABC_F_INVIS)
			p_voice->bar_start |= 0x8000;
		if (s->sflags & S_NOREPBRA)
			p_voice->bar_start |= 0x4000;
	}
	bar_type = s->as.u.bar.type;
	if (bar_type == B_COL)			/* ':' */
		return;
	if ((bar_type & 0x0f) != B_COL)		/* if not left repeat bar */
		return;
	if (!(s->sflags & S_RRBAR)) {		/* 'xx:' (not ':xx:') */
		p_voice->bar_start = bar_type & 0x3fff;
		if (s->as.flags & ABC_F_INVIS)
			p_voice->bar_start |= 0x8000;
		if (s->sflags & S_NOREPBRA)
			p_voice->bar_start |= 0x4000;
		if (s->prev && s->prev->type == BAR)
			unlksym(s);
		else
			s->as.u.bar.type = B_BAR;
		return;
	}
	if (bar_type == B_DREP) {		/* '::' */
		s->as.u.bar.type = B_RREP;
		p_voice->bar_start = B_LREP;
		if (s->as.flags & ABC_F_INVIS)
			p_voice->bar_start |= 0x8000;
		if (s->sflags & S_NOREPBRA)
			p_voice->bar_start |= 0x4000;
		return;
	}
	for (i = 0; bar_type != 0; i++)
		bar_type >>= 4;
	bar_type = s->as.u.bar.type;
	s->as.u.bar.type = bar_type >> ((i / 2) * 4);
	i = ((i + 1) / 2 * 4);
	bar_type &= 0x3fff;
	p_voice->bar_start = bar_type & ((1 << i) - 1);
	if (s->as.flags & ABC_F_INVIS)
		p_voice->bar_start |= 0x8000;
	if (s->sflags & S_NOREPBRA)
		p_voice->bar_start |= 0x4000;
}

/* -- move the symbols of an empty staff to the next one -- */
static void sym_staff_move(int staff,
			struct SYMBOL *s,
			struct SYSTEM *sy)
{
	for (;;) {
		if (s->staff == staff
		 && s->type != CLEF) {
			s->staff++;
			s->as.flags |= ABC_F_INVIS;
		}
		s = s->ts_next;
		if (s == tsnext || s->type == STAVES)
			break;
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
	char empty[MAXSTAFF];

	/* reset the staves */
	sy = cursys;
	for (staff = 0; staff <= nstaff; staff++) {
		p_staff = &staff_tb[staff];
		p_staff->y = 0;		/* staff system not computed */
		p_staff->clef.stafflines = sy->staff[staff].clef.stafflines;
		if (sy->staff[staff].clef.staffscale != 0)
			p_staff->clef.staffscale = sy->staff[staff].clef.staffscale;
	}

	/* search the next end of line,
	 * set the repeat measures, (remove some dble bars?)
	 * and flag the empty staves
	 */
	memset(empty, 1, sizeof empty);
	for (s = tsfirst; s; s = s->ts_next) {
		if (s->sflags & S_NL)
			break;
		switch (s->type) {
		case STAVES:
			for (staff = 0; staff <= nstaff; staff++) {
				sy->staff[staff].empty = empty[staff];
				empty[staff] = 1;
			}
			sy = sy->next;
			for (staff = 0; staff <= sy->nstaff; staff++) {
				p_staff = &staff_tb[staff];
				if (sy->staff[staff].clef.stafflines >= 0)
					p_staff->clef.stafflines = sy->staff[staff].clef.stafflines;
				if (sy->staff[staff].clef.staffscale != 0)
					p_staff->clef.staffscale = sy->staff[staff].clef.staffscale;
			}
			break;
		case GRACE:
			empty[s->staff] = 0;
			break;
		case NOTEREST:
		case SPACE:
		case MREST:
			if (cfmt.staffnonote > 1) {
				empty[s->staff] = 0;
			} else if (!(s->as.flags & ABC_F_INVIS)) {
				if (s->as.type == ABC_T_NOTE
				 || cfmt.staffnonote != 0)
					empty[s->staff] = 0;
			}
			break;
		}
	}
	tsnext = s;

	/* set the last empty staves and
	 * define the offsets of the measure bars */
	for (staff = 0; staff <= nstaff; staff++) {
		sy->staff[staff].empty = empty[staff];
		if (empty[staff])
			continue;

		p_staff = &staff_tb[staff];
		p_staff->botbar = p_staff->clef.stafflines <= 3 ? 6 : 0;
		switch (p_staff->clef.stafflines) {
		case 0:
		case 1:
		case 3:	p_staff->topbar = 18; break;
		case 2:	p_staff->topbar = 12; break;
		default:
			p_staff->topbar = 6 * (p_staff->clef.stafflines - 1);
			break;
		}
	}

	/* move the symbols of the empty staves to the next staff */
	sy = cursys;
	for (staff = 0; staff < nstaff; staff++) {
		if (sy->staff[staff].empty)
			sym_staff_move(staff, tsfirst, sy);
	}
	for (s = tsfirst; s; s = s->ts_next) {
		if (s->type == STAVES) {
			sy = sy->next;
			for (staff = 0; staff < nstaff; staff++) {
				if (sy->staff[staff].empty)
					sym_staff_move(staff, s, sy);
			}
		}
	}

//	/* let the last empty staff have a full height */
//	if (empty[nstaff])
//		staff_tb[nstaff].topbar = 0;

	/* initialize the music line */
	init_music_line();
	if (!empty[nstaff])
		insert_meter = 0;

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
	s = tsfirst;
	xmin = x = xmax = 0;
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
#if 0
	if (s->type == FMTCHG		/* if PS/SVG sequence at end of line */
	 && (s->u == PSSEQ || s-> == SVGSEQ)) {
		s->sflags &= ~S_SEQST;
		s->shrink = 0;
	}
#endif

	/* set max shrink and stretch */
	if (!cfmt.continueall)
		beta0 = BETA_X;
	else
		beta0 = BETA_C;

	/* memorize the glue for the last music line */
	if (tsnext) {
		if (x - width >= 0) {
			alfa_last = (x - width) / (x - xmin);	/* shrink */
			beta_last = 0;
		} else {
			alfa_last = 0;
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
			alfa = 1;
		} else {
			alfa = (x - width) / (x - xmin);	/* shrink */
			if (alfa > 1) {
				error(0, s,
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

//fixme:initmusic_line
//	init_music_line();	/* add the first symbols of the line */
//	insert_meter = 0;	/* not first line */
}
/* -- initialize the start of generation / new music line -- */
static void gen_init(void)
{
	struct SYMBOL *s;

	for (s = tsfirst ; s; s = s->ts_next) {
		if (s->extra)
			output_ps(s, ABC_S_HEAD);
		switch (s->type) {
		case CLEF:
		case KEYSIG:
		case TIMESIG:
			continue;
		case FMTCHG:
			if (s->extra)
				output_ps(s, 127);
			if (!s->extra) {
				unlksym(s);
				if (!tsfirst)
					return;
			}
//			continue;
			break;		/* may be Q: */
		case STAVES:
			cursys = cursys->next;
			unlksym(s);
			if (!tsfirst)
				return;
			continue;	/* fix %%staves - %%vskip */
//			break;
		}
		return;
	}
	tsfirst = NULL;			/* no more notes */
}

/* -- update the clefs at start of line -- */
static void update_clefs(void)
{
	struct SYMBOL *s;
	int staff;

	s = tsfirst;
	while (s && s->type == CLEF)
		s = s->ts_next;
	for ( ; s; s = s->ts_next) {
		if (s->type != CLEF)
			continue;
		staff = s->staff;
		cursys->staff[staff].clef.type = s->as.u.clef.type;
		cursys->staff[staff].clef.line = s->as.u.clef.line;
		cursys->staff[staff].clef.octave = s->as.u.clef.octave;
		cursys->staff[staff].clef.invis = s->as.u.clef.invis;
	}
}

/* -- show the errors -- */
static void error_show(void)
{
	struct SYMBOL *s;

	for (s = tsfirst; s; s = s->ts_next) {
		if (s->as.flags & ABC_F_ERROR) {
			putxy(s->x, staff_tb[s->staff].y + s->y);
			a2b("showerror\n");
		}
	}
}

/* -- delay output until the staves are defined (by draw_systems) -- */
static float delayed_output(float indent)
{
	float line_height;
	char *outbuf_sav, *mbf_sav, tmpbuf[20 * 1024];

	outbuf_sav = outbuf;
	mbf_sav = mbf;
	mbf = outbuf = tmpbuf;
	*outbuf = '\0';
	outft = -1;
	draw_sym_near();
	outbuf = outbuf_sav;
	mbf = mbf_sav;
	outft = -1;
	line_height = draw_systems(indent);
	a2b("%s", tmpbuf);
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
		if (cfmt.combinevoices >= 0)
			combine_voices();
		set_stem_dir();		/* set the stems direction in 'multi' */
	}
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next)
		set_beams(p_voice->sym);	/* decide on beams */
	set_stems();			/* set the stem lengths */
	if (first_voice->next) {		/* when multi-voices */
		set_rest_offset();	/* set the vertical offset of rests */
		set_overlap();		/* shift the notes on voice overlap */
	}
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
	alfa_last = 0.1;
	beta_last = 0;
	for (;;) {			/* loop per music line */
		float line_height;

//fixme:initmusic_line			<- pb repeat bars...
//	init_music_line();
		indent = set_indent();
		set_piece();
//fixme:initmusic_line			<- pb repeat bars...
//		init_music_line();

//fixme:initmusic_line
//	insert_meter = 0;
		set_sym_glue(lwidth - indent);
		if (indent != 0)
			a2b("%.2f 0 T\n", indent); /* do indentation */
		line_height = delayed_output(indent);
		draw_all_symb();
		draw_all_deco();
		if (showerror)
			error_show();
		bskip(line_height);
		if (indent != 0)
			a2b("%.2f 0 T\n", -indent);
		update_clefs();
		tsfirst = tsnext;
		gen_init();
		if (!tsfirst)
			break;
		buffer_eob();
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
