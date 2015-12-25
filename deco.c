/*
 * Decoration handling.
 *
 * This file is part of abcm2ps.
 *
 * Copyright (C) 2000-2004, Jean-François Moine.
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
#include <ctype.h>

#include "abcparse.h"
#include "abc2ps.h"

int nbar;		/* current measure number */
int nbar_rep;		/* last repeat bar number */

static struct deco_elt {
	struct deco_elt *next;	/* next decoration */
	struct SYMBOL *s;	/* symbol */
	unsigned char t;	/* decoration index */
	unsigned char staff;	/* staff */
	char inv;		/* invert the glyph if 1 */
	char flags;
#define DE_VAL	0x01		/* put extra value if 1 */
#define DE_UP	0x02		/* above the staff */
#define DE_TREATED 0x04		/* (for !decox(! - !decox)!) */
#define DE_BELOW 0x08		/* below the staff */
	float x, y;		/* x, y */
	float v;		/* extra value */
	char *str;		/* string / 0 */
} *deco_head, *deco_tail;

struct deco_def_s;
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
	unsigned char wl;	/* width */
	unsigned char wr;
	unsigned char str;	/* string index - 255=deco name */
} deco_def_tb[128] = {
	{0, 0, 0, 0},			/* 0: unknown */
	{"dot", 0, 0, 4},		/* 1 */
	{"roll", 3, 10, 10},		/* 2 */
	{"fermata", 3, 6, 12},		/* 3 */
	{"emphasis", 3, 21, 8},		/* 4 */
	{"lowermordent", 3, 5, 10},	/* 5 */
	{"coda", 3, 16, 22},		/* 6 */
	{"uppermordent", 3, 4, 10},	/* 7 */
	{"segno", 3, 17, 20},		/* 8 */
	{"trill", 3, 7, 11},		/* 9 */
	{"upbow", 3, 8, 10},		/* 10 */
	{"downbow", 3, 9, 9},		/* 11 */
	{"gmark", 3, 3, 6},		/* 12 */
	{"slide", 1, 2, 3, 7},		/* 13 */
	{"tenuto", 0, 1, 4},		/* 14 */
	{"crescendo(", 7, -1, 20},	/* 15 */
	{"crescendo)", 7, 19, 20},	/* 16 */
	{"diminuendo(", 7, -1, 20},	/* 17 */
	{"diminuendo)", 7, 19, 20},	/* 18 */
	{"breath", 3, 18, 0},		/* 19 */
	{"longphrase", 3, 24, 0},	/* 20 */
	{"mediumphrase", 3, 25, 0},	/* 21 */
	{"shortphrase", 3, 26, 0},	/* 22 */
	{"invertedfermata", 3, 6, 12},	/* 23 */
	{"arpeggio", 2, 27, 0, 10},	/* 24 */
	{"invertedturn", 3, 22, 10},	/* 25 */
	{"invertedturnx", 3, 23, 10},	/* 26 */
	{"0", 3, 11, 8, 0, 0, 255},
	{"1", 3, 11, 8, 0, 0, 255},
	{"2", 3, 11, 8, 0, 0, 255},
	{"3", 3, 11, 8, 0, 0, 255},
	{"4", 3, 11, 8, 0, 0, 255},
	{"5", 3, 11, 8, 0, 0, 255},
	{"+", 3, 20, 7},
	{"accent", 3, 21, 8},
	{"D.C.", 3, 12, 16, 0, 0, 255},
	{"D.S.", 3, 12, 16, 0, 0, 255},
	{"emphasis", 3, 21, 8},
	{"fine", 3, 12, 16, 0, 0, 1},
	{"f", 6, 13, 20, 2, 2},
	{"ff", 6, 13, 20, 2, 5},
	{"fff", 6, 13, 20, 2, 8},
	{"ffff", 6, 13, 20, 2, 11},
	{"mf", 6, 13, 20, 2, 5},
	{"mordent", 3, 5, 10},
	{"open", 3, 30, 10},
	{"p", 6, 13, 20, 2, 2},
	{"pp", 6, 13, 20, 2, 5},
	{"ppp", 6, 13, 20, 2, 8},
	{"pppp", 6, 13, 20, 2, 11},
	{"pralltriller", 3, 4, 10},
	{"sfz", 6, 14, 20, 2, 8},
	{"turn", 3, 22, 10},
	{"wedge", 3, 29, 8},
	{"turnx", 3, 23, 10},
	{"trill(", 5, -1, 8},
	{"trill)", 5, 28, 8},
	{"snap", 3, 31, 14},
	{"thumb", 3, 15, 14},
};

/* c function table */
static draw_f *func_tb[] = {
	d_near,		/* 0 - near the note */
	d_slide,	/* 1 */
	d_arp,		/* 2 */
	d_upstaff,	/* 3 - tied to note */
	d_upstaff,	/* 4 (below the staff) */
	d_trill,	/* 5 */
	d_pf,		/* 6 - tied to staff */
	d_cresc,	/* 7 */
};

/* postscript function table */
static char *ps_func_tb[64] = {
	"stc",		/* 0: dot */
	"emb",		/* 1: tenuto */
	"sld",		/* 2: slide */
	"grm",		/* 3: gracing mark */
	"umrd",		/* 4: uppermordent */
	"lmrd",		/* 5: lowermordent */
	"hld",		/* 6: fermata */
	"trl",		/* 7: trill */
	"upb",		/* 8: upbow */
	"dnb",		/* 9: downbow */
	"cpu",		/* 10: roll */
	"fng",		/* 11: fingers */
	"dacs",		/* 12: D.C./ D.S. */
	"pf",		/* 13: p, f, pp, .. */
	"sfz",		/* 14: sfz */
	"thumb",	/* 15: thumb */
	"coda",		/* 16: coda */
	"sgno",		/* 17: segno */
	"brth",		/* 18: breath */
	"cresc",	/* 19: (de)crescendo */
	"dplus",	/* 20: plus */
	"accent",	/* 21: accent */
	"turn",		/* 22: turn */
	"turnx",	/* 23: turn with bar */
	"lphr",		/* 24: longphrase */
	"mphr",		/* 25: mediumphrase */
	"sphr",		/* 26: shortphrase */
	"arp",		/* 27: arpeggio */
	"ltr",		/* 28: long trill */
	"wedge",	/* 29: wedge */
	"opend",	/* 30: open */
	"snap",		/* 31: snap */
};

static char *str_tb[32] = {
	0,
	"FINE",		/* 1 */
};

static struct SYMBOL *first_note;	/* first note/rest of the line */

static void draw_gchord(struct SYMBOL *s, float gchy);

/* get the max/min vertical offset */
static float get_y(struct SYMBOL *s,
		   int up,
		   float x,
		   float w,
		   float h)
{
	struct SYMBOL *s2, *s_start;
	int staff, start_seen;
	float y;

	s_start = s;
	start_seen = 0;
	staff = s->staff;
	y = up ? s->dc_top : s->dc_bot;
	while (s->ts_prev != 0 && s->ts_prev->x >= x)
		s = s->ts_prev;
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
				if (y > s2->dc_bot - h)
					y = s2->dc_bot - h;
			}
		}
	}
	return y;
}

/* adjust the vertical offsets */
static void set_y(struct SYMBOL *s,
		  int up,
		  float x,
		  float w,
		  float y)
{
	struct SYMBOL *s_start;
	int staff, start_seen;

	s_start = s;
	start_seen = 0;
	staff = s->staff;
	while (s->ts_prev != 0 && s->ts_prev->x >= x)
		s = s->ts_prev;
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
}

/* -- drawing functions -- */
/* special case for arpeggio */
static void d_arp(struct deco_elt *de)
{
	struct SYMBOL *s;
	struct deco_def_s *dd;
	int m, h;
	float yc, xc;

	s = de->s;
	dd = &deco_def_tb[de->t];
	xc = 10;
	for (m = 0; m <= s->nhd; m++) {
		float dx;

		if (s->as.u.note.accs[m])
			dx = 8 - s->shhd[m] + s->shac[m];
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
	h = s->ymx - s->ymn;
	if (h < 18)
		h = 18;
	yc = (float) (s->yav - h / 2 - 8);

	de->flags = DE_VAL;
	de->v = h;
	de->x = s->x - xc;
	de->y = yc;
}

/* special case for (de)crescendo */
static void d_cresc(struct deco_elt *de)
{
	struct SYMBOL *s;
	struct deco_def_s *dd;
	int staff, voice, up;
	float x, y, dx;
	struct SYMBOL *s2;

	if (de->flags & DE_TREATED)
		return;
	s = de->s;
	dd = &deco_def_tb[de->t];
	voice = s->voice;

	if (dd->ps_func < 0) {		/* start of (de)crescendo */
		int t;

		/* !! works when '(de)crescendo)' is '(de)crescendo(' + 1 !! */
		t = de->t + 1;
		for (de = de->next; de != 0; de = de->next)
			if (de->t == t && de->s->voice == voice)
				break;
		if (de == 0) {		/* no end, insert one */
			de = (struct deco_elt *) getarena(sizeof *de);
			memset(de, 0, sizeof *de);
			deco_tail->next = de;
			deco_tail = de;
			de->s = s;
			de->t = t;
		}
		s2 = de->s;
		x = s->x + 4.;
		if (s->type == NOTE
		    && s->as.u.note.dc.n > 1)
			x += 6.;
	} else {			/* end without start */
		s2 = s;
		s = first_note;
		x = s->x - s->wl - 4.;
	}
	de->staff = staff = s2->staff;

#if 1
/*fixme: test*/
	if (s2->multi != 0)
		up = s2->multi > 0 ? 1 : 0;
	else
#endif
	if (cfmt.exprabove
	    || (!cfmt.exprbelow
		&& staff_tb[staff].nvocal != 0))
		up = 1;
	else	up = 0;

	if (s2 == s
	    && dd->ps_func < 0) {	/* if no decoration end */
		dx = realwidth - x - 6.;
		if (dx < 20.) {
			x = realwidth - 20. - 6.;
			dx = 20.;
		}
	} else {
		dx = s2->x - x - 4.;
		if (s2->type == NOTE
		    && s2->as.u.note.dc.n > 1)
			dx -= 6.;
		if (dx < 20.)
			dx = 20.;
	}

	y = get_y(s, up, x, dx, dd->h);

	if (de->t == 16) {	/* 'crescendo)' */
		x += dx;
		dx = -dx;
	}

	de->flags = DE_VAL|DE_TREATED;
	if (up)
		de->flags |= DE_UP;
	de->v = dx;
	de->x = x;
	de->y = y;
	/* (set_y is done in draw_deco_staff) */
}

/* near the note (dot, tenuto) */
static void d_near(struct deco_elt *de)
{
	struct SYMBOL *s;
	struct deco_def_s *dd;
	int y, sig;

	s = de->s;
	dd = &deco_def_tb[de->t];
	sig = s->stem > 0 ? -1 : 1;
	if (s->multi)
		sig = -sig;
	if (sig > 0)
		y = s->dc_top;
	else	y = s->dc_bot;
	y += 3 * sig;
	if (y > -3 && y < 24 + 3) {
		if (sig > 0)
			y++;
		y = (y + 5) / 6 * 6 - 3;	/* between lines */
	}
	if (s->dc_top < y + 2)
		s->dc_top = y + 2;
	if (s->dc_bot > y - 2)
		s->dc_bot = y - 2;

	de->x = s->x + s->shhd[0];
	de->y = y;
}

/* special case for piano/forte indications */
static void d_pf(struct deco_elt *de)
{
	struct SYMBOL *s;
	struct deco_def_s *dd;
	float y, w;
	char *str;
	int up;

	s = de->s;
	dd = &deco_def_tb[de->t];
#if 1
/*fixme: test*/
	if (s->multi != 0)
		up = s->multi > 0 ? 1 : 0;
	else
#endif
	if (cfmt.exprabove
	    || (!cfmt.exprbelow
		&& staff_tb[s->staff].nvocal != 0))
		up = 1;
	else	up = 0;

	str = dd->name;
	if (dd->str != 0 && dd->str != 255)
		str = str_tb[dd->str];
	w = dd->wl + dd->wr;
	y = get_y(s, up, s->x - dd->wl, w, dd->h);

	if (up)
		de->flags = DE_UP;
	de->v = w;
	de->x = s->x;
	de->y = y;
	de->str = str;
	/* (set_y is done in draw_deco_staff) */
}

/* special case for slide */
static void d_slide(struct deco_elt *de)
{
	struct SYMBOL *s;
	struct deco_def_s *dd;
	int m;
	float yc, xc;

	s = de->s;
	dd = &deco_def_tb[de->t];
	yc = s->ymn;
	xc = 5;
	for (m = 0; m <= s->nhd; m++) {
		float dx, dy;

		if (s->as.u.note.accs[m])
			dx = 4 - s->shhd[m] + s->shac[m];
		else {
			dx = 5 - s->shhd[m];
			switch (s->head) {
			case H_SQUARE:
			case H_OVAL:
				dx += 2.5;
				break;
			}
		}
		dy = (float) (3 * (s->pits[m] - 18)) - yc;
		if (dy < 10 && dx > xc)
			xc = dx;
	}
	de->x = s->x - xc;
	de->y = yc;
}

/* special case for long trill */
static void d_trill(struct deco_elt *de)
{
	struct SYMBOL *s;
	struct deco_def_s *dd;
	int staff, voice, up;
	float x, y, dx;
	struct SYMBOL *s2;

	if (de->flags & DE_TREATED)
		return;
	s = de->s;
	dd = &deco_def_tb[de->t];
	voice = s->voice;
	if (dd->ps_func < 0) {		/* start of trill */
		int t;

		/* !! works when 'trill)' is 'trill(' + 1 !! */
		t = de->t + 1;
		for (de = de->next; de != 0; de = de->next)
			if (de->t == t && de->s->voice == voice)
				break;
		if (de == 0) {		/* no end, insert one */
			de = (struct deco_elt *) getarena(sizeof *de);
			memset(de, 0, sizeof *de);
			deco_tail->next = de;
			deco_tail = de;
			de->s = s;
			de->t = t;
		}
		s2 = de->s;
		x = s->x;
		if (s->type == NOTE
		    && s->as.u.note.dc.n > 1)
			x += 6.;
	} else {			/* end without start */
		s2 = s;
		s = first_note;
		x = s->x - s->wl - 4.;
	}
	de->staff = staff = s2->staff;

	up = s2->multi >= 0;
	if (s2 == s) {			/* if no decoration end */
		dx = realwidth - x - 6.;
		if (dx < 20.) {
			x = realwidth - 20. - 6.;
			dx = 20.;
		}
	} else {
		dx = s2->x - x - 6.;
		if (s2->type == NOTE)
			dx -= 6.;
		if (dx < 20.)
			dx = 20.;
	}

	y = get_y(s, up, x, dx, dd->h);
	if (up) {
		if (y < 24. + 2.)
			y = 24. + 2.;
	} else {
		if (y > -2.)
			y = -2.;
	}

	de->flags = DE_VAL|DE_TREATED;
	de->v = dx;
	de->x = x;
	de->y = y;

	if (up)
		y += dd->h;
	set_y(s, up, x, dx, y);
}

/* above (or below) the staff */
static void d_upstaff(struct deco_elt *de)
{
	struct SYMBOL *s;
	struct deco_def_s *dd;
	float x, yc;
	int inv;
	char *str;

	s = de->s;
	dd = &deco_def_tb[de->t];
	inv = 0;
	x = s->x + s->shhd[0];
	if (dd->str != 0)
		if (dd->str == 255)
			str = dd->name;
		else	str = str_tb[dd->str];
	else	str = 0;
	switch (de->t) {
	case 2:		/* roll */
		if (s->multi < 0
		    || (s->multi == 0 && s->stem > 0)) {
			yc = s->dc_bot;
			if (yc > -2.)
				yc = -2.;
			yc -= dd->h;
			s->dc_bot = yc;
			inv = 1;
		} else {
			yc = s->dc_top + 3;
			if (yc < 24. + 2.)
				yc = 24. + 2.;
			if (s->stem <= 0
			    && (s->dots == 0 || ((int) s->y % 6)))
				yc -= 2;
			s->dc_top = yc + dd->h;
		}
		break;
	case 19:	/* breath */
	case 20:	/* longphrase */
	case 21:	/* mediumphrase */
	case 22:	/* shortphrase */
		yc = 24. + 3.;
		if (s->next != 0)
			x += (s->next->x - x) * 0.4;
		else	x += 10;
		break;
	case 25:	/* invertedturn */
	case 26:	/* invertedturnx */
		inv = 1;
		/* fall thru */
	default:
		if (s->multi >= 0
		    && de->t != 23	/* invertedfermata */
		    && !(de->flags & DE_BELOW)) {
			yc = s->dc_top;
			if (yc < 24. + 2.)
				yc = 24. + 2.;
			s->dc_top = yc + dd->h;
		} else {
			yc = s->dc_bot;
			if (yc > -2.)
				yc = -2.;
			yc -= dd->h;
			s->dc_bot = yc;
			switch (de->t) {
			case 3:		/* fermata */
			case 23:	/* invertedfermata */
				inv = 1;
				break;
			}
		}
		break;
	}
	if (inv)
		yc += dd->h;

	de->inv = inv;
	de->str = str;
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
	int deco;
	char name[16];
	int c_func;
	char ps_func[16];
	int h, wl, wr, n;
	int ps_x, str_x;

	/* extract the arguments */
	if (sscanf(text, "%15s %d %15s %d %d %d%n",
		   name, &c_func, ps_func, &h, &wl, &wr, &n) != 6) {
		error(1, 0, "Invalid deco %s", text);
		return;
	}
	if (c_func < 0 || c_func >= sizeof func_tb / sizeof func_tb[0]) {
		error(1, 0, "%%%%deco: bad C function index (%s)", text);
		return;
	}
	if (h < 0 || wl < 0 || wr < 0) {
		error(1, 0, "%%%%deco: cannot have a negative value (%s)", text);
		return;
	}
	if (h > 50 || wl > 30 || wr > 30) {
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
	if (*text != '\0') {
		for (str_x = 1; str_x < sizeof str_tb / sizeof str_tb[0]; str_x++) {
			if (str_tb[str_x] == 0
			    || strcmp(str_tb[str_x], text) == 0)
				break;
		}
		if (str_x == sizeof str_tb / sizeof str_tb[0]) {
			error(1, 0, "Too many decoration strings");
			return;
		}
	} else	str_x = 0;

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
	if (str_x != 0 && str_tb[str_x] == 0) {
		if (strcmp(text, name) == 0)
			str_x = 255;
		else	str_tb[str_x] = strdup(text);
	}
	dd->str = str_x;
}

/* -- convert the decorations -- */
void deco_cnv(struct deco *dc,
	      struct SYMBOL *s)
{
	int i;

	for (i = dc->n; --i >= 0; ) {
		unsigned char deco;

		deco = dc->t[i];
		if (deco < 128) {
			deco = deco_tune[deco];
			if (deco == 0
			    && dc->t[i] != 0)
				error(1, s->as.linenum,
				      "Notation '%c' not treated", dc->t[i]);
		} else	deco = deco_intern(deco);
		dc->t[i] = deco;
	}
}

/* -- update the x position of a decoration -- */
void deco_update(struct SYMBOL *s, float dx)
{
	struct deco_elt *de;

	for (de = deco_head; de != 0; de = de->next) {
		if (de->s == s)
			de->x += dx;
	}
}

/* -- convert the external deco number to the internal one -- */
unsigned char deco_intern(unsigned char deco)
{
	char *name;

	name = deco_tb[deco - 128];
	for (deco = 1; deco < 128; deco++) {
		if (deco_def_tb[deco].name == 0)
			deco = 127;
		else if (strcmp(deco_def_tb[deco].name, name) == 0)
			break;
	}
	if (deco == 128) {
		error(1, 0, "Decoration %s not treated", name);
		deco = 0;
	}
	return deco;
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
		case 1: wl += 7.; break;	/* slide */
		case 2: wl += 10.; break;	/* arpeggio */
		}
	}
	return wl;
}

/* -- draw the decorations -- */
/* (the staves are defined) */
void draw_all_deco(void)
{
	struct deco_elt *de;

	for (de = deco_head; de != 0; de = de->next) {
		struct deco_def_s *dd;
		int f;

		dd = &deco_def_tb[de->t];
		if ((f = dd->ps_func) < 0)
			continue;

		if (de->flags & DE_VAL)
			PUT1("%.1f ", de->v);
		if (de->str)
			PUT1("(%s) ", de->str);
		PUT2("%.1f %1.2f ",
		     de->x,
		     de->y + staff_tb[de->staff].y);
		if (de->inv)
			PUT1("gsave 1 -1 scale neg %s grestore\n", ps_func_tb[f]);
		else	PUT1("%s\n", ps_func_tb[f]);
	}
}

/* -- draw the decorations near the note (only) -- */
/* (the staves are not yet defined) */
/* this function must be called first as it builds the deco element table */
void draw_deco_near(void)
{
	struct SYMBOL *s;
	int k;
	struct deco *dc;
	unsigned char deco;
	struct deco_def_s *dd;
	struct deco_elt *de;
	struct SYMBOL *first;

	deco_head = deco_tail = 0;
	first = 0;
	for (s = first_voice->sym; s != 0; s = s->ts_next) {
		switch (s->type) {
		case BAR:
			if (s->as.u.bar.dc.n == 0)
				continue;
			dc = &s->as.u.bar.dc;
			break;
		case NOTE:
		case REST:
		case MREST:
			if (first == 0)
				first = s;
			if (s->as.u.note.dc.n == 0)
				continue;
			dc = &s->as.u.note.dc;
			break;
		default:
			continue;
		}

		for (k = dc->n; --k >= 0; ) {
			deco = dc->t[k];
			if (deco == 0)
				continue;
			dd = &deco_def_tb[deco];

			/* memorize the decorations */
			de = (struct deco_elt *) getarena(sizeof *de);
			memset(de, 0, sizeof *de);
			if (deco_head == 0)
				deco_head = de;
			else	deco_tail->next = de;
			deco_tail = de;
			de->s = s;
			de->t = deco;
			de->staff = s->staff;

			if (dd->func >= 3)	/* if not near the note */
				continue;
			if (s->type != NOTE) {
				error(1, s->as.linenum,
				      "Cannot have a %s on a rest or a bar",
				       dd->name);
				continue;
			}
			func_tb[dd->func](de);
		}
	}
	first_note = first;
}

/* -- draw more decorations tied to the note -- */
/* (the staves are not yet defined) */
void draw_deco_note(void)
{
	struct deco_elt *de;

	for (de = deco_head; de != 0; de = de->next) {
		struct deco_def_s *dd;
		int f;

		dd = &deco_def_tb[de->t];
		f = dd->func;
		if (f < 3 || f >= 6)
			continue;	/* not tied to the note */
		if (f == 4)
			de->flags |= DE_BELOW;
		func_tb[f](de);
	}
}

/* -- draw the decorations tied to the staff -- */
/* (the staves are not yet defined) */
void draw_deco_staff(void)
{
	struct SYMBOL *s;
	struct VOICE_S *p_voice;
	float x, y;
	struct deco_elt *de;
	int voice;
	int some_gchord;
	struct {
		float ymin, ymax;
	} minmax[MAXSTAFF];

	/* set the top and bottom of all symbols out of the staves */
	for (s = first_voice->sym; s != 0; s = s->ts_next) {
		if (s->dc_top < 24. + 2.)
			s->dc_top = 24. + 2.;
		if (s->dc_bot > -2.)
			s->dc_bot = -2.;
	}

	/* search the vertical offset for the guitar chords */
	memset(minmax, 0, sizeof minmax);
	some_gchord = 0;
	for (s = first_voice->sym; s != 0; s = s->ts_next) {
		float w;
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
		w = cwid('a') * cfmt.gchordfont.size * 1.1;
		if ((p = strchr(s->as.text, '\n')) != 0)
			w *= p - s->as.text;
		else	w *= strlen(s->as.text);
		y = get_y(s, 1, s->x, w, 0);
		if (y > minmax[s->staff].ymax)
			minmax[s->staff].ymax = y;
	}

	/* draw the guitar chords if any */
	if (some_gchord) {
		set_font(&cfmt.gchordfont);
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
			draw_gchord(s, minmax[s->staff].ymax + 4);
		}
	}

	/* compute the max number of lyrics */
	if (!cfmt.musiconly) {
		for (s = first_voice->sym; s != 0; s = s->ts_next) {
			struct lyrics *ly;
			int nlyric;

			if ((ly = s->ly) == 0)
				continue;
			for (nlyric = MAXLY; --nlyric >= 0; )
				if (ly->w[nlyric] != 0)
					break;
			nlyric++;
			voice = s->voice;
			if (voice_tb[voice].nvocal < nlyric)
				voice_tb[voice].nvocal = nlyric;
		}
		for (p_voice = first_voice; p_voice; p_voice = p_voice->next)
			staff_tb[p_voice->staff].nvocal += p_voice->nvocal;
	}

	/* draw the repeat bars */
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		struct SYMBOL *s1, *s2, *first_repeat;
		float y2;
		int i, repnl;

		if (p_voice->second
		    || staff_tb[p_voice->staff].brace_end)
			continue;

		/* search the max y offset */
		y = 24. + 6. + 20.;
		first_repeat = 0;
		for (s = p_voice->sym; s != 0; s = s->next) {
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
			i = 4;
			for (;;) {
				if (s->next == 0)
					break;
				s = s->next;
				if (s->type == BAR) {
					if (((s->as.u.bar.type & 0xf0)	/* if complex bar */
					     && s->as.u.bar.type != (B_OBRA << 4) + B_CBRA)
					    || s->as.u.bar.type == B_CBRA
					    || s->as.u.bar.repeat_bar)
						break;
					if (--i < 0)	/*fixme*/
						break;
				}
			}
			y2 = get_y(s1, 1, s1->x, s->x - s1->x, 0);
			if (y2 > y)
				y = y2;

			/* have room for the repeat numbers */
			y2 = s1->next->dc_top;
			if (s1->next->as.text != 0)	/* if guitar chord */
				y2 -= cfmt.gchordfont.size; /* got already */
			if (y2 > 24. + 6. + 4.)
				y = y2 + 10. + 4.;

			if (s->as.u.bar.repeat_bar)
				s = s->prev;
		}

		/* draw the repeat indications */
		repnl = 0;
		for (s = first_repeat; s != 0; s = s->next) {
			char *p;
			float w;

			if (s->type != BAR)
				continue;
			if (!s->as.u.bar.repeat_bar)
				continue;
			s1 = s;
			i = 4;
			s2 = 0;
			for (;;) {
				if (s->next == 0)
					break;
				s = s->next;
				if (s->type == BAR) {
					if (((s->as.u.bar.type & 0xf0)	/* if complex bar */
					     && s->as.u.bar.type != (B_OBRA << 4) + B_CBRA)
					    || s->as.u.bar.type == B_CBRA
					    || s->as.u.bar.repeat_bar) {
						s2 = s;
						break;
					}
					if (i == 4)
						s2 = s;
					if (--i < 0)	/*fixme*/
						break;
				}
			}
			if (s2 == 0)
				s2 = s;
/*fixme*/
			if (s1 == s2)
				break;
			x = s1->x;
			if ((s1->as.u.bar.type & 0x0f) == B_COL)
				x -= 4;
			i = 0;			/* no end of bracket */
			w = s2->x - x - 8;
			if (s2->type != BAR)
				w = realwidth - x - 4;
			else if (((s2->as.u.bar.type & 0xf0)	/* if complex bar */
				   && s2->as.u.bar.type != (B_OBRA << 4) + B_CBRA)
				 || s2->as.u.bar.type == B_CBRA) {
				i =  2;
				if ((s2->as.u.bar.type & 0x0f) == B_COL)
					w -= 4;
				else if (!(s2->sflags & S_RRBAR)
					 || s2->as.u.bar.type == B_CBRA
					 || s2->as.u.bar.type == (B_CBRA << 4) + B_BAR)
					w += 8;		/* explicit repeat end */
				if (p_voice != first_voice)
					w -= 4;
			}
			p = s1->as.text;
			if (p == 0) {
				i--;		/* no start of bracket */
				p = "";
			}
			if (i == 0 && s2->next == 0) {	/* 2nd ending at end of line */
				if (p_voice->bar_start != 0)
					i = 2;
				else	repnl = 1;	/* continue on next line */
			}
			if (i >= 0) {
				PUT2("(%s) %d ", p, i);
				PUT4("%.1f %.1f \x01%c%5.2f repbra\n",
				     w, x, '0' + s1->staff, y);
				set_y(s1, 1, x, w, y + 2.);
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
		if (de->flags & DE_UP) {
			if (de->y > minmax[de->staff].ymax)
				minmax[de->staff].ymax = de->y;
		} else {
			if (de->y < minmax[de->staff].ymin)
				minmax[de->staff].ymin = de->y;
		}
	}

	/* and set them at a same vertical offset */
	for (de = deco_head; de != 0; de = de->next) {
		struct deco_def_s *dd;

		dd = &deco_def_tb[de->t];
		if (dd->ps_func < 0
		    || dd->func < 6)
			continue;
		x = de->x;
		if (de->flags & DE_UP)
			y = minmax[de->staff].ymax;
		else	y = minmax[de->staff].ymin;
		de->y = y;
		if (de->flags & DE_UP)
			y += dd->h;
		set_y(de->s, de->flags & DE_UP, x, de->v, y);
	}

	/* draw the measure numbers */
	if (cfmt.measurenb >= 0) {
		char *showm;
		int bar_time, any_nb;
		float wmeasure;

		showm = cfmt.measurebox ? "showb" : "show";
		any_nb = 0;

		/* get the current bar number */
		for (s = first_voice->sym;
		     s->next != 0;	/* ?? */
		     s = s->next) {
			switch (s->type) {
			case TIMESIG:
				wmeasure = s->as.u.meter.wmeasure;
			case CLEF:
			case KEYSIG:
			case PART:
			case TEMPO:
				continue;
			case BAR:
				if (s->u != 0)
					nbar = s->u;		/* (%%setbarnb) */
				else if (s->as.u.bar.repeat_bar
					 && s->as.text != 0
					 && s->as.text[0] != '1')
					nbar = nbar_rep; /* restart bar numbering */
				break;
			default:
				break;
			}
			break;
		}
		if (nbar > 1) {
			if (cfmt.measurenb == 0) {
				set_font(&cfmt.measurefont);
				any_nb = 1;
				PUT4(" 0 \x01%c%5.2f M (%d) %s",
				     '0', 24. + 14.,
				     nbar, showm);
			} else if (nbar % cfmt.measurenb == 0) {
				x = s->x - s->wl - 8.;
				set_font(&cfmt.measurefont);
				any_nb = 1;
				y = get_y(s, 1, s->x, 20., 0);
				if (y < s->dc_top + 5)
					y = s->dc_top + 5;
				if (s->next != 0
				    && y < s->next->dc_top - 5)
					y = s->next->dc_top - 5;
				set_y(s, 1, s->x, 20., y + 6.);
				PUT5("%.1f \x01%c%5.2f M (%d) %s",
				     x, '0', y, nbar, showm);
			}
		}

/*fixme: KO when no bar at the end of the previous line */
		wmeasure = first_voice->meter.wmeasure;
		bar_time = first_voice->sym->time
			+ wmeasure;
		for (s = first_voice->sym; s != 0; s = s->next) {
			switch (s->type) {
			case TIMESIG:
				wmeasure = s->as.u.meter.wmeasure;
				bar_time = s->time + wmeasure;
				continue;
			case MREST:
			case MREP:
				nbar += s->as.u.bar.len - 1;
			default:
				continue;
			case BAR:
				break;
			}
			if (s->u != 0)
				nbar = s->u;		/* (%%setbarnb) */
			if (s->time < bar_time		/* incomplete measure */
			    || s->as.u.bar.type == B_INVIS
			    || s->as.u.bar.type == B_CBRA)
				continue;
			if (s->u == 0) {
				nbar++;
				if (s->as.u.bar.repeat_bar
				    && s->as.text != 0) {
					if (s->as.text[0] == '1')
						nbar_rep = nbar;
					else	nbar = nbar_rep; /* restart bar numbering */
				}
			}
			bar_time = s->time + wmeasure;
			if (s->next == 0
			    || cfmt.measurenb == 0
			    || (nbar % cfmt.measurenb) != 0
			    || nbar <= 1)
				continue;
			if (!any_nb) {
				any_nb = 1;
				set_font(&cfmt.measurefont);
			}
/*fixme: compute the real width of the number */
			y = get_y(s, 1, s->x, 20., 0);
			if (y < s->dc_top + 5)
				y = s->dc_top + 5;
/* fixme: have the number just right below the top of the next symbol*/
			if (s->next != 0
			    && y < s->next->dc_top - 5)
				y = s->next->dc_top - 5;
			set_y(s, 1, s->x, 20., y + 6.);
			PUT5(" %.1f \x01%c%5.2f M (%d) %s",
			     s->x, '0', y,
			     nbar, showm);
		}
		if (any_nb)
			PUT0("\n");
	}
}

/* -- draw guitar chords -- */
/* (the staves are not yet defined) */
static void draw_gchord(struct SYMBOL *s,
			float gchy)
{
	float x, y, xspc, yspc, gchyb, gchyl, gchyr;
	float xmin, xmax, ymin, ymax;
	int box;
	char *p, *q;
	char t[81];

	/* compute the y offset of all position types */
	yspc = cfmt.gchordfont.size * cfmt.gchordfont.swfac;
	if (gchy < 34.)
		gchy = 34.;
	ymin = gchy;
	gchy -= yspc;
	gchyb = s->dc_bot - yspc;
	gchyl = gchyr = s->y - yspc * 0.75;
	p = s->as.text;
	for (;;) {
		if ((q = strchr(p, '\n')) != 0)
			*q = '\0';
		switch (*p) {
		default:		/* default = above */
		case '^':		/* above */
			gchy += yspc;
			break;
		case '_':		/* below */
			break;
		case '<':		/* left */
			gchyl += yspc * 0.5;
			break;
		case '>':		/* right */
			gchyr += yspc * 0.5;
			break;
		case '@':		/* absolute */
			break;
		}
		if (q == 0)
			break;
		*q = '\n';
		p = q + 1;
	}
	ymax = s->dc_top = gchy + yspc;
	xmin = xmax = s->x;
	box = cfmt.gchordbox;

	/* loop on each line */
	p = s->as.text;
	for (;;) {
		if ((q = strchr(p, '\n')) != 0)
			*q = '\0';
		x = s->x;
		tex_str(t, p, sizeof t, &xspc);
		p = t;
		xspc *= cfmt.gchordfont.size;
		switch (*p) {
		case '^':		/* above */
		case '_':		/* below */
			if (*p == '^')
				xspc -= cwid('^') * cfmt.gchordfont.size;
			else	xspc -= cwid('_') * cfmt.gchordfont.size;
			p++;
			/* fall thru */
		default: {		/* default = above */
			float tmp;

			tmp = xspc;
			xspc *= GCHPRE;
			if (xspc > 8)
				xspc = 8;
			x -= xspc;
			if (t[0] == '_') {
				y = gchyb;
				s->dc_bot = gchyb - 2;
				gchyb -= yspc;
			} else {
				y = gchy;
				gchy -= yspc;
				if (box && *p != '^') {
					if (x < xmin)
						xmin = x;
					tmp += x;
					if (tmp > xmax)
						xmax = tmp;
					box = 2;
				}
			}
			break;
		    }
		case '<':		/* left */
/*fixme: what symbol space?*/
			x -= xspc - cwid('<') * cfmt.gchordfont.size + 6;
			y = gchyl;
			gchyl -= yspc;
			p++;
			break;
		case '>':		/* right */
/*fixme: what symbol space?*/
			x += 6;
			y = gchyr;
			gchyr -= yspc;
			p++;
			break;
		case '@': {		/* absolute */
			int n;
			float xo, yo;

			p++;
			if (sscanf(p, "%f,%f%n", &xo, &yo, &n) != 2) {
				error(1, s->as.linenum,
				      "Error in guitar chord \"@\" format");
				y = s->y + yo;
			} else {
				x += xo;
				y = s->y + yo;
				p += n;
			}
			break;
		    }
		}
		PUT4("%.1f \x01%c%5.2f M (%s) gcshow ",
		     x, '0' + s->staff, y, p);
		if (q == 0)
			break;
		*q = '\n';
		p = q + 1;
	}

	/* draw the box */
	if (box == 2) {		/* if any normal guitar chord */
		PUT5("%.1f \x01%c%5.2f %.1f %.1f box",
		     xmin - 2, '0' + s->staff, ymin - 5,
		     xmax - xmin + 8, ymax - ymin + 4);
	}

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

/* -- draw the parts and the tempo information -- */
/* (the staves are being defined) */
float draw_partempo(float top,
		    int any_part,
		    int any_tempo,
		    int any_vocal)
{
	struct SYMBOL *s;
	float h, ht, w, y, ymin, nw, dy;
	int i;
	char tmp[128];

	/* put the tempo indication at top */
	dy = 0;
	if (any_tempo) {
		int head, dots, flags, beat;
		float sc, dx;

		ht = cfmt.tempofont.size + 2. + 2.;
		if (any_vocal)
			dy = ht;
		else {
			nw = 6. + cwid(' ') * cfmt.tempofont.size * 6;
			/*fixme:have tempo on other voices but the 1st?*/

			/* get the minimal y offset */
			ymin = 24. + 12.;
			for (s = first_voice->sym; s != 0; s = s->next) {
				if (s->type != TEMPO)
					continue;

				if (s->as.u.tempo.str1 != 0) {
					tex_str(tmp, s->as.u.tempo.str1, sizeof tmp, &w);
					nw += w * cfmt.tempofont.size;
				}
				if (s->as.u.tempo.value != 0) {
					i = 1;
					while (i < sizeof s->as.u.tempo.length
							/ sizeof s->as.u.tempo.length[0]
					       && s->as.u.tempo.length[i] > 0) {
						nw += 10;
						i++;
					}
					nw += 10 + 10;
				}
				if (s->as.u.tempo.str2 != 0) {
					tex_str(tmp, s->as.u.tempo.str2, sizeof tmp, &w);
					nw += w * cfmt.tempofont.size;
				}
				y = get_y(s, 1, s->x - 5., nw + 30. + 10., 0) + 2.;
				if (y > ymin)
					ymin = y;
			}
			if (top < ymin + ht)
				dy = ymin + ht - top;
		}

		/* draw the tempo indications */
		set_font(&cfmt.tempofont);
		sc = 0.7 * cfmt.tempofont.size / 15.0;	/*fixme: 15.0 = initial tempofont*/
		beat = get_beat(&first_voice->meter);
		for (s = first_voice->sym; s != 0; s = s->next) {
			if (s->type != TEMPO) {
				if (s->type == TIMESIG)
					beat = get_beat(&s->as.u.meter);
				continue;
			}

			/*fixme: cf left shift (-5.)*/
			PUT2("%.1f %.1f M ", s->x - 5., 2. - ht);
			if (s->as.u.tempo.str1 != 0) {
				tex_str(tmp, s->as.u.tempo.str1, sizeof tmp, 0);
				PUT1("(%s) show\n", tmp);
			}

			/* draw the tempo indication, if specified */
			if (s->as.u.tempo.value != 0) {
				int j;

				if (s->as.u.tempo.length[0] == 0)
					s->as.u.tempo.length[0] = beat;
				j = 0;
				while (j < sizeof s->as.u.tempo.length
						/ sizeof s->as.u.tempo.length[0]
				       && s->as.u.tempo.length[j] > 0) {
					identify_note(s, s->as.u.tempo.length[j],
						      &head, &dots, &flags);
					PUT1("gsave %.2f dup scale 15 3 RM currentpoint\n",
					     sc);
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
					dx = 4.0;
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
						for (i = 0; i < dots; i++) {
							PUT1(" %.1f 0 dt", dotx);
							dx = dotx;
							dotx += 3.5;
						}
					}
					/* (16 is the stem height) */
					if (s->as.u.tempo.length[j] < SEMIBREVE) {
						if (flags <= 0)
							PUT1(" %d su", STEM);
						else {
							PUT2(" %d %d sfu", flags, STEM);
							if (dx < 6.0)
								dx = 6.0;
						}
					}
					PUT1(" grestore %.2f 0 RM\n",
					     (dx + 18) * sc);
					j++;
				}
				PUT1("( = %d) show\n",
				     s->as.u.tempo.value);
			}

			if (s->as.u.tempo.str2 != 0) {
				tex_str(tmp, s->as.u.tempo.str2, sizeof tmp, 0);
				PUT1("(%s) show\n", tmp);
			}
		}
	} else	ht = 0;

	/* then, put the parts */
	if (!any_part)
		return dy;

/*fixme: should reduce if parts don't overlap tempo...*/
	h = cfmt.partsfont.size + 2. + 2.;	/* + cfmt.partsspace; */
	
	if (any_vocal)
		dy += h;
	else {
		ymin = 24. + 14.;
		for (s = first_voice->sym; s != 0; s = s->next) {
			if (s->type != PART)
				continue;
			tex_str(tmp, &s->as.text[2], sizeof tmp, &w);
			w *= cfmt.partsfont.size;
			y = get_y(s, 1, s->x - 10., w + 15., 0) + 5.;
			if (ymin < y)
				ymin = y;
		}
		if (top < ymin + h + ht)
			dy = ymin + h + ht - top;
	}

	set_font(&cfmt.partsfont);
	for (s = first_voice->sym; s != 0; s = s->next) {
		if (s->type != PART)
			continue;
		tex_str(tmp, &s->as.text[2], sizeof tmp, 0);
		PUT4("%.1f %.1f M (%s) show%s\n",
		     s->x - 10., 2. - ht - h, tmp, cfmt.partsbox ? "b" : "");
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
