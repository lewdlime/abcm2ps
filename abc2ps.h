/* -- general macros -- */

#include "config.h"

#define VERBOSE0	2		/* default verbosity */
#define OUTPUTFILE	"Out.ps"	/* standard output file */
#if defined(unix) || defined(__unix__)
#define DIRSEP '/'
#else
#define DIRSEP '\\'
#endif

/* basic page dimensions */
#ifdef A4_FORMAT
#define PAGEHEIGHT	(29.7 * CM)
#define PAGEWIDTH	(21.0 * CM)
#define MARGIN		(1.8 * CM)
#else
#define PAGEHEIGHT	(11.0 * IN)
#define PAGEWIDTH	(8.5 * IN)
#define MARGIN		(0.7 * IN)
#endif

/* -- macros controlling music typesetting -- */

#define STEM_YOFF	1.0	/* offset stem from note center */
#define STEM_XOFF	3.5
#define STEM		20	/* standard stem length */
#define STEM_MIN	16	/* min stem length under beams */
#define STEM_MIN2	12	/* ... for notes with two beams */
#define STEM_MIN3	10	/* ... for notes with three beams */
#define STEM_MIN4	10	/* ... for notes with four beams */
#define STEM_CH 	16	/* standard stem length for chord */
#define STEM_CH_MIN	12	/* min stem length for chords under beams */
#define STEM_CH_MIN2	 8	/* ... for notes with two beams */
#define STEM_CH_MIN3	 7	/* ... for notes with three beams */
#define STEM_CH_MIN4	 7	/* ... for notes with four beams */
#define BEAM_DEPTH	3.2	/* width of a beam stroke (was 2.6) */
#define BEAM_OFFSET	0.25	/* pos of flat beam relative to staff line */
#define BEAM_SHIFT	5.0	/* shift of second and third beams (was 5.3) */
/*  To align the 4th beam as the 1st: shift=6-(depth-2*offset)/3  */
#define BEAM_FLATFAC	0.6	/* factor to decrease slope of long beams */
#define BEAM_THRESH	0.06	/* flat beam if slope below this threshold */
#define BEAM_SLOPE	0.5	/* max slope of a beam */
#define BEAM_STUB	6.0	/* length of stub for flag under beam */ 
#define SLUR_SLOPE	1.0	/* max slope of a slur */
#define DOTSHIFT	5	/* shift dot when up flag on note */
#define GSTEM		13.0	/* grace note stem length */
#define GSTEM_XOFF	2.0	/* x offset for grace note stem */
#define GSPACE0		12.0	/* space from grace note to big note */
#define GSPACE		8.0	/* space between grace notes */
#define CUT_NPLETS	0	/* 1 to always cut nplets off beams */
#define RANFAC		0.05	/* max random shift = RANFAC * spacing */

#define BETA_C		0.1	/* max expansion for flag -c */
#define ALFA_X		1.0	/* max compression before complaining */
#define BETA_X		1.2	/* max expansion before complaining */

#define VOCPRE		0.4	/* portion of vocals word before note */
#define GCHPRE		0.4	/* portion of guitar chord before note */
#define LYDIG_SH	3.	/* shift for lyrics starting with a digit */

/* -- Parameters for note spacing -- */
/* -- bnn determines how strongly the first note enters into the spacing.
      For bnn=1, the spacing is calculated using the first note.
      For bnn=0, the spacing is the average for the two notes.
   -- fnn multiplies the spacing under a beam, to compress the notes a bit
   -- gnn multiplies the spacing a second time within an n-tuplet
 */

#define bnnp 0.9
#define fnnp 0.9
#define gnnp 0.8

/* -- macros for program internals -- */

#define CM		28.35	/* factor to transform cm to pt */
#define PT		1.00	/* factor to transform pt to pt */
#define IN		72.00	/* factor to transform inch to pt */

#define STRL1		128	/* string length for file names */
#define MAXSTAFF	16	/* max staves */
#define BSIZE		512	/* buffer size for one input string */
#define BUFFSZ		40000	/* size of output buffer */
#define BUFFSZ1		5000	/* buffer reserved for one staff */

#define BREVE		(BASE_LEN * 2)	/* double note (square note) */
#define SEMIBREVE	BASE_LEN	/* whole note */
#define MINIM		(BASE_LEN / 2)	/* half note (white note) */
#define CROTCHET 	(BASE_LEN / 4)	/* quarter note (black note) */
#define QUAVER		(BASE_LEN / 8)	/* 1/8 note */
#define SEMIQUAVER	(BASE_LEN / 16)	/* 1/16 note */

#define SWFAC		0.50	/* factor to estimate width of string */

#define MAXFORMATS	10	/* max number of defined page formats */
#define STRLFMT		81	/* string length in FORMAT struct */
#define MAXFONTS	20	/* max number of fonts */
#define MAXENC		6	/* max number of ISO-Latin encodings */

#define OBEYLINES	0
#define T_JUSTIFY	1
#define T_FILL		2
#define OBEYCENTER	3
#define SKIP		4

struct SYMBOL;
#define NCOMP	5		/* max number of composer lines */
#define NTITLE	3		/* max number of title lines */

struct ISTRUCT {		/* information fields */
	char *area;
	char *book;
	char *comp[NCOMP];
	char *orig;
	char *parts;
	char *rhyth;
	char *src;
	struct SYMBOL *tempo;
	char *title[NTITLE];
	char *xref;
	short ncomp;
	short ntitle;
};

extern struct ISTRUCT info, default_info;

extern unsigned char deco_glob[256], deco_tune[256];

/* lyrics */
#define MAXLY	16	/* max number of lyrics */
struct lyrics {
	unsigned char *w[MAXLY];	/* ptr to words */
};

/* lyric fonts */
struct lyric_fonts_s {
	int font;
	float size;
};
extern struct lyric_fonts_s lyric_fonts[8];
extern int nlyric_font;

/* music element */
struct SYMBOL { 		/* struct for a drawable symbol */
	struct abcsym as;	/* abc symbol !!must be the first field!! */
	struct SYMBOL *next, *prev;	/* voice linkage */
	unsigned char type;	/* symbol type */
#define NO_TYPE		0	/* invalid type */
#define NOTE		1	/* valid symbol types */
#define REST		2
#define BAR		3
#define CLEF		4
#define TIMESIG 	5
#define KEYSIG		6
#define TEMPO		7
#define STAVES		8
#define MREST		9
#define PART		10
#define MREP		11
#define GRACE		12
#define FMTCHG		13
	unsigned char seq;	/* sequence # - see parse.c */
	unsigned char voice;	/* voice (0..nvoice) */
	unsigned char staff;	/* staff (0..nstaff) */
	int	len;		/* main note length */
	signed char pits[MAXHD]; /* pitches for notes */
	struct SYMBOL *ts_next, *ts_prev; /* time linkage */
	int	time;		/* starting time */
	unsigned char sflags;	/* symbol flags */
#define S_EOLN		0x0001	/* end of line */
#define S_WORD_ST	0x0002	/* word starts here */
#define S_BEAM_BREAK	0x0004	/* 2nd beam must restart here */
#define S_NO_HEAD	0x0008	/* don't draw note head */
#define S_2S_BEAM	0x0010	/* beam on 2 staves */
#define S_NPLET_ST	0x0020	/* start or in a n-plet sequence */
#define S_NPLET_END	0x0040	/* end or in a n-plet sequence */
#define S_RRBAR		0x0080	/* right repeat bar */
	unsigned char nhd;	/* number of notes in chord - 1 */
	signed char stem;	/* 1 / -1 for stem up / down */
	signed char nflags;	/* number of note flags when > 0 */
	char	dots;		/* number of dots */
	char	head;		/* head type */
#define H_FULL		0
#define H_EMPTY 	1
#define H_OVAL		2
#define H_SQUARE	3
	signed char multi;	/* multi voice in the staff (+1, 0, -1) */
	short	u;		/* auxillary information:
				 *	- small clef when CLE
				 *	- old key signature when KEYSIG
				 *	- reset bar number when BAR
				 *	- subtype when FMTCHG (format change)
				 *	  with value in xmx: */
#define STBRK 0				/* staff break */
#define PSSEQ 1				/* postscript sequence */
	short	doty;		/* dot y pos when voices overlap */
	float	x;		/* position */
	short	y;
	short	ymn, ymx, yav;	/* min,max,avg note head height */
	float	xmx;		/* max h-pos of a head rel to top */
	float	dc_top;		/* max offset needed for decorations */
	float	dc_bot;		/* min offset for decoration */
	float	xs, ys;		/* position of stem end */
	float	wl, wr;		/* left,right min width */
	float	pl, pr;		/* left,right preferred width */
	float	shrink, stretch; /* glue before this symbol */
	float	shhd[MAXHD];	/* horizontal shift for heads */
	float	shac[MAXHD];	/* horizontal shift for accidentals */
	struct lyrics *ly;	/* lyrics */
	struct SYMBOL *grace;	/* grace notes */
};

/* bar types */
#define B_INVIS B_OBRA		/* invisible; for endings without bars */
#define B_SINGLE B_BAR		/* |	single bar */
#define B_DOUBLE 0x11		/* ||	thin double bar */
#define B_THIN_THICK 0x13	/* |]	thick at section end  */
#define B_THICK_THIN 0x21	/* [|	thick at section start */
#define B_LREP 0x14		/* |:	left repeat bar */
#define B_RREP 0x41		/* :|	right repeat bar */
#define B_DREP 0x44		/* ::	double repeat bar */
#define B_DASH 0x04		/* :	dashed bar */

struct FONTSPEC {
	int	fnum;		/* index to fontnames in format.c */
	float	size;
	float	swfac;
};

struct FORMAT { 		/* struct for page layout */
	float	pageheight, pagewidth;
	float	topmargin, botmargin, leftmargin, rightmargin;
	float	topspace, wordsspace, titlespace, subtitlespace, partsspace;
	float	composerspace, musicspace, staffsep, vocalspace, textspace;
	float	scale, maxshrink, lineskipfac, parskipfac, sysstaffsep;
	float	indent, infospace, slurheight, notespacingfactor;
	int	landscape, titleleft, continueall, writehistory;
	int	stretchstaff, stretchlast, withxrefs, barsperstaff;
	int	oneperpage, musiconly, titlecaps, graceslurs, straightflags;
	int	splittune, encoding, partsbox, infoline, printtempo, autoclef;
	int	measurenb, measurefirst, measurebox, flatbeams, squarebreve;
	int	exprabove, exprbelow, breathlow, vocalabove, freegchord;
	int	printparts, gchordbox;
	char	*footer, *header;
	struct FONTSPEC titlefont, subtitlefont, vocalfont, textfont;
	struct FONTSPEC tempofont, composerfont, partsfont, gchordfont;
	struct FONTSPEC wordsfont, footerfont, headerfont, infofont;
	struct FONTSPEC repeatfont, measurefont;
};

extern struct FORMAT cfmt;	/* current format for output */

extern char *mbf;		/* where to PUTx() */
extern int nbuf;		/* number of bytes buffered */
extern int use_buffer;		/* 1 if lines are being accumulated */

extern char page_init[201];	/* initialization string after page break */
extern int tunenum;		/* number of current tune */
extern int pagenum;		/* current page number */
extern int nbar;		/* current measure number */
extern int nbar_rep;		/* last repeat bar number */

extern int in_page;

				 /* switches modified by flags: */
extern int pagenumbers; 		/* write page numbers ? */
extern int epsf;			/* for EPSF postscript output */
extern int choose_outname;		/* 1 names outfile w. title/fnam */
extern int break_continues;		/* ignore continuations ? */

extern char outf[STRL1];		/* output file name */
extern char *in_fname;			/* current input file name */

extern int file_initialized;		/* for output file */
extern FILE *fout;			/* output file */

#define MAXWHISTLE	4	/* max number of whistle tablature */
struct WHISTLE_S {
	short voice;		/* voice number */
	short pitch;		/* absolute key pitch */
};
extern struct WHISTLE_S whistle_tb[MAXWHISTLE];
extern int nwhistle;

extern int s_argc;		/* command line arguments */
extern char **s_argv;

struct STAFF {
	struct clef_s clef;	/* base clef */
	unsigned brace:1;	/* 1st staff of a brace */
	unsigned brace_end:1;	/* 2nd staff of a brace */
	unsigned bracket:1;	/* 1st staff of a bracket */
	unsigned bracket_end:1;	/* last staff of a bracket */
	unsigned forced_clef:1;	/* explicit clef */
	unsigned stop_bar:1;	/* stop drawing bar on this staff */
	float	y;		/* y position */
	short	nvocal;		/* number of vocals (0..n) */
};
extern struct STAFF staff_tb[MAXSTAFF];
extern int nstaff;		/* (0..MAXSTAFF-1) */

struct VOICE_S {
	struct SYMBOL *sym;	/* associated symbols */
	struct SYMBOL *last_symbol; /* last symbol while scanning */
	struct SYMBOL *s_anc;	/* ancillary symbol pointer */
	struct VOICE_S *next, *prev;	/* staff links */
	char	*name;		/* voice id */
	char	*nm;		/* voice name */
	char	*snm;		/* voice subname */
	struct clef_s clef;	/* current clef */
	struct key_s key;	/* current key signature */
	struct meter_s meter;	/* current time signature */
	float	yvocal;		/* current vocal vertical offset */
	char	*bar_text;	/* bar text at start of staff when bar_start */
	int	time;		/* current time while parsing */
	unsigned forced_clef:1;	/* explicit clef */
	unsigned second:1;	/* secondary voice in a brace/parenthesis */
	unsigned floating:1;	/* floating voice in a brace */
	unsigned selected:1;	/* selected while sorting by time (music.c) */
	unsigned bar_repeat:1;	/* bar at start of staff is a repeat bar */
	signed char bar_start;	/* bar type at start of staff / -1 */
	unsigned char r_plet;	/* number of n-plet notes while parsing */
	unsigned char nvocal;	/* number of vocals (0..n) */
	signed char clone;	/* duplicate from this voice number */
	unsigned char staff;	/* staff (0..n-1) */
	signed char sfp;	/* key signature while parsing */
	signed char stem;	/* stem direction while parsing */
};
extern struct VOICE_S voice_tb[MAXVOICE];	/* voice table */
extern int nvoice;		/* (0..MAXVOICE-1) */
extern int current_voice;	/* current voice while parsing */
extern struct VOICE_S *first_voice;	/* first_voice */

extern float realwidth;		/* real staff width while generating */

#define SPACETB_SZ 9
extern float space_tb[SPACETB_SZ], dot_space;	/* note spacing */

/* PUTn: add to buffer with n arguments */
#define PUT0(f) do {sprintf(mbf,f); a2b(); } while (0)
#define PUT1(f,a) do {sprintf(mbf,f,a); a2b(); } while (0)
#define PUT2(f,a,b) do {sprintf(mbf,f,a,b); a2b(); } while (0)
#define PUT3(f,a,b,c) do {sprintf(mbf,f,a,b,c); a2b(); } while (0)
#define PUT4(f,a,b,c,d) do {sprintf(mbf,f,a,b,c,d); a2b(); } while (0)
#define PUT5(f,a,b,c,d,e) do {sprintf(mbf,f,a,b,c,d,e); a2b(); } while (0)

/* -- external routines -- */
/* abc2ps.c */
void clrarena(int level);
void lvlarena(int level);
char *getarena(int len);
void ops_into_fmt(void);
void strext(char *fid,
	    char *ext);
/* buffer.c */
void a2b(void);
void abskip(float h);
void buffer_eob(void);
void bskip(float h);
void check_buffer(void);
void clear_buffer(void);
void close_output_file(void);
float get_bposy(void);
void write_buffer(void);
void open_output_file(void);
void set_buffer(float *p_v);
void write_eps(void);
void write_pagebreak(void);
/* deco.c */
void deco_add(char *text);
void deco_cnv(struct deco *dc, struct SYMBOL *s);
unsigned char deco_intern(unsigned char deco);
void deco_update(struct SYMBOL *s, float dx);
float deco_width(struct SYMBOL *s);
void draw_all_deco(void);
void draw_deco_near(void);
void draw_deco_note(void);
void draw_deco_staff(void);
void reset_deco(int deco_old);
float draw_partempo(float top,
		    int any_part,
		    int any_tempo,
		    int any_vocal);
/* draw.c */
void draw_staff(int mline,
		float indent);
void draw_sym_near(void);
void draw_symbols(struct VOICE_S *p_voice);
void draw_whistle(void);
/* format.c */
int interpret_format_line(char *w,
			  char *p);
void define_fonts(void);
void make_font_list(void);
void print_format(void);
int read_fmt_file(char *filename,
		  char *dirname);
void set_format(void);
/* music.c */
void output_music(void);
void reset_gen(void);
/* parse.c */
extern float multicol_start;
struct SYMBOL *add_sym(struct VOICE_S *p_voice,
		       int type);
void voice_dup(void);
void do_tune(struct abctune *t,
	     int header_only);
void identify_note(struct SYMBOL *s,
		   int len,
		   int *p_head,
		   int *p_dots,
		   int *p_flags);
struct SYMBOL *ins_sym(int type,
		       struct SYMBOL *s);
/* subs.c */
void bug(char *msg,
	 int fatal);
void error(int sev, int linenum, char *fmt, ...);
void cap_str(unsigned char *c);
float ranf(float x1,
	   float x2);
float scan_u(char *str);
/* types of a text line */
#define TEXT_H	0	/* H: */
#define TEXT_W	1	/* W: */
#define TEXT_Z	2	/* Z: */
#define TEXT_N	3	/* N: */
#define TEXT_D	4	/* D: */
#define TEXT_PS 5	/* postscript format */
#define TEXT_MAX 6
void add_text(char *str,
	      int type);
void add_to_text_block(char *s,
		       int job);
void clear_text(void);
float cwid(unsigned char c);
int is_xrefstr(char *str);
void put_history(void);
void put_words(void);
void set_font(struct FONTSPEC *font);
void tex_str(char *d,
	     char *s,
	     int maxlen,
	     float *wid);
void write_title(int i);
void write_heading(void);
void write_user_ps(void);
void write_text_block(int job,
		      int abc_state);
/* syms.c */
void define_encoding(int enc);
void define_font(char *name,
		 int num);
void define_symbols(void);
