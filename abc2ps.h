/* -- general macros -- */

#include <stdio.h>
#include <time.h>

#include "config.h"
#include "abcparse.h"

#define OUTPUTFILE	"Out.ps"	/* standard output file */
#ifndef WIN32
#define DIRSEP '/'
#else
#define DIRSEP '\\'
#endif

#define CM		* 28.35	/* factor to transform cm to pt */
#define PT			/* factor to transform pt to pt */
#define IN		* 72.0	/* factor to transform inch to pt */

/* basic page dimensions */
#ifdef A4_FORMAT
#define PAGEHEIGHT	(29.7 CM)
#define PAGEWIDTH	(21.0 CM)
#define MARGIN		(1.8 CM)
#else
#define PAGEHEIGHT	(11.0 IN)
#define PAGEWIDTH	(8.5 IN)
#define MARGIN		(0.7 IN)
#endif

/* -- macros controlling music typesetting -- */

#define STEM_YOFF	1.0	/* offset stem from note center */
#define STEM_XOFF	3.5
#define STEM		20	/* default stem height */
#define STEM_MIN	16	/* min stem height under beams */
#define STEM_MIN2	14	/* ... for notes with two beams */
#define STEM_MIN3	12	/* ... for notes with three beams */
#define STEM_MIN4	10	/* ... for notes with four beams */
#define STEM_CH_MIN	14	/* min stem height for chords under beams */
#define STEM_CH_MIN2	10	/* ... for notes with two beams */
#define STEM_CH_MIN3	 9	/* ... for notes with three beams */
#define STEM_CH_MIN4	 9	/* ... for notes with four beams */
#define BEAM_DEPTH	3.2	/* width of a beam stroke */
#define BEAM_OFFSET	0.25	/* pos of flat beam relative to staff line */
#define BEAM_SHIFT	5.0	/* shift of second and third beams (was 5.3) */
/*  To align the 4th beam as the 1st: shift=6-(depth-2*offset)/3  */
#define BEAM_FLATFAC	0.6	/* factor to decrease slope of long beams */
#define BEAM_THRESH	0.06	/* flat beam if slope below this threshold */
#define BEAM_SLOPE	0.5	/* max slope of a beam */
#define BEAM_STUB	6.0	/* length of stub for flag under beam */ 
#define SLUR_SLOPE	1.0	/* max slope of a slur */
#define DOTSHIFT	5	/* dot shift when up flag on note */
#define GSTEM		14	/* grace note stem length */
#define GSTEM_XOFF	1.6	/* x offset for grace note stem */

#define BETA_C		0.1	/* max expansion for flag -c */
#define BETA_X		1.0	/* max expansion before complaining */

#define VOCPRE		0.4	/* portion of vocals word before note */
#define GCHPRE		0.4	/* portion of guitar chord before note */

/* -- Parameters for note spacing -- */
/* -- fnn multiplies the spacing under a beam, to compress the notes a bit
 */

#define fnnp 0.9

/* -- macros for program internals -- */

#define STRL1		256	/* string length for file names */
#define MAXSTAFF	16	/* max staves */
#define BSIZE		512	/* buffer size for one input string */
#define BUFFSZ		64000	/* size of output buffer */

#define BREVE		(BASE_LEN * 2)	/* double note (square note) */
#define SEMIBREVE	BASE_LEN	/* whole note */
#define MINIM		(BASE_LEN / 2)	/* half note (white note) */
#define CROTCHET 	(BASE_LEN / 4)	/* quarter note (black note) */
#define QUAVER		(BASE_LEN / 8)	/* 1/8 note */
#define SEMIQUAVER	(BASE_LEN / 16)	/* 1/16 note */

#define MAXFONTS	30	/* max number of fonts */

#define T_LEFT		0
#define T_JUSTIFY	1
#define T_FILL		2
#define T_CENTER	3
#define T_SKIP		4
#define T_RIGHT		5

#define YSTEP	128		/* number of steps for y offsets */

extern unsigned char deco_glob[256], deco_tune[256];

struct FONTSPEC {
	int fnum;		/* index to font tables in format.c */
	float size;
	float swfac;
};
extern char *fontnames[MAXFONTS];	/* list of font names */

/* lyrics */
#define LY_HYPH	0x10	/* replacement character for hyphen */
#define LY_UNDER 0x11	/* replacement character for underscore */
#define MAXLY	16	/* max number of lyrics */
struct lyl {
	struct FONTSPEC *f;	/* font */
	float w;		/* width */
	float s;		/* shift / note */
	char t[1];		/* word */
};
struct lyrics {
	struct lyl *lyl[MAXLY];	/* ptr to lyric lines */
};

/* music element */
struct SYMBOL { 		/* struct for a drawable symbol */
	struct abcsym as;	/* abc symbol !!must be the first field!! */
	struct SYMBOL *next, *prev;	/* voice linkage */
	unsigned char type;	/* symbol type */
#define NO_TYPE		0	/* invalid type */
#define NOTEREST	1	/* valid symbol types */
#define SPACE		2
#define BAR		3
#define CLEF		4
#define TIMESIG 	5
#define KEYSIG		6
#define TEMPO		7
#define STAVES		8
#define MREST		9
#define PART		10
#define GRACE		11
#define FMTCHG		12
#define TUPLET		13
#define STBRK		14
	unsigned char voice;	/* voice (0..nvoice) */
	unsigned char staff;	/* staff (0..nstaff) */
	unsigned char nhd;	/* number of notes in chord - 1 */
	int dur;		/* main note duration */
	signed char pits[MAXHD]; /* pitches for notes */
	struct SYMBOL *ts_next, *ts_prev; /* time linkage */
	int time;		/* starting time */
	unsigned int sflags;	/* symbol flags */
#define S_EOLN		0x0001		/* end of line */
#define S_BEAM_ST	0x0002		/* beam starts here */
#define S_BEAM_BR1	0x0004		/* 2nd beam must restart here */
#define S_BEAM_BR2	0x0008		/* 3rd beam must restart here */
#define S_BEAM_END	0x0010		/* beam ends here */
#define S_OTHER_HEAD	0x0020		/* don't draw any note head */
#define S_IN_TUPLET	0x0040		/* in a tuplet */
#define S_TREM2		0x0080		/* tremolo on 2 notes */
#define S_RRBAR		0x0100		/* right repeat bar (when bar) */
#define S_XSTEM		0x0200		/* cross-staff stem (when note) */
#define S_BEAM_ON	0x0400		/* continue beaming */
#define S_SL1		0x0800		/* some chord slur start */
#define S_SL2		0x1000		/* some chord slur end */
#define S_TI1		0x2000		/* some chord tie start */
#define S_PERC		0x4000		/* percussion */
#define S_RBSTOP	0x8000		/* repeat bracket stop */
#define S_REPEAT	0x00020000	/* sequence / measure repeat */
#define S_NL		0x00040000	/* start of new music line */
#define S_SEQST		0x00080000	/* start of vertical sequence */
#define S_SECOND	0x00100000	/* symbol on a secondary voice */
#define S_FLOATING	0x00200000	/* symbol on a floating voice */
#define S_NOREPBRA	0x00400000	/* don't print the repeat bracket */
#define S_TREM1		0x00800000	/* tremolo on 1 note */
	unsigned short posit;		/* indication position / staff (2 bits) */
					/* 0: auto, 1: above, 2: below */
#define POS_DYN 0			/* shifts */
#define POS_GCH 2
#define POS_ORN 4
#define POS_VOC 6
#define POS_VOL 8
	signed char stem;	/* 1 / -1 for stem up / down */
	signed char nflags;	/* number of note flags when > 0 */
	char dots;		/* number of dots */
	unsigned char head;	/* head type */
#define H_FULL		0
#define H_EMPTY 	1
#define H_OVAL		2
#define H_SQUARE	3
	signed char multi;	/* multi voice in the staff (+1, 0, -1) */
	signed char nohdix;	/* no head index (for unison) */
	unsigned char gcf;	/* font for guitar chords */
	unsigned char anf;	/* font for annotations */
	short u;		/* auxillary information:
				 *	- CLEF: small clef
				 *	- KEYSIG: old key signature
				 *	- BAR: new bar number
				 *	- TUPLET: tuplet format
				 *	- NOTE: tremolo number
				 *	- FMTCHG (format change): subtype */
#define PSSEQ 0				/* postscript sequence */
#define SVGSEQ 1			/* SVG sequence */
#define REPEAT 2			/* repeat sequence or measure
					 *	doty: # measures if > 0
					 *	      # notes/rests if < 0
					 *	nohdix: # repeat */
	float x;		/* x offset */
	signed char y;		/* y offset of note head */
	signed char ymn, ymx, yav; /* min, max, avg note head y offset */
	float xmx;		/* max h-pos of a head rel to top
				 * width when STBRK */
	float xs, ys;		/* offset of stem end */
	float wl, wr;		/* left, right min width */
	float space;		/* natural space before symbol */
	float shrink;		/* minimum space before symbol */
	float xmax;		/* max x offset */
	float shhd[MAXHD];	/* horizontal shift for heads */
	float shac[MAXHD];	/* horizontal shift for accidentals */
	struct lyrics *ly;	/* lyrics */
	struct SYMBOL *extra;	/* extra symbols (grace notes, tempo... */
	signed char doty;	/* dot y pos when voices overlap
				 * forced when STBRK */
};

/* bar types !tied to abcparse.h! */
#define B_SINGLE B_BAR		/* |	single bar */
#define B_DOUBLE 0x11		/* ||	thin double bar */
#define B_THIN_THICK 0x13	/* |]	thick at section end  */
#define B_THICK_THIN 0x21	/* [|	thick at section start */
#define B_LREP 0x14		/* |:	left repeat bar */
#define B_RREP 0x41		/* :|	right repeat bar */
#define B_DREP 0x44		/* ::	double repeat bar */
#define B_DASH 0x04		/* :	dashed bar */

extern unsigned short *micro_tb; /* ptr to the microtone table of the tune */

struct FORMAT { 		/* struct for page layout */
	float pageheight, pagewidth;
	float topmargin, botmargin, leftmargin, rightmargin;
	float topspace, wordsspace, titlespace, subtitlespace, partsspace;
	float composerspace, musicspace, staffsep, vocalspace, textspace;
	float scale, maxshrink, lineskipfac, parskipfac, sysstaffsep;
	float indent, infospace, slurheight, notespacingfactor;
	float maxstaffsep, maxsysstaffsep, stemheight;
	int abc2pscompat, alignbars, aligncomposer, autoclef;
	int barsperstaff, breakoneoln, bstemdown, comball;
	int combinevoices, contbarnb, continueall, dynalign, dynamic;
	int flatbeams;
	int infoline, gchord, gchordbox, graceslurs, gracespace, hyphencont;
	int landscape, linewarn, measurebox, measurefirst, measurenb;
	int oneperpage, ornament;
#ifdef HAVE_PANGO
	int pango;
#endif
	int partsbox, pdfmark;
	int setdefl, shiftunisson, splittune, squarebreve;
	int staffnonote, straightflags, stretchstaff, stretchlast;
	int textoption, titlecaps, titleleft, titletrim;
	int timewarn, transpose, tuplets, vocal, volume;
	char *bgcolor, *dateformat, *header, *footer, *titleformat;
#define FONT_UMAX 5		/* max number of user fonts */
#define ANNOTATIONFONT 5
#define COMPOSERFONT 6
#define FOOTERFONT 7
#define GCHORDFONT 8
#define HEADERFONT 9
#define HISTORYFONT 10
#define INFOFONT 11
#define MEASUREFONT 12
#define PARTSFONT 13
#define REPEATFONT 14
#define SUBTITLEFONT 15
#define TEMPOFONT 16
#define TEXTFONT 17
#define TITLEFONT 18
#define VOCALFONT 19
#define VOICEFONT 20
#define WORDSFONT 21
#define FONT_DYN 22		/* index of dynamic fonts (gch, an, ly) */
#define FONT_DYNX 12		/* number of dynamic fonts */
#define FONT_MAX (FONT_DYN+FONT_DYNX)		/* whole number of fonts */
	struct FONTSPEC font_tb[FONT_MAX];
	char ndfont;		/* current index of dynamic fonts */
	unsigned char gcf, anf, vof;	/* fonts for
				 * guitar chords, annotations and lyrics */
	unsigned short posit;	/* position of indications / staff */
	unsigned int fields[2];	/* info fields to print
				 *[0] is 'A'..'Z', [1] is 'a'..'z' */
};

extern struct FORMAT cfmt;	/* current format */
extern struct FORMAT dfmt;	/* global format */

typedef struct SYMBOL *INFO[26]; /* information fields ('A' .. 'Z') */
extern INFO info;

extern char *outbuf;		/* output buffer.. should hold one tune */
extern char *mbf;		/* where to PUTx() */
extern int use_buffer;		/* 1 if lines are being accumulated */

extern int outft;		/* last font in the output file */
extern int tunenum;		/* number of current tune */
extern int pagenum;		/* current page number */
extern int nbar;		/* current measure number */
extern int nbar_rep;		/* last repeat bar number */
extern int in_page;
extern int defl;		/* decoration flags */
#define DEF_NOST 0x01		/* long deco with no start */
#define DEF_NOEN 0x02		/* long deco with no end */
#define DEF_STEMUP 0x04		/* stem up (1) or down (0) */

		/* switches modified by flags: */
extern int quiet;		/* quiet mode */
extern int secure;		/* secure mode */
extern int annotate;		/* output source references */
extern int pagenumbers; 	/* write page numbers */
extern int epsf;		/* EPSF (1) / SVG (2) output */
extern int svg;			/* SVG (1) or XML (2 - HTML + SVG) output */
extern int showerror;		/* show the errors */

extern char outfn[FILENAME_MAX]; /* output file name */
extern char *in_fname;		/* current input file name */
extern time_t mtime;		/* last modification time of the input file */

extern int file_initialized;	/* for output file */
extern FILE *fout;		/* output file */

#define MAXTBLT 8
struct tblt_s {
	char *head;		/* PS head function */
	char *note;		/* PS note function */
	char *bar;		/* PS bar function */
	float wh;		/* width of head */
	float ha;		/* height above the staff */
	float hu;		/* height under the staff */
	short pitch;		/* pitch when no associated 'w:' / 0 */
	char instr[2];		/* instrument pitch */
};
extern struct tblt_s *tblts[MAXTBLT];

#define MAXCMDTBLT	4	/* max number of -T in command line */
struct cmdtblt_s {
	short index;		/* tablature number */
	short active;		/* activate or not */
	char *vn;		/* voice name */
};
extern struct cmdtblt_s cmdtblts[MAXCMDTBLT];
extern int ncmdtblt;

extern int s_argc;		/* command line arguments */
extern char **s_argv;

struct STAFF_S {
	struct clef_s clef;	/* base clef */
	char forced_clef;	/* explicit clef */
	char empty;		/* no symbol on this staff */
	short botbar, topbar;	/* bottom and top of bar */
	float y;		/* y position */
	float top[YSTEP], bot[YSTEP];	/* top/bottom y offsets */
};
extern struct STAFF_S staff_tb[MAXSTAFF];
extern int nstaff;		/* (0..MAXSTAFF-1) */

struct VOICE_S {
	struct SYMBOL *sym;	/* associated symbols */
	struct SYMBOL *last_sym; /* last symbol while scanning */
	struct VOICE_S *next;	/* link */
	char id[VOICE_ID_SZ];	/* voice id */
	char *nm;		/* voice name */
	char *snm;		/* voice subname */
	char *bar_text;		/* bar text at start of staff when bar_start */
	struct SYMBOL *tie;	/* note with ties of previous line */
	struct SYMBOL *rtie;	/* note with ties before 1st repeat bar */
	struct tblt_s *tblts[2]; /* tablatures */
	float scale;		/* scale */
	int time;		/* current time while parsing */
	struct clef_s clef;	/* current clef */
	struct key_s key;	/* current key signature */
	struct meter_s meter;	/* current time signature */
	struct key_s ckey;	/* key signature while parsing */
	struct key_s okey;	/* original key signature while parsing */
	unsigned hy_st;		/* lyrics hyphens at start of line (bit array) */
	unsigned ignore:1;	/* ignore this voice */
	unsigned forced_clef:1;	/* explicit clef */
	unsigned second:1;	/* secondary voice in a brace/parenthesis */
	unsigned floating:1;	/* floating voice in a brace system */
	unsigned bar_repeat:1;	/* bar at start of staff is a repeat bar */
	unsigned norepbra:1;	/* don't display the repeat brackets */
	unsigned have_ly:1;	/* some lyrics in this voice */
	unsigned new_name:1;	/* redisplay the voice name */
	unsigned space:1;	/* have a space before the next note (parsing) */
	unsigned perc:1;	/* percussion */
	short wmeasure;		/* measure duration while parsing */
	short transpose;	/* transposition while parsing */
	short bar_start;	/* bar type at start of staff / 0 */
	unsigned short posit;	/* position of indications / staff */
	signed char clone;	/* duplicate from this voice number */
	unsigned char staff;	/* staff (0..n-1) */
	unsigned char cstaff;	/* staff while parsing */
	signed char stem;	/* stem direction while parsing */
	signed char gstem;	/* grace stem direction while parsing */
	unsigned char slur_st;	/* slurs at start of staff */
};
extern struct VOICE_S voice_tb[MAXVOICE]; /* voice table */
extern struct VOICE_S *first_voice; /* first_voice */

extern struct SYMBOL *tsfirst;	/* first symbol in the time linked list */
extern struct SYMBOL *tsnext;	/* next line when cut */
extern float realwidth;		/* real staff width while generating */

#define NFLAGS_SZ 10		/* size of note flags tables */
#define C_XFLAGS 5		/* index of crotchet in flags tables */
extern float space_tb[NFLAGS_SZ]; /* note spacing */

struct SYSTEM {			/* staff system */
	struct SYSTEM *next;
	short top_voice;	/* first voice in the staff system */
	short nstaff;
	struct {
		short flags;
#define OPEN_BRACE 0x01
#define CLOSE_BRACE 0x02
#define OPEN_BRACKET 0x04
#define CLOSE_BRACKET 0x08
#define OPEN_PARENTH 0x10
#define CLOSE_PARENTH 0x20
#define STOP_BAR 0x40
#define FL_VOICE 0x80
#define OPEN_BRACE2 0x0100
#define CLOSE_BRACE2 0x0200
#define OPEN_BRACKET2 0x0400
#define CLOSE_BRACKET2 0x0800
		char empty;
		char dum;
		struct clef_s clef;
		float sep, maxsep;
	} staff[MAXSTAFF];
	struct {
		signed char range;
		unsigned char staff;
		char second;
		char dum;
		float sep, maxsep;
		struct clef_s clef;
	} voice[MAXVOICE];
};
struct SYSTEM *cursys;		/* current staff system */

/* -- external routines -- */
/* abc2ps.c */
void clrarena(int level);
int lvlarena(int level);
void *getarena(int len);
void strext(char *fid, char *ext);
/* buffer.c */
#define PUT0(f) a2b(f)
#define PUT1(f,a) a2b(f,a)
#define PUT2(f,a,b) a2b(f,a,b)
#define PUT3(f,a,b,c) a2b(f,a,b,c)
#define PUT4(f,a,b,c,d) a2b(f,a,b,c,d)
#define PUT5(f,a,b,c,d,e) a2b(f,a,b,c,d,e)
void a2b(char *fmt, ...)
#ifdef __GNUC__
	__attribute__ ((format (printf, 1, 2)))
#endif
	;
void abskip(float h);
void buffer_eob(void);
void marg_init(void);
void bskip(float h);
void check_buffer(void);
void clear_buffer(void);
void close_output_file(void);
void close_page(void);
float get_bposy(void);
void write_buffer(void);
void open_output_file(void);
int (*output)(FILE *out, const char *fmt, ...)
#ifdef __GNUC__
	__attribute__ ((format (printf, 2, 3)))
#endif
	;
void write_eps(void);
/* deco.c */
void deco_add(char *text);
void deco_cnv(struct deco *dc, struct SYMBOL *s, struct SYMBOL *prev);
unsigned char deco_intern(unsigned char deco);
void deco_update(struct SYMBOL *s, float dx);
float deco_width(struct SYMBOL *s);
void draw_all_deco(void);
int draw_deco_head(int deco, float x, float y, int stem);
void draw_all_deco_head(struct SYMBOL *s, float x, float y);
void draw_deco_near(void);
void draw_deco_note(void);
void draw_deco_staff(void);
float draw_partempo(float top,
		    int any_part,
		    int any_tempo);
void draw_measnb(void);
void reset_deco(void);
void set_defl(int new_defl);
float tempo_width(struct SYMBOL *s);
void write_tempo(struct SYMBOL *s,
		int beat,
		float sc);
float y_get(struct SYMBOL *s,
	    int up,
	    float x,
	    float w,
	    float h);
void y_set(struct SYMBOL *s,
	   int up,
	   float x,
	   float w,
	   float y);
/* draw.c */
void draw_sym_near(void);
void draw_all_symb(void);
float draw_systems(float indent);
void output_ps(struct SYMBOL *s, int state);
void putf(float f);
void putx(float x);
void puty(float y);
void putxy(float x, float y);
void set_scale(struct SYMBOL *s);
void set_sscale(int staff);
/* format.c */
void define_fonts(void);
int get_textopt(char *p);
void interpret_fmt_line(char *w, char *p, int lock);
void lock_fmt(void *fmt);
void make_font_list(void);
FILE *open_file(char *fn,
		char *ext,
		char *rfn);
void print_format(void);
void set_font(int ft);
void set_format(void);
struct tblt_s *tblt_parse(char *p);
/* music.c */
void output_music(void);
void reset_gen(void);
/* parse.c */
extern float multicol_start;
void do_tune(struct abctune *t,
	     int header_only);
void identify_note(struct SYMBOL *s,
		   int len,
		   int *p_head,
		   int *p_dots,
		   int *p_flags);
struct SYMBOL *sym_add(struct VOICE_S *p_voice,
		       int type);
/* subs.c */
void bug(char *msg, int fatal);
void error(int sev, struct SYMBOL *s, char *fmt, ...);
float scan_u(char *str);
float cwid(unsigned short c);
void get_str_font(int *cft, int *dft);
void set_str_font(int cft, int dft);
#ifdef HAVE_PANGO
void pg_init(void);
void pg_reset_font(void);
#endif
void put_history(void);
void put_words(struct SYMBOL *words);
void str_font(int ft);
#define A_LEFT 0
#define A_CENTER 1
#define A_RIGHT 2
#define A_LYRIC 3
#define A_GCHORD 4
#define A_ANNOT 5
#define A_GCHEXP 6
void str_out(char *p, int action);
void str_ft_out(char *p, int end);
void put_str(char *str, int action);
float tex_str(char *s);
extern char tex_buf[];	/* result of tex_str() */
#define TEX_BUF_SZ 512
char *trim_title(char *p, int first);
void user_ps_add(char *s, char use);
void user_ps_write(void);
void write_title(struct SYMBOL *s);
void write_heading(struct abctune *t);
void write_user_ps(void);
void write_text(char *cmd, char *s, int job);
/* svg.c */
void define_svg_symbols(char *title, float w, float h);
int svg_output(FILE *out, const char *fmt, ...)
#ifdef __GNUC__
	__attribute__ ((format (printf, 2, 3)))
#endif
	;
void svg_write(char *buf, int len);
void svg_close();
/* syms.c */
void define_cmap(void);
void define_font(char *name, int num, int enc);
void define_symbols(void);
