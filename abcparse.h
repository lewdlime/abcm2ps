/*++
 * Declarations for abcparse.c.
 *
 *-*/

#define MAXVOICE 32	/* max number of voices */

#define MAXHD	8	/* max heads on one stem */
#define MAXDC	7	/* max decorations */

#define BASE_LEN 1536	/* basic note length (semibreve or whole note - same as MIDI) */

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

/* note structure */
struct deco {			/* describes decorations */
	char n;			/* number of decorations */
	unsigned char t[MAXDC];	/* decoration type */
};

struct note {		/* note or rest */
	signed char pits[MAXHD]; /* pitches for notes */
	short lens[MAXHD];	/* note lengths as multiple of BASE */
	unsigned char accs[MAXHD]; /* code for accidentals */
	char sl1[MAXHD];	/* which slur starts on this head */
	char sl2[MAXHD];	/* which slur ends on this head */
	char ti1[MAXHD];	/* flag to start tie here */
	char ti2[MAXHD];	/* flag to end tie here */
	short len;		/* note length (shortest in chords) */
	unsigned invis:1;	/* invisible rest */
	unsigned word_end:1;	/* 1 if word ends here */
	unsigned stemless:1;	/* note with no stem */
	unsigned lyric_start:1;	/* may start a lyric here */
	unsigned grace:1;	/* grace note */
	unsigned sappo:1;	/* short appoggiatura */
	char nhd;		/* number of notes in chord - 1 */
	char p_plet, q_plet, r_plet; /* data for n-plets */
	char slur_st; 		/* how many slurs start here */
	char slur_end;		/* how many slurs end here */
	signed char brhythm;	/* broken rhythm */
	struct deco dc;		/* decorations */
};

/* symbol definition */
struct abctune;
struct abcsym {
	struct abctune *tune;	/* tune */
	struct abcsym *next;	/* next symbol */
	struct abcsym *prev;	/* previous symbol */
	char type;		/* symbol type */
#define ABC_T_NULL 0
#define ABC_T_INFO 1		/* (text[0] gives the info type) */
#define ABC_T_PSCOM 2
#define ABC_T_CLEF 3
#define ABC_T_NOTE 4
#define ABC_T_REST 5
#define ABC_T_BAR 6
#define ABC_T_EOLN 7
#define ABC_T_INFO2 8		/* (info without header - H:) */
#define ABC_T_MREST 9		/* multi-measure rest */
#define ABC_T_MREP 10		/* measure repeat */
#define ABC_T_V_OVER 11		/* voice overlay */
	char state;		/* symbol state in file/tune */
#define ABC_S_GLOBAL 0			/* global */
#define ABC_S_HEAD 1			/* in header (after X:) */
#define ABC_S_TUNE 2			/* in tune (after K:) */
#define ABC_S_EMBED 3			/* embedded header (between [..]) */
	short linenum;		/* line number / ABC file */
	char *text;		/* main text (INFO, PSCOM),
				 * guitar chord (NOTE, REST, BAR) */
	char *comment;		/* comment part (when keep_comment) */
	union {			/* type dependent part */
		struct key_s {		/* K: info */
			signed char sf;		/* sharp (> 0) flats (< 0) */
			char bagpipe;		/* HP or Hp */
			char minor;		/* major (0) / minor (1) */
			char empty;		/* clef alone if 1 */
			char nacc;		/* explicit accidentals */
			char pits[8];
			char accs[8];
		} key;
		struct {		/* L: info */
			int base_length;	/* basic note length */
		} length;
		struct meter_s {	/* M: info */
			short wmeasure;		/* duration of a measure */
			short nmeter;		/* number of meter elements */
#define MAX_MEASURE 6
			struct {
				char top[8];	/* top value */
				char bot[2];	/* bottom value */
			} meter[MAX_MEASURE];
		} meter;
		struct {		/* Q: info */
			char *str1;		/* string before */
			short length[4];	/* up to 4 note lengths */
			short value;		/* tempo value */
			char *str2;		/* string after */
		} tempo;
		struct {		/* V: info */
			char *name;		/* name */
			char *fname;		/* full name */
			char *nname;		/* nick name */
			unsigned char voice;	/* voice number */
			char merge;		/* merge with previous voice */
			signed char stem;	/* have all stems up or down */
		} voice;
		struct {		/* bar, mrest or mrep */
			struct deco dc;		/* decorations */
			int type;
			char repeat_bar;
			char len;		/* len if mrest or mrep */
		} bar;
		struct clef_s {		/* clef */
			char type;
#define TREBLE 0
#define ALTO 1
#define BASS 2
#define PERC 3
			char line;
			signed char octave;
			signed char transpose;
			char invis;
#ifndef CLEF_TRANSPOSE
			char check_pitch;	/* check if old abc2ps transposition */
#endif
		} clef;
		struct note note;	/* note, rest */
		struct {		/* user defined accent */
			unsigned char symbol;
			unsigned char value;
		} user;
		struct staff_s {	/* %%staves */
			unsigned char voice;
			char flags;
#define OPEN_BRACE 0x01
#define CLOSE_BRACE 0x02
#define OPEN_BRACKET 0x04
#define CLOSE_BRACKET 0x08
#define OPEN_PARENTH 0x10
#define CLOSE_PARENTH 0x20
#define STOP_BAR 0x40
			char *name;
		} staves[MAXVOICE];
		struct {		/* voice overlay */
			char type;
#define V_OVER_S 0				/* single & */
#define V_OVER_D 1				/* && */
#define V_OVER_SS 2				/* (& */
#define V_OVER_SD 3				/* (&& */
#define V_OVER_E 4				/* )& */
			unsigned char voice;
		} v_over;
	} u;
};

/* tune definition */
struct abctune {
	struct abctune *next;	/* next tune */
	struct abctune *prev;	/* previous tune */
	struct abcsym *first_sym; /* first symbol */
	struct abcsym *last_sym; /* last symbol */
	int client_data;	/* client data */
};

#ifdef WIN32
#define strcasecmp stricmp
#define strncasecmp strnicmp
#endif

#if defined(__cplusplus)
extern "C" {
#endif
extern char *deco_tb[];
extern int severity;
void abc_delete(struct abcsym *as);
void abc_free(struct abctune *first_tune);
void abc_init(void *alloc_f_api(int size),
	      void free_f_api(void *ptr),
	      void level_f_api(int level),
	      int client_sz_api,
	      int keep_comment_api);
void abc_insert(char *file_api,
		struct abcsym *s);
struct abcsym *abc_new(struct abctune *t,
		       unsigned char *p,
		       unsigned char *comment);
struct abctune *abc_parse(char *file_api);
char *get_str(unsigned char *d,
	      unsigned char *s,
	      int maxlen);
void note_sort(struct abcsym *s);
unsigned char *parse_deco(unsigned char *p,
			  struct deco *deco);
#if defined(__cplusplus)
}
#endif
