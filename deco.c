/*++
 * Decoration handling.
 *
 * This file is part of abcm2ps.
 *
 * Copyright (C) 2000, Jean-François Moine.
 *
 * Contact: mailto:moinejf@free.fr
 * Original site: http://moinejf.free.fr/
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
 *
 * fixme: top & bottom of staves KO: the staves are already defined
 *
 *--*/

#include <stdio.h>
#include <string.h>

#include "abcparse.h"
#include "abc2ps.h"

struct deco_def_s;
typedef void draw_f(struct SYMBOL *s,
		    struct deco_def_s *dd,
		    float staffb,
		    float *top,
		    float *bot);
static draw_f d_near, d_slide, d_upstaff, d_roll, d_pf;

/* decoration table */
static struct deco_def_s {
	unsigned char func;	/* function index */
	unsigned char ps_func;	/* postscript function index */
	unsigned char h;	/* height */
	unsigned char flags;
#define DECO_NEAR 0x01		/* near the note */
#define DECO_INVERT 0x02	/* invert the figure when 2nd voice in the staff */
#define DECO_INVF 0x04		/* invert the figure */
#define DECO_TEXT 0x08		/* with text */
#define DECO_NONOTE 0x10	/* not associated with a note */
} deco_def[128] = {
	{1, 0, 4, DECO_NEAR},	/* dot		- 0 */
	{3, 11, 5, DECO_TEXT},	/* 0 */
	{3, 11, 5, DECO_TEXT},	/* 1 */
	{3, 11, 5, DECO_TEXT},	/* 2 */
	{3, 11, 5, DECO_TEXT},	/* 3 */
	{3, 11, 5, DECO_TEXT},	/* 4 */
	{3, 11, 5, DECO_TEXT},	/* 5 */
	{0, 0, 0, 0},		/* plus */
	{0, 0, 0, 0},		/* accent */
	{3, 18, 0, DECO_TEXT}, /* breath */
	{0, 0, 0, 0},		/* crescendo_s	- 10 */
	{0, 0, 0, 0},		/* crescendo_e */
	{3, 16, 20, 0},		/* coda */
	{3, 12, 12, DECO_TEXT},	/* DC */
	{3, 12, 12, DECO_TEXT},	/* DS */
	{0, 0, 0, 0},		/* diminuendo_s */
	{0, 0, 0, 0},		/* diminuendo_e */
	{3, 9, 9, 0},		/* downbow */
	{0, 0, 0, 0},		/* emphasis */
	{5, 13, 16, DECO_NONOTE}, /* f */
	{3, 6, 6, DECO_INVERT}, /* fermata	- 20 */
	{5, 13, 16, DECO_NONOTE}, /* ff */
	{5, 13, 16, DECO_NONOTE}, /* fff */
	{5, 13, 16, DECO_NONOTE}, /* ffff */
	{3, 12, 12, DECO_TEXT},	/* fine */
	{3, 6, 6, DECO_INVF},	/* invertedfermata */
	{0, 0, 0, 0},		/* longphrase */
	{3, 5, 6, 0},		/* lowermordent */
	{0, 0, 0, 0},		/* mediumphrase */
	{5, 13, 16, DECO_NONOTE}, /* mf */
	{3, 5, 6, 0},		/* mordent	- 30 */
	{0, 0, 0, 0},		/* open */
	{5, 13, 16, DECO_NONOTE}, /* p */
	{5, 13, 16, DECO_NONOTE}, /* pp */
	{5, 13, 16, DECO_NONOTE}, /* ppp */
	{5, 13, 16, DECO_NONOTE}, /* pppp */
	{3, 4, 6, 0},		/* pralltriller */
	{0, 0, 0, 0},		/* repeatbar */
	{0, 0, 0, 0},		/* repeatbar2 */
	{4, 10, 6, 0},		/* roll */
	{3, 17, 18, 0},		/* segno	- 40 */
	{5, 14, 16, DECO_NONOTE}, /* sfz */
	{0, 0, 0, 0},		/* shortphrase */
	{0, 0, 0, 0},		/* snap */
	{1, 1, 4, DECO_NEAR},	/* tenuto */
	{0, 0, 0, 0},		/* thumb */
	{3, 7, 9, 0},		/* trill */
	{3, 3, 5, 0},		/* turn */
	{3, 8, 9, 0},		/* upbow */
	{3, 4, 6, 0},		/* uppermordent */
	{0, 0, 0, 0},		/* wedge	- 50 */
	{2, 2, 3, DECO_NEAR},	/* slide */
	{5, 15, 14, DECO_NONOTE}, /* cresc */
	{5, 15, 14, DECO_NONOTE}, /* decresc */
	{5, 15, 14, DECO_NONOTE}, /* dimin */
	{5, 13, 16, DECO_NONOTE}, /* fp */
};

/* function table */
static draw_f *func_tb[] = {
	0,
	d_near,		/* 1 */
	d_slide,	/* 2 */
	d_upstaff,	/* 3 */
	d_roll,		/* 4 */
	d_pf,		/* 5 */
};

/* postscript function table */
static char *ps_func_tb[] = {
	"stc",		/* 0: dot */
	"emb",		/* 1: tenuto */
	"sld",		/* 2: slide */
	"grm",		/* 3: turn */
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
	"crdc",		/* 15: cresc, decresc, .. */
	"coda",		/* 16: coda */
	"sgno",		/* 17: segno */
        "brth",         /* 18: breath */
};

/* -- drawing functions -- */
/* near the note */
static void d_near(struct SYMBOL *s,
		   struct deco_def_s *dd,
		   float staffb,
		   float *top,
		   float *bot)
{
	int y, sig;

	sig = s->stem > 0 ? -1 : 1;
	if (s->multi) {
		sig = -sig;
		y = ((int) (s->ys + 2.9 * sig)) / 3 * 3;
		y += 6 * sig;
	} else	y = s->y + 6 * sig;
	if (y < *top + 3)
		y = *top + 3;
	else if (y > *bot - 4)
		y = *bot - 4;
	if (!(y % 6) && y >= 0 && y <= 24)
		y += 3 * sig;		/* between the lines */
	if (*top < y + 4)
		*top = y + 4;
	if (*bot >= y)
		*bot = y - 1;
	PUT2(" %.1f %s", (float) y + staffb, ps_func_tb[dd->ps_func]);
}

/* special case for piano/forte indications */
static void d_pf(struct SYMBOL *s,
		 struct deco_def_s *dd,
		 float staffb,
		 float *top,
		 float *bot)
{
	float yc;
	char *f, *p;
	struct STAFF *staffp;

	f = ps_func_tb[dd->ps_func];
	staffp = &staff_tb[(int) s->staff];
	/*fixme: may be more complex*/
	/*fixme: should be in an other pass*/
	if (staffp->nvocal == 0) {

		/* below the staff */
		if (s->stem > 0)
			yc = s->y;
		else	yc = s->ys;
		yc -= 3;
		if (yc > -5)
			yc = -5;
		if (yc > *bot)
			yc = *bot;
		yc -= dd->h + 3;
		if (*bot >= yc)
			*bot = yc - 1;
	} else {

		/* above the staff */
		if (s->stem > 0)
			yc = s->ys;
		else	yc = s->ymx + 3;
		if (yc < 24 + 1)
			yc = 24 + 1;
		yc += 3;
		if (yc < *top + 2)
			yc = *top + 2;
		if (*top <= yc + dd->h)
			*top = yc + dd->h + 1;
	}

	switch (dd - deco_def + 128) {
	case D_p: p = "p"; break;
	case D_pp: p = "pp"; break;
	case D_ppp: p = "ppp"; break;
	case D_pppp: p = "pppp"; break;
	case D_mf: p = "mf"; break;
	case D_f: p = "f"; break;
	case D_ff: p = "ff"; break;
	case D_fff: p = "fff"; break;
	case D_ffff: p = "ffff"; break;
	case D_sfz: p = "sfz"; break;
	case D_cresc: p = "Cresc."; break;
	case D_decresc: p = "Decresc."; break;
	case D_dimin: p = "Dimin."; break;
	case D_fp: p = "fp"; break;
	default: ERROR(("No text for %s", deco_tb[dd - deco_def])); return;
	}
	PUT3(" (%s) %.2f %s", p, yc + staffb, f);
}

/* special case for roll */
static void d_roll(struct SYMBOL *s,
		   struct deco_def_s *dd,
		   float staffb,
		   float *top,
		   float *bot)
{
	float yc;
	int down;
	char *f;

	f = ps_func_tb[dd->ps_func];
	down = 0;
	if (s->multi < 0
	    || (s->multi == 0 && s->stem > 0))
		down = 1;
	if (down) {
		if (s->stem > 0)
			yc = s->y;
		else	yc = s->ys;
		yc -= 3;
		if (yc > *bot)
			yc = *bot;
		if (yc > 0)
			yc = 0;
		yc -= dd->h + 2;
		if (*bot >= yc)
			*bot = yc - 1;
		PUT2(" gsave 1 -1 scale %.2f %s grestore",
		     -dd->h - yc - staffb, f);
	} else {
		if (s->stem > 0)
			yc = s->ys + 3;
		else	{
			yc = s->ymx + 3;
			if (s->dots == 0 || ((int) s->y % 6))
				yc -= 2;
		}
		if (yc < 24 + 1)
			yc = 24 + 1;
		yc += 3;
		if (yc < *top + 2)
			yc = *top + 2;
		if (*top <= yc + dd->h)
			*top = yc + dd->h + 1;
		PUT2(" %.2f %s", yc + staffb, f);
	}
}

/* special case for slide */
static void d_slide(struct SYMBOL *s,
		    struct deco_def_s *dd,
		    float staffb,
		    float *top,
		    float *bot)
{
	int m;
	float yc, xc;

	yc = s->ymn;
	xc = 5;
	for (m = 0; m <= s->nhd; m++) {
		float dx, dy;

		dx = 5 - s->shhd[m];
		if (s->head == H_OVAL)
			dx += 2.5;
		if (s->as.u.note.accs[m])
			dx = 4 - s->shhd[m] + s->shac[m];
		dy = (float) (3 * (s->pits[m] - 18)) - yc;
		if (dy < 10 && dx > xc)
			xc = dx;
	}
	PUT3(" %.1f %.1f %s", yc + staffb, xc, ps_func_tb[dd->ps_func]);
}

/* above the staff */
static void d_upstaff(struct SYMBOL *s,
		      struct deco_def_s *dd,
		      float staffb,
		      float *top,
		      float *bot)
{
	float yc;
	char *f;
	int invert;

	f = ps_func_tb[dd->ps_func];
	invert = (dd->flags & DECO_INVF) ? 1 : 0;
	if (s->multi >= 0) {
		if (s->stem > 0)
			yc = s->ys;
		else	yc = s->ymx + 3;
		if (yc < 24 + 1)
			yc = 24 + 1;
		yc += 3;
		if (yc < *top + 2)
			yc = *top + 2;
		if (*top <= yc + dd->h)
			*top = yc + dd->h + 1;
	} else {
		yc = s->ys;
		if (yc > *bot)
			yc = *bot;
		if (yc > 0)
			yc = 0;
		yc -= dd->h + 5;
		if (*bot >= yc)
			*bot = yc - 1;
		if (dd->flags & DECO_INVERT)
			invert = !invert;
	}
	yc += staffb;
	if (invert)
		PUT2(" gsave 1 -1 scale %.2f %s grestore",
		     -dd->h - yc, f);
	else if (dd->flags & DECO_TEXT) {
		char buf[8];

		if (dd->ps_func == 11)		/* fingering */
			sprintf(buf, "%d", dd - deco_def + 128 - D_0);
		else	{			/* D.C. / D.S. / FINE */
			switch (dd - deco_def + 128) {
			case D_breath:
				strcpy(buf, ",");
				yc = staffb + 24. + 3.;
				break;
			case D_DC:
				strcpy(buf, "D.C.");
				break;
			case D_DS:
				strcpy(buf, "D.S.");
				break;
			case D_fine:
				strcpy(buf, "FINE");
				break;
			}
		}
		PUT3(" (%s) %.2f %s", buf, yc, f);
	} else	PUT2(" %.2f %s", yc, f);
}

/* -- draw the decorations -- */
float draw_decorations(struct SYMBOL *s,
		       float *tp)
{
	int k;
	int staff;
	float top, top1, bot, staffb;
	struct deco *dc;
	unsigned char deco;
	struct deco_def_s *dd;
/*fixme: bottom should be returned*/

	staff = s->staff;
	staffb = staff_tb[staff].y;		/* bottom of staff */

	top = -1000;
	bot = 1000;

	if (s->type == BAR)
		dc = &s->as.u.bar.dc;
	else	dc = &s->as.u.note.dc;

	/* decos close to head */
	for (k = dc->n; --k >= 0; ) {
		deco = dc->t[k];
		if (deco == 0)
			continue;
		deco -= 128;
		dd = &deco_def[deco];
		if (dd->func == 0) {
			ERROR(("Decoration %s not treated",
			       deco_tb[deco]));
			continue;
		}
		if (!(dd->flags & DECO_NEAR))
			continue;
		if (s->type != NOTE) {
			ERROR(("Cannot have a %s on a rest or a bar",
			       deco_tb[deco]));
			continue;
		}
		func_tb[dd->func](s, dd, staffb, &top, &bot);
	}

	top1 = top;

	/* decos further away */
	for (k = dc->n; --k >= 0; ) {
		deco = dc->t[k];
		if (deco == 0)
			continue;
		dd = &deco_def[deco - 128];
		if (dd->func == 0)
			continue;
		if ((dd->flags & DECO_NEAR)
		    || (dd->flags & DECO_NONOTE))
			continue;
		func_tb[dd->func](s, dd, staffb, &top, &bot);
	}

	/* decos not associated with the note */
	for (k = dc->n; --k >= 0; ) {
		deco = dc->t[k];
		if (deco == 0)
			continue;
		dd = &deco_def[deco - 128];
		if (dd->func == 0)
			continue;
		if (!(dd->flags & DECO_NONOTE))
			continue;
		func_tb[dd->func](s, dd, staffb, &top, &bot);
	}

	if (staff_tb[staff].botpos > bot)
		staff_tb[staff].botpos = bot;
	*tp = top;
	return top1;
}

/* -- initialize the default decorations -- */
void reset_deco(int deco_old)
{
	memset(&deco_glob, 0, sizeof deco_glob);

	/* standard */
#if DECO_IS_ROLL
	deco_glob['~'] = D_roll;
#endif
	deco_glob['H'] = D_fermata;
	deco_glob['L'] = D_emphasis;
	deco_glob['M'] = D_lowermordent;
	deco_glob['O'] = D_coda;
	deco_glob['P'] = D_uppermordent;
	deco_glob['S'] = D_segno;
	deco_glob['T'] = D_trill;
	deco_glob['u'] = D_upbow;
	deco_glob['v'] = D_downbow;

	/* non-standard */
#if !DECO_IS_ROLL
	deco_glob['~'] = D_turn;
#endif
	deco_glob['J'] = D_slide;
	deco_glob['R'] = D_roll;

	/* abc2ps */
	if (deco_old) {
		deco_glob['M'] = D_tenuto;
	}
}
