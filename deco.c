/*
 * Decoration handling.
 *
 * This file is part of abcm2ps.
 *
 * Copyright (C) 2000-2017, Jean-François Moine.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#ifdef WIN32
#define lroundf(x) ((long) ((x) + 0.5))
#endif

#include "abcm2ps.h"

int defl;		/* decoration flags */
char *deco[256];	/* decoration names */

static struct deco_elt {
	struct deco_elt *next, *prev;	/* next/previous decoration */
	struct SYMBOL *s;	/* symbol */
	struct deco_elt *start;	/* start a long decoration ending here */
	unsigned char t;	/* decoration index */
	unsigned char staff;	/* staff */
	unsigned char flags;
#define DE_VAL	0x01		/* put extra value if 1 */
#define DE_UP	0x02		/* above the staff */
//#define DE_BELOW 0x08		/* below the staff */
//#define DE_GRACE 0x10		/* in grace note */
#define DE_INV 0x20		/* invert the glyph */
#define DE_LDST 0x40		/* start of long decoration */
#define DE_LDEN 0x80		/* end of long decoration */
	unsigned char defl;	/* decorations flags - see DEF_xx */
	signed char m;		/* chord index */
	float x, y;		/* x, y */
	float dy;		/* dy for annotation strings */
	float val;		/* extra value */
//	char *str;		/* string / 0 */
} *deco_head, *deco_tail;

typedef void draw_f(struct deco_elt *de);
static draw_f d_arp, d_cresc, d_near, d_slide, d_upstaff,
		d_pf, d_trill;

/* decoration table */
/* !! don't change the order of the numbered items !! */
static struct deco_def_s {
	char *name;
	unsigned char func;	/* function index */
	signed char ps_func;	/* postscript function index */
	unsigned char h;	/* height */
	unsigned char wl, wr;	/* left and right widths */
	unsigned char strx;	/* string index - 255=deco name */
	unsigned char ld_start;	/* index of start of long decoration */
	unsigned char ld_end;	/* index of end of long decoration */
	unsigned char flags;	/* only DE_LDST and DE_LDEN */
} deco_def_tb[128];

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
static const short f_near = (1 << 0) | (1 << 1) | (1 << 2);
static const short f_note = (1 << 3) | (1 << 4) | (1 << 5);
static const short f_staff = (1 << 6) | (1 << 7);

/* postscript function table */
static char *ps_func_tb[128];

static char *str_tb[32];

/* standard decorations */
static char *std_deco_tb[] = {
	"dot 0 stc 5 1 1",
	"roll 3 cpu 7 6 6",
	"fermata 3 hld 12 7 7",
	"emphasis 3 accent 7 4 4",
	"lowermordent 3 lmrd 10 2 2",
	"coda 3 coda 24 10 10",
	"uppermordent 3 umrd 10 2 2",
	"segno 3 sgno 20 4 4",
	"trill 3 trl 11 4 4",
	"upbow 3 upb 10 5 5",
	"downbow 3 dnb 9 5 5",
	"gmark 3 grm 6 5 5",
	"slide 1 sld 3 7 0",
	"tenuto 0 emb 5 2 2",
	"breath 3 brth 0 1 20",
	"longphrase 3 lphr 0 1 1",
	"mediumphrase 3 mphr 0 1 1",
	"shortphrase 3 sphr 0 1 1",
	"invertedfermata 3 hld 12 7 7",
	"invertedturn 3 turn 10 0 5",
	"invertedturnx 3 turnx 10 0 5",
	"0 3 fng 8 3 3 0",
	"1 3 fng 8 3 3 1",
	"2 3 fng 8 3 3 2",
	"3 3 fng 8 3 3 3",
	"4 3 fng 8 3 3 4",
	"5 3 fng 8 3 3 5",
	"plus 3 dplus 7 3 3",
	"+ 3 dplus 7 3 3",
	"accent 3 accent 7 4 4",
	"> 3 accent 7 4 4",
	"marcato 3 marcato 9 3 3",
	"^ 3 marcato 9 3 3",
	"D.C. 3 dacs 16 10 10 D.C.",
	"D.S. 3 dacs 16 10 10 D.S.",
	"fine 3 dacs 16 10 10 FINE",
	"f 6 pf 18 1 7",
	"ff 6 pf 18 2 10",
	"fff 6 pf 18 4 13",
	"ffff 6 pf 18 6 16",
	"mf 6 pf 18 6 13",
	"mp 6 pf 18 6 16",
	"mordent 3 lmrd 10 2 2",
	"open 3 opend 10 2 2",
	"p 6 pf 18 2 8",
	"pp 6 pf 18 5 14",
	"ppp 6 pf 18 8 20",
	"pppp 6 pf 18 10 25",
	"pralltriller 3 umrd 10 2 2",
	"sfz 6 sfz 18 4 10",
	"ped 4 ped 20 0 0",
	"ped-up 4 pedoff 20 0 0",
	"turn 3 turn 10 0 5",
	"wedge 3 wedge 8 1 1",
	"turnx 3 turnx 10 0 5",
	"trill( 5 ltr 8 0 0",
	"trill) 5 ltr 8 0 0",
	"snap 3 snap 14 3 3",
	"thumb 3 thumb 14 2 2",
	"arpeggio 2 arp 12 10 0",
	"crescendo( 7 cresc 18 0 0",
	"crescendo) 7 cresc 18 0 0",
	"<( 7 cresc 18 0 0",
	"<) 7 cresc 18 0 0",
	"diminuendo( 7 dim 18 0 0",
	"diminuendo) 7 dim 18 0 0",
	">( 7 dim 18 0 0",
	">) 7 dim 18 0 0",
	"-( 8 gliss 0 0 0",
	"-) 8 gliss 0 0 0",
	"~( 8 glisq 0 0 0",
	"~) 8 glisq 0 0 0",
	"8va( 3 o8va 10 0 0",
	"8va) 3 o8va 10 0 0",
	"8vb( 4 o8vb 10 0 0",
	"8vb) 4 o8vb 10 0 0",
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
	"/ 38 0 0 6 6",
	"// 38 0 0 6 6",
	"/// 38 0 0 6 6",
	"beam-accel 39 0 0 0 0",
	"beam-rall 39 0 0 0 0",
	"stemless 40 0 0 0 0",
	"rbend 41 0 0 0 0",
	0
};

/* user decorations */
static struct u_deco {
	struct u_deco *next;
	char text[256];			// dummy size
} *user_deco;

//static struct SYMBOL *first_note;	/* first note/rest of the line */

static unsigned char deco_define(char *name);
static void draw_gchord(struct SYMBOL *s, float gchy_min, float gchy_max);

/* -- get the max/min vertical offset -- */
float y_get(int staff,
		int up,
		float x,
		float w)
{
	struct STAFF_S *p_staff;
	int i, j;
	float y;

	p_staff = &staff_tb[staff];
	i = (int) (x / realwidth * YSTEP);
	if (i < 0) {
//		fprintf(stderr, "y_get i:%d\n", i);
		i = 0;
	}
	j = (int) ((x + w) / realwidth * YSTEP);
	if (j >= YSTEP) {
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
	}
	return y;
}

/* -- adjust the vertical offsets -- */
void y_set(int staff,
		int up,
		float x,
		float w,
		float y)
{
	struct STAFF_S *p_staff;
	int i, j;

	p_staff = &staff_tb[staff];
	i = (int) (x / realwidth * YSTEP);
	/* (may occur when annotation on 'y' at start of an empty staff) */
	if (i < 0) {
//		fprintf(stderr, "y_set i:%d\n", i);
		i = 0;
	}
	j = (int) ((x + w) / realwidth * YSTEP);
	if (j >= YSTEP) {
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
}

//// set the string of a decoration
//static char *set_str(struct deco_elt *de, char *str)
//{
//	float dx, dy;
//	int n;
//
//	if (sscanf(str, "@%f,%f%n", &dx, &dy, &n) == 2) {
//		de->x += dx;
//		de->dy = dy;
//		return str + n;
//	}
//	return str;
//}

/* -- get the staff position of the dynamic and volume marks -- */
static int up_p(struct SYMBOL *s, int pos)
{
	switch (pos) {
	case SL_ABOVE:
		return 1;
	case SL_BELOW:
		return 0;
	}
	if (s->multi != 0)
		return s->multi > 0;
	if (!voice_tb[s->voice].have_ly)
		return 0;

	/* above if the lyrics are below the staff */
	return s->posit.voc != SL_ABOVE;
}

/* -- drawing functions -- */
/* 2: special case for arpeggio */
static void d_arp(struct deco_elt *de)
{
	struct SYMBOL *s;
	struct deco_def_s *dd;
	int m, h;
	float xc, dx;

	s = de->s;
	dd = &deco_def_tb[de->t];
	xc = 0;
	for (m = 0; m <= s->nhd; m++) {
		if (s->u.note.notes[m].acc) {
			dx = 5 + s->u.note.notes[m].shac;
		} else {
			dx = 6 - s->u.note.notes[m].shhd;
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
	de->val = h;
	de->x = s->x - xc;
	de->y = (float) (3 * (s->pits[0] - 18)) - 3;
}

/* 7: special case for crescendo/diminuendo */
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
	de1 = de->start;		/* start of the deco */
//	if (de1) {
		s = de1->s;
		x = s->x + 3;
//	} else {			/* end without start */
//		if (!first_note) {
//			dd = &deco_def_tb[de->t];
//			error(1, s2, "No start of deco !%s!", dd->name);
//			de->t = 0;
//			return;
//		}
//		s = first_note;
//		x = s->x - s->wl - 4;
//	}
	de->staff = s2->staff;
	de->flags &= ~DE_LDEN;		/* old behaviour */
	de->flags |= DE_VAL;
	up = up_p(s2, s2->posit.dyn);
	if (up)
		de->flags |= DE_UP;

	/* shift the starting point if any dynamic mark on the left */
	if (de1 && de1->prev && de1->prev->s == s
	 && ((de->flags ^ de1->prev->flags) & DE_UP) == 0) {
		dd2 = &deco_def_tb[de1->prev->t];
		if (f_staff & (1 << dd2->func)) {
			x2 = de1->prev->x + de1->prev->val + 4;
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
		if (de->next && de->next->s == s
		 && ((de->flags ^ de->next->flags) & DE_UP) == 0) {
			dd2 = &deco_def_tb[de->next->t];
			if (f_staff & (1 << dd2->func))	/* if dynamic mark */
				x2 -= 5;
		}
		dx = x2 - x - 4;
		if (dx < 20) {
			x -= (20 - dx) * 0.5;
//			if (!de->start)
//				x -= (20 - dx) * 0.5;
			dx = 20;
		}
	}

	de->val = dx;
	de->x = x;
	de->y = y_get(de->staff, up, x, dx);
	if (!up) {
		dd = &deco_def_tb[de->t];
		de->y -= dd->h;
	}
	/* (y_set is done later in draw_deco_staff) */
}

/* special case for glissendo */
static void d_gliss(struct deco_elt *de2)
{
	struct deco_elt *de1;
	struct SYMBOL *s1, *s2;

	de1 = de2->start;
	s1 = de1->s;
	if (s1->dots)
		de1->x += 5 + s1->xmx;
	s2 = de2->s;
	de2->x -= 2 + s2->u.note.notes[0].shac ?
			(s2->u.note.notes[0].shac + 3) : hw_tb[s2->head];
}

/* 0: near the note (dot, tenuto) */
static void d_near(struct deco_elt *de)
{
	struct SYMBOL *s;
	struct deco_def_s *dd;
	int y, up;

	s = de->s;
	dd = &deco_def_tb[de->t];

	if (s->multi)
		up = s->multi > 0;
	else
		up = s->stem < 0;
	if (up)
		y = s->ymx;
	else
		y = s->ymn - dd->h;
	if (y > -6 && y < 24) {
		if (up)
			y += 3;
		y = (y + 6) / 6 * 6 - 6;	/* between lines */
	}
	if (up)
		s->ymx = y + dd->h;
	else
		s->ymn = y;
	de->y = (float) y;
	de->x = s->x;
	if (s->type == NOTEREST)
		de->x += s->u.note.notes[s->stem >= 0 ? 0 : s->nhd].shhd;
	if (dd->name[0] == 'd'			/* if dot decoration */
	 && s->nflags >= -1) {			/* on stem */
		if (up) {
			if (s->stem > 0)
				de->x += STEM_XOFF;
		} else {
			if (s->stem < 0)
				de->x -= STEM_XOFF;
		}
	}
	if (dd->strx != 0 && dd->strx != 255) {
		de->x = s->x;
		de->y = s->y;
//		de->str = set_str(de, str_tb[dd->strx]);
	}
}

/* 6: dynamic marks */
static void d_pf(struct deco_elt *de)
{
	struct SYMBOL *s;
	struct deco_def_s *dd, *dd2;
	float x, x2;
//	char *str;
	int up;

	s = de->s;
	dd = &deco_def_tb[de->t];

	de->val = dd->wl + dd->wr;

	up = up_p(s, s->posit.vol);
	if (up)
		de->flags |= DE_UP;

	x = s->x - dd->wl;
	if (de->prev && de->prev->s == s
	 && ((de->flags ^ de->prev->flags) & DE_UP) == 0) {
		dd2 = &deco_def_tb[de->prev->t];
		if (f_staff & (1 << dd2->func)) {	/* if dynamic mark */
			x2 = de->prev->x + de->prev->val + 4;
			if (x2 > x)
				x = x2;
		}
#if 0
//fixme:test volume shift
// does not work with
//	cE!p!E !fff!Ceg|
	} else if (!up && s->stem < 0 && s->ymn < 10) {
		float y;

		x2 = x - (STEM_XOFF + dd->wr + 4);
		y = y_get(s->staff, up, x2, de->val);
		if (y > s->ymn) {
			x = x2;
		} else {
			x2 -= 3;
			y = y_get(s->staff, up, x2, de->val);
			if (y > s->ymn)
				x = x2;
		}
#endif
	}

	de->x = x;
	de->y = y_get(s->staff, up, x, de->val);
	if (!up)
		de->y -= dd->h;

//	str = dd->name;
//	if (dd->strx != 0 && dd->strx != 255)
//		str = set_str(de, str_tb[dd->strx]);
//	de->str = str;
	/* (y_set is done later in draw_deco_staff) */
}

/* 1: special case for slide */
static void d_slide(struct deco_elt *de)
{
	struct SYMBOL *s;
	int m, yc;
	float xc, dx;

	s = de->s;
	yc = s->pits[0];
	xc = 5;
	if (s->type == NOTEREST) {
		for (m = 0; m <= s->nhd; m++) {
			if (s->u.note.notes[m].acc) {
				dx = 4 + s->u.note.notes[m].shac;
			} else {
				dx = 5 - s->u.note.notes[m].shhd;
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
	}
	de->x = s->x - xc;
	de->y = (float) (3 * (yc - 18));
}

/* 5: special case for long trill */
static void d_trill(struct deco_elt *de)
{
	struct SYMBOL *s, *s2;
	struct deco_def_s *dd;
	int staff, up;
	float x, y, w;

	if (de->flags & DE_LDST)
		return;
	s2 = de->s;

//	if (de->start) {		/* deco start */
		s = de->start->s;
		x = s->x;
		if (s->abc_type == ABC_T_NOTE
		 && s->u.note.dc.n > 1)
			x += 10;
//	} else {			/* end without start */
//		s = first_note;
//		if (!s) {
//			dd = &deco_def_tb[de->t];
//			error(1, s2, "No start of deco !%s!", dd->name);
//			de->t = 0;
//			return;
//		}
//		x = s->x - s->wl - 4;
//	}
	de->staff = staff = s2->staff;

	dd = &deco_def_tb[de->t];
	if (dd->func == 4)		// if below
		up = 0;
	else
		up = s2->multi >= 0;
	if (de->defl & DEF_NOEN) {	/* if no decoration end */
		w = de->x - x;
		if (w < 20) {
			x = de->x - 20 - 3;
			w = 20;
		}
	} else {
		w = s2->x - x - 6;
		if (s2->abc_type == ABC_T_NOTE)
			w -= 6;
		if (w < 20) {
			x -= (20 - w) * 0.5;
//			if (!de->start)
//				x -= (20 - w) * 0.5;
			w = 20;
		}
	}

	y = y_get(staff, up, x, w);
	if (up) {
		float stafft;

		stafft = staff_tb[s->staff].topbar + 2;
		if (y < stafft)
			y = stafft;
	} else {
		float staffb;

		y -= dd->h;
		staffb = staff_tb[s->staff].botbar - 2;
		if (y > staffb)
			y = staffb;
	}
	de->flags &= ~DE_LDEN;
	de->flags |= DE_VAL;
	de->val = w;
	de->x = x;
	de->y = y;
	if (up)
		y += dd->h;
	y_set(staff, up, x, w, y);
	if (up)
		s->ymx = s2->ymx = y;
	else
		s->ymn = s2->ymn = y;
}

/* 3, 4: above (or below) the staff */
static void d_upstaff(struct deco_elt *de)
{
	struct SYMBOL *s;
	struct deco_def_s *dd;
	char *ps_name;
	float x, yc, stafft, staffb, w;
	int up, inv;

	// don't treat here the long decorations
	if (de->flags & DE_LDST)
		return;
	if (de->start) {
		d_trill(de);
		return;
	}

	s = de->s;
	dd = &deco_def_tb[de->t];
	inv = 0;
	x = s->x;
	if (s->type == NOTEREST)
		x += s->u.note.notes[s->stem >= 0 ? 0 : s->nhd].shhd;
	w = dd->wl + dd->wr;
	stafft = staff_tb[s->staff].topbar + 2;
	staffb = staff_tb[s->staff].botbar - 2;

	up = -1;			// undefined
	if (dd->func == 4) {		// upstaff below
		up = 0;
	} else {
		switch (s->posit.orn) {
		case SL_ABOVE:
			up = 1;
			break;
		case SL_BELOW:
			up = 0;
			break;
		}
	}

	ps_name = ps_func_tb[dd->ps_func];
	if (strcmp(ps_name, "accent") == 0
	 || strcmp(ps_name, "cpu") == 0) {
		if (!up
		 || (up < 0
		  && (s->multi < 0
		   || (s->multi == 0 && s->stem > 0)))) {
			yc = y_get(s->staff, 0, s->x - dd->wl, w);
			if (yc > staffb)
				yc = staffb;
			yc -= dd->h;
			y_set(s->staff, 0, s->x, 0, yc);
			inv = 1;
			s->ymn = yc;
		} else {
			yc = y_get(s->staff, 1, s->x, 0);
			if (yc < stafft)
				yc = stafft;
			y_set(s->staff, 1, s->x - dd->wl, w, yc + dd->h);
			s->ymx = yc + dd->h;
		}
	} else if (strcmp(ps_name, "brth") == 0
		|| strcmp(ps_name, "lphr") == 0
		|| strcmp(ps_name, "mphr") == 0
		|| strcmp(ps_name, "sphr") == 0) {
		yc = stafft + 1;
		if (ps_name[0] == 'b') {		// if breath
			if (yc < s->ymx)
				yc = s->ymx;
		}
		for (s = s->ts_next; s; s = s->ts_next)
			if (s->shrink != 0)
				break;
		if (s)
			x += (s->x - x) * 0.4;
		else
			x += (realwidth - x) * 0.4;
	} else {
		if (strncmp(dd->name, "invert", 6) == 0)
			inv = 1;
		if (strcmp(dd->name, "invertedfermata") != 0
		 && (up > 0
		  || (up < 0 && s->multi >= 0))) {
			yc = y_get(s->staff, 1, s->x - dd->wl, w);
			if (yc < stafft)
				yc = stafft;
			y_set(s->staff, 1, s->x - dd->wl, w, yc + dd->h);
			s->ymx = yc + dd->h;
		} else {
			yc = y_get(s->staff, 0, s->x - dd->wl, w);
			if (yc > staffb)
				yc = staffb;
			yc -= dd->h;
			y_set(s->staff, 0, s->x - dd->wl, w, yc);
			if (strcmp(dd->name, "fermata") == 0)
//			 || strcmp(dd->name, "invertedfermata") == 0)
				inv = 1;
			s->ymn = yc;
		}
	}
	if (inv) {
		yc += dd->h;
		de->flags |= DE_INV;
	}
	de->x = x;
	de->y = yc;

//	if (dd->strx != 0) {
//		if (dd->strx == 255)
//			de->str = dd->name;
//		else
//			de->str = set_str(de, str_tb[dd->strx]);
//	}
}

/* -- add a decoration - from %%deco -- */
/* syntax:
 *	%%deco <name> <c_func> <ps_func> <h> <wl> <wr> [<str>]
 */
void deco_add(char *s)
{
	struct u_deco *d;
	int l;

	l = strlen(s);
	d = malloc(sizeof *user_deco - sizeof user_deco->text + l + 1);
	strcpy(d->text, s);
	d->next = user_deco;
	user_deco = d;
}

static int get_deco(char *name)
{
	struct deco_def_s *dd;
	int ideco;

	for (ideco = 1, dd = &deco_def_tb[1]; ideco < 128; ideco++, dd++) {
		if (!dd->name
		 || strcmp(dd->name, name) == 0)
			return ideco;
	}
	error(1, NULL, "Too many decorations");
	return ideco;
}

static unsigned char deco_build(char *name, char *text)
{
	struct deco_def_s *dd;
	int c_func, ideco, h, o, wl, wr, n;
	unsigned l, ps_x, strx;
	char name2[32];
	char ps_func[16];

	/* extract the arguments */
	if (sscanf(text, "%15s %d %15s %d %d %d%n",
			name2, &c_func, ps_func, &h, &wl, &wr, &n) != 6) {
		error(1, NULL, "Invalid %%%%deco %s", text);
		return 128;
	}
	if ((unsigned) c_func > 10
	 && (c_func < 32 || c_func > 41)) {
		error(1, NULL, "%%%%deco: bad C function index (%s)", text);
		return 128;
	}
	if (h < 0 || wl < 0 || wr < 0) {
		error(1, NULL, "%%%%deco: cannot have a negative value (%s)", text);
		return 128;
	}
	if (h > 50 || wl > 80 || wr > 80) {
		error(1, NULL, "%%%%deco: abnormal h/wl/wr value (%s)", text);
		return 128;
	}
	text += n;
	while (isspace((unsigned char) *text))
		text++;

	/* search the decoration */
	ideco = get_deco(name);
	if (ideco == 128)
		return ideco;
	dd = &deco_def_tb[ideco];

	/* search the postscript function */
	for (ps_x = 0; ps_x < sizeof ps_func_tb / sizeof ps_func_tb[0]; ps_x++) {
		if (ps_func_tb[ps_x] == 0
		 || strcmp(ps_func_tb[ps_x], ps_func) == 0)
			break;
	}
	if (ps_x == sizeof ps_func_tb / sizeof ps_func_tb[0]) {
		error(1, NULL, "Too many postscript functions");
		return 128;
	}

	/* have an index for the string */
	if (strcmp(text, name) == 0) {
		strx = 255;
	} else if (*text == '\0') {
		strx = c_func == 6 ? 255 : 0;
	} else {
		for (strx = 1;
		     strx < sizeof str_tb / sizeof str_tb[0];
		     strx++) {
			if (!str_tb[strx]) {
				if (*text == '"') {
					text++;
					l = strlen(text);
					str_tb[strx] = malloc(l);
					memcpy(str_tb[strx], text, l - 1);
					str_tb[strx][l - 1] = '\0';
				} else {
					str_tb[strx] = strdup(text);
				}
				break;
			}
			if (strcmp(str_tb[strx], text) == 0)
				break;
		}
		if (strx == sizeof str_tb / sizeof str_tb[0]) {
			error(1, NULL, "Too many decoration strings");
			return 128;
		}
	}

	/* set the values */
	if (!dd->name)
		dd->name = name;	/* new decoration */
	dd->func = strncmp(dd->name, "head-", 5) == 0 ? 9 : c_func;
	if (!ps_func_tb[ps_x]) {
		if (ps_func[0] == '-' && ps_func[1] == '\0')
			ps_x = -1;
		else
			ps_func_tb[ps_x] = strdup(ps_func);
	}
	dd->ps_func = ps_x;
	dd->h = h;
	dd->wl = wl;
	dd->wr = wr;
 	dd->strx = strx;

	/* link the start and end of long decorations */
	l = strlen(name);
	if (l == 0)
		return ideco;
	l--;
	if (name[l] == '('
	 || (name[l] == ')' && !strchr(name, '('))) {
		struct deco_def_s *ddo;

		strcpy(name2, name);
		if (name[l] == '(') {
			dd->flags = DE_LDST;
			name2[l] = ')';
		} else {
			dd->flags = DE_LDEN;
			name2[l] = '(';
		}
		for (o = 1, ddo = &deco_def_tb[1]; o < 128; o++, ddo++) {
			if (!ddo->name)
				break;
			if (strcmp(ddo->name, name2) == 0) {
				if (name[l] == '(') {
					ddo->ld_start = ideco;
					dd->ld_end = o;
				} else {
					dd->ld_start = o;
					ddo->ld_end = ideco;
				}
				break;
			}
		}
		if (o >= 128 || !ddo->name)
//fixme: memory leak...
			deco_define(strdup(name2));
	}
	return ideco;
}

/* -- set the duration of the notes under a feathered beam -- */
static void set_feathered_beam(struct SYMBOL *s1,
				int accel)
{
	struct SYMBOL *s, *s2;
	int n, t, tt, d, b, i;
	float a;

	/* search the end of the beam */
	d = s1->dur;
	s2 = NULL;
	n = 1;
	for (s = (struct SYMBOL *) s1->abc_next;
	     s;
	     s = (struct SYMBOL *) s->abc_next) {
		if (s->dur != d
		 || (s->flags & ABC_F_SPACE))
			break;
		s2 = s;
		n++;
	}
	if (!s2)
		return;
	b = d / 2;			/* smallest note duration */
	a = (float) d / (n - 1);		/* delta duration */
	tt = d * n;
	t = 0;
	if (accel) {				/* !beam-accel! */
		for (s = s1, i = n - 1;
		     s != s2;
		     s = (struct SYMBOL *) s->abc_next, i--) {
			d = (int) lroundf(a * i) + b;
			s->dur = d;
			t += d;
		}
	} else {				/* !beam-rall! */
		for (s = s1, i = 0;
		     s != s2;
		     s = (struct SYMBOL *) s->abc_next, i++) {
			d = (int) lroundf(a * i) + b;
			s->dur = d;
			t += d;
		}
	}
	s2->dur = tt - t;
}

/* -- define a decoration -- */
static unsigned char deco_define(char *name)
{
	struct u_deco *d;
	unsigned char ideco;
	int l;

	l = strlen(name);
	for (d = user_deco; d; d = d->next) {
		if (strncmp(d->text, name, l) == 0
		 && d->text[l] == ' ')
			return deco_build(name, d->text);
	}
	for (ideco = 0; ; ideco++) {
		if (!std_deco_tb[ideco])
			break;
		if (strncmp(std_deco_tb[ideco], name, l) == 0
		 && std_deco_tb[ideco][l] == ' ')
			return deco_build(name, std_deco_tb[ideco]);
	}
	return 128;
}

/* -- convert the external deco number to the internal one -- */
static unsigned char deco_intern(unsigned char ideco,
				struct SYMBOL *s)
{
	char *name;

	if (ideco < 128) {
		name = deco[ideco];
		if (!name) {
			error(1, s, "Bad character '%c'", ideco);
			return 0;
		}
	} else {
		name = parse.deco_tb[ideco - 128];
	}
	for (ideco = 1; ideco < 128; ideco++) {
		if (!deco_def_tb[ideco].name) {
			ideco = deco_define(name);
			break;
		}
		if (strcmp(deco_def_tb[ideco].name, name) == 0)
			break;
	}
	if (ideco == 128) {
		if (cfmt.decoerr)
			error(1, s, "Decoration !%s! not defined", name);
		ideco = 0;
	}
	return ideco;
}

/* -- convert the decorations -- */
void deco_cnv(struct decos *dc,
		struct SYMBOL *s,
		struct SYMBOL *prev)
{
	int i, j, m, n;
	struct deco_def_s *dd;
	unsigned char ideco;
	static char must_note_fmt[] = "Deco !%s! must be on a note";

	for (i = dc->n; --i >= 0; ) {
		if ((ideco = dc->tm[i].t) == 0)
			continue;
		ideco = deco_intern(ideco, s);
		dc->tm[i].t = ideco;
		if (ideco == 0)
			continue;
		dd = &deco_def_tb[ideco];
		m = dc->tm[i].m;

		/* special decorations */
		switch (dd->func) {
		case 2:			// arp
			if (m >= 0) {
				error(1, s,
					"!%s! cannot be on a head (function 2)",
					dd->name);
				break;
			}
			/* fall thru */
		case 0:			// near

			/* special case for dotted bars */
			if (dd->func == 0 && s->abc_type == ABC_T_BAR
			 && strcmp(dd->name, "dot") == 0) {
				s->u.bar.dotted = 1;
				break;
			}
			// fall thru
		case 1:			// slide
			if (s->abc_type != ABC_T_NOTE
			 && s->abc_type != ABC_T_REST) {
				error(1, s,
					"!%s! must be on a note or a rest",
					dd->name);
				break;
			}
			continue;
		case 8:			// gliss: move to the upper note of the chord
			if (s->abc_type != ABC_T_NOTE) {
				error(1, s, "!%s! must be on a note",
							dd->name);
				break;
			}
			if (m < 0)
				dc->tm[i].m = s->nhd;
			continue;
		case 9:		// move the alternate head of the chord to the notes
			if (s->abc_type != ABC_T_NOTE
			 && s->abc_type != ABC_T_REST) {
				error(1, s,
					"!%s! must be on a note or a rest",
					dd->name);
				break;
			}
			if (m >= 0) {
				s->u.note.notes[m].invisible = 1;
				continue;
			}

			// apply !head-xx! to each head
			dc->tm[i].m = 0;
			s->u.note.notes[0].invisible = 1;
			n = dc->n;
			for (m = 1; m <= s->nhd; m++) {
				if (n >= MAXDC) {
					error(1, s,
						"Too many decorations");
						break;
				}
				dc->tm[n].t = ideco;
				dc->tm[n++].m = m;
				s->u.note.notes[m].invisible = 1;
			}
			dc->n = n;
			continue;
		default:
			if (dd->name[0] == '8' && dd->name[1] == 'v'
			 && dd->name[4] == '\0') {
				if (dd->name[3] == '(') {
					if (dd->name[2] == 'a')
						curvoice->ottava = -7;
					else if (dd->name[2] == 'b')
						curvoice->ottava = 7;
				} else if (dd->name[3] == ')') {
					if (dd->name[2] == 'a'
					 || dd->name[2] == 'b')
						curvoice->ottava = 0;
				}
			}
			continue;
		case 32:		/* invisible */
			if (m < 0)
				s->flags |= ABC_F_INVIS;
			else
				s->u.note.notes[m].invisible = 1;
			break;
		case 33:		/* beamon */
			s->sflags |= S_BEAM_ON;
			break;
		case 34:		/* trem1..trem4 */
			if (s->abc_type != ABC_T_NOTE
			 || !prev
			 || prev->abc_type != ABC_T_NOTE) {
				error(1, s,
					"!%s! must be on the last of a couple of notes",
					dd->name);
				break;
			}
			s->sflags |= (S_TREM2 | S_BEAM_END);
			s->sflags &= ~S_BEAM_ST;
			prev->sflags |= (S_TREM2 | S_BEAM_ST);
			prev->sflags &= ~S_BEAM_END;
			s->aux = prev->aux = dd->name[4] - '0';
			for (j = 0; j <= s->nhd; j++)
				s->u.note.notes[j].len *= 2;
			for (j = 0; j <= prev->nhd; j++)
				prev->u.note.notes[j].len *= 2;
			break;
		case 35:		/* xstem */
			if (s->abc_type != ABC_T_NOTE) {
				error(1, s, must_note_fmt, dd->name);
				break;
			}
			s->sflags |= S_XSTEM;
			break;
		case 36:		/* beambr1 / beambr2 */
			if (s->abc_type != ABC_T_NOTE) {
				error(1, s, must_note_fmt, dd->name);
				break;
			}
			s->sflags |= dd->name[6] == '1' ?
					S_BEAM_BR1 : S_BEAM_BR2;
			break;
		case 37:		/* rbstop */
			s->sflags |= S_RBSTOP;
			break;
		case 38:		/* /, // and /// = tremolo */
			if (s->abc_type != ABC_T_NOTE) {
				error(1, s, must_note_fmt, dd->name);
				break;
			}
			s->sflags |= S_TREM1;
			s->aux = strlen(dd->name);	/* 1, 2 or 3 */
			break;
		case 39:		/* beam-accel/beam-rall */
			if (s->abc_type != ABC_T_NOTE) {
				error(1, s, must_note_fmt, dd->name);
				break;
			}
			s->sflags |= S_FEATHERED_BEAM;
			set_feathered_beam(s, dd->name[5] == 'a');
			break;
		case 40:		/* stemless */
			if (s->abc_type != ABC_T_NOTE) {
				error(1, s, must_note_fmt, dd->name);
				break;
			}
			s->flags |= ABC_F_STEMLESS;
			break;
		case 41:		/* rbend */
			s->flags |= ABC_F_RBSTOP;	/* with bracket end */
			s->sflags |= S_RBSTOP;
			break;
		}
		dc->tm[i].t = 0;	/* already treated */
	}
}

/* -- update the x position of a decoration -- */
void deco_update(struct SYMBOL *s, float dx)
{
	struct deco_elt *de;

	for (de = deco_head; de; de = de->next) {
		if (de->s == s) {
			while (de && de->s == s) {
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
	struct decos *dc;
	int i;
	float wl;

	wl = 0;
	if (s->type == BAR)
		dc = &s->u.bar.dc;
	else
		dc = &s->u.note.dc;
	for (i = dc->n; --i >= 0; ) {
		struct deco_def_s *dd;

		dd =  &deco_def_tb[dc->tm[i].t];
		switch (dd->func) {
		case 1:			/* slide */
			if (wl < 7)
				wl = 7;
			break;
		case 2:			/* arpeggio */
			if (wl < 14)
				wl = 14;
			break;
		}
	}
	if (wl != 0 && s->prev && s->prev->type == BAR)
		wl -= 3;
	return wl;
}

/* -- draw the decorations -- */
/* (the staves are defined) */
void draw_all_deco(void)
{
	struct deco_elt *de;
	struct deco_def_s *dd;
	struct SYMBOL *s;
	int f, staff, l;
	char *gl, *p;
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

	for (de = deco_head; de; de = de->next) {
		if (de->t == 0)			// deleted
			continue;
		dd = &deco_def_tb[de->t];
//		if ((dd->flags & DE_LDST) && dd->ld_end != 0)
		if (dd->ld_end != 0)
			continue;		// start of long decoration
		if ((f = dd->ps_func) < 0)
			continue;		// old behaviour

		// handle the stem direction
		gl = ps_func_tb[f];		// glyph name(s)
		p = strchr(gl, '/');
		if (p) {
			if (de->s->stem >= 0) {
				l = (int) (p - gl);
			} else {
				gl = p + 1;
				l = strlen(gl);
			}
		} else {
			l = strlen(gl);
		}

		s = de->s;
// David Lacroix - 16-12-28
//		set_color(s->color);

		// no scale if staff decoration
		if (f_staff & (1 << dd->func))
			set_sscale(-1);
		else
			set_scale(s);

		staff = de->staff;
		x = de->x;
//		y = de->y + staff_tb[staff].y / staff_tb[staff].staffscale;
		y = de->y + staff_tb[staff].y;

		/* update the coordinates if head decoration */
		if (de->m >= 0) {
			x += s->u.note.notes[de->m].shhd *
						staff_tb[staff].staffscale;

		/* center the dynamic marks between two staves */
/*fixme: KO when deco on other voice and same direction*/
		} else if ((f_staff & (1 << dd->func))
		 && !cfmt.dynalign
		 && (((de->flags & DE_UP) && staff > 0)
		  || (!(de->flags & DE_UP) && staff < nstaff))) {
			if (de->flags & DE_UP)
				ym = ymid[--staff];
			else
				ym = ymid[staff++];
			ym -= dd->h * 0.5;
			if (((de->flags & DE_UP) && y < ym)
			 || (!(de->flags & DE_UP) && y > ym)) {
//				if (s->staff > staff) {
//					while (s->staff != staff)
//						s = s->ts_prev;
//				} else if (s->staff < staff) {
//					while (s->staff != staff)
//						s = s->ts_next;
//				}
				y2 = y_get(staff, !(de->flags & DE_UP),
							de->x, de->val)
					+ staff_tb[staff].y;
				if (de->flags & DE_UP)
					y2 -= dd->h;
				if (((de->flags & DE_UP) && y2 > ym)
				 || (!(de->flags & DE_UP) && y2 < ym)) {
					y = ym;
					y_set(staff, de->flags & DE_UP,
							de->x, de->val,
						  ((de->flags & DE_UP) ? y + dd->h : y)
						- staff_tb[staff].y);
				}
			}
		}

		set_defl(de->defl);
		if (de->flags & DE_VAL) {
			if (dd->func != 2
			 || voice_tb[s->voice].scale != 1)
				putx(de->val);
			else
				putf(de->val);
		}
		if (dd->strx != 0) {
			char *p, *q;

			y += dd->h * 0.2;		// font descent
			if (dd->strx == 255) {
				p = dd->name;
			} else {
				p = str_tb[dd->strx];
				if (*p == '@') {
					float dx, dy;
					int n;

					if (sscanf(p, "@%f,%f%n",
							&dx, &dy, &n) == 2) {
						x += dx;
						y += dy;
						p += n;
					}
					str_font(ANNOTATIONFONT);
					outft = -1;	// force font selection
					putxy(x, y);
					a2b("M");
					put_str(p, A_LEFT);
					continue;
				}
			}
			a2b("(");
			q = p;
			while (*p != '\0') {
				if (*p == '(' || *p == ')') {
					if (p != q)
						a2b("%.*s", (int) (p - q), q);
					a2b("\\");
					q = p;
				}
				p++;
			}
			if (p != q)
				a2b("%.*s", (int) (p - q), q);
			a2b(")");
		}
		putxy(x, y);
		if (de->flags & DE_LDEN) {
//			if (de->start) {
				x = de->start->x;
				y = de->start->y + staff_tb[de->start->staff].y;
//			} else {
//				x = first_note->x - first_note->wl - 4;
//			}
			if (x > de->x - 20)
				x = de->x - 20;
			putxy(x, y);
		}
		if (de->flags & DE_INV)
			a2b("gsave 1 -1 scale neg %.*s grestore\n",
					l, gl);
		else
			a2b("%.*s\n", l, gl);
	}
	set_sscale(-1);			/* restore the scale */
	set_color(0);
}

/* -- create the deco elements, and treat the near ones -- */
static void deco_create(struct SYMBOL *s,
			struct decos *dc)
{
	int k, m, posit;
	unsigned char ideco;
	struct deco_def_s *dd;
	struct deco_elt *de;

/*fixme:pb with decorations above the staff*/
	for (k = 0; k < dc->n; k++) {
		m = dc->tm[k].m;
		if ((ideco = dc->tm[k].t) == 0)
			continue;
		dd = &deco_def_tb[ideco];
		/* check if hidden */
		switch (dd->func) {
		default:
			posit = 0;
			break;
		case 3:				/* d_upstaff */
		case 4:
//fixme:trill does not work yet
		case 5:				/* trill */
			posit = s->posit.orn;
			break;
		case 6:				/* d_pf */
			posit = s->posit.vol;
			break;
		case 7:				/* d_cresc */
			posit = s->posit.dyn;
			break;
		}
		if (posit == SL_HIDDEN) {
			dc->tm[k].t = 0;
			continue;
		}

		de = (struct deco_elt *) getarena(sizeof *de);
		memset(de, 0, sizeof *de);
		de->prev = deco_tail;
		if (!deco_tail)
			deco_head = de;
		else
			deco_tail->next = de;
		deco_tail = de;
		de->s = s;
		de->t = dd - deco_def_tb;
		de->staff = s->staff;
		de->m = m;
//		if (s->flags & ABC_F_GRACE)
//			de->flags = DE_GRACE;
		if (dd->flags & DE_LDST) {
			de->flags |= DE_LDST;
		} else if (dd->flags & DE_LDEN) {
			de->flags |= DE_LDEN;
			de->defl = DEF_NOST;
		}
		if (cfmt.setdefl && s->stem >= 0)
			de->defl |= DEF_STEMUP;

		/* set the coordinates of the decoration */
		if (m >= 0) {			/* head decoration */
			de->x = s->x;
			de->y = 3 * (s->pits[m] - 18);
//			if (dd->func == 9)	/* alternate note head */
//				s->u.note.notes[m].invisible = 1;
			continue;
		}
		if (!(f_near & (1 << dd->func))) /* if not near the note */
			continue;
		func_tb[dd->func](de);
	}
}

// link the long decorations
static void ll_deco(void)
{
	struct deco_elt *de, *de2, *tail;
	struct deco_def_s *dd;
	int t, voice, staff;

	// add ending decorations
	tail = deco_tail;
	if (!tail)
		return;
	for (de = deco_head; ; de = de->next) {
		t = de->t;
		dd = &deco_def_tb[t];

		if (!(de->flags & DE_LDST)) {
			if (de == tail)
				break;
			continue;
		}
		t = dd->ld_end;
#if 0
		if (t == 0) {		// if long deco has no end
			int l;		// create one
			char *name;

			l = strlen(dd->name);
			name = getarena(l + 1);
			strcpy(name, dd->name);
			name[l - 1] = ')';
			t = get_deco(name);
			if (t != 128) {
				struct deco_def_s *dd2;

				dd2 = &deco_def_tb[t];
				dd2->name = name;
				dd2->func = dd->func;
				dd2->ps_func = dd->ps_func;
				dd2->h = dd->h;
				dd->ld_end = t;
			} else {
				t = 0;
			}
		}
#endif
		voice = de->s->voice;	/* search later in the voice */
		for (de2 = de->next; de2; de2 = de2->next)
			if (!de2->start
			 && de2->t == t && de2->s->voice == voice)
				break;
		if (!de2) {		/* no end, search in the staff */
			staff = de->s->staff;
			for (de2 = de->next; de2; de2 = de2->next)
				if (!de2->start
				 && de2->t == t && de2->s->staff == staff)
					break;
		}
		if (!de2) {		/* no end, insert one */
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
			de2->m = de->m;
		}
		de2->start = de;
		de2->defl &= ~DEF_NOST;
		if (dd->func == 8)
			d_gliss(de2);
		if (de == tail)
			break;
	}

	// add starting decorations
	for (de2 = deco_head; ; de2 = de2->next) {
		if (!(de2->flags & DE_LDEN) // not the end of long decoration
		 || de2->start) {		// start already found
			if (de2 == tail)
				break;
			continue;
		}
		t = de2->t;
		dd = &deco_def_tb[t];
		de = (struct deco_elt *) getarena(sizeof *de);
		memset(de, 0, sizeof *de);
		de->prev = deco_tail;
		deco_tail->next = de;
		deco_tail = de;
		de->s = prev_scut(de2->s);
		de->t = dd->ld_start;
		de->flags = DE_LDST;
		de->defl = DEF_NOST;
		de->x = de->s->x;	//de2->s->x - de2->s->wl - 4;
		de->y = de2->s->y;
		de->m = de2->m;
		de2->start = de;
//		de2->defl &= ~DEF_NOST;
		if (de2 == tail)
			break;
	}
}

/* -- create the decorations and treat the ones near the notes -- */
/* (the staves are not yet defined) */
/* this function must be called first as it builds the deco element table */
void draw_deco_near(void)
{
	struct SYMBOL *s, *g;
	struct decos *dc;
//	struct SYMBOL *first;

	deco_head = deco_tail = NULL;
//	first = NULL;
	for (s = tsfirst; s; s = s->ts_next) {
		switch (s->type) {
		case BAR:
		case MREST:
			if (s->u.bar.dc.n == 0)
				continue;
			dc = &s->u.bar.dc;
			break;
		case NOTEREST:
		case SPACE:
//			if (!first)
//				first = s;
			if (s->u.note.dc.n == 0)
				continue;
			dc = &s->u.note.dc;
			break;
		case GRACE:
			for (g = s->extra; g; g = g->next) {
				if (g->abc_type != ABC_T_NOTE
				 || g->u.note.dc.n == 0)
					continue;
				dc = &g->u.note.dc;
				deco_create(g, dc);
			}
			/* fall thru */
		default:
			continue;
		}
		deco_create(s, dc);
	}
//	first_note = first;

	ll_deco();			// link the long decorations
}

/* -- draw the decorations tied to a note -- */
/* (the staves are not yet defined) */
void draw_deco_note(void)
{
	struct deco_elt *de;
	struct deco_def_s *dd;
	int f, t;

	for (de = deco_head; de; de = de->next) {
		t = de->t;
		dd = &deco_def_tb[t];
		f = dd->func;
		if (!(f_note & (1 << f))	/* if not tied to the note */
		 || de->m >= 0)			/* or head decoration */
			continue;
//		if (f == 4)
//			de->flags |= DE_BELOW;
		func_tb[f](de);
	}
}

/* draw the repeat brackets */
static void draw_repbra(struct VOICE_S *p_voice)
{
	struct SYMBOL *s, *s1, *s2, *first_repeat;
	int i;
	float x, y, y2, w;

	/* search the max y offset */
	y = staff_tb[p_voice->staff].topbar + 6 + 20;
	first_repeat = 0;
	for (s = p_voice->sym->next; s; s = s->next) {
		if (s->type != BAR)
			continue;
		if (!(s->sflags & S_RBSTART)
		 || (s->sflags & S_NOREPBRA))
			continue;
/*fixme: line cut on repeat!*/
		if (!s->next)
			break;
		if (!first_repeat)
			first_repeat = s;
		s1 = s;
		for (;;) {
			if (!s->next)
				break;
			s = s->next;
			if (s->sflags & S_RBSTOP)
				break;
		}
		y2 = y_get(p_voice->staff, 1, s1->x, s->x - s1->x);
		if (y < y2)
			y = y2;

		/* have room for the repeat numbers */
		if (s1->gch) {
			w = s1->gch->w;
			y2 = y_get(p_voice->staff, 1, s1->x + 4, w);
			y2 += cfmt.font_tb[REPEATFONT].size + 2;
			if (y < y2)
				y = y2;
		}
		if (s->sflags & S_RBSTART)
			s = s->prev;
	}

	/* draw the repeat indications */
	s = first_repeat;
	if (!s)
		return;
//	set_sscale(p_voice->staff);
//temporary
	set_sscale(-1);
	set_font(REPEATFONT);
	for ( ; s; s = s->next) {
		char *p;

		if (!(s->sflags & S_RBSTART)
		 || (s->sflags & S_NOREPBRA))
			continue;
		s1 = s;
		for (;;) {
			if (!s->next)
				break;
			s = s->next;
			if (s->sflags & S_RBSTOP)
				break;
		}
		s2 = s;
		if (s1 == s2)
			break;
		x = s1->x;
		if ((s1->u.bar.type & 0x0f) == B_COL)
			x -= 4;
		if (s2->type != BAR) {
			if (s2->sflags & S_RBSTOP)
				w = 0;
			else
				w = s2->x - realwidth + 4;
		} else if (((s2->u.bar.type & 0xf0)	/* if complex bar */
			 && s2->u.bar.type != (B_OBRA | B_CBRA))
			|| s2->u.bar.type == B_CBRA) {
			if (s2->u.bar.type == B_CBRA)
				s2->flags |= ABC_F_INVIS;
/*fixme:%%staves: cursys moved?*/
			if (s1->staff > 0
			 && !(cursys->staff[s1->staff - 1].flags & STOP_BAR))
				w = s2->wl;
			else if ((s2->u.bar.type & 0x0f) == B_COL)
				w = 12;
			else if (!(s2->sflags & S_RRBAR))
//				|| s2->u.bar.type == B_CBRA)
				w = 0;		/* explicit repeat end */
			else
				w = 8;
		} else {
			w = (s2->sflags & S_RBSTOP) ? 0 : 8;
		}
		w = s2->x - x - w;
		p = s1->text;
		if (!p)
			p = "";
		if (!s2->next			/* 2nd ending at end of line */
		 && !(s2->sflags & S_RBSTOP)
		 && (p_voice->bar_start == 0)) {
			p_voice->bar_start = B_OBRA | 0x1000;	/* S_RBSTART */
			p_voice->bar_repeat = 1;	/* continue on next line */
		}
		if (s1->flags & ABC_F_RBSTART)
			i = (s2->flags & ABC_F_RBSTOP) ? 3 : 1;
		else
			i = (s2->flags & ABC_F_RBSTOP) ? 2 : 0;
		a2b("(%s)-%.1f %d ",
			p, cfmt.font_tb[REPEATFONT].size * 0.8 + 1, i);
		putx(w);
		putxy(x, y * staff_tb[s1->staff].staffscale);
		a2b("yns%d repbra\n", s1->staff);
		y_set(s1->staff, 1, x, w, y + 2);
		if (s->u.bar.repeat_bar)
			s = s->prev;
	}
}

/* -- draw the music elements tied to the staff -- */
/* (the staves are not yet defined) */
void draw_deco_staff(void)
{
	struct SYMBOL *s, *first_gchord;
	struct VOICE_S *p_voice;
	float y, w;
	struct deco_elt *de;
	struct {
		float ymin, ymax;
	} minmax[MAXSTAFF];

//	outft = -1;				/* force font output */

	/* search the vertical offset for the guitar chords */
	memset(minmax, 0, sizeof minmax);
	first_gchord = 0;
	for (s = tsfirst; s; s = s->ts_next) {
		struct gch *gch, *gch2;
		int ix;

		gch = s->gch;
		if (!gch)
			continue;
		if (!first_gchord)
			first_gchord = s;
		gch2 = NULL;
		for (ix = 0; ix < MAXGCH; ix++, gch++) {
			if (gch->type == '\0')
				break;
			if (gch->type != 'g')
				continue;
			gch2 = gch;	/* guitar chord closest to the staff */
			if (gch->y < 0)
				break;
		}
		if (gch2) {
			w = gch2->w;
			if (gch2->y >= 0) {
				y = y_get(s->staff, 1, s->x, w);
				if (y > minmax[s->staff].ymax)
					minmax[s->staff].ymax = y;
			} else {
				y = y_get(s->staff, 0, s->x, w);
				if (y < minmax[s->staff].ymin)
					minmax[s->staff].ymin = y;
			}
		}
	}

	/* draw the guitar chords if any */
	if (first_gchord) {
		int i;

		for (i = 0; i <= nstaff; i++) {
			int top, bot;

			bot = staff_tb[i].botbar;
			if (minmax[i].ymin > bot - 6)
				minmax[i].ymin = bot - 6;
			top = staff_tb[i].topbar;
			if (minmax[i].ymax < top + 6)
				minmax[i].ymax = top + 6;
		}
		set_sscale(-1);		/* restore the scale parameters */
		for (s = first_gchord; s; s = s->ts_next) {
			if (!s->gch)
				continue;
			switch (s->type) {
			case NOTEREST:
			case SPACE:
			case MREST:
				break;
			case BAR:
				if (!s->u.bar.repeat_bar)
					break;
			default:
				continue;
			}
			draw_gchord(s, minmax[s->staff].ymin,
					minmax[s->staff].ymax);
		}
	}

	/* draw the repeat brackets */
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		if (p_voice->second || !p_voice->sym)
			continue;
		draw_repbra(p_voice);
	}

	/* create the decorations tied to the staves */
	memset(minmax, 0, sizeof minmax);
	for (de = deco_head; de; de = de->next) {
		struct deco_def_s *dd;

		dd = &deco_def_tb[de->t];
		if (!(f_staff & (1 << dd->func)) /* if not tied to the staff */
		 || de->m >= 0)			/* or chord decoration */
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
	for (de = deco_head; de; de = de->next) {
		struct deco_def_s *dd;

		dd = &deco_def_tb[de->t];
		if (dd->ps_func < 0
		 || !(f_staff & (1 << dd->func)))
			continue;
		if (cfmt.dynalign) {
			if (de->flags & DE_UP)
				y = minmax[de->staff].ymax;
			else
				y = minmax[de->staff].ymin;
			de->y = y;
		} else {
			y = de->y;
		}
		if (de->flags & DE_UP)
			y += dd->h;
		y_set(de->staff, de->flags & DE_UP, de->x, de->val, y);
	}
}

/* -- draw the guitar chords and annotations -- */
/* (the staves are not yet defined) */
static void draw_gchord(struct SYMBOL *s,
			float gchy_min, float gchy_max)
{
	struct gch *gch, *gch2;
	int action, ix, box, yav;
	float x, y, w, h, y_above, y_below;
	float hbox, xboxl, yboxh, yboxl, expdx;

	/* adjust the vertical offset according to the guitar chords */
//fixme: w may be too small
	w = s->gch->w;
#if 1
	y_above = y_get(s->staff, 1, s->x - 2, w);
	y_below = y_get(s->staff, 0, s->x - 2, w);
#else
	y_above = y_get(s->staff, 1, s->x - 2, w) + 2;
	y_below = y_get(s->staff, 0, s->x - 2, w) - 2;
#endif
	gch2 = NULL;
	yav = ((s->pits[s->nhd] + s->pits[0]) / 2 - 18) * 3;
	for (ix = 0, gch = s->gch; ix < MAXGCH; ix++, gch++) {
		if (gch->type == '\0')
			break;
		if (gch->type != 'g')
			continue;
		gch2 = gch;		/* guitar chord closest to the staff */
		if (gch->y < 0)
			break;
	}
	if (gch2) {
		if (gch2->y >= 0) {
			if (y_above < gchy_max)
				y_above = gchy_max;
		} else {
			if (y_below > gchy_min)
				y_below = gchy_min;
		}
	}

	str_font(s->gch->font);
	set_font(s->gch->font);			/* needed if scaled staff */
//	set_sscale(s->staff);
//temporary
	set_sscale(-1);
//	action = A_GCHORD;
	xboxl = s->x;
	yboxh = -100;
	yboxl = 100;
	box = 0;
	expdx = 0;
	for (ix = 0, gch = s->gch; ix < MAXGCH; ix++, gch++) {
		if (gch->type == '\0')
			break;
		h = cfmt.font_tb[gch->font].size;
		str_font(gch->font);
		tex_str(s->text + gch->idx);
		w = gch->w;
		if (gch->type == 'g') {			/* guitar chord */
			if (!strchr(tex_buf, '\t')) {
				action = A_GCHORD;
			} else {
				struct SYMBOL *next;
				char *r;
				int n;

				/* some TAB: expand the guitar chord */
				x = realwidth;
				next = s->next;
				while (next) {
					switch (next->type) {
					default:
						next = next->next;
						continue;
					case NOTEREST:
					case BAR:
						x = next->x;
						break;
					}
					break;
				}
				n = 0;
				r = tex_buf;
				for (;;) {
					n++;
					r = strchr(r, '\t');
					if (!r)
						break;
					r++;
				}
				expdx = (x - s->x - w) / n;
				action = A_GCHEXP;
			}
		} else {
			action = A_ANNOT;
		}
		x = s->x + gch->x;
		switch (gch->type) {
		case '_':			/* below */
			y = gch->y + y_below;
			y_set(s->staff, 0, x, w, y - h * 0.2 - 2);
			break;
		case '^':			/* above */
			y = gch->y + y_above;
			y_set(s->staff, 1, x, w, y + h * 0.8 + 2);
			break;
		default:			/* guitar chord */
			hbox = gch->box ? 3 : 2;
			if (gch->y >= 0) {
				y = gch->y + y_above;
				y_set(s->staff, 1, x, w, y + h + hbox);
			} else {
				y = gch->y + y_below;
				y_set(s->staff, 0, x, w, y - hbox);
			}
			if (gch->box) {
				if (xboxl > x)
					xboxl = x;
				if (yboxl > y)
					yboxl = y;
				if (yboxh < y + h)
					yboxh = y + h;
				box++;
			}
			break;
		case '<':			/* left */
/*fixme: what symbol space?*/
			if (s->u.note.notes[0].acc)
				x -= s->u.note.notes[0].shac;
			y = yav + gch->y;
			break;
		case '>':			/* right */
			x += s->xmx;
			if (s->dots > 0)
				x += 1.5 + 3.5 * s->dots;
			y = yav + gch->y;
			break;
		case '@':			/* absolute */
			y = yav + gch->y;
			break;
		}
		putxy(x, (y  + h * 0.2) *		/* (descent) */
				staff_tb[s->staff].staffscale);
		a2b("yns%d M ", s->staff);
		if (action == A_GCHEXP)
			a2b("%.2f ", expdx);
		str_out(tex_buf, action);
		if (gch->type == 'g' && box > 0) {
			if (box == 1)
				a2b(" boxend");
			else
				a2b(" boxmark");
		}
		a2b("\n");
	}

	/* draw the box around the guitar chords */
	if (box) {
		xboxl -= 2;
		putxy(xboxl, (yboxl - 1) * staff_tb[s->staff].staffscale);
		a2b("yns%d %.1f boxdraw\n", s->staff, yboxh - yboxl + 3);
	}
}

/* -- draw the measure bar numbers -- */
void draw_measnb(void)
{
	struct SYMBOL *s;
	struct SYSTEM *sy;
	char *showm;
	int any_nb, staff, bar_num;
	float x, y, w, font_size;

	showm = cfmt.measurebox ? "showb" : "show";
	any_nb = 0;

	/* search the first staff */
	sy = cursys;
	for (staff = 0; staff <= nstaff; staff++) {
		if (!sy->staff[staff].empty)
			break;
	}
	if (staff > nstaff)
		return;				/* no visible staff */
//fixme: must use the scale, otherwise bad y offset (y0 empty)
	set_sscale(staff);

	/* leave the measure numbers as unscaled */
	font_size = cfmt.font_tb[MEASUREFONT].size;
	cfmt.font_tb[MEASUREFONT].size /= staff_tb[staff].staffscale;

	s = tsfirst;				/* clef */
	bar_num = nbar;
	if (bar_num > 1) {
		if (cfmt.measurenb == 0) {
			set_font(MEASUREFONT);
			any_nb = 1;
			x = 0;
			w = 20;
			y = y_get(staff, 1, x, w);
			if (y < staff_tb[staff].topbar + 14)
				y = staff_tb[staff].topbar + 14;
			a2b("0 ");
			puty(y);
			a2b("y%d M(%d)%s", staff, bar_num, showm);
			y_set(staff, 1, x, w, y + cfmt.font_tb[MEASUREFONT].size + 2);
		} else if (bar_num % cfmt.measurenb == 0) {
			for ( ; ; s = s->ts_next) {
				switch (s->type) {
				case TIMESIG:
				case CLEF:
				case KEYSIG:
				case FMTCHG:
				case STBRK:
					continue;
				}
				break;
			}
			if (s->prev && s->prev->type != CLEF)
				s = s->prev;
			x = s->x - s->wl;
			set_font(MEASUREFONT);
			any_nb = 1;
			w = cwid('0') * cfmt.font_tb[MEASUREFONT].swfac;
			if (bar_num >= 10) {
				if (bar_num >= 100)
					w *= 3;
				else
					w *= 2;
			}
			if (cfmt.measurebox)
				w += 4;
			y = y_get(staff, 1, x, w);
			if (y < staff_tb[staff].topbar + 6)
				y = staff_tb[staff].topbar + 6;
			y += 2;
			putxy(x, y);
			a2b("y%d M(%d)%s", staff, bar_num, showm);
			y += cfmt.font_tb[MEASUREFONT].size;
			y_set(staff, 1, x, w, y);
			s->ymx = y;
		}
	}

	for ( ; s; s = s->ts_next) {
		if (s->sflags & S_NEW_SY) {
			sy = sy->next;
			for (staff = 0; staff < nstaff; staff++) {
				if (!sy->staff[staff].empty)
					break;
			}
			set_sscale(staff);
		}
		if (s->type != BAR || s->aux <= 0)
			continue;
		bar_num = s->aux;
		if (cfmt.measurenb == 0
		 || (bar_num % cfmt.measurenb) != 0
		 || !s->next)
			continue;
		if (!any_nb) {
			any_nb = 1;
			set_font(MEASUREFONT);
		}
		w = cwid('0') * cfmt.font_tb[MEASUREFONT].swfac;
		if (bar_num >= 10) {
			if (bar_num >= 100)
				w *= 3;
			else
				w *= 2;
		}
		if (cfmt.measurebox)
			w += 4;
		x = s->x - w * 0.4;
		y = y_get(staff, 1, x, w);
		if (y < staff_tb[staff].topbar + 6)
			y = staff_tb[staff].topbar + 6;
		if (s->next->abc_type == ABC_T_NOTE) {
			if (s->next->stem > 0) {
				if (y < s->next->ys - cfmt.font_tb[MEASUREFONT].size)
					y = s->next->ys - cfmt.font_tb[MEASUREFONT].size;
			} else {
				if (y < s->next->y)
					y = s->next->y;
			}
		}
		y += 2;
		a2b(" ");
		putxy(x, y);
		a2b("y%d M(%d)%s", staff, bar_num, showm);
		y += cfmt.font_tb[MEASUREFONT].size;
		y_set(staff, 1, x, w, y);
		s->ymx = y;
	}
	if (any_nb)
		a2b("\n");
	nbar = bar_num;

	cfmt.font_tb[MEASUREFONT].size = font_size;
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

	a2b("gsave %.2f dup scale 8 3 RM currentpoint ", sc);
	identify_note(s, len, &head, &dots, &flags);
	switch (head) {
	case H_OVAL:
		a2b("HD");
		break;
	case H_EMPTY:
		a2b("Hd");
		break;
	default:
		a2b("hd");
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
			a2b(" %.1f 0 dt", dotx);
			dx = dotx;
			dotx += 3.5;
		}
	}
	if (len < SEMIBREVE) {
		if (flags <= 0) {
			a2b(" %d su", STEM);
		} else {
			a2b(" %d %d sfu", flags, STEM);
			if (dx < 6)
				dx = 6;
		}
	}
	a2b(" grestore %.1f 0 RM\n", (dx + 15) * sc);
}

/* -- return the tempo width -- */
float tempo_width(struct SYMBOL *s)
{
	unsigned i;
	float w;

	w = 0;
	if (s->u.tempo.str1)
		w += tex_str(s->u.tempo.str1);
	if (s->u.tempo.beats[0] != 0) {
		if (s->u.tempo.circa)
			w += tex_str("ca. ");
		i = 1;
		while (i < sizeof s->u.tempo.beats
				/ sizeof s->u.tempo.beats[0]
		       && s->u.tempo.beats[i] != 0) {
			w += 10;
			i++;
		}
		w += 6 + cwid(' ') * cfmt.font_tb[TEMPOFONT].swfac * 6
			+ 10 + 10;
	}
	if (s->u.tempo.str2)
		w += tex_str(s->u.tempo.str2);
	return w;
}

/* - output a tempo --*/
void write_tempo(struct SYMBOL *s,
		 int beat,
		 float sc)
{
	unsigned i;
	char tmp[16];

	if (s->u.tempo.str1)
		put_str(s->u.tempo.str1, A_LEFT);
	if (s->u.tempo.beats[0] != 0) {
		sc *= 0.7 * cfmt.font_tb[TEMPOFONT].size / 15.0;
						/*fixme: 15.0 = initial tempofont*/
		for (i = 0;
		     i < sizeof s->u.tempo.beats
				/ sizeof s->u.tempo.beats[0]
			&& s->u.tempo.beats[i] != 0;
		     i++) {
			draw_notempo(s, s->u.tempo.beats[i], sc);
		}
		put_str("= ", A_LEFT);
		if (s->u.tempo.tempo != 0) {
			if (s->u.tempo.circa)
				put_str("ca. ", A_LEFT);
			snprintf(tmp, sizeof tmp, "%d", s->u.tempo.tempo);
			put_str(tmp, A_LEFT);
		} else {
			draw_notempo(s, s->u.tempo.new_beat, sc);
		}
	}
	if (s->u.tempo.str2)
		put_str(s->u.tempo.str2, A_LEFT);
}

/* -- draw the parts and the tempo information -- */
/* (the staves are being defined) */
float draw_partempo(int staff, float top)
{
	struct SYMBOL *s, *g;
	int beat, dosh, shift;
	int some_part, some_tempo;
	float h, ht, w, x, y, ymin, dy;

	/* put the tempo indication at top */
	dy = 0;
	ht = 0;
	some_part = some_tempo = 0;

	/* get the minimal y offset */
	ymin = staff_tb[staff].topbar + 12;
	dosh = 0;
	shift = 1;
	x = 0;
	for (s = tsfirst; s; s = s->ts_next) {
		g = s->extra;
		if (!g)
			continue;
		for ( ; g; g = g->next)
			if (g->type == TEMPO)
				break;
		if (!g)
			continue;
		if (!some_tempo) {
			some_tempo = 1;
			str_font(TEMPOFONT);
		}
		w = tempo_width(g);
		y = y_get(staff, 1, s->x - 5, w) + 2;
		if (y > ymin)
			ymin = y;
		if (x >= s->x - 5 && !(dosh & (shift >> 1)))
			dosh |= shift;
		shift <<= 1;
		x = s->x - 5 + w;
	}
	if (some_tempo) {
		ht = cfmt.font_tb[TEMPOFONT].size + 2 + 2;
		y = 2 - ht;
		h = y - ht;
		if (dosh != 0)
			ht *= 2;
		if (top < ymin + ht)
			dy = ymin + ht - top;

		/* draw the tempo indications */
		str_font(TEMPOFONT);
		beat = 0;
		for (s = tsfirst; s; s = s->ts_next) {
			if (!(s->sflags & S_SEQST))
				continue;
			if (s->type == TIMESIG)
				beat = get_beat(&s->u.meter);
			g = s->extra;
//			if (!g)
//				continue;
			for ( ; g; g = g->next)
				if (g->type == TEMPO)
					break;
			if (!g)
				continue;

			/*fixme: cf left shift (-5)*/
			a2b("%.1f %.1f M ", s->x - 5,
					(dosh & 1) ? h : y);
			dosh >>= 1;
			write_tempo(g, beat, 1);
		}
	}

	/* then, put the parts */
/*fixme: should reduce if parts don't overlap tempo...*/
	ymin = staff_tb[staff].topbar + 14;
	for (s = tsfirst; s; s = s->ts_next) {
		g = s->extra;
		if (!g)
			continue;
		for (; g; g = g->next)
			if (g->type == PART)
				break;
		if (!g)
			continue;
		if (!some_part) {
			some_part = 1;
			str_font(PARTSFONT);
		}
		w = tex_str(&g->text[2]);
		y = y_get(staff, 1, s->x - 10, w + 3) + 5;
		if (ymin < y)
			ymin = y;
	}
	if (!some_part)
		goto out;

	h = cfmt.font_tb[PARTSFONT].size + 2 + 2;
						/* + cfmt.partsspace; ?? */
	if (top < ymin + h + ht)
		dy = ymin + h + ht - top;

	set_font(PARTSFONT);
	for (s = tsfirst; s; s = s->ts_next) {
		g = s->extra;
		if (!g)
			continue;
		for (; g; g = g->next)
			if (g->type == PART)
				break;
		if (!g)
			continue;
//		w = tex_str(&g->text[2]);
		a2b("%.1f %.1f M", s->x - 10, 2 - ht - h);
		tex_str(&g->text[2]);
		str_out(tex_buf, A_LEFT);
		if (cfmt.partsbox)
			a2b(" %.1f %.1f %.1f boxend boxdraw",
				s->x - 10 - 2, 2 - ht - h - 4, h);
//			a2b(" %.1f %.1f %.1f %.1f box",
//				s->x - 10 - 2, 2 - ht - h - 4,
//				w + 4, h);
		a2b("\n");
	}
out:
	return dy;
}

/* -- initialize the default decorations -- */
void init_deco(void)
{
	memset(&deco, 0, sizeof deco);

	/* standard */
	deco['.'] = "dot";
#ifdef DECO_IS_ROLL
	deco['~'] = "roll";
#endif
	deco['H'] = "fermata";
	deco['L'] = "emphasis";
	deco['M'] = "lowermordent";
	deco['O'] = "coda";
	deco['P'] = "uppermordent";
	deco['S'] = "segno";
	deco['T'] = "trill";
	deco['u'] = "upbow";
	deco['v'] = "downbow";

	/* non-standard */
#ifndef DECO_IS_ROLL
	deco['~'] = "gmark";
#endif
	deco['J'] = "slide";
	deco['R'] = "roll";
}

/* reset the decoration table at start of a new tune */
void reset_deco(void)
{
//	struct deco_def_s *dd;
//	int ideco;
//
//	for (ideco = 1, dd = &deco_def_tb[1]; ideco < 128; ideco++, dd++) {
//		if (!dd->name)
//			break;
//		free(dd->name);
//	}
	memset(deco_def_tb, 0, sizeof deco_def_tb);
}

/* -- set the decoration flags -- */
void set_defl(int new_defl)
{
	if (defl == new_defl)
		return;
	defl = new_defl;
	a2b("/defl %d def ", new_defl);
}
