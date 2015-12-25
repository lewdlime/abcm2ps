/* -- general macros -- */

#ifndef VERSION
#define VERSION		"1.6.12"	/* default version */
#endif
#define VDATE		"Dec 30, 2000"	/* version date */
#define VERBOSE0	2		/* default verbosity */
#define OUTPUTFILE	"Out.ps"	/* standard output file */
#define PS_LEVEL	2		/* PS language level: must be 1 or 2 */
#ifdef unix
#define DIRSEP '/'
#else
#define DIRSEP '\\'
#endif

/* default directory to search for format files */
#define DEFAULT_FDIR	""

/* basic page dimensions */
#ifdef US_LETTER
#define PAGEHEIGHT	(11.0 * IN)
#define STAFFWIDTH	(7.1 * IN)
#define LEFTMARGIN	(0.7 * IN)
#else
#define PAGEHEIGHT	(29.7 * CM)
#define STAFFWIDTH	(17.4 * CM)
#define LEFTMARGIN	(1.8 * CM)
#endif

/* -- macros controlling music typesetting -- */

#define BASEWIDTH	0.8	/* width for lines drawn within music */
#define SLURWIDTH	0.8	/* width for lines for slurs */
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
#define BEAM_DEPTH	3.0	/* width of a beam stroke (was 2.6) */
#define BEAM_OFFSET	0.25	/* pos of flat beam relative to staff line */
#define BEAM_SHIFT	4.9	/* shift of second and third beams (was 5.3) */
/*  To align the 4th beam as the 1st: shift=6-(depth-2*offset)/3  */
#define BEAM_FLATFAC	0.6	/* factor to decrease slope of long beams */
#define BEAM_THRESH	0.06	/* flat beam if slope below this threshold */
#define BEAM_SLOPE	0.5	/* max slope of a beam */
#define BEAM_STUB	6.0	/* length of stub for flag under beam */ 
#define SLUR_SLOPE	1.0	/* max slope of a slur */
#define DOTSHIFT	5	/* shift dot when up flag on note */
#define GSTEM		10.0	/* grace note stem length */
#define GSTEM_XOFF	2.0	/* x offset for grace note stem */
#define GSPACE0		10.0	/* space from grace note to big note */
#define GSPACE		7.0	/* space between grace notes */
#define DECO_IS_ROLL	0	/* ~ makes roll if 1, otherwise twiddle */
#define CUT_NPLETS	0	/* 1 to always cut nplets off beams */
#define RANFAC		0.05	/* max random shift = RANFAC * spacing */

#define BETA_C		0.1	/* max expansion for flag -c */
#define ALFA_X		1.0	/* max compression before complaining */
#define BETA_X		1.2	/* max expansion before complaining */

#define VOCPRE		0.4	/* portion of vocals word before note */
#define GCHPRE		0.4	/* portion of guitar chord before note */

/* -- macros for program internals -- */

#define CM		28.35	/* factor to transform cm to pt */
#define PT		1.00	/* factor to transform pt to pt */
#define IN		72.00	/* factor to transform inch to pt */

#define STRL1		101	/* string length for file names */
#define MAXSTAFF	16	/* max staves */
#define BSIZE		512	/* buffer size for one input string */
#define BUFFSZ		40000	/* size of output buffer */
#define BUFFSZ1		3000	/* buffer reserved for one staff */

#define BREVE		(BASE_LEN * 2)	/* double note (square note) */
#define SEMIBREVE	BASE_LEN	/* whole note */
#define MINIM		(BASE_LEN / 2)	/* half note (white note) */
#define CROTCHET 	(BASE_LEN / 4)	/* quarter note (black note) */
#define QUAVER		(BASE_LEN / 8)	/* 1/8 note */
#define SEMIQUAVER	(BASE_LEN / 16)	/* 1/16 note */

#define H_FULL		1	/* types of heads */
#define H_EMPTY 	2
#define H_OVAL		3

#define G_FILL		1	/* modes for glue */
#define G_SHRINK	2
#define G_SPACE 	3
#define G_STRETCH	4

#define SWFAC		0.50	/* factor to estimate width of string */

#define MAXFORMATS	10	/* max number of defined page formats */
#define STRLFMT		81	/* string length in FORMAT struct */
#define MAXFONTS	20	/* max number of fonts */

#define ALIGN		1
#define RAGGED		2
#define OBEYLINES	3
#define OBEYCENTER	4
#define SKIP		5

struct SYMBOL;
#define NCOMP	5		/* max number of composer lines */
#define NTITLE	3		/* max number of title lines */

struct ISTRUCT {		/* information fields */
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

extern unsigned char deco_glob[128], deco_tune[128];

struct SYMBOL { 		/* struct for a drawable symbol */
	struct abcsym as;	/* abc symbol !!must be the first field!! */
	struct SYMBOL *next, *prev;	/* linkage in voice */
	unsigned char type;	/* type of symbol */
#define NO_TYPE		0	/* invalid type */
#define INVISIBLE	1	/* valid symbol types */
#define NOTE		2
#define REST		3
#define BAR		4
#define CLEF		5
#define TIMESIG 	6
#define KEYSIG		7
#define TEMPO		8
#define STAVES		9
	unsigned char seq;	/* symbol sequence */
#define SQ_ANY 0x00
#define SQ_CLEF 0x10
#define SQ_SIG 0x20
#define SQ_BAR 0x30
#define SQ_EXTRA 0x40
#define SQ_NOTE 0x70
	short	len;		/* basic note length */
	short pits[MAXHD];	/* pitches for notes */
	struct SYMBOL *ts_next, *ts_prev; /* linkage in time */
	int	time;		/* starting time */
	unsigned eoln:1;	/* end of line */
	unsigned word_st:1;	/* word starts here */
	unsigned beam_break:1;	/* 2nd beam must restart here */
	unsigned no_head:1;	/* don't draw note head */
	unsigned char nhd;	/* number of notes in chord - 1 */
	signed char stem;	/* 0,1,-1 for no stem, up, down */
	char	flags;		/* number of flags or bars */
	char	dots;		/* number of dots */
	char	head;		/* type of head */
	char	voice;		/* voice (0..nvoice) */
	char	staff;		/* staff (0..nstaff) */
	signed char multi;	/* multi voice in the staff (+1, 0, -1) */
	short	u, v;		/* auxillary information */
	short	doty;		/* dot y pos when voices overlap */
	float	x;		/* position */
	short	y;
	short	ymn, ymx, yav;	/* min,mav,avg note head height */
	float	xmx;		/* max h-pos of a head rel to top */
	float	dc_top;		/* max height needed for decorations */
	float	xs, ys;		/* position of stem end */
	float	wl, wr;		/* left,right min width */
	float	pl, pr;		/* left,right preferred width */
	float	xl, xr;		/* left,right expanded width */
	float	shrink, stretch; /* glue before this symbol */
	float	gchy;		/* height of guitar chord */
	float	shhd[MAXHD];	/* horizontal shift for heads */
	float	shac[MAXHD];	/* horizontal shift for accidentals */
};

struct FONTSPEC {
	char	name[STRLFMT];
	float	size;
	float	swfac;
};

struct FORMAT { 		/* struct for page layout */
	char	name[STRLFMT];
	float	pageheight,staffwidth;
	float	topmargin,botmargin,leftmargin;
	float	topspace,wordsspace,titlespace,subtitlespace,partsspace;
	float	composerspace,musicspace,staffsep,vocalspace,textspace;
	float	scale,maxshrink,lineskipfac,parskipfac;
	float	indent;
	int	landscape,titleleft,continueall,writehistory;
	int	stretchstaff,stretchlast,withxrefs,barsperstaff;
	int	oneperpage,musiconly,titlecaps;
	int	encoding, partsbox;
	struct FONTSPEC titlefont,subtitlefont,vocalfont,textfont,tempofont;
	struct FONTSPEC composerfont,partsfont,gchordfont,wordsfont;
	int	measurenb, measurefirst, measurebox;
};

extern struct FORMAT cfmt;	/* current format for output */

extern char fontnames[MAXFONTS][STRLFMT];	/* list of needed fonts */
extern int  nfontnames;

extern int  ntxt;
extern char *style;

extern char *mbf;		/* where to PUTx() */
extern int nbuf;		/* number of bytes buffered */
extern float bposy;		/* current position in buffered data */
extern int use_buffer;		/* 1 if lines are being accumulated */

extern char page_init[201];	/* initialization string after page break */
extern int linenum;		/* current line number in input file */
extern int tunenum;		/* number of current tune */
extern int pagenum;		/* current page in output file */

extern float posy;	 	/* vertical position on page */

#ifdef DEBUG
extern int verbose; 		/* verbosity, global and within tune */
#endif
extern int in_page;

				 /* switches modified by flags: */
extern int gmode;			/* switch for glue treatment */
extern int pagenumbers; 		/* write page numbers ? */
extern int epsf;			/* for EPSF postscript output */
extern int choose_outname;		/* 1 names outfile w. title/fnam */
extern int break_continues;		/* ignore continuations ? */

extern char outf[STRL1];		/* output file name */
extern char *in_fname;			/* current input file name */

extern int  file_initialized;		/* for output file */
extern FILE *fout;			/* output file */
extern int nepsf;			/* counter for epsf output files */

struct STAFF {
	char	clef;		/* basic clef */
	char	nvocal;		/* number of vocals (1..n) */
	unsigned brace:1;	/* 1st staff of a brace */
	unsigned brace_end:1;	/* 2nd staff of a brace */
	unsigned bracket:1;	/* 1st staff of a bracket */
	unsigned bracket_end:1;	/* last staff of a bracket */
	unsigned forced_clef:1;	/* explicit clef */
	unsigned stop_bar:1;	/* stop drawing bar on this staff */
	float	toppos;		/* place needed at top */
	float	botpos;		/* place needed at bottom */
	float	y;		/* y position */
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
	float	nmw, snmw;	/* width */
	struct SYMBOL *p_key;	/* key definition */
	struct SYMBOL *p_meter;	/* meter definition */
	char	*bar_text;	/* bar text at start of staff when bar_start */
	char	bar_start;	/* bar type at start of staff / -1 */
	unsigned char anc;	/* ancillary variables */
	char	voice;		/* voice number */
	char	bagpipe;	/* switch for HP mode */
	char	staff;		/* staff (0..n-1) */
	char	clef;		/* current clef while parsing */
	unsigned forced_clef:1;	/* explicit clef */
	unsigned second:1;	/* secondary voice in a brace/parenthesis */
	unsigned floating:1;	/* floating voice in a brace */
};
extern struct VOICE_S voice_tb[MAXVOICE];	/* voice table */
extern int nvoice;		/* (0..MAXVOICE-1) */
extern int current_voice;	/* current voice while parsing */
extern struct VOICE_S *first_voice;	/* first_voice */

/* PUTn: add to buffer with n arguments */
#define PUT0(f) do {sprintf(mbf,f); a2b(); } while (0)
#define PUT1(f,a) do {sprintf(mbf,f,a); a2b(); } while (0)
#define PUT2(f,a,b) do {sprintf(mbf,f,a,b); a2b(); } while (0)
#define PUT3(f,a,b,c) do {sprintf(mbf,f,a,b,c); a2b(); } while (0)
#define PUT4(f,a,b,c,d) do {sprintf(mbf,f,a,b,c,d); a2b(); } while (0)
#define PUT5(f,a,b,c,d,e) do {sprintf(mbf,f,a,b,c,d,e); a2b(); } while (0)

/* -- external routines -- */
/* abc2ps.c */
char *getarena(int len);
/* buffer.c */
void a2b(void);
void buffer_eob(FILE *fp);
void bskip(float h);
void check_buffer(FILE *fp,
		  int nb);
void clear_buffer(void);
void init_pdims(void);
void write_buffer(FILE *fp);
void close_ps(FILE *fp);
void close_epsf(FILE *fp);
void close_page(FILE *fp);
void init_epsf(FILE *fp);
void init_page(FILE *fp);
void init_ps(FILE *fp,
	     char str[],
	     int  is_epsf,
	     float bx1,
	     float by1,
	     float bx2,
	     float by2);
void write_pagebreak(FILE *fp);
/* deco.c */
void reset_deco(int deco_old);
float draw_decorations(struct SYMBOL *s,
		       float *tp);
/* format.c */
int interpret_format_line(char *l);
void make_font_list(void);
void print_format(void);
int read_fmt_file(char filename[],
		  char dirname[]);
void set_pretty_format(void);
void set_pretty2_format(void);
void set_standard_format(void);
/* music.c */
void output_music(void);
void reset_gen(void);
/* parse.c */
struct SYMBOL *add_sym(struct VOICE_S *p_voice,
		       int type);
void voice_dup(void);
void do_tune(struct abctune *t,
	     int header_only);
void identify_note(int len,
		  int *p_head,
		  int *p_dots,
		  int *p_flags);
struct SYMBOL *ins_sym(int type,
		       struct SYMBOL *s);
/* subs.c */
#define TEXT_H	0	/* type of a text line */
#define TEXT_W	1
#define TEXT_Z	2
#define TEXT_N	3
#define TEXT_D	4
#define TEXT_MAX 5
void add_text(char *str,
	      int type);
void add_to_text_block(char ln[],
		       int add_final_nl);
void check_margin(float new_posx);
void clear_text(void);
void close_output_file(void);
float cwid(char c);
void epsf_title(char title[],
		char fnm[]);
int is_xrefstr(char str[]);
int make_arglist(char str[],
		 char *av[]);
void open_output_file(char fnam[]);
void ops_into_fmt(struct FORMAT *fmt);
void put_history(FILE *fp);
void put_words(FILE *fp);
void set_font(struct FONTSPEC *font);
int tex_str(char *d,
	    char *s,
	    int maxlen,
	    float *wid);
void put_str3(char *head,
	      char *str,
	      char *tail);
void write_inside_title(void);
void write_heading(void);
void write_parts(void);
void write_text_block(FILE *fp,
		      int  job,
		      int abc_state);
/* syms.c */
void define_encoding(FILE *fp,
		     int enc);
void define_font(FILE *fp,
		 char name[],
		 int  num,
		 int enc);
void define_symbols(FILE *fp);
/* util.c */
extern char newline[];
int abbrev(char str[],
	   char ab[],
	   int nchar);
void bug(char msg[],
	 int fatal);
#ifndef DEBUG
void error_head(void);
#define ERROR(x) do { error_head(); printf x ; printf(newline); } while (0)
#else
#define ERROR(x) do { if (verbose <= 3) printf(newline); printf x; } while (0)
#endif
void cap_str(char *c);
int get_file_size(char fname[]);
float ranf(float x1,
	   float x2);
int match(char str[],
	  char pat[]);
float scan_u(char str[]);
void strext(char *fid1,
	    char *ext);
