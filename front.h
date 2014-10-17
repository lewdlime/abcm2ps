/* declarations for front.c */

/* file types */
#define FE_ABC 0
#define FE_FMT 1
#define FE_PS 2
void front_init(int edit,
		int eol,
		void include_cb(unsigned char *fn));
unsigned char *frontend(unsigned char *s,
			int ftype,
			char *fname,
			int linenum);
