/*
 *  This file is part of abc2ps, Copyright (C) 1996,1997 Michael Methfessel
 *  See file abc2ps.c for details.
 */

/*   Global style parameters for the note spacing and Knuthian glue. */

/*   Parameters here are used to set spaces around notes.
     Names ending in p: prefered natural spacings
     Names ending in x: expanded spacings
     Units: everything is based on a staff which is 24 points high
     (ie. 6 pt between two staff lines). */

/* name for this set of parameters */
#define STYLE "std"

/* ----- Parameters for the note-note spacing ----- */
/* That is: the internote spacing between two notes that follow
   each other without a bar in between.

   -- lnn is an overall multiplier, i.e. the final note width in points
      is the return value of function nwidth times lnn.
   -- bnn determines how strongly the first note enters into the spacing.
      For bnn=1, the spacing is calculated using the first note.
      For bnn=0, the spacing is the average for the two notes.
   -- fnn multiplies the spacing under a beam, to compress the notes a bit
   -- gnn multiplies the spacing a second time within an n-tuplet */

#define lnnp 40.0
#define bnnp 0.9
#define fnnp 0.9
#define gnnp 0.8

#if 0
float lnnx=90;
float bnnx=0.9;
float fnnx=0.9;
float gnnx=0.85;

/* ---- Parameters for the bar-note spacing ----- */
/* That is: the spacing between a bar and note at the measure start.

   -- lbn is the overall multiplier for the return values from nwidth.
   -- vbn is the note duration which defines the default spacing.
   -- bbn determines how strongly the note duration enters into the spacing.
      For bbn=1, the spacing is lbn times the return value of nwidth.
      For bbn=0, the spacing is lbn times the width of rbn times timesig. */

float lbnp=30;
float bbnp=0.2;
float rbnp=0.1;

float lbnx=75;
float bbnx=0.2;
float rbnx=0.08;

/* ---- Parameters for the note-bar spacing ----- */
/* That is: the spacing between a note at the measure end and the bar.

   -- lnb is the overall multiplier for the return values from nwidth.
   -- vnb is the note duration which defines the default spacing.
   -- bnb determines how strongly the note duration enters into the spacing.
      For bnb=1, the spacing is lnb times the return value of nwidth.
      For bnb=0, the spacing is lnb times the width of rbn times timesig. */

float lnbp=30;
float bnbp=0.7;
float rnbp=0.125;

float lnbx=75;
float bnbx=0.6;
float rnbx=0.125;

/* ---- Parameters for centered single note in a measure ----- */
/* That is: the total length = bar-note + note-bar spacings

   -- ln0 is the overall multiplier for the return values from nwidth.
   -- bn0 interpolates between two limiting cases
      For bn0=0, this case is treated like bar-note and note-bar cases
      For bn0=1, the note is centered in the measure.  */

float ln0p=30;
float bn0p=0;

float ln0x=90;
float bn0x=0;
#endif
