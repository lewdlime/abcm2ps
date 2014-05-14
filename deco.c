/*
 * Decoration handling.
 *
 * This file is part of abcm2ps.
 *
 * Copyright (C) 2000-2014, Jean-François Moine.
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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#ifdef WIN32
#define lroundf(x) ((long) ((x) + 0.5))
#endif

#include "abc2ps.h"

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
	unsigned char strx;	/* string index - 255=deco name */
	unsigned char ld_end;	/* index of end of long decoration */
	unsigned char dum;
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
	"turn 3 turn 10 0 5",
	"wedge 3 wedge 8 1 1",
	"turnx 3 turnx 10 0 5",
	"trill( 5 - 8 0 0",
	"trill) 5 ltr 8 0 0",
	"snap 3 snap 14 3 3",
	"thumb 3 thumb 14 2 2",
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
	"/ 38 0 0 6 6",
	"// 38 0 0 6 6",
	"/// 38 0 0 6 6",
	"beam-accel 39 0 0 0 0",
	"beam-rall 39 0 0 0 0",
	0
};

/* user decorations */
static struct u_deco {
	struct u_deco *next;
	char text[2];
} *user_deco;

static struct SYMBOL *first_note;	/* first note/rest of the line */

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
/* special case for arpeggio */
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
		if (s->as.u.note.accs[m]) {
			dx = 5 + s->shac[m];
		} else {
			dx = 6 - s->shhd[m];
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
	de1 = de->start;		/* start of the deco */
	if (de1) {
		s = de1->s;
		x = s->x + 3;
	} else {			/* end without start */
		s = first_note;
		x = s->x - s->wl - 4;
	}
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
		if (de->next && de->next->s == s
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
	dd = &deco_def_tb[de->t];
	de->y = y_get(de->staff, up, x, dx);
	if (!up)
		de->y -= dd->h;
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
	de->x = s->x + s->shhd[s->stem >= 0 ? 0 : s->nhd];
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

	de->v = dd->wl + dd->wr;

	up = up_p(s, s->posit.vol);
	if (up)
		de->flags |= DE_UP;

	x = s->x - dd->wl;
	if (de->prev && de->prev->s == s
	 && ((de->flags ^ de->prev->flags) & DE_UP) == 0) {
		dd2 = &deco_def_tb[de->prev->t];
		if (dd2->func >= 6) {	/* if dynamic mark */
			x2 = de->prev->x + de->prev->v + 4;
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
		y = y_get(s->staff, up, x2, de->v);
		if (y > s->ymn) {
			x = x2;
		} else {
			x2 -= 3;
			y = y_get(s->staff, up, x2, de->v);
			if (y > s->ymn)
				x = x2;
		}
#endif
	}

	str = dd->name;
	if (dd->strx != 0 && dd->strx != 255)
		str = str_tb[dd->strx];

	de->x = x;
	de->y = y_get(s->staff, up, x, de->v);
	if (!up)
		de->y -= dd->h;
	de->str = str;
	/* (y_set is done later in draw_deco_staff) */
}

/* special case for slide and tremolo */
static void d_slide(struct deco_elt *de)
{
	struct SYMBOL *s;
	int m, yc;
	float xc, dx;

	s = de->s;
	yc = s->pits[0];
	xc = 5;
	for (m = 0; m <= s->nhd; m++) {
		if (s->as.u.note.accs[m]) {
			dx = 4 + s->shac[m];
		} else {
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
	struct SYMBOL *s, *s2;
	struct deco_def_s *dd;
	int staff, up;
	float x, y, w;

	if (de->flags & DE_LDST)
		return;
	s2 = de->s;

	if (de->start) {		/* deco start */
		s = de->start->s;
		x = s->x;
		if (s->as.type == ABC_T_NOTE
		 && s->as.u.note.dc.n > 1)
			x += 10;
	} else {			/* end without start */
		s = first_note;
		x = s->x - s->wl - 4;
	}
	de->staff = staff = s2->staff;

	up = s2->multi >= 0;
	if (de->defl & DEF_NOEN) {	/* if no decoration end */
		w = de->x - x;
		if (w < 20) {
			x = de->x - 20 - 3;
			w = 20;
		}
	} else {
		w = s2->x - x - 6;
		if (s2->as.type == ABC_T_NOTE)
			w -= 6;
		if (w < 20) {
			x -= (20 - w) * 0.5;
			if (de->start == 0)
				x -= (20 - w) * 0.5;
			w = 20;
		}
	}

	dd = &deco_def_tb[de->t];
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
	de->v = w;
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

/* above (or below) the staff */
static void d_upstaff(struct deco_elt *de)
{
	struct SYMBOL *s;
	struct deco_def_s *dd;
	float x, yc, stafft, staffb, w;
	int inv;

	s = de->s;
	dd = &deco_def_tb[de->t];
	inv = 0;
	x = s->x + s->shhd[s->stem >= 0 ? 0 : s->nhd];
	w = dd->wl + dd->wr;
	stafft = staff_tb[s->staff].topbar + 2;
	staffb = staff_tb[s->staff].botbar - 2;
	if (dd->strx != 0)
		de->str = dd->strx == 255 ? dd->name : str_tb[dd->strx];

	switch (s->posit.orn) {
	case SL_ABOVE:
		de->flags &= ~DE_BELOW;
		break;
	case SL_BELOW:
		de->flags |= DE_BELOW;
		break;
	}

	if (strcmp(dd->name, ">") == 0
	 || strcmp(dd->name, "accent") == 0
	 || strcmp(dd->name, "emphasis") == 0
	 || strcmp(dd->name, "roll") == 0) {
		if (s->multi < 0
		 || (s->multi == 0 && s->stem > 0)) {
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
	} else if (strcmp(dd->name, "breath") == 0
		|| strcmp(dd->name, "longphrase") == 0
		|| strcmp(dd->name, "mediumphrase") == 0
		|| strcmp(dd->name, "shortphrase") == 0) {
		yc = stafft + 1;
		for (s = s->ts_next; s; s = s->ts_next)
			if (s->shrink != 0)
				break;
		if (s)
			x += (s->x - x) * 0.4;
		else
			x += (realwidth - x) * 0.4;
	} else {
		if (strcmp(dd->name, "invertedturn") == 0
		 || strcmp(dd->name, "invertedturnx") == 0)
			inv = 1;
		if (s->multi >= 0
		 && strcmp(dd->name, "invertedfermata") != 0
		 && !(de->flags & DE_BELOW)) {
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
			if (strcmp(dd->name, "fermata") == 0
			 || strcmp(dd->name, "invertedfermata") == 0)
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

static unsigned char deco_build(char *text)
{
	struct deco_def_s *dd;
	int c_func, ideco, h, o, wl, wr, n;
	unsigned l, ps_x, strx;
	char name[32];
	char ps_func[16];

	/* extract the arguments */
	if (sscanf(text, "%15s %d %15s %d %d %d%n",
			name, &c_func, ps_func, &h, &wl, &wr, &n) != 6) {
		error(1, 0, "Invalid deco %s", text);
		return 128;
	}
	if ((unsigned) c_func >= sizeof func_tb / sizeof func_tb[0]
	 && (c_func < 32 || c_func > 39)) {
		error(1, 0, "%%%%deco: bad C function index (%s)", text);
		return 128;
	}
	if (h < 0 || wl < 0 || wr < 0) {
		error(1, 0, "%%%%deco: cannot have a negative value (%s)", text);
		return 128;
	}
	if (h > 50 || wl > 80 || wr > 80) {
		error(1, 0, "%%%%deco: abnormal h/wl/wr value (%s)", text);
		return 128;
	}
	text += n;
	while (isspace((unsigned char) *text))
		text++;

	/* search the decoration */
	for (ideco = 1, dd = &deco_def_tb[1]; ideco < 128; ideco++, dd++) {
		if (!dd->name
		 || strcmp(dd->name, name) == 0)
			break;
	}
	if (ideco == 128) {
		error(1, 0, "Too many decorations");
		return 128;
	}

	/* search the postscript function */
	for (ps_x = 0; ps_x < sizeof ps_func_tb / sizeof ps_func_tb[0]; ps_x++) {
		if (ps_func_tb[ps_x] == 0
		 || strcmp(ps_func_tb[ps_x], ps_func) == 0)
			break;
	}
	if (ps_x == sizeof ps_func_tb / sizeof ps_func_tb[0]) {
		error(1, 0, "Too many postscript functions");
		return 128;
	}

	/* have an index for the string */
	if (*text == '\0') {
		strx = 0;
	} else if (strcmp(text, name) == 0) {
		strx = 255;
	} else {
		for (strx = 1;
		     strx < sizeof str_tb / sizeof str_tb[0];
		     strx++) {
			if (str_tb[strx] == 0) {
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
			error(1, 0, "Too many decoration strings");
			return 128;
		}
	}

	/* set the values */
	if (!dd->name)
		dd->name = strdup(name);	/* new decoration */
	dd->func = c_func;
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
	if (name[l] == '(' || name[l] == ')') {
		struct deco_def_s *ddo;

		for (o = 1, ddo = &deco_def_tb[1]; o < 128; o++, ddo++) {
			if (!ddo->name)
				break;
			if (strlen(ddo->name) == l + 1
			 && strncmp(ddo->name, name, l) == 0) {
				if (name[l] == '('
				 && ddo->name[l] == ')') {
					dd->ld_end = o;
					break;
				}
				if (name[l] == ')'
				 && ddo->name[l] == '(') {
					ddo->ld_end = ideco;
					break;
				}
			}
		}
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
	for (s = (struct SYMBOL *) s1->as.next;
	     s;
	     s = (struct SYMBOL *) s->as.next) {
		if (s->dur != d
		 || (s->as.flags & ABC_F_SPACE))
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
		     s = (struct SYMBOL *) s->as.next, i--) {
			d = (int) lroundf(a * i) + b;
			s->dur = d;
			t += d;
		}
	} else {				/* !beam-rall! */
		for (s = s1, i = 0;
		     s != s2;
		     s = (struct SYMBOL *) s->as.next, i++) {
			d = (int) lroundf(a * i) + b;
			s->dur = d;
			t += d;
		}
	}
	s2->dur = tt - t;
}

/* -- convert the decorations -- */
void deco_cnv(struct deco *dc,
		struct SYMBOL *s,
		struct SYMBOL *prev)
{
	int i, j;
	struct deco_def_s *dd;
	unsigned char ideco;
	static char must_note_fmt[] = "Deco !%s! must be on a note";

	for (i = dc->n; --i >= 0; ) {
		if ((ideco = dc->t[i]) == 0)
			continue;
		if (ideco < 128) {
			ideco = deco[ideco];
			if (ideco == 0)
				error(1, s,
					"Notation '%c' not treated", dc->t[i]);
		} else {
			ideco = deco_intern(ideco);
		}
		dc->t[i] = ideco;
		if (ideco == 0)
			continue;

		/* special decorations */
		dd = &deco_def_tb[ideco];
		switch (dd->func) {
		default:
			continue;
		case 32:		/* 32 = invisible */
			s->as.flags |= ABC_F_INVIS;
			break;
		case 33:		/* 33 = beamon */
			s->sflags |= S_BEAM_ON;
			break;
		case 34:		/* 34 = trem1..trem4 */
			if (s->as.type != ABC_T_NOTE
			 || !prev
			 || prev->as.type != ABC_T_NOTE) {
				error(1, s,
					"!%s! must be on the last of a couple of notes",
					dd->name);
				break;
			}
			s->sflags |= (S_TREM2 | S_BEAM_END);
			s->sflags &= ~S_BEAM_ST;
			prev->sflags |= (S_TREM2 | S_BEAM_ST);
			prev->sflags &= ~S_BEAM_END;
			s->u = prev->u = dd->name[4] - '0';
			for (j = 0; j <= s->nhd; j++)
				s->as.u.note.lens[j] *= 2;
			for (j = 0; j <= prev->nhd; j++)
				prev->as.u.note.lens[j] *= 2;
			break;
		case 35:		/* 35 = xstem */
			if (s->as.type != ABC_T_NOTE) {
				error(1, s, must_note_fmt, dd->name);
				break;
			}
			s->sflags |= S_XSTEM;
			break;
		case 36:		/* 36 = beambr1 / beambr2 */
			if (s->as.type != ABC_T_NOTE) {
				error(1, s, must_note_fmt, dd->name);
				break;
			}
			s->sflags |= dd->name[6] == '1' ?
					S_BEAM_BR1 : S_BEAM_BR2;
			break;
		case 37:		/* 37 = rbstop */
			s->sflags |= S_RBSTOP;
			break;
		case 38:		/* 38 = /, // and /// = tremolo */
			if (s->as.type != ABC_T_NOTE) {
				error(1, s, must_note_fmt, dd->name);
				break;
			}
			s->sflags |= S_TREM1;
			s->u = strlen(dd->name);	/* 1, 2 or 3 */
			break;
		case 39:		/* 39 = beam-accel/beam-rall */
			if (s->as.type != ABC_T_NOTE) {
				error(1, s, must_note_fmt, dd->name);
				break;
			}
			s->sflags |= S_FEATHERED_BEAM;
			set_feathered_beam(s, dd->name[5] == 'a');
			break;
		}
		dc->t[i] = 0;			/* already treated */
	}
}

/* -- define a user decoration -- */
static unsigned char user_deco_define(char *name)
{
	struct u_deco *d;
	int l;

	l = strlen(name);
	for (d = user_deco; d; d = d->next) {
		if (strncmp(d->text, name, l) == 0
		 && d->text[l] == ' ')
			return deco_build(d->text);
	}
	return 128;
}

/* -- define a standard decoration -- */
unsigned char deco_define(char *name)
{
	unsigned char ideco;
	int l;

	l = strlen(name);
	for (ideco = 0; ; ideco++) {
		if (!std_deco_tb[ideco])
			return 128;
		if (strncmp(std_deco_tb[ideco], name, l) == 0
		 && std_deco_tb[ideco][l] == ' ')
			break;
	}
	return deco_build(std_deco_tb[ideco]);
}

/* -- convert the external deco number to the internal one -- */
unsigned char deco_intern(unsigned char ideco)
{
	char *name;

	if (ideco == 0)
		return ideco;
	name = deco_tb[ideco - 128];
	for (ideco = 1; ideco < 128; ideco++) {
		if (!deco_def_tb[ideco].name) {
			ideco = user_deco_define(name);	/* try a user decoration */
			if (ideco == 128)		/* try a standard decoration */
				ideco = deco_define(name);
			break;
		}
		if (strcmp(deco_def_tb[ideco].name, name) == 0)
			break;
	}
	if (ideco == 128) {
		error(1, 0, "Decoration !%s! not treated", name);
		ideco = 0;
	}
	return ideco;
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
	struct deco *dc;
	int i;
	float wl;

	wl = 0;
	if (s->type == BAR)
		dc = &s->as.u.bar.dc;
	else
		dc = &s->as.u.note.dc;
	for (i = dc->n; --i >= 0; ) {
		struct deco_def_s *dd;

		dd =  &deco_def_tb[dc->t[i]];
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

	for (de = deco_head; de; de = de->next) {
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
			else
				ym = ymid[staff++];
			ym -= dd->h * 0.5;
			if (((de->flags & DE_UP) && y < ym)
			 || (!(de->flags & DE_UP) && y > ym)) {
				struct SYMBOL *s;

				s = de->s;
				if (s->staff > staff) {
					while (s->staff != staff)
						s = s->ts_prev;
				} else if (s->staff < staff) {
					while (s->staff != staff)
						s = s->ts_next;
				}
				y2 = y_get(staff, !(de->flags & DE_UP),
							de->x, de->v)
					+ staff_tb[staff].y;
				if (de->flags & DE_UP)
					y2 -= dd->h;
				if (((de->flags & DE_UP) && y2 > ym)
				 || (!(de->flags & DE_UP) && y2 < ym)) {
					y = ym;
					y_set(staff, de->flags & DE_UP,
							de->x, de->v,
						  ((de->flags & DE_UP) ? y + dd->h : y)
						- staff_tb[staff].y);
				}
			}
		}

		set_scale(de->s);
		set_defl(de->defl);
/*fixme: scaled or not?*/
		if (de->flags & DE_VAL)
			putf(de->v);
		if (de->str) {
			char *p, *q;

			a2b("(");
			q = p = de->str;
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
		putxy(de->x, y);
		if (de->flags & DE_LDEN) {
			if (de->start) {
				x = de->start->x;
				y = de->start->y + staff_tb[de->start->staff].y;
			} else {
				x = first_note->x - first_note->wl - 4;
			}
			if (x > de->x - 20)
				x = de->x - 20;
			putxy(x, y);
		}
		if (de->flags & DE_GRACE) {
			if (de->flags & DE_INV)
				a2b("gsave T 0.7 -0.7 scale 0 0 %s grestore\n",
						ps_func_tb[f]);
			else
				a2b("gsave T 0.7 dup scale 0 0 %s grestore\n",
						ps_func_tb[f]);
		} else {
			if (de->flags & DE_INV)
				a2b("gsave 1 -1 scale neg %s grestore\n",
						ps_func_tb[f]);
			else
				a2b("%s\n", ps_func_tb[f]);
		}
	}
	set_sscale(-1);			/* restore the scale */
}

/* -- draw a decoration relative to a note head -- */
/* return 1 if the decoration is a head */
int draw_deco_head(int ideco, float x, float y, int stem)
{
	struct deco_def_s *dd;
	char *str;

	if (ideco == 0)
		return 0;
	dd = &deco_def_tb[ideco];
	if (dd->ps_func < 0)
		return 0;
	if (cfmt.setdefl)
		set_defl(stem >= 0 ? DEF_STEMUP : 0);
	switch (dd->func) {
	case 2:
	case 5:
	case 7:
		a2b("0 ");
		break;
	case 3:
	case 4:
		if (dd->strx == 0)
			break;
		/* fall thru */
	case 6:
		str = dd->name;
		if (dd->strx != 0 && dd->strx != 255)
			str = str_tb[dd->strx];
		a2b("(%s)", str);
		break;
	}
	putxy(x, y);
	a2b("%s ", ps_func_tb[dd->ps_func]);
	return strncmp(dd->name, "head-", 5) == 0;
}

/* -- draw the chord decorations relative to the heads -- */
void draw_all_deco_head(struct SYMBOL *s, float x, float y)
{
	int k;
	unsigned char ideco;
	struct deco *dc;
	struct deco_def_s *dd;

	dc = &s->as.u.note.dc;
	for (k = dc->n; --k >= 0; ) {
		if (k >= dc->h && k < dc->s)	/* skip the head decorations */
			continue;
		if ((ideco = dc->t[k]) == 0)
			continue;
		dd = &deco_def_tb[ideco];

		if (strncmp(dd->name, "head-", 5) != 0)
			continue;
		draw_deco_head(ideco, x, y, s->stem);
	}
}

/* -- create the deco elements, and treat the near ones -- */
static void deco_create(struct SYMBOL *s,
			struct deco *dc)
{
	int k, l, posit;
	unsigned char ideco;
	struct deco_def_s *dd;
	struct deco_elt *de;
#if 1
/*fixme:pb with decorations above the staff*/
	for (k = 0; k < dc->n; k++) {
		if (k >= dc->h && k < dc->s)	/* skip the head decorations */
			continue;
		if ((ideco = dc->t[k]) == 0)
			continue;
		dd = &deco_def_tb[ideco];
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
		if ((ideco = dc->t[k]) == 0)
			continue;
		dd = &deco_def_tb[ideco];
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
			dc->t[k] = 0;
			continue;
		}

		/* memorize the decorations, but not the head ones */
		if (strncmp(dd->name, "head-", 5) == 0) {
			switch (s->type) {
			case NOTEREST:
				s->sflags |= S_OTHER_HEAD;
				break;
			default:
				error(1, s, "Cannot have !%s! on a bar",
					dd->name);
				break;
			}
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
		if (s->as.type == ABC_T_NOTE
		 && (s->as.flags & ABC_F_GRACE))
			de->flags = DE_GRACE;
		if (dd->ld_end != 0) {
			de->flags |= DE_LDST;
		} else {
			l = strlen(dd->name) - 1;
			if (l > 0 && dd->name[l] == ')') {
				if (strchr(dd->name, '(') == 0) {
					de->flags |= DE_LDEN;
					de->defl = DEF_NOST;
				}
			}
		}
		if (cfmt.setdefl && s->stem >= 0)
			de->defl |= DEF_STEMUP;

		if (dd->func >= 3)	/* if not near the note */
			continue;
		if (s->as.type != ABC_T_NOTE) {
			error(1, s,
				"Cannot have !%s! on a rest or a bar",
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

	deco_head = deco_tail = NULL;
	first = NULL;
	for (s = tsfirst; s; s = s->ts_next) {
		switch (s->type) {
		case BAR:
		case MREST:
			if (s->as.u.bar.dc.n == 0)
				continue;
			dc = &s->as.u.bar.dc;
			break;
		case NOTEREST:
		case SPACE:
			if (!first)
				first = s;
			if (s->as.u.note.dc.n == 0)
				continue;
			dc = &s->as.u.note.dc;
			break;
		case GRACE:
			for (g = s->extra; g; g = g->next) {
				if (g->as.type != ABC_T_NOTE
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
	int f, t, staff, voice;

	for (de = deco_head; de; de = de->next) {
		t = de->t;
		dd = &deco_def_tb[t];
		if (de->flags & DE_LDST) {	/* start of long decoration */
			t = dd->ld_end;
			voice = de->s->voice;	/* search in the voice */
			for (de2 = de->next; de2; de2 = de2->next)
				if (de2->t == t && de2->s->voice == voice)
					break;
			if (!de2) {		/* search in the staff */
				staff = de->s->staff;
				for (de2 = de->next; de2; de2 = de2->next)
					if (de2->t == t && de2->s->staff == staff)
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
	struct SYMBOL *s, *first_gchord;
	struct VOICE_S *p_voice;
	float x, y, w;
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
			minmax[i].ymin -= 3;
			if (minmax[i].ymin > bot - 10)
				minmax[i].ymin = bot -10;
			top = staff_tb[i].topbar;
			minmax[i].ymax += 3;
			if (minmax[i].ymax < top + 10)
				minmax[i].ymax = top + 10;
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
				if (!s->as.u.bar.repeat_bar)
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
		struct SYMBOL *s1, *s2, *first_repeat;
		float y2;
		int i, repnl;

		if (p_voice->second || !p_voice->sym)
			continue;

		/* search the max y offset and set the end of bracket */
		y = staff_tb[p_voice->staff].topbar + 6 + 20;
		first_repeat = 0;
		for (s = p_voice->sym->next; s; s = s->next) {
			if (s->type != BAR
			 || !s->as.u.bar.repeat_bar
			 || (s->sflags & S_NOREPBRA))
				continue;
/*fixme: line cut on repeat!*/
			if (!s->next)
				break;
			if (!first_repeat) {
				set_font(REPEATFONT);
				first_repeat = s;
			}
			s1 = s;

			/* a bracket may be 4 measures
			 * but only 2 measures when it has no start */
			i = s1->as.text ? 4 : 2;
			for (;;) {
				if (!s->next)
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
			if (s->as.u.bar.repeat_bar)
				s = s->prev;
		}

		/* draw the repeat indications */
		s = first_repeat;
		if (!s)
			continue;
		set_sscale(p_voice->staff);
		repnl = 0;
		for ( ; s; s = s->next) {
			char *p;

			if (s->type != BAR
			 || !s->as.u.bar.repeat_bar
			 || (s->sflags & S_NOREPBRA))
				continue;
			s1 = s;
			for (;;) {
				if (!s->next)
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
			if (s1 == s2)
				break;
			x = s1->x;
			if ((s1->as.u.bar.type & 0x07) == B_COL)
				x -= 4;
			i = 0;				/* no bracket end */
			if (s2->sflags & S_RBSTOP) {
				w = 8;			/* (w = left shift) */
			} else if (s2->type != BAR) {
				w = s2->x - realwidth + 4;
			} else if (((s2->as.u.bar.type & 0xf0)	/* if complex bar */
				 && s2->as.u.bar.type != (B_OBRA << 4) + B_CBRA)
				|| s2->as.u.bar.type == B_CBRA) {
				i = 2;			/* bracket start and stop */
/*fixme:%%staves: cursys moved?*/
				if (s->staff > 0
				 && !(cursys->staff[s->staff - 1].flags & STOP_BAR)) {
					w = s2->wl;
				} else if ((s2->as.u.bar.type & 0x0f) == B_COL) {
					w = 12;
				} else if (!(s2->sflags & S_RRBAR)
					|| s2->as.u.bar.type == B_CBRA) {
					w = 0;		/* explicit repeat end */

					/* if ']', don't display as thick bar */
					if (s2->as.u.bar.type == B_CBRA)
						s2->as.flags |= ABC_F_INVIS;
				} else {
					w = 8;
				}
			} else {
				w = 8;
			}
			w = s2->x - x - w;
			p = s1->as.text;
			if (!p) {
				i--;		/* no bracket start (1) or not drawn */
				p = "";
			}
			if (i == 0 && !s2->next	/* 2nd ending at end of line */
			 && !(s2->sflags & S_RBSTOP)) {
				if (p_voice->bar_start == 0)
					repnl = 1;	/* continue on next line */
			}
			if (i >= 0) {
				a2b("(%s)-%.1f %d ",
					p, cfmt.font_tb[REPEATFONT].size * 0.8 + 1, i);
				putx(w);
				putxy(x, y);
				a2b("y%d repbra\n", s1->staff);
				y_set(s1->staff, 1, x, w, y + 2);
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
	for (de = deco_head; de; de = de->next) {
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
	for (de = deco_head; de; de = de->next) {
		struct deco_def_s *dd;

		dd = &deco_def_tb[de->t];
		if (dd->ps_func < 0
		 || dd->func < 6)
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
		y_set(de->staff, de->flags & DE_UP, de->x, de->v, y);
	}
}

/* -- draw the guitar chords and annotations -- */
/* (the staves are not yet defined) */
static void draw_gchord(struct SYMBOL *s,
			float gchy_min, float gchy_max)
{
	struct gch *gch, *gch2;
	int action, ix, box;
	float x, y, w, h, y_above, y_below;
	float hbox, xboxh, xboxl, yboxh, yboxl, expdx;

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
	set_sscale(s->staff);
//	action = A_GCHORD;
	xboxh = xboxl = s->x;
	yboxh = -100;
	yboxl = 100;
	box = 0;
	expdx = 0;
	for (ix = 0, gch = s->gch; ix < MAXGCH; ix++, gch++) {
		if (gch->type == '\0')
			break;
		h = cfmt.font_tb[gch->font].size;
		str_font(gch->font);
		w = tex_str(s->as.text + gch->idx);
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
				w += x;
				if (xboxh < w)
					xboxh = w;
				if (yboxl > y)
					yboxl = y;
				if (yboxh < y + h)
					yboxh = y + h;
				box++;
			}
			break;
		case '<':			/* left */
/*fixme: what symbol space?*/
			if (s->as.u.note.accs[0])
				x -= s->shac[0];
			y = s->yav + gch->y;
			break;
		case '>':			/* right */
			x += s->xmx;
			if (s->dots > 0)
				x += 1.5 + 3.5 * s->dots;
			y = s->yav + gch->y;
			break;
		case '@':			/* absolute */
			y = s->yav + gch->y;
			break;
		}
		putxy(x, y + h * 0.2);		/* (descent) */
		a2b("y%d M ", s->staff);
		if (action == A_GCHEXP)
			a2b("%.2f ", expdx);
		str_out(tex_buf, action);
		if (gch->type == 'g' && box > 0)
			a2b(" boxstart");
		a2b("\n");
	}

	/* draw the box of the guitar chords */
	if (xboxh != xboxl) {		/* if any normal guitar chord */
		xboxl -= 2;
		w = xboxh - xboxl + 2;
		putxy(xboxl, yboxl - 2);
#if 1
		a2b("y%d %.1f boxdraw\n",
			s->staff, yboxh - yboxl + 3);
#else
		a2b("y%d %.1f %.1f box\n",
			s->staff, w, yboxh - yboxl + 3);
#endif
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
	cfmt.font_tb[MEASUREFONT].size /= staff_tb[staff].clef.staffscale;

	s = tsfirst;				/* clef */
	bar_num = nbar;
	if (bar_num > 1) {
		if (cfmt.measurenb == 0) {
			set_font(MEASUREFONT);
			any_nb = 1;
			x = 0;
			w = 20;
//			while (s->staff != staff)
//				s = s->ts_next;
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
			while (s->staff != staff)
				s = s->ts_next;
			if (s->prev->type != CLEF)
				s = s->prev;
			x = s->x - s->wl;
			set_font(MEASUREFONT);
			any_nb = 1;
			w = cwid('0') * cfmt.font_tb[MEASUREFONT].size;
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
		switch (s->type) {
		case STAVES:
			sy = sy->next;
			for (staff = 0; staff < nstaff; staff++) {
				if (!sy->staff[staff].empty)
					break;
			}
			set_sscale(staff);
			continue;
		default:
			continue;
		case BAR:
			break;
		}
		if (s->u <= 0)
			continue;
		bar_num = s->u;
		if (cfmt.measurenb == 0
		 || (bar_num % cfmt.measurenb) != 0
		 || !s->next)
			continue;
		while (s->staff != staff)
			s = s->ts_next;
		if (!any_nb) {
			any_nb = 1;
			set_font(MEASUREFONT);
		}
		w = cwid('0') * cfmt.font_tb[MEASUREFONT].size;
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
		if (s->next->as.type == ABC_T_NOTE) {
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
		if (flags <= 0)
			a2b(" %d su", STEM);
		else {
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
	if (s->as.u.tempo.str1)
		w += tex_str(s->as.u.tempo.str1);
	if (s->as.u.tempo.value != 0) {
		i = 1;
		while (i < sizeof s->as.u.tempo.length
				/ sizeof s->as.u.tempo.length[0]
		       && s->as.u.tempo.length[i] > 0) {
			w += 10;
			i++;
		}
		w += 6 + cwid(' ') * cfmt.font_tb[TEMPOFONT].size * 6
			+ 10 + 10;
	}
	if (s->as.u.tempo.str2)
		w += tex_str(s->as.u.tempo.str2);
	return w;
}

/* - output a tempo --*/
void write_tempo(struct SYMBOL *s,
		 int beat,
		 float sc)
{
	int top, bot;
	unsigned j;

	if (s->as.u.tempo.str1)
		put_str(s->as.u.tempo.str1, A_LEFT);
	if (s->as.u.tempo.value != 0) {
		sc *= 0.7 * cfmt.font_tb[TEMPOFONT].size / 15.0;
						/*fixme: 15.0 = initial tempofont*/
		if (s->as.u.tempo.length[0] == 0) {
			if (beat == 0)
				beat = get_beat(&voice_tb[cursys->top_voice].meter);
			s->as.u.tempo.length[0] = beat;
		}
		for (j = 0;
		     j < sizeof s->as.u.tempo.length
				/ sizeof s->as.u.tempo.length[0]
			&& s->as.u.tempo.length[j] > 0;
		     j++) {
			draw_notempo(s, s->as.u.tempo.length[j], sc);
		}
		put_str("= ", A_LEFT);
		if (sscanf(s->as.u.tempo.value, "%d/%d", &top, &bot) == 2
		 && bot > 0)
			draw_notempo(s, top * BASE_LEN / bot, sc);
		else
			put_str(s->as.u.tempo.value, A_LEFT);
	}
	if (s->as.u.tempo.str2)
		put_str(s->as.u.tempo.str2, A_LEFT);
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
				beat = get_beat(&s->as.u.meter);
			g = s->extra;
			if (!g)
				continue;
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
			h = cfmt.font_tb[PARTSFONT].size + 2 + 2;
						/* + cfmt.partsspace; ?? */
			str_font(PARTSFONT);
		}
		w = tex_str(&g->as.text[2]);
		y = y_get(staff, 1, s->x - 10, w + 15) + 5;
		if (ymin < y)
			ymin = y;
	}
	if (!some_part)
		goto out;

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
		w = tex_str(&g->as.text[2]);
		a2b("%.1f %.1f M", s->x - 10, 2 - ht - h);
		str_out(tex_buf, A_LEFT);
		if (cfmt.partsbox)
			a2b(" %.1f %.1f %.1f %.1f box",
				s->x - 10 - 2, 2 - ht - h - 4,
				w + 4, h);
		a2b("\n");
	}
out:
	return dy * staff_tb[staff].clef.staffscale;
}

/* -- initialize the default decorations -- */
void reset_deco(void)
{
	memset(&deco, 0, sizeof deco);

	/* standard */
	deco['.'] = deco_define("dot");
#ifdef DECO_IS_ROLL
	deco['~'] = deco_define("roll");
#endif
	deco['H'] = deco_define("fermata");
	deco['L'] = deco_define("emphasis");
	deco['M'] = deco_define("lowermordent");
	deco['O'] = deco_define("coda");
	deco['P'] = deco_define("uppermordent");
	deco['S'] = deco_define("segno");
	deco['T'] = deco_define("trill");
	deco['u'] = deco_define("upbow");
	deco['v'] = deco_define("downbow");

	/* non-standard */
#ifndef DECO_IS_ROLL
	deco['~'] = deco_define("gmark");
#endif
	deco['J'] = deco_define("slide");
	deco['R'] = deco_define("roll");
}

/* -- set the decoration flags -- */
void set_defl(int new_defl)
{
	if (defl == new_defl)
		return;
	defl = new_defl;
	a2b("/defl %d def ", new_defl);
}
