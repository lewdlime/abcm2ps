/*++
 * Declarations for abcparse.c.
 *
 *-*/

#define MAXVOICE 32	/* max number of voices */

#define MAXHD	8	/* max heads in a chord */
#define MAXDC	45	/* max decorations per note/chord/bar */
#define MAXMICRO 32	/* max microtone values (5 bits in accs[]) */

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

/* slur/tie types (3 bits) */
#define SL_ABOVE 0x01
#define SL_BELOW 0x02
#define SL_AUTO 0x03
#define SL_DOTTED 0x04		/* (modifier bit) */

/* note structure */
struct deco {		/* decorations */
	char n;			/* whole number of decorations */
	char h;			/* start of head decorations */
	char s;			/* start of decorations from s: (d:) */
	unsigned char t[MAXDC];	/* decoration type */
};

struct note {		/* note or rest */
	signed char pits[MAXHD]; /* pitches */
	short lens[MAXHD];	/* note lengths (# pts in [1] if space) */
	unsigned char accs[MAXHD]; /* code for accidentals & index in micro_tb */
	unsigned char sl1[MAXHD]; /* slur start per head */
	char sl2[MAXHD];	/* number of slur end per head */
	char ti1[MAXHD];	/* flag to start tie here */
	unsigned char decs[MAXHD]; /* head decorations (index: 5 bits, len: 3 bits) */
	short chlen;		/* chord length */
	char nhd;		/* number of notes in chord - 1 */
	unsigned char slur_st;	/* slurs starting here (2 bits array) */
	char slur_end;		/* number of slurs ending here */
	signed char brhythm;	/* broken rhythm */
	unsigned char microscale; /* microtone denominator - 1 */
	struct deco dc;		/* decorations */
};

/* symbol definition */
struct abctune;
struct abcsym {
	struct abctune *tune;	/* tune */
	struct abcsym *next, *prev; /* next / previous symbol */
	char type;		/* symbol type */
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
	unsigned short colnum;	/* ABC source column number */
	int linenum;		/* ABC source line number */
	char *fn;		/* ABC source file name */
	char *text;		/* main text (INFO, PSCOM),
				 * guitar chord (NOTE, REST, BAR) */
	char *comment;		/* comment part (when keep_comment) */
	union {			/* type dependent part */
		struct key_s {		/* K: info */
			signed char sf;		/* sharp (> 0) flats (< 0) */
			char empty;		/* clef alone if 1, 'none' if 2 */
			char exp;		/* exp (1) or mod (0) */
			char mode;		/* mode */
/* 0: Ionian, 1: Dorian, 2: Phrygian, 3: Lydian, 4: Mixolydian
 * 5: Aeolian, 6: Locrian, 7: major, 8:minor, 9: HP, 10: Hp */
#define MAJOR 7
#define MINOR 8
#define BAGPIPE 9				/* bagpipe when >= 8 */
			signed char nacc;	/* number  of explicit accidentals */
						/* (-1) if no accidental */
			signed char octave;	/* 'octave=' */
#define NO_OCTAVE 10				/* no 'octave=' */
			unsigned char microscale; /* microtone denominator - 1 */
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
#define MAX_MEASURE 6
			struct {
				char top[8];	/* top value */
				char bot[2];	/* bottom value */
			} meter[MAX_MEASURE];
		} meter;
		struct {		/* Q: info */
			char *str1;		/* string before */
			short length[4];	/* up to 4 note lengths */
			char *value;		/* tempo value */
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
		} voice;
		struct {		/* bar, mrest or mrep */
			int type;
			char repeat_bar;
			char len;		/* len if mrest or mrep */
			char dotted;
			struct deco dc;		/* decorations */
		} bar;
		struct clef_s {		/* clef (and staff!) */
			char *name;		/* PS drawing function */
			float staffscale;	/* != 0 when change */
			signed char stafflines;	/* >= 0 when change */
			signed char type;	/* no clef if < 0 */
#define TREBLE 0
#define ALTO 1
#define BASS 2
#define PERC 3
			char line;
			signed char octave;
			signed char transpose;
			char invis;
			char check_pitch;	/* check if old abc2ps transposition */
		} clef;
		struct note note;	/* note, rest */
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

/* tune definition */
struct abctune {
	struct abctune *next;	/* next tune */
	struct abcsym *first_sym; /* first symbol */
	struct abcsym *last_sym; /* last symbol */
	int abc_vers;		/* ABC version = (H << 16) + (M << 8) + L */
	void *client_data;	/* client data */
	unsigned short micro_tb[MAXMICRO]; /* microtone values [ (n-1) | (d-1) ] */
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
		       char *p,
		       char *comment);
struct abctune *abc_parse(char *file_api);
char *get_str(char *d,
	      char *s,
	      int maxlen);
char *parse_deco(char *p,
		 struct deco *deco);
#if defined(__cplusplus)
}
#endif
