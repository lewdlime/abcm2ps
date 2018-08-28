/*
 * Drawing functions.
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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "abcm2ps.h"

struct BEAM {			/* packages info on one beam */
	struct SYMBOL *s1, *s2;
	float a, b;
	short nflags;
};

static char *acc_tb[] = { "", "sh", "nt", "ft", "dsh", "dft" };

/* scaling stuff */
static int scale_voice;		/* staff (0) or voice(1) scaling */
static float cur_scale = 1;	/* voice or staff scale */
static float cur_trans = 0;	/* != 0 when scaled staff */
static float cur_staff = 1;	/* current scaled staff */
static int cur_color = 0;	/* current color */

static void draw_note(float x,
		      struct SYMBOL *s,
		      int fl);
static void set_tie_room(void);

// set the symbol color


/* set the voice color */
void set_color(int new_color)
{
	if (new_color == cur_color)
		return;
	cur_color = new_color;
	a2b("%.2f %.2f %.2f setrgbcolor ",
		(float) (cur_color >> 16) / 255,
		(float) ((cur_color >> 8) & 0xff) / 255,
		(float) (cur_color & 0xff) / 255);
}

/* output debug annotations */
static void anno_out(struct SYMBOL *s, char type)
{
	if (s->linenum == 0)
		return;
	if (mbf[-1] != '\n')
		*mbf++ = '\n';
	a2b("%%A %c %d %d ", type, s->linenum, s->colnum);
	putxy(s->x - s->wl - 2, staff_tb[s->staff].y + s->ymn - 2);
	if (type != 'b' && type != 'e')		/* if not beam */
		a2b("%.1f %d", s->wl + s->wr + 4, s->ymx - s->ymn + 4);
	a2b("\n");
}

/* -- up/down shift needed to get k*6 -- */
static float rnd6(float y)
{
	int iy;

	iy = ((int) (y + 2.999) + 12) / 6 * 6 - 12;
	return iy - y;
}

/* -- compute the best vertical offset for the beams -- */
static float b_pos(int grace,
		   int stem,
		   int flags,
		   float b)
{
	float d1, d2, shift, depth;
	float top, bot;

	shift = !grace ? BEAM_SHIFT : 3.5;
	depth = !grace ? BEAM_DEPTH : 1.8;
	if (stem > 0) {
		bot = b - (flags - 1) * shift - depth;
		if (bot > 26)
			return 0;
		top = b;
	} else {
		top = b + (flags - 1) * shift + depth;
		if (top < -2)
			return 0;
		bot = b;
	}

	d1 = rnd6(top - BEAM_OFFSET);
	d2 = rnd6(bot + BEAM_OFFSET);
	if (d1 * d1 > d2 * d2)
		return d2;
	return d1;
}

/* duplicate a note for beaming continuation */
static struct SYMBOL *sym_dup(struct SYMBOL *s_orig)
{
	struct SYMBOL *s;
	int m;

	s = (struct SYMBOL *) getarena(sizeof *s);
	memcpy(s, s_orig, sizeof *s);
	s->flags |= ABC_F_INVIS;
	s->text = NULL;
	for (m = 0; m <= s->nhd; m++)
		s->u.note.notes[m].sl1 = 0;
	memset(&s->u.note.dc, 0, sizeof s->u.note.dc);

	s->gch = NULL;
	s->ly = NULL;
	return s;
}

/* -- calculate a beam -- */
/* (the staves may be defined or not) */
static int calculate_beam(struct BEAM *bm,
			  struct SYMBOL *s1)
{
	struct SYMBOL *s, *s2;
	int notes, nflags, staff, voice, two_staves, two_dir, visible = 0;
	float x, y, ys, a, b, max_stem_err;
	float sx, sy, sxx, sxy, syy, a0, stem_xoff, scale;
	static float min_tb[2][6] = {
		{STEM_MIN, STEM_MIN,
			STEM_MIN2, STEM_MIN3, STEM_MIN4, STEM_MIN4},
		{STEM_CH_MIN, STEM_CH_MIN,
			STEM_CH_MIN2, STEM_CH_MIN3, STEM_CH_MIN4, STEM_CH_MIN4}
	};

	if (!(s1->sflags & S_BEAM_ST)) {	/* beam from previous music line */
		s = sym_dup(s1);
		s->prev = s1->prev;
		if (s->prev)
			s->prev->next = s;
		else
			voice_tb[s->voice].sym = s;
		s1->prev = s;
		s->next = s1;
		s->ts_prev = s1->ts_prev;
//		if (s->ts_prev)
			s->ts_prev->ts_next = s;
		s1->ts_prev = s;
		s->ts_next = s1;
		for (s2 = s->ts_prev; /*s2*/; s2 = s2->ts_prev) {
			switch (s2->type) {
			default:
				continue;
			case CLEF:
			case KEYSIG:
			case TIMESIG:
				break;
			}
			break;
		}
		s->x -= 12;
		if (s->x > s2->x + 12)
			s->x = s2->x + 12;
		s->sflags &= S_SEQST;
		s->sflags |= S_BEAM_ST | S_TEMP;
		s->u.note.slur_st = 0;
		s->u.note.slur_end = 0;
		s1 = s;
	}

	/* search last note in beam */
	notes = nflags = 0;	/* set x positions, count notes and flags */
	two_staves = two_dir = 0;
	staff = s1->staff;
	voice = s1->voice;
	stem_xoff = (s1->flags & ABC_F_GRACE) ? GSTEM_XOFF : s1->u.note.sdx;
	for (s2 = s1; ; s2 = s2->next) {
		if (s2->abc_type == ABC_T_NOTE) {
			if (s2->nflags > nflags)
				nflags = s2->nflags;
			notes++;
			if (s2->staff != staff)
				two_staves = 1;
			if (s2->stem != s1->stem)
				two_dir = 1;
			if (!visible
			 && !(s2->flags & ABC_F_INVIS)
			 && (!(s2->flags & ABC_F_STEMLESS)
			  || (s2->sflags & S_TREM2)))
				visible = 1;
			if (s2->sflags & S_BEAM_END)
				break;
		}
		if (!s2->next) {		/* beam towards next music line */
			for (; ; s2 = s2->prev) {
				if (s2->abc_type == ABC_T_NOTE)
					break;
			}
			s = sym_dup(s2);
			s->next = s2->next;
			if (s->next)
				s->next->prev = s;
			s2->next = s;
			s->prev = s2;
			s->ts_next = s2->ts_next;
			if (s->ts_next)
				s->ts_next->ts_prev = s;
			s2->ts_next = s;
			s->ts_prev = s2;
			s->sflags &= S_SEQST;
			s->sflags |= S_BEAM_END | S_TEMP;
			s->u.note.slur_st = 0;
			s->u.note.slur_end = 0;
			s->x += 12;
			if (s->x < realwidth - 12)
				s->x = realwidth - 12;
			s2 = s;
			notes++;
			break;
		}
	}
	if (!visible)
		return 0;

	bm->s2 = s2;			/* (don't display the flags) */
	if (staff_tb[staff].y == 0) {	/* staves not defined */
		if (two_staves)
			return 0;
	} else {			/* staves defined */
		if (!two_staves) {
			bm->s1 = s1;	/* beam already calculated */
if (s1->xs == s2->xs)
 bug("beam with null length", 1);
			bm->a = (s1->ys- s2->ys) / (s1->xs - s2->xs);
			bm->b = s1->ys - s1->xs * bm->a
				+ staff_tb[staff].y;
			bm->nflags = nflags;
			return 1;
		}
	}

	sx = sy = sxx = sxy = syy = 0;	/* linear fit through stem ends */
	for (s = s1; ; s = s->next) {
		if (s->abc_type != ABC_T_NOTE)
			continue;
		if ((scale = voice_tb[s->voice].scale) == 1)
			scale = staff_tb[s->staff].staffscale;
		if (s->stem >= 0)
			x = stem_xoff + s->u.note.notes[0].shhd;
		else
			x = -stem_xoff + s->u.note.notes[s->nhd].shhd;
		x *= scale;
		x += s->x;
		s->xs = x;
		y = s->ys + staff_tb[s->staff].y;
		sx += x; sy += y;
		sxx += x * x; sxy += x * y; syy += y * y;
		if (s == s2)
			break;
	}

	/* beam fct: y=ax+b */
	a = (sxy * notes - sx * sy) / (sxx * notes - sx * sx);
	b = (sy - a * sx) / notes;

	/* the next few lines modify the slope of the beam */
	if (!(s1->flags & ABC_F_GRACE)) {
		if (notes >= 3) {
			float hh;

			hh = syy - a * sxy - b * sy;	/* flatten if notes not in line */
			if (hh > 0
			 && hh / (notes - 2) > .5)
				a *= BEAM_FLATFAC;
		}
		if (a >= 0)
			a = BEAM_SLOPE * a / (BEAM_SLOPE + a);	/* max steepness for beam */
		else
			a = BEAM_SLOPE * a / (BEAM_SLOPE - a);
	} else {
		if (a > BEAM_SLOPE)
			a = BEAM_SLOPE;
		else if (a < -BEAM_SLOPE)
			a = -BEAM_SLOPE;
	}

	/* to decide if to draw flat etc. use normalized slope a0 */
	a0 = a * (s2->xs - s1->xs) / (20 * (notes - 1));

	if (a0 * a0 < BEAM_THRESH * BEAM_THRESH)
		a = 0;			/* flat below threshhold */

	b = (sy - a * sx) / notes;	/* recalculate b for new slope */

/*  if (nflags>1) b=b+2*stem;*/	/* leave a bit more room if several beams */

	/* have flat beams when asked */
	if (cfmt.flatbeams) {
		if (!(s1->flags & ABC_F_GRACE))
			b = -11 + staff_tb[staff].y;
		else
			b = 35 + staff_tb[staff].y;
		a = 0;
	}

/*fixme: have a look again*/
	/* have room for the symbols in the staff */
	max_stem_err = 0;		/* check stem lengths */
	s = s1;
	if (two_dir) {				/* 2 directions */
/*fixme: more to do*/
		if (!(s1->flags & ABC_F_GRACE))
			ys = BEAM_SHIFT;
		else
			ys = 3.5;
		ys *= (nflags - 1);
		ys += BEAM_DEPTH;
		ys *= .5;
		if (s1->stem != s2->stem && s1->nflags < s2->nflags)
			ys *= s2->stem;
		else
			ys *= s1->stem;
		b += ys;
	} else if (!(s1->flags & ABC_F_GRACE)) {	/* normal notes */
		float stem_err, beam_h;

		beam_h = BEAM_DEPTH + BEAM_SHIFT * (nflags - 1);
		while (s->ts_prev->abc_type == ABC_T_NOTE
		    && s->ts_prev->time == s->time
		    && s->ts_prev->x > s1->xs)
			s = s->ts_prev;

		for (; s && s->time <= s2->time; s = s->ts_next) {
			if (s->abc_type != ABC_T_NOTE
			 || (s->flags & ABC_F_INVIS)
			 || (s->staff != staff
			  && s->voice != voice)) {
				continue;
			}
			x = s->voice == voice ? s->xs : s->x;
			ys = a * x + b - staff_tb[s->staff].y;
			if (s->voice == voice) {
				if (s->nhd == 0)
					stem_err = min_tb[0][(unsigned) s->nflags];
				else
					stem_err = min_tb[1][(unsigned) s->nflags];
				if (s->stem > 0) {
					if (s->pits[s->nhd] > 26) {
						stem_err -= 2;
						if (s->pits[s->nhd] > 28)
							stem_err -= 2;
					}
					stem_err -= ys - (float) (3 *
						(s->pits[s->nhd] - 18));
				} else {
					if (s->pits[0] < 18) {
						stem_err -= 2;
						if (s->pits[0] < 16)
							stem_err -= 2;
					}
					stem_err -= (float) (3 *
						(s->pits[0] - 18)) - ys;
				}
				stem_err += BEAM_DEPTH + BEAM_SHIFT * (s->nflags - 1);
			} else {
/*fixme: KO when two_staves*/
				if (s1->stem > 0) {
					if (s->stem > 0) {
/*fixme: KO when the voice numbers are inverted*/
						if (s->ymn > ys + 4
						 || s->ymx < ys - beam_h - 2)
							continue;
						if (s->voice > voice)
							stem_err = s->ymx - ys;
						else
							stem_err = s->ymn + 8 - ys;
					} else {
						stem_err = s->ymx - ys;
					}
				} else {
					if (s->stem < 0) {
						if (s->ymx < ys - 4
						 || s->ymn > ys - beam_h - 2)
							continue;
						if (s->voice < voice)
							stem_err = ys - s->ymn;
						else
							stem_err = ys - s->ymx + 8;
					} else {
						stem_err = ys - s->ymn;
					}
				}
				stem_err += 2 + beam_h;
			}
			if (stem_err > max_stem_err)
				max_stem_err = stem_err;
		}
	} else {				/* grace notes */
		for ( ; ; s = s->next) {
			float stem_err;

			ys = a * s->xs + b - staff_tb[s->staff].y;
			stem_err = GSTEM - 2;
			if (s->stem > 0)
				stem_err -= ys - (float) (3 *
					(s->pits[s->nhd] - 18));
			else
				stem_err += ys - (float) (3 *
					(s->pits[0] - 18));
			stem_err += 3 * (s->nflags - 1);
			if (stem_err > max_stem_err)
				max_stem_err = stem_err;
			if (s == s2)
				break;
		}
	}

	if (max_stem_err > 0)		/* shift beam if stems too short */
		b += s1->stem * max_stem_err;

	/* have room for the gracenotes, bars and clefs */
/*fixme: test*/
    if (!two_staves && !two_dir)
	for (s = s1->next; ; s = s->next) {
		struct SYMBOL *g;

		switch (s->type) {
		case NOTEREST:		/* cannot move rests in multi-voices */
			if (s->abc_type != ABC_T_REST)
				break;
			g = s->ts_next;
			if (!g || g->staff != staff
			 || g->type != NOTEREST)
				break;
//fixme:too much vertical shift if some space above the note
//fixme:this does not fix rest under beam in second voice (ts_prev)
			/*fall thru*/
		case BAR:
#if 1
			if (s->flags & ABC_F_INVIS)
#else
//??
			if (!(s->flags & ABC_F_INVIS))
#endif
				break;
			/*fall thru*/
		case CLEF:
			y = a * s->x + b;
			if (s1->stem > 0) {
				y = s->ymx - y
					+ BEAM_DEPTH + BEAM_SHIFT * (nflags - 1)
					+ 2;
				if (y > 0)
					b += y;
			} else {
				y = s->ymn - y
					- BEAM_DEPTH - BEAM_SHIFT * (nflags - 1)
					- 2;
				if (y < 0)
					b += y;
			}
			break;
		case GRACE:
			g = s->extra;
			for ( ; g; g = g->next) {
				if (g->type != NOTEREST)
					continue;
				y = a * g->x + b;
				if (s1->stem > 0) {
					y = g->ymx - y
						+ BEAM_DEPTH + BEAM_SHIFT * (nflags - 1)
						+ 2;
					if (y > 0)
						b += y;
				} else {
					y = g->ymn - y
						- BEAM_DEPTH - BEAM_SHIFT * (nflags - 1)
						- 2;
					if (y < 0)
						b += y;
				}
			}
			break;
		}
		if (s == s2)
			break;
	}

	if (a == 0)		/* shift flat beams onto staff lines */
		b += b_pos(s1->flags & ABC_F_GRACE, s1->stem, nflags,
				b - staff_tb[staff].y);

	/* adjust final stems and rests under beam */
	for (s = s1; ; s = s->next) {
		float dy;

		switch (s->abc_type) {
		case ABC_T_NOTE:
			s->ys = a * s->xs + b - staff_tb[s->staff].y;
			if (s->stem > 0) {
				s->ymx = s->ys + 2.5;
#if 0
//fixme: hack
				if (s->ts_prev
				 && s->ts_prev->stem > 0
				 && s->ts_prev->staff == s->staff
				 && s->ts_prev->ymn < s->ymx
				 && s->ts_prev->x == s->x
				 && s->u.note.notes[0].shhd == 0) {
					s->ts_prev->x -= 5;	/* fix stem clash */
					s->ts_prev->xs -= 5;
				}
#endif
			} else {
				s->ymn = s->ys - 2.5;
			}
			break;
		case ABC_T_REST:
			y = a * s->x + b - staff_tb[s->staff].y;
			dy = BEAM_DEPTH + BEAM_SHIFT * (nflags - 1)
				+ (s->head != H_FULL ? 4 : 9);
			if (s1->stem > 0) {
				y -= dy;
				if (s1->multi == 0 && y > 12)
					y = 12;
				if (s->y <= y)
					break;
			} else {
				y += dy;
				if (s1->multi == 0 && y < 12)
					y = 12;
				if (s->y >= y)
					break;
			}
			if (s->head != H_FULL) {
				int iy;

				iy = ((int) y + 3 + 12) / 6 * 6 - 12;
				y = iy;
			}
			s->y = y;
			break;
		}
		if (s == s2)
			break;
	}

	/* save beam parameters */
	if (staff_tb[staff].y == 0)	/* if staves not defined */
		return 0;
	bm->s1 = s1;
	bm->a = a;
	bm->b = b;
	bm->nflags = nflags;
	return 1;
}

/* -- draw a single beam -- */
/* (the staves are defined) */
static void draw_beam(float x1,
		      float x2,
		      float dy,
		      float h,
		      struct BEAM *bm,
		      int n)			/* beam number (1..n) */
{
	struct SYMBOL *s;
	float y1, dy2;

	s = bm->s1;
	if ((s->sflags & S_TREM2) && n > s->nflags - s->aux
	 && s->head != H_EMPTY) {
		if (s->head >= H_OVAL) {
			x1 = s->x + 6;
			x2 = bm->s2->x - 6;
		} else {
			x1 += 5;
			x2 -= 6;
		}
	}

	y1 = bm->a * x1 + bm->b - dy;
	x2 -= x1;
	dy2 = bm->a * x2;

	putf(h);
	putx(x2);
	putf(dy2);
	putxy(x1, y1);
	a2b("bm\n");
}

/* -- draw the beams for one word -- */
/* (the staves are defined) */
static void draw_beams(struct BEAM *bm)
{
	struct SYMBOL *s, *s1, *s2;
	int i, beam_dir;
	float shift, bshift, bstub, bh, da;

	s1 = bm->s1;
/*fixme: KO if many staves with different scales*/
//fixme: useless?
//	set_scale(s1);
	s2 = bm->s2;
	if (!(s1->flags & ABC_F_GRACE)) {
		bshift = BEAM_SHIFT;
		bstub = BEAM_STUB;
		shift = .34;		/* (half width of the stem) */
		bh = BEAM_DEPTH;
	} else {
		bshift = 3.5;
		bstub = 3.2;
		shift = .29;
		bh = 1.8;
	}

/*fixme: quick hack for stubs at end of beam and different stem directions*/
	beam_dir = s1->stem;
	if (s1->stem != s2->stem
	 && s1->nflags < s2->nflags)
		beam_dir = s2->stem;
	if (beam_dir < 0)
		bh = -bh;
	if (cur_trans == 0 && cur_scale != 1) {
		bm->a /= cur_scale;
		bm->b = s1->ys - s1->xs * bm->a
			+ staff_tb[s1->staff].y;
		bshift *= cur_scale;
	}

	/* make first beam over whole word and adjust the stem lengths */
	draw_beam(s1->xs - shift, s2->xs + shift, 0., bh, bm, 1);
	da = 0;
	for (s = s1; ; s = s->next) {
		if (s->abc_type == ABC_T_NOTE
		 && s->stem != beam_dir)
			s->ys = bm->a * s->xs + bm->b
				- staff_tb[s->staff].y
				+ bshift * (s->nflags - 1) * s->stem
				- bh;
		if (s == s2)
			break;
	}

	if (s1->sflags & S_FEATHERED_BEAM) {
		da = bshift / (s2->xs - s1->xs);
		if (s1->dur > s2->dur) {
			da = -da;
			bshift = da * s1->xs;
		} else {
			bshift = da * s2->xs;
		}
		da = da * beam_dir;
	}

	/* other beams with two or more flags */
	shift = 0;
	for (i = 2; i <= bm->nflags; i++) {
		shift += bshift;
		if (da != 0)
			bm->a += da;
		for (s = s1; ; s = s->next) {
			struct SYMBOL *k1, *k2;
			float x1;

			if (s->abc_type != ABC_T_NOTE
			 || s->nflags < i) {
				if (s == s2)
					break;
				continue;
			}
			if ((s->sflags & S_TREM1)
			 && i > s->nflags - s->aux) {
				if (s->head >= H_OVAL)
					x1 = s->x;
				else
					x1 = s->xs;
				draw_beam(x1 - 5, x1 + 5,
					  (shift + 2.5) * beam_dir,
					  bh, bm, i);
				if (s == s2)
					break;
				continue;
			}
			k1 = s;
			for (;;) {
				if (s == s2)
					break;
				if (s->next->type == NOTEREST) {
					if (s->next->sflags & S_TREM1) {
						if (s->next->nflags - s->next->aux < i)
							break;
					} else if (s->next->nflags < i) {
						break;
					}
				}
				if ((s->next->sflags & S_BEAM_BR1)
				 || ((s->next->sflags & S_BEAM_BR2) && i > 2))
					break;
				s = s->next;
			}
			k2 = s;
			while (k2->abc_type != ABC_T_NOTE)
				k2 = k2->prev;
			x1 = k1->xs;
			if (k1 == k2) {
				if (k1 == s1) {
					x1 += bstub;
				} else if (k1 == s2) {
					x1 -= bstub;
				} else if ((k1->sflags & S_BEAM_BR1)
					|| ((k1->sflags & S_BEAM_BR2)
					 && i > 2)) {
					x1 += bstub;
				} else {
					struct SYMBOL *k;

					k = k1->next;
					while (k->abc_type != ABC_T_NOTE)
						k = k->next;
					if ((k->sflags & S_BEAM_BR1)
					 || ((k->sflags & S_BEAM_BR2)
					  && i > 2)) {
						x1 -= bstub;
					} else {
						k1 = k1->prev;
						while (k1->abc_type != ABC_T_NOTE)
							k1 = k1->prev;
						if (k1->nflags < k->nflags
						 || (k1->nflags == k->nflags
						  && k1->dots < k->dots))
							x1 += bstub;
						else
							x1 -= bstub;
					}
				}
			}
			draw_beam(x1, k2->xs,
#if 1
				  shift * beam_dir,
#else
				  shift * k1->stem,	/*fixme: more complicated */
#endif
				  bh, bm, i);
			if (s == s2)
				break;
		}
	}
	if (s1->sflags & S_TEMP)
		unlksym(s1);
	else if (s2->sflags & S_TEMP)
		unlksym(s2);
}

/* -- draw a system brace or bracket -- */
static void draw_sysbra(float x, int staff, int flag)
{
	int i, end;
	float yt, yb;

	while (cursys->staff[staff].empty) {
//	    || staff_tb[staff].stafflines == 0) {
		if (cursys->staff[staff].flags & flag)
			return;
		staff++;
	}
	i = end = staff;
	for (;;) {
		if (!cursys->staff[i].empty)
//		 && staff_tb[i].stafflines != 0)
			end = i;
		if (cursys->staff[i].flags & flag)
			break;
		i++;
	}
	yt = staff_tb[staff].y + staff_tb[staff].topbar
				* staff_tb[staff].staffscale;
	yb = staff_tb[end].y + staff_tb[end].botbar
				* staff_tb[end].staffscale;
	a2b("%.1f %.1f %.1f %s\n",
	     yt - yb, x, yt,
	     (flag & (CLOSE_BRACE | CLOSE_BRACE2)) ? "brace" : "bracket");
}

/* -- draw the left side of the staves -- */
static void draw_lstaff(float x)
{
	int i, j, l, nst;
	float yb;

	if (cfmt.alignbars)
		return;
	nst = cursys->nstaff;
	l = 0;
	for (i = 0; ; i++) {
		if (cursys->staff[i].flags & (OPEN_BRACE | OPEN_BRACKET))
			l++;
		if (!cursys->staff[i].empty)
//		 && staff_tb[i].stafflines != 0)
			break;
		if (cursys->staff[i].flags & (CLOSE_BRACE | CLOSE_BRACKET))
			l--;
		if (i == nst)
			break;
	}
	for (j = nst; j > i; j--) {
		if (!cursys->staff[j].empty)
//		 && staff_tb[j].stafflines != 0)
			break;
	}
	if (i == j && l == 0)
		return;
	set_sscale(-1);
	yb = staff_tb[j].y + staff_tb[j].botbar
				* staff_tb[j].staffscale;
	a2b("%.1f %.1f %.1f bar\n",
	     staff_tb[i].y + staff_tb[i].topbar * staff_tb[i].staffscale - yb,
	     x, yb);
	for (i = 0; i <= nst; i++) {
		if (cursys->staff[i].flags & OPEN_BRACE)
			draw_sysbra(x, i, CLOSE_BRACE);
		if (cursys->staff[i].flags & OPEN_BRACKET)
			draw_sysbra(x, i, CLOSE_BRACKET);
		if (cursys->staff[i].flags & OPEN_BRACE2)
			draw_sysbra(x - 6, i, CLOSE_BRACE2);
		if (cursys->staff[i].flags & OPEN_BRACKET2)
			draw_sysbra(x - 6, i, CLOSE_BRACKET2);
	}
}

/* -- draw a staff -- */
static void draw_staff(int staff,
			float x1, float x2)
{
	char *stafflines;
	int i, l, thick = -1;
	float y, w;

	/* draw the staff */
	set_sscale(staff);
	y = staff_tb[staff].y;
	stafflines = staff_tb[staff].stafflines;
	l = strlen(stafflines);
	for (i = 0; i < l; i++) {
		if (stafflines[i] != '.') {
			w = x2 - x1;
			for ( ; i < l; i++) {
				if (stafflines[i] != '.') {
					if (stafflines[i] != '|') {
						if (thick != 1) {
							if (thick >= 0)
								a2b("stroke\n");
							a2b("1.5 SLW ");
							thick = 1;
						}
					} else {
						if (thick != 0) {
							if (thick >= 0)
								a2b("stroke\n");
							a2b("dlw ");
							thick = 0;
						}
					}
					putx(w);
					putxy(x1, y);
					a2b("M 0 RL ");
				}
				y += 6;
			}
			a2b("stroke\n");
			break;
		}
		y += 6;
	}
}

/* -- draw the time signature -- */
static void draw_timesig(float x,
			 struct SYMBOL *s)
{
	unsigned i, staff, l, l2;
	char *f, meter[64];
	float dx, y;

	if (s->u.meter.nmeter == 0)
		return;
	staff = s->staff;
	x -= s->wl;
	y = staff_tb[staff].y;
	for (i = 0; i < s->u.meter.nmeter; i++) {
		l = strlen(s->u.meter.meter[i].top);
		if (l > sizeof s->u.meter.meter[i].top)
			l = sizeof s->u.meter.meter[i].top;
		if (s->u.meter.meter[i].bot[0] != '\0') {
			sprintf(meter, "(%.8s)(%.2s)",
				s->u.meter.meter[i].top,
				s->u.meter.meter[i].bot);
			f = "tsig";
			l2 = strlen(s->u.meter.meter[i].bot);
			if (l2 > sizeof s->u.meter.meter[i].bot)
				l2 = sizeof s->u.meter.meter[i].bot;
			if (l2 > l)
				l = l2;
		} else switch (s->u.meter.meter[i].top[0]) {
			case 'C':
				if (s->u.meter.meter[i].top[1] != '|') {
					f = "csig";
				} else {
					f = "ctsig";
					l--;
				}
				meter[0] = '\0';
				x -= 5;
				y += 12;
				break;
			case 'c':
				if (s->u.meter.meter[i].top[1] != '.') {
					f = "imsig";
				} else {
					f = "iMsig";
					l--;
				}
				meter[0] = '\0';
				break;
			case 'o':
				if (s->u.meter.meter[i].top[1] != '.') {
					f = "pmsig";
				} else {
					f = "pMsig";
					l--;
				}
				meter[0] = '\0';
				break;
			case '(':
			case ')':
				sprintf(meter, "(\\%s)",
					s->u.meter.meter[i].top);
				f = "stsig";
				break;
			default:
				sprintf(meter, "(%.8s)",
					s->u.meter.meter[i].top);
				f = "stsig";
				break;
		}
		if (meter[0] != '\0')
			a2b("%s ", meter);
		dx = (float) (13 * l);
		putxy(x + dx * .5, y);
		a2b("%s\n", f);
		x += dx;
	}
}

/* -- draw an accidental -- */
static void draw_acc(int acc, int microscale)
{
	int n, d;

	n = parse.micro_tb[acc >> 3];
	if (acc >> 3 != 0 && microscale) {
		if (microscale) {
			d = microscale;
			n = acc >> 3;
		} else {
			d = ((n & 0xff) + 1) * 2;
			n = (n >> 8) + 1;
		}
		a2b("%d %s%d ", n, acc_tb[acc & 0x07], d);
	} else {
		a2b("%s%d ", acc_tb[acc & 0x07], n);
	}
}

// draw helper lines
static void draw_hl(float x, float staffb, int up,
		int y, char *stafflines, char *hltype)
{
	int i, l;

	l = strlen(stafflines);

	// lower ledger lines
	if (!up) {
		for (i = 0; i < l - 1; i++) {
			if (stafflines[i] != '.')
				break;
		}
		i = i * 6 - 6;
		for ( ; i >= y; i -= 6) {
			putxy(x, staffb + i);
			a2b("%s ", hltype);
		}
		return;
	}

	// upper ledger lines
	i = l * 6;
	for ( ; i <= y; i += 6) {
		putxy(x, staffb + i);
		a2b("%s ", hltype);
	}
}

/* -- draw a key signature -- */
static void draw_keysig(struct VOICE_S *p_voice,
			float x,
			struct SYMBOL *s)
{
	int old_sf = s->aux;
	int staff = p_voice->staff;
	float staffb = staff_tb[staff].y;
	int i, clef_ix, shift;
	const signed char *p_seq;

	static const char sharp_cl[] = {24, 9, 15, 21, 6, 12, 18};
	static const char flat_cl[] = {12, 18, 24, 9, 15, 21, 6};
	// (the ending 0 is needed to avoid array overflow)
	static const signed char sharp1[] = {-9, 12, -9, -9, 12, -9, 0};
	static const signed char sharp2[] = {12, -9, 12, -9, 12, -9, 0};
	static const signed char flat1[] = {9, -12, 9, -12, 9, -12, 0};
	static const signed char flat2[] = {-12, 9, -12, 9, -12, 9, 0};

	clef_ix = s->u.key.clef_delta;
	if (clef_ix & 1)
		clef_ix += 7;
	clef_ix /= 2;
	while (clef_ix < 0)
		clef_ix += 7;
	clef_ix %= 7;

	/* normal accidentals */
	if (s->u.key.nacc == 0 && !s->u.key.empty) {

		/* put neutrals if 'accidental cancel' */
		if (cfmt.cancelkey || s->u.key.sf == 0) {

			/* when flats to sharps, or sharps to flats, */
			if (s->u.key.sf == 0
			 || old_sf * s->u.key.sf < 0) {

				/* old sharps */
				shift = sharp_cl[clef_ix];
				p_seq = shift > 9 ? sharp1 : sharp2;
				for (i = 0; i < old_sf; i++) {
					putxy(x, staffb + shift);
					a2b("nt0 ");
					shift += *p_seq++;
					x += 5.5;
				}

				/* old flats */
				shift = flat_cl[clef_ix];
				p_seq = shift < 18 ? flat1 : flat2;
				for (i = 0; i > old_sf; i--) {
					putxy(x, staffb + shift);
					a2b("nt0 ");
					shift += *p_seq++;
					x += 5.5;
				}
				if (s->u.key.sf != 0)
					x += 3;		/* extra space */
			}
		}

		/* new sharps */
		if (s->u.key.sf > 0) {
			shift = sharp_cl[clef_ix];
			p_seq = shift > 9 ? sharp1 : sharp2;
			for (i = 0; i < s->u.key.sf; i++) {
				putxy(x, staffb + shift);
				a2b("sh0 ");
				shift += *p_seq++;
				x += 5.5;
			}
			if (cfmt.cancelkey && s->u.key.sf < old_sf) {
				x += 2;
				for (; i < old_sf; i++) {
					putxy(x, staffb + shift);
					a2b("nt0 ");
					shift += *p_seq++;
					x += 5.5;
				}
			}
		}

		/* new flats */
		if (s->u.key.sf < 0) {
			shift = flat_cl[clef_ix];
			p_seq = shift < 18 ? flat1 : flat2;
			for (i = 0; i > s->u.key.sf; i--) {
				putxy(x, staffb + shift);
				a2b("ft0 ");
				shift += *p_seq++;
				x += 5.5;
			}
			if (cfmt.cancelkey && s->u.key.sf > old_sf) {
				x += 2;
				for (; i > old_sf; i--) {
					putxy(x, staffb + shift);
					a2b("nt0 ");
					shift += *p_seq++;
					x += 5.5;
				}
			}
		}
	} else {
		int acc, last_acc, last_shift;

		/* explicit accidentals */
		last_acc = s->u.key.accs[0];
		last_shift = 100;
		for (i = 0; i < s->u.key.nacc; i++) {
			acc = s->u.key.accs[i];
			shift = s->u.key.clef_delta * 3	// clef shift
				+ 3 * (s->u.key.pits[i] - 18);
			if (i != 0
			 && (shift > last_shift + 18
			  || shift < last_shift - 18))
				x -= 5.5;		// no clash
			else if (acc != last_acc)
				x += 3;
			last_acc = acc;
			if (shift < 0)
				draw_hl(x, staffb, 0,
					shift,		/* lower ledger line */
					staff_tb[s->staff].stafflines, "hl");
			else if (shift > 24)
				draw_hl(x, staffb, 1,
					shift,		/* upper ledger line */
					staff_tb[s->staff].stafflines, "hl");
			last_shift = shift;
			putxy(x, staffb + shift);
			draw_acc(acc, s->u.key.microscale);
			x += 5.5;
		}
	}
	if (old_sf != 0 || s->u.key.sf != 0 || s->u.key.nacc != 0)
		a2b("\n");
}

/* -- convert the standard measure bars -- */
static int bar_cnv(int bar_type)
{
	switch (bar_type) {
	case B_OBRA:
/*	case B_CBRA: */
	case (B_OBRA << 4) + B_CBRA:
		return 0;			/* invisible */
//	case B_COL:
//		return B_BAR;			/* dotted */
#if 0
	case (B_CBRA << 4) + B_BAR:
		return B_BAR;
#endif
	case (B_SINGLE << 8) | B_LREP:
	case (B_BAR << 4) + B_COL:
		bar_type |= (B_OBRA << 8);		/* ||: and |: -> [|: */
		break;
	case (B_BAR << 8) + (B_COL << 4) + B_COL:
		bar_type |= (B_OBRA << 12);		/* |:: -> [|:: */
		break;
	case (B_BAR << 12) + (B_COL << 8) + (B_COL << 4) + B_COL:
		bar_type |= (B_OBRA << 16);		/* |::: -> [|::: */
		break;
	case (B_COL << 4) + B_BAR:
	case (B_COL << 8) + (B_COL << 4) + B_BAR:
	case (B_COL << 12) + (B_COL << 8) + (B_COL << 4) + B_BAR:
		bar_type <<= 4;
		bar_type |= B_CBRA;			/* :..| -> :..|] */
		break;
	case (B_COL << 4) + B_COL:
		bar_type = cfmt.dblrepbar;		/* :: -> dble repeat bar */
		break;
	}
	return bar_type;
}

/* -- draw a measure bar -- */
static void draw_bar(struct SYMBOL *s, float bot, float h)
{
	int staff, bar_type;
	float x, yb;
	char *psf;

	staff = s->staff;
	yb = staff_tb[staff].y;
	x = s->x;

	/* if measure repeat, draw the '%' like glyphs */
	if (s->u.bar.len != 0) {
		struct SYMBOL *s2;

		set_scale(s);
		if (s->u.bar.len == 1) {
			for (s2 = s->prev; s2->abc_type != ABC_T_REST; s2 = s2->prev)
				;
			putxy(s2->x, yb + 12);
			a2b("mrep\n");
		} else {
			putxy(x, yb + 12);
			a2b("mrep2\n");
			if (s->voice == cursys->top_voice) {
/*fixme				set_font(s->gcf); */
				set_font(cfmt.anf);
				putxy(x, yb + staff_tb[staff].topbar + 4);
				a2b("M(%d)showc\n", s->u.bar.len);
			}
		}
	}

	/* don't put a line between the staves if there is no bar above */
	if (staff != 0
	 && s->ts_prev
	 && (s->ts_prev->type != BAR || s->ts_prev->staff != staff - 1))
		h = staff_tb[staff].topbar * staff_tb[staff].staffscale;

	bar_type = bar_cnv(s->u.bar.type);
	if (bar_type == 0)
		return;				/* invisible */
	for (;;) {
		psf = "bar";
		switch (bar_type & 0x07) {
		case B_BAR:
			if (s->u.bar.dotted)
				psf = "dotbar";
			break;
		case B_OBRA:
		case B_CBRA:
			psf = "thbar";
			x -= 3;
			break;
		case B_COL:
			x -= 2;
			break;
		}
		switch (bar_type & 0x07) {
		default:
			set_sscale(-1);
			a2b("%.1f %.1f %.1f %s ", h, x, bot, psf);
			break;
		case B_COL:
			set_sscale(staff);
			putxy(x + 1, staff_tb[staff].y);
			a2b("rdots ");
			break;
		}
		bar_type >>= 4;
		if (bar_type == 0)
			break;
		x -= 3;
	}
	a2b("\n");
}

/* -- draw a rest -- */
/* (the staves are defined) */
static void draw_rest(struct SYMBOL *s)
{
	int i, j, y, l;
//	int no_head;
	char *stafflines;
	float x, dotx, staffb;
	static char *rest_tb[NFLAGS_SZ] = {
		"r128", "r64", "r32", "r16", "r8",
		"r4",
		"r2", "r1", "r0", "r00"
	};

	/* don't display the rests of invisible staves */
	/* (must do this here for voices out of their normal staff) */
	if (staff_tb[s->staff].empty)
		return;

	/* if rest alone in the measure or measure repeat, center */
	if (s->dur == voice_tb[s->voice].meter.wmeasure
	 || ((s->sflags & S_REPEAT) && s->doty >= 0)) {
		struct SYMBOL *s2;

		/* don't use next/prev: there is no bar in voice averlay */
		s2 = s->ts_next;
		while (s2 && s2->time != s->time + s->dur)
			s2 = s2->ts_next;
		if (s2)
			x = s2->x;
		else
			x = realwidth;
		s2 = s;
		while (!(s2->sflags & S_SEQST))
			s2 = s2->ts_prev;
		s2 = s2->ts_prev;
		x = (x + s2->x) * .5;

		/* center the associated decorations */
		if (s->u.note.dc.n > 0)
			deco_update(s, x - s->x);
		s->x = x;
	} else {
		x = s->x + s->u.note.notes[0].shhd * cur_scale;
	}
	if (s->flags & ABC_F_INVIS)
//	 && !(s->sflags & S_OTHER_HEAD))	//fixme: before new deco struct
		return;

	staffb = staff_tb[s->staff].y;		/* bottom of staff */

	if (s->sflags & S_REPEAT) {
		putxy(x, staffb + 12);
		if (s->doty < 0) {
			a2b("srep\n");
		} else {
			a2b("mrep\n");
			if (s->doty > 2
			 && s->voice == cursys->top_voice) {
/*fixme				set_font(s->gcf); */
				set_font(cfmt.anf);
				putxy(x, staffb + 24 + 4);
				a2b("M(%d)showc\n", s->doty);
			}
		}
		return;
	}

	y = s->y;

	i = C_XFLAGS - s->nflags;		/* rest_tb index */
	stafflines = staff_tb[s->staff].stafflines;
	l = strlen(stafflines);
	if (i == 7 && y == 12
	 && l <= 2)
		y -= 6;				/* semibreve a bit lower */

	putxy(x, y + staffb);				/* rest */
	a2b("%s ", s->u.note.notes[0].head ?
			s->u.note.notes[0].head : rest_tb[i]);

	/* output ledger line(s) when greater than minim */
	if (i >= 6) {
		j = y / 6;
		switch (i) {
		default:
			if (j >= l - 1 || stafflines[j + 1] != '|') {
				putxy(x, y + staffb);
				a2b("hl1 ");
			}
			if (i == 9) {		// longa
				y -= 6;
				j--;
			}
			break;
		case 7:
			y += 6;
			j++;
		case 6:
			break;
		}
		if (j >= l || stafflines[j] != '|') {
			putxy(x, y + staffb);
			a2b("hl1 ");
		}
	}

	dotx = 8;
	for (i = 0; i < s->dots; i++) {
		a2b("%.1f 3 dt ", dotx);
		dotx += 3.5;
	}
	a2b("\n");
}

/* -- draw grace notes -- */
/* (the staves are defined) */
static void draw_gracenotes(struct SYMBOL *s)
{
	int yy;
	float x0, y0, x1, y1, x2, y2, x3, y3, bet1, bet2, dy1, dy2;
	struct SYMBOL *g, *last;
	struct BEAM bm;

	/* draw the notes */
	bm.s2 = NULL;				/* (draw flags) */
	for (g = s->extra; g; g = g->next) {
		if (g->type != NOTEREST)
			continue;
		if ((g->sflags & (S_BEAM_ST | S_BEAM_END)) == S_BEAM_ST) {
			if (annotate)
				anno_out(g, 'b');
			if (calculate_beam(&bm, g))
				draw_beams(&bm);
		}
		draw_note(g->x, g, bm.s2 == NULL);
		if (annotate)
			anno_out(s, 'g');
		if (g == bm.s2)
			bm.s2 = NULL;			/* (draw flags again) */

		if (g->flags & ABC_F_SAPPO) {	/* (on 1st note only) */
			if (!g->next) {			/* if one note */
				x1 = 9;
				y1 = g->stem > 0 ? 5 : -5;
			} else {			/* many notes */
				x1 = (g->next->x - g->x) * .5 + 4;
				y1 = (g->ys + g->next->ys) * .5 - g->y;
				if (g->stem > 0)
					y1 -= 1;
				else
					y1 += 1;
			}
			putxy(x1, y1);
			a2b("g%ca\n", g->stem > 0 ? 'u' : 'd');
		}
		if (annotate
		 && (g->sflags & (S_BEAM_ST | S_BEAM_END)) == S_BEAM_END)
			anno_out(g, 'e');
		if (!g->next)
			break;			/* (keep the last note) */
	}

	/* slur */
	if (voice_tb[s->voice].key.instr == K_HP	/* no slur when bagpipe */
	 || voice_tb[s->voice].key.instr == K_Hp
	 || pipeformat
	 || !cfmt.graceslurs
	 || s->u.note.slur_st		/* explicit slur */
	 || !s->next
	 || s->next->abc_type != ABC_T_NOTE)
		return;
	last = g;
	if (last->stem >= 0) {
		yy = 127;
		for (g = s->extra; g; g = g->next) {
			if (g->type != NOTEREST)
				continue;
			if (g->y < yy) {
				yy = g->y;
				last = g;
			}
		}
		x0 = last->x;
		y0 = last->y - 5;
		if (s->extra != last) {
			x0 -= 4;
			y0 += 1;
		}
		s = s->next;
		x3 = s->x - 1;
		if (s->stem < 0)
			x3 -= 4;
		y3 = 3 * (s->pits[0] - 18) - 5;
		dy1 = (x3 - x0) * .4;
		if (dy1 > 3)
			dy1 = 3;
		dy2 = dy1;
		bet1 = .2;
		bet2 = .8;
		if (y0 > y3 + 7) {
			x0 = last->x - 1;
			y0 += .5;
			y3 += 6.5;
			x3 = s->x - 5.5;
			dy1 = (y0 - y3) * .8;
			dy2 = (y0 - y3) * .2;
			bet1 = 0;
		} else if (y3 > y0 + 4) {
			y3 = y0 + 4;
			x0 = last->x + 2;
			y0 = last->y - 4;
		}
	} else {
		yy = -127;
		for (g = s->extra; g; g = g->next) {
			if (g->type != NOTEREST)
				continue;
			if (g->y > yy) {
				yy = g->y;
				last = g;
			}
		}
		x0 = last->x;
		y0 = last->y + 5;
		if (s->extra != last) {
			x0 -= 4;
			y0 -= 1;
		}
		s = s->next;
		x3 = s->x - 1;
		if (s->stem >= 0)
			x3 -= 2;
		y3 = 3 * (s->pits[s->nhd] - 18) + 5;
		dy1 = (x0 - x3) * .4;
		if (dy1 < -3)
			dy1 = -3;
		dy2 = dy1;
		bet1 = .2;
		bet2 = .8;
		if (y0 < y3 - 7) {
			x0 = last->x - 1;
			y0 -= .5;
			y3 -= 6.5;
			x3 = s->x - 5.5;
			dy1 = (y0 - y3) * .8;
			dy2 = (y0 - y3) * .2;
			bet1 = 0;
		} else if (y3 < y0 - 4) {
			y3 = y0 - 4;
			x0 = last->x + 2;
			y0 = last->y + 4;
		}
	}

	x1 = bet1 * x3 + (1 - bet1) * x0;
	y1 = bet1 * y3 + (1 - bet1) * y0 - dy1;
	x2 = bet2 * x3 + (1 - bet2) * x0;
	y2 = bet2 * y3 + (1 - bet2) * y0 - dy2;

	a2b("%.2f %.2f %.2f %.2f %.2f %.2f ",
		x1 - x0, y1 - y0,
		x2 - x0, y2 - y0,
		x3 - x0, y3 - y0);
	putxy(x0, y0 + staff_tb[s->staff].y);
	a2b("gsl\n");
}

/* -- set the y offset of the dots -- */
static void setdoty(struct SYMBOL *s,
		    signed char *y_tb)
{
	int m, m1, y, doty;

	/* set the normal offsets */
	doty = s->doty;
	for (m = 0; m <= s->nhd; m++) {
		y = 3 * (s->pits[m] - 18);	/* note height on staff */
		if ((y % 6) == 0) {
			if (doty != 0)
				y -= 3;
			else
				y += 3;
		}
		y_tb[m] = y;
	}

	/* dispatch and recenter the dots in the staff space */
	for (m = 0; m < s->nhd; m++) {
		if (y_tb[m + 1] > y_tb[m])
			continue;
		m1 = m;
		while (m1 > 0) {
			if (y_tb[m1] > y_tb[m1 - 1] + 6)
				break;
			m1--;
		}
		if (3 * (s->pits[m1] - 18) - y_tb[m1]
				< y_tb[m + 1] - 3 *
					(s->pits[m + 1] - 18)) {
			while (m1 <= m)
				y_tb[m1++] -= 6;
		} else {
			y_tb[m + 1] = y_tb[m] + 6;
		}
	}
}

/* -- draw m-th head with accidentals and dots -- */
/* (the staves are defined) */
static void draw_basic_note(float x,
			    struct SYMBOL *s,
			    int m,
			    signed char *y_tb)
{
	struct note *note = &s->u.note.notes[m];
	int y, head, dots, nflags, acc;
//	int no_head;
	int old_color = -1;
	float staffb, shhd;
	char *p;
	char hd[32];

	staffb = staff_tb[s->staff].y;		/* bottom of staff */
	y = 3 * (s->pits[m] - 18);		/* note height on staff */
	shhd = note->shhd * cur_scale;

	if (s->flags & ABC_F_INVIS)
		return;

	putxy(x + shhd, y + staffb);		/* output x and y */

//	/* special case when no head */
//	if (s->nohdi1 >= 0
//	 && m >= s->nohdi1 && m < s->nohdi2) {
//		a2b("xydef");			/* set x y */
//		return;
//	}

	identify_note(s, note->len, &head, &dots, &nflags);
	acc = note->acc;

	/* output a ledger line if horizontal shift / chord
	 * and note on a line */
	if (y % 6 == 0
	 && shhd != (s->stem > 0 ? s->u.note.notes[0].shhd :
				s->u.note.notes[s->nhd].shhd)) {
		int yy;

		yy = 0;
		if (y >= 30) {
			yy = y;
			if (yy % 6)
				yy -= 3;
		} else if (y <= -6) {
			yy = y;
			if (yy % 6)
				yy += 3;
		}
		if (yy) {
			putxy(x + shhd, yy + staffb);
			a2b("hl ");
		}
	}

	/* draw the head */
	if (note->invisible) {
		p = "xydef";
	} else if ((p = note->head) != NULL) {
		snprintf(hd, sizeof hd, "%.*s", note->hlen, p);
		p = hd;
		a2b("2 copy xydef ");		/* set x y */
	} else if (s->flags & ABC_F_GRACE) {
		p = "ghd";
	} else if (s->type == CUSTOS) {
		p = "custos";
	} else if ((s->sflags & S_PERC) && acc != 0) {
		sprintf(hd, "p%shd", acc_tb[acc & 0x07]);
		acc = 0;
		p = hd;
	} else {
		switch (head) {
		case H_OVAL:
			if (note->len < BREVE) {
				p = "HD";
				break;
			}
			if (s->head != H_SQUARE) {
				p = "HDD";
				break;
			}
			/* fall thru */
		case H_SQUARE:
			p = note->len < BREVE * 2 ? "breve" : "longa";

			/* don't display dots on last note of the tune */
			if (!tsnext && s->next
			 && s->next->type == BAR && !s->next->next)
				dots = 0;
			break;
		case H_EMPTY:
			p = "Hd"; break;
		default:
			p = "hd"; break;
		}
	}
	if (note->color >= 0) {
		old_color = cur_color;
		set_color(note->color);
	}
	a2b("%s", p);

	/* draw the dots */
/*fixme: to see for grace notes*/
	if (dots) {
		float dotx;
		int doty;

		dotx = 7.7 + s->xmx - note->shhd;
		doty = y_tb[m] - y;
		if (scale_voice)
			doty /= cur_scale;
		while (--dots >= 0) {
			a2b(" %.1f %d dt", dotx, doty);
			dotx += 3.5;
		}
	}

	/* draw the accidental */
	if (acc != 0) {
		x -= note->shac * cur_scale;
		a2b(" ");
		putx(x);
		a2b(s->flags & ABC_F_GRACE ? "gsc " : "y ");
		draw_acc(acc, s->u.note.microscale);
		if (s->flags & ABC_F_GRACE)
			a2b(" grestore");
	}

	if (old_color >= 0) {
		a2b("\n");
		set_color(old_color);
	}
}

/* -- draw a note or a chord -- */
/* (the staves are defined) */
static void draw_note(float x,
		      struct SYMBOL *s,
		      int fl)
{
	int m, ma;
	float staffb, slen, shhd;
	char c, *hltype;
	signed char y_tb[MAXHD];

	if (s->dots)
		setdoty(s, y_tb);
	if (s->head >= H_OVAL)
		x += 1;
	staffb = staff_tb[s->staff].y;

	/* output the ledger lines */
	if (!(s->flags & ABC_F_INVIS)) {
		if (s->flags & ABC_F_GRACE) {
			hltype = "ghl";
		} else {
			switch (s->head) {
			default:
				hltype = "hl";
				break;
			case H_OVAL:
				hltype = "hl1";
				break;
			case H_SQUARE:
				hltype = "hl2";
				break;
			}
		}
		shhd = (s->stem > 0 ? s->u.note.notes[0].shhd :
					s->u.note.notes[s->nhd].shhd)
				* cur_scale;
		if (s->pits[0] < 22)
			draw_hl(x + shhd, staffb, 0,
				3 * (s->pits[0] - 18),	/* lower ledger lines */
				staff_tb[s->staff].stafflines, hltype);
		if (s->pits[s->nhd] > 22)
			draw_hl(x + shhd, staffb, 1,
				3 * (s->pits[s->nhd] - 18), /* upper ledger lines */
				staff_tb[s->staff].stafflines, hltype);
	}

	/* draw the master note, first or last one */
	if (cfmt.setdefl)
		set_defl(s->stem >= 0 ? DEF_STEMUP : 0);
	ma = s->stem >= 0 ? 0 : s->nhd;
	draw_basic_note(x, s, ma, y_tb);

	/* draw the stem and flags */
	if (!(s->flags & (ABC_F_INVIS | ABC_F_STEMLESS))) {
		char c2;

		c = s->stem >= 0 ? 'u' : 'd';
		slen = (s->ys - s->y) / voice_tb[s->voice].scale;
		if (!fl || s->nflags - s->aux <= 0) {	/* stem only */
			c2 = (s->flags & ABC_F_GRACE) ? 'g' : 's';
			if (s->nflags > 0) {	/* (fix for PS low resolution) */
				if (s->stem >= 0)
					slen -= 1;
				else
					slen += 1;
			}
			a2b(" %.1f %c%c", slen, c2, c);
		} else {				/* stem and flags */
			if (cfmt.straightflags)
				c = 's';		/* straight flag */
			c2 = (s->flags & ABC_F_GRACE) ? 'g' : 'f';
			a2b(" %d %.1f s%c%c", s->nflags - s->aux, slen, c2, c);
		}
	} else if (s->sflags & S_XSTEM) {	/* cross-staff stem */
		struct SYMBOL *s2;

		s2 = s->ts_prev;
			slen = (s2->stem > 0 ? s2->y : s2->ys) - s->y;
		slen += staff_tb[s2->staff].y - staffb;
/*fixme:KO when different scales*/
		slen /= voice_tb[s->voice].scale;
		a2b(" %.1f su", slen);
	}

	/* draw the tremolo bars */
	if (!(s->flags & ABC_F_INVIS)
	 && fl
	 && (s->sflags & S_TREM1)) {
		float x1;

		x1 = x + (s->stem > 0 ? s->u.note.notes[0].shhd :
					s->u.note.notes[s->nhd].shhd)
				* cur_scale;
		slen = 3 * (s->pits[s->stem > 0 ? s->nhd : 0] - 18);
		if (s->head >= H_OVAL) {
			if (s->stem > 0)
				slen += 5 + 5.4 * s->aux;
			else
				slen -= 5 + 5.4;
		} else {
			x1 += ((s->flags & ABC_F_GRACE)
					? GSTEM_XOFF : STEM_XOFF)
							* s->stem;
			if (s->stem > 0)
				slen += 6 + 5.4 * s->aux;
			else
				slen -= 6 + 5.4;
		}
		slen /= voice_tb[s->voice].scale;
		a2b(" %d ", s->aux);
		putxy(x1, staffb + slen);
		a2b("trem");
	}

	/* draw the other note heads */
	for (m = 0; m <= s->nhd; m++) {
		if (m == ma)
			continue;
		a2b(" ");
		draw_basic_note(x, s, m, y_tb);
	}
	a2b("\n");
}

/* -- find where to terminate/start a slur -- */
static struct SYMBOL *next_scut(struct SYMBOL *s)
{
	struct SYMBOL *prev;

	prev = s;
	for (s = s->next; s; s = s->next) {
		if (s->type == BAR
		 && ((s->sflags & S_RRBAR)
			|| s->u.bar.type == B_THIN_THICK
			|| s->u.bar.type == B_THICK_THIN
			|| (s->u.bar.repeat_bar
			 && s->text
			 && s->text[0] != '1')))
			return s;
		prev = s;
	}
	/*fixme: KO when no note for this voice at end of staff */
	return prev;
}

struct SYMBOL *prev_scut(struct SYMBOL *s)
{
	while (s->prev) {
		s = s->prev;
		if (s->type == BAR
		 && ((s->sflags & S_RRBAR)
		  || s->u.bar.type == B_THIN_THICK
		  || s->u.bar.type == B_THICK_THIN
		  || (s->u.bar.repeat_bar
		   && s->text
		   && s->text[0] != '1')))
			return s;
	}

	/* return a symbol of any voice starting before the start of the voice */
	s = voice_tb[s->voice].sym;
	while (s->type != CLEF)
		s = s->ts_prev;		/* search a main voice */
	if (s->next && s->next->type == KEYSIG)
		s = s->next;
	if (s->next && s->next->type == TIMESIG)
		s = s->next;
	return s;
}

/* -- decide whether a slur goes up or down -- */
static int slur_direction(struct SYMBOL *k1,
			  struct SYMBOL *k2)
{
	struct SYMBOL *s;
	int some_upstem, low;

	if ((k1->flags & ABC_F_GRACE) && k1->stem > 0)
		return -1;

	some_upstem = low = 0;
	for (s = k1; ; s = s->next) {
		if (s->abc_type == ABC_T_NOTE) {
			if (!(s->flags & ABC_F_STEMLESS)) {
				if (s->stem < 0)
					return 1;
				some_upstem = 1;
			}
			if (s->pits[0] < 22) /* if under middle staff */
				low = 1;
		}
		if (s == k2)
			break;
	}
	if (!some_upstem && !low)
		return 1;
	return -1;
}

/* -- output a slur / tie -- */
static void slur_out(float x1,
		     float y1,
		     float x2,
		     float y2,
		     int s,
		     float height,
		     int dotted,
		     int staff)	/* if < 0, the staves are defined */
{
	float alfa, beta, mx, my, xx1, yy1, xx2, yy2, dx, dy, dz;
	float scale_y;

	alfa = .3;
	beta = .45;

	/* for wide flat slurs, make shape more square */
	dy = y2 - y1;
	if (dy < 0)
		dy = -dy;
	dx = x2 - x1;
	if (dx > 40. && dy / dx < .7) {
		alfa = .3 + .002 * (dx - 40.);
		if (alfa > .7)
			alfa = .7;
	}

	/* alfa, beta, and height determine Bezier control points pp1,pp2
	 *
	 *           X====alfa===|===alfa=====X
	 *	    /		 |	       \
	 *	  pp1		 |	        pp2
	 *	  /	       height		 \
	 *	beta		 |		 beta
	 *      /		 |		   \
	 *    p1		 m		     p2
	 *
	 */

	mx = .5 * (x1 + x2);
	my = .5 * (y1 + y2);

	xx1 = mx + alfa * (x1 - mx);
	yy1 = my + alfa * (y1 - my) + height;
	xx1 = x1 + beta * (xx1 - x1);
	yy1 = y1 + beta * (yy1 - y1);

	xx2 = mx + alfa * (x2 - mx);
	yy2 = my + alfa * (y2 - my) + height;
	xx2 = x2 + beta * (xx2 - x2);
	yy2 = y2 + beta * (yy2 - y2);

	dx = .03 * (x2 - x1);
//	if (dx > 10.)
//		dx = 10.;
//	dy = 1.6 * s;
	dy = 2 * s;
	dz = .2 + .001 * (x2 - x1);
	if (dz > .6)
		dz = .6;
	dz *= s;
	
	scale_y = scale_voice ? cur_scale : 1;
	if (!dotted)
		a2b("%.2f %.2f %.2f %.2f %.2f %.2f 0 %.2f ",
			(xx2 - dx - x2) / cur_scale,
				(yy2 + dy - y2 - dz) / scale_y,
			(xx1 + dx - x2) / cur_scale,
				(yy1 + dy - y2 - dz) / scale_y,
			(x1 - x2) / cur_scale,
				(y1 - y2 - dz) / scale_y,
				dz);
	a2b("%.2f %.2f %.2f %.2f %.2f %.2f ",
		(xx1 - x1) / cur_scale, (yy1 - y1) / scale_y,
		(xx2 - x1) / cur_scale, (yy2 - y1) / scale_y,
		(x2 - x1) / cur_scale, (y2 - y1) / scale_y);
	putxy(x1, y1);
	if (staff >= 0)
		a2b("y%d ", staff);
	a2b(dotted ? "dSL\n" : "SL\n");
}

/* -- check if slur sequence in a multi-voice staff -- */
static int slur_multi(struct SYMBOL *k1,
		      struct SYMBOL *k2)
{
	for (;;) {
		if (k1->multi != 0)	/* if multi voice */
			/*fixme: may change*/
			return k1->multi;
		if (k1 == k2)
			break;
		k1 = k1->next;
	}
	return 0;
}

/* -- draw a phrasing slur between two symbols -- */
/* (the staves are not yet defined) */
/* (not a pretty routine, this) */
static int draw_slur(struct SYMBOL *k1_orig,
		     struct SYMBOL *k2,
		     int m1,
		     int m2,
		     int slur_type)
{
	struct SYMBOL *k1, *k;
	float x1, y1, x2, y2, height, addy;
	float a, y, z, h, dx, dy;
	int s, nn, upstaff, two_staves;

	k1 = k1_orig;
	while (k1->voice != k2->voice)
		k1 = k1->ts_next;

/*fixme: if two staves, may have upper or lower slur*/
	switch (slur_type & 0x07) {	/* (ignore dot bit) */
	case SL_ABOVE: s = 1; break;
	case SL_BELOW: s = -1; break;
	default:
		if ((s = slur_multi(k1, k2)) == 0)
			s = slur_direction(k1, k2);
		break;
	}

	nn = 1;
	upstaff = k1->staff;
	two_staves = 0;
	if (k1 != k2)
	    for (k = k1->next; k; k = k->next) {
		if (k->type == NOTEREST) {
			nn++;
			if (k->staff != upstaff) {
				two_staves = 1;
				if (k->staff < upstaff)
					upstaff = k->staff;
			}
		}
		if (k == k2)
			break;
	}
/*fixme: KO when two staves*/
if (two_staves) error(0, k1, "*** multi-staves slurs not treated yet");

	/* fix endpoints */
//	x1 = k1->x + k1->xmx;		/* take the max right side */
	x1 = k1_orig->x + k1_orig->u.note.notes[0].shhd;
	if (k1_orig != k2) {
//		x2 = k2->x;
		x2 = k2->x + k2->u.note.notes[0].shhd;
	} else {		/* (the slur starts on last note of the line) */
		for (k = k2->ts_next; k; k = k->ts_next)
			if (k->sflags & S_NEW_SY)
				break;
		if (!k)
			x2 = realwidth;
		else
			x2 = k->x;
	}

	if (m1 >= 0) {
		y1 = (float) (3 * (k1->pits[m1] - 18) + 5 * s);
	} else {
		y1 = (float) (s > 0 ? k1->ymx + 2 : k1->ymn - 2);
		if (k1->abc_type == ABC_T_NOTE) {
			if (s > 0) {
				if (k1->stem > 0) {
					x1 += 5;
					if ((k1->sflags & S_BEAM_END)
					 && k1->nflags >= -1	/* if with a stem */
//fixme: check if at end of tuplet
					 && (!(k1->sflags & S_IN_TUPLET))) {
//					  || k1->ys > y1 - 3)) {
						if (k1->nflags > 0) {
							x1 += 2;
							y1 = k1->ys - 3;
						} else {
							y1 = k1->ys - 6;
						}
// don't clash with decorations
//					} else {
//						y1 = k1->ys + 3;
					}
//				} else {
//					y1 = k1->y + 8;
				}
			} else {
				if (k1->stem < 0) {
					x1 -= 1;
					if ((k1->sflags & S_BEAM_END)
					 && k1->nflags >= -1
					 && (!(k1->sflags & S_IN_TUPLET)
					  || k1->ys < y1 + 3)) {
						if (k1->nflags > 0) {
							x1 += 2;
							y1 = k1->ys + 3;
						} else {
							y1 = k1->ys + 6;
						}
//					} else {
//						y1 = k1->ys - 3;
					}
//				} else {
//					y1 = k1->y - 8;
				}
			}
		}
	}
	if (m2 >= 0) {
		y2 = (float) (3 * (k2->pits[m2] - 18) + 5 * s);
	} else {
		y2 = (float) (s > 0 ? k2->ymx + 2 : k2->ymn - 2);
		if (k2->abc_type == ABC_T_NOTE) {
			if (s > 0) {
				if (k2->stem > 0) {
					x2 += 1;
					if ((k2->sflags & S_BEAM_ST)
					 && k2->nflags >= -1
					 && (!(k2->sflags & S_IN_TUPLET)))
//						|| k2->ys > y2 - 3))
						y2 = k2->ys - 6;
//					else
//						y2 = k2->ys + 3;
//				} else {
//					y2 = k2->y + 8;
				}
			} else {
				if (k2->stem < 0) {
					x2 -= 5;
					if ((k2->sflags & S_BEAM_ST)
					 && k2->nflags >= -1
					 && (!(k2->sflags & S_IN_TUPLET)))
//						|| k2->ys < y2 + 3))
						y2 = k2->ys + 6;
//					else
//						y2 = k2->ys - 3;
//				} else {
//					y2 = k2->y - 8;
				}
			}
		}
	}

	if (k1->abc_type != ABC_T_NOTE) {
		y1 = y2 + 1.2 * s;
		x1 = k1->x + k1->wr * .5;
		if (x1 > x2 - 12)
			x1 = x2 - 12;
	}

	if (k2->abc_type != ABC_T_NOTE) {
		if (k1->abc_type == ABC_T_NOTE)
			y2 = y1 + 1.2 * s;
		else
			y2 = y1;
		if (k1 != k2)
			x2 = k2->x - k2->wl * .3;
	}

	if (nn >= 3) {
		if (k1->next->type != BAR
		 && k1->next->x < x1 + 48) {
			if (s > 0) {
				y = k1->next->ymx - 2;
				if (y1 < y)
					y1 = y;
			} else {
				y = k1->next->ymn + 2;
				if (y1 > y)
					y1 = y;
			}
		}
		if (k2->prev
		 && k2->prev->type != BAR
		 && k2->prev->x > x2 - 48) {
			if (s > 0) {
				y = k2->prev->ymx - 2;
				if (y2 < y)
					y2 = y;
			} else {
				y = k2->prev->ymn + 2;
				if (y2 > y)
					y2 = y;
			}
		}
	}

	a = (y2 - y1) / (x2 - x1);		/* slur steepness */
	if (a > SLUR_SLOPE || a < -SLUR_SLOPE) {
		if (a > SLUR_SLOPE)
			a = SLUR_SLOPE;
		else
			a = -SLUR_SLOPE;
		if (a * s > 0)
			y1 = y2 - a * (x2 - x1);
		else
			y2 = y1 + a * (x2 - x1);
	}

	/* for big vertical jump, shift endpoints */
	y = y2 - y1;
	if (y > 8)
		y = 8;
	else if (y < -8)
		y = -8;
	z = y;
	if (z < 0)
		z = -z;
	dx = .5 * z;
	dy = .3 * y;
	if (y * s > 0) {
		x2 -= dx;
		y2 -= dy;
	} else {
		x1 += dx;
		y1 += dy;
	}

	/* special case for grace notes */
	if (k1->flags & ABC_F_GRACE)
		x1 = k1->x - GSTEM_XOFF * .5;
	if (k2->flags & ABC_F_GRACE)
		x2 = k2->x + GSTEM_XOFF * 1.5;

	h = 0;
	a = (y2 - y1) / (x2 - x1);
	if (k1 != k2
	 && k1->voice == k2->voice) {
	    addy = y1 - a * x1;
	    for (k = k1->next; k != k2 ; k = k->next) {
		if (k->staff != upstaff)
			continue;
		switch (k->type) {
		case NOTEREST:
			if (s > 0) {
				y = 3 * (k->pits[k->nhd] - 18) + 6;
				if (y < k->ymx)
					y = k->ymx;
				y -= a * k->x + addy;
				if (y > h)
					h = y;
			} else {
				y = 3 * (k->pits[0] - 18) - 6;
				if (y > k->ymn)
					y = k->ymn;
				y -= a * k->x + addy;
				if (y < h)
					h = y;
			}
			break;
		case GRACE: {
			struct SYMBOL *g;

			for (g = k->extra; g; g = g->next) {
#if 1
				if (g->type != NOTEREST)
					continue;
				if (s > 0) {
					y = 3 * (g->pits[g->nhd] - 18) + 6;
					if (y < g->ymx)
						y = g->ymx;
					y -= a * g->x + addy;
					if (y > h)
						h = y;
				} else {
					y = 3 * (g->pits[0] - 18) - 6;
					if (y > g->ymn)
						y = g->ymn;
					y -= a * g->x + addy;
					if (y < h)
						h = y;
				}
#else
				y = g->y - a * k->x - addy;
				if (s > 0) {
					y += GSTEM + 2;
					if (y > h)
						h = y;
				} else {
					y -= 2;
					if (y < h)
						h = y;
				}
#endif
			}
			break;
		    }
		}
	    }
	    y1 += .45 * h;
	    y2 += .45 * h;
	    h *= .65;
	}

	if (nn > 3)
		height = (.08 * (x2 - x1) + 12) * s;
	else
		height = (.03 * (x2 - x1) + 8) * s;
	if (s > 0) {
		if (height < 3 * h)
			height = 3 * h;
		if (height > 40)
			height = 40;
	} else {
		if (height > 3 * h)
			height = 3 * h;
		if (height < -40)
			height = -40;
	}

	y = y2 - y1;
	if (y < 0)
		y = -y;
	if (s > 0) {
		if (height < .8 * y)
			height = .8 * y;
	} else {
		if (height > -.8 * y)
			height = -.8 * y;
	}
	height *= cfmt.slurheight;

	slur_out(x1, y1, x2, y2, s, height, slur_type & SL_DOTTED, upstaff);

	/* have room for other symbols */
	dx = x2 - x1;
	a = (y2 - y1) / dx;
/*fixme: it seems to work with .4, but why?*/
	addy = y1 - a * x1 + .4 * height;
	if (k1->voice == k2->voice)
	    for (k = k1; k != k2; k = k->next) {
		if (k->staff != upstaff)
			continue;
		y = a * k->x + addy;
		if (k->ymx < y)
			k->ymx = y;
		else if (k->ymn > y)
			k->ymn = y;
		if (k->next == k2) {
			dx = x2;
			if (k2->sflags & S_SL1)
				dx -= 5;
		} else {
			dx = k->next->x;
		}
		if (k != k1)
			x1 = k->x;
		dx -= x1;
		y_set(upstaff, s > 0, x1, dx, y);
	}
	return (s > 0 ? SL_ABOVE : SL_BELOW) | (slur_type & SL_DOTTED);
}

/* -- draw the slurs between 2 symbols --*/
static void draw_slurs(struct SYMBOL *first,
		       struct SYMBOL *last)
{
	struct SYMBOL *s, *s1, *k, *gr1, *gr2;
	int i, m1, m2, gr1_out, slur_type, cont;

	gr1 = gr2 = NULL;
	s = first;
	for (;;) {
		if (!s || s == last) {
			if (!gr1
			 || !(s = gr1->next)
			 || s == last)
				break;
			gr1 = NULL;
		}
		if (s->type == GRACE) {
			gr1 = s;
			s = s->extra;
			continue;
		}
		if ((s->type != NOTEREST && s->type != SPACE)
		 || (s->u.note.slur_st == 0
			&& !(s->sflags & S_SL1))) {
			s = s->next;
			continue;
		}
		k = NULL;		/* find matching slur end */
		s1 = s->next;
		gr1_out = 0;
		for (;;) {
			if (!s1) {
				if (gr2) {
					s1 = gr2->next;
					gr2 = NULL;
					continue;
				}
				if (!gr1 || gr1_out)
					break;
				s1 = gr1->next;
				gr1_out = 1;
				continue;
			}
			if (s1->type == GRACE) {
				gr2 = s1;
				s1 = s1->extra;
				continue;
			}
			if (s1->type == BAR
			 && ((s1->sflags & S_RRBAR)
			  || s1->u.bar.type == B_THIN_THICK
			  || s1->u.bar.type == B_THICK_THIN
			  || (s1->u.bar.repeat_bar
			   && s1->text
			   && s1->text[0] != '1'))) {
				k = s1;
				break;
			}
			if (s1->type != NOTEREST && s1->type != SPACE) {
				s1 = s1->next;
				continue;
			}
			if (s1->u.note.slur_end
			 || (s1->sflags & S_SL2)) {
				k = s1;
				break;
			}
			if (s1->u.note.slur_st
			 || (s1->sflags & S_SL1)) {
				if (gr2) {	/* if in grace note sequence */
					for (k = s1; k->next; k = k->next)
						;
					k->next = gr2->next;
					if (gr2->next)
						gr2->next->prev = k;
//					gr2->u.note.slur_st = SL_AUTO;
					k = NULL;
				}
				draw_slurs(s1, last);
				if (gr2
				 && gr2->next) {
					gr2->next->prev->next = NULL;
					gr2->next->prev = gr2;
				}
			}
			if (s1 == last)
				break;
			s1 = s1->next;
		}
		if (!s1) {
			k = next_scut(s);
		} else if (!k) {
			s = s1;
			if (s == last)
				break;
			continue;
		}

		/* if slur in grace note sequence, change the linkages */
		if (gr1) {
			for (s1 = s; s1->next; s1 = s1->next)
				;
			s1->next = gr1->next;
			if (gr1->next)
				gr1->next->prev = s1;
			gr1->u.note.slur_st = SL_AUTO;
		}
		if (gr2) {
			gr2->prev->next = gr2->extra;
			gr2->extra->prev = gr2->prev;
			gr2->u.note.slur_st = SL_AUTO;
		}
		if (s->u.note.slur_st) {
			slur_type = s->u.note.slur_st & 0x0f;
			s->u.note.slur_st >>= 4;
			m1 = -1;
		} else {
			for (m1 = 0; m1 <= s->nhd; m1++)
				if (s->u.note.notes[m1].sl1)
					break;
			slur_type = s->u.note.notes[m1].sl1 & 0x0f;
			s->u.note.notes[m1].sl1 >>= 4;
			if (s->u.note.notes[m1].sl1 == 0) {
				for (i = m1 + 1; i <= s->nhd; i++)
					if (s->u.note.notes[i].sl1)
						break;
				if (i > s->nhd)
					s->sflags &= ~S_SL1;
			}
		}
		m2 = -1;
		cont = 0;
		if ((k->type == NOTEREST || k->type == SPACE)
		  && (k->u.note.slur_end
		   || (k->sflags & S_SL2))) {
			if (k->u.note.slur_end) {
				k->u.note.slur_end--;
			} else {
				for (m2 = 0; m2 <= k->nhd; m2++)
					if (k->u.note.notes[m2].sl2)
						break;
				k->u.note.notes[m2].sl2--;
				if (k->u.note.notes[m2].sl2 == 0) {
					for (i = m2 + 1; i <= k->nhd; i++)
						if (k->u.note.notes[i].sl2)
							break;
					if (i > k->nhd)
						k->sflags &= ~S_SL2;
				}
			}
		} else {
			if (k->type != BAR
			 || (!(k->sflags & S_RRBAR)
			  && k->u.bar.type != B_THIN_THICK
			  && k->u.bar.type != B_THICK_THIN
			  && (!k->u.bar.repeat_bar
			   || !k->text
			   || k->text[0] == '1')))
				cont = 1;
		}
		slur_type = draw_slur(s, k, m1, m2, slur_type);
		if (cont) {
/*fixme: the slur types are inverted*/
			voice_tb[k->voice].slur_st <<= 4;
			voice_tb[k->voice].slur_st += slur_type;
		}

		/* if slur in grace note sequence, restore the linkages */
		if (gr1
		 && gr1->next) {
			gr1->next->prev->next = NULL;
			gr1->next->prev = gr1;
		}
		if (gr2) {
			gr2->prev->next = gr2;
			gr2->extra->prev = NULL;
		}

		if (s->u.note.slur_st
		 || (s->sflags & S_SL1))
			continue;
		if (s == last)
			break;
		s = s->next;
	}
}

/* -- draw a tuplet -- */
/* (the staves are not yet defined) */
/* See http://moinejf.free.fr/abcm2ps-doc/tuplets.xhtml
 * about the value of 'aux' */
static struct SYMBOL *draw_tuplet(struct SYMBOL *t,	/* tuplet in extra */
				  struct SYMBOL *s)	/* main note */
{
	struct SYMBOL *s1, *s2, *sy, *next, *g;
	int r, upstaff, nb_only, some_slur, dir;
	float x1, x2, y1, y2, xm, ym, a, s0, yy, yx, dy;

	next = s;
	if ((t->aux & 0xf000) == 0x1000)	/* if 'when' == never */
		goto done;

	/* treat the nested tuplets starting on this symbol */
	for (g = t->next; g; g = g->next) {
		if (g->type == TUPLET) {
			sy = draw_tuplet(g, s);
			if (sy->time > next->time)
				next = sy;
		}
	}

	/* search the first and last notes/rests of the tuplet */
	r = t->u.tuplet.r_plet;
	s1 = NULL;
	some_slur = 0;
	upstaff = s->staff;
	for (s2 = s; s2; s2 = s2->next) {
		if (s2 != s
		 && (s2->sflags & S_IN_TUPLET)) {
			for (g = s2->extra; g; g = g->next) {
				if (g->type == TUPLET) {
					sy = draw_tuplet(g, s2);
					if (sy->time > next->time)
						next = sy;
				}
			}
		}
		if (s2->type != NOTEREST) {
			if (s2->type == GRACE) {
				for (g = s2->extra; g; g = g->next) {
					if (g->type != NOTEREST)
						continue;
					if (g->u.note.slur_st
					 || (g->sflags & S_SL1))
						some_slur = 1;
				}
			}
			continue;
		}
		if (s2->u.note.slur_st		/* if slur start/end */
		 || s2->u.note.slur_end
		 || (s2->sflags & (S_SL1 | S_SL2)))
			some_slur = 1;
		if (s2->staff < upstaff)
			upstaff = s2->staff;
		if (!s1)
			s1 = s2;
		if (--r <= 0)
			break;
	}
	if (!s2)
		goto done;			/* no solution... */
	if (s2->time > next->time)
		next = s2;

	dir = t->aux & 0x000f;
	if (!dir)
		dir = s1->stem > 0 ? SL_ABOVE : SL_BELOW;

	if (s1 == s2) {				/* tuplet with 1 note (!) */
		nb_only = 1;
	} else if ((t->aux & 0x0f00) == 0x0100) {	/* 'what' == slur */
		nb_only = 1;
		draw_slur(s1, s2, -1, -1, dir);
	} else {

		/* search if a bracket is needed */
		if ((t->aux & 0xf000) == 0x2000	/* if 'when' == always */
		 || s1->abc_type != ABC_T_NOTE || s2->abc_type != ABC_T_NOTE) {
			nb_only = 0;
		} else {
			nb_only = 1;
			for (sy = s1; ; sy = sy->next) {
				if (sy->type != NOTEREST) {
					if (sy->type == GRACE
					 || sy->type == SPACE)
						continue;
					nb_only = 0;
					break;
				}
				if (sy == s2)
					break;
				if (sy->sflags & S_BEAM_END) {
					nb_only = 0;
					break;
				}
			}
			if (nb_only
			 && !(s1->sflags & (S_BEAM_ST | S_BEAM_BR1 | S_BEAM_BR2))) {
				for (sy = s1->prev; sy; sy = sy->prev) {
					if (sy->type == NOTEREST) {
						if (sy->nflags >= s1->nflags)
							nb_only = 0;
						break;
					}
				}
			}
			if (nb_only && !(s2->sflags & S_BEAM_END)) {
				for (sy = s2->next; sy; sy = sy->next) {
					if (sy->type == NOTEREST) {
						if (!(sy->sflags & (S_BEAM_BR1 | S_BEAM_BR2))
						 && sy->nflags >= s2->nflags)
							nb_only = 0;
						break;
					}
				}
			}
		}
	}

	/* if number only, draw it */
	if (nb_only) {
		float a, b;

		if ((t->aux & 0x00f0) == 0x0010) /* if 'which' == none */
			goto done;
		xm = (s2->x + s1->x) * .5;
		if (s1 == s2)			/* tuplet with 1 note */
			a = 0;
		else
			a = (s2->ys - s1->ys) / (s2->x - s1->x);
		b = s1->ys - a * s1->x;
		yy = a * xm + b;
		if (dir == SL_ABOVE) {
			ym = y_get(s1->staff, 1, xm - 3, 6);
			if (ym > yy)
				b += ym - yy;
			b += 2;
		} else {
			ym = y_get(s1->staff, 0, xm - 3, 6);
			if (ym < yy)
				b += ym - yy;
			b -= 10;
		}
		for (sy = s1; ; sy = sy->next) {
			if (sy->x >= xm)
				break;
		}
		if (s1->stem * s2->stem > 0) {
			if (s1->stem > 0)
				xm += GSTEM_XOFF;
			else
				xm -= GSTEM_XOFF;
		}
		ym = a * xm + b;
		if ((t->aux & 0x00f0) == 0)	/* if 'which' == number */
			a2b("(%d)", t->u.tuplet.p_plet);
		else
			a2b("(%d:%d)", t->u.tuplet.p_plet, t->u.tuplet.q_plet);
		putxy(xm, ym);
		a2b("y%d bnum\n", s1->staff);

		if (dir == SL_ABOVE) {
			ym += 8;
			if (sy->ymx < ym)
				sy->ymx = (short) ym;
			y_set(s1->staff, 1, xm - 3, 6, ym);
		} else {
			if (sy->ymn > ym)
				sy->ymn = (short) ym;
			y_set(s1->staff, 0, xm - 3, 6, ym);
		}
		goto done;
	}

	/* draw the slurs when inside the tuplet */
	if (some_slur) {
		draw_slurs(s1, s2);
		if (s1->u.note.slur_st
		 || (s1->sflags & S_SL1))
			return next;
		for (sy = s1->next; sy != s2; sy = sy->next) {
			if (sy->u.note.slur_st		/* if slur start/end */
			 || sy->u.note.slur_end
			 || (sy->sflags & (S_SL1 | S_SL2)))
				return next;		/* don't draw now */
		}

		/* don't draw the tuplet when a slur ends on the last note */
		if (s2->u.note.slur_end
		 || (s2->sflags & S_SL2))
			return next;
	}

	if ((t->aux & 0x0f00) != 0)		/* if 'what' != square */
		fprintf(stderr, "'what' value of %%%%tuplets not yet coded\n");

/*fixme: two staves not treated*/
/*fixme: to optimize*/
	dir = t->aux & 0x000f;			/* 'where' */
	if (!dir)
		dir = s1->multi >= 0 ? SL_ABOVE : SL_BELOW;
    if (dir == SL_ABOVE) {

	/* sole or upper voice: the bracket is above the staff */
	x1 = s1->x - 4;
	y1 = 24;
	if (s1->staff == upstaff) {
		sy = s1;
		if (sy->abc_type != ABC_T_NOTE) {
			for (sy = sy->next; sy != s2; sy = sy->next)
				if (sy->abc_type == ABC_T_NOTE)
					break;
		}
		ym = y_get(upstaff, 1, sy->x, 0);
		if (ym > y1)
			y1 = ym;
		if (s1->stem > 0)
			x1 += 3;
	}
	y2 = 24;
	if (s2->staff == upstaff) {
		sy = s2;
		if (sy->abc_type != ABC_T_NOTE) {
			for (sy = sy->prev; sy != s1; sy = sy->prev)
				if (sy->abc_type == ABC_T_NOTE)
					break;
		}
		ym = y_get(upstaff, 1, sy->x, 0);
		if (ym > y2)
			y2 = ym;
	}

	/* end the backet according to the last note duration */
	if (s2->dur > s2->prev->dur) {
		if (s2->next)
			x2 = s2->next->x - s2->next->wl - 5;
		else
			x2 = realwidth - 6;
	} else {
		x2 = s2->x + 4;
		r = s2->stem >= 0 ? 0 : s2->nhd;
		if (s2->u.note.notes[r].shhd > 0)
			x2 += s2->u.note.notes[r].shhd;
		if (s2->staff == upstaff
		 && s2->stem > 0)
			x2 += 3.5;
	}

	xm = .5 * (x1 + x2);
	ym = .5 * (y1 + y2);

	a = (y2 - y1) / (x2 - x1);
	s0 = 3 * (s2->pits[s2->nhd] -
			s1->pits[s1->nhd]) / (x2 - x1);
	if (s0 > 0) {
		if (a < 0)
			a = 0;
		else if (a > s0)
			a = s0;
	} else {
		if (a > 0)
			a = 0;
		else if (a < s0)
			a = s0;
	}
	if (a * a < .1 * .1)
		a = 0;

	/* shift up bracket if needed */
	dy = 0;
	for (sy = s1; ; sy = sy->next) {
		if (sy->dur == 0	/* not a note or a rest */
		 || sy->staff != upstaff) {
			if (sy == s2)
				break;
			continue;
		}
		yy = ym + (sy->x - xm) * a;
		yx = y_get(upstaff, 1, sy->x, 0);
		if (yx - yy > dy)
			dy = yx - yy;
		if (sy == s2)
			break;
	}

	ym += dy + 2;
	y1 = ym + a * (x1 - xm);
	y2 = ym + a * (x2 - xm);
	putxy(x2 - x1, y2 - y1);
	putxy(x1, y1 + 4);
	a2b("y%d tubr", upstaff);

	/* shift the slurs / decorations */
	ym += 8;
	for (sy = s1; ; sy = sy->next) {
		if (sy->staff == upstaff) {
			yy = ym + (sy->x - xm) * a;
			if (sy->ymx < yy)
				sy->ymx = yy;
			if (sy == s2)
				break;
			y_set(upstaff, 1, sy->x, sy->next->x - sy->x, yy);
		} else if (sy == s2) {
			break;
		}
	}

    } else {	/* lower voice of the staff: the bracket is below the staff */
/*fixme: think to all that again..*/
	x1 = s1->x - 7;

	if (s2->dur > s2->prev->dur) {
		if (s2->next)
			x2 = s2->next->x - s2->next->wl - 8;
		else
			x2 = realwidth - 6;
	} else {
		x2 = s2->x + 2;
		if (s2->u.note.notes[s2->nhd].shhd > 0)
			x2 += s2->u.note.notes[s2->nhd].shhd;
	}

	if (s1->staff == upstaff) {
		sy = s1;
		if (sy->abc_type != ABC_T_NOTE) {
			for (sy = sy->next; sy != s2; sy = sy->next)
				if (sy->abc_type == ABC_T_NOTE)
					break;
		}
		y1 = y_get(upstaff, 0, sy->x, 0);
	} else {
		y1 = 0;
	}
	if (s2->staff == upstaff) {
		sy = s2;
		if (sy->abc_type != ABC_T_NOTE) {
			for (sy = sy->prev; sy != s1; sy = sy->prev)
				if (sy->abc_type == ABC_T_NOTE)
					break;
		}
		y2 = y_get(upstaff, 0, sy->x, 0);
	} else {
		y2 = 0;
	}

	xm = .5 * (x1 + x2);
	ym = .5 * (y1 + y2);

	a = (y2 - y1) / (x2 - x1);
	s0 = 3 * (s2->pits[0] - s1->pits[0]) / (x2 - x1);
	if (s0 > 0) {
		if (a < 0)
			a = 0;
		else if (a > s0)
			a = s0;
	} else {
		if (a > 0)
			a = 0;
		else if (a < s0)
			a = s0;
	}
	if (a * a < .1 * .1)
		a = 0;

	/* shift down bracket if needed */
	dy = 0;
	for (sy = s1; ; sy = sy->next) {
		if (sy->dur == 0	/* not a note nor a rest */
		 || sy->staff != upstaff) {
			if (sy == s2)
				break;
			continue;
		}
		yy = ym + (sy->x - xm) * a;
		yx = y_get(upstaff, 0, sy->x, 0);
		if (yx - yy < dy)
			dy = yx - yy;
		if (sy == s2)
			break;
	}

	ym += dy - 10;
	y1 = ym + a * (x1 - xm);
	y2 = ym + a * (x2 - xm);
	putxy(x2 - x1, y2 - y1);
	putxy(x1, y1 + 4);
	a2b("y%d tubrl",upstaff);

	/* shift the slurs / decorations */
	ym -= 2;
	for (sy = s1; ; sy = sy->next) {
		if (sy->staff == upstaff) {
			if (sy == s2)
				break;
			yy = ym + (sy->x - xm) * a;
			if (sy->ymn > yy)
				sy->ymn = (short) yy;
			y_set(upstaff, 0, sy->x, sy->next->x - sy->x, yy);
		}
		if (sy == s2)
			break;
	}
    } /* lower voice */

	if ((t->aux & 0x00f0) == 0x10) {	/* if 'which' == none */
		a2b("\n");
		goto done;
	}
	yy = .5 * (y1 + y2);
	if ((t->aux & 0x00f0) == 0)		/* if 'which' == number */
		a2b("(%d)", t->u.tuplet.p_plet);
	else
		a2b("(%d:%d)", t->u.tuplet.p_plet, t->u.tuplet.q_plet);
	putxy(xm, yy);
	a2b("y%d bnumb\n", upstaff);

done:
	s->sflags &= ~S_IN_TUPLET;
	return next;
}

/* -- draw the ties between two notes/chords -- */
static void draw_note_ties(struct SYMBOL *k1,
			   struct SYMBOL *k2,
			   int ntie,
			   unsigned char *mhead1,
			   unsigned char *mhead2,
			   int job)
{
	int i, s, m1, m2, p, p1, p2, y, staff;
	float x1, x2, h, sh;

	for (i = 0; i < ntie; i++) {
		m1 = mhead1[i];
		p1 = k1->pits[m1];
		m2 = mhead2[i];
		p2 = job != 2 ? k2->pits[m2] : p1;
		s = (k1->u.note.notes[m1].ti1 & 0x07) == SL_ABOVE ? 1 : -1;

		x1 = k1->x;
		sh = k1->u.note.notes[m1].shhd;		/* head shift */
		if (s > 0) {
			if (m1 < k1->nhd && p1 + 1 == k1->pits[m1 + 1])
				if (k1->u.note.notes[m1 + 1].shhd > sh)
					sh = k1->u.note.notes[m1 + 1].shhd;
		} else {
			if (m1 > 0 && p1 == k1->pits[m1 - 1] + 1)
				if (k1->u.note.notes[m1 - 1].shhd > sh)
					sh = k1->u.note.notes[m1 - 1].shhd;
		}
//		x1 += sh;
		x1 += sh * 0.6;

		x2 = k2->x;
		if (job != 2) {
			sh = k2->u.note.notes[m2].shhd;
			if (s > 0) {
				if (m2 < k2->nhd && p2 + 1 == k2->pits[m2 + 1])
					if (k2->u.note.notes[m2 + 1].shhd < sh)
						sh = k2->u.note.notes[m2 + 1].shhd;
			} else {
				if (m2 > 0 && p2 == k2->pits[m2 - 1] + 1)
					if (k2->u.note.notes[m2 - 1].shhd < sh)
						sh = k2->u.note.notes[m2 - 1].shhd;
			}
			x2 += sh * 0.6;
		}

		staff = k1->staff;
		switch (job) {
		case 0:
			if (p1 == p2 || (p1 & 1))
				p = p1;
			else
				p = p2;
			break;
		case 3:				/* clef or staff change */
			s = -s;
			// fall thru
		case 1:				/* no starting note */
			x1 = k1->x;
			if (x1 > x2 - 20)
				x1 = x2 - 20;
			p = p2;
			staff = k2->staff;
			break;
/*		case 2:				 * no ending note */
		default:
			if (k1 != k2) {
				if (k2->type == BAR)
					x2 -= 2;
				else
					x2 -= k2->wl;
			} else {
				struct SYMBOL *k;
				int time;

				time = k1->time + k1->dur;
				for (k = k1->ts_next; k; k = k->ts_next)
					if (k->time > time)
						break;
				if (!k)
					x2 = realwidth;
				else
					x2 = k->x;
			}
			if (x2 < x1 + 16)
				x2 = x1 + 16;
			p = p1;
			break;
		}
		if (x2 - x1 > 20) {
			x1 += 3.5;
			x2 -= 3.5;
		} else {
			x1 += 1.5;
			x2 -= 1.5;
		}

		y = 3 * (p - 18);
//fixme: clash when 2 ties on second interval chord
//		if (p & 1)
//			y += 2 * s;
#if 0
		if (job != 1 && job != 3) {
			if (s > 0) {
//				if (k1->nflags > -2 && k1->stem > 0
//				 && k1->nhd == 0)
//					x1 += 4.5;
				if (!(p & 1) && k1->dots > 0)
					y = 3 * (p - 18) + 6;
			}
//		} else {
//			if (s < 0) {
//				if (k2->nflags > -2 && k2->stem < 0
//				 && k2->nhd == 0)
//					x2 -= 4.5;
//			}
		}
#endif

		h = (.04 * (x2 - x1) + 10) * s;
		slur_out(x1, staff_tb[staff].y + y,
			 x2, staff_tb[staff].y + y,
			 s, h, k1->u.note.notes[m1].ti1 & SL_DOTTED, -1);
	}
}

/* -- draw ties between neighboring notes/chords -- */
static void draw_ties(struct SYMBOL *k1,
		      struct SYMBOL *k2,
		      int job)		/* 0: normal
					 * 1: no starting note
					 * 2: no ending note
					 * 3: no start for clef or staff change */
{
	struct SYMBOL *k3;
	int i, m1, nh1, pit, ntie, tie2, ntie3, time;
	unsigned char mhead1[MAXHD], mhead2[MAXHD], mhead3[MAXHD];

	time = k1->time + k1->dur;
	ntie = ntie3 = 0;
	nh1 = k1->nhd;

	/* half ties from last note in line or before new repeat */
	if (job == 2) {
		for (i = 0; i <= nh1; i++) {
			if (k1->u.note.notes[i].ti1)
				mhead3[ntie3++] = i;
		}
		draw_note_ties(k1, k2 ? k2 : k1, ntie3, mhead3, mhead3, job);
		return;
	}

	/* set up list of ties to draw */
	for (i = 0; i <= nh1; i++) {
		if (k1->u.note.notes[i].ti1 == 0)
			continue;
		tie2 = -1;
		pit = k1->u.note.notes[i].pit;
		for (m1 = k2->nhd; m1 >= 0; m1--) {
			switch (k2->u.note.notes[m1].pit - pit) {
			case 1:			/* maybe ^c - _d */
			case -1:		/* _d - ^c */
				if (k1->u.note.notes[i].acc != k2->u.note.notes[m1].acc)
					tie2 = m1;
			default:
				continue;
			case 0:
				tie2 = m1;
				break;
			}
			break;
		}
		if (tie2 >= 0) {		/* 1st or 2nd choice */
			mhead1[ntie] = i;
			mhead2[ntie++] = tie2;
		} else {
			mhead3[ntie3++] = i;	/* no match */
		}
	}

	/* draw the ties */
	draw_note_ties(k1, k2, ntie, mhead1, mhead2, job);

	/* if any bad tie, try an other voice of the same staff */
	if (ntie3 == 0)
		return;				/* no bad tie */
	k3 = k1->ts_next;
	while (k3 && k3->time < time)
		k3 = k3->ts_next;
	while (k3 && k3->time == time) {
		if (k3->abc_type != ABC_T_NOTE
		 || k3->staff != k1->staff) {
			k3 = k3->ts_next;
			continue;
		}
		ntie = 0;
		for (i = ntie3; --i >= 0; ) {
			pit = k1->u.note.notes[mhead3[i]].pit;
			for (m1 = k3->nhd; m1 >= 0; m1--) {
				if (k3->u.note.notes[m1].pit == pit) {
					mhead1[ntie] = mhead3[i];
					mhead2[ntie++] = m1;
					ntie3--;
//					mhead3[i] = mhead3[--ntie3];
					break;
				}
			}
		}
		if (ntie > 0) {
			draw_note_ties(k1, k3,
					ntie, mhead1, mhead2,
					job == 1 ? 1 : 0);
			if (ntie3 == 0)
				return;
		}
		k3 = k3->ts_next;
	}

//	if (ntie3 != 0)
		error(1, k1, "Bad tie");
}

static void draw_ties_g(struct SYMBOL *s1,
			struct SYMBOL *s2,
			int job)
{
	struct SYMBOL *g;

	if (s1->type == GRACE) {
		for (g = s1->extra; g; g = g->next) {
			if (g->sflags & S_TI1)
				draw_ties(g, s2, job);
		}
	} else {
		draw_ties(s1, s2, job);
	}
}

/* -- try to get the symbol of a ending tie when combined voices -- */
static struct SYMBOL *tie_comb(struct SYMBOL *s)
{
	struct SYMBOL *s1;
	int time, st;

	time = s->time + s->dur;
	st = s->staff;
	for (s1 = s->ts_next; s1; s1 = s1->ts_next) {
		if (s1->staff != st)
			continue;
		if (s1->time == time) {
			if (s1->abc_type == ABC_T_NOTE)
				return s1;
			continue;
		}
		if (s1->time > time)
			return s;		// bad tie
	}
	return NULL;				// no ending tie
}

/* -- draw all ties between neighboring notes -- */
static void draw_all_ties(struct VOICE_S *p_voice)
{
	struct SYMBOL *s1, *s2, *s3, *s4, *rtie;
	struct SYMBOL tie;
	int clef_chg, time;

	for (s1 = p_voice->sym; s1; s1 = s1->next) {
		switch (s1->type) {
		case CLEF:
		case KEYSIG:
		case TIMESIG:
			continue;
		}
		break;
	}
	rtie = p_voice->rtie;			/* tie from 1st repeat bar */
	for (s2 = s1; s2; s2 = s2->next) {
//		if (s2->abc_type == ABC_T_NOTE
		if (s2->dur
		 || s2->type == GRACE)
			break;
		if (s2->type != BAR
		 || !s2->u.bar.repeat_bar
		 || !s2->text)
			continue;
		if (s2->text[0] == '1')	/* 1st repeat bar */
			rtie = p_voice->tie;
		else
			p_voice->tie = rtie;
	}
	if (!s2)
		return;
	if (p_voice->tie) {			/* tie from previous line */
		p_voice->tie->x = s1->x + s1->wr;
		s1 = p_voice->tie;
		p_voice->tie = NULL;
		s1->staff = s2->staff;
		s1->ts_next = s2->ts_next;	/* (for tie to other voice) */
		s1->time = s2->time - s1->dur;	/* (if after repeat sequence) */
		draw_ties(s1, s2, 1);		/* tie to 1st note */
	}

	/* search the start of ties */
	clef_chg = 0;
	for (;;) {
		for (s1 = s2; s1; s1 = s1->next) {
			if (s1->sflags & S_TI1)
				break;
			if (!rtie)
				continue;
			if (s1->type != BAR
			 || !s1->u.bar.repeat_bar
			 || !s1->text)
				continue;
			if (s1->text[0] == '1') {	/* 1st repeat bar */
				rtie = NULL;
				continue;
			}
			if (s1->u.bar.type == B_BAR)
				continue;		// not a repeat
			for (s2 = s1->next; s2; s2 = s2->next)
				if (s2->abc_type == ABC_T_NOTE)
					break;
			if (!s2) {
				s1 = NULL;
				break;
			}
			memcpy(&tie, rtie, sizeof tie);
//			tie.x = s1->x + s1->wr;
			tie.x = s1->x;
			tie.next = s2;
			tie.staff = s2->staff;
			tie.time = s2->time - tie.dur;
			draw_ties(&tie, s2, 1);
		}
		if (!s1)
			break;

		/* search the end of the tie
		 * and notice the clef changes (may occur in an other voice) */
		time = s1->time + s1->dur;
		for (s2 = s1->next; s2; s2 = s2->next) {
			if (s2->dur != 0)
				break;
			if (s2->type == BAR
			 && s2->u.bar.repeat_bar
			 && s2->text) {
				if (s2->text[0] != '1')
					break;
				rtie = s1;	/* 1st repeat bar */
			}
		}
		if (!s2) {
			for (s4 = s1->ts_next; s4; s4 = s4->ts_next) {
				if (s4->staff != s1->staff)
					continue;
				if (s4->time < time)
					continue;
				if (s4->time > time) {
					s4 = NULL;
					break;
				}
				if (s4->dur != 0)
					break;
			}
			if (!s4) {
				draw_ties_g(s1, NULL, 2);
				p_voice->tie = s1;
				break;
			}
		} else {
			if (s2->abc_type != ABC_T_NOTE
			 && s2->abc_type != ABC_T_BAR) {
				error(1, s1, "Bad tie");
				continue;
			}
			if (s2->time == time) {
				s4 = s2;
			} else {
				s4 = tie_comb(s1);
				if (s4 == s1) {
					error(1, s1, "Bad tie");
					continue;
				}
			}
		}
		for (s3 = s1->ts_next; s3; s3 = s3->ts_next) {
			if (s3->staff != s1->staff)
				continue;
			if (s3->time > time)
				break;
			if (s3->type == CLEF) {
				clef_chg = 1;
				continue;
			}
#if 0
			if (s3->type == BAR) {
//				if ((s3->sflags & S_RRBAR)
//				 || s3->u.bar.type == B_THIN_THICK
//				 || s3->u.bar.type == B_THICK_THIN) {
//					s4 = s3;
//					break;
//				}
				if (!s3->u.bar.repeat_bar
				 || !s3->text)
					continue;
				if (s3->text[0] != '1') {
					s4 = s3;
					break;
				}
				rtie = s1;		/* 1st repeat bar */
			}
#endif
		}

		/* ties with clef or staff change */
		if (clef_chg || s1->staff != s4->staff) {
			float x, dx;

			clef_chg = 0;
			dx = (s4->x - s1->x) * 0.4;
			x = s4->x;
			s4->x -= dx;
			if (s4->x > s1->x + 32.)
				s4->x = s1->x + 32.;
			draw_ties_g(s1, s4, 2);
			s4->x = x;
			x = s1->x;
			s1->x += dx;
			if (s1->x < s4->x - 24.)
				s1->x = s4->x - 24.;
			draw_ties(s1, s4, 3);
			s1->x = x;
			continue;
		}
		draw_ties_g(s1, s4, s4->abc_type == ABC_T_NOTE ? 0 : 2);
	}
	p_voice->rtie = rtie;
}

/* -- draw all phrasing slurs for one staff -- */
/* (the staves are not yet defined) */
static void draw_all_slurs(struct VOICE_S *p_voice)
{
	struct SYMBOL *s, *k;
	int i, m2, slur_type;
	unsigned char slur_st;

	s = p_voice->sym;
	if (!s)
		return;
	slur_type = p_voice->slur_st;
	p_voice->slur_st = 0;

	/* the starting slur types are inverted */
	slur_st = 0;
	while (slur_type != 0) {
		slur_st <<= 4;
		slur_st |= (slur_type & 0x0f);
		slur_type >>= 4;
	}

	/* draw the slurs inside the music line */
	draw_slurs(s, NULL);

	/* do unbalanced slurs still left over */
	for ( ; s; s = s->next) {
		if (s->type != NOTEREST && s->type != SPACE)
			continue;
		while (s->u.note.slur_end
		    || (s->sflags & S_SL2)) {
			if (s->u.note.slur_end) {
				s->u.note.slur_end--;
				m2 = -1;
			} else {
				for (m2 = 0; m2 <= s->nhd; m2++)
					if (s->u.note.notes[m2].sl2)
						break;
				s->u.note.notes[m2].sl2--;
				if (s->u.note.notes[m2].sl2 == 0) {
					for (i = m2 + 1; i <= s->nhd; i++)
						if (s->u.note.notes[i].sl2)
							break;
					if (i > s->nhd)
						s->sflags &= ~S_SL2;
				}
			}
			slur_type = slur_st & 0x0f;
			k = prev_scut(s);
			draw_slur(k, s, -1, m2, slur_type);
			if (k->type != BAR
			 || (!(k->sflags & S_RRBAR)
			  && k->u.bar.type != B_THIN_THICK
			  && k->u.bar.type != B_THICK_THIN
			  && (!k->u.bar.repeat_bar
			   || !k->text
			   || k->text[0] == '1')))
				slur_st >>= 4;
		}
	}
	s = p_voice->sym;
	while (slur_st != 0) {
		slur_type = slur_st & 0x0f;
		slur_st >>= 4;
		k = next_scut(s);
		draw_slur(s, k, -1, -1, slur_type);
		if (k->type != BAR
		 || (!(k->sflags & S_RRBAR)
		  && k->u.bar.type != B_THIN_THICK
		  && k->u.bar.type != B_THICK_THIN
		  && (!k->u.bar.repeat_bar
		   || !k->text
		   || k->text[0] == '1'))) {
/*fixme: the slur types are inverted again*/
			p_voice->slur_st <<= 4;
			p_voice->slur_st += slur_type;
		}
	}
}

/* -- work out accidentals to be applied to each note -- */
static void setmap(int sf,	/* number of sharps/flats in key sig (-7 to +7) */
		   unsigned char *map)	/* for 7 notes only */
{
	int j;

	for (j = 7; --j >= 0; )
		map[j] = A_NULL;
	switch (sf) {
	case 7: map[6] = A_SH;
	case 6: map[2] = A_SH;
	case 5: map[5] = A_SH;
	case 4: map[1] = A_SH;
	case 3: map[4] = A_SH;
	case 2: map[0] = A_SH;
	case 1: map[3] = A_SH;
		break;
	case -7: map[3] = A_FT;
	case -6: map[0] = A_FT;
	case -5: map[4] = A_FT;
	case -4: map[1] = A_FT;
	case -3: map[5] = A_FT;
	case -2: map[2] = A_FT;
	case -1: map[6] = A_FT;
		break;
	}
}

/* output a tablature string escaping the parenthesis */
static void tbl_out(char *s, float x, int j, char *f)
{
	char *p;

	a2b("(");
	p = s;
	for (;;) {
		while (*p != '\0' && *p != '(' && *p != ')' )
			p++;
		if (p != s) {
			a2b("%.*s", (int) (p - s), s);
			s = p;
		}
		if (*p == '\0')
			break;
		a2b("\\");
		p++;
	}
	a2b(")%.1f y %d %s ", x, j, f);
}

/* -- draw the tablature with w: -- */
static void draw_tblt_w(struct VOICE_S *p_voice,
			int nly,
			float y,
			struct tblt_s *tblt)
{
	struct SYMBOL *s;
	struct lyrics *ly;
	struct lyl *lyl;
	char *p;
	int j, l;

	a2b("/y{%.1f yns%d}def ", y, p_voice->staff);
	set_font(VOCALFONT);
	a2b("%.1f 0 y %d %s\n", realwidth, nly, tblt->head);
	for (j = 0; j < nly ; j++) {
		for (s = p_voice->sym; s; s = s->next) {
			ly = s->ly;
			if (!ly
			 || (lyl = ly->lyl[j]) == NULL) {
				if (s->type == BAR) {
					if (!tblt->bar)
						continue;
					p = &tex_buf[16];
					*p-- = '\0';
					l = bar_cnv(s->u.bar.type);
					while (l != 0) {
						*p-- = "?|[]:???"[l & 0x07];
						l >>= 4;
					}
					p++;
					tbl_out(p, s->x, j, tblt->bar);
				}
				continue;
			}
			tbl_out(lyl->t, s->x, j, tblt->note);
		}
		a2b("\n");
	}
}

/* -- draw the tablature with automatic pitch -- */
static void draw_tblt_p(struct VOICE_S *p_voice,
			float y,
			struct tblt_s *tblt)
{
	struct SYMBOL *s;
	int j, pitch, octave, sf, tied, acc;
	unsigned char workmap[70];	/* sharps/flats - base: lowest 'C' */
	unsigned char basemap[7];
	static int scale[7] = {0, 2, 4, 5, 7, 9, 11};	/* index = natural note */
	static int acc_pitch[6] = {0, 1, 0, -1, 2, -2};	/* index = enum accidentals */

	sf = p_voice->key.sf;
	setmap(sf, basemap);
	for (j = 0; j < 10; j++)
		memcpy(&workmap[7 * j], basemap, 7);
	a2b("gsave 0 %.1f yns%d T(%.2s)%s\n", y, p_voice->staff,
		tblt->instr, tblt->head);
	tied = 0;
	for (s = p_voice->sym; s; s = s->next) {
		switch (s->type) {
		case NOTEREST:
			if (s->abc_type == ABC_T_REST)
				continue;
			if (tied) {
				tied = s->u.note.notes[0].ti1;
				continue;
			}
			break;
		case KEYSIG:
			sf = s->u.key.sf;
			setmap(sf, basemap);
			for (j = 0; j < 10; j++)
				memcpy(&workmap[7 * j], basemap, 7);
			continue;
		case BAR:
			if (s->flags & ABC_F_INVIS)
				continue;
			for (j = 0; j < 10; j++)
				memcpy(&workmap[7 * j], basemap, 7);
			continue;
		default:
			continue;
		}
		pitch = s->u.note.notes[0].pit + 19;
		acc = s->u.note.notes[0].acc;
		if (acc != 0) {
			workmap[pitch] = acc == A_NT
				? A_NULL
				: (acc & 0x07);
		}
		pitch = scale[pitch % 7]
			+ acc_pitch[workmap[pitch]]
			+ 12 * (pitch / 7)
			- tblt->pitch;
		octave = 0;
		while (pitch < 0) {
			pitch += 12;
			octave--;
		}
		while (pitch >= 36) {
			pitch -= 12;
			octave++;
		}
		if ((acc & 0xf8) == 0) {
			a2b("%d %d %.2f %s\n", octave, pitch, s->x, tblt->note);
		} else {
			int n, d;
			float micro_p;

			n = parse.micro_tb[acc >> 3];
			d = (n & 0xff) + 1;
			n = (n >> 8) + 1;
			switch (acc & 0x07) {
			case A_FT:
			case A_DF:
				n = -n;
				break;
			}
			micro_p = (float) pitch + (float) n / d;
			a2b("%d %.3f %.2f %s\n", octave, micro_p, s->x, tblt->note);
		}
		tied = s->u.note.notes[0].ti1;
	}
	a2b("grestore\n");
}

/* -- draw the lyrics under (or above) notes -- */
/* !! this routine is tied to set_width() !! */
static void draw_lyric_line(struct VOICE_S *p_voice,
			    int j)
{
	struct SYMBOL *s;
	struct lyrics *ly;
	struct lyl *lyl;
	int hyflag, l, lflag;
	int ft, curft, defft;
	char *p;
	float lastx, w;
	float x0, shift;

	hyflag = lflag = 0;
	if (p_voice->hy_st & (1 << j)) {
		hyflag = 1;
		p_voice->hy_st &= ~(1 << j);
	}
	for (s = p_voice->sym; /*s*/; s = s->next)
		if (s->type != CLEF
		 && s->type != KEYSIG && s->type != TIMESIG)
			break;
	if (s->prev)
		lastx = s->prev->x;
	else
		lastx = tsfirst->x;
	x0 = 0;
	for ( ; s; s = s->next) {
		ly = s->ly;
		if (!ly
		 || (lyl = ly->lyl[j]) == NULL) {
			switch (s->type) {
			case NOTEREST:
				if (s->abc_type == ABC_T_NOTE)
					break;
				/* fall thru */
			case MREST:
				if (lflag) {
					putx(x0 - lastx);
					putx(lastx + 3);
					a2b("y wln ");
					lflag = 0;
					lastx = s->x + s->wr;
				}
			}
			continue;
		}
#if 1
		ft = lyl->f - cfmt.font_tb;
		get_str_font(&curft, &defft);
		if (ft != curft) {
			set_str_font(ft, defft);
		}
#else
		if (lyl->f != f) {		/* font change */
			f = lyl->f;
			str_font(f - cfmt.font_tb);
			if (lskip < f->size * 1.1)
				lskip = f->size * 1.1;
		}
#endif
		p = lyl->t;
		w = lyl->w;
		shift = lyl->s;
		if (hyflag) {
			if (*p == LY_UNDER) {		/* '_' */
				*p = LY_HYPH;
			} else if (*p != LY_HYPH) {	/* not '-' */
				putx(s->x - shift - lastx);
				putx(lastx);
				a2b("y hyph ");
				hyflag = 0;
				lastx = s->x + s->wr;
			}
		}
		if (lflag
		 && *p != LY_UNDER) {		/* not '_' */
			putx(x0 - lastx + 3);
			putx(lastx + 3);
			a2b("y wln ");
			lflag = 0;
			lastx = s->x + s->wr;
		}
		if (*p == LY_HYPH		/* '-' */
		 || *p == LY_UNDER) {		/* '_' */
			if (x0 == 0 && lastx > s->x - 18)
				lastx = s->x - 18;
			if (*p == LY_HYPH)
				hyflag = 1;
			else
				lflag = 1;
			x0 = s->x - shift;
			continue;
		}
		x0 = s->x - shift;
		l = strlen(p) - 1;
		if (p[l] == LY_HYPH) {		/* '-' at end */
			p[l] = '\0';
			hyflag = 1;
		}
		putx(x0);
		a2b("y M ");
		put_str(p, A_LYRIC);
		lastx = x0 + w;
	}
	if (hyflag) {
		x0 = realwidth - 10;
		if (x0 < lastx + 10)
			x0 = lastx + 10;
		putx(x0 - lastx);
		putx(lastx);
		a2b("y hyph ");
		if (cfmt.hyphencont)
			p_voice->hy_st |= (1 << j);
	}

	/* see if any underscore in the next line */
	for (s = tsnext; s; s = s->ts_next)
		if (s->voice == p_voice - voice_tb)
			break;
	for ( ; s; s = s->next) {
		if (s->abc_type == ABC_T_NOTE) {
			if (s->ly && s->ly->lyl[j]
			 && s->ly->lyl[j]->t[0] == LY_UNDER) {
				lflag = 1;
				x0 = realwidth - 15;
				if (x0 < lastx + 12)
					x0 = lastx + 12;
			}
			break;
		}
	}
	if (lflag) {
		putx(x0 - lastx + 3);
		putx(lastx + 3);
		a2b("y wln");
	}
	a2b("\n");
}

static float draw_lyrics(struct VOICE_S *p_voice,
			 int nly,
			 float *h,
			 float y,
			 int incr)	/* 1: below, -1: above */
{
	int j, top;
	float sc;

	/* check if the lyrics contain tablatures */
	if (p_voice->tblts[0]) {
		if (p_voice->tblts[0]->pitch == 0)
			return y;		/* yes */
		if (p_voice->tblts[1]
		 && p_voice->tblts[1]->pitch == 0)
			return y;		/* yes */
	}

	str_font(VOCALFONT);
	outft = -1;				/* force font output */
	sc = staff_tb[p_voice->staff].staffscale;

	/* under the staff */
	if (incr > 0) {
		if (y > -cfmt.vocalspace)
			y = -cfmt.vocalspace;
		y += h[0] / 6;			// descent
		y *= sc;
		for (j = 0; j < nly; j++) {
			y -= h[j] * 1.1;
			a2b("/y{%.1f yns%d}! ", y, p_voice->staff);
			draw_lyric_line(p_voice, j);
		}
		return (y - h[j - 1] / 6) / sc;
	}

	/* above the staff */
	top = staff_tb[p_voice->staff].topbar + cfmt.vocalspace;
	if (y < top)
		y = top;
	y += h[nly - 1] / 6;			// descent
	y *= sc;
	for (j = nly; --j >= 0; ) {
		a2b("/y{%.1f yns%d}! ", y, p_voice->staff);
		draw_lyric_line(p_voice, j);
		y += h[j] * 1.1;
	}
	return y / sc;
}

/* -- draw all the lyrics and the tablatures -- */
/* (the staves are not yet defined) */
static void draw_all_lyrics(void)
{
	struct VOICE_S *p_voice;
	struct SYMBOL *s;
	int staff, voice, nly, i;
	struct {
		short a, b;
		float top, bot;
	} lyst_tb[MAXSTAFF];
	struct {
		int nly;
		float h[MAXLY];
	} lyvo_tb[MAXVOICE];
	char above_tb[MAXVOICE];
	char rv_tb[MAXVOICE];
	float top, bot, y, sc;

	/* check if any lyric */
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		if (p_voice->have_ly
		 || p_voice->tblts[0])
			break;
	}
	if (!p_voice)
		return;

	/* compute the number of lyrics per voice - staff
	 * and their y offset on the staff */
	memset(above_tb, 0, sizeof above_tb);
	memset(lyvo_tb, 0, sizeof lyvo_tb);
	memset(lyst_tb, 0, sizeof lyst_tb);
	staff = -1;
	top = bot = 0;
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		if (!p_voice->sym)
			continue;
		voice = p_voice - voice_tb;
		if (p_voice->staff != staff) {
			top = 0;
			bot = 0;
			staff = p_voice->staff;
		}
		nly = -1;
		if (p_voice->have_ly) {
			for (s = p_voice->sym; s; s = s->next) {
				struct lyrics *ly;
				float x, w;

				ly = s->ly;
				if (!ly)
					continue;
/*fixme:should get the real width*/
				x = s->x;
				if (ly->lyl[0]) {
					x -= ly->lyl[0]->s;
					w = ly->lyl[0]->w;
				} else {
					w = 10;
				}
				y = y_get(p_voice->staff, 1, x, w);
				if (top < y)
					top = y;
				y = y_get(p_voice->staff, 0, x, w);
				if (bot > y)
					bot = y;
				for (i = 0; i < MAXLY; i++) {
					if (!ly->lyl[i])
						continue;
					if (i > nly)
						nly = i;
					if (lyvo_tb[voice].h[i] < ly->lyl[i]->f->size)
						lyvo_tb[voice].h[i] =
							ly->lyl[i]->f->size;
				}
			}
		} else {
			y = y_get(p_voice->staff, 1, 0, realwidth);
			if (top < y)
				top = y;
			y = y_get(p_voice->staff, 0, 0, realwidth);
			if (bot > y)
				bot = y;
		}
		lyst_tb[staff].top = top;
		lyst_tb[staff].bot = bot;
		if (nly < 0)
			continue;
		nly++;
		lyvo_tb[voice].nly = nly;
		if (p_voice->posit.voc != 0)
			above_tb[voice] = p_voice->posit.voc == SL_ABOVE;
		else if (p_voice->next
/*fixme:%%staves:KO - find an other way..*/
		      && p_voice->next->staff == staff
		      && p_voice->next->have_ly)
			above_tb[voice] = 1;
		if (above_tb[voice])
			lyst_tb[staff].a = 1;
		else
			lyst_tb[staff].b = 1;
	}

	/* draw the lyrics under the staves */
	i = 0;
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		struct tblt_s *tblt;

		if (!p_voice->sym)
			continue;
		if (!p_voice->have_ly
		 && !p_voice->tblts[0])
			continue;
		voice = p_voice - voice_tb;
		if (above_tb[voice]) {
			rv_tb[i++] = voice;
			continue;
		}
		staff = p_voice->staff;
		if (lyvo_tb[voice].nly > 0)
			lyst_tb[staff].bot = draw_lyrics(p_voice, lyvo_tb[voice].nly,
							 lyvo_tb[voice].h,
							 lyst_tb[staff].bot, 1);
		sc = staff_tb[p_voice->staff].staffscale;
		for (nly = 0; nly < 2; nly++) {
			if ((tblt = p_voice->tblts[nly]) == NULL)
				break;
//			if (tblt->hu > 0) {
//				lyst_tb[staff].bot -= tblt->hu;
//				lyst_tb[staff].b = 1;
//			}
			if (tblt->pitch == 0)
				draw_tblt_w(p_voice,
					lyvo_tb[voice].nly,
					lyst_tb[staff].bot * sc - tblt->hu,
					tblt);
			else
				draw_tblt_p(p_voice,
//					lyst_tb[staff].bot,
					lyst_tb[staff].bot * sc - tblt->hu,
					tblt);
			if (tblt->hu > 0) {
				lyst_tb[staff].bot -= tblt->hu / sc;
				lyst_tb[staff].b = 1;
			}
			if (tblt->ha != 0) {
				lyst_tb[staff].top += tblt->ha / sc;
				lyst_tb[staff].a = 1;
			}
		}
	}

	/* draw the lyrics above the staff */
	while (--i >= 0) {
		voice = rv_tb[i];
		p_voice = &voice_tb[voice];
		staff = p_voice->staff;
		lyst_tb[staff].top = draw_lyrics(p_voice, lyvo_tb[voice].nly,
						 lyvo_tb[voice].h,
						 lyst_tb[staff].top, -1);
	}

	/* set the max y offsets of all symbols */
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		if (!p_voice->sym)
			continue;
		staff = p_voice->staff;
		if (lyst_tb[staff].a) {
			top = lyst_tb[staff].top + 2;
			for (s = p_voice->sym; s; s = s->next) {
/*fixme: may have lyrics crossing a next symbol*/
				if (s->ly) {
/*fixme:should set the real width*/
					y_set(staff, 1, s->x - 2, 10, top);
				}
			}
		}
		if (lyst_tb[staff].b) {
			bot = lyst_tb[staff].bot - 2;
			if (lyvo_tb[p_voice - voice_tb].nly > 0) {
				for (s = p_voice->sym; s; s = s->next) {
					if (s->ly) {
/*fixme:should set the real width*/
						y_set(staff, 0, s->x - 2, 10, bot);
					}
				}
			} else {
				y_set(staff, 0, 0, realwidth, bot);
			}
		}
	}
}

/* -- draw the symbols near the notes -- */
/* (the staves are not yet defined) */
/* order:
 * - beams
 * - decorations near the notes
 * - measure bar numbers
 * - n-plets
 * - decorations tied to the notes
 * - slurs
 * - guitar chords
 * - then remaining decorations
 * The buffer output is delayed until the definition of the staff system,
 * so, global variables must be saved (see music.c delayed_output()).
 */
void draw_sym_near(void)
{
	struct VOICE_S *p_voice;
	struct SYMBOL *s, *g;
	int i, staff;

	/* calculate the beams but don't draw them (the staves are not yet defined) */
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		struct BEAM bm;
		int first_note = 1;

		for (s = p_voice->sym; s; s = s->next) {
			for (g = s->extra; g; g = g->next) {
				if (g->type != NOTEREST)
					continue;
				if ((g->sflags & (S_BEAM_ST | S_BEAM_END))
							== S_BEAM_ST)
					calculate_beam(&bm, g);
			}
			if (s->abc_type != ABC_T_NOTE)
				continue;
			if (((s->sflags & S_BEAM_ST) && !(s->sflags & S_BEAM_END))
			 || (first_note && !(s->sflags & S_BEAM_ST))) {
				first_note = 0;
				calculate_beam(&bm, s);
			}
		}
	}

	/* initialize the y offsets */
	for (staff = 0; staff <= nstaff; staff++) {
		for (i = 0; i < YSTEP; i++) {
			staff_tb[staff].top[i] = 0;
			staff_tb[staff].bot[i] = 24;
		}
	}

	set_tie_room();
	draw_deco_near();

	/* set the min/max vertical offsets */
	for (s = tsfirst; s; s = s->ts_next) {
		int y;

		if (s->flags & ABC_F_INVIS)
			continue;
		if (s->type == GRACE) {
			for (g = s->extra; g; g = g->next) {
				y_set(s->staff, 1, g->x - 2, 4,
						g->ymx + 1);
				y_set(s->staff, 0, g->x - 2, 4,
						g->ymn - 1);
			}
			continue;
		}
		if (s->type != MREST) {
			y_set(s->staff, 1, s->x - s->wl, s->wl + s->wr, s->ymx + 2);
			y_set(s->staff, 0, s->x - s->wl, s->wl + s->wr, s->ymn - 2);
		} else {
			y_set(s->staff, 1, s->x - 16, 32, s->ymx + 2);
		}
		if (s->abc_type != ABC_T_NOTE)
			continue;

		/* have room for the accidentals */
		if (s->u.note.notes[s->nhd].acc) {
			y = s->y + 8;
			if (s->ymx < y)
				s->ymx = y;
			y_set(s->staff, 1, s->x, 0., y);
		}
		if (s->u.note.notes[0].acc) {
			y = s->y;
			if ((s->u.note.notes[0].acc & 0x07) == A_SH
			 || s->u.note.notes[0].acc == A_NT)
				y -= 7;
			else
				y -= 5;
			if (s->ymn > y)
				s->ymn = y;
			y_set(s->staff, 0, s->x, 0., y);
		}
	}

	if (cfmt.measurenb >= 0)
		draw_measnb();

//	draw_deco_note();

	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		s = p_voice->sym;
		if (!s)
			continue;
//color
		set_color(s->color);
		set_sscale(p_voice->staff);

		/* draw the tuplets near the notes */
		for ( ; s; s = s->next) {
			if (s->sflags & S_IN_TUPLET) {
				for (g = s->extra; g; g = g->next) {
					if (g->type == TUPLET) {
						s = draw_tuplet(g, s);
						break;
					}
				}
			}
		}
		draw_all_slurs(p_voice);

		/* draw the tuplets over the slurs */
		for (s = p_voice->sym; s; s = s->next) {
			if (s->sflags & S_IN_TUPLET) {
				for (g = s->extra ; g; g = g->next) {
					if (g->type == TUPLET) {
						s = draw_tuplet(g, s);
						break;
					}
				}
			}
		}
	}

	/* set the top and bottom for all symbols to be out of the staves */
	{
		int top, bot;

		for (staff = 0; staff <= nstaff; staff++) {
			top = staff_tb[staff].topbar + 2;
			bot = staff_tb[staff].botbar - 2;
			for (i = 0; i < YSTEP; i++) {
				if (top > staff_tb[staff].top[i])
					staff_tb[staff].top[i] = (float) top;
				if (bot < staff_tb[staff].bot[i])
					staff_tb[staff].bot[i] = (float) bot;
			}
		}
	}
	set_color(0);
	draw_deco_note();
	draw_deco_staff();
	set_sscale(-1);		/* restore the scale parameters */
	draw_all_lyrics();
}

/* -- draw the name/subname of the voices -- */
static void draw_vname(float indent)
{
	struct VOICE_S *p_voice;
	int n, staff;
	struct {
		int nl;
		char *v[8];
	} staff_d[MAXSTAFF], *staff_p;
	char *p, *q;
	float y;

	for (staff = cursys->nstaff; staff >= 0; staff--) {
		if (!cursys->staff[staff].empty)
			break;
	}
	if (staff < 0)
		return;

	memset(staff_d, 0, sizeof staff_d);
	n = 0;
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		if (!p_voice->sym)
			continue;
		staff = cursys->voice[p_voice - voice_tb].staff;
		if (cursys->staff[staff].empty)
			continue;
		if (p_voice->new_name) {
			p_voice->new_name = 0;
			p = p_voice->nm;
		} else {
			p = p_voice->snm;
		}
		if (!p)
			continue;
		if (cursys->staff[staff].flags & CLOSE_BRACE2) {
			while (!(cursys->staff[staff].flags & OPEN_BRACE2))
				staff--;
		} else if (cursys->staff[staff].flags & CLOSE_BRACE) {
			while (!(cursys->staff[staff].flags & OPEN_BRACE))
				staff--;
		}
		staff_p = &staff_d[staff];
		for (;;) {
			staff_p->v[staff_p->nl++] = p;
			p = strstr(p, "\\n");
			if (!p
			 || staff_p->nl >= MAXSTAFF)
				break;
			p += 2;
		}
		n++;
	}
	if (n == 0)
		return;
	str_font(VOICEFONT);
	indent = -indent * .5;			/* center */
	for (staff = nstaff; staff >= 0; staff--) {
		staff_p = &staff_d[staff];
		if (staff_p->nl == 0)
			continue;
		y = staff_tb[staff].y
			+ staff_tb[staff].topbar * .5
				* staff_tb[staff].staffscale
			+ 9 * (staff_p->nl - 1)
			- cfmt.font_tb[VOICEFONT].size * .3;
		n = staff;
		if (cursys->staff[staff].flags & OPEN_BRACE2) {
			while (!(cursys->staff[n].flags & CLOSE_BRACE2))
				n++;
		} else if (cursys->staff[staff].flags & OPEN_BRACE) {
			while (!(cursys->staff[n].flags & CLOSE_BRACE))
				n++;
		}
		if (n != staff)
			y -= (staff_tb[staff].y - staff_tb[n].y) * .5;
		for (n = 0; n < staff_p->nl; n++) {
			p = staff_p->v[n];
			q = strstr(p, "\\n");
			if (q)
				*q = '\0';
			a2b("%.1f %.1f M ", indent, y);
			put_str(p, A_CENTER);
			y -= 18.;
			if (q)
				*q = '\\';
		}
	}
}

/* -- set the y offset of the staves and return the whole height -- */
static float set_staff(void)
{
	struct SYSTEM *sy;
	struct SYMBOL *s;
	int i, staff, prev_staff;
	float y, staffsep, dy, maxsep, mbot, v;
	char empty[MAXSTAFF];

	/* search the empty staves in each parts */
	memset(empty, 1, sizeof empty);
	for (staff = 0; staff <= nstaff; staff++)
		staff_tb[staff].empty = 0;
	sy = cursys;
	for (staff = 0; staff <= nstaff; staff++) {
		if (!sy->staff[staff].empty)
			empty[staff] = 0;
	}
	for (s = tsfirst; s; s = s->ts_next) {
		if (!(s->sflags & S_NEW_SY))
			continue;
		sy = sy->next;
		for (staff = 0; staff <= nstaff; staff++) {
			if (!sy->staff[staff].empty)
				empty[staff] = 0;
		}
	}

	/* output the scale of the voices
	 * and flag as non empty the staves with tablatures */
	{
		struct VOICE_S *p_voice;

		for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
			if (p_voice->scale != 1)
				a2b("/scvo%d{gsave %.2f dup scale}!\n",
				     (int) (p_voice - voice_tb),
				     p_voice->scale);
			if (p_voice->tblts[0])
				empty[p_voice->staff] = 0;
		}
	}

	/* set the vertical offset of the 1st staff */
	for (staff = 0; staff <= nstaff; staff++) {
		if (!empty[staff])
			break;
		staff_tb[staff].empty = 1;
	}
	y = 0;
	if (staff > nstaff) {
		staff--;			/* one staff, empty */
	} else {
		for (i = 0; i < YSTEP; i++) {
			v = staff_tb[staff].top[i];
			if (y < v)
				y = v;
		}
	}

	/* draw the parts and tempo indications if any */
	y += draw_partempo(staff, y);

	if (empty[staff])
		return y;

	y *= staff_tb[staff].staffscale;
	staffsep = cfmt.staffsep * 0.5 +
			staff_tb[staff].topbar * staff_tb[staff].staffscale;
	if (y < staffsep)
		y = staffsep;
	staff_tb[staff].y = -y;

	/* set the offset of the other staves */
	prev_staff = staff;
	for (staff++; staff <= nstaff; staff++) {
		if (empty[staff]) {
			staff_tb[staff].empty = 1;
			continue;
		}
		if (sy->staff[prev_staff].sep != 0)
			staffsep = sy->staff[prev_staff].sep;
		else
			staffsep = cfmt.sysstaffsep;
		if (sy->staff[prev_staff].maxsep != 0)
			maxsep = sy->staff[prev_staff].maxsep;
		else
			maxsep = cfmt.maxsysstaffsep;

		dy = 0;
		if (staff_tb[staff].staffscale
				== staff_tb[prev_staff].staffscale) {
			for (i = 0; i < YSTEP; i++) {
				v = staff_tb[staff].top[i]
				  - staff_tb[prev_staff].bot[i];
				if (dy < v)
					dy = v;
			}
			dy *= staff_tb[staff].staffscale;
		} else {
			for (i = 0; i < YSTEP; i++) {
				v = staff_tb[staff].top[i]
					* staff_tb[staff].staffscale
				  - staff_tb[prev_staff].bot[i]
					* staff_tb[prev_staff].staffscale;
				if (dy < v)
					dy = v;
			}
		}
		staffsep += staff_tb[staff].topbar
				* staff_tb[staff].staffscale;
		if (dy < staffsep)
			dy = staffsep;
		maxsep += staff_tb[staff].topbar
				* staff_tb[staff].staffscale;
		if (dy > maxsep)
			dy = maxsep;
		y += dy;
		staff_tb[staff].y = -y;

		prev_staff = staff;
	}
	mbot = 0;
	for (i = 0; i < YSTEP; i++) {
		v = staff_tb[prev_staff].bot[i];
		if (mbot > v)
			mbot = v;
	}
	mbot *= staff_tb[prev_staff].staffscale;

	/* output the staff offsets */
	for (staff = nstaff; staff >= 0; staff--) {
		dy = staff_tb[staff].y;
		if (staff_tb[staff].staffscale != 1
		 && staff_tb[staff].staffscale != 0) {
			a2b("/scst%d{gsave 0 %.2f T %.2f dup scale}!\n",
			     staff, dy, staff_tb[staff].staffscale);
			a2b("/y%d{}!\n", staff);
		} else {
			a2b("/y%d{%.1f add}!\n", staff, dy);
		}
		a2b("/yns%d{%.1f add}!\n", staff, dy);
	}

	if (mbot == 0) {
		for (staff = nstaff; staff >= 0; staff--) {
			if (!empty[staff])
				break;
		}
		if (staff < 0)		/* no symbol in this system ! */
			return y;
	}
	dy = -mbot;
	staffsep = cfmt.staffsep * 0.5;
	if (dy < staffsep)
		dy = staffsep;
	maxsep = cfmt.maxstaffsep * 0.5;
	if (dy > maxsep)
		dy = maxsep;

	/* return the whole staff system height */
	return y + dy;
}

/* -- set the bottom and height of the measure bars -- */
static void bar_set(float *bar_bot, float *bar_height, float *xstaff)
{
	int staff;
	float dy, staffscale, top, bot;

	dy = 0;
	for (staff = 0; staff <= cursys->nstaff; staff++) {
//		if (cursys->staff[staff].empty) {
		if (xstaff[staff] < 0) {
			bar_bot[staff] = bar_height[staff] = 0;
			continue;
		}
		staffscale = staff_tb[staff].staffscale;
		top = staff_tb[staff].topbar * staffscale;
		bot = staff_tb[staff].botbar * staffscale;
		if (dy == 0)
			dy = staff_tb[staff].y + top;
		bar_bot[staff] = staff_tb[staff].y + bot;
		bar_height[staff] = dy - bar_bot[staff];

		if (cursys->staff[staff].flags & STOP_BAR)
			dy = 0;
		else
			dy = bar_bot[staff];
	}
}

/* -- draw the staff systems and the measure bars -- */
float draw_systems(float indent)
{
	struct SYMBOL *s, *s2;
	int staff, bar_force;
	float xstaff[MAXSTAFF], bar_bot[MAXSTAFF], bar_height[MAXSTAFF];
	float staves_bar, x, x2, line_height;

	line_height = set_staff();
	draw_vname(indent);

	/* draw the staff, skipping the staff breaks */
	for (staff = 0; staff <= nstaff; staff++)
		xstaff[staff] = cursys->staff[staff].empty ? -1 : 0;
	bar_set(bar_bot, bar_height, xstaff);
	draw_lstaff(0);
	bar_force = 0;
	for (s = tsfirst; s; s = s->ts_next) {
		if (bar_force && s->time != bar_force) {
			bar_force = 0;
			for (staff = 0; staff <= nstaff; staff++) {
				if (cursys->staff[staff].empty)
					xstaff[staff] = -1;
			}
			bar_set(bar_bot, bar_height, xstaff);
		}
		if (s->sflags & S_NEW_SY) {
			staves_bar = 0;
			for (s2 = s; s2; s2 = s2->ts_next) {
				if (s2->time != s->time)
					break;
				if (s2->type == BAR) {
					staves_bar = s2->x;
					break;
				}
			}
			cursys = cursys->next;
			for (staff = 0; staff <= nstaff; staff++) {
				x = xstaff[staff];
				if (x < 0) {			// no staff yet
					if (!cursys->staff[staff].empty) {
						if (staves_bar != 0)
							xstaff[staff] = staves_bar;
						else
							xstaff[staff] = s->x - s->wl - 2;
					}
					continue;
				}
				if (!cursys->staff[staff].empty) // if not staff stop
					continue;
				if (staves_bar != 0) {
					x2 = staves_bar;
					bar_force = s->time;
				} else {
					x2 = s->x - s->wl - 2;
					xstaff[staff] = -1;
				}
				draw_staff(staff, x, x2);
			}
			bar_set(bar_bot, bar_height, xstaff);
		}
		staff = s->staff;
		switch (s->type) {
		case BAR:
			if ((s->sflags & S_SECOND)
//			 || cursys->staff[staff].empty)
			 || xstaff[staff] < 0)
				s->flags |= ABC_F_INVIS;
			if (s->flags & ABC_F_INVIS)
				break;
			draw_bar(s, bar_bot[staff], bar_height[staff]);
			if (annotate)
				anno_out(s, 'B');
			break;
		case STBRK:
			if (cursys->voice[s->voice].range == 0) {
				if (s->xmx > .5 CM) {
					int i, nvoice;

					/* draw the left system if stbrk in all voices */
					nvoice = 0;
					for (i = 0; i < MAXVOICE; i++) {
						if (cursys->voice[i].range > 0)
							nvoice++;
					}
					for (s2 = s->ts_next; s2; s2 = s2->ts_next) {
						if (s2->type != STBRK)
							break;
						nvoice--;
					}
					if (nvoice == 0)
						draw_lstaff(s->x);
				}
			}
			s2 = s->prev;
			if (!s2)
				break;
			x2 = s2->x;
			if (s2->type != BAR)
				x2 += s2->wr;
			staff = s->staff;
			x = xstaff[staff];
			if (x >= 0) {
				if (x >= x2)
					continue;
				draw_staff(staff, x, x2);
			}
			xstaff[staff] = s->x;
			break;
		default:
//fixme:does not work for "%%staves K: M: $" */
//removed for K:/M: in empty staves
//			if (cursys->staff[staff].empty)
//				s->flags |= ABC_F_INVIS;
			break;
		}
	}

	// draw the end of the staves
	for (staff = 0; staff <= nstaff; staff++) {
		if (bar_force && cursys->staff[staff].empty)
			continue;
		if ((x = xstaff[staff]) < 0)
			continue;
		draw_staff(staff, x, realwidth);
	}
	set_sscale(-1);
	return line_height;
}

/* -- output PostScript sequences and set the staff and voice colors -- */
void output_ps(struct SYMBOL *s, int color)
{
	struct SYMBOL *g, *g2;

	g = s->extra;
	g2 = NULL;
	for ( ; g; g = g->next) {
		if (g->type == FMTCHG) {
			switch (g->aux) {
			case PSSEQ:
//			case SVGSEQ:
//				if (g->aux == SVGSEQ)
//					a2b("%%svg %s\n", g->text);
//				else
					a2b("%s\n", g->text);
				if (!g2)
					s->extra = g->next;
				else
					g2->next = g->next;
				continue;
			}
		}
		g2 = g;
	}
}

/* -- draw remaining symbols when the staves are defined -- */
static void draw_symbols(struct VOICE_S *p_voice)
{
	struct BEAM bm;
	struct SYMBOL *s;
	float x, y;
	int staff, first_note;

#if 0
	/* output the PostScript code at start of line */
	for (s = p_voice->sym; s; s = s->next) {
		if (s->extra)
			output_ps(s, 1);
		switch (s->type) {
		case CLEF:
		case KEYSIG:
		case TIMESIG:
		case BAR:
			continue;	/* skip the symbols added by init_music_line() */
		}
		break;
	}
#endif

	bm.s2 = NULL;
	first_note = 1;
	for (s = p_voice->sym; s; s = s->next) {
		if (s->extra)
			output_ps(s, 1);
		if (s->flags & ABC_F_INVIS) {
			switch (s->type) {
			case KEYSIG:
				memcpy(&p_voice->key, &s->u.key,
					sizeof p_voice->key);
			default:
				continue;
			case NOTEREST:
			case GRACE:
				break;
			}
		}
		set_color(s->color);
		x = s->x;
		switch (s->type) {
		case NOTEREST:
			set_scale(s);
			if (s->abc_type == ABC_T_NOTE) {
				if ((s->sflags & (S_BEAM_ST | S_BEAM_END)) == S_BEAM_ST
				 || (first_note && !(s->sflags & S_BEAM_ST))) {
					first_note = 0;
					if (calculate_beam(&bm, s)) {
						if (annotate)
							anno_out(s, 'b');
						draw_beams(&bm);
					}
				}
				draw_note(x, s, bm.s2 == NULL);
				if (annotate)
					anno_out(s, 'N');
				if (s == bm.s2)
					bm.s2 = NULL;
				if (annotate
				 && (s->sflags & (S_BEAM_ST | S_BEAM_END))
							== S_BEAM_END)
					anno_out(s, 'e');
				break;
			}
			draw_rest(s);
			if (annotate)
				anno_out(s, 'R');
			break;
		case BAR:
			break;			/* drawn in draw_systems */
		case CLEF:
			staff = s->staff;
			if (s->sflags & S_SECOND)
/*			 || p_voice->staff != staff)	*/
				break;		/* only one clef per staff */
			if ((s->flags & ABC_F_INVIS)
			 || staff_tb[staff].empty)
				break;
			set_color(0);
			set_sscale(staff);
			y = staff_tb[staff].y;
			x -= 10;		/* clef shift - see set_width() */
			putxy(x, y + s->y);
			if (s->u.clef.name)
				a2b("%s\n", s->u.clef.name);
			else
				a2b("%c%cclef\n",
				     s->aux ? 's' : ' ',
				     "tcbp"[(unsigned) s->u.clef.type]);
			if (s->u.clef.octave != 0) {
/*fixme:break the compatibility and avoid strange numbers*/
				if (s->u.clef.octave > 0)
					y += s->ymx - 9;
				else
					y += s->ymn;
				putxy(x - 2, y);
				a2b("oct\n");
			}
			if (annotate)
				anno_out(s, 'c');
			break;
		case TIMESIG:
//fixme: set staff color
			memcpy(&p_voice->meter, &s->u.meter,
					sizeof p_voice->meter);
			if ((s->sflags & S_SECOND)
			 || staff_tb[s->staff].empty)
				break;
			if (cfmt.alignbars && s->staff != 0)
				break;
			set_color(0);
			set_sscale(s->staff);
			draw_timesig(x, s);
			if (annotate)
				anno_out(s, 'M');
			break;
		case KEYSIG:
//fixme: set staff color
			memcpy(&p_voice->key, &s->u.key,
					sizeof p_voice->key);
			if ((s->sflags & S_SECOND)
			 || staff_tb[s->staff].empty)
				break;
			set_color(0);
			set_sscale(s->staff);
			draw_keysig(p_voice, x, s);
			if (annotate)
				anno_out(s, 'K');
			break;
		case MREST:
			set_scale(s);
			putxy(x, staff_tb[s->staff].y + 12);
			a2b("mrest(%d)/Times-Bold 15 selectfont ",
				s->u.bar.len);
			putxy(x, staff_tb[s->staff].y + 28);
			a2b("M showc\n");
			if (annotate)
				anno_out(s, 'Z');
			break;
		case GRACE:
			set_scale(s);
			draw_gracenotes(s);
			break;
		case SPACE:
		case STBRK:
		case FMTCHG:
			break;			/* nothing */
		case CUSTOS:
			set_scale(s);
			s->sflags |= ABC_F_STEMLESS;
			draw_note(x, s, 0);
			break;
		default:
			bug("Symbol not drawn", 1);
		}
	}
	set_scale(p_voice->sym);
	draw_all_ties(p_voice);
	set_sscale(-1);
	set_color(0);
}

/* -- draw all symbols -- */
void draw_all_symb(void)
{
	struct VOICE_S *p_voice;
	struct SYMBOL *s;

	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
#if 1 /* pb about rest display when "%%staffnonote 0" fixed in draw_rest */
		if (p_voice->sym)
#else
		if (p_voice->sym
		 && !staff_tb[p_voice->staff].empty)
#endif
			draw_symbols(p_voice);
	}

	// update the clefs
	for (s = tsfirst; s; s = s->ts_next) {
		if (s->type == CLEF)
			staff_tb[s->staff].s_clef = s;
	}
}

/* -- output a floating value, and x and y according to the current scale -- */
void putf(float v)
{
	a2b("%.1f ", v);
}

void putx(float x)
{
	putf(x / cur_scale);
}

void puty(float y)
{
	putf(scale_voice ?
		y / cur_scale :		/* scaled voice */
		y - cur_trans);		/* scaled staff */
}

void putxy(float x, float y)
{
	if (scale_voice)
		a2b("%.1f %.1f ",
		     x / cur_scale, y / cur_scale);	/* scaled voice */
	else
		a2b("%.1f %.1f ",
		     x / cur_scale, y - cur_trans);	/* scaled staff */
}

/* -- set the voice or staff scale -- */
void set_scale(struct SYMBOL *s)
{
	float scale, trans;

	scale = voice_tb[s->voice].scale;
	if (scale == 1) {
		set_sscale(s->staff);
		return;
	}
/*fixme: KO when both staff and voice are scaled */
	trans = 0;
	scale_voice = 1;
	if (scale == cur_scale && trans == cur_trans)
		return;
	if (cur_scale != 1)
		a2b("grestore ");
	cur_scale = scale;
	cur_trans = trans;
	if (scale != 1)
		a2b("scvo%d ", s->voice);
}

/* -- set the staff scale (only) -- */
void set_sscale(int staff)
{
	float scale, trans;

	scale_voice = 0;
	if (staff != cur_staff && cur_scale != 1)
		cur_scale = 0;
	if (staff >= 0)
		scale = staff_tb[staff].staffscale;
	else
		scale = 1;
	if (staff >= 0 && scale != 1)
		trans = staff_tb[staff].y;
	else
		trans = 0;
	if (scale == cur_scale && trans == cur_trans)
		return;
	if (cur_scale != 1)
		a2b("grestore ");
	cur_scale = scale;
	cur_trans = trans;
	if (scale != 1) {
		a2b("scst%d ", staff);
		cur_staff = staff;
	}
}

/* -- set the tie directions for one voice -- */
static void set_tie_dir(struct SYMBOL *sym)
{
	struct SYMBOL *s;
	int i, ntie, dir, sec, pit, ti;

	for (s = sym; s; s = s->next) {
		if (!(s->sflags & S_TI1))
			continue;

		/* if other voice, set the ties in opposite direction */
		if (s->multi != 0) {
/*			struct SYMBOL *s2;

			s2 = s->ts_next;
			if (s2->time == s->time && s2->staff == s->staff) { */
				dir = s->multi > 0 ? SL_ABOVE : SL_BELOW;
				for (i = 0; i <= s->nhd; i++) {
					ti = s->u.note.notes[i].ti1;
					if (!((ti & 0x07) == SL_AUTO))
						continue;
					s->u.note.notes[i].ti1 = (ti & SL_DOTTED) | dir;
				}
				continue;
/*			} */
		}

		/* if one note, set the direction according to the stem */
		sec = ntie = 0;
		pit = 128;
		for (i = 0; i <= s->nhd; i++) {
			if (s->u.note.notes[i].ti1) {
				ntie++;
				if (pit < 128
				 && s->u.note.notes[i].pit <= pit + 1)
					sec++;
				pit = s->u.note.notes[i].pit;
			}
		}
		if (ntie <= 1) {
			dir = s->stem < 0 ? SL_ABOVE : SL_BELOW;
			for (i = 0; i <= s->nhd; i++) {
				ti = s->u.note.notes[i].ti1;
				if (ti != 0) {
					if ((ti & 0x07) == SL_AUTO)
						s->u.note.notes[i].ti1 =
							(ti & SL_DOTTED) | dir;
					break;
				}
			}
			continue;
		}
		if (sec == 0) {
			if (ntie & 1) {
/* in chords with an odd number of notes, the outer noteheads are paired off
 * center notes are tied according to their position in relation to the
 * center line */
				ntie /= 2;
				dir = SL_BELOW;
				for (i = 0; i <= s->nhd; i++) {
					ti = s->u.note.notes[i].ti1;
					if (ti == 0)
						continue;
					if (ntie == 0) {	/* central tie */
						if (s->pits[i] >= 22)
							dir = SL_ABOVE;
					}
					if ((ti & 0x07) == SL_AUTO)
						s->u.note.notes[i].ti1 =
							(ti & SL_DOTTED) | dir;
					if (ntie-- == 0)
						dir = SL_ABOVE;
				}
				continue;
			}
/* even number of notes, ties divided in opposite directions */
			ntie /= 2;
			dir = SL_BELOW;
			for (i = 0; i <= s->nhd; i++) {
				ti = s->u.note.notes[i].ti1;
				if (ti == 0)
					continue;
				if ((ti & 0x07) == SL_AUTO)
					s->u.note.notes[i].ti1 =
						(ti & SL_DOTTED) | dir;
				if (--ntie == 0)
					dir = SL_ABOVE;
			}
			continue;
		}
/*fixme: treat more than one second */
/*		if (nsec == 1) {	*/
/* When a chord contains the interval of a second, tie those two notes in
 * opposition; then fill in the remaining notes of the chord accordingly */
			pit = 128;
			for (i = 0; i <= s->nhd; i++) {
				if (s->u.note.notes[i].ti1) {
					if (pit < 128
					 && s->u.note.notes[i].pit <= pit + 1) {
						ntie = i;
						break;
					}
					pit = s->u.note.notes[i].pit;
				}
			}
			dir = SL_BELOW;
			for (i = 0; i <= s->nhd; i++) {
				ti = s->u.note.notes[i].ti1;
				if (ti == 0)
					continue;
				if (ntie == i)
					dir = SL_ABOVE;
				if ((ti & 0x07) == SL_AUTO)
					s->u.note.notes[i].ti1 =
							(ti & SL_DOTTED) | dir;
			}
/*fixme..
			continue;
		}
..*/
/* if a chord contains more than one pair of seconds, the pair farthest
 * from the center line receives the ties drawn in opposition */
	}
}

/* -- have room for the ties out of the staves -- */
static void set_tie_room(void)
{
	struct VOICE_S *p_voice;
	struct SYMBOL *s, *s2;

	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		s = p_voice->sym;
		if (!s)
			continue;
		s = s->next;
		if (!s)
			continue;
		set_tie_dir(s);
		for ( ; s; s = s->next) {
			float dx, y, dy;

			if (!(s->sflags & S_TI1))
				continue;
			if (s->pits[0] < 20
			 && (s->u.note.notes[0].ti1 & 0x07) == SL_BELOW)
				;
			else if (s->pits[s->nhd] > 24
			      && (s->u.note.notes[s->nhd].ti1 & 0x07) == SL_ABOVE)
				;
			else
				continue;
			s2 = s->next;
			while (s2 && s2->abc_type != ABC_T_NOTE)
				s2 = s2->next;
			if (s2) {
				if (s2->staff != s->staff)
					continue;
				dx = s2->x - s->x - 10;
			} else {
				dx = realwidth - s->x - 10;
			}
			if (dx < 100)
				dy = 9;
			else if (dx < 300)
				dy = 12;
			else
				dy = 16;
			if (s->pits[s->nhd] > 24) {
				y = 3 * (s->pits[s->nhd] - 18) + dy;
				if (s->ymx < y)
					s->ymx = y;
				if (s2 && s2->ymx < y)
					s2->ymx = y;
				y_set(s->staff, 1, s->x + 5, dx, y);
			}
			if (s->pits[0] < 20) {
				y = 3 * (s->pits[0] - 18) - dy;
				if (s->ymn > y)
					s->ymn = y;
				if (s2 && s2->ymn > y)
					s2->ymn = y;
				y_set(s->staff, 0, s->x + 5, dx, y);
			}
		}
	}
}
