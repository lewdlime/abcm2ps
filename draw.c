/*
 * Drawing functions.
 *
 * This file is part of abcm2ps.
 *
 * Copyright (C) 1998-2002 Jean-François Moine
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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "abcparse.h"
#include "abc2ps.h"

struct BEAM {			/* packages info on one beam */
	struct SYMBOL *s1, *s2;
	float a, b;
	float x, y, t;
	short stem, staff;
};

int nbar;		/* current measure number */

/* -- check if space enough in the output buffer -- */
static void nbuf_check(void)
{
	if (nbuf + 100 > BUFFSZ) {
		ERROR(("PS output exceeds reserved space per staff"
		       " -- increase BUFFSZ"));
		exit(1);
	}
}

/* -- up/down shift needed to get k*6 -- */
static float rnd6(float x)
{
	int ix, iy, ir;

	ix = (int) (x + 600.999 - 3.0);
	iy = ix - 600;
	ir = ix % 6;
	if (ir != 0)
		iy += 6 - ir;
	return (float) iy - x;
}

/* -- b_pos -- */
static float b_pos(int stem,
		   int flags,
		   float b)
{
	float d1, d2;
	float top, bot;

	if (stem > 0) {
		bot = b - (flags - 1) * BEAM_SHIFT - BEAM_DEPTH;
		if (bot > 26)
			return b;
		top = b;
	} else {
		top = b + (flags - 1) * BEAM_SHIFT + BEAM_DEPTH;
		if (top < -2)
			return b;
		bot = b;
	}

	d1 = rnd6(top - BEAM_OFFSET);
	d2 = rnd6(bot + BEAM_OFFSET);
	if (d1 * d1 > d2 * d2)
		return b + d2;
	return b + d1;
}

/* -- calculate_beam -- */
/* (the staves may be defined or not) */
static int calculate_beam(struct BEAM *bm,
			  struct SYMBOL *s1)
{
	struct SYMBOL *s, *s2;
	int i, notes, flags, staff, voice;
	float x, y, ys, a, b, max_stem_err;
	float sx, sy, sxx, sxy, syy, a0;
	int two_staves;
  
	/* find first and last note in beam */
	notes = flags = 0;	/* set x positions, count notes and flags */
	two_staves = 0;
	staff = s1->staff;
	for (s = s1; ; s = s->next) {
		s->xs = s->x;
		if (s->type != NOTE)
			continue;
		if (s->stem > 0)
			s->xs +=  STEM_XOFF + s->shhd[0];
		else	s->xs += -STEM_XOFF + s->shhd[s->nhd];
		if (s->nflags > flags)
			flags = s->nflags;
		notes++;
		if (s->staff != staff)
			two_staves = 1;
		if (s->as.u.note.word_end)
			break;
	}

	if ((s2 = s) == 0) {
		ERROR(("No beam end!"));
		return 0;
	}

	bm->s2 = s2;		/* (don't display the flags) */
	if (bm->staff >= 0) {	/* staves not defined */
		if (two_staves) {
			for (s = s1; ; s = s->next) {
				if (s->type == NOTE)
					s->sflags |= S_2S_BEAM;
				if (s == s2)
					break;
			}
			return 0;
		}
		bm->staff = staff;
	} else {		/* staves defined */
		if (!two_staves)
			return 0;
	}

	sx = sy = sxx = sxy = syy = 0;	/* linear fit through stem ends */
	for (s = s1; ; s = s->next) {
		if (s->type != NOTE) {
#if 0
			if (s == s2)
				break;
#endif
			continue;
		}
		x = s->xs;
		y = s->ys + staff_tb[(unsigned) s->staff].y;
		sx += x; sy += y;
		sxx += x * x; sxy += x * y; syy += y * y;
		if (s == s2)
			break;
	}

	/* beam fct: y=ax+b */
	a = (sxy * notes - sx * sy) / (sxx * notes - sx * sx);
	b = (sy - a * sx) / notes;

	/* the next few lines modify the slope of the beam */
	if (notes >= 3) {
		float hh;

		hh = syy - a * sxy - b * sy;	/* flatten if notes not in line */
		if (hh > 0
		    && hh / (notes - 2) > 0.5)
			a *= BEAM_FLATFAC;
	}

	if (a >= 0)
		a = BEAM_SLOPE * a / (BEAM_SLOPE + a);	/* max steepness for beam */
	else	a = BEAM_SLOPE * a / (BEAM_SLOPE - a);

	/* to decide if to draw flat etc. use normalized slope a0 */
	a0 = a * (s2->xs - s1->xs) / (20 * (notes - 1));

	if (a0 < BEAM_THRESH && a0 > -BEAM_THRESH)
		a = 0;			/* flat below threshhold */

	b = (sy - a * sx) / notes;		/* recalculate b for new slope */

/*  if (flags>1) b=b+2*stem;*/		/* leave a bit more room if several beams */

	/* have flat beams when asked */
	if (cfmt.flatbeams
	    && voice_tb[(unsigned) s1->voice].bagpipe) {
		b = -11 + staff_tb[(unsigned) s1->staff].y;
		a = 0;
	}

/*fixme: have a look again*/
	/* have room for the symbols in the staff */
/*fixme: should also include grace notes here */
	max_stem_err = 0;		/* check stem lengths */
	voice = s1->voice;
	s = s1;
	while (s->ts_prev->type == NOTE
	       && s->ts_prev->time == s->time)
		s = s->ts_prev;
	for (; s != 0 && s->time <= s2->time; s = s->ts_next) {
		float min_stem, stem_err, slen;

		if (s->type != NOTE
		    || (s->staff != staff
			&& s->voice != voice)) {
			continue;
		}
		if (s->voice == voice) {
			ys = a * s->xs + b
				- staff_tb[(unsigned) s->staff].y;
			if (s->nhd == 0) {
				min_stem = STEM_MIN;
				switch (s->nflags) {
				case 2: min_stem = STEM_MIN2; break;
				case 3: min_stem = STEM_MIN3; break;
				case 4: min_stem = STEM_MIN4; break;
				}
			} else {
				min_stem = STEM_CH_MIN;
				switch (s->nflags) {
				case 2: min_stem = STEM_CH_MIN2; break;
				case 3: min_stem = STEM_CH_MIN3; break;
				case 4: min_stem = STEM_CH_MIN4; break;
				}
			}
			min_stem += BEAM_DEPTH + BEAM_SHIFT * (s->nflags - 1);
			if (s->stem > 0) {
				slen = ys - s->ymx;
				if (s->pits[s->nhd] > 26) {
					min_stem -= 2;
					if (s->pits[s->nhd] > 28)
						min_stem -= 2;
				}
			} else {
				slen = s->ymn - ys;
				if (s->pits[0] < 18) {
					min_stem -= 2;
					if (s->pits[0] < 16)
						min_stem -= 2;
				}
			}
			stem_err = min_stem - slen;
		} else {
/*fixme: does not work if two_staves */
			ys = a * s->x + b
				- staff_tb[(unsigned) s->staff].y;
			if (s1->stem > 0) {
				if (s->stem > 0) {
					if (s->voice < voice)
						continue;
					stem_err = s->ys + 8. - ys;
				} else	stem_err = s->y + 8. - ys;
			} else {
				if (s->stem > 0)
					stem_err = ys - s->y + 8.;
				else	stem_err = ys - s->ys + 8.;
			}
		}
		if (stem_err > max_stem_err)
			max_stem_err = stem_err;
	}

	if (max_stem_err > 0)			/* shift beam if stems too short */
		b += s1->stem * max_stem_err;

	for (s = s1->next; ; s = s->next) {	/* room for gracenotes */
		if (s->type != NOTE
		    || s->as.u.note.gr == 0) {
			if (s == s2)
				break;
			continue;
		}

		for (i = 0; i < s->as.u.note.gr->n; i++) {
			float yyg, yg, try;

			yyg = a * (s->x - GSPACE0) + b;
			yg = (float) (3 * (s->as.u.note.gr->p[i] - 18));
			if (s->stem > 0) {
				try = yg + GSTEM - yyg + BEAM_DEPTH + 2;
				if (try > 0)
					b += try;
			} else {
				try = yg - yyg - BEAM_DEPTH - 7;
				if (try < 0)
					b += try;
			}
		}
		if (s == s2)
			break;
	}

	if (a * a < 0.01)	/* shift flat beams onto staff lines */
/*fixme*/
		b = b_pos(s1->stem, flags, b);

	/* adjust final stems and rests under beam */
	for (s = s1; ; s = s->next) {
		switch (s->type) {
		case NOTE:
			s->ys = a * s->xs + b - staff_tb[(unsigned) s->staff].y;
			if (s->stem > 0) {
				if (s->dc_top < s->ys + 2.)
					s->dc_top = s->ys + 2.;
			} else {
				if (s->dc_bot > s->ys - 2.)
					s->dc_bot = s->ys - 2.;
			}
			break;
		case REST:
/*fixme: pb when two_voices*/
			y = a * s->xs + b - staff_tb[(unsigned) s->staff].y;
			if (s1->stem > 0) {
				y -= BEAM_DEPTH + (flags - 1) * BEAM_SHIFT;
				y -= s->head != H_FULL ? 4 : 9;
				if (y > 12)
					y = 12;
			} else {
				y += BEAM_DEPTH + (flags - 1) * BEAM_SHIFT;
				y += s->head != H_FULL ? 4 : 11;
				if (y < 12)
					y = 12;
			}
			if (s->head != H_FULL) {
				int iy;

				iy = (int) (y + 3.0) / 6;
				y = 6 * iy;
			}
			s->y = y;
			break;
		}
		if (s == s2)
			break;
	}

	/* save beam parameters */
	bm->s1 = s1;
	bm->a = a;
	bm->b = b;
	bm->stem = s1->stem;		/* general direction */
	bm->t = s1->stem * BEAM_DEPTH;
	return 1;
}

/* -- draw a single beam -- */
/* (the staves may be defined or not) */
static void draw_beam(float x1,
		      float x2,
		      float dy,
		      struct BEAM *bm)
{
	float y1, dy2;

	y1 = bm->a * x1 + bm->b - dy;
	dy2 = bm->a * (x2 - x1);
	PUT4("%.1f %.1f %.1f %.1f ",
	    bm->t, x2 - x1, dy2, x1);
	if (bm->staff < 0)
		PUT1("%.1f bm\n", y1);
	else	PUT2("\x01%c%5.2f bm\n", '0' + bm->staff, y1);
}

/* -- draw the beams for one word -- */
/* (the staves may be defined or not) */
static void draw_beams(struct BEAM *bm)
{
	struct SYMBOL *s, *s1, *s2;
	int i, maxfl;
	float shift;
	int two_staves;

	s1 = bm->s1;
	s2 = bm->s2;
	maxfl = 1;

	nbuf_check();

	/* make first beam over whole word */
	two_staves = bm->staff < 0;

	for (s = s1; ; s = s->next) {
#if 0
		/* numbers for nplets on same beam */
		if ((s->sflags & (S_NPLET_ST|S_NPLET_END)) == S_NPLET_ST) {
			struct SYMBOL *s3;

			s3 = s->next;
			for (;;) {
				if ((s3->sflags & (S_NPLET_ST|S_NPLET_END)) == S_NPLET_END)
					break;
				if (s3 == s2) {
					s3 = 0;
					break;
				}
				s3 = s3->next;
			}

			if (s3 != 0) {
				float xn, yn;

				xn = 0.5 * (s->xs + s3->xs);
				if (bm->stem < 0)
					yn = -12.;
				else	yn = 4.;
				yn += bm->a * xn + bm->b;
				if (bm->staff < 0)
					PUT3("(%d) %.1f %.1f bnum\n",
					     s->as.u.note.p_plet, xn, yn);
				else	PUT4("(%d) %.1f \x01%c%5.2f bnum\n",
					     s->as.u.note.p_plet, xn, '0' + bm->staff, yn);
				s->as.u.note.p_plet = 0;
				s->sflags &= ~(S_NPLET_ST|S_NPLET_END);
				do {
					s3 = s3->prev;
				} while (s3->x > xn);
				if (bm->stem > 0) {
					yn += 12. + 2.;
					if (s3->next->x > xn + 2.) {
						if (s3->dc_top < yn)
							s3->dc_top = yn;
					}
					s3 = s3->next;
					if (s3->dc_top < yn)
						s3->dc_top = yn;
				} else {
					yn -= 2.;
					if (s3->next->x > xn + 2.) {
						if (s3->dc_bot > yn)
							s3->dc_bot = yn;
					}
					s3 = s3->next;
					if (s3->dc_bot > yn)
						s3->dc_bot = yn;
				}
			}
		}
#endif
		if (s->nflags > maxfl)
			maxfl = s->nflags;
		if (s == s2)
			break;
	}

	draw_beam(s1->xs, s2->xs, 0.0, bm);

	/* other beams with two or more flags */
	shift = 0;
	for (i = 2; i <= maxfl; i++) {
		struct SYMBOL *k1, *k2;
		int inbeam;

		k1 = k2 = s1;
		inbeam = 0;
		shift += BEAM_SHIFT;
		for (s = s1; ; s = s->next) {
			if (s->type != NOTE) {
#if 0
				if (s == s2)	/* (not useful) */
					break;
#endif
				continue;
			}
			if (!inbeam && s->nflags >= i) {
				k1 = s;
				inbeam = 1;
			}
			if (inbeam && (s->nflags < i
				       || s == s2
				       || (s->sflags & S_BEAM_BREAK))) {
				float x1;

				inbeam = 0;
				if (s->nflags >= i
				    && (s == s2 || (s->sflags & S_BEAM_BREAK)))
					k2 = s;
				x1 = k1->xs;
				if (k1 == k2) {
					if (k1 == s1)
						x1 += BEAM_STUB;
					else if (k1 == s2)
						x1 -= BEAM_STUB;
					else if (k1->prev->nflags < k1->next->nflags)
						x1 += BEAM_STUB;
					else	x1 -= BEAM_STUB;
				}
				draw_beam(x1, k2->xs,
					  shift * k1->stem,	/*fixme: more complicated */
					  bm);

				/* if on 2 staves, update the stem lengths */
				if (two_staves) {
					two_staves = k1->stem;
					for (;;) {
						if (k1->type == NOTE) {
						    if (k1->stem != s1->stem) {
							k1->ys = bm->a * k1->xs + bm->b
							    - staff_tb[(unsigned) k1->staff].y
							    - bm->t;
							if (k1->stem != two_staves)
							    k1->ys -= two_staves * shift;
						    }
						}
						if (k1 == k2)
							break;
						k1 = k1->next;
					}
				}
			}
			k2 = s;
			if (s == s2)
				break;
		}
	}
}

/* -- draw the name/subname of the voices -- */
static void draw_vname(int first_line,
		       float indent)
{
	struct VOICE_S *p_voice;
	int n, staff;
	struct {
		struct VOICE_S *v[4];
		int nv;
		int nl;
	} staff_d[MAXSTAFF], *staff_p;
	float y;

	memset(staff_d, 0, sizeof staff_d);
	n = 0;
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		staff = p_voice->staff;
		if (staff_tb[staff].brace_end)
			staff--;
		staff_p = &staff_d[staff];
		if (first_line) {
			if (p_voice->nm == 0)
				continue;
			if (strlen(p_voice->nm) > 32)
				staff_p->nl++;
		} else {
			if (p_voice->snm == 0)
				continue;
			if (strlen(p_voice->snm) > 8)
				staff_p->nl++;
		}
		staff_p->v[staff_p->nv] = p_voice;
		staff_p->nv++;
		staff_p->nl++;
		n++;
	}
	if (n == 0)
		return;
	set_font(&cfmt.vocalfont);
	for (staff = nstaff; staff >= 0; staff--) {
		staff_p = &staff_d[staff];
		if (staff_p->nl == 0)
			continue;
		y = staff_tb[staff].y + 12. - 9. * (staff_p->nl - 1)
			- cfmt.vocalfont.size * 0.3;
		if (staff_tb[staff].brace)
			y -= (staff_tb[staff].y - staff_tb[staff + 1].y) * 0.5;
		for (n = staff_p->nv; --n >= 0;) {
			p_voice = staff_p->v[n];
			/*fixme: truncate*/
			PUT3("%.1f %.1f M (%s) show\n",
			     -indent, y,
			     first_line ? p_voice->nm : p_voice->snm);
			y += 18.;
		}
	}
}

/* -- draw the staves and the left side -- */
void draw_staff(int first_line,
		float indent)
{
	int i;

	if (indent != 0)
		draw_vname(first_line, indent);	/* draw the voices name/subnames */

	/* draw the staves */
	for (i = nstaff; i >= 0; i--) {
		PUT2("0 %.2f M %.2f staff\n",
		     staff_tb[i].y, realwidth);
	}

	/* measure numbering */
	if (nbar > 1) {
		if (cfmt.measurenb == 0) {
			set_font(&cfmt.composerfont);
			PUT3("0 %.1f M (%d) show%s\n",
			     staff_tb[0].y + 24. + 16.,
			     nbar,
			     cfmt.measurebox ? "b" : "");
		} else if (cfmt.measurenb > 0
			   && nbar % cfmt.measurenb == 0) {
			struct SYMBOL *s;
			float x;

			for (s = first_voice->sym; s != 0; s = s->next) {
				if (s->type != CLEF
				    && s->type != KEYSIG
				    && s->type != TIMESIG)
					break;
			}
			x = s->x - s->pl - 10.;
			set_font(&cfmt.composerfont);
			PUT4("%.1f %.1f M (%d) show%s\n",
			     x, staff_tb[0].y + 24. + 6.,
			     nbar,
			     cfmt.measurebox ? "b" : "");
		}
	}

	if (nstaff == 0)
		return;

	PUT2("%.1f 0 %.1f bar\n",
	     staff_tb[0].y - staff_tb[nstaff].y + 24.,
	     staff_tb[nstaff].y);
	for (i = 0; i <= nstaff; i++) {
		float y;

		if (staff_tb[i].brace) {
			y = staff_tb[i].y + 24.;
			PUT2("%.1f 0 %.1f brace\n",
			     y - staff_tb[++i].y, y);
		} else if (staff_tb[i].bracket) {
			y = staff_tb[i++].y + 24.;
			while (!staff_tb[i].bracket_end)
				i++;
			PUT2("%.1f 0 %.1f bracket\n",
			     y - staff_tb[i].y, y);
		}
	}
}

/* -- draw_timesig -- */
static void draw_timesig(float x,
			 struct SYMBOL *s)
{
	int	j;

	if (s->as.u.meter.flag == 1)
		for (j = nstaff; j >= 0; j--)
			PUT2("%.1f %.1f csig\n", x, staff_tb[j].y);
	else if (s->as.u.meter.flag == 2)
		for (j = nstaff; j >= 0; j--)
			PUT2("%.1f %.1f ctsig\n", x, staff_tb[j].y);
	else if (s->as.u.meter.m2 != 0) {
		if (s->as.u.meter.top != 0)
			for (j = nstaff; j >= 0; j--)
				PUT4("(%s) (%d) %.1f %.1f tsig\n",
				     s->as.u.meter.top, s->as.u.meter.m2, x, staff_tb[j].y);
		else	for (j = nstaff; j >= 0; j--)
				PUT4("(%d) (%d) %.1f %.1f tsig\n",
				     s->as.u.meter.m1, s->as.u.meter.m2, x, staff_tb[j].y);
	} else {
		if (s->as.u.meter.top != 0)
			for (j = nstaff; j >= 0; j--)
				PUT3("(%s) %.1f %.1f stsig\n",
				     s->as.u.meter.top, x, staff_tb[j].y);
		else	for (j = nstaff; j >= 0; j--)
				PUT3("(%d) %.1f %.1f stsig\n",
				     s->as.u.meter.m1, x, staff_tb[j].y);
	}
}

/* -- draw_keysig -- */
static void draw_keysig(struct VOICE_S *p_voice,
			float x,
			struct SYMBOL *s)
{
	int i;
	static char sharp_tb[7] = {24, 15, 27, 18, 9, 21, 12};
	static char flat_tb[7] = {12, 21, 9, 18, 6, 15, 3};
	static signed char sharp_cl[7] = {0, -15, -9, -3, -18, -12, -6};
	static signed char flat_cl[7] = {0, 6, -9, -3, 3, -12, -6};

	if (!p_voice->second) {
		int old_sf = s->u;
		int staff = p_voice->staff;
		float staffb = staff_tb[staff].y;
		int clef_ix;
		int shift, clef_shift;

		clef_ix = 0 - 2;	/* treble */
		switch (staff_tb[staff].clef.type) {
		case ALTO:
			clef_ix = 3 - 3;
			break;
		case BASS:
			clef_ix = 6 - 4;
			break;
		}
		clef_ix += staff_tb[staff].clef.line;
		if (clef_ix < 0)
			clef_ix += 7;
		else if (clef_ix >= 7)
			clef_ix -= 7;
		/* if flats to sharps, or sharps to flats, put neutrals */
		if (s->as.u.key.sf == 0
		    || old_sf * s->as.u.key.sf < 0) {

			/* old sharps */
			clef_shift = sharp_cl[clef_ix];
			for (i = 0; i < old_sf; i++) {
				if ((shift = sharp_tb[i] + clef_shift) < -3)
					shift += 21;
				PUT2("%.1f %.1f nt0 ", x, staffb + shift);
				x += 5;
			}

			/* old flats */
			clef_shift = flat_cl[clef_ix];
			for (i = 0; i > old_sf; i--) {
				if ((shift = flat_tb[-i] + clef_shift) < -3)
					shift += 21;
				PUT2("%.1f %.1f nt0 ", x, staffb + shift);
				x += 5;
			}
			if (s->as.u.key.sf != 0)
				x += 3;		/* extra space */
		}

		/* new sharps */
		clef_shift = sharp_cl[clef_ix];
		for (i = 0; i < s->as.u.key.sf; i++) {
			if ((shift = sharp_tb[i] + clef_shift) < -3)
				shift += 21;
			PUT2("%.1f %.1f sh0 ", x, staffb + shift);
			x += 5;
		}

		/* new flats */
		clef_shift = flat_cl[clef_ix];
		for (i = 0; i > s->as.u.key.sf; i--) {
			if ((shift = flat_tb[-i] + clef_shift) < -3)
				shift += 21;
			PUT2("%.1f %.1f ft0 ", x, staffb + shift);
			x += 5;
		}

		/* if less sharps or flats, add neutrals */
		/* sharps */
		if (s->as.u.key.sf > 0) {
			clef_shift = sharp_cl[clef_ix];
			for (i = s->as.u.key.sf; i < old_sf; i++) {
				if ((shift = sharp_tb[i] + clef_shift) < -3)
					shift += 21;
				PUT2("%.1f %.1f nt0 ", x, staffb + shift);
				x += 5;
			}
		/* flats */
		} else if (s->as.u.key.sf < 0) {
			clef_shift = flat_cl[clef_ix];
			for (i = s->as.u.key.sf; i > old_sf; i--) {
				if ((shift = flat_tb[-i] + clef_shift) < -3)
					shift += 21;
				PUT2("%.1f %.1f nt0 ", x, staffb + shift);
				x += 5;
			}
		}
		if (old_sf != 0 && s->as.u.key.sf != 0)
			PUT0("\n");
	}

	/* memorize the current keysig of the voice */
	p_voice->sf = s->as.u.key.sf;
	p_voice->bagpipe = s->as.u.key.bagpipe;
}

/* -- draw_bar1 -- */
static void draw_bar1(float x,
		      float y,
		      float h,
		      struct SYMBOL *s)
{
	switch (s->as.u.bar.type) {
	case B_SINGLE:
		PUT3("%.1f %.1f %.1f bar\n", h, x, y); break;
	case B_DOUBLE:
		PUT3("%.1f %.1f %.1f dbar\n", h, x, y); break;
	case B_LREP:
	case B_THICK_THIN:
		PUT3("%.1f %.1f %.1f fbar1\n", h, x, y); break;
	case B_RREP:
	case B_THIN_THICK:
		PUT3("%.1f %.1f %.1f fbar2\n", h, x, y); break;
	case B_DREP:
		PUT3("%.1f %.1f %.1f fbar1 ", h, x - 1, y);
		PUT3("%.1f %.1f %.1f fbar2\n", h, x + 1, y); break;
	case B_INVIS: break;
	case B_DASH:
		PUT3("[5] 0 setdash %.1f %.1f %.1f bar [] 0 setdash\n",
		     h, x, y);
		break;
	default:
		ERROR(("line %d - cannot draw bar type %d",
		       s->as.linenum, s->as.u.bar.type));
		break;
	}
}

/* -- draw_bar -- */
static void draw_bar(float x,
		     struct SYMBOL *s)
{
	int	staff;
	float	stafft, y;
	int	dotsb = 0;
	int	dotsa = 0;

	stafft = staff_tb[0].y + 24.;	/* top of upper staff */
	for (staff = 0; staff < nstaff; staff++) {
		if (staff_tb[staff].stop_bar) {
			y = staff_tb[staff].y;
			draw_bar1(x, y, stafft - y, s);
			stafft = staff_tb[staff + 1].y + 24.;
		}
	}
	y = staff_tb[nstaff].y;
	draw_bar1(x, y, stafft - y, s);

	switch (s->as.u.bar.type) {
	case B_LREP:
		dotsb = 10;
		break;
	case B_RREP:
		dotsa = 10;
		break;
	case B_DREP:
		dotsb = 9;
		dotsa = 9;
		break;
	default:
		break;
	}
	if (dotsb != 0) {
		for (staff = nstaff; staff >= 0; staff--)
			PUT2(" %.1f %.1f rdots",
			     x + dotsb, staff_tb[staff].y);
		PUT0("\n");
	}
	if (dotsa != 0) {
		for (staff = nstaff; staff >= 0; staff--)
			PUT2(" %.1f %.1f rdots",
			     x - dotsa, staff_tb[staff].y);
		PUT0("\n");
	}
}

/* -- draw_rest -- */
/* (the staves are defined) */
static void draw_rest(struct SYMBOL *s)
{
	int i;
	float x, y, dotx, doty, staffb;
	char *p;

	if (s->as.u.note.invis)
		return;

	x = s->x;
	y = s->y;

	/* if rest alone in the measure, do it semibreve and center */
	if ((s->sflags & S_WMEASURE) && s->head != H_SQUARE) {
		struct SYMBOL *s2;

		if ((s2 = s->next) != 0) {
			s->head = H_OVAL;
			if (s->len > SEMIBREVE)
				s->len = BREVE;
			s->dots = 0;
			x = (s2->x + x) * 0.5 - 10.;
		}
	}

	staffb = staff_tb[(unsigned) s->staff].y;	/* bottom of staff */
	PUT2("%.1f %.0f ", x, y + staffb);

	switch (s->head) {
	case H_SQUARE:
	case H_OVAL:
		if (s->len < BREVE)
			p = "r1";
		else if (s->len < BREVE * 2)
			p = "r0";
		else	p = "r00";
		PUT0(p);
		if (y < -6		/* add one helper line */
		    /*fixme:add upper helper line when breve*/
		    || y >= 24) {
			PUT1(" %.1f hl", y + 6. + staffb);
		}
		dotx = 8;
		doty = -3;
		break;
	case H_EMPTY:
		PUT0("r2");
		if (y <= -6		/* add one helper line */
		    || y >= 30) {
			PUT1(" %.1f hl", y + staffb);
		}
		dotx = 8;
		doty = 3;
		break;
	default:
		switch (s->nflags) {
		case 0: p = "r4"; break;
		case 1: p = "r8"; break;
		case 2: p = "r16"; break;
		case 3: p = "r32"; break;
		default: p = "r64"; break;
		}
		PUT0(p);
		dotx = 6.5;
		if ((int) y % 6)
			doty = 0;		/* dots */
		else	doty = 3;
		break;
	}

	for (i = 0; i < s->dots; i++) {
		PUT2(" %.1f %.1f dt", dotx, doty);
		dotx += 3.5;
	}
	PUT0("\n");
}

/* -- draw_gracenotes -- */
/* (the staves are defined) */
static void draw_gracenotes(float x,
			    float w,
			    float d,
			    struct SYMBOL *s)
{
	int i, n, ii, m;
	float xg[MAXGR], lg, px, py, xx;
	int yg[MAXGR], yy;
	float x0, y0, x1, y1, x2, y2, x3, y3, bet1, bet2, dy1, dy2, dx, fac, facx;
	float a, b, staffb;
static char *acc2_tb[] = { "", "gsh0", "gnt0", "gft0", "gds0", "gdf0" };

	n = s->as.u.note.gr->n;
	a = b = 0.0;		/* compiler warning */

	staffb = staff_tb[(unsigned) s->staff].y;	/* bottom of staff */
	facx = 0.3;
	fac = d / w - 1;
	if (fac < 0)
		fac = 0;
	fac = 1. + (fac * facx) / (fac + facx);

	dx = 0;
	for (m = 0; m <= s->nhd; m++) {	/* room for accidentals */
		float dd;

		dd = -s->shhd[m];
		if (s->as.u.note.accs[m])
			dd = -s->shhd[m] + s->shac[m];
		if (s->as.u.note.accs[m] == A_FT || s->as.u.note.accs[m] == A_NT)
			dd -= 2;
		if (dx < dd)
			dx = dd;
	}

	xx = x - fac * (dx + GSPACE0);
	for (i = n; --i >= 0; ) {		/* set note positions */
		yg[i] = 3 * (s->as.u.note.gr->p[i] - 18);
		if (i == n - 1) {		/* some subtle shifts.. */
			if (yg[i] >= s->ymx)
				xx += 1;	/* gnote above a bit closer */
			if (n == 1 && yg[i] < s->ymn - 7)
				xx -= 2;	/* below with flag further */
		}

		if (i < n - 1
		    && yg[i] > yg[i + 1] + 8)
			xx += fac * 1.8;

		xg[i] = xx;
		xx -= fac * GSPACE;
		if (s->as.u.note.gr->a[i])
			xx -= 3.5;
	}

	if (n > 1) {
		float s1, delta;
		float sx, sy, sxx, sxy, lmin, lmm;

		s1 = sx = sy = sxx = sxy = 0;	/* linear fit through stems */
		for (i = 0; i < n; i++) {
			px = xg[i] + GSTEM_XOFF;
			py = yg[i] + GSTEM;
			s1 += 1;
			sx += px;
			sy += py;
			sxx += px * px;
			sxy += px * py;
		}
		delta = s1 * sxx - sx * sx;	/* beam fct: y=ax+b */
		a = (s1 * sxy - sx * sy) / delta;
		if (a > BEAM_SLOPE)
			a = BEAM_SLOPE;
		else if (a < -BEAM_SLOPE)
			a = -BEAM_SLOPE;
		b = (sy - a * sx) / s1;

		if (voice_tb[(unsigned) s->voice].bagpipe) {
			if (cfmt.flatbeams) {
				a = 0;
				b = 35;
			}
			lmm = 13;
		} else	lmm = 10;

		lmin = lmm;			/* shift to get min stems */
		for (i = 0; i < n; i++) {
			px = xg[i] + GSTEM_XOFF;
			py = a * px + b;
			lg = py - yg[i];
			if (lg < lmin)
				lmin = lg;
		}
		if (lmin < lmm)
			b += lmm - lmin;
	}

	/* draw grace notes */
	for (i = 0; i < n; i++) {
		int acc, y;

		if (n > 1) {
			px = xg[i] + GSTEM_XOFF;
			py = a * px + b;
			lg = py - yg[i];
			PUT3("%.1f %.1f %.1f gnt ",
			     lg, xg[i], yg[i] + staffb);
		} else {
			char c;

			lg = GSTEM;
			if (voice_tb[(unsigned) s->voice].bagpipe) {
				lg += 3;
				if (cfmt.straightflags)
					c = 's';
				else	c = 'h';
			} else if (s->as.u.note.gr->sappo)
				c = 's';
			else	c = ' ';
			PUT4("%.1f %.1f %.1f gn1%c ",
			     lg, xg[i], yg[i] + staffb, c);
		}

		if ((acc = s->as.u.note.gr->a[i]) != 0)
			PUT3("%.1f %.1f %s ",
			     xg[i] - 4.5, yg[i] + staffb, acc2_tb[acc]);

		y = yg[i];		/* helper lines */
		if (y <= -6) {
			if (y % 6)
				PUT2("%.1f %.1f ghl ",
				     xg[i], y + 3 + staffb);
			else	PUT2("%.1f %.1f ghl ",
				     xg[i], y + staffb);
		}
		if (y >= 30) {
			if (y % 6)
				PUT2("%.1f %.1f ghl ",
				     xg[i], y - 3 + staffb);
			else	PUT2("%.1f %.1f ghl ",
				     xg[i], y + staffb);
		}
	}

	/* beam */
	if (n > 1) {
		float px0, pxn;

		px0 = xg[0] + GSTEM_XOFF;
		pxn = xg[n - 1] + GSTEM_XOFF;
		PUT5("%.1f %.1f %.1f %.1f gbm%d ",
		     px0, a * px0 + b + staffb,
		     pxn, a * pxn + b + staffb,
		     voice_tb[(unsigned) s->voice].bagpipe ? 3 : 2);
	}

	/* slur */
	if (voice_tb[(unsigned) s->voice].bagpipe	/* no slur when bagpipe */
	    || !cfmt.graceslurs)
		return;
	bet1 = 0.2;
	bet2 = 0.8;
	yy = 1000;
	ii = 0;
	for (i = n; --i >= 0; )
		if (yg[i] <= yy) {
			yy = yg[i];
			ii = i;
		}
	x0 = xg[ii];
	y0 = yg[ii] - 5;
	if (ii > 0) {
		x0 -= 4;
		y0 += 1;
	}
	x3 = x - 1;
	if (s->stem < 0)
		x3 -= 3;
	y3 = s->ymn - 5;
	dy1 = (x3 - x0) * 0.4;
	if (dy1 > 3)
		dy1 = 3;
	dy2 = dy1;

	if (yg[ii] > s->ymn + 7) {
		x0 = xg[ii] - 1;
		y0 = yg[ii] - 4.5;
		y3 = s->ymn + 1.5;
		x3 = x - dx - 5.5;
		dy2 = (y0 - y3) * 0.2;
		dy1 = (y0 - y3) * 0.8;
		bet1 = 0.0;
	}

	if (y3 > y0 + 4) {
		y3 = y0 + 4;
		x0 = xg[ii] + 2;
		y0 = yg[ii] - 4;
	}

	x1 = bet1 * x3 + (1 - bet1) * x0;
	y1 = bet1 * y3 + (1 - bet1) * y0 - dy1;
	x2 = bet2 * x3 + (1 - bet2) * x0;
	y2 = bet2 * y3 + (1 - bet2) * y0 - dy2;

	PUT4(" %.1f %.1f %.1f %.1f",
	     x1, y1 + staffb, x2, y2 + staffb);
	PUT4(" %.1f %.1f %.1f %.1f gsl\n",
	     x3, y3 + staffb, x0, y0 + staffb);
}

/* -- draw m-th head with accidentals and dots -- */
/* (the staves are defined) */
static void draw_basic_note(float x,
			    float w,
			    float d,
			    struct SYMBOL *s,
			    int m)
{
	int y;
	float staffb;
	char *p;
static char *acc_tb[] = { "", "sh", "nt", "ft", "dsh", "dft" };

	staffb = staff_tb[(unsigned) s->staff].y;	/* bottom of staff */
	y = 3 * (s->pits[m] - 18);		/* height on staff */

	/* special case when no head */
	if (s->sflags & S_NO_HEAD) {
		struct SYMBOL *s2;

		s2 = s->ts_next;
/*fixme: have an other symbol flag */
		if ((m == 0
		     && s->pits[0] == s->ts_next->pits[s->ts_next->nhd])
		    || (m == s->nhd
			&& s->pits[m] == s->ts_prev->pits[0])) {
			PUT2("/x %.1f def /y %.1f def",	/* set x y */
			     x + s->shhd[m],
			     (float) y + staffb);
			return;
		}
	}

	/* draw the head */
	PUT2("%.1f %.1f ",
	     x + s->shhd[m],
	     (float) y + staffb);
	switch (s->head) {
	case H_SQUARE:
		if (s->as.u.note.lens[0] < BREVE * 2)
			p = "breve";
		else	p = "longa";
		break;
	case H_OVAL:
		if (s->as.u.note.lens[0] < BREVE)
			p = "HD";
		else	p = "HDD";
		break;
	case H_EMPTY:
		p = "Hd"; break;
	default:
		p = "hd"; break;
	}
	PUT0(p);

	/* add a helper line if horizontal shift */
	if (s->shhd[m]) {
		int yy;

		yy = 0;
		if (y >= 30) {
			yy = y;
			if (yy % 6)
				yy -= 3;
		}
		if (y <= -6) {
			yy = y;
			if (yy % 6)
				yy += 3;
		}
		if (yy)
			PUT1(" %.1f hl", (float) yy + staffb);
	}

	/* draw the dots */
	if (s->dots) {
		int i;
		float dotx;
		int doty;

		dotx = 8. + s->xmx - s->shhd[m];
		if (y % 6)
			doty = 0;
		else	{
			if ((doty = s->doty) == 0)	/* defined when voices overlap */
				doty = 3;
			if (s->nflags && s->stem > 0
			    && (s->sflags & S_WORD_ST)
			    && s->as.u.note.word_end
			    && s->nhd == 0)
				dotx += DOTSHIFT;
		}
		switch (s->head) {
		case H_SQUARE:
		case H_OVAL:
			dotx += 2;
			break;
		case H_EMPTY:
			dotx += 1;
			break;
		}
		for (i = 0; i < s->dots; i++) {
			PUT2(" %.1f %d dt", dotx, doty);
			dotx += 3.5;
		}
	}

	/* draw the accidentals */
	if (s->as.u.note.accs[m]) {
		float add, fac;

		add = 0.3 * (d - w - 3.);
		fac = 1. + add / s->wl;
		if (fac < 1.)
			fac = 1.;
		else if (fac > 1.2)
			fac = 1.2;
		PUT2(" %.1f %s",
		     -fac * s->shac[m],
		     acc_tb[(unsigned) s->as.u.note.accs[m]]);
	}
}

/* -- draw_note -- */
/* (the staves are defined) */
static void draw_note(float x,
		      float w,
		      float d,
		      struct SYMBOL *s,
		      int fl)
{
	int	y, i, m, ma;
	float	staffb;

	if (s->as.u.note.gr != 0)
		draw_gracenotes(x, w, d, s);		/* draw grace notes */

	staffb = staff_tb[(unsigned) s->staff].y;

	/* draw the master note - can be only the first or the last note */
	if (s->stem >= 0)
		ma = 0;
	else	ma = s->nhd;
	draw_basic_note(x, w, d, s, ma);	/* draw note head */
	if (!s->as.u.note.stemless) {		/* add stem */
		char	c;
		float	slen;

		c = s->stem > 0 ? 'u' : 'd';
		slen = s->stem * (s->ys - s->y);
		PUT2(" %.1f s%c", slen, c);
		if (fl && s->nflags > 0) {	/* and add flags */
			if (voice_tb[(int) s->voice].bagpipe
			    && cfmt.straightflags)
				c = 's';		/* straight flag */
			PUT3(" %.1f f%d%c", slen, s->nflags, c);
		}
	}

	y = s->ymn;				/* lower helper lines */
	if (y <= -6) {
		for (i = -6; i >= y; i -= 6)
			PUT1(" %.1f hl", (float) i + staffb);
		switch (s->head) {
		case H_SQUARE:
		case H_OVAL:
			PUT0("1");
			break;
		}
	}
	y = s->ymx;				/* upper helper lines */
	if (y >= 30) {
		for (i = 30; i <= y; i += 6)
			PUT1(" %.1f hl", (float) i + staffb);
		switch (s->head) {
		case H_SQUARE:
		case H_OVAL:
			PUT0("1");
			break;
		}
	}

	/* draw the other notes */
	for (m = 0; m <= s->nhd; m++) {
		if (m == ma)
			continue;
		PUT0(" ");
		draw_basic_note(x, w, d, s, m);		/* draw note heads */
	}

	PUT0("\n");
}

/* -- draw_bracket -- */
/* (the staves are not yet defined) */
static void draw_bracket(struct SYMBOL *s1,
			 struct SYMBOL *s2)
{
	struct SYMBOL *sy;
	int upstaff, nb_only;
/*	int two_staves; */
	float x1, x2, y1, y2, xm, ym, s, s0, yy, yx, dy;

	/* search what is the upper staff and if a bracket is needed */
	nb_only = s1->type == NOTE && s2->type == NOTE;
	if (!((s1->sflags & S_WORD_ST) || (s1->prev->sflags & S_BEAM_BREAK)
	       || s1->prev->nflags < s1->nflags)
/*	       || s1->prev->as.u.note.word_end) */
	    || !(s2->as.u.note.word_end || (s2->sflags & S_BEAM_BREAK)))
		nb_only = 0;
	upstaff = s1->staff;
/*	two_staves = 0; */
	for (sy = s1; ; sy = sy->next) {
		if (sy->len == 0) {	/* not a note or a rest */
			nb_only = 0;
			if (sy == s2)
				break;
			continue;
		}
		if (sy->staff != upstaff) {
/*			two_staves = 1; */
			if (sy->staff < upstaff)
				upstaff = sy->staff;
/*			break; */
		}
		if (sy == s2)
			break;
		if (sy->as.u.note.word_end || (sy->sflags & S_BEAM_BREAK))
			nb_only = 0;
	}

	/* if nplet number only, draw it */
	if (nb_only) {
		float a, b;

		a = (s2->ys - s1->ys) / (s2->xs - s1->xs);
		b = s1->ys - a * s1->xs;
		if (s1->stem > 0) {
			if (s1->multi) {
				for (sy = s1; ; sy = sy->next) {
					yy = a * sy->xs + b;
					if (sy->dc_top > yy)
						b += sy->dc_top - yy;
					if (sy == s2)
						break;
				}
			}
			b += 4.;
		} else {
			if (s1->multi) {
				for (sy = s1; ; sy = sy->next) {
					yy = a * sy->xs + b;
					if (sy->dc_bot < yy)
						b += sy->dc_bot - yy;
					if (sy == s2)
						break;
				}
			}
			b -= 12.;
		}
		xm = (s2->xs + s1->xs) * 0.5;
		ym = a * xm + b;
		PUT4("(%d) %.1f \x01%c%5.2f bnum\n",
		     s1->as.u.note.p_plet, xm, '0' + s1->staff, ym);
		if (s1->stem > 0) {
			b += 8.;
			for (sy = s1; ; sy = sy->next) {
				yy = a * sy->xs + b;
				if (sy->dc_top < yy)
					sy->dc_top = yy;
				if (sy == s2)
					break;
			}
		} else {
			for (sy = s1; ; sy = sy->next) {
				yy = a * sy->xs + b;
				if (sy->dc_bot > yy)
					sy->dc_bot = yy;
				if (sy == s2)
					break;
			}
		}
		return;
	}

/*fixme: two staves not treated*/
/*fixme: to optimize*/
    if (s1->multi >= 0) {

	/* sole or upper voice: the bracket is above the staff */
	x1 = s1->x - 4.;
	x2 = s2->x + 4.;
	if (s1->staff == upstaff) {
		sy = s1;
		if (sy->type != NOTE) {
			for (sy = sy->next; sy != s2; sy = sy->next)
				if (sy->type == NOTE)
					break;
		}
		y1 = sy->dc_top;
		if (y1 < 24.)
			y1 = 24.;
		if (s1->stem > 0)
			x1 += 3.;
	} else	y1 = 24.;
	if (s2->staff == upstaff) {
		sy = s2;
		if (sy->type != NOTE) {
			for (sy = sy->prev; sy != s1; sy = sy->prev)
				if (sy->type == NOTE)
					break;
		}
		y2 = sy->dc_top;
		if (y2 < 24.)
			y2 = 24.;
		if (s2->stem > 0)
			x2 += 3.;
	} else	y2 = 24.;

	xm = 0.5 * (x1 + x2);
	ym = 0.5 * (y1 + y2);

	s = (y2 - y1) / (x2 - x1);
	s0 = (s2->ymx - s1->ymx) / (x2 - x1);
	if (s0 > 0) {
		if (s < 0)
			s = 0;
		else if (s > s0)
			s = s0;
	} else {
		if (s > 0)
			s = 0;
		else if (s < s0)
			s = s0;
	}
	if (s * s < 0.1 * 0.1)
		s = 0;

	/* shift up bracket if needed */
	dy = 0;
	for (sy = s1; ; sy = sy->next) {
		if (sy->len == 0	/* not a note nor a rest */
		    || sy->staff != upstaff) {
			if (sy == s2)
				break;
			continue;
		}
		yy = ym + (sy->x - xm) * s;
		yx = sy->dc_top;
		if (yx - yy > dy)
			dy = yx - yy;
		if (sy == s2)
			break;
	}

	ym += dy + 4.;
	y1 = ym + s * (x1 - xm);
	y2 = ym + s * (x2 - xm);
	PUT3("%.1f \x01%c%5.2f ", x1, '0' + upstaff, y1 + 4.);
	PUT3("%.1f \x01%c%5.2f hbr ",
	     xm - 6., '0' + upstaff, ym + s * -6. + 4.);
	PUT3("%.1f \x01%c%5.2f ", x2, '0' + upstaff, y2 + 4.);
	PUT3("%.1f \x01%c%5.2f hbr ",
	     xm + 6., '0' + upstaff, ym + s * 6. + 4.);

	/* shift the slurs / decorations */
	ym += 8.;
	for (sy = s1; ; sy = sy->next) {
		if (sy->staff == upstaff) {
			yy = ym + (sy->x - xm) * s;
			if (sy->dc_top < yy)
				sy->dc_top = yy;
		}
		if (sy == s2)
			break;
	}

    } else {	/* lower voice of the staff: the bracket of below the staff */
/*fixme: think to all that again..*/
	x1 = s1->x - 8.;
/*fixme: the note may be shifted to the right*/
	x2 = s2->x;
	if (s1->staff == upstaff) {
		sy = s1;
		if (sy->type != NOTE) {
			for (sy = sy->next; sy != s2; sy = sy->next)
				if (sy->type == NOTE)
					break;
		}
		y1 = sy->dc_bot;
	} else	y1 = 0;
	if (s2->staff == upstaff) {
		sy = s2;
		if (sy->type != NOTE) {
			for (sy = sy->prev; sy != s1; sy = sy->prev)
				if (sy->type == NOTE)
					break;
		}
		y2 = sy->dc_bot;
	} else	y2 = 0;

	xm = 0.5 * (x1 + x2);
	ym = 0.5 * (y1 + y2);

	s = (y2 - y1) / (x2 - x1);
	s0 = (s2->ymn - s1->ymn) / (x2 - x1);
	if (s0 > 0) {
		if (s < 0)
			s = 0;
		else if (s > s0)
			s = s0;
	} else {
		if (s > 0)
			s = 0;
		else if (s < s0)
			s = s0;
	}
	if (s * s < 0.1 * 0.1)
		s = 0;

	/* shift down bracket if needed */
	dy = 0;
	for (sy = s1; ; sy = sy->next) {
		if (sy->len == 0	/* not a note nor a rest */
		    || sy->staff != upstaff) {
			if (sy == s2)
				break;
			continue;
		}
		yy = ym + (sy->x - xm) * s;
		yx = sy->dc_bot;
		if (yx - yy < dy)
			dy = yx - yy;
		if (sy == s2)
			break;
	}

	ym += dy - 12.;
	y1 = ym + s * (x1 - xm);
	y2 = ym + s * (x2 - xm);
	PUT3("%.1f \x01%c%5.2f ", x1, '0' + upstaff, y1 + 4.);
	PUT3("dlw %.1f \x01%c%5.2f M lineto 0 3 rlineto stroke ",
	     xm - 6., '0' + upstaff, ym + s * -6. + 4.);
	PUT3("%.1f \x01%c%5.2f ", x2, '0' + upstaff, y2 + 4.);
	PUT3("%.1f \x01%c%5.2f M lineto 0 3 rlineto stroke ",
	     xm + 6., '0' + upstaff, ym + s * 6. + 4.);

	/* shift the slurs / decorations */
	for (sy = s1; ; sy = sy->next) {
		if (sy->staff == upstaff) {
			yy = ym + (sy->x - xm) * s;
			if (sy->dc_bot > yy)
				sy->dc_bot = yy;
		}
		if (sy == s2)
			break;
	}
    } /* lower voice */

	yy = 0.5 * (y1 + y2);
	PUT4("(%d) %.1f \x01%c%5.2f bnum\n",
	     s1->as.u.note.p_plet, xm, '0' + upstaff, yy);
}

/* -- draw_nplet_brackets  -- */
/* (the staves are not yet defined) */
static void draw_nplet_brackets(struct SYMBOL *sym)
{
	struct SYMBOL *s, *s1;

	for (s = sym; s != 0; s = s->next) {
		if ((s->sflags & (S_NPLET_ST|S_NPLET_END)) != S_NPLET_ST)
			continue;
		s1 = s;
		for (; s->next != 0; s = s->next) {
			if ((s->sflags & (S_NPLET_ST|S_NPLET_END)) == S_NPLET_END)
				break;
		}
		draw_bracket(s1, s);
	}
}

/* -- output slur -- */
static void output_slur(float x1,
			float y1,
			float x2,
			float y2,
			int s,
			float height,
			float shift,
			int staff)	/* if < 0, the staves are defined */
{
	float alfa, beta, mx, my, xx1, yy1, xx2, yy2, dx, dy, dz;

	alfa = 0.3;
	beta = 0.45;

	/* for wide flat slurs, make shape more square */
	dy = y2 - y1;
	if (dy < 0)
		dy = -dy;
	dx = x2 - x1;
#if 0
	if (dx < 0)
		dx =- dx;
#endif
	if (dx > 40. && dy / dx < 0.7) {
		alfa = 0.3 + 0.002 * (dx - 40.);
		if (alfa > 0.7)
			alfa = 0.7;
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

	mx = 0.5 * (x1 + x2);
	my = 0.5 * (y1 + y2);

	xx1 = mx + alfa * (x1 - mx);
	yy1 = my + alfa * (y1 - my) + height;
	xx1 = x1 + beta * (xx1 - x1);
	yy1 = y1 + beta * (yy1 - y1);

	xx2 = mx + alfa * (x2 - mx);
	yy2 = my + alfa * (y2 - my) + height;
	xx2 = x2 + beta * (xx2 - x2);
	yy2 = y2 + beta * (yy2 - y2);

	dx = 0.03 * (x2 - x1);
	if (dx > 10.)
		dx = 10.;
	dy = 1.;
	dz = 0.2;
	if (x2 - x1 > 100.)
		dz += 0.001 * (x2 - x1);
	if (dz > 0.6)
		dz = 0.6;

	if (staff < 0) {
		PUT4("%.1f %.1f %.1f %.1f ", 
		     xx2 - dx, yy2 + shift + dy * s, xx1 + dx, yy1 + shift + dy * s);
		PUT3("%.1f %.1f 0 %.1f ", x1, y1 + shift + dz * s, dz * s);
		PUT4("%.1f %.1f %.1f %.1f ", xx1, yy1 + shift, xx2, yy2 + shift);
		PUT4("%.1f %.1f %.1f %.1f SL\n", x2, y2 + shift, x1, y1 + shift);
	} else {
		PUT3("%.1f \x01%c%5.2f ",
		     xx2 - dx, '0' + staff, yy2 + shift + dy * s);
		PUT3("%.1f \x01%c%5.2f ",
		     xx1 + dx, '0' + staff, yy1 + shift + dy * s);
		PUT4("%.1f \x01%c%5.2f 0 %.1f ",
		      x1, '0' + staff, y1 + shift + dz * s, dz * s);
		PUT3("%.1f \x01%c%5.2f ",
		     xx1, '0' + staff, yy1 + shift);
		PUT3("%.1f \x01%c%5.2f ",
		     xx2, '0' + staff, yy2 + shift);
		PUT3("%.1f \x01%c%5.2f ",
		     x2, '0' + staff, y2 + shift);
		PUT3("%.1f \x01%c%5.2f SL\n",
		     x1, '0' + staff, y1 + shift);
	}
}

/* -- return min or max, depending on s -- */
static float extreme(int s,
		     float a,
		     float b)
{
	if (s > 0)
		return a > b ? a : b;
	return a > b ? b : a;
}

/* -- decide whether a slur goes up or down -- */
static int slur_direction(struct SYMBOL *k1,
			  struct SYMBOL *k2)
{
	struct SYMBOL *s;
	int are_stems, are_downstems, y_max;

	are_stems = are_downstems = 0;
	y_max = 300;
	for (s = k1; ; s = s->next) {
		if (s->type == NOTE) {
			if (!s->as.u.note.stemless) {
				are_stems = 1;
				if (s->stem < 0)
					are_downstems = 1;
			}
			if (y_max > s->ymn)
				y_max = s->ymn;
		}
		if (s == k2)
			break;
	}
	if (are_downstems
	    || (!are_stems
		&& y_max >= 12))
		return 1;
	return -1;
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

/* -- draw phrasing slur between two symbols -- */
/* (the staves are not yet defined) */
/* (not a pretty routine, this) */
static void draw_phrasing(struct SYMBOL *k1,
			  struct SYMBOL *k2,
			  int level)
{
	struct SYMBOL *k;
	float x1, y1, x2, y2, height, addx, addy;
	float hmin, a;
	float y, z, h, dx, dy;
	int s, nn;
	int upstaff;
	int two_staves;

/*fixme: if two staves, may have upper or lower slur*/
	if (k1 == k2)
		return;
	nbuf_check();
	if ((s = slur_multi(k1, k2)) == 0)
		s = slur_direction(k1, k2);

	nn = 1;
	upstaff = k1->staff;
	two_staves = 0;
	for (k = k1->next; ; k = k->next) {
		if (k->len > 0) {	/* note or rest */
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
if (two_staves) printf("*** multi-staves slurs not treated\n");

	/* fix endpoints */
	x1 = k1->x + k1->xmx;		/* take the max right side */
	x2 = k2->x;
	if (k1->type == NOTE) {		/* here if k1 points to note */
		if (k1->stem * s > 0) {
			x1 += s * 4;
			y1 = k1->ys + s * 2;
			k = next_note(k1);
/*--fixme*/
#if 1
			if (k == k2 && k1->stem != k2->stem) {
				if (s > 0) {
					if (k2->y < y1) {
						y1 = k2->y;
						if (y1 < k1->y)
							y1 = k1->y;
						y1 += 6;
					}
				} else {
					if (k2->y > y1) {
						y1 = k2->y;
						if (y1 > k1->y)
							y1 = k1->y;
						y1 -= 6;
					}
				}
#else
			if (k != 0) {
				if (k == k2) {
					if (s > 0)
						y1 = (k1->ys + k1->ymx) * 0.5;
					else	y1 = (k1->ymn + k1->ys) * 0.5;
				}
				if (k->stem * s > 0)
					y = k->ys;
				else	y = k->y;
				if (k1->stem > 0) {
					if (y > y1)
						y1 = y;
				} else {
					if (y < y1)
						y1 = y;
				}
#endif
			}
		} else	y1 = k1->y + s * 6;
		if (s > 0) {
			if (y1 < k1->dc_top + 2.5)
				y1 = k1->dc_top + 2.5;
		} else {
			if (y1 > k1->dc_bot - 2.5)
				y1 = k1->dc_bot - 2.5;
		}
	} else	y1 = k1->y;

	if (k2->type == NOTE) {		/* here if k2 points to note */
		if (k2->stem * s > 0) {
			x2 += s * 3;
			y2 = k2->ys + s * 2;
			k = prev_note(k2);
/*--fixme*/
#if 1
			if (k == k1 && k2->stem != k1->stem) {
				if (s > 0) {
					if (k1->y < y2) {
						y2 = k1->y;
						if (y2 < k2->y)
							y2 = k2->y;
						y2 += 6;
					}
				} else {
					if (k1->y > y2) {
						y2 = k1->y;
						if (y2 > k2->y)
							y2 = k2->y;
						y2 -= 6;
					}
				}
#else
			if (k != 0) {
				if (k == k1) {
					if (s > 0)
						y2 = (k2->ys + k2->ymx) * 0.5;
					else	y2 = (k2->ymn + k2->ys) * 0.5;
				}
				if (k->stem * s > 0)
					y = k->ys;
				else	y = k->y;
				if (k2->stem > 0) {
					if (y > y2)
						y2 = y;
				} else {
					if (y < y2)
						y2 = y;
				}
#endif
			}
		} else	y2 = k2->y + s * 6;

		if (s > 0) {
			if (y2 < k2->dc_top + 2.5)
				y2 = k2->dc_top + 2.5;
		} else {
			if (y2 > k2->dc_bot - 2.5)
				y2 = k2->dc_bot - 2.5;
		}
	} else	y2 = k2->y;

	if (k1->type != NOTE) {
		x1 += k1->wr;
		y1 = y2 + 1.2 * s;
		if (nn > 1) {
			if (s > 0) {
				if (y1 < 24 + 4)
					y1 = 24 + 4;
			} else {
				if (y1 > -4)
					y1 = -4;
			}
		}
	}

	if (k2->type != NOTE) {
		y2 = y1 + 1.2 * s;
		if (nn > 1) {
			if (s > 0) {
				if (y2 < 24 + 4)
					y2 = 24 + 4;
			} else {
				if (y2 > -4)
					y2 = -4;
			}
		}
	}

	/* shift endpoints */
	addx = 0.04 * (x2 - x1);
	if (addx > 3.0)
		addx = 3.0;
	addy = 0.02 * (x2 - x1);
	if (addy > 3.0)
		addy = 3.0;
	x1 += addx;
	x2 -= addx;

/*fixme: to simplify*/
	if (k1->staff == upstaff)
		y1 += s * addy;
	else	y1 = -6.;
	if (k2->staff == upstaff)
		y2 += s * addy;
	else	y2 = -6.;

	a = (y2 - y1) / (x2 - x1);		/* slur steepness */
	if (a > SLUR_SLOPE)
		a = SLUR_SLOPE;
	else if (a < -SLUR_SLOPE)
		a = -SLUR_SLOPE;
	if (a * s > 0)
		y1 = y2 - a * (x2 - x1);
	else	y2 = y1 + a * (x2 - x1);

	/* for big vertical jump, shift endpoints */
	y = y2 - y1;
	if (y > 8)
		y = 8;
	else if (y < -8)
		y = -8;
	z = y;
	if (z < 0)
		z = -z;
	dx = 0.5 * z;
	dy = 0.3 * y;
	if (y * s > 0) {
		x2 -= dx;
		y2 -= dy;
	} else {
		x1 += dx;
		y1 += dy;
	}

	h = 0;
	a = (y2 - y1) / (x2 - x1);
	addy = y1 - a * x1;
	for (k = k1->next; k != k2 ; k = k->next) {
		if (k->type == NOTE
		    && k->staff == upstaff) {
			y = (s > 0 ? k->ymx : k->ymn);
			y = extreme(s,
				    y + 6 * s,
				    k->ys + 2 * s);
			y -= a * k->x + addy;
			h = extreme(s, h, y);
		}
	}

	y1 += 0.4 * h;
	y2 += 0.4 * h;
	h *= 0.6;

	if (nn > 3)
		hmin = (0.12 * (x2 - x1) + 12.) * s;
	else	hmin = (0.03 * (x2 - x1) + 8.) * s;
	height = extreme(s, hmin, 3.0 * h);
	height = extreme(-s, height, 50. * s);

	y = y2 - y1;
	if (y < 0)
		y = -y;
	if (s > 0) {
		if (height < 0.8 * y)
			height = 0.8 * y;
	} else {
		if (height > -0.8 * y)
			height = -0.8 * y;
	}

	output_slur(x1, y1, x2, y2, s,
		    height, 3. * s * level, upstaff);

	/* have room for other symbols */
	if (s > 0)
		y = (y1 > y2 ? y1 : y2);
	else	y = (y1 < y2 ? y1 : y2);
/*---fixme: why 0.3?*/
	y += 0.3 * height;
	for (k = k1; ; k = k->next) {
		if (k == k2
		    && k->as.u.note.slur_st)
			break;	/* the next slur will set the top/bottom */
		if (k->staff == upstaff) {
			if (k->dc_top < y)
				k->dc_top = y;
			else if (k->dc_bot > y)
				k->dc_bot = y;
		}
		if (k == k2)
			break;
	}
}

/* -- find place to terminate/start slur -- */
static struct SYMBOL *next_scut(struct SYMBOL *s)
{
	struct SYMBOL *prev;

	prev = s;
	for (s = s->next; s != 0; s = s->next) {
		if (s->type == BAR
		    && (s->as.u.bar.type == B_RREP
			|| s->as.u.bar.type == B_DREP
			|| s->as.u.bar.type == B_THIN_THICK
			|| s->as.u.bar.type == B_THICK_THIN
			|| (s->as.text != 0
			    && s->as.text[0] != '1')))
			return s;
		prev = s;
	}
	/*fixme: KO when no note for this voice at end of staff */
	return prev;
}

static struct SYMBOL *prev_scut(struct SYMBOL *s)
{
	struct SYMBOL *sym;
	int voice;

	voice = s->voice;
	for ( ; s != 0; s = s->prev) {
		if (s->type == BAR
		    && (s->as.u.bar.type == B_LREP
			|| s->as.u.bar.type == B_DREP
			|| s->as.u.bar.type == B_THIN_THICK
			|| s->as.u.bar.type == B_THICK_THIN
			|| (s->as.text != 0
			    && s->as.text[0] != '1')))
			return s;
	}

	/* return sym before first note/rest/bar */
	sym = voice_tb[voice].sym;
	for (s = sym; s != 0; s = s->next) {
		if (s->len > 0		/* if note or rest */
		    || s->type == BAR)
			return s->prev;
	}
	return sym;
}

/* -- draw the ties between two notes/chords -- */
static void draw_chord_ties(struct SYMBOL *k1,
			    struct SYMBOL *k2,
			    int nslur,
			    int *mhead1,
			    int *mhead2,
			    int job)
{
	struct SYMBOL *cut;
	int i, m1, p1, p2, y;
	int s0, s1, s;
	float x1, y1, x2, y2, height, addx, addy;

	s1 = 0;
	if ((s0 = k1->multi) == 0
	    && (s0 = k2->multi) == 0)
		s1 = slur_direction(k1, k2);
	for (i = 0; i < nslur; i++) {
		int m2;

		m1 = mhead1[i];
		p1 = k1->pits[m1];
		m2 = mhead2[i];
		p2 = k2->pits[m2];
		if ((s = s0) == 0) {

			/* try to have the same tie direction as the next one*/
			if (job != 2
			    && k2->nhd != 0
			    && m2 == k2->nhd
			    && k2->as.u.note.ti1[m2])
				s = 1;
			else if (k1->nhd == 0)
				s = s1;
			else if (m1 == 0)	/* if bottom */
				s = -1;
			else if (m1 == k1->nhd)	/* if top */
				s = 1;
			else	s = s1;
		}

		x1 = k1->x + k1->shhd[m1];
		x2 = k2->x;
		if (job == 2) {		/* half slurs from last note in line */
			cut = next_scut(k1);
			x2 = cut->x;
			if (cut == k1) {
				x2 = realwidth;
#if 0
				if (x2 < x1 + 20.)
					x2 = x1 + 20.;
#endif
			}
		} else if (job == 1) {	/* half slurs to first note in line */
			cut = prev_scut(k1);
			x1 = cut->x;
			if (cut == k1->prev) {
				x1 = cut->x + cut->wr;
				if (x1 > x2 - 20.)
					x1 = x2 - 20.;
			}
		}

		addx = 0.04 * (x2 - x1);
		if (addx > 3.0)
			addx = 3.0;
		addy = 0.02 * (x2 - x1);
		if (addy > 3.0)
			addy = 3.0;

		x1 += 3. + addx;
		x2 -= 3. + addx;
		if (s > 0) {
			if (k1->stem > 0)
				x1 += 1.5;
		} else /*if (s < 0)*/ {
			if (k2->stem < 0)
				x2 -= 1.5;
		}

		if (k1->staff != k2->staff) {

			/* tie between 2 staves */
			s = k1->staff - k2->staff;
			y1 = (float) (3 * (p1 - 18) + 6 * s)
				+ staff_tb[(unsigned) k1->staff].y;
			y2 = (float) (3 * (p2 - 18) - 6 * s)
				+ staff_tb[(unsigned) k2->staff].y;
			PUT4("%.1f %.2f M %.1f %.2f lineto stroke\n",
			     x1, y1, x2, y2);
			continue;
		}

		y = 3 * (p1 - 18);
		y1 = y + (4. + addy) * s;
		y = 3 * (p2 - 18);
		y2 = y + (4. + addy) * s;

#if 0 /*??*/
		if (s > 0 && !(y % 6) && k1->dots > 0) {
			y2 = y1 = y + (5.5 + addy) * s;
			x1 -= 2;
			x2 += 2;
		}
#endif
		y1 += staff_tb[(unsigned) k1->staff].y;
		y2 += staff_tb[(unsigned) k2->staff].y;

		height = (0.04 * (x2 - x1) + 5.) * s;
		output_slur(x1, y1, x2, y2,
			    s, height, 0.0, -1);
	}
}

/* -- draw ties between neighboring notes/chords -- */
static void draw_ties(struct SYMBOL *k1,
		      struct SYMBOL *k2,
		      int job)
{
	int i, j, m1;
	int mhead1[MAXHD], mhead2[MAXHD], nslur, nh1, nh2;

	nbuf_check();

	nslur = 0;
	nh1 = k1->nhd;

	if (job == 2) {			/* half slurs from last note in line */
		for (i = 0; i <= nh1; i++) {
			if (k1->as.u.note.ti1[i]) {
				mhead1[nslur] = i;
				nslur++;
			}
			j = i + 1;
			for (m1 = 0; m1 <= nh1; m1++) {
				if (k1->as.u.note.sl1[m1] == j) {
					mhead1[nslur] = m1;
					nslur++;
					break;
				}
			}
		}
		if (nslur > 0)
			draw_chord_ties(k1, k1,
					nslur, mhead1, mhead1, job);
		return;
	}

	if (job == 1) {			/* half slurs to first note in line */
		/* (ti2 is just used in this case) */
		for (i = 0; i <= nh1; i++) {
			if (k1->as.u.note.ti2[i]) {
				mhead1[nslur] = i;
				nslur++;
			}
			j = i + 1;
			for (m1 = 0; m1 <= nh1; m1++) {
				if (k1->as.u.note.sl2[m1] == j) {
					mhead1[nslur] = m1;
					nslur++;
					break;
				}
			}
		}
		if (nslur > 0)
			draw_chord_ties(k1, k1,
					nslur, mhead1, mhead1, job);
		return;
	}

	/* real 2-note case: set up list of slurs/ties to draw */
	nh2 = k2->nhd;
	for (i = 0; i <= nh1; i++) {
		int m2;

		if ((m2 = k1->as.u.note.ti1[i]) != 0) {
			mhead1[nslur] = i;
			mhead2[nslur] = m2 - 1;
			nslur++;
		}
		j = i + 1;
		for (m1 = 0; m1 <= nh1; m1++) {
			if (k1->as.u.note.sl1[m1] == j) {
				for (m2 = 0; m2 <= nh2; m2++) {
					if (k2->as.u.note.sl2[m2] == j) {
						mhead1[nslur] = m1;
						mhead2[nslur] = m2;
						nslur++;
						break;
					}
				}
			}
		}
	}
	if (nslur > 0)
		draw_chord_ties(k1, k2,
				nslur, mhead1, mhead2, job);
}

/* -- draw all slurs/ties between neighboring notes -- */
static void draw_all_ties(struct SYMBOL *sym)
{
	struct SYMBOL *s1, *s2;

	for (s1 = sym; s1 != 0; s1 = s1->next) {
		if (s1->type == NOTE)
			break;
	}
	if (s1 == 0)
		return;
	draw_ties(s1, s1, 1);
  
	for (;;) {
		for (s2 = s1->next; s2 != 0; s2 = s2->next) {
			if (s2->type == NOTE)
				break;
		}
		if (s2 == 0)
			break;
		draw_ties(s1, s2, 0);
		s1 = s2;
	}
	draw_ties(s1, s1, 2);
}

/* -- draw all phrasing slurs for one staff -- */
/* (the staves are not yet defined) */
static void draw_all_phrasings(struct SYMBOL *sym)
{
	struct SYMBOL *s, *s1, *k;
	struct SYMBOL *cut;
	int pass,num;

	for (pass = 0; ; pass++) {
		num = 0;
		for (s = sym; s != 0; s = s->next) {
			if (s->type != NOTE
			    || !s->as.u.note.slur_st)
				continue;
			k = 0;			/* find matching slur end */
			for (s1 = s->next; s1 != 0; s1 = s1->next) {
				if (s1->as.u.note.slur_end) {
					k = s1;
					break;
				}
				if (s1->as.u.note.slur_st)
					break;
			}
			if (k == 0)
				continue;
			cut = next_scut(s);
			for (s1 = cut->next; s1 != 0; s1 = s1->next)
				if (s1 == k)
					break;
			if (s1 != 0 && s1 != k) {
				draw_phrasing(s, cut, pass);
				s = prev_scut(k);
			}
			draw_phrasing(s, k, pass);
			num++;
			s->as.u.note.slur_st--;
			k->as.u.note.slur_end--;
		}
		if (num == 0)
			break;
	}

	/* do unbalanced slurs still left over */
	for (s = sym; s != 0; s = s->next) {
		if (s->type != NOTE)
			continue;
		if (s->as.u.note.slur_end) {
			cut = prev_scut(s);
			draw_phrasing(cut, s, 0);
		}
		if (s->as.u.note.slur_st) {
			cut = next_scut(s);
			draw_phrasing(s, cut, 0);
/*fixme: a slur may end in the raw after the next one */
		}
	}
}

/* -- draw the lyrics under notes -- */
/* !! this routine is tied to set_width() and set_staff() !! */
static void draw_vocals(struct SYMBOL *sym)
{
	int hyflag, l, j, lflag, nwl;
	float lastx, vfsize, w, swfac, lskip;
	unsigned char t[81];
	float y;
	int curfont;

	if ((nwl = voice_tb[(unsigned) sym->voice].nvocal) == 0)
		return;
	y = voice_tb[(unsigned) sym->voice].yvocal;
	curfont = -1;				/* (force new font) */
	vfsize = lskip = swfac = 0;		/* (compiler warning) */
	for (j = 0; j < nwl; j++) {
		struct SYMBOL *s;
		float x0;

		hyflag = lflag = 0;
		lastx = -10;
		x0 = 0;		/* (compiler warning) */
		for (s = sym; s != 0; s = s->next) {
			struct lyrics *ly;
			float shift;
			char *p;

			if (s->type != NOTE
			    || (ly = s->ly) == 0
			    || (p = ly->w[j]) == 0) {
				switch (s->type) {
				case REST:
				case MREST:
				case MREP:
					if (lflag) {
						PUT3("%.1f %.1f %.1f wln ",
						     x0 - lastx, lastx + 3, y);
						lflag = 0;
					}
				}
				continue;
			}
			if (*p++ != curfont) {	/* font change (see get_lyric) */
				curfont = p[-1];
				vfsize = lyric_fonts[curfont].size;
				lskip = 1.1 * vfsize;
				swfac = cfmt.vocalfont.swfac * vfsize;
				PUT2("%.1f F%d ",
					vfsize,
					lyric_fonts[curfont].font);
			}
			tex_str(t, p, sizeof t, &w);
			if (isdigit(t[0]))
				shift = LYDIG_SH * swfac * cwid('1');
			else if (t[0] == '\x03')	/* '_' */
				shift = 0;
			else {
				shift = swfac * (w + 2 * cwid(' ')) * VOCPRE;
				if (shift > 20.)
					shift = 20.;
			}
			if (lflag
			    && t[0] != '\x03') {	/* not '_' */
				PUT3("%.1f %.1f %.1f wln ",
				     x0 - lastx + 3., lastx + 3., y);
				lflag = 0;
			}

			x0 = s->x - shift;
			if (hyflag) {
				float x;

				x = (x0 + lastx - 2.) * 0.5;
				PUT2("%.1f %.1f whf ", x, y);
				hyflag = 0;
				lastx = x + 4.;
				if (t[0] == '\x03')	/* '_' */
					t[0] = '\x02';
			}

			if (t[0] == '\x03') {		/* '_' */
				if (lastx < 0)
					lastx = s->prev->x + s->prev->wr;
				lflag = 1;
				continue;
			}
			if (t[0] == '\x02') {		/* '-' between syllabes */
				hyflag = 1;
				continue;
			}
			l = strlen(t);
			if (t[l - 1] == '\x02') {	/* '-' between syllabes */
				t[l - 1] = '\0';
				hyflag = 1;
			}
			PUT3("(%s) %.1f %.1f wd ", t, x0, y);
			lastx = x0 + swfac * w;
		}
		if (hyflag)
			PUT2("%.1f %.1f whf ", lastx + 5., y);
		if (lflag)
			PUT3("%.1f %.1f %.1f wln",
			     x0 - lastx + 3., lastx + 3., y);
		PUT0("\n");
		y -= lskip;
	}
}

/* -- draw the symbols near the notes -- */
/* (the staves are not yet defined) */
/* order:
 * - beams
 * - decorations near the notes
 * - n-plets
 * - slurs
 * - decorations tied to the notes
 * - guitar chords, then remaining decorations
 */
void draw_sym_near(void)
{
	struct VOICE_S *p_voice;
	struct SYMBOL *s;

	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		struct BEAM bm;

		bm.s2 = 0;
		bm.staff = 0;
		for (s = p_voice->sym; s != 0; s = s->next) {
			if (s->type == NOTE
			    && (s->sflags & S_WORD_ST)
			    && !s->as.u.note.word_end) {
				if (calculate_beam(&bm, s))
					draw_beams(&bm);
				if (s == bm.s2)
					bm.s2 = 0;
			}
		}
	}

	draw_deco_near();

	/* have room for the accidentals */
	for (s = first_voice->sym; s != 0; s = s->ts_next) {
		int nhd;
		float y;

		if (s->type != NOTE)
			continue;
		nhd = s->as.u.note.nhd;
		if (s->as.u.note.accs[nhd]) {
			y = s->y + 8;
			if (s->dc_top < y)
				s->dc_top = y;
		}
		if (s->as.u.note.accs[0]) {
			y = s->y;
			if (s->as.u.note.accs[0] == A_SH
			    || s->as.u.note.accs[0] == A_NT)
				y -= 7;
			else	y -= 5;
			if (s->dc_bot > y)
				s->dc_bot = y;
		}
	}

	for (p_voice = first_voice; p_voice; p_voice = p_voice->next)
		draw_nplet_brackets(p_voice->sym);

	for (p_voice = first_voice; p_voice; p_voice = p_voice->next)
		draw_all_phrasings(p_voice->sym);

	/* set the top and bottom of all symbols out of the staves */
	for (s = first_voice->sym; s != 0; s = s->ts_next) {
		if (s->dc_top < 24. + 2.)
			s->dc_top = 24. + 2.;
		if (s->dc_bot > -2.)
			s->dc_bot = -2.;
	}

	draw_deco_note();

	draw_deco_staff();
}

/* -- draw remaining symbols when the staves are defined -- */
void draw_symbols(struct VOICE_S *p_voice)
{
	struct SYMBOL *sym;
	float x, y;
	struct BEAM bm;
	struct SYMBOL *s;
	int bar_time;

	sym = p_voice->sym;

	/* draw the symbols */
	bm.s2 = 0;
	bm.staff = -1;
	bar_time = sym->time + p_voice->wmeasure;
	for (s = sym; s != 0; s = s->next) {
		nbuf_check();
		x = s->x;
		switch (s->type) {
		case NOTE:
			if ((s->sflags & S_WORD_ST)
			    && !s->as.u.note.word_end) {
				if (calculate_beam(&bm, s))
					draw_beams(&bm);
			}
			draw_note(x, s->shrink - s->prev->shrink,
				  x - s->prev->x,
				  s, bm.s2 == 0);
			if (s == bm.s2)
				bm.s2 = 0;
			break;
		case REST:
			draw_rest(s);
			break;
		case BAR:
			if (p_voice != first_voice)
				break;
			draw_bar(x, s);
			if (s->u != 0)
				nbar = s->u;		/* (%%setbarnb) */
			if (s->time < bar_time		/* incomplete measure */
			    || s->as.u.bar.type == B_INVIS)
				break;
			if (s->u == 0)
				nbar++;
			bar_time = s->time + p_voice->wmeasure;
			if (s->next != 0
			    && cfmt.measurenb > 0
			    && (nbar % cfmt.measurenb) == 0) {
				set_font(&cfmt.composerfont);
				PUT4("%.1f %.1f M (%d) show%s\n",
				     x, s->dc_top + staff_tb[0].y + 3.,
				     nbar, cfmt.measurebox ? "b" : "");
			}
			break;
		case CLEF: {
			int	staff;
			char	ct = 't';		/* clef type - def: treble */

			if (p_voice->second)
				continue;		/* only one clef per staff */
			staff = s->staff;
			memcpy(&staff_tb[staff].clef, &s->as.u.clef, /* (for next lines) */
				sizeof s->as.u.clef);
			y = staff_tb[staff].y;

			switch (s->as.u.clef.type) {
			case BASS:
				ct = 'b';
				y += (float) ((s->as.u.clef.line - 4) * 6);
				break;
			case ALTO:
				ct = 'c';
				y += (float) ((s->as.u.clef.line - 3) * 6);
				break;
			case TREBLE:
				y += (float) ((s->as.u.clef.line - 2) * 6);
				break;
			default:
				bug("unknown clef type", 0);
			}
			PUT4("%.1f %.1f %c%cclef\n", x, y,
			     s->u ? 's' : ' ', ct);
			if (s->as.u.clef.octave != 0) {
/*fixme: OK for treble clef only*/
				if (s->as.u.clef.octave > 0) {
					y += 36.;
					x -= 1.5;
				} else {
					y -= 18.;
					x -= 3.5;
				}
				PUT2("/Times-Roman 10 selectfont %.1f %.1f M (8) show\n",
				     x, y);
			}
			break;
		}
		case TIMESIG:
			if (p_voice != first_voice)
				break;
			draw_timesig(x, s);
			memcpy(&first_voice->meter, &s->as.u.meter,
			       sizeof first_voice->meter);
			break;
		case KEYSIG:
			draw_keysig(p_voice, x, s);
			break;
		case MREST:
		case MREP:
			if (p_voice->second)
				continue;
			if (s->type == MREST)
				PUT3("(%d) %.1f %.1f mrest\n",
				     s->as.u.note.lens[0],
				     x, staff_tb[(unsigned) s->staff].y + 12.);
			else	PUT2("%.1f %.1f mrep\n",
				     x, staff_tb[(unsigned) s->staff].y + 6.);
			if (p_voice == first_voice)
				nbar += s->as.u.note.lens[0] - 1;
			break;
		case INVISIBLE:
		case TEMPO:
		case STAVES:
		case PART:
			break;			/* nothing */
		default:
			bug("Symbol not drawn", 1);
		}
	}

	draw_all_ties(sym);

	draw_vocals(sym);
}
