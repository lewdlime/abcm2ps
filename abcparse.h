/*++
 * Declarations for abcparse.c.
 *
 *-*/

#define MAXVOICE 32	/* max number of voices */

#define MAXHD	8	/* max heads on one stem */
#define MAXGR	30	/* max grace notes */
#define MAXDC	8	/* max decorations */
#define MAXLY	8	/* max number of lyrics */

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

/* bar types */
enum bar_type {			/* codes for different types of bars */
	B_INVIS,		/* invisible; for endings without bars and [|] */
	B_SINGLE,		/* |	single bar */
	B_DOUBLE,		/* ||	thin double bar */
	B_THIN_THICK,		/* |]   thick at section end  */
	B_THICK_THIN,		/* [|   thick at section start */
	B_LREP,			/* |:	left repeat bar */
	B_RREP,			/* :|	right repeat bar */
	B_DREP,			/* ::	double repeat bar */
	B_DASH			/* :    dashed bar */
};

/* clefs */
enum clef_type {
	TREBLE,
	ALTO1,
	ALTO2,
	ALTO,		/* 3 */
	ALTO4,
	BASS3,
	BASS		/* 4 */
};

/* decorations = index the deco_tb table in abcparse.c */
enum deco_type {
	D_NIL,		/* no decoration */
	D_dot = 128,	/* '.' - 0 */
	D_0,
	D_1,
	D_2,
	D_3,
	D_4,
	D_5,
	D_plus,
	D_accent,
	D_breath,
	D_crescendo_s,	/* 10 */
	D_crescendo_e,
	D_coda,
	D_DC,
	D_DS,
	D_diminuendo_s,
	D_diminuendo_e,
	D_downbow,
	D_emphasis,
	D_f,
	D_fermata,	/* 20 */
	D_ff,
	D_fff,
	D_ffff,
	D_fine,
	D_invertedfermata,
	D_longphrase,
	D_lowermordent,
	D_mediumphrase,
	D_mf,
	D_mordent,	/* 30 */
	D_open,
	D_p,
	D_pp,
	D_ppp,
	D_pppp,
	D_pralltriller,
	D_repeatbar,
	D_repeatbar2,
	D_roll,
	D_segno,	/* 40 */
	D_sfz,
	D_shortphrase,
	D_snap,
	D_tenuto,
	D_thumb,
	D_trill,
	D_turn,
	D_upbow,
	D_uppermordent,
	D_wedge,	/* 50 */
	D_slide,
	D_cresc,
	D_decresc,
	D_dimin,
	D_fp
};

/* note structure */
struct grace {			/* describes grace notes */
	char n;			/* number of grace notes */
	char sappo;		/* short appoggiatura */
	char p[MAXGR];		/* pitches */
	char a[MAXGR];		/* accidentals */
};
struct deco {			/* describes decorations */
	char n;			/* number of decorations */
	unsigned char t[MAXDC];	/* type of deco */
};
struct lyrics {
	unsigned char *w[MAXLY];	/* ptr to words */
};

struct note {		/* note or rest */
	short pits[MAXHD];	/* pitches for notes */
	short lens[MAXHD];	/* note lengths as multiple of BASE */
	char accs[MAXHD];	/* code for accidentals */
	char sl1[MAXHD];	/* which slur starts on this head */
	char sl2[MAXHD];	/* which slur ends on this head */
	char ti1[MAXHD];	/* flag to start tie here */
	char ti2[MAXHD];	/* flag to end tie here */
	unsigned invis:1;	/* invisible rest */
	unsigned word_end:1;	/* 1 if word ends here */
	unsigned stemless:1;	/* note with no stem (black note) */
	char nhd;		/* number of notes in chord - 1 */
	char p_plet, q_plet, r_plet; /* data for n-plets */
	char slur_st; 		/* how many slurs start here */
	char slur_end;		/* how many slurs end here */
	struct grace *gr;	/* grace notes */
	struct deco dc;		/* decorations */
	struct lyrics *ly;	/* lyrics */
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
	char state;		/* symbol state in file/tune */
#define ABC_S_GLOBAL 0			/* global definition */
#define ABC_S_HEAD 1			/* header definition (after X:) */
#define ABC_S_TUNE 2			/* in tune definition (after K:) */
#define ABC_S_EMBED 3			/* embedded header (between [..]) */
	short linenum;		/* line number / ABC file */
	char *text;		/* main text (INFO, PSCOM),
				 * guitar chord (NOTE, REST, BAR) */
	char *comment;		/* comment part (when keep_comment) */
	union {			/* type dependent part */
		struct {		/* K: info */
			signed char sf;		/* sharp (> 0) flats (< 0) */
			signed char old_sf;	/* previous value */
			char bagpipe;
			char minor;		/* major (0) / minor (1) */
			char empty;		/* no clef if 1 */
		} key;
		struct {		/* L: info */
			int base_length;	/* basic note length */
		} length;
		struct {		/* M: info */
			int m1;			/* upper value (0: no meter) */
			int m2;			/* lower value */
			int flag;		/* flag: 1->C, 2->C| */
			char *top;		/* upper value in string */
		} meter;
		struct {		/* Q: info */
			char *str;
			int length;
			int value;		/* (0: no tempo) */
		} tempo;
		struct {		/* V: info */
			char *name;		/* name */
			char *fname;		/* full name */
			char *nname;		/* nick name */
			char voice;		/* voice number */
		} voice;
		struct {		/* bar */
			struct deco dc;		/* decorations */
			enum bar_type type;
		} bar;
		struct {		/* clef */
			char clef;
			char forced;
		} clef;
		struct note note;	/* note or rest */
		struct {		/* user defined accent */
			unsigned char symbol;
			unsigned char value;
		} user;
		struct staff_s {	/* %%staves */
			char voice;
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

#if defined(__cplusplus)
extern "C" {
#endif
extern char *deco_tb[];
void abc_delete(struct abcsym *as);
void abc_free(struct abctune *first_tune);
void abc_init(void *alloc_f_api(int size),
	      void free_f_api(void *ptr),
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
#if defined(__cplusplus)
}
#endif
