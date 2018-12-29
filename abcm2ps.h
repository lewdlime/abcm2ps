/* -- general macros -- */

#include <stdio.h>
#include <time.h>

#include "config.h"

#define MAXVOICE 32	/* max number of voices */

#define MAXHD	8	/* max heads in a chord */
#define MAXDC	32	/* max decorations per symbol */
#define MAXMICRO 32	/* max microtone values (5 bits in accs[]) */
#define DC_NAME_SZ 128	/* size of the decoration name table */

#define BASE_LEN 1536	/* basic note length (semibreve or whole note - same as MIDI) */

#define VOICE_ID_SZ 16	/* max size of the voice identifiers */

/* accidentals */
enum accidentals {
	A_NULL,		/* none */
	A_SH,		/* sharp */
	A_NT,		/* natural */
	A_FT,		/* flat */
	A_DS,		/* double sharp */
	A_DF		/* double flat */
};

/* bar types - 4 bits per symbol */
#define B_BAR 1		/* | */
#define B_OBRA 2	/* [ */
#define B_CBRA 3	/* ] */
#define B_COL 4		/* : */

#define B_SINGLE 0x01		/* |	single bar */
#define B_DOUBLE 0x11		/* ||	thin double bar */
#define B_THIN_THICK 0x13	/* |]	thick at section end  */
#define B_THICK_THIN 0x21	/* [|	thick at section start */
#define B_LREP 0x14		/* |:	left repeat bar */
#define B_RREP 0x41		/* :|	right repeat bar */
#define B_DREP 0x44		/* ::	double repeat bar */
#define B_DASH 0x04		/* :	dashed bar */

/* slur/tie types (4 bits) */
#define SL_ABOVE 0x01
#define SL_BELOW 0x02
#define SL_HIDDEN 0x03		/* also opposite for gstemdir */
#define SL_AUTO 0x04
#define SL_DOTTED 0x08		/* (modifier bit) */

#define OUTPUTFILE "Out.ps"	/* standard output file */

#ifndef WIN32
#define DIRSEP '/'
#else
#define DIRSEP '\\'
#define strcasecmp stricmp
#define strncasecmp _strnicmp
#define strdup _strdup
#define snprintf _snprintf
#ifdef _MSC_VER
#define fileno _fileno
#endif
#endif

#define CM		* 37.8	/* factor to transform cm to pt */
#define PT			/* factor to transform pt to pt */
#define IN		* 96.0	/* factor to transform inch to pt */

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
#define STEM		21	/* default stem height = one octave */
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
#define BEAM_SHIFT	5.0	/* shift of second and third beams */
/*  To align the 4th beam as the 1st: shift=6-(depth-2*offset)/3  */
#define BEAM_FLATFAC	0.6	/* factor to decrease slope of long beams */
#define BEAM_THRESH	0.06	/* flat beam if slope below this threshold */
#define BEAM_SLOPE	0.5	/* max slope of a beam */
#define BEAM_STUB	7.0	/* length of stub for flag under beam */ 
#define SLUR_SLOPE	1.0	/* max slope of a slur */
#define GSTEM		15	/* grace note stem length */
#define GSTEM_XOFF	2.3	/* x offset for grace note stem */

#define BETA_C		0.1	/* max expansion for flag -c */
#define BETA_X		1.0	/* max expansion before complaining */

#define VOCPRE		0.4	/* portion of vocals word before note */
#define GCHPRE		0.4	/* portion of guitar chord before note */

/* -- Parameters for note spacing -- */
/* fnn multiplies the spacing under a beam, to compress the notes a bit */

#define fnnp 0.9

/* -- macros for program internals -- */

#define STRL1		256	/* string length for file names */
#define MAXSTAFF	32	/* max staves */
#define BSIZE		512	/* buffer size for one input string */

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

struct decos {		/* decorations */
	char n;			/* whole number of decorations */
	struct {
		unsigned char t;	/* decoration index */
		signed char m;		/* index in chord when note / -1 */
	} tm[MAXDC];
};

struct note_map {	/* note mapping */
	struct note_map *next;	/* note linkage */
	char type;		/* map type */
#define MAP_ONE 0
#define MAP_OCT 1
#define MAP_KEY 2
#define MAP_ALL 3
	signed char pit;	/* note pitch and accidental */
	unsigned char acc;
	char *heads;		/* comma separated list of note heads */
	int color;		/* color */
	signed char print_pit;	/* print pitch */
	unsigned char print_acc; /* print acc */
};
struct map {		/* voice mapping */
	struct map *next;	/* name linkage */
	char *name;
	struct note_map *notes;	/* mapping of the notes */
};
extern struct map *maps; /* note mappings */

struct note {		/* note head */
	int len;		/* note duration (# pts in [1] if space) */
	signed char pit;	/* absolute pitch from source - used for ties and map */
	unsigned char acc;	/* code for accidental & index in micro_tb */
	unsigned char sl1;	/* slur start */
	char sl2;		/* number of slur ends */
	char ti1;		/* flag to start tie here */
	char hlen;		/* length of the head string */
	char invisible;		/* alternate note head */
	float shhd;		/* horizontal head shift (#pts if space) */
	float shac;		/* horizontal accidental shift */
	char *head;		/* head */
	int color;		/* heads when note mapping */
};

struct notes {		/* note chord or rest */
	struct note notes[MAXHD]; /* note heads */
	unsigned char slur_st;	/* slurs starting here (2 bits array) */
	char slur_end;		/* number of slurs ending here */
	signed char brhythm;	/* broken rhythm */
	unsigned char microscale; /* microtone denominator - 1 */
	float sdx;		/* x offset of the stem */
	struct decos dc;	/* decorations */
};

extern int severity;

extern char *deco[256];

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
	char t[256];		/* word (dummy size) */
};
struct lyrics {
	struct lyl *lyl[MAXLY];	/* ptr to lyric lines */
};

/* guitar chord / annotations */
#define MAXGCH 8		/* max number of guitar chords / annotations */
struct gch {
	char type;		/* ann. char, 'g' gchord, 'r' repeat, '\0' end */
	unsigned char idx;	/* index in as.text */
	unsigned char font;	/* font */
	char box;		/* 1 if in box */
	float x, y;		/* x y offset / note + (top or bottom) of staff */
	float w;		/* width */
};

/* positions / directions - see SL_xxx */
struct posit_s {
	unsigned short dyn:4;	/* %%dynamic */
	unsigned short gch:4;	/* %%gchord */
	unsigned short orn:4;	/* %%ornament */
	unsigned short voc:4;	/* %%vocal */
	unsigned short vol:4;	/* %%volume */
	unsigned short std:4;	/* %%stemdir */
	unsigned short gsd:4;	/* %%gstemdir */
};

/* music element */
struct SYMBOL { 		/* struct for a drawable symbol */
	struct SYMBOL *abc_next, *abc_prev; /* source linkage */
	struct SYMBOL *next, *prev;	/* voice linkage */
	struct SYMBOL *ts_next, *ts_prev; /* time linkage */
	struct SYMBOL *extra;	/* extra symbols (grace notes, tempo... */
	char abc_type;		/* ABC symbol type */
#define ABC_T_NULL	0
#define ABC_T_INFO 	1		/* (text[0] gives the info type) */
#define ABC_T_PSCOM	2
#define ABC_T_CLEF	3
#define ABC_T_NOTE	4
#define ABC_T_REST	5
#define ABC_T_BAR	6
#define ABC_T_EOLN	7
#define ABC_T_MREST	8		/* multi-measure rest */
#define ABC_T_MREP	9		/* measure repeat */
#define ABC_T_V_OVER	10		/* voice overlay */
#define ABC_T_TUPLET	11
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
#define CUSTOS		15
#define NSYMTYPES	16
	unsigned char voice;	/* voice (0..nvoice) */
	unsigned char staff;	/* staff (0..nstaff) */
	unsigned char nhd;	/* number of notes in chord - 1 */
	signed char pits[MAXHD]; /* pitches / clef */
	int dur;		/* main note duration */
	int time;		/* starting time */
	unsigned int sflags;	/* symbol flags */
#define S_EOLN		0x0001		/* end of line */
#define S_BEAM_ST	0x0002		/* beam starts here */
#define S_BEAM_BR1	0x0004		/* 2nd beam must restart here */
#define S_BEAM_BR2	0x0008		/* 3rd beam must restart here */
#define S_BEAM_END	0x0010		/* beam ends here */
#define S_CLEF_AUTO	0x0020		/* auto clef (when clef) */
#define S_IN_TUPLET	0x0040		/* in a tuplet */
#define S_TREM2		0x0080		/* tremolo on 2 notes */
#define S_RRBAR		0x0100		/* right repeat bar (when bar) */
#define S_XSTEM		0x0200		/* cross-staff stem (when note) */
#define S_BEAM_ON	0x0400		/* continue beaming */
#define S_SL1		0x0800		/* some chord slur start */
#define S_SL2		0x1000		/* some chord slur end */
#define S_TI1		0x2000		/* some chord tie start */
#define S_PERC		0x4000		/* percussion */
#define S_RBSTOP	0x8000		// end of repeat bracket
#define S_FEATHERED_BEAM 0x00010000	/* feathered beam */
#define S_REPEAT	0x00020000	/* sequence / measure repeat */
#define S_NL		0x00040000	/* start of new music line */
#define S_SEQST		0x00080000	/* start of vertical sequence */
#define S_SECOND	0x00100000	/* symbol on a secondary voice */
#define S_FLOATING	0x00200000	/* symbol on a floating voice */
#define S_NOREPBRA	0x00400000	/* don't print the repeat bracket */
#define S_TREM1		0x00800000	/* tremolo on 1 note */
#define S_TEMP		0x01000000	/* temporary symbol */
#define S_SHIFTUNISON_1	0x02000000	/* %%shiftunison 1 */
#define S_SHIFTUNISON_2	0x04000000	/* %%shiftunison 2 */
#define S_NEW_SY	0x08000000	/* staff system change (%%staves) */
#define S_RBSTART	0x10000000	// start of repeat bracket
#define S_OTTAVA	0x20000000	// ottava decoration (start or stop)
	struct posit_s posit;	/* positions / directions */
	signed char stem;	/* 1 / -1 for stem up / down */
	signed char combine;	/* voice combine */
	signed char nflags;	/* number of note flags when > 0 */
	char dots;		/* number of dots */
	unsigned char head;	/* head type */
#define H_FULL		0
#define H_EMPTY 	1
#define H_OVAL		2
#define H_SQUARE	3
	signed char multi;	/* multi voice in the staff (+1, 0, -1) */
	signed char nohdi1;	/* no head index (for unison) / nb of repeat */
	signed char nohdi2;
	signed char doty;	/* NOTEREST: y pos of dot when voices overlap
				 * STBRK: forced
				 * FMTCHG REPEAT: infos */
	short aux;		/* auxillary information:
				 *	- CLEF: small clef
				 *	- KEYSIG: old key signature
				 *	- BAR: new bar number
				 *	- TUPLET: tuplet format
				 *	- NOTE: tremolo number / feathered beam
				 *	- FMTCHG (format change): subtype */
#define PSSEQ 0				/* postscript sequence */
#define SVGSEQ 1			/* SVG sequence */
#define REPEAT 2			/* repeat sequence or measure
					 *	doty: # measures if > 0
					 *	      # notes/rests if < 0
					 *	nohdi1: # repeat */
	int color;
	float x;		/* x offset */
	signed char y;		/* y offset of note head */
	signed char ymn, ymx;	/* min, max, note head y offset */
	signed char mid;	// y offset of the staff middle line
	float xmx;		/* max h-pos of a head rel to top
				 * width when STBRK */
	float xs, ys;		/* coord of stem end / bar height */
	float wl, wr;		/* left, right min width */
	float space;		/* natural space before symbol */
	float shrink;		/* minimum space before symbol */
	float xmax;		/* max x offset */
	struct gch *gch;	/* guitar chords / annotations */
	struct lyrics *ly;	/* lyrics */

	char state;		/* symbol state in file/tune */
#define ABC_S_GLOBAL 0			/* global */
#define ABC_S_HEAD 1			/* in header (after X:) */
#define ABC_S_TUNE 2			/* in tune (after K:) */
	unsigned short flags;
#define ABC_F_ERROR	0x0001		/* error around this symbol */
#define ABC_F_INVIS	0x0002		/* invisible symbol */
#define ABC_F_SPACE	0x0004		/* space before a note */
#define ABC_F_STEMLESS	0x0008		/* note with no stem */
#define ABC_F_LYRIC_START 0x0010	/* may start a lyric here */
#define ABC_F_GRACE	0x0020		/* grace note */
#define ABC_F_GR_END	0x0040		/* end of grace note sequence */
#define ABC_F_SAPPO	0x0080		/* short appoggiatura */
#define ABC_F_RBSTART	0x0100		// start of repeat bracket and mark
#define ABC_F_RBSTOP	0x0200		// end of repeat bracket and mark
	unsigned short colnum;	/* ABC source column number */
	int linenum;		/* ABC source line number */
	char *fn;		/* ABC source file name */
	char *text;		/* main text (INFO, PSCOM),
				 * guitar chord (NOTE, REST, BAR) */
	union {			/* type dependent part */
		struct key_s {		/* K: info */
			signed char sf;		/* sharp (> 0) flats (< 0) */
			char empty;		/* clef alone if 1, 'none' if 2 */
			char exp;		/* exp (1) or mod (0) */
//			char mode;		/* mode */
//					/* 0: Ionian, 1: Dorian, 2: Phrygian, 3: Lydian,
//					 * 4: Mixolydian, 5: Aeolian, 6: Locrian */
			char instr;		/* specific instrument */
#define K_HP 1				/* bagpipe */
#define K_Hp 2
#define K_DRUM 3			/* percussion */
			signed char nacc;	/* number of explicit accidentals */
			signed char cue;	/* cue voice (scale 0.7) */
			signed char octave;	/* 'octave=' */
#define NO_OCTAVE 10				/* no 'octave=' */
			unsigned char microscale; /* microtone denominator - 1 */
			char clef_delta;	/* clef delta */
			char key_delta;		// tonic base
			char *stafflines;
			float staffscale;
			signed char pits[8];
			unsigned char accs[8];
		} key;
		struct {		/* L: info */
			int base_length;	/* basic note length */
		} length;
		struct meter_s {	/* M: info */
			short wmeasure;		/* duration of a measure */
			unsigned char nmeter;	/* number of meter elements */
			char expdur;		/* explicit measure duration */
#define MAX_MEASURE 16
			struct {
				char top[8];	/* top value */
				char bot[2];	/* bottom value */
			} meter[MAX_MEASURE];
		} meter;
		struct {		/* Q: info */
			char *str1;		/* string before */
			short beats[4];		/* up to 4 beats */
			short circa;		/* "ca. " */
			short tempo;		/* number of beats per mn or */
			short new_beat;		/* old beat */
			char *str2;		/* string after */
		} tempo;
		struct {		/* V: info */
			char id[VOICE_ID_SZ];	/* voice ID */
			char *fname;		/* full name */
			char *nname;		/* nick name */
			float scale;		/* != 0 when change */
			unsigned char voice;	/* voice number */
			signed char octave;	/* 'octave=' - same as in K: */
			char merge;		/* merge with previous voice */
			signed char stem;	/* have stems up or down (2 = auto) */
			signed char gstem;	/* have grace stems up or down (2 = auto) */
			signed char dyn;	/* have dynamic marks above or below the staff */
			signed char lyrics;	/* have lyrics above or below the staff */
			signed char gchord;	/* have gchord above or below the staff */
			signed char cue;	/* cue voice (scale 0.7) */
			char *stafflines;
			float staffscale;
		} voice;
		struct {		/* bar, mrest or mrep */
			int type;
			char repeat_bar;
			char len;		/* len if mrest or mrep */
			char dotted;
			struct decos dc;	/* decorations */
		} bar;
		struct clef_s {		/* clef */
			char *name;		/* PS drawing function */
			signed char type;
#define TREBLE 0
#define ALTO 1
#define BASS 2
#define PERC 3
#define AUTOCLEF 4
			char line;
			signed char octave;	/* '+8' / '-8' */
			signed char transpose;	/* if '^8' / '_8' */
			char invis;		/* clef 'none' */
			char check_pitch;	/* check if old abc2ps transposition */
		} clef;
		struct notes note;	/* note, rest */
		struct {		/* user defined accent */
			unsigned char symbol;
			unsigned char value;
		} user;
		struct {
			char type;	/* 0: end of line
					 * 1: continuation ('\')
					 * 2: line break ('!') */
		} eoln;
		struct {		/* voice overlay */
			char type;
#define V_OVER_V 0				/* & */
#define V_OVER_S 1				/* (& */
#define V_OVER_E 2				/* &) */
			unsigned char voice;
		} v_over;
		struct {		/* tuplet */
			char p_plet, q_plet, r_plet;
		} tuplet;
	} u;
};

/* parse definition */
struct parse {
	struct SYMBOL *first_sym; /* first symbol */
	struct SYMBOL *last_sym; /* last symbol */
	int abc_vers;		/* ABC version = (H << 16) + (M << 8) + L */
	char *deco_tb[DC_NAME_SZ]; /* decoration names */
	unsigned short micro_tb[MAXMICRO]; /* microtone values [ (n-1) | (d-1) ] */
	int abc_state;		/* parser state */
};
extern struct parse parse;

#define	FONT_UMAX 10		/* max number of user fonts 0..9 */
enum e_fonts {
	ANNOTATIONFONT = FONT_UMAX,
	COMPOSERFONT,
	FOOTERFONT,
	GCHORDFONT,
	HEADERFONT,
	HISTORYFONT,
	INFOFONT,
	MEASUREFONT,
	PARTSFONT,
	REPEATFONT,
	SUBTITLEFONT,
	TEMPOFONT,
	TEXTFONT,
	TITLEFONT,
	VOCALFONT,
	VOICEFONT,
	WORDSFONT,
	FONT_DYN		/* index of dynamic fonts (gch, an, ly) */
};
#define	FONT_DYNX 12				/* number of dynamic fonts */
#define	FONT_MAX (FONT_DYN + FONT_DYNX)		/* whole number of fonts */

struct FORMAT { 		/* struct for page layout */
	float pageheight, pagewidth;
	float topmargin, botmargin, leftmargin, rightmargin;
	float topspace, wordsspace, titlespace, subtitlespace, partsspace;
	float composerspace, musicspace, vocalspace, textspace;
	float breaklimit, maxshrink, lineskipfac, parskipfac, stemheight;
	float gutter, indent, infospace, slurheight, notespacingfactor, scale;
	float staffsep, sysstaffsep, maxstaffsep, maxsysstaffsep, stretchlast;
	int abc2pscompat, alignbars, aligncomposer, autoclef;
	int barsperstaff, breakoneoln, bstemdown, cancelkey, capo;
	int combinevoices, contbarnb, continueall, custos;
	int dblrepbar, decoerr, dynalign, flatbeams, infoline;
	int gchordbox, graceslurs, graceword,gracespace, hyphencont;
	int keywarn, landscape, linewarn;
	int measurebox, measurefirst, measurenb, micronewps;
	int oneperpage;
#ifdef HAVE_PANGO
	int pango;
#endif
	int partsbox, pdfmark;
	int rbdbstop, rbmax, rbmin;
	int setdefl, shiftunison, splittune, squarebreve;
	int staffnonote, straightflags, stretchstaff;
	int textoption, titlecaps, titleleft, titletrim;
	int timewarn, transpose, tuplets;
	char *bgcolor, *dateformat, *header, *footer, *titleformat;
	char *musicfont;
	struct FONTSPEC font_tb[FONT_MAX];
	char ndfont;		/* current index of dynamic fonts */
	unsigned char gcf, anf, vof;	/* fonts for guitar chords,
					 * annotations and lyrics */
	unsigned int fields[2];	/* info fields to print
				 *[0] is 'A'..'Z', [1] is 'a'..'z' */
	struct posit_s posit;
};

extern struct FORMAT cfmt;	/* current format */
extern struct FORMAT dfmt;	/* global format */

typedef struct SYMBOL *INFO[26]; /* information fields ('A' .. 'Z') */
extern INFO info;

extern char *outbuf;		/* output buffer.. should hold one tune */
extern int outbufsz;		/* size of outbuf */
extern char *mbf;		/* where to PUTx() */
extern int use_buffer;		/* 1 if lines are being accumulated */

extern int outft;		/* last font in the output file */
extern int tunenum;		/* number of current tune */
extern int pagenum;		/* current page number */
extern int pagenum_nr;		/* current page (non-resettable) */
extern int nbar;		/* current measure number */
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
extern int epsf;		/* 1: EPSF, 2: SVG, 3: embedded ABC */
extern int svg;			/* 1: SVG, 2: XHTML */
extern int showerror;		/* show the errors */
extern int pipeformat;		/* format for bagpipes */

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
	struct SYMBOL *s_clef;	/* clef at start of music line */
	char empty;		/* no symbol on this staff */
	char *stafflines;
	float staffscale;
	short botbar, topbar;	/* bottom and top of bar */
	float y;		/* y position */
	float top[YSTEP], bot[YSTEP];	/* top/bottom y offsets */
};
extern struct STAFF_S staff_tb[MAXSTAFF];
extern int nstaff;		/* (0..MAXSTAFF-1) */

struct VOICE_S {
	char id[VOICE_ID_SZ];	/* voice id */
							/* generation */
	struct VOICE_S *next;	/* link */
	struct SYMBOL *sym;	/* associated symbols */
	struct SYMBOL *last_sym; /* last symbol while scanning */
	struct SYMBOL *lyric_start;	/* start of lyrics while scanning */
	char *nm;		/* voice name */
	char *snm;		/* voice subname */
	char *bar_text;		/* bar text at start of staff when bar_start */
	struct gch *bar_gch;	/* bar text */
	struct SYMBOL *tie;	/* note with ties of previous line */
	struct SYMBOL *rtie;	/* note with ties before 1st repeat bar */
	struct tblt_s *tblts[2]; /* tablatures */
	float scale;		/* scale */
	int time;		/* current time (parsing) */
	struct SYMBOL *s_clef;	/* clef at end of music line */
	struct key_s key;	/* current key signature */
	struct meter_s meter;	/* current time signature */
	struct key_s ckey;	/* key signature while parsing */
	struct key_s okey;	/* original key signature (parsing) */
	unsigned hy_st;		/* lyrics hyphens at start of line (bit array) */
	unsigned ignore:1;	/* ignore this voice (%%staves) */
	unsigned second:1;	/* secondary voice in a brace/parenthesis */
	unsigned floating:1;	/* floating voice in a brace system */
	unsigned bar_repeat:1;	/* bar at start of staff is a repeat bar */
	unsigned norepbra:1;	/* don't display the repeat brackets */
	unsigned have_ly:1;	/* some lyrics in this voice */
	unsigned new_name:1;	/* redisplay the voice name */
	unsigned space:1;	/* have a space before the next note (parsing) */
	unsigned perc:1;	/* percussion */
	unsigned auto_len:1;	/* auto L: (parsing) */
	short wmeasure;		/* measure duration (parsing) */
	short transpose;	/* transposition (parsing) */
	short bar_start;	/* bar type at start of staff / 0 */
	struct posit_s posit;	/* positions / directions */
	signed char octave;	/* octave (parsing) */
	signed char ottava;	/* !8va(! ... (parsing) */
	signed char clone;	/* duplicate from this voice number */
	signed char over;	/* overlay of this voice number */
	unsigned char staff;	/* staff (0..n-1) */
	unsigned char cstaff;	/* staff (parsing) */
	unsigned char slur_st;	/* slurs at start of staff */
	signed char combine;	/* voice combine */
	int color;
	char *stafflines;
	float staffscale;
							/* parsing */
	struct SYMBOL *last_note;	/* last note or rest */
	char *map_name;
	int ulen;			/* unit note length */
	unsigned char microscale;	/* microtone scale */
	unsigned char mvoice;		/* main voice when voice overlay */
};
extern struct VOICE_S *curvoice;	/* current voice while parsing */
extern struct VOICE_S voice_tb[MAXVOICE]; /* voice table */
extern struct VOICE_S *first_voice; /* first_voice */

extern struct SYMBOL *tsfirst;	/* first symbol in the time linked list */
extern struct SYMBOL *tsnext;	/* next line when cut */
extern float realwidth;		/* real staff width while generating */

#define NFLAGS_SZ 10		/* size of note flags tables */
#define C_XFLAGS 5		/* index of crotchet in flags tables */
extern float space_tb[NFLAGS_SZ]; /* note spacing */
extern float hw_tb[];		// width of note heads

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
#define MASTER_VOICE 0x1000
		char empty;
		char *stafflines;
		float staffscale;
//		struct clef_s clef;
		float sep, maxsep;
	} staff[MAXSTAFF];
	struct {
		signed char range;
		unsigned char staff;
		char second;
		char dum;
		float sep, maxsep;
//		struct clef_s clef;
	} voice[MAXVOICE];
};
extern struct SYSTEM *cursys;		/* current staff system */

/* -- external routines -- */
/* abcm2ps.c */
void include_file(unsigned char *fn);
void clrarena(int level);
int lvlarena(int level);
void *getarena(int len);
void strext(char *fid, char *ext);
/* abcparse.c */
void abc_parse(char *p, char *fname, int linenum);
void abc_eof(void);
char *get_str(char *d,
	      char *s,
	      int maxlen);
char *parse_acc_pit(char *p,
		int *pit,
		int *acc);
/* buffer.c */
void a2b(char *fmt, ...)
#ifdef __GNUC__
	__attribute__ ((format (printf, 1, 2)))
#endif
	;
void block_put(void);
void buffer_eob(int eot);
void marg_init(void);
void bskip(float h);
void check_buffer(void);
void init_outbuf(int kbsz);
void close_output_file(void);
void close_page(void);
float get_bposy(void);
void open_fout(void);
void write_buffer(void);
extern int (*output)(FILE *out, const char *fmt, ...)
#ifdef __GNUC__
	__attribute__ ((format (printf, 2, 3)))
#endif
	;
void write_eps(void);
/* deco.c */
void deco_add(char *text);
void deco_cnv(struct decos *dc, struct SYMBOL *s, struct SYMBOL *prev);
void deco_update(struct SYMBOL *s, float dx);
float deco_width(struct SYMBOL *s);
void draw_all_deco(void);
//int draw_deco_head(int deco, float x, float y, int stem);
void draw_deco_near(void);
void draw_deco_note(void);
void draw_deco_staff(void);
float draw_partempo(int staff, float top);
void draw_measnb(void);
void init_deco(void);
void reset_deco(void);
void set_defl(int new_defl);
float tempo_width(struct SYMBOL *s);
void write_tempo(struct SYMBOL *s,
		int beat,
		float sc);
float y_get(int staff,
		int up,
		float x,
		float w);
void y_set(int staff,
		int up,
		float x,
		float w,
		float y);
/* draw.c */
void draw_sym_near(void);
void draw_all_symb(void);
float draw_systems(float indent);
void output_ps(struct SYMBOL *s, int color);
struct SYMBOL *prev_scut(struct SYMBOL *s);
void putf(float f);
void putx(float x);
void puty(float y);
void putxy(float x, float y);
void set_scale(struct SYMBOL *s);
void set_sscale(int staff);
void set_color(int color);
/* format.c */
void define_fonts(void);
int get_textopt(char *p);
int get_font_encoding(int ft);
int get_bool(char *p);
void interpret_fmt_line(char *w, char *p, int lock);
void lock_fmt(void *fmt);
void make_font_list(void);
FILE *open_file(char *fn,
		char *ext,
		char *rfn);
void print_format(void);
void set_font(int ft);
void set_format(void);
void set_voice_param(struct VOICE_S *p_voice, int state, char *w, char *p);
struct tblt_s *tblt_parse(char *p);
/* front.c */
#define FE_ABC 0
#define FE_FMT 1
#define FE_PS 2
void frontend(unsigned char *s,
		int ftype,
		char *fname,
		int linenum);
/* glyph.c */
char *glyph_out(char *p);
void glyph_add(char *p);
/* music.c */
void output_music(void);
void reset_gen(void);
void unlksym(struct SYMBOL *s);
/* parse.c */
extern float multicol_start;
void do_tune(void);
void identify_note(struct SYMBOL *s,
		int len,
		int *p_head,
		int *p_dots,
		int *p_flags);
void sort_pitch(struct SYMBOL *s);
struct SYMBOL *sym_add(struct VOICE_S *p_voice,
			int type);
/* subs.c */
void bug(char *msg, int fatal);
void error(int sev, struct SYMBOL *s, char *fmt, ...);
float scan_u(char *str, int type);
float cwid(unsigned char c);
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
void put_str(char *str, int action);
float tex_str(char *s);
extern char tex_buf[];	/* result of tex_str() */
#define TEX_BUF_SZ 512
char *trim_title(char *p, struct SYMBOL *title);
void user_ps_add(char *s, char use);
void user_ps_write(void);
void write_title(struct SYMBOL *s);
void write_heading(void);
void write_user_ps(void);
void write_text(char *cmd, char *s, int job);
/* svg.c */
void define_svg_symbols(char *title, int num, float w, float h);
void svg_def_id(char *id, int idsz);
void svg_font_switch(void);
int svg_output(FILE *out, const char *fmt, ...)
#ifdef __GNUC__
	__attribute__ ((format (printf, 2, 3)))
#endif
	;
void svg_write(char *buf, int len);
void svg_close();
/* syms.c */
void define_font(char *name, int num, int enc);
void define_symbols(void);
