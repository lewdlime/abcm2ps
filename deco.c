/*
 * Decoration handling.
 *
 * This file is part of abcm2ps.
 *
 * Copyright (C) 2000-2006, Jean-François Moine.
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

int nbar;		/* current measure number */
int nbar_rep;		/* last repeat bar number */

int defl;		/* decoration flags */

static struct deco_elt {
	struct deco_elt *next, *prev;	/* next/previous decoration */
	struct SYMBOL *s;	/* symbol */
	struct deco_elt *start;	/* start a long decoration ending here */
	unsigned char t;	/* decoration index */
	unsigned char staff;	/* staff */
	unsigned char flags;
#define DE_VAL	0x01		/* put extra value if 1 */
#define DE_UP	0x02		/* above the staff */
#define DE_BELOW 0x08		/* below the staff */
#define DE_GRACE 0x10		/* in grace note */
#define DE_INV 0x20		/* invert the glyph */
#define DE_LDST 0x40		/* start of long decoration */
#define DE_LDEN 0x80		/* end of long decoration */
	unsigned char defl;	/* decorations flags - see DEF_xx */
	float x, y;		/* x, y */
	float v;		/* extra value */
	char *str;		/* string / 0 */
} *deco_head, *deco_tail;

typedef void draw_f(struct deco_elt *de);
static draw_f d_arp, d_cresc, d_near, d_slide, d_upstaff, d_pf, d_trill;

/* decoration table */
/* !! don't change the order of the numbered items !! */
static struct deco_def_s {
	char *name;
	unsigned char func;	/* function index */
	signed char ps_func;	/* postscript function index */
	unsigned char h;	/* height */
	unsigned char wl;	/* width */
	unsigned char wr;
	unsigned char str;	/* string index - 255=deco name */
} deco_def_tb[128] = {
	{0,		0, 0, 0},	/* 0: unknown */
	{"dot",		0, 0, 2},	/* 1 */
	{"roll",	3, 1, 10},	/* 2 */
	{"fermata",	3, 2, 12},	/* 3 */
	{"emphasis",	3, 3, 8},	/* 4 */
	{"lowermordent", 3, 4, 10},	/* 5 */
	{"coda",	3, 5, 24},	/* 6 */
	{"uppermordent", 3, 6, 10},	/* 7 */
	{"segno",	3, 7, 20},	/* 8 */
	{"trill",	3, 8, 11},	/* 9 */
	{"upbow",	3, 9, 10},	/* 10 */
	{"downbow",	3, 10, 9},	/* 11 */
	{"gmark",	3, 11, 6},	/* 12 */
	{"slide",	1, 12, 3, 7},	/* 13 */
	{"tenuto",	0, 13, 2},	/* 14 */
	{"breath",	3, 14, 0},	/* 15 */
	{"longphrase",	3, 15, 0},	/* 16 */
	{"mediumphrase", 3, 16, 0},	/* 17 */
	{"shortphrase", 3, 17, 0},	/* 18 */
	{"invertedfermata", 3, 2, 12},	/* 19 */
	{"invertedturn", 3, 18, 10},	/* 20 */
	{"invertedturnx", 3, 19, 10},	/* 21 */
};

/* c function table */
static draw_f *func_tb[] = {
	d_near,		/* 0 - near the note */
	d_slide,	/* 1 */
	d_arp,		/* 2 */
	d_upstaff,	/* 3 - tied to note */
	d_upstaff,	/* 4 (below the staff) */
	d_trill,	/* 5 */
	d_pf,		/* 6 - tied to staff (dynamic marks) */
	d_cresc,	/* 7 */
};

/* postscript function table */
static char *ps_func_tb[128] = {
	"stc",		/* 0: dot */
	"cpu",		/* 1: roll */
	"hld",		/* 2: fermata */
	"accent",	/* 3: accent */
	"lmrd",		/* 4: lowermordent */
	"coda",		/* 5: coda */
	"umrd",		/* 6: uppermordent */
	"sgno",		/* 7: segno */
	"trl",		/* 8: trill */
	"upb",		/* 9: upbow */
	"dnb",		/* 10: downbow */
	"grm",		/* 11: gracing mark */
	"sld",		/* 12: slide */
	"emb",		/* 13: tenuto */
	"brth",		/* 14: breath */
	"lphr",		/* 15: longphrase */
	"mphr",		/* 16: mediumphrase */
	"sphr",		/* 17: shortphrase */
	"turn",		/* 18: turn */
	"turnx",	/* 19: turn with bar */
};

static char *str_tb[32];

static char *std_deco_tb[] = {
	"0 3 fng 8 0 0 0",
	"1 3 fng 8 0 0 1",
	"2 3 fng 8 0 0 2",
	"3 3 fng 8 0 0 3",
	"4 3 fng 8 0 0 4",
	"5 3 fng 8 0 0 5",
	"plus 3 dplus 7 0 0",
	"+ 3 dplus 7 0 0",
	"accent 3 accent 8 0 0",
	"> 3 accent 8 0 0",
	"D.C. 3 dacs 16 0 0 D.C.",
	"D.S. 3 dacs 16 0 0 D.S.",
	"fine 3 dacs 16 0 0 FINE",
	"f 6 pf 18 4 4",
	"ff 6 pf 18 5 11",
	"fff 6 pf 18 7 17",
	"ffff 6 pf 18 8 24",
	"mf 6 pf 18 5 11",
	"mp 6 pf 18 5 11",
	"mordent 3 lmrd 10 0 0",
	"open 3 opend 10 0 0",
	"p 6 pf 18 4 4",
	"pp 6 pf 18 5 11",
	"ppp 6 pf 18 7 17",
	"pppp 6 pf 18 8 24",
	"pralltriller 3 umrd 10 0 0",
	"sfz 6 sfz 18 7 17",
	"turn 3 turn 10 0 0",
	"wedge 3 wedge 8 0 0",
	"turnx 3 turnx 10 0 0",
	"trill( 5 - 8 0 0",
	"trill) 5 ltr 8 0 0",
	"snap 3 snap 14 0 0",
	"thumb 3 thumb 14 0 0",
	"arpeggio 2 arp 12 10 0",
	"crescendo( 7 - 18 0 0",
	"crescendo) 7 cresc 18 0 0",
	"<( 7 - 18 0 0",
	"<) 7 cresc 18 0 0",
	"diminuendo( 7 - 18 0 0",
	"diminuendo) 7 dim 18 0 0",
	">( 7 - 18 0 0",
	">) 7 dim 18 0 0",
	"invisible 32 0 0 0 0",
	"beamon 33 0 0 0 0",
	"trem1 34 0 0 0 0",
	"trem2 34 0 0 0 0",
	"trem3 34 0 0 0 0",
	"trem4 34 0 0 0 0",
	"xstem 35 0 0 0 0",
	"beambr1 36 0 0 0 0",
	"beambr2 36 0 0 0 0",
	"rbstop 37 0 0 0 0",
	0
};

static struct SYMBOL *first_note;	/* first note/rest of the line */

static void draw_gchord(struct SYMBOL *s, float gchy);

/* -- get the max/min vertical offset -- */
float y_get(struct SYMBOL *s,
	    int up,
	    float x,
	    float w,
	    float h)
{
#ifdef YSTEP
	struct STAFF_S *p_staff;
	int i, j;
	float y;

	p_staff = &staff_tb[s->staff];
	i = (int) (x / realwidth * YSTEP);
if (i < 0) {
fprintf(stderr, "y_get i:%d\n", i);
i = 0;
}
	j = (int) ((x + w) / realwidth * YSTEP);
	if (j >= YSTEP) {
/*fprintf(stderr, "y_get i:%d", j);*/
		j = YSTEP - 1;
		if (i > j)
			i = j;
	}
	if (up) {
		y = p_staff->top[i++];
		while (i <= j) {
			if (y < p_staff->top[i])
				y = p_staff->top[i];
			i++;
		}
	} else {
		y = p_staff->bot[i++];
		while (i <= j) {
			if (y > p_staff->bot[i])
				y = p_staff->bot[i];
			i++;
		}
		y -= h;
	}
#else
	struct SYMBOL *s2, *s_start;
	int staff, start_seen;
	float y;

	s_start = s;
	staff = s->staff;
	if (s->x < x || s->ts_prev == 0)
		start_seen = 1;
	else {
		while (s->ts_prev != 0 && s->ts_prev->x >= x)
			s = s->ts_prev;
		while (s->staff != staff) {
			if (s->ts_prev == 0) {
				while (s->staff != staff)
					s = s->ts_next;
				break;
			}
			s = s->ts_prev;
		}
		start_seen = s == s_start;
	}
	y = up ? s->dc_top : s->dc_bot;

	if (x > s->x + 5) {
/*fixme:KO if no decoration*/
		if (up) {
			y -= h;
			if (y < s->ymx && x < s->x + 10)
				y = s->ymx;
		} else {
			y += h;
			if (y > s->ymn && x < s->x + 10)
				y = s->ymn;
		}
		s = s->ts_next;
	}
	x += w;		/* right offset */
	if (up) {
		for (s2 = s; s2 != 0; s2 = s2->ts_next) {
			if (start_seen) {
				if (s2->x > x)
					break;
			} else if (s2 == s_start)
				start_seen = 1;
			if (s2->staff == staff) {
				if (y < s2->dc_top)
					y = s2->dc_top;
			}
		}
	} else {
		for (s2 = s; s2 != 0; s2 = s2->ts_next) {
			if (start_seen) {
				if (s2->x > x)
					break;
			} else if (s2 == s_start)
				start_seen = 1;
			if (s2->staff == staff) {
				if (y > s2->dc_bot)
					y = s2->dc_bot;
			}
		}
		y -= h;
	}
#endif
	return y;
}

/* -- adjust the vertical offsets -- */
void y_set(struct SYMBOL *s,
	   int up,
	   float x,
	   float w,
	   float y)
{
#ifdef YSTEP
	struct STAFF_S *p_staff;
	int i, j;

	p_staff = &staff_tb[s->staff];
	i = (int) (x / realwidth * YSTEP);
if (i < 0) {
fprintf(stderr, "y_set i:%d\n", i);
i = 0;
}
	j = (int) ((x + w) / realwidth * YSTEP);
	if (j >= YSTEP) {
/*fprintf(stderr, "y_get i:%d", j);*/
		j = YSTEP - 1;
		if (i > j)
			i = j;
	}
	if (up) {
		while (i <= j) {
			if (p_staff->top[i] < y)
				p_staff->top[i] = y;
			i++;
		}
	} else {
		while (i <= j) {
			if (p_staff->bot[i] > y)
				p_staff->bot[i] = y;
			i++;
		}
	}
#else
	struct SYMBOL *s_start;
	int staff, start_seen;

	s_start = s;
	staff = s->staff;
	if (x > s->x + 5) {
		s = s->ts_next;
		start_seen = 1;
	} else {
		while (s->ts_prev != 0 && s->ts_prev->x >= x)
			s = s->ts_prev;
		start_seen = 0;
	}
	x += w;		/* right offset */
	if (up) {
		for (; s != 0; s = s->ts_next) {
			if (start_seen) {
				if (s->x > x)
					break;
			} else if (s == s_start)
				start_seen = 1;
			if (s->staff == staff
			    && s->dc_top < y)
				s->dc_top = y;
		}
	} else {
		for (; s != 0; s = s->ts_next) {
			if (start_seen) {
				if (s->x > x)
					break;
			} else if (s == s_start)
				start_seen = 1;
			if (s->staff == staff
			    && s->dc_bot > y)
				s->dc_bot = y;
		}
	}
#endif
}

/* -- get the staff position of the dynamic marks -- */
static int dyn_p(struct SYMBOL *s)
{
	if (s->sflags & S_DYNUP)
		return 1;
	if (s->sflags & S_DYNDN)
		return 0;
	if (s->multi != 0)
		return s->multi > 0 ? 1 : 0;
	if (cfmt.exprabove
	    || (!cfmt.exprbelow
/*fixme: only if the lyrics are below the staff*/
		&& voice_tb[s->voice].have_ly))
		return 1;
	return 0;
}

/* -- drawing functions -- */
/* special case for arpeggio */
static void d_arp(struct deco_elt *de)
{
	struct SYMBOL *s;
	struct deco_def_s *dd;
	int m, h;
	float xc, dx;

	s = de->s;
	dd = &deco_def_tb[de->t];
	xc = 10;
	for (m = 0; m <= s->nhd; m++) {
		if (s->as.u.note.accs[m])
			dx = 8 + s->shac[m];
		else {
			dx = 10 - s->shhd[m];
			switch (s->head) {
			case H_SQUARE:
			case H_OVAL:
				dx += 2.5;
				break;
			}
		}
		if (dx > xc)
			xc = dx;
	}
	h = 3 * (s->pits[s->nhd] - s->pits[0]) + 4;
	m = dd->h;		/* minimum height */
	if (h < m)
		h = m;

	de->flags |= DE_VAL;
	de->v = h;
	de->x = s->x - xc;
	de->y = (float) (3 * (s->pits[0] - 18)) - 3;
}

/* special case for crescendo/diminuendo */
static void d_cresc(struct deco_elt *de)
{
	struct SYMBOL *s, *s2;
	struct deco_def_s *dd, *dd2;
	struct deco_elt *de1;
	int up;
	float x, dx, x2;

	if (de->flags & DE_LDST)
		return;
	s2 = de->s;
	dd = &deco_def_tb[de->t];
	de1 = de->start;		/* start of the deco */
	if (de1 != 0) {
		s = de1->s;
		x = s->x + 3;
	} else {			/* end without start */
		s = first_note;
		x = s->x - s->wl - 4;
	}
	de->staff = s2->staff;
	de->flags &= ~DE_LDEN;		/* old behaviour */
	de->flags |= DE_VAL;
	up = dyn_p(s2);
	if (up)
		de->flags |= DE_UP;

	/* shift the starting point if any dynamic mark on the left */
	if (de1 != 0 && de1->prev != 0 && de1->prev->s == s
	    && ((de->flags ^ de1->prev->flags) & DE_UP) == 0) {
		dd2 = &deco_def_tb[de1->prev->t];
		if (dd2->func >= 6) {
			x2 = de1->prev->x + de1->prev->v + 4;
			if (x2 > x)
				x = x2;
		}
	}

	if (de->defl & DEF_NOEN) {	/* if no decoration end */
		dx = de->x - x;
		if (dx < 20) {
			x = de->x - 20 - 3;
			dx = 20;
		}
	} else {
		x2 = s2->x;
		if (de->next != 0 && de->next->s == s
		    && ((de->flags ^ de->next->flags) & DE_UP) == 0) {
			dd2 = &deco_def_tb[de->next->t];
			if (dd2->func >= 6)	/* if dynamic mark */
			x2 -= 5;
		}
		dx = x2 - x - 4;
		if (dx < 20) {
			x -= (20 - dx) * 0.5;
			if (de->start == 0)
				x -= (20 - dx) * 0.5;
			dx = 20;
		}
	}

	de->v = dx;
	de->x = x;
	de->y = y_get(s2, up, x, dx, dd->h);
	/* (y_set is done later in draw_deco_staff) */
}

/* near the note (dot, tenuto) */
static void d_near(struct deco_elt *de)
{
	struct SYMBOL *s;
	struct deco_def_s *dd;
	int y, up;

	s = de->s;
	dd = &deco_def_tb[de->t];
	up = s->stem > 0 ? 0 : 1;
	if (s->multi)
		up = !up;
	if (up)
		y = s->ymx + 3;
	else	y = s->ymn - 3;
	if (y > -3 && y < 24 + 3) {
		if (up)
			y++;
		y = (y + 5) / 6 * 6 - 3;	/* between lines */
	}
	if (s->ymx < y + dd->h)
		s->ymx = y + dd->h;
	if (s->ymn > y - dd->h)
		s->ymn = y - dd->h;

	de->x = s->x + s->shhd[s->stem >= 0 ? 0 : s->nhd];
	de->y = (float) y;
}

/* special case for piano/forte indications */
static void d_pf(struct deco_elt *de)
{
	struct SYMBOL *s;
	struct deco_def_s *dd, *dd2;
	float x, x2;
	char *str;
	int up;

	s = de->s;
	dd = &deco_def_tb[de->t];

	up = dyn_p(s);
	if (up)
		de->flags |= DE_UP;

	x = s->x - dd->wl;
	if (de->prev != 0 && de->prev->s == s
	    && ((de->flags ^ de->prev->flags) & DE_UP) == 0) {
		dd2 = &deco_def_tb[de->prev->t];
		if (dd2->func >= 6) {	/* if dynamic mark */
			x2 = de->prev->x + de->prev->v + 4;
			if (x2 > x)
				x = x2;
		}
	}

	str = dd->name;
	if (dd->str != 0 && dd->str != 255)
		str = str_tb[dd->str];

	de->v = dd->wl + dd->wr;
	de->x = x;
	de->y = y_get(s, up, s->x, de->v, dd->h);
	de->str = str;
	/* (y_set is done later in draw_deco_staff) */
}

/* special case for slide */
static void d_slide(struct deco_elt *de)
{
	struct SYMBOL *s;
	struct deco_def_s *dd;
	int m, yc;
	float xc, dx;

	s = de->s;
	dd = &deco_def_tb[de->t];
	yc = s->pits[0];
	xc = 5;
	for (m = 0; m <= s->nhd; m++) {
		if (s->as.u.note.accs[m])
			dx = 4 + s->shac[m];
		else {
			dx = 5 - s->shhd[m];
			switch (s->head) {
			case H_SQUARE:
			case H_OVAL:
				dx += 2.5;
				break;
			}
		}
		if (s->pits[m] <= yc + 3 && dx > xc)
			xc = dx;
	}
	de->x = s->x - xc;
	de->y = (float) (3 * (yc - 18));
}

/* special case for long trill */
static void d_trill(struct deco_elt *de)
{
	struct SYMBOL *s;
	struct deco_def_s *dd;
	int staff, up;
	float x, y, dx;
	struct SYMBOL *s2;

	if (de->flags & DE_LDST)
		return;
	s2 = de->s;
	dd = &deco_def_tb[de->t];

	if (de->start != 0) {		/* deco start */
		s = de->start->s;
		x = s->x;
		if (s->type == NOTE
		    && s->as.u.note.dc.n > 1)
			x += 10;
	} else {			/* end without start */
		s = first_note;
		x = s->x - s->wl - 4;
	}
	de->staff = staff = s2->staff;

	up = s2->multi >= 0;
	if (de->defl & DEF_NOEN) {	/* if no decoration end */
		dx = de->x - x;
		if (dx < 20) {
			x = de->x - 20 - 3;
			dx = 20;
		}
	} else {
		dx = s2->x - x - 6;
		if (s2->type == NOTE)
			dx -= 6;
		if (dx < 20) {
			x -= (20 - dx) * 0.5;
			if (de->start == 0)
				x -= (20 - dx) * 0.5;
			dx = 20;
		}
	}

	y = y_get(s2, up, x, dx, dd->h);
	de->flags &= ~DE_LDEN;
	de->flags |= DE_VAL;
	de->v = dx;
	de->x = x;
	de->y = y;
	if (up)
		y += dd->h;
	y_set(s2, up, x, dx, y);
}

/* above (or below) the staff */
static void d_upstaff(struct deco_elt *de)
{
	struct SYMBOL *s;
	struct deco_def_s *dd;
	float x, yc;
	int inv;

	s = de->s;
	dd = &deco_def_tb[de->t];
	inv = 0;
	x = s->x + s->shhd[s->stem >= 0 ? 0 : s->nhd];
	if (dd->str != 0)
		de->str = dd->str == 255 ? dd->name : str_tb[dd->str];
	switch (de->t) {
	case 2:		/* roll */
		if (s->multi < 0
		    || (s->multi == 0 && s->stem > 0)) {
#ifdef YSTEP
			yc = y_get(s, 0, s->x, 0, 0);
#else
			yc = s->dc_bot;
#endif
			if (yc > -2)
				yc = -2;
			yc -= dd->h;
#ifdef YSTEP
			y_set(s, 0, s->x, 0, yc);
#else
			s->dc_bot = yc;
#endif
			inv = 1;
		} else {
#ifdef YSTEP
			yc = y_get(s, 1, s->x, 0, 0) + 3;
#else
			yc = s->dc_top + 3;
#endif
			if (yc < 24 + 2)
				yc = 24 + 2;
			if (s->stem <= 0
			    && (s->dots == 0 || ((int) s->y % 6)))
				yc -= 2;
#ifdef YSTEP
			y_set(s, 1, s->x, 0, yc + dd->h);
#else
			s->dc_top = yc + dd->h;
#endif
		}
		break;
	case 15:	/* breath */
	case 16:	/* longphrase */
	case 17:	/* mediumphrase */
	case 18: {	/* shortphrase */
		int stime, seq;

		yc = 24 + 3;
		stime = s->time;
		seq = s->seq;
		for (s = s->ts_next; s != 0; s = s->ts_next)
			if (s->time != stime || s->seq != seq)
				break;
		if (s != 0)
			x += (s->x - x) * 0.4;
		else	x += 10;
		break;
	    }
	case 20:	/* invertedturn */
	case 21:	/* invertedturnx */
		inv = 1;
		/* fall thru */
	default:
		if (s->multi >= 0
		    && de->t != 19	/* invertedfermata */
		    && !(de->flags & DE_BELOW)) {
#ifdef YSTEP
			yc = y_get(s, 1, s->x, 0, 0);
#else
			yc = s->dc_top;
#endif
			if (yc < 24 + 2)
				yc = 24 + 2;
#ifdef YSTEP
			y_set(s, 1, s->x, 0, yc + dd->h);
#else
			s->dc_top = yc + dd->h;
#endif
		} else {
#ifdef YSTEP
			yc = y_get(s, 0, s->x, 0, 0);
#else
			yc = s->dc_bot;
#endif
			if (yc > -2)
				yc = -2;
			yc -= dd->h;
#ifdef YSTEP
			y_set(s, 0, s->x, 0, yc);
#else
			s->dc_bot = yc;
#endif
			switch (de->t) {
			case 3:		/* fermata */
			case 19:	/* invertedfermata */
				inv = 1;
				break;
			}
		}
		break;
	}
	if (inv) {
		yc += dd->h;
		de->flags |= DE_INV;
	}
	de->x = x;
	de->y = yc;
}

/* -- add a decoration - from %%deco -- */
/* syntax:
 *	%%deco <name> <c_func> <ps_func> <h> <wl> <wr> [<str>]
 */
void deco_add(char *text)
{
	struct deco_def_s *dd;
	int c_func, deco, h, wl, wr, n, ps_x, str_x;
	char name[16];
	char ps_func[16];

	/* extract the arguments */
	if (sscanf(text, "%15s %d %15s %d %d %d%n",
		   name, &c_func, ps_func, &h, &wl, &wr, &n) != 6) {
		error(1, 0, "Invalid deco %s", text);
		return;
	}
	if ((c_func < 0 || c_func >= sizeof func_tb / sizeof func_tb[0])
	    && (c_func < 32 || c_func > 37)) {
		error(1, 0, "%%%%deco: bad C function index (%s)", text);
		return;
	}
	if (h < 0 || wl < 0 || wr < 0) {
		error(1, 0, "%%%%deco: cannot have a negative value (%s)", text);
		return;
	}
	if (h > 50 || wl > 80 || wr > 80) {
		error(1, 0, "%%%%deco: abnormal h/wl/wr value (%s)", text);
		return;
	}
	text += n;
	while (isspace((unsigned char) *text))
		text++;

	/* search the decoration */
	for (deco = 1, dd = &deco_def_tb[1]; deco < 128; deco++, dd++) {
		if (dd->name == 0
		    || strcmp(dd->name, name) == 0)
			break;
	}
	if (deco == 128) {
		error(1, 0, "Too many decorations");
		return;
	}

	/* search the postscript function */
	for (ps_x = 0; ps_x < sizeof ps_func_tb / sizeof ps_func_tb[0]; ps_x++) {
		if (ps_func_tb[ps_x] == 0
		    || strcmp(ps_func_tb[ps_x], ps_func) == 0)
			break;
	}
	if (ps_x == sizeof ps_func_tb / sizeof ps_func_tb[0]) {
		error(1, 0, "Too many postscript functions");
		return;
	}

	/* have an index for the string */
	if (*text == '\0')
		str_x = 0;
	else if (strcmp(text, name) == 0)
		str_x = 255;
	else {
		for (str_x = 1;
		     str_x < sizeof str_tb / sizeof str_tb[0];
		     str_x++) {
			if (str_tb[str_x] == 0) {
				str_tb[str_x] = strdup(text);
				break;
			}
			if (strcmp(str_tb[str_x], text) == 0)
				break;
		}
		if (str_x == sizeof str_tb / sizeof str_tb[0]) {
			error(1, 0, "Too many decoration strings");
			return;
		}
	}

	/* set the values */
	if (dd->name == 0)
		dd->name = strdup(name);	/* new decoration */
	dd->func = c_func;
	if (ps_func_tb[ps_x] == 0) {
		if (ps_func[0] == '-' && ps_func[1] == '\0')
			ps_x = -1;
		else	ps_func_tb[ps_x] = strdup(ps_func);
	}
	dd->ps_func = ps_x;
	dd->h = h;
	dd->wl = wl;
	dd->wr = wr;
	dd->str = str_x;
}

/* -- convert the decorations -- */
void deco_cnv(struct deco *dc,
	      struct SYMBOL *s)
{
	int i, j;
	struct deco_def_s *dd;
	unsigned char deco;
	struct VOICE_S *p_voice;

	for (i = dc->n; --i >= 0; ) {
		if ((deco = dc->t[i]) == 0)
			continue;
		if (deco < 128) {
			deco = deco_tune[deco];
			if (deco == 0)
				error(1, s,
				      "Notation '%c' not treated", dc->t[i]);
		} else	deco = deco_intern(deco);
		dc->t[i] = deco;
		if (deco == 0)
			continue;

		/* special decorations */
		dd = &deco_def_tb[deco];
		switch (dd->func) {
		default:
			continue;
		case 32:		/* 32 = invisible */
			if (s->type != NOTE)
				error(1, s,
				      "Cannot have +%s+ on a rest or a bar",
				       dd->name);
			else	s->as.u.note.invis = 1;
			break;
		case 33:		/* 33 = beamon */
			s->sflags |= S_BEAM_ON;
			break;
		case 34:		/* 34 = trem1..trem4 */
			if (s->type != NOTE || s->prev->type != NOTE) {
				error(1, s,
				      "+%s+ must be on the last of a couple of notes",
				       dd->name);
				break;
			}
			s->sflags |= S_TREM;
			s->sflags &= ~S_WORD_ST;
			s->as.u.note.word_end = 1;
			s->prev->sflags |= (S_TREM | S_WORD_ST);
			s->prev->as.u.note.word_end = 0;
			s->nflags = s->prev->nflags = dd->name[4] - '0';
			for (j = 0; j <= s->nhd; j++)
				s->as.u.note.lens[j] *= 2;
			for (j = 0; j <= s->prev->nhd; j++)
				s->prev->as.u.note.lens[j] *= 2;
			break;
		case 35:		/* 35 = xstem */
			if (s->type != NOTE) {
				error(1, s,
				      "Cannot have +%s+ on a rest or a bar",
				       dd->name);
				break;
			}
			s->sflags |= S_XSTEM;
			break;
		case 36:		/* 36 = beambr1 / beambr2 */
			if (s->type != NOTE) {
				error(1, s,
				      "Cannot have +%s+ on a rest or a bar",
				       dd->name);
				break;
			}
			s->sflags |= dd->name[6] == '1' ?
				S_BEAM_BR1 : S_BEAM_BR2;
			break;
		case 37:		/* 37 = rbstop */
			s->sflags |= S_RBSTOP;
			break;
		}
		dc->t[i] = 0;
	}
	p_voice = &voice_tb[s->voice];
	if (p_voice->dyn != 0)
		s->sflags |= p_voice->dyn > 0 ? S_DYNUP : S_DYNDN;
}

/* -- define a standard decoration -- */
static unsigned char deco_define(char *name)
{
	unsigned char deco;
	int l;

	l = strlen(name);
	for (deco = 0; ; deco++) {
		if (std_deco_tb[deco] == 0)
			return 128;
		if (strncmp(std_deco_tb[deco], name, l) == 0
		    && std_deco_tb[deco][l] == ' ') {
			deco_add(std_deco_tb[deco]);
			switch (name[l - 1]) {
			case '(': deco_add(std_deco_tb[deco + 1]); break;
			case ')': deco_add(std_deco_tb[deco - 1]); break;
			}
			for (deco = 1; deco < 128; deco++) {
				if (deco_def_tb[deco].name == 0) {
					deco = 128;
					break;
				}
				if (strcmp(deco_def_tb[deco].name, name) == 0)
					break;
			}
			break;
		}
	}
	return deco;
}

/* -- convert the external deco number to the internal one -- */
unsigned char deco_intern(unsigned char deco)
{
	char *name;

	if (deco == 0)
		return deco;
	name = deco_tb[deco - 128];
	for (deco = 1; deco < 128; deco++) {
		if (deco_def_tb[deco].name == 0) {
			deco = deco_define(name);	/* try a standard decoration */
			break;
		}
		if (strcmp(deco_def_tb[deco].name, name) == 0)
			break;
	}
	if (deco == 128) {
		error(1, 0, "Decoration %s not treated", name);
		deco = 0;
	}
	return deco;
}

/* -- update the x position of a decoration -- */
void deco_update(struct SYMBOL *s, float dx)
{
	struct deco_elt *de;

	for (de = deco_head; de != 0; de = de->next) {
		if (de->s == s) {
			while (de != 0 && de->s == s) {
				de->x += dx;
				de = de->next;
			}
			break;
		}
	}
}

/* -- adjust the symbol width -- */
float deco_width(struct SYMBOL *s)
{
	struct deco *dc;
	int i;
	float wl;

	wl = 0;
	if (s->type == BAR)
		dc = &s->as.u.bar.dc;
	else	dc = &s->as.u.note.dc;
	for (i = dc->n; --i >= 0; ) {
		struct deco_def_s *dd;

		dd =  &deco_def_tb[dc->t[i]];
		switch (dd->func) {
		case 1:			/* slide */
			if (wl < 7)
				wl = 7;
			break;
		case 2:			/* arpeggio */
			if (wl < 10)
				wl = 10;
			break;
		}
	}
	return wl;
}

/* -- draw the decorations -- */
/* (the staves are defined) */
void draw_all_deco(void)
{
	struct deco_elt *de;
	struct deco_def_s *dd;
	int f, staff;
	float x, y, y2, ym;
	float ymid[MAXSTAFF];

	if (!cfmt.dynalign) {
		staff = nstaff;
		y = staff_tb[staff].y;
		while (--staff >= 0) {
			y2 = staff_tb[staff].y;
			ymid[staff] = (y + 24 + y2) * 0.5;
			y = y2;
		}
	}

	for (de = deco_head; de != 0; de = de->next) {
		dd = &deco_def_tb[de->t];
		if ((f = dd->ps_func) < 0)
			continue;
		staff = de->staff;
		y = de->y + staff_tb[staff].y;

		/* center the dynamic marks between two staves */
/*fixme: KO when deco on other voice and same direction*/
		if (dd->func >= 6 && !cfmt.dynalign
		    && (((de->flags & DE_UP) && staff > 0)
			|| (!(de->flags & DE_UP) && staff < nstaff))) {
			if (de->flags & DE_UP)
				ym = ymid[--staff];
			else	ym = ymid[staff++];
			ym -= dd->h * 0.5;
			if (((de->flags & DE_UP) && y < ym)
			    || (!(de->flags & DE_UP) && y > ym)) {
				struct SYMBOL *s;

				s = de->s;
				while (s->staff != staff)
					s = s->ts_prev;
				y2 = y_get(s, !(de->flags & DE_UP),
					   de->x, de->v, dd->h)
					+ staff_tb[staff].y;
				if (((de->flags & DE_UP) && y2 > ym)
				    || (!(de->flags & DE_UP) && y2 < ym)) {
					y = ym;
					y_set(de->s, de->flags & DE_UP,
					      de->x, de->v,
					      ((de->flags & DE_UP) ? y + dd->h : y)
						- staff_tb[de->staff].y);
				}
			}
		}

		set_scale(de->s->voice);
		set_defl(de->defl);
/*fixme: scaled or not?*/
		if (de->flags & DE_VAL)
			putf(de->v);
		if (de->str)
			PUT1("(%s)", de->str);
		putxy(de->x, y);
		if (de->flags & DE_LDEN) {
			if (de->start != 0) {
				x = de->start->x;
				y = de->start->y + staff_tb[de->start->staff].y;
			} else	x = first_note->x - first_note->wl - 4;
			if (x > de->x - 20)
				x = de->x - 20;
			putxy(x, y);
		}
		if (de->flags & DE_GRACE) {
			if (de->flags & DE_INV)
				PUT1("gsave T 0.7 -0.7 scale 0 0 %s grestore\n", ps_func_tb[f]);
			else	PUT1("gsave T 0.7 dup scale 0 0 %s grestore\n", ps_func_tb[f]);
		} else {
			if (de->flags & DE_INV)
				PUT1("gsave 1 -1 scale neg %s grestore\n", ps_func_tb[f]);
			else	PUT1("%s\n", ps_func_tb[f]);
		}
	}
	set_scale(-1);			/* restore the scale */
}

/* -- draw a decoration relative to a note head -- */
/* return 1 if the decoration is a head */
int draw_deco_head(int deco, float x, float y, int stem)
{
	struct deco_def_s *dd;
	char *str;

	if (deco == 0)
		return 0;
	dd = &deco_def_tb[deco];
	if (dd->ps_func < 0)
		return 0;
	if (cfmt.setdefl) {
		int fl;

		fl = stem >= 0 ? DEF_STEMUP : 0;
		if (defl != fl) {
			defl = fl;
			PUT1("/defl %d def ", fl);
		}
	}
	switch (dd->func) {
	case 2:
	case 5:
	case 7:
		PUT0("0 ");
		break;
	case 3:
	case 4:
		if (dd->str == 0)
			break;
		/* fall thru */
	case 6:
		str = dd->name;
		if (dd->str != 0 && dd->str != 255)
			str = str_tb[dd->str];
		PUT1("(%s)", str);
		break;
	}
	putxy(x, y);
	PUT1("%s ", ps_func_tb[dd->ps_func]);
	return strncmp(dd->name, "head-", 5) == 0;
}

/* -- draw the chord decorations relative to the heads -- */
void draw_all_deco_head(struct SYMBOL *s, float x, float y)
{
	int k;
	unsigned char deco;
	struct deco *dc;
	struct deco_def_s *dd;

	dc = &s->as.u.note.dc;
	for (k = dc->n; --k >= 0; ) {
		if (k >= dc->h && k < dc->s)	/* skip the head decorations */
			continue;
		if ((deco = dc->t[k]) == 0)
			continue;
		dd = &deco_def_tb[deco];

		if (strncmp(dd->name, "head-", 5) != 0)
			continue;
		draw_deco_head(deco, x, y, s->stem);
	}
}

/* -- create the deco elements, and treat the near ones -- */
static void deco_create(struct SYMBOL *s,
			struct deco *dc)
{
	int k, l;
	unsigned char deco;
	struct deco_def_s *dd;
	struct deco_elt *de;
#if 1
/*fixme:pb with decorations above the staff*/
	for (k = 0; k < dc->n; k++) {
		if (k >= dc->h && k < dc->s)	/* skip the head decorations */
			continue;
		if ((deco = dc->t[k]) == 0)
			continue;
		dd = &deco_def_tb[deco];
#else
	int i, j;
	struct deco_def_s *d_tb[MAXDC];

	/* the decorations above the staff must be treated in reverse order */ 
	memset(&d_tb, 0, sizeof d_tb);
	i = 0;
	j = dc->n;
	for (k = 0; k < dc->n; k++) {
		if (k >= dc->h && k < dc->s)	/* skip the head decorations */
			continue;
		if ((deco = dc->t[k]) == 0)
			continue;
		dd = &deco_def_tb[deco];
		if (dd->func < 3) {		/* if near the note */
			if (s->multi > 0
			    || (s->multi == 0 && s->stem < 0)) {
				d_tb[--j] = dd;
				continue;
			}
		} else if (dd->func == 3	/* if tied to note (not below) */
			   || dd->func == 5) {
			if (s->multi >= 0) {
				d_tb[--j] = dd;
				continue;
			}
		}
		d_tb[i++] = dd;
	}

	for (k = 0; k < dc->n; k++) {
		if ((dd = d_tb[k]) == 0)
			continue;
#endif

		/* memorize the decorations, but not the head ones */
		if (strncmp(dd->name, "head-", 5) == 0) {
			switch (s->type) {
			case NOTE:
			case REST:
				s->sflags |= S_OTHER_HEAD;
				break;
			default:
				error(1, s, "Cannot have +%s+ on a bar",
				      dd->name);
				break;
			}
			continue;
		}
		de = (struct deco_elt *) getarena(sizeof *de);
		memset(de, 0, sizeof *de);
		if ((de->prev = deco_tail) == 0)
			deco_head = de;
		else	deco_tail->next = de;
		deco_tail = de;
		de->s = s;
		de->t = dd - deco_def_tb;
		de->staff = s->staff;
		if (s->type == NOTE && s->as.u.note.grace)
			de->flags = DE_GRACE;
		l = strlen(dd->name) - 1;
		if (l > 0) {
			switch (dd->name[l]) {
			case '(':
				de->flags |= DE_LDST;
				break;
			case ')':
				if (strchr(dd->name, '(') != 0)
					break;
				de->flags |= DE_LDEN;
				de->defl = DEF_NOST;
				break;
			}
		}
		if (cfmt.setdefl && s->stem >= 0)
			de->defl |= DEF_STEMUP;

		if (dd->func >= 3)	/* if not near the note */
			continue;
		if (s->type != NOTE) {
			error(1, s,
			      "Cannot have +%s+ on a rest or a bar",
			       dd->name);
			continue;
		}
		func_tb[dd->func](de);
	}
}

/* -- create the decorations and treat the ones near the notes -- */
/* (the staves are not yet defined) */
/* this function must be called first as it builds the deco element table */
void draw_deco_near(void)
{
	struct SYMBOL *s, *g;
	struct deco *dc;
	struct SYMBOL *first;

	deco_head = deco_tail = 0;
	first = 0;
	for (s = first_voice->sym; s != 0; s = s->ts_next) {
		switch (s->type) {
		case BAR:
		case MREST:
			if (s->as.u.bar.dc.n == 0)
				continue;
			dc = &s->as.u.bar.dc;
			break;
		case NOTE:
		case REST:
			if (first == 0)
				first = s;
			if (s->as.u.note.dc.n == 0)
				continue;
			dc = &s->as.u.note.dc;
			break;
		case GRACE:
			for (g = s->grace; g != 0; g = g->next) {
				if (g->type != NOTE
				    || g->as.u.note.dc.n == 0)
					continue;
				dc = &g->as.u.note.dc;
				deco_create(g, dc);
			}
			/* fall thru */
		default:
			continue;
		}
		deco_create(s, dc);
	}
	first_note = first;
}

/* -- draw the decorations tied to a note -- */
/* (the staves are not yet defined) */
void draw_deco_note(void)
{
	struct deco_elt *de, *de2;
	struct deco_def_s *dd;
	int f, t, voice;

	for (de = deco_head; de != 0; de = de->next) {
		t = de->t;
		dd = &deco_def_tb[t];
		if (de->flags & DE_LDST) {	/* start of long decoration */
			t++;
			voice = de->s->voice;
			for (de2 = de->next; de2 != 0; de2 = de2->next)
				if (de2->t == t && de2->s->voice == voice)
					break;
			if (de2 == 0) {		/* no end, insert one */
				de2 = (struct deco_elt *) getarena(sizeof *de2);
				memset(de2, 0, sizeof *de2);
				de2->prev = deco_tail;
				deco_tail->next = de2;
				deco_tail = de2;
				de2->s = de->s;
				de2->t = t;
				de2->defl = DEF_NOEN;
				de2->flags = DE_LDEN;
				de2->x = realwidth - 6;
				de2->y = de->s->y;
			}
			de2->start = de;
			de2->defl &= ~DEF_NOST;
		}
		f = dd->func;
		if (f < 3 || f >= 6)
			continue;	/* not tied to the note */
		if (f == 4)
			de->flags |= DE_BELOW;
		func_tb[f](de);
	}
}

/* -- draw the music elements tied to the staff -- */
/* (the staves are not yet defined) */
void draw_deco_staff(void)
{
	struct SYMBOL *s;
	struct VOICE_S *p_voice;
	float x, y, w;
	struct deco_elt *de;
	int some_gchord;
	struct {
		float ymin, ymax;
	} minmax[MAXSTAFF];

	/* search the vertical offset for the guitar chords */
	memset(minmax, 0, sizeof minmax);
	some_gchord = 0;
	for (s = first_voice->sym; s != 0; s = s->ts_next) {
		char *p;

		if (s->as.text == 0)
			continue;
		switch (s->type) {
		case NOTE:
		case REST:
		case MREST:
			break;
		case BAR:
			if (!s->as.u.bar.repeat_bar)
				break;
		default:
			continue;
		}
		some_gchord = 1;
		w = cwid('a') * cfmt.font_tb[s->gcf].swfac;
		if ((p = strchr(s->as.text, '\n')) != 0
		    || (p = strchr(s->as.text, ';')) != 0)
			w *= p - s->as.text;
		else	w *= strlen(s->as.text);
		y = y_get(s, 1, s->x, w, 0);
		if (y > minmax[s->staff].ymax)
			minmax[s->staff].ymax = y;
	}

	/* draw the guitar chords if any */
	if (some_gchord) {
		int i;

		for (i = nstaff; i >= 0; i--) {
			minmax[i].ymax += 4;
			if (minmax[i].ymax < 34)
				minmax[i].ymax = 34;
		}
		strcf = -1;
		for (s = first_voice->sym; s != 0; s = s->ts_next) {
			if (s->as.text == 0)
				continue;
			switch (s->type) {
			case NOTE:
			case REST:
			case MREST:
				break;
			case BAR:
				if (!s->as.u.bar.repeat_bar)
					break;
			default:
				continue;
			}
			draw_gchord(s, minmax[s->staff].ymax);
		}
	}

	/* draw the repeat brackets */
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		struct SYMBOL *s1, *s2, *first_repeat;
		float y2;
		int i, repnl;

		if (p_voice->second)
			continue;

		/* search the max y offset */
		y = 24 + 6 + 20;
		first_repeat = 0;
		for (s = p_voice->sym->next; s != 0; s = s->next) {
			if (s->type != BAR
			    || !s->as.u.bar.repeat_bar)
				continue;
/*fixme: line cut on repeat!*/
			if (s->next == 0)
				break;
			if (first_repeat == 0) {
				set_font(&cfmt.repeatfont);
				first_repeat = s;
			}
			s1 = s;
			i = s1->as.text ? 4 : 2;
			for (;;) {
				if (s->next == 0)
					break;
				s = s->next;
				if (s->sflags & S_RBSTOP)
					break;
				if (s->type != BAR)
					continue;
				if (((s->as.u.bar.type & 0xf0)	/* if complex bar */
				     && s->as.u.bar.type != (B_OBRA << 4) + B_CBRA)
				    || s->as.u.bar.type == B_CBRA
				    || s->as.u.bar.repeat_bar)
					break;
				if (--i <= 0) {

					/* have a shorter repeat bracket */
					s = s1;
					i = 2;
					for (;;) {
						s = s->next;
						if (s->type != BAR)
							continue;
						if (--i <= 0)
							break;
					}
					s->sflags |= S_RBSTOP;
					break;
				}
			}
			y2 = y_get(s1, 1, s1->x, s->x - s1->x, 0);
			if (y < y2)
				y = y2;

			/* have room for the repeat numbers */
			if (s1->as.text != 0) {
#ifdef YSTEP
				w = cwid('W') * cfmt.repeatfont.swfac
					* strlen(s1->as.text);
				y2 = y_get(s1, 1, s1->x + 4, w, 0);
				y2 += cfmt.repeatfont.size + 2;
				if (y < y2)
					y = y2;
#else
			    for (s2 = s1->next; s2 != s; s2 = s2->next) {
				switch (s2->type) {
				case PART:
				case TEMPO:
				case FMTCHG:
				case TUPLET:
				case WHISTLE:
					continue;
				case GRACE:
				case NOTE:
				case REST:
					y2 = s2->dc_top;
					if (s2->as.text != 0)	/* if guitar chord */
						y2 -= cfmt.font_tb[s->gcf].size; /* got already */
					y2 += cfmt.repeatfont.size + 2;
					if (y < y2)
						y = y2;
					break;
				default:
					break;
				}
				break;
			    }
#endif
			}
			if (s->as.u.bar.repeat_bar)
				s = s->prev;
		}

		/* draw the repeat indications */
		repnl = 0;
		for (s = first_repeat; s != 0; s = s->next) {
			char *p;

			if (s->type != BAR
			    || !s->as.u.bar.repeat_bar)
				continue;
			s1 = s;
			for (;;) {
				if (s->next == 0)
					break;
				s = s->next;
				if (s->sflags & S_RBSTOP)
					break;
				if (s->type != BAR)
					continue;
				if (((s->as.u.bar.type & 0xf0)	/* if complex bar */
				     && s->as.u.bar.type != (B_OBRA << 4) + B_CBRA)
				    || s->as.u.bar.type == B_CBRA
				    || s->as.u.bar.repeat_bar)
					break;
			}
			s2 = s;
/*fixme*/
			if (s1 == s2)
				break;
			x = s1->x;
			if ((s1->as.u.bar.type & 0x0f) == B_COL)
				x -= 4;
			i = 0;			/* no bracket end */
			w = s2->x - x - 8;
			if (s2->sflags & S_RBSTOP)
				;
			else if (s2->type != BAR)
				w = realwidth - x - 4;
			else if (((s2->as.u.bar.type & 0xf0)	/* if complex bar */
				   && s2->as.u.bar.type != (B_OBRA << 4) + B_CBRA)
				 || s2->as.u.bar.type == B_CBRA) {
				i =  2;		/* bracket start and stop */
				if ((s2->as.u.bar.type & 0x0f) == B_COL)
					w -= 4;
				else if (!(s2->sflags & S_RRBAR)
					 || s2->as.u.bar.type == B_CBRA
					 || s2->as.u.bar.type == (B_CBRA << 4) + B_BAR)
					w += 8;		/* explicit repeat end */
				if (p_voice != first_voice
				    && !staff_tb[s->staff - 1].stop_bar)
					w -= 4;
			}
			p = s1->as.text;
			if (p == 0) {
				i--;		/* no bracket start (1) or not draw */
				p = "";
			}
			if (i == 0 && s2->next == 0	/* 2nd ending at end of line */
			    && !(s2->sflags & S_RBSTOP)) {
				if (p_voice->bar_start != 0)
					i = 2;		/* bracket start and stop */
				else	repnl = 1;	/* continue on next line */
			}
			if (i >= 0) {
				PUT3("(%s)-%.1f %d ",
				     p, cfmt.repeatfont.size * 0.8 + 1, i);
				putf(w);
				putxy(x, y);
				PUT1("y%d repbra\n", s1->staff);
				y_set(s1, 1, x, w, y + 2);
			}
			if (s->as.u.bar.repeat_bar)
				s = s->prev;
		}
		if (repnl) {
			p_voice->bar_start = B_OBRA;
			p_voice->bar_repeat = 1;
		}
	}

	/* create the decorations tied to the staves */
	memset(minmax, 0, sizeof minmax);
	for (de = deco_head; de != 0; de = de->next) {
		struct deco_def_s *dd;

		dd = &deco_def_tb[de->t];
		if (dd->func < 6)		/* if not tied to the staff */
			continue;
		func_tb[dd->func](de);
		if (dd->ps_func < 0)
			continue;
		if (cfmt.dynalign) {
			if (de->flags & DE_UP) {
				if (de->y > minmax[de->staff].ymax)
					minmax[de->staff].ymax = de->y;
			} else {
				if (de->y < minmax[de->staff].ymin)
					minmax[de->staff].ymin = de->y;
			}
		}
	}

	/* and, if wanted, set them at a same vertical offset */
	for (de = deco_head; de != 0; de = de->next) {
		struct deco_def_s *dd;

		dd = &deco_def_tb[de->t];
		if (dd->ps_func < 0
		    || dd->func < 6)
			continue;
		if (cfmt.dynalign) {
			if (de->flags & DE_UP)
				y = minmax[de->staff].ymax;
			else	y = minmax[de->staff].ymin;
			de->y = y;
		} else	y = de->y;
		if (de->flags & DE_UP)
			y += dd->h;
		y_set(de->s, de->flags & DE_UP, de->x, de->v, y);
	}
}

/* -- draw the guitar chords and annotations -- */
/* (the staves are not yet defined) */
static void draw_gchord(struct SYMBOL *s,
			float gchy)
{
	float x, y, xspc, yspca, yspcc, gchyb, gchyl, gchyr;
	float xmin, xmax, ymin, ymax;
	int box;
	char *p, *q, *psf, sep, antype;

	/* compute the y offset of all position types */
	ymax = gchy;
	yspcc = cfmt.font_tb[s->gcf].size;
	yspca = cfmt.font_tb[s->anf].size;
	gchy -= yspcc;
#ifdef YSTEP
	gchyb = y_get(s, 0, s->x, 3, 0) - yspca;
#else
	gchyb = s->dc_bot - yspca;
#endif
	gchyl = gchyr = s->yav - yspca * 0.75;
	p = s->as.text;
	antype = '\0';
	sep = '\n';
	for (;;) {
		if (sep == '\n') {
			if (*p != '\0' && strchr("^_<>@", *p) != 0)
				antype = *p++;
			else	antype = '\0';
		}
		switch (antype) {
		default:		/* guitar chord */
			gchy += yspcc;
			break;
		case '^':		/* above */
			gchy += yspca;
			break;
		case '_':		/* below */
			break;
		case '<':		/* left */
			gchyl += yspca * 0.5;
			break;
		case '>':		/* right */
			gchyr += yspca * 0.5;
			break;
		case '@':		/* absolute */
			break;
		}
		for (;;) {
			if (*p == '\0' || *p == ';' || *p == '\n')
				break;
			p++;
			if (p[-1] == '\\') {
				if (*p == '\\' || *p == ';')
					p++;
			}
		}
		sep = *p;
		if (sep == '\0')
			break;
		p++;
	}
#ifdef YSTEP
	ymin = gchy + yspcc;
	y_set(s, 1, s->x, 3, ymin);
#else
	ymin = s->dc_top = gchy + yspcc;
#endif
	xmin = xmax = s->x;
	box = cfmt.gchordbox;
	psf = 0;
	x = y = 0;				/* (compiler warning) */

	/* loop on each line */
	p = s->as.text;
	antype = '\0';
	sep = '\n';
	for (;;) {
		if (sep == '\n') {
			if (*p == '@') {
				int n;
				float xo, yo;

				x = s->x;
				if (sscanf(p, "@%f,%f%n", &xo, &yo, &n) != 2) {
					error(1, s,
					      "Error in annotation \"@\"");
					y = s->yav;
				} else {
					x += xo;
					y = s->yav + yo;
					p += n;
					if (*p == ' ')
						p++;
				}
				antype = '@';
			} else if (*p != '\0' && strchr("^_<>", *p) != 0)
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
		if (antype == '\0') {
			if (strcf != s->gcf) {
				strcf = s->gcf;
				set_font(&cfmt.font_tb[s->gcf]);
			}
			psf = "gc";
		} else {
			if (strcf != s->anf) {
				strcf = s->anf;
				set_font(&cfmt.font_tb[s->anf]);
			}
			psf = "an";
		}
		xspc = tex_str(p);
		switch (antype) {
		case '^':		/* above */
		case '_':		/* below */
		default: {		/* default = above */
			float tmp;

			tmp = xspc;
			xspc *= GCHPRE;
			if (xspc > 8)
				xspc = 8;
			x = s->x - xspc;
			if (antype == '_') {
				y = gchyb;
#ifdef YSTEP
				y_set(s, 0, x, xspc, gchyb - 2);
#else
				s->dc_bot = gchyb - 2;
#endif
				gchyb -= yspca;
			} else {
				y = gchy;
				if (antype == '\0') {
					gchy -= yspcc;
					if (box) {
						if (x < xmin)
							xmin = x;
						tmp += x;
						if (tmp > xmax)
							xmax = tmp;
						if (ymax < y + yspcc)
							ymax = y + yspcc;
						if (ymin > y)
							ymin = y;
						box = 2;
					}
				} else	gchy -= yspca;

			}
			break;
		    }
		case '<':		/* left */
/*fixme: what symbol space?*/
			x = s->x - xspc - 6;
			if (s->as.u.note.accs[0])
				x -= s->shac[0];
			y = gchyl;
			gchyl -= yspca;
			break;
		case '>':		/* right */
			x = s->x + s->xmx + 6;
			if (s->dots > 0)
				x += 1.5 + 3.5 * s->dots;
			y = gchyr;
			gchyr -= yspca;
			break;
		case '@':		/* absolute */
			antype = '\001';
			break;
		case '\001':		/* next absolute */
			y -= yspca;
			break;
		}
		putxy(x, y);
		PUT3("y%d M(%s)%sshow ",
		     s->staff, tex_buf, psf);
		if (sep == '\0')
			break;
		*q = sep;
		p = q + 1;
	}

	/* draw the box */
	if (box == 2) {		/* if any normal guitar chord */
		PUT5("%.1f %.1f y%d %.1f %.1f box",
		     xmin - 2, ymin - 5, s->staff,
		     xmax - xmin + 4, ymax - ymin + 4);
	}
	PUT0("\n");
}

/* -- draw the measure bar numbers -- */
void draw_measnb(void)
{
	struct SYMBOL *s;
	char *showm;
	int bar_time, any_nb, wmeasure;
	float x, y, w;

	showm = cfmt.measurebox ? "showb" : "show";
	any_nb = 0;

	/* get the current bar number */
/*fixme: what to do if no symbol in the 1st voice?*/
	if ((s = first_voice->sym->next) == 0)
		return;
	for ( ; s->next != 0; s = s->next) {
		switch (s->type) {
		case TIMESIG:
		case CLEF:
		case KEYSIG:
		case PART:
		case TEMPO:
		case FMTCHG:
		case WHISTLE:
			continue;
		case BAR:
			if (s->u != 0)
				nbar = s->u;		/* (%%setbarnb) */
			else if (s->as.u.bar.repeat_bar
				 && s->as.text != 0
				 && cfmt.contbarnb == 0) {
				if (s->as.text[0] == '1')
					nbar_rep = nbar;
				else	nbar = nbar_rep; /* restart bar numbering */
			}
			break;
		default:
			break;
		}
		break;
	}
	if (nbar > 1) {
		if (s->prev->type != CLEF)
			s = s->prev;
		if (cfmt.measurenb == 0) {
			s = s->prev;		/* clef */
			set_font(&cfmt.measurefont);
			any_nb = 1;
			x = 0;
			w = 20;
			y = y_get(s, 1, x, w, 0);
			if (y < 24 + 14)
				y = 24 + 14;
			PUT0("0 ");
			puty(y);
			PUT2("y0 M(%d)%s",nbar, showm);
			y_set(s, 1, x, w, y + cfmt.measurefont.size + 2);
		} else if (nbar % cfmt.measurenb == 0) {
			x = s->x - 8;
			set_font(&cfmt.measurefont);
			any_nb = 1;
			w = cwid('0') * cfmt.measurefont.size;
			if (nbar >= 10) {
				if (nbar >= 100)
					w *= 3;
				else	w *= 2;
			}
			if (cfmt.measurebox)
				w += 4;
			y = y_get(s, 1, x, w, 0);
			if (y < 24 + 7)
				y = 24 + 7;
			putxy(x, y);
			PUT2("y0 M(%d)%s", nbar, showm);
			y_set(s, 1, x, w, y + cfmt.measurefont.size + 2);
		}
	}

/*fixme: KO when no bar at the end of the previous line */
	wmeasure = first_voice->meter.wmeasure;
	bar_time = first_voice->sym->time + wmeasure;
	for (s = first_voice->sym->next; s != 0; s = s->next) {
		switch (s->type) {
		case TIMESIG:
			wmeasure = s->as.u.meter.wmeasure;
			bar_time = s->time + wmeasure;
			continue;
		case MREST:
			nbar += s->as.u.bar.len - 1;
			continue;
		case MREP:
			if (s->as.u.bar.len > 2)
				nbar += s->as.u.bar.len - 2;
			continue;
		default:
			continue;
		case BAR:
			break;
		}
		if (s->u != 0)
			nbar = s->u;		/* (%%setbarnb) */
		if (s->time < bar_time)		/* incomplete measure */
			continue;
		if (s->u == 0) {
			nbar++;
			if (s->as.u.bar.repeat_bar
			    && s->as.text != 0
			    && cfmt.contbarnb == 0) {
				if (s->as.text[0] == '1')
					nbar_rep = nbar;
				else	nbar = nbar_rep; /* restart bar numbering */
			}
		}
		bar_time = s->time + wmeasure;
		if (s->as.u.bar.repeat_bar
		    || s->next == 0
		    || cfmt.measurenb == 0
		    || (nbar % cfmt.measurenb) != 0
		    || nbar <= 1
		    || (s->next->type == MREP
			&& s->next->as.u.bar.len > 1))
			continue;
		if (!any_nb) {
			any_nb = 1;
			set_font(&cfmt.measurefont);
		}
		w = cwid('0') * cfmt.measurefont.size;
		if (nbar >= 10) {
			if (nbar >= 100)
				w *= 3;
			else	w *= 2;
		}
		if (cfmt.measurebox)
			w += 4;
		x = s->x - w * 0.4;
		y = y_get(s, 1, x, w, 0);
		if (y < 24 + 7)
			y = 24 + 7;
		if (s->next->type == NOTE) {
			if (s->next->stem > 0) {
				if (y < s->next->ys - cfmt.measurefont.size)
					y = s->next->ys - cfmt.measurefont.size;
			} else {
				if (y < s->next->y)
					y = s->next->y;
			}
		}
		PUT0(" ");
		putxy(x, y);
		PUT2("y0 M(%d)%s",nbar, showm);
		y_set(s, 1, x, w, y + cfmt.measurefont.size + 2);
	}
	if (any_nb)
		PUT0("\n");
}

/* -- get the beat from a time signature -- */
static int get_beat(struct meter_s *m)
{
	int top, bot;

	if (m->meter[0].top[0] == 'C') {
		if (m->meter[0].top[0] == '|')
			return BASE_LEN / 2;
		return BASE_LEN / 4;
	}
	if (m->meter[0].bot[0] == '\0')
		return BASE_LEN / 4;
	sscanf(m->meter[0].top, "%d", &top);
	sscanf(m->meter[0].bot, "%d", &bot);
	if (bot >= 8 && top >= 6 && top % 3 == 0)
		return BASE_LEN * 3 / 8;
	return BASE_LEN / bot;
}

/* -- draw the note of the tempo -- */
static void draw_notempo(struct SYMBOL *s, int len, float sc)
{
	int head, dots, flags;
	float dx;

	identify_note(s, len, &head, &dots, &flags);
	PUT1("gsave %.2f dup scale 8 3 RM currentpoint ", sc);
	switch (head) {
	case H_OVAL:
		PUT0("HD");
		break;
	case H_EMPTY:
		PUT0("Hd");
		break;
	default:
		PUT0("hd");
		break;
	}
	dx = 4;
	if (dots) {
		float dotx;

		dotx = 8;
		if (flags > 0)
			dotx += 4;
		switch (head) {
		case H_SQUARE:
		case H_OVAL:
			dotx += 2;
			break;
		case H_EMPTY:
			dotx += 1;
			break;
		}
		while (--dots >= 0) {
			PUT1(" %.1f 0 dt", dotx);
			dx = dotx;
			dotx += 3.5;
		}
	}
	if (len < SEMIBREVE) {
		if (flags <= 0)
			PUT1(" %d su", STEM);
		else {
			PUT2(" %d %d sfu", flags, STEM);
			if (dx < 6)
				dx = 6;
		}
	}
	PUT1(" grestore %.1f 0 RM\n", (dx + 15) * sc);
}

/* -- draw the parts and the tempo information -- */
/* (the staves are being defined) */
float draw_partempo(float top,
		    int any_part,
		    int any_tempo)
{
	struct SYMBOL *s;
	float h, ht, w, y, ymin, dy;
	int i;

	/* put the tempo indication at top */
	dy = 0;
	if (any_tempo) {
/*4.12.25+*/
		int beat, dosh, shift;
/*4.12.26+*/
		float sc, x, nw;

		ht = cfmt.tempofont.size + 2 + 2;
		str_font(&cfmt.tempofont);
		nw = 6. + cwid(' ') * cfmt.tempofont.size * 6;
/*fixme:have tempo on other voices but the 1st?*/

		/* get the minimal y offset */
		ymin = 24 + 12;
		dosh = 0;
		shift = 1;
		x = 0;
		for (s = first_voice->sym->next; s != 0; s = s->next) {
			if (s->type != TEMPO)
				continue;
			w = nw;
			if (s->as.u.tempo.str1 != 0)
				w += tex_str(s->as.u.tempo.str1);
			if (s->as.u.tempo.value != 0) {
				i = 1;
				while (i < sizeof s->as.u.tempo.length
						/ sizeof s->as.u.tempo.length[0]
				       && s->as.u.tempo.length[i] > 0) {
					w += 10;
					i++;
				}
				w += 10 + 10;
			}
			if (s->as.u.tempo.str2 != 0)
				w += tex_str(s->as.u.tempo.str2);
			y = y_get(s, 1, s->x - 5, w, 0) + 2;
			if (y > ymin)
				ymin = y;
			if (x >= s->x - 5 && !(dosh & (shift >> 1)))
				dosh |= shift;
			shift <<= 1;
			x = s->x - 5 + w;
		}
		y = 2 - ht;
		h = y - ht;
		if (dosh != 0)
			ht *= 2;
		if (top < ymin + ht)
			dy = ymin + ht - top;

		/* draw the tempo indications */
		str_font(&cfmt.tempofont);
		set_font(&cfmt.tempofont);
		strcf = 0;
		sc = 0.7 * cfmt.tempofont.size / 15.0;	/*fixme: 15.0 = initial tempofont*/
		beat = get_beat(&first_voice->meter);
		for (s = first_voice->sym; s != 0; s = s->next) {
			if (s->type != TEMPO) {
				if (s->type == TIMESIG)
					beat = get_beat(&s->as.u.meter);
				continue;
			}

			/*fixme: cf left shift (-5)*/
			PUT2("%.1f %.1f M ", s->x - 5,
				(dosh & 1) ? h : y);
			dosh >>= 1;
			if (s->as.u.tempo.str1 != 0)
				put_str(s->as.u.tempo.str1, A_LEFT);

			/* draw the tempo indication, if specified */
			if (s->as.u.tempo.value != 0) {
				int j, top, bot;

				if (s->as.u.tempo.length[0] == 0)
					s->as.u.tempo.length[0] = beat;
				for (j = 0;
				     j < sizeof s->as.u.tempo.length
						/ sizeof s->as.u.tempo.length[0]
					&& s->as.u.tempo.length[j] > 0;
				     j++) {
					draw_notempo(s, s->as.u.tempo.length[j], sc);
				}
				PUT0("(= )show ");
				if (sscanf(s->as.u.tempo.value, "%d/%d", &top, &bot) == 2
				    && bot > 0)
					draw_notempo(s, top * BASE_LEN / bot, sc);
				else	put_str(s->as.u.tempo.value, A_LEFT);
			}

			if (s->as.u.tempo.str2 != 0)
				put_str(s->as.u.tempo.str2, A_LEFT);
		}
	} else	ht = 0;
/*4.12.25-*/

	/* then, put the parts */
	if (!any_part)
		return dy;

/*fixme: should reduce if parts don't overlap tempo...*/
	h = cfmt.partsfont.size + 2 + 2;	/* + cfmt.partsspace; */
	
	str_font(&cfmt.partsfont);
	ymin = 24 + 14;
	for (s = first_voice->sym->next; s != 0; s = s->next) {
		if (s->type != PART)
			continue;
		w = tex_str(&s->as.text[2]);
		y = y_get(s, 1, s->x - 10, w + 15, 0) + 5;
		if (ymin < y)
			ymin = y;
	}
	if (top < ymin + h + ht)
		dy = ymin + h + ht - top;

	set_font(&cfmt.partsfont);
	for (s = first_voice->sym->next; s != 0; s = s->next) {
		if (s->type != PART)
			continue;
		tex_str(&s->as.text[2]);
		PUT4("%.1f %.1f M(%s)show%s\n",
		     s->x - 10., 2. - ht - h, tex_buf,
		     cfmt.partsbox ? "b" : "");
	}
	return dy;
}

/* -- initialize the default decorations -- */
void reset_deco(int deco_old)
{
	memset(&deco_glob, 0, sizeof deco_glob);

	/* standard */
	deco_glob['.'] = 1;
#ifdef DECO_IS_ROLL
	deco_glob['~'] = 2;
#endif
	deco_glob['H'] = 3;
	deco_glob['L'] = 4;
	deco_glob['M'] = 5;
	deco_glob['O'] = 6;
	deco_glob['P'] = 7;
	deco_glob['S'] = 8;
	deco_glob['T'] = 9;
	deco_glob['u'] = 10;
	deco_glob['v'] = 11;

	/* non-standard */
#ifndef DECO_IS_ROLL
	deco_glob['~'] = 12;
#endif
	deco_glob['J'] = 13;
	deco_glob['R'] = 2;

	/* abc2ps */
	if (deco_old) {
		deco_glob['M'] = 14;
	}
}

/* -- set the decoration flags -- */
void set_defl(int new_defl)
{
	if (defl == new_defl)
		return;
	defl = new_defl;
	PUT1("/defl %d def ", new_defl);
}
