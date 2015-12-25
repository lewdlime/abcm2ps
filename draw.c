/*
 * Drawing functions.
 *
 * This file is part of abcm2ps.
 *
 * Copyright (C) 1998-2004 Jean-François Moine
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

static void draw_note(float x,
		      struct SYMBOL *s,
		      int fl);

/* -- check if space enough in the output buffer -- */
static void nbuf_check(void)
{
	if (nbuf + 100 > BUFFSZ) {
		error(1, 0, "PS output exceeds reserved space per staff"
		       " -- increase BUFFSZ");
		exit(1);
	}
}

/* -- up/down shift needed to get k*6 -- */
static float rnd6(float y)
{
	int iy;

	iy = (int) (y + 2.999) / 6 * 6;
	return iy - y;
}

/* -- compute the best vertical offset for the beams -- */
static float b_pos(int stem,
		   int flags,
		   float b)
{
	float d1, d2;
	float top, bot;

	if (stem > 0) {
		bot = b - (flags - 1) * BEAM_SHIFT - BEAM_DEPTH;
		if (bot > 26)
			return 0;
		top = b;
	} else {
		top = b + (flags - 1) * BEAM_SHIFT + BEAM_DEPTH;
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

/* -- same as previous, but for grace note -- */
static float b_gpos(int stem,
		   int flags,
		   float b)
{
	float d1, d2;
	float top, bot;

	bot = b - (flags - 1) * 3 - 1.7;
	if (bot > 26)
		return 0;
	top = b;

	d1 = rnd6(top - BEAM_OFFSET);
	d2 = rnd6(bot + BEAM_OFFSET);
	if (d1 * d1 > d2 * d2)
		return d2;
	return d1;
}

/* -- calculate_beam -- */
/* (the staves may be defined or not) */
static int calculate_beam(struct BEAM *bm,
			  struct SYMBOL *s1)
{
	struct SYMBOL *s, *s2;
	int notes, flags, staff, voice;
	float x, y, ys, a, b, max_stem_err;
	float sx, sy, sxx, sxy, syy, a0, stem_xoff;
	int two_staves;
  
	/* find first and last note in beam */
	notes = flags = 0;	/* set x positions, count notes and flags */
	two_staves = 0;
	staff = s1->staff;
	stem_xoff = s1->as.u.note.grace ? GSTEM_XOFF : STEM_XOFF;
	for (s = s1; ; s = s->next) {
		s->xs = s->x;
		if (s->type != NOTE)
			continue;
		if (s->stem > 0)
			s->xs +=  stem_xoff + s->shhd[0];
		else	s->xs += -stem_xoff + s->shhd[s->nhd];
		if (s->nflags > flags)
			flags = s->nflags;
		notes++;
		if (s->staff != staff)
			two_staves = 1;
		if (s->as.u.note.word_end)
			break;
	}

	if ((s2 = s) == 0) {
		error(1, s1->as.linenum, "No beam end!");
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
		if (!two_staves && !s1->as.u.note.grace)
			return 0;
	}

	sx = sy = sxx = sxy = syy = 0;	/* linear fit through stem ends */
	for (s = s1; ; s = s->next) {
		if (s->type != NOTE)
			continue;
		x = s->xs;
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
	if (!s1->as.u.note.grace) {
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

/*  if (flags>1) b=b+2*stem;*/	/* leave a bit more room if several beams */

	/* have flat beams when asked */
	if (cfmt.flatbeams
	    && voice_tb[s1->voice].key.bagpipe) {
		if (!s1->as.u.note.grace)
			b = -11 + staff_tb[s1->staff].y;
		else	b = 35 + staff_tb[s1->staff].y;
		a = 0;
	}

/*fixme: have a look again*/
	/* have room for the symbols in the staff */
	max_stem_err = 0;		/* check stem lengths */
	voice = s1->voice;
	s = s1;
	if (two_staves)
		/*fixme: to do*/
		;
	else if (!s1->as.u.note.grace) {
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
				ys = a * s->xs + b - staff_tb[s->staff].y;
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
/*fixme: KO when two_staves */
				ys = a * s->x + b - staff_tb[s->staff].y;
				if (s1->stem > 0) {
					if (s->stem > 0) {
/*fixme: KO when the voice numbers are inverted */
						if (s->voice < voice)
							continue;
						if (s->y > ys)
							continue;
						stem_err = s->ys + 8. - ys;
					} else	stem_err = s->y + 8. - ys;
				} else {
					if (s->stem > 0)
						stem_err = ys - s->y + 8.;
					else {
						stem_err = ys - s->ys + 8.;
						if (s->y < ys)
							continue;
					}
				}
			}
			if (stem_err > max_stem_err)
				max_stem_err = stem_err;
		}
	} else {			/* grace notes */
		for ( ; ; s = s->next) {
			float min_stem, stem_err, slen;

			ys = a * s->xs + b - staff_tb[s->staff].y;
			min_stem = GSTEM;
			slen = ys - s->ymx;
			stem_err = min_stem - slen;
			if (stem_err > max_stem_err)
				max_stem_err = stem_err;
			if (s == s2)
				break;
		}
	}

	if (max_stem_err > 0)		/* shift beam if stems too short */
		b += s1->stem * max_stem_err;

	/* have room for the gracenotes */
	for (s = s1->next; ; s = s->next) {
		struct SYMBOL *g;

		if ((g = s->grace) == 0) {
			if (s == s2)
				break;
			continue;
		}
		x = s->x;
		for (; g != 0; g = g->next) {
			float yyg, try;

			yyg = a * (x + g->x) + b;

			if (s->next->stem > 0) {
				try = g->ys - yyg
					+ BEAM_DEPTH + (flags - 1) * BEAM_SHIFT
					+ 2;
				if (try > 0)
					b += try;
			} else {
				try = g->y - yyg
					- BEAM_DEPTH - (flags - 1) * BEAM_SHIFT
					- 7;
				if (try < 0)
					b += try;
			}
		}
		if (s == s2)
			break;
	}

	if (a == 0) {		/* shift flat beams onto staff lines */
		if (!s1->as.u.note.grace)
			b += b_pos(s1->stem, flags, b - staff_tb[s1->staff].y);
		else	b += b_gpos(s1->stem, flags, b - staff_tb[s1->staff].y);
	}

	/* adjust final stems and rests under beam */
	for (s = s1; ; s = s->next) {
		switch (s->type) {
		case NOTE:
			s->ys = a * s->xs + b - staff_tb[s->staff].y;
			if (s->stem > 0) {
				if (s->dc_top < s->ys + 2)
					s->dc_top = s->ys + 2;
			} else {
				if (s->dc_bot > s->ys - 2)
					s->dc_bot = s->ys - 2;
			}
			break;
		case REST:
			y = a * s->xs + b - staff_tb[s->staff].y;
			if (s1->stem > 0) {
				y -= BEAM_DEPTH + (flags - 1) * BEAM_SHIFT;
				y -= s->head != H_FULL ? 4 : 9;
				if (s1->multi == 0 && y > 12)
					y = 12;
				if (s->y <= y)
					break;
			} else {
				y += BEAM_DEPTH + (flags - 1) * BEAM_SHIFT;
				y += s->head != H_FULL ? 4 : 11;
				if (s1->multi == 0 && y < 12)
					y = 12;
				if (s->y >= y)
					break;
			}
			if (s->head != H_FULL) {
				int iy;

				iy = (int) (y + 3.0) / 6 * 6;
				y = iy;
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
	if (!s1->as.u.note.grace)
		bm->t = s1->stem * BEAM_DEPTH;
	else	bm->t = s1->stem * 1.6;
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
	float shift, bshift, bstub;
	int two_staves;

	nbuf_check();
	s1 = bm->s1;
	s2 = bm->s2;
	if (!s1->as.u.note.grace) {
		bshift = BEAM_SHIFT;
		bstub = BEAM_STUB;
	} else {
		bshift = 3;
		bstub = 3.2;
	}

	/* make first beam over whole word */
	maxfl = 1;
	for (s = s1; ; s = s->next) {
		if (s->nflags > maxfl)
			maxfl = s->nflags;
		if (s == s2)
			break;
	}
	draw_beam(s1->xs, s2->xs, 0.0, bm);

	/* other beams with two or more flags */
	two_staves = bm->staff < 0;
	shift = 0;
	for (i = 2; i <= maxfl; i++) {
		struct SYMBOL *k1, *k2;
		int inbeam;

		k1 = k2 = s1;
		inbeam = 0;
		shift += bshift;
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
				if (s->nflags >= i)
					k2 = s;
				x1 = k1->xs;
				if (k1 == k2) {
					if (k1 == s1)
						x1 += bstub;
					else if (k1 == s2 || (k1->sflags & S_BEAM_BREAK)) {
						x1 -= bstub;
					} else {
						struct SYMBOL *k;

						k = k1->prev;
						while (k != 0 && k->type != NOTE)
							k = k->prev;
						if (k == 0
						    || (k->sflags & S_BEAM_BREAK)
						    || k->nflags < k1->next->nflags
						    || (k->nflags == k1->next->nflags
						     && k->dots < k1->next->dots))
							x1 += bstub;
						else	x1 -= bstub;
					}
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
							    - staff_tb[k1->staff].y
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
			if ((k2 = s) == s2)
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
		int nl;
		char *v[8];
	} staff_d[MAXSTAFF], *staff_p;
	char *p, *q;
	char t[64];
	float y;

	memset(staff_d, 0, sizeof staff_d);
	n = 0;
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		staff = p_voice->staff;
		if (staff_tb[staff].brace_end)
			staff--;
		staff_p = &staff_d[staff];
		if (first_line) {
			if ((p = p_voice->nm) == 0)
				continue;
		} else {
			if ((p = p_voice->snm) == 0)
				continue;
		}
		for (;;) {
			staff_p->v[staff_p->nl++] = p;
			if ((p = strstr(p, "\\n")) == 0)
				break;
			p += 2;
		}
		n++;
	}
	if (n == 0)
		return;
	set_font(&cfmt.vocalfont);
	indent = -indent * 0.5;
	for (staff = nstaff; staff >= 0; staff--) {
		staff_p = &staff_d[staff];
		if (staff_p->nl == 0)
			continue;
		y = staff_tb[staff].y + 12. + 9. * (staff_p->nl - 1)
			- cfmt.vocalfont.size * 0.3;
		if (staff_tb[staff].brace)
			y -= (staff_tb[staff].y - staff_tb[staff + 1].y) * 0.5;
		for (n = 0; n < staff_p->nl; n++) {
			p = staff_p->v[n];
			if ((q = strstr(p, "\\n")) != 0)
				*q = '\0';
			tex_str(t, p, sizeof t, 0);
			PUT3("%.1f %.1f M (%s) cshow\n", indent, y, t);
			y -= 18.;
			if (q != 0)
				*q = '\\';
		}
	}
}

/* -- draw the left side of the staves -- */
static void draw_lstaff(float x)
{
	int i;

	PUT3("%.1f %.1f %.1f bar\n",
	     staff_tb[0].y - staff_tb[nstaff].y + 24.,
	     x, staff_tb[nstaff].y);
	for (i = 0; i <= nstaff; i++) {
		float y;

		if (staff_tb[i].brace) {
			y = staff_tb[i].y + 24.;
			PUT3("%.1f %.1f %.1f brace\n",
			     y - staff_tb[++i].y, x, y);
		} else if (staff_tb[i].bracket) {
			y = staff_tb[i++].y + 24.;
			while (!staff_tb[i].bracket_end)
				i++;
			PUT3("%.1f %.1f %.1f bracket\n",
			     y - staff_tb[i].y, x, y);
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
		PUT2("%.1f 0 %.1f staff\n",
		     realwidth, staff_tb[i].y);
	}

	if (nstaff != 0)
		draw_lstaff(0);
}

/* -- draw the time signature -- */
static void draw_timesig(float x,
			 struct SYMBOL *s)
{
	int i, j;

	if (s->as.u.meter.wmeasure == 0)
		return;
	x = x - s->wl + 3.5;
	for (i = 0; i < s->as.u.meter.nmeter; i++) {
		char *f, meter[64];
		int l;
		float dx;

		if (s->as.u.meter.meter[i].top[0] == ' ') {
			x += 6.;			/* half-space */
			continue;
		}
		l = strlen(s->as.u.meter.meter[i].top);
		if (s->as.u.meter.meter[i].bot[0] != '\0') {
			int l2;

			sprintf(meter, "(%.8s) (%.2s) ",
				s->as.u.meter.meter[i].top,
				s->as.u.meter.meter[i].bot);
			f = "tsig";
			l2 = strlen(s->as.u.meter.meter[i].bot);
			if (l2 > l)
				l = l2;
		} else {
			if (s->as.u.meter.meter[i].top[0] == 'C') {
				if (s->as.u.meter.meter[i].top[1] != '|')
					f = "csig";
				else	f = "ctsig";
				meter[0] = '\0';
			} else if (s->as.u.meter.meter[i].top[0] == '('
				   || s->as.u.meter.meter[i].top[0] == ')') {
				sprintf(meter, "(\\%s) ",
					s->as.u.meter.meter[i].top);
				f = "stsig";
			} else {
				sprintf(meter, "(%.8s) ",
					s->as.u.meter.meter[i].top);
				f = "stsig";
			}
		}
		dx = 7 * l;
		for (j = nstaff; j >= 0; j--)
			PUT4("%s%.1f %.1f %s\n",
			     meter, x + dx * 0.5,
			     staff_tb[j].y, f);
		x += dx;
	}
}

/* -- draw a key signature -- */
static void draw_keysig(struct VOICE_S *p_voice,
			float x,
			struct SYMBOL *s)
{
	int old_sf = s->u;
	int staff = p_voice->staff;
	float staffb = staff_tb[staff].y;
	int i, clef_ix;
	int shift, clef_shift;

	static char sharp_tb[7] = {24, 15, 27, 18, 9, 21, 12};
	static char flat_tb[7] = {12, 21, 9, 18, 6, 15, 3};
	static signed char sharp_cl[7] = {0, -15, -9, -3, -18, -12, -6};
	static signed char flat_cl[7] = {0, 6, -9, -3, 3, -12, -6};

	/* memorize the current keysig of the voice */
	memcpy(&p_voice->key, &s->as.u.key, sizeof p_voice->key);

	if (p_voice->second)
		return;

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

	/* normal accidentals */
	if (s->as.u.key.nacc == 0) {

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

		/* if less sharps or flats, put neutrals */
		/* sharps */
		} else if (s->as.u.key.sf > 0) {
			if (s->as.u.key.sf < old_sf) {
				clef_shift = sharp_cl[clef_ix];
				for (i = s->as.u.key.sf; i < old_sf; i++) {
					if ((shift = sharp_tb[i] + clef_shift) < -3)
						shift += 21;
					PUT2("%.1f %.1f nt0 ", x, staffb + shift);
					x += 5;
				}
				x += 3;		/* extra space */
			}
		/* flats */
		} else /*if (s->as.u.key.sf < 0)*/ {
			if (s->as.u.key.sf > old_sf) {
				clef_shift = flat_cl[clef_ix];
				for (i = s->as.u.key.sf; i > old_sf; i--) {
					if ((shift = flat_tb[-i] + clef_shift) < -3)
						shift += 21;
					PUT2("%.1f %.1f nt0 ", x, staffb + shift);
					x += 5;
				}
				x += 3;		/* extra space */
			}
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
	} else {
		int last_acc;

		/* explicit accidentals */
		last_acc = s->as.u.key.accs[0];
		for (i = 0; i < s->as.u.key.nacc; i++) {
			char *p;

			shift = sharp_cl[clef_ix];
			p = "sh0";
			if (s->as.u.key.accs[i] != last_acc) {
				last_acc = s->as.u.key.accs[i];
				x += 3;
			}
			switch (last_acc) {
			case A_SH:
			case A_DS:
				break;
			case A_FT:
			case A_DF:
				shift = flat_cl[clef_ix];
				p = "ft0";
				break;
			case A_NT:
				p = "nt0";
				break;
			}
			shift += 3 * (s->as.u.key.pits[i] - 18);
			if (shift < -3)
				shift += 21;
			else if (shift >= 24 + 3)
				shift -= 21;
			PUT3("%.1f %.1f %s ", x, staffb + shift, p);
			x += 5;
		}
	}
	if (old_sf != 0 || s->as.u.key.sf != 0 || s->as.u.key.nacc != 0)
		PUT0("\n");
}

/* -- draw a measure bar -- */
static void draw_bar(float x,
		     struct SYMBOL *s)
{
	int	staff;
	float	stafft, y;
	int bar_type, dash;
	char *psf;

	dash = 0;
	bar_type = s->as.u.bar.type;
	switch (bar_type) {
	case B_OBRA:
	case B_CBRA:
	case (B_OBRA << 4) + B_CBRA:
		return;
	case B_COL:
		dash = 1;
		bar_type = B_BAR;
		break;
	case (B_CBRA << 4) + B_BAR:
		bar_type = B_BAR;
		break;
	case (B_BAR << 4) + B_COL:
		bar_type |= (B_OBRA << 8);
		break;
	case (B_BAR << 8) + (B_COL << 4) + B_COL:
		bar_type |= (B_OBRA << 12);
		break;
	case (B_BAR << 12) + (B_COL << 8) + (B_COL << 4) + B_COL:
		bar_type |= (B_OBRA << 16);
		break;
	case (B_COL << 4) + B_BAR:
	case (B_COL << 8) + (B_COL << 4) + B_BAR:
	case (B_COL << 12) + (B_COL << 8) + (B_COL << 4) + B_BAR:
		bar_type <<= 4;
		bar_type |= B_CBRA;
		break;
	case (B_COL << 4) + B_COL:
		bar_type = (B_COL << 12) + (B_CBRA << 8) + (B_OBRA << 4) + B_COL;
		break;
	}
	for (;;) {
		stafft = staff_tb[0].y + 24.;	/* top of upper staff */
		psf = "bar";
		switch (bar_type & 0x0f) {
		case B_BAR:
			if (dash)
				psf = "dabar";
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
		switch (bar_type & 0x0f) {
		default:
			for (staff = 0; ; staff++) {
				if (staff_tb[staff].stop_bar
				    || staff == nstaff) {
					y = staff_tb[staff].y;
					PUT4("%.1f %.1f %.1f %s ",
					     stafft - y, x, y, psf);
					if (staff == nstaff)
						break;
					stafft = staff_tb[staff + 1].y + 24.;
				}
			}
			break;
		case B_COL:
			for (staff = nstaff; staff >= 0; staff--)
				PUT2("%.1f %.1f rdots ",
				     x + 1, staff_tb[staff].y);
			break;
		}
		bar_type >>= 4;
		if (bar_type == 0)
			break;
		x -= 3;
	}
	PUT0("\n");
}

/* -- draw a rest -- */
/* (the staves are defined) */
static void draw_rest(struct SYMBOL *s)
{
	int i, y;
	float x, dotx, staffb;

static char *rest_tb[9] = {
	"r64", "r32", "r16", "r8",
	"r4",
	"r2", "r1", "r0", "r00"
};

	if (s->as.u.note.invis)
		return;

	x = s->x + s->shhd[0];
	y = s->y;
	i = 4 - s->nflags;		/* rest_tb index */

	/* if rest alone in the measure, do it semibreve and center */
	if (s->as.u.note.len == voice_tb[s->voice].meter.wmeasure) {
		struct SYMBOL *prev;
/*fixme: vertical spacing pb may occur when multi-voice*/
		i = 6;			/* r1 */
		s->dots = 0;
		if (s->next != 0)
			x = s->next->x;
		else	x = realwidth;
		prev = s->prev;
		while (prev->type == TEMPO || prev->type == PART)
			prev = prev->prev;
		x = (x + prev->x) * 0.5;

		/* center the associated decorations */
		if (s->as.u.note.dc.n > 0)
			deco_update(s, x - s->x);
	}

	staffb = staff_tb[s->staff].y;	/* bottom of staff */

	PUT3("%.1f %.1f %s", x, y + staffb, rest_tb[i]);

	/* add helper line(s) */
	switch (i) {
	case 8:			/* breve / longa */
	case 7:
		if (y >= 24)
			PUT1(" %.1f hl", y + 6 + staffb);
		if (i == 7) {
			if (y <= -6)
				PUT1(" %.1f hl", y + staffb);
		} else {
			if (y <= 0)
				PUT1(" %.1f hl", y - 6 + staffb);
		}
		break;
	case 6:			/* semibreve */
		if (y < -6
		    || y >= 24)
			PUT1(" %.1f hl", y + 6 + staffb);
		break;
	case 5:			/* minim */
		if (y <= -6
		    || y >= 30)
			PUT1(" %.1f hl", y + staffb);
		break;
	}

	dotx = 8.0;
	for (i = 0; i < s->dots; i++) {
		PUT1(" %.1f 3 dt", dotx);
		dotx += 3.5;
	}
	PUT0("\n");
}

/* -- draw grace notes -- */
/* (the staves are defined) */
static void draw_gracenotes(float x,
			    struct SYMBOL *s)
{
	int yy;
	float x0, y0, x1, y1, x2, y2, x3, y3, bet1, bet2, dy1, dy2;
	float a, b, staffb;
	struct SYMBOL *g, *last;
	struct BEAM bm;

	a = b = 0;			/* compiler warning */

	staffb = staff_tb[s->staff].y;	/* bottom of staff */

	/* draw the notes */
	bm.s2 = 0;
	bm.staff = -1;				/* staves defined */
	for (g = s->grace; ; g = g->next) {
		if (s->grace->next != 0) {	/* many notes */
			if ((g->sflags & S_WORD_ST)
			    && !g->as.u.note.word_end) {
				if (calculate_beam(&bm, g))
					draw_beams(&bm);
			}
		}
		draw_note(g->x, g, bm.s2 == 0);
		if (s == bm.s2)
			bm.s2 = 0;

		if (g->as.u.note.sappo)
			PUT0(" ga\n");

		if (g->next == 0) {
			last = g;
			break;
		}
	}


	/* slur */
	if (voice_tb[s->voice].key.bagpipe	/* no slur when bagpipe */
	    || !cfmt.graceslurs
	    || s->as.u.note.slur_st		/* explicit slur */
	    || s->next == 0
	    || s->next->type != NOTE)
		return;
	yy = 1000;
	for (g = s->grace; g != 0; g = g->next) {
		if (g->y < yy) {
			yy = g->y;
			last = g;
		}
	}
	x0 = last->x;
	y0 = last->y - 5;
	if (s->grace != last) {
		x0 -= 4;
		y0 += 1;
	}
	s = s->next;
	x = s->x;
	x3 = x - 1;
	if (s->stem < 0)
		x3 -= 3;
	y3 = s->ymn - 5;
	dy1 = (x3 - x0) * 0.4;
	if (dy1 > 3)
		dy1 = 3;
	dy2 = dy1;

	bet1 = 0.2;
	bet2 = 0.8;
	if (last->y > s->ymn + 7) {
		x0 = last->x - 1;
		y0 = last->y - 4.5;
		y3 = s->ymn + 1.5;
		x3 = x - 5.5;
		dy2 = (y0 - y3) * 0.2;
		dy1 = (y0 - y3) * 0.8;
		bet1 = 0;
	}

	if (y3 > y0 + 4) {
		y3 = y0 + 4;
		x0 = last->x + 2;
		y0 = last->y - 4;
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
			    struct SYMBOL *s,
			    int m)
{
	int y;
	float staffb;
	char *p;
	int head, dots, nflags;
static char *acc_tb[] = { "", "sh", "nt", "ft", "dsh", "dft" };

	staffb = staff_tb[s->staff].y;		/* bottom of staff */
	y = 3 * (s->pits[m] - 18);		/* height on staff */

	/* special case when no head */
	if (s->sflags & S_NO_HEAD) {
		if ((m == 0 && s->stem > 0)
		    || (m == s->nhd && s->stem < 0)) {
			PUT2("/x %.1f def /y %.1f def",	/* set x y */
			     x + s->shhd[m],
			     y + staffb);
			return;
		}
	}

	identify_note(s, s->as.u.note.lens[m],
		      &head, &dots, &nflags);

	/* draw the head */
	PUT2("%.1f %.1f ", x + s->shhd[m], y + staffb);
	if (s->as.u.note.grace)
		p = "ghd";
	else if (s->as.u.note.accs[m]
		 && voice_tb[s->voice].clef.type == PERC)
		p = "2 copy /y exch def /x exch def dsh0";
	else	{
		switch (head) {
		case H_SQUARE:
			if (s->as.u.note.lens[m] < BREVE * 2)
				p = "breve";
			else	p = "longa";
			break;
		case H_OVAL:
			if (s->as.u.note.lens[m] < BREVE)
				p = "HD";
			else	p = "HDD";
			break;
		case H_EMPTY:
			p = "Hd"; break;
		default:
			p = "hd"; break;
		}
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
		} else if (y <= -6) {
			yy = y;
			if (yy % 6)
				yy += 3;
		}
		if (yy)
			PUT1(" %.1f hl", yy + staffb);
	}

	/* draw the dots */
/*fixme: to see for grace notes*/
	if (dots) {
		int i;
		float dotx;
		int doty;

		dotx = 8. + s->xmx - s->shhd[m];
		if (y % 6)
			doty = 0;
		else	{
			if ((doty = s->doty) == 0)	/* defined when voices overlap */
				doty = 3;
		}
		if (s->nflags > 0 && s->stem > 0
		    && (s->sflags & S_WORD_ST)
		    && s->as.u.note.word_end
		    && doty > 0)
			dotx += DOTSHIFT;
		switch (head) {
		case H_SQUARE:
		case H_OVAL:
			dotx += 2;
			break;
		case H_EMPTY:
			dotx += 1;
			break;
		}
		for (i = 0; i < dots; i++) {
			PUT2(" %.1f %d dt", dotx, doty);
			dotx += 3.5;
		}
	}

	/* draw the accidental */
	if (s->as.u.note.accs[m]
	    && voice_tb[s->voice].clef.type != PERC) {
		if (s->as.u.note.grace)
			p = "g";
		else	p = "";
		PUT3(" %.1f %s%s",
		     - s->shac[m],
		     p, acc_tb[s->as.u.note.accs[m]]);
	}
}

/* -- draw a note or a chord -- */
/* (the staves are defined) */
static void draw_note(float x,
		      struct SYMBOL *s,
		      int fl)
{
	int	y, i, m, ma;
	float	staffb;
	char *hltype;

	staffb = staff_tb[s->staff].y;

	/* draw the master note - can be only the first or the last note */
	if (s->stem >= 0)
		ma = 0;
	else	ma = s->nhd;
	draw_basic_note(x, s, ma);		/* draw the note head */
	if (!s->as.u.note.stemless) {		/* add stem and flags */
		char	c, c2;
		float	slen;

		c = s->stem > 0 ? 'u' : 'd';
		slen = s->stem * (s->ys - s->y);
		if (!fl || s->nflags <= 0) {		/* stem only */
			c2 = s->as.u.note.grace ? 'g' : 's';
			PUT3(" %.1f %c%c", slen, c2, c);
		} else {				/* stem and flags */
			if (voice_tb[(int) s->voice].key.bagpipe
			    && cfmt.straightflags)
				c = 's';		/* straight flag */
			c2 = s->as.u.note.grace ? 'g' : 'f';
			PUT4(" %d %.1f s%c%c", s->nflags, slen, c2, c);
		}
	}

	if (s->as.u.note.grace)
		hltype = "ghl";
	else {
		switch (s->head) {
		default:
			hltype = "hl";
			break;
		case H_SQUARE:
		case H_OVAL:
			hltype = "hl1";
			break;
		}
	}
	y = s->ymn;				/* lower helper lines */
	if (y <= -6) {
		for (i = -6; i >= y; i -= 6)
			PUT2(" %.1f %s", i + staffb, hltype);
	}
	y = s->ymx;				/* upper helper lines */
	if (y >= 30) {
		for (i = 30; i <= y; i += 6)
			PUT2(" %.1f %s", i + staffb, hltype);
	}

	/* draw the other notes */
	for (m = 0; m <= s->nhd; m++) {
		if (m == ma)
			continue;
		PUT0(" ");
		draw_basic_note(x, s, m);	/* draw the note heads */
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
			if (sy->type != GRACE)
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

    } else {	/* lower voice of the staff: the bracket is below the staff */
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
	PUT3("dlw %.1f \x01%c%5.2f M lineto 0 3 RL stroke ",
	     xm - 6., '0' + upstaff, ym + s * -6. + 4.);
	PUT3("%.1f \x01%c%5.2f ", x2, '0' + upstaff, y2 + 4.);
	PUT3("%.1f \x01%c%5.2f M lineto 0 3 RL stroke ",
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
		     xx2 - dx, yy2 + dy * s, xx1 + dx, yy1 + dy * s);
		PUT3("%.1f %.1f 0 %.1f ", x1, y1 + dz * s, dz * s);
		PUT4("%.1f %.1f %.1f %.1f ", xx1, yy1, xx2, yy2);
		PUT4("%.1f %.1f %.1f %.1f SL\n", x2, y2, x1, y1);
	} else {
		PUT3("%.1f \x01%c%5.2f ",
		     xx2 - dx, '0' + staff, yy2 + dy * s);
		PUT3("%.1f \x01%c%5.2f ",
		     xx1 + dx, '0' + staff, yy1 + dy * s);
		PUT4("%.1f \x01%c%5.2f 0 %.1f ",
		      x1, '0' + staff, y1 + dz * s, dz * s);
		PUT3("%.1f \x01%c%5.2f ",
		     xx1, '0' + staff, yy1);
		PUT3("%.1f \x01%c%5.2f ",
		     xx2, '0' + staff, yy2);
		PUT3("%.1f \x01%c%5.2f ",
		     x2, '0' + staff, y2);
		PUT3("%.1f \x01%c%5.2f SL\n",
		     x1, '0' + staff, y1);
	}
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

/* -- draw a phrasing slur between two symbols -- */
/* (the staves are not yet defined) */
/* (not a pretty routine, this) */
static void draw_slur(struct SYMBOL *k1,
		      struct SYMBOL *k2)
{
	struct SYMBOL *k;
	float x1, y1, x2, y2, height, addx, addy;
	float a, y, z, h, dx, dy;
	int s, nn;
	int upstaff;
	int two_staves;

/*fixme: if two staves, may have upper or lower slur*/
	nbuf_check();
	if ((s = slur_multi(k1, k2)) == 0)
		s = slur_direction(k1, k2);

	nn = 1;
	upstaff = k1->staff;
	two_staves = 0;
	for (k = k1->next; k != 0; k = k->next) {
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
if (two_staves) fprintf(stderr, "*** multi-staves slurs not treated\n");

	/* fix endpoints */
	x1 = k1->x + k1->xmx;		/* take the max right side */
	if (k1 != k2)
		x2 = k2->x;
	else	x2 = realwidth;		/* (the slur starts on last note of the line) */
	y1 = k1->y;
	y2 = k2->y;

	if (k1->type == NOTE) {
		if (k1->stem * s > 0) {
			x1 += s * 4;
			y1 = k1->ys + s * 2;
		} else	y1 += s * 6;

		if (s > 0) {
			if (y1 < k1->dc_top + 2.5)
				y1 = k1->dc_top + 2.5;
		} else {
			if (y1 > k1->dc_bot - 2.5)
				y1 = k1->dc_bot - 2.5;
		}
	}

	if (k2->type == NOTE) {
		if (k2->stem * s > 0) {
			x2 += s * 3;
			y2 = k2->ys + s * 2;
		} else	y2 += s * 6;

		if (s > 0) {
			if (y2 < k2->dc_top + 2.5)
				y2 = k2->dc_top + 2.5;
		} else {
			if (y2 > k2->dc_bot - 2.5)
				y2 = k2->dc_bot - 2.5;
		}

		/* special case when different stem directions */
		/* (assert s > 0) */
		if (k1->type == NOTE
		    && k1->stem != k2->stem) {
			if (k1->stem > 0) {
				if (y1 > y2 - 3) {
					if ((k1->sflags & S_WORD_ST)
					    && k1->as.u.note.word_end) {
						y2 += 6;
						y1 = y2 - 3;
						if (y1 < k1->dc_top + 2.5)
							x1 += 3;
					} else	y2 = y1 + 3;
				}
			} else {
				if (y2 > y1 - 3) {
					if ((k2->sflags & S_WORD_ST)
					    && k2->as.u.note.word_end) {
						y1 += 6;
						y2 = y1 - 3;
						if (y2 < k2->dc_top + 2.5)
							x2 -= 3;
					} else	y1 = y2 + 3;
				}
			}
		}
	}

	if (k1->type != NOTE) {
		y1 = y2 + 1.2 * s;
		x1 = k1->x + k1->wr * 0.3;
		if (x1 > x2 - 12)
			x1 = x2 - 12;
	}

	if (k2->type != NOTE) {
		y2 = y1 + 1.2 * s;
		if (k1 != k2)
			x2 = k2->x - k2->wl * 0.3;
	}

	if (nn > 3) {
		if (s > 0) {
			if (y1 < k1->next->dc_top)
				y1 = k1->next->dc_top;
			if (y2 < k2->prev->dc_top)
				y2 = k2->prev->dc_top;
		} else {
			if (y1 > k1->next->dc_bot)
				y1 = k1->next->dc_bot;
			if (y2 > k2->prev->dc_bot)
				y2 = k2->prev->dc_bot;
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

	/* special case for grace notes */
	if (k1->as.u.note.grace)
		x1 = k1->x - GSTEM_XOFF * 0.5;
	if (k2->as.u.note.grace)
		x2 = k2->x + GSTEM_XOFF * 1.5;

	h = 0;
	a = (y2 - y1) / (x2 - x1);
	addy = y1 - a * x1;
	for (k = k1->next; k != 0 && k != k2 ; k = k->next) {
		if (k->staff != upstaff)
			continue;
		switch (k->type) {
		case NOTE:
		case REST:
			if (s > 0) {
				y = k->ymx + 6;
				if (y < k->ys + 2)
					y = k->ys + 2;
				y -= a * k->x + addy;
				if (y > h)
					h = y;
			} else {
				y = k->ymn - 6;
				if (y > k->ys - 2)
					y = k->ys - 2;
				y -= a * k->x + addy;
				if (y < h)
					h = y;
			}
			break;
		case GRACE: {
			struct SYMBOL *g;

			for (g = k->grace; g != 0; g = g->next) {
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
			}
			break;
		    }
		}
	}

	y1 += 0.45 * h;
	y2 += 0.45 * h;
	h *= 0.65;

	if (nn > 3)
		height = (0.08 * (x2 - x1) + 12.) * s;
	else	height = (0.03 * (x2 - x1) + 8.) * s;
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
		if (height < 0.8 * y)
			height = 0.8 * y;
	} else {
		if (height > -0.8 * y)
			height = -0.8 * y;
	}
	height *= cfmt.slurheight;

	output_slur(x1, y1, x2, y2, s,
		    height, upstaff);

	/* have room for other symbols */
	a = (y2 - y1) / (x2 - x1);
/*---fixme: it seems to work with 0.4, but why?*/
	addy = y1 - a * x1 + 0.4 * height;
	for (k = k1; ; k = k->next) {
		if (k == k2
		    && k->as.u.note.slur_st)
			break;	/* the next slur will set the top/bottom */
		if (k->staff == upstaff) {
			y = a * k->x + addy;
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
			|| (s->as.u.bar.repeat_bar
			    && s->as.text != 0
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
		    && (s->as.u.bar.type == B_RREP
			|| s->as.u.bar.type == B_DREP
			|| s->as.u.bar.type == B_THIN_THICK
			|| s->as.u.bar.type == B_THICK_THIN
			|| (s->as.u.bar.repeat_bar
			    && s->as.text != 0
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
static void draw_note_ties(struct SYMBOL *k1,
			   struct SYMBOL *k2,
			   int nslur,
			   int *mhead1,
			   int *mhead2,
			   int job)
{
	int i, s0, s1, s;
	float x1, x2, height;

	s1 = 0;
	if ((s0 = k1->multi) == 0
	    && (s0 = k2->multi) == 0)
		s1 = slur_direction(k1, k2);
	for (i = 0; i < nslur; i++) {
		int m1, m2, p1, p2, y1, y2;

		m1 = mhead1[i];
		p1 = k1->pits[m1];
		m2 = mhead2[i];
		p2 = k2->pits[m2];
		if ((s = s0) == 0) {

			/* try to have the same tie direction as the next one */
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
		x2 = k2->x + k2->shhd[m2];
		if (job == 2) {		/* half slurs from last note in line */
			p2 = p1;
			x2 -= k2->wl;
			if (k1 == k2)
				x2 = realwidth - 2;
		} else if (job == 1) {	/* half slurs to first note in line */
			p1 = p2;
/*			if (k1 == k2->prev) { */
				x1 = k1->x + k1->wr;
				if (x1 > x2 - 20)
					x1 = x2 - 20;
/*			} */
		}
		if (x2 - x1 > 20) {
			x1 += 2;
			x2 -= 2;
		}
		y1 = 3 * (p1 - 18) + 2 * s;
		y2 = 3 * (p2 - 18) + 2 * s;
		if (k1->nhd != 0)
			x1 += 4.5;
		else	y1 += ((p1 % 2) ? 3 : 2) * s;
		if (k2->nhd != 0)
			x2 -= 4.5;
		else	y2 += ((p2 % 2) ? 3 : 2) * s;
		if (s > 0) {
			if (k1->nflags > -2 && k1->stem > 0
			    && k1->nhd == 0)
				x1 += 4.5;
			if (!(p1 % 2) && k1->dots > 0)
				y1 = 3 * (p1 - 18) + 6;
		} else /*if (s < 0)*/ {
			if (k2->nflags > -2 && k2->stem < 0
			    && k2->nhd == 0)
				x2 -= 4.5;
		}

		/* tie between 2 staves */
		if (k1->staff != k2->staff) {
			s = k1->staff - k2->staff;
			y1 = 3 * (p1 - 18) + 3 * s;
			y2 = 3 * (p2 - 18) - 3 * s;
			x1 += 4;
			x2 -= 4;
			PUT4("%.1f %.1f M %.1f %.1f lineto stroke\n",
			     x1, staff_tb[k1->staff].y + y1,
			     x2, staff_tb[k2->staff].y + y2);
			continue;
		}

		height = (0.04 * (x2 - x1) + 8) * s;
		output_slur(x1, staff_tb[k1->staff].y + y1,
			    x2, staff_tb[k1->staff].y + y2,
			    s, height, -1);
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
			draw_note_ties(k1, k2,
					nslur, mhead1, mhead1, job);
		return;
	}

	if (job == 1) {			/* half slurs to first note in line */
		/* (ti2 is just used in this case) */
		for (i = 0; i <= nh1; i++) {
			if (k2->as.u.note.ti2[i]) {
				mhead1[nslur] = i;
				nslur++;
			}
			j = i + 1;
			for (m1 = 0; m1 <= nh1; m1++) {
				if (k2->as.u.note.sl2[m1] == j) {
					mhead1[nslur] = m1;
					nslur++;
					break;
				}
			}
		}
		if (nslur > 0)
			draw_note_ties(k1, k2,
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
		draw_note_ties(k1, k2,
				nslur, mhead1, mhead2, job);
}

/* -- draw all slurs/ties between neighboring notes -- */
static void draw_all_ties(struct SYMBOL *sym)
{
	struct SYMBOL *s1, *s2;

	for (s1 = sym; s1 != 0; s1 = s1->next)
		if (s1->type != CLEF && s1->type != KEYSIG
		    && s1->type != TIMESIG)
			break;
	for (s2 = s1; s2 != 0; s2 = s2->next) {
		if (s2->type == NOTE)
			break;
	}
	if (s2 == 0)
		return;
	draw_ties(s1, s2, 1);		/* 1st note */

	for (;;) {
		s1 = s2;		/* keep the last note */
		for (s2 = s2->next; s2 != 0; s2 = s2->next) {
			if (s2->type == NOTE)
				break;
			if (s2->type == BAR
			    && (s2->as.u.bar.type == B_RREP
				|| s2->as.u.bar.type == B_DREP
				|| s2->as.u.bar.type == B_THIN_THICK
				|| s2->as.u.bar.type == B_THICK_THIN
				|| (s2->as.u.bar.repeat_bar
				    && s2->as.text != 0
				    && s2->as.text[0] != '1')))
				break;
		}
		if (s2 == 0)
			break;
		draw_ties(s1, s2, s2->type == NOTE ? 0 : 2);
		if (s2->type != NOTE) {
			for (s2 = s2->next; s2 != 0; s2 = s2->next) {
				if (s2->type == NOTE)
					break;
			}
			if (s2 == 0)
				break;
		}
	}
	s2 = next_scut(s1);
	draw_ties(s1, s2, 2);
}

/* -- draw all phrasing slurs for one staff -- */
/* (the staves are not yet defined) */
static void draw_all_slurs(struct SYMBOL *sym)
{
	struct SYMBOL *s, *s1, *k;
	struct SYMBOL *cut, *gr1, *gr2;
	int pass, num, gr1_out;

	for (pass = 0; ; pass++) {
		num = 0;
		gr1 = gr2 = 0;
		s = sym;
		for (;;) {
			if (s == 0) {
				if (gr1 == 0
				    || (s = gr1->next) == 0)
					break;
				gr1 = 0;
			}
			if (s->grace != 0) {
				gr1 = s;
				s = s->grace;
				continue;
			}
			if ((s->type != NOTE
			     && s->type != REST)
			    || !s->as.u.note.slur_st) {
				s = s->next;
				continue;
			}
			k = 0;			/* find matching slur end */
			s1 = s->next;
			gr1_out = 0;
			for (;;) {
				if (s1 == 0) {
					if (gr2 != 0) {
						s1 = gr2->next;
						gr2 = 0;
						continue;
					}
					if (gr1 == 0 || gr1_out)
						break;
					s1 = gr1->next;
					gr1_out = 1;
					continue;
				}
				if (s1->grace != 0) {
					gr2 = s1;
					s1 = s1->grace;
					continue;
				}
				if (s1->type != NOTE
				    && s1->type != REST) {
					s1 = s1->next;
					continue;
				}
				if (s1->as.u.note.slur_end) {
					k = s1;
					break;
				}
				if (s1->as.u.note.slur_st)
					break;
				s1 = s1->next;
			}
			if (k == 0) {
				s = s->next;
				continue;
			}

			/* if slur in grace note sequence, change the linkages */
			if (gr1 != 0) {
				for (s1 = s; s1->next != 0; s1 = s1->next)
					;
				s1->next = gr1->next;
				gr1->next->prev = s1;
				gr1->as.u.note.slur_st = 1;
			}
			if (gr2 != 0) {
				gr2->prev->next = gr2->grace;
				gr2->grace->prev = gr2->prev;
				gr2->as.u.note.slur_st = 1;
			}

			s->as.u.note.slur_st--;
			k->as.u.note.slur_end--;
			cut = next_scut(s);
			if (cut->time <= k->time)
			    for (s1 = cut->next; s1 != 0; s1 = s1->next) {
				if (s1 == k) {
					draw_slur(s, cut);
					s = prev_scut(k);
					break;
				}
			}
			draw_slur(s, k);
			num++;

			/* if slur in grace note sequence, restore the linkages */
			if (gr1 != 0) {
				gr1->next->prev->next = 0;
				gr1->next->prev = gr1;
			}
			if (gr2 != 0) {
				gr2->prev->next = gr2;
				gr2->grace->prev = 0;
			}

			s = s->next;
		}
		if (num == 0)
			break;
	}

	/* do unbalanced slurs still left over */
	for (s = sym; s != 0; s = s->next) {
		if (s->type != NOTE
		    && s->type != REST)
			continue;
		if (s->as.u.note.slur_end) {
			cut = prev_scut(s);
			draw_slur(cut, s);
		}
		if (s->as.u.note.slur_st) {
			cut = next_scut(s);
			draw_slur(s, cut);
/*fixme: a slur may end in the row after the next one */
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

	if ((nwl = voice_tb[sym->voice].nvocal) == 0)
		return;
	y = voice_tb[sym->voice].yvocal;
	curfont = -1;				/* (force new font) */
	lskip = swfac = 0;			/* (compiler warning) */
	for (j = 0; j < nwl; j++) {
		struct SYMBOL *s;
		float x0, shift;

		hyflag = lflag = 0;
		s = sym->next;			/* keysig */
		lastx = s->x + s->wr;
		x0 = 0;				/* (compiler warning) */
		for (s = sym; s != 0; s = s->next) {
			struct lyrics *ly;
			char *p;

			if ((ly = s->ly) == 0
			    || (p = ly->w[j]) == 0) {
				switch (s->type) {
				case REST:
				case MREST:
				case MREP:
					if (lflag) {
						PUT3("%.1f %.1f %.1f wln ",
						     x0 - lastx, lastx + 3, y);
						lflag = 0;
						lastx = s->x + s->wr;
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
				     vfsize, lyric_fonts[curfont].font);
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
			if (hyflag
			    && t[0] == '\x03')		/* '_' */
				t[0] = '\x02';
			if (hyflag
			    && t[0] != '\x02') {	/* not '-' */
				vfsize = s->x - shift - lastx;
				l = (int) vfsize / 40 + 1;
				vfsize /= l;
				x0 = lastx + vfsize * 0.5 - 1.5;
				while (--l >= 0) {
					PUT2("%.1f %.1f whf ", x0, y);
					x0 += vfsize;
				}
				hyflag = 0;
				lastx = s->x + s->wr;
			}
			if (lflag
			    && t[0] != '\x03') {	/* not '_' */
				PUT3("%.1f %.1f %.1f wln ",
				     x0 - lastx + 3., lastx + 3., y);
				lflag = 0;
				lastx = s->x + s->wr;
			}

			x0 = s->x - shift;
			if (t[0] == '\x02'		/* '-' */
			    || t[0] == '\x03') {	/* '_' */
				if (t[0] == '\x02')
					hyflag = 1;
				else	lflag = 1;
				continue;
			}
			l = strlen(t) - 1;
			if (t[l] == '\x02') {		/* '-' at end */
				t[l] = '\0';
				hyflag = 1;
			}
			PUT3("%.1f %.1f M (%s) show ", x0, y, t);
			lastx = x0 + swfac * w;
		}
		if (hyflag) {
			vfsize = realwidth - 10. - lastx;
			l = (int) vfsize / 40 + 1;
			vfsize /= l;
			x0 = lastx + vfsize * 0.5 - 1.5;
			while (--l >= 0) {
				PUT2("%.1f %.1f whf ", x0, y);
				x0 += vfsize;
			}
		}
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

	/* have room for the accidentals and adjust the grace notes x offsets */
	for (s = first_voice->sym; s != 0; s = s->ts_next) {
		int nhd;
		float x, y;
		struct SYMBOL *gr;

		if (s->type != NOTE) {
			if ((gr = s->grace) == 0)
				continue;
			for (gr = s->grace; gr->next != 0; gr = gr->next)
				;
			x = s->x - gr->x;
			for (gr = s->grace; gr != 0; gr = gr->next)
				gr->x += x;
			continue;
		}
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

	draw_deco_note();

	for (p_voice = first_voice; p_voice; p_voice = p_voice->next)
		draw_all_slurs(p_voice->sym);

	draw_deco_staff();
}

/* -- draw remaining symbols when the staves are defined -- */
void draw_symbols(struct VOICE_S *p_voice)
{
	struct SYMBOL *sym;
	float x, y;
	struct BEAM bm;
	struct SYMBOL *s;

	sym = p_voice->sym;

	/* draw the symbols */
	bm.s2 = 0;
	bm.staff = -1;				/* staves defined */
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
			draw_note(x, s, bm.s2 == 0);
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
			break;
		case CLEF: {
			int	staff;
			char	ct = 't';	/* clef type - def: treble */

			if (p_voice->second)
				break;		/* only one clef per staff */
			staff = s->staff;
			memcpy(&staff_tb[staff].clef, &s->as.u.clef, /* (for next lines) */
				sizeof s->as.u.clef);
			if (s->as.u.clef.invis)
				break;
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
			case PERC:
				ct = 'p';
				break;
			default:
				bug("unknown clef type", 0);
			}
			PUT4("%.1f %.1f %c%cclef\n", x, y,
			     s->u ? 's' : ' ', ct);
			if (s->as.u.clef.octave == 0)
				break;
/*fixme: works fine for treble clef only*/
			PUT3("%.1f %.1f oct%c\n", x, y,
			     s->as.u.clef.octave > 0 ? 'u' : 'l');
			break;
		}
		case TIMESIG:
			memcpy(&p_voice->meter, &s->as.u.meter,
			       sizeof p_voice->meter);
			if (p_voice != first_voice)
				break;
			draw_timesig(x, s);
			break;
		case KEYSIG:
			draw_keysig(p_voice, x, s);
			break;
		case MREST:
			if (p_voice->second)
				break;
			PUT3("(%d) %.1f %.1f mrest\n",
			     s->as.u.bar.len,
			     x, staff_tb[s->staff].y);
			break;
		case MREP:
			if (p_voice->second)
				break;
			if (s->as.u.bar.len == 1) {
				x = (s->prev->x + s->next->x) * 0.5;
				PUT2("%.1f %.1f mrep\n",
				     x, staff_tb[s->staff].y);
				break;
			}
			PUT2("%.1f %.1f mrep2\n",
			     s->prev->x, staff_tb[s->staff].y);
			if (s->voice != first_voice - voice_tb)
				break;
			set_font(&cfmt.gchordfont);
			PUT3("%.1f %.1f M (%d) cshow\n",
			     s->prev->x, staff_tb[s->staff].y + 24 + 4,
			     s->as.u.bar.len);
			break;
		case GRACE:
			draw_gracenotes(x, s);
			break;
		case TEMPO:
		case STAVES:
		case PART:
			break;			/* nothing */
		case FMTCHG:
			if (s->u == STBRK) {
				float dx, dy;

				if (p_voice != first_voice)
					break;
				dx = s->ts_prev->x - x;
				if (s->ts_prev->type == BAR)
					dx += 1;
				else	dx += s->ts_prev->wr;
				y = staff_tb[nstaff].y - 1.;
				dy = staff_tb[0].y + 24. + 2. - y;
				PUT4("currentgray 1.0 setgray"
				     " %.1f %.1f %.1f %.1f M 2 copy"
				     " 0 exch RL 0 RL"
				     " 0 exch neg RL neg 0 RL fill"
				     " setgray\n",
				     dx, dy, x, y);
				if (nstaff != 0 && s->xmx > 0.5 * CM)
					draw_lstaff(x);
				break;
			}
			if (s->u == PSSEQ) {
				PUT1("%s\n", &s->as.text[13]);
				break;
			}
#if 1
/*fixme: should remove the other format changes */
			break;
#else
			/* fall thru */
#endif
		default:
			bug("Symbol not drawn", 1);
		}
	}

	draw_all_ties(sym);

	draw_vocals(sym);
}

/* -- work out accidentals to be applied to each note -- */
void setmap(int sf,	/* number of sharps/flats in key sig (-7 to +7) */
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

/* -- draw the tin whistle tablature -- */
void draw_whistle(void)
{
	struct VOICE_S *p_voice;
	struct SYMBOL *s;
	int i, j, pitch, w_pitch, w_octave;
	int sf;
	unsigned char workmap[70];	/* sharps/flats - base: lowest 'C' */
	unsigned char basemap[7];
	static char pitnam[12 * 2] = "C\0C#D\0EbE\0F\0F#G\0AbA\0BbB\0";
	static int w_tb[12] = {
		0x222222, 0x122222, 0x022222, 0x012222, 0x002222, 0x000222,
		0x000122, 0x000022, 0x000012, 0x000002, 0x000220, 0x000000
	};
	static int scale[7] = {0, 2, 4, 5, 7, 9, 11};	/* index = natural note */
	static int acc_pitch[6] = {0, 1, 0, -1, 2, -2};	/* index = enum accidentals */

	sf = 0;
	for (i = 0; i < nwhistle; i++) {
		p_voice = &voice_tb[whistle_tb[i].voice];
		if (p_voice != first_voice
		    && p_voice->prev == 0)
			continue;
		w_pitch = whistle_tb[i].pitch;
		PUT1("(%.2s) tw_head\n", &pitnam[(w_pitch % 12) * 2]);
		for (s = p_voice->sym; s != 0; s = s->next) {
			switch (s->type) {
			case NOTE:
				break;
			case KEYSIG:
				sf = s->as.u.key.sf;
				setmap(sf, basemap);
				for (j = 0; j < 10; j++)
					memcpy(&workmap[7 * j],
					       basemap, 7);
				continue;
			case BAR:
				if (s->as.u.bar.type == B_INVIS)
					continue;
				for (j = 0; j < 10; j++)
					memcpy(&workmap[7 * j],
					       basemap, 7);
				continue;
			default:
				continue;
			}
			if (s->as.u.note.ti2[0] != 0)	/* tied note */
				continue;
			pitch = s->as.u.note.pits[0] + 19;
			if (s->as.u.note.accs[0] != 0) {
				workmap[pitch] = s->as.u.note.accs[0] == A_NT
					? A_NULL
					: s->as.u.note.accs[0];
			}
			pitch = scale[pitch % 7]
				+ acc_pitch[workmap[pitch]]
				+ 12 * (pitch / 7);
			w_octave = 0;
			pitch -= w_pitch;
			while (pitch < 0) {
				pitch += 12;
				w_octave--;
			}
			while (pitch >= 36) {
				pitch -= 12;
				w_octave++;
			}
			PUT1("%.2f 0 ", s->x);
			if (w_octave > 0)
				PUT0("tw_over ");
			else if (w_octave < 0)
				PUT0("tw_under ");
			w_octave = pitch / 12;
			if (pitch == 12)
				pitch = 0x222220;	/* only special case (?) */
			else	pitch = w_tb[pitch % 12];
			for (j = 1; j < 7; j++) {
				PUT1("tw_%d ", pitch & 0x0f);
				pitch >>= 4;
			}
			if (w_octave == 0)
				PUT0("pop pop");
			else if (w_octave == 1)
				PUT0("tw_p");
			else	PUT0("tw_pp");
			PUT0("\n");
		}
		bskip(63.);
	}
}
