..
   Process this file with rst2man from python-docutils
   to generate a nroff manual page

=======
abcm2ps
=======

--------------------------------------------------
translate ABC music notation to PostScript or SVG
--------------------------------------------------

SYNOPSIS
========

  ``abcm2ps`` [global_options] ABC_file [file_options] ...

DESCRIPTION
===========

``abcm2ps`` translates tunes written in
the ABC music notation format to customary sheet music scores in
PostScript or SVG. It is based on ``abc2ps`` 1.2.5 and was
developed mainly to print Baroque organ scores that have
independent voices played on multiple keyboards and a
pedal-board. The program has since been extended to support
various other notation conventions in use for sheet music.

Options given immediately after the command name apply to
the run as a whole; options given after an ABC file name apply
to that file.

Formatting parameters can also be set in "format files" and
in the ABC files themselves.

OPTIONS
=======

The list of the command line options may be known running::

   abcm2ps -h

The options may be grouped when they have no argument, but the
last one (ex: ``-lnGI20``).

The options may be disabled when starting with '+' or ending with '-'
(ex: ``+MT1`` is the same as ``-MT1-``).

The general output format is the last found in the command line.
It may be:

-E    for Encapsulated PostScript, one file per tune

-g    for SVG, one file per tune

-v    for SVG, one file per page

-X    for XHTML+SVG

-z    for (X)HTML+SVG with (X)HTML+ABC input

(see below for more information)

List of the options
-------------------

\-
   Read the abc file from stdin.

\--<format> <value>
   Set the <format> parameter to <value>.

   This has the same effect as a format parameter
   directly in the source file.

-a <float>
   Maximal horizontal compression when staff breaks are
   chosen automatically. Must be a float between 0 and 1.

   This correspond to the ``%%maxshrink``
   formatting parameter (default: 0.65).

-A
   This option inserts reference elements in the PostScript
   or SVG output.

-B <int>, +B
   Try to typeset <int> bars on each staff line.

   This corresponds to the ``%%barsperstaff`` formatting parameter.

-b <int>
   Start measure numbering at <int>.

   This corresponds to the ``%%measurefirst`` formatting parameter.

-c, +c
   The continuation symbol is implicitly appended to each
   music line. This amounts to automatic line breaking.

   This corresponds to the ``%%continueall`` formatting parameter.

-D <dir>
    Search the format files in the directory <dir>.

-d <unit>
   Set the vertical interstaff space to <unit>.

   This corresponds to the ``%%staffsep`` formatting parameter
   (default: 46pt).

-E
   Produce EPS output instead of simple PS.

   In this mode, each tune goes to a different file which
   name is "<name>nnn.eps" or "<title>.eps" (see option '-O')

        - 'nnn' is a sequence number incremented at each tune

   Output to stdout is forbidden.

   EPS files are normally embedded into Postscript documents,
   but they may be a way to generate graphical images. For
   example, using GhostScript::

        abcm2ps voices -Ee7
        gs -sDEVICE=pngmono -r80 -g590x174 \
           -dBATCH -dNOPAUSE \
           -sOutputFile=quitolis.png Out001.eps

   \(the values for -g are the values of the bounding box in
   the .eps, multiplied by (80 / 72), where 80 is the value
   for -r, and 72 is the default resolution)

-e [ <tune index list> ] [ <regular expression> ]
   Select which tunes from an ABC file to print.

   <tune index list> is either a comma-separated list of tune
   numbers (as per the ``X:`` header), or a regular expression
   which will be matched against the tune headers as a whole.
   The ``-e`` option must occur after an ABC file
   name and applies to that file.

   Ranges of tune numbers may be specified like ``<t1>-<t2>``;
   <t2> may be omitted which means
   "all remaining tunes until the end of file". Note that
   filtering may cause problems, e.g., with global (non-tune)
   definitions in the ABC file.

   This corresponds to the ``%%select`` formatting parameter.

-F <file>, +F
    Read the format (or PostScript) file <file>.

    When omitted, the default type of a format file is '.fmt'.

    In the form '+F', the default format file ('default.fmt') is not
    read.

-f
   Enable flat beams (useful for bagpipe tunes).

   This corresponds to the ``%%flatbeams`` formatting parameter.

-G, +G
   Omit slurs on grace notes.

   This corresponds to the ``%%graceslurs`` formatting parameter.

-g
    Produce SVG output instead of EPS.

    In this mode each tune goes to a different file which name
    is 'Outnnn.svg' (see option '-O').

    If the output is stdout (option '-O-'), all the SVG images
    are output without XML header.

-H
    Display the current format values.

-h
   Quick help, equivalent to "abcm2ps" without any arguments.

   This also shows the default settings for some parameters.

-I <unit>
   Indent the first line of the tune by <unit> (default: 0).

   This corresponds to the ``%%indent`` formatting parameter.

-i, +i
    Insert a red cercle around the errors in the PostScript output.

-j <int>[b], +j
   Output a measure number every <int> measures.

   If <int> is 0, the measure number appears at the left of each staff.
   The trailing ``b`` causes a box to be drawn
   around each measure number (default: no measure numbering).

   This corresponds to the ``%%measurenb`` formatting parameter.

-k <int>
   Set the size of the PostScript output buffer in Kibytes.

   Setting this value to a higher value permits the
   generation of big tunes with -E or -g. The default value is 64.

-l, +l
   Generate landscape output.

   This corresponds to the ``%%landscape`` formatting parameter.

-M, +M
   Suppress lyrics.

   See the ``%%writefields w`` formatting parameter.

-m <unit>
   Set the left margin to <unit> (default: 1.8cm).

   This corresponds to the ``%%leftmargin`` formatting parameter.

-N <int>, +N
   Number the pages.

   <int> indicates the mode:

      0
         no page numbers
      1
         at top left
      2
         at top right
      3
         at top left on even pages, top right on odd pages
      4
         at top right on even pages, top left on odd pages

   For compatibility with previous versions, '+N' is the same as
   '-N0', and '-N' is the same as '-N2'.

   If a header is defined ("%%header"), this option is ignored.

-n, +n
   Include notes and history from ABC tune ``N:`` fields.

   See the ``%%writehistory N`` formatting parameter.

-O [ <directory> ] [ <name> ], +O
   Define the output file directory and/or name.

   The directory must end with the directory separator
   ('/' for unix/windows, '\\' for mac).

   By default, the output file goes to the current directory
   with the name:

      'Out.ps' for PS,

      'Outnnn.eps' for EPS (see option '-E'),

      'Outnnn.svg' for SVG (see options '-g' and '-v') or

      'Out.xhtml' for XHTML+SVG (see options '-X' and '-z').

   'nnn' is a sequence number.

   When <name> is present, it is the name of the file, or it
   replaces "Out" in the file name.

   If <name> is '=', it is replaced by the name of the ABC
   source file (not for '-z').

   If <name> is '-', the result is output to stdout (not for EPS).
   '+O' resets the output file directory and name to their defaults.

-p
   Bagpipe format.

   When present, format output for bagpipe regardless of key.

-q
   Quiet mode.

   When present, only the errors are shown.

-s <float>
   Set the page scale factor to <float>. Note that the header
   and footer are not scaled (default: 0.75).

   This corresponds to the ``%%scale`` formatting parameter.

-S
   Secure mode.

   When present, file inclusion (%%format and %%EPS) and PostScript
   injection (%%beginps and %%postscript) are disabled.

-T <int> [ <voice> ], +T [ <int> [<voice> ] ]
   Activate or deactivate tablature drawing.

   - <int> is the tablature number as defined in ``%%tablature``.
      There may be only 8 different tablatures.

   - <voice> is the voice name, full name or subname as found in V:.
      When absent, apply to all voices.

   Up to 4 such commands may be defined.

      Ex: '-T1flute +T2'

-V
   Show the version number.

-v
   Produce SVG output instead of simple PS.

   In this mode each page goes to a different file which name
   is 'Outnnn.svg' (see option '-O').

-w <unit>
   Adjust the right margin such that the staff width
   is <unit> (default: none).

   This corresponds to the ``%%staffwidth`` formatting parameter.

-X
   Produce XML+SVG output instead of simple PS.

   The default file name is 'Out.xhtml' (see option '-O').

-x, +x
   Include the ``X:`` tune number in the title.

   This corresponds to the ``%%writefields`` formatting parameter.

-z
   Produce SVG images from ABC embedded in markup language files
   (HTML, XHTML..).

   The source file is copied to the output file and the ABC sequences
   are converted to SVG images.
   The ABC sequences start by either ``%abc..`` or ``X:..``
   and stop on the first markup tag (``<..``).

   The generation creates one image per block, i.e. a music line
   or a text block. For a same rendering as the other SVG generation
   (-g, -v or -X), don't forget to set the line space to null, for
   example enclosing the ABC sequences by::

      <div style="line-height:0"> .. </div>

   There can be only one output file.

   Note that the default output file is 'Out.xhtml', so, don't
   forget to change the file type if you generate HTML (.html)
   or XML (.xml) files.

   See "sample8.html" for a source example.

-0, +0
   Split tunes across page breaks if necessary.

   This corresponds to the ``%%splittune`` formatting parameter.

-1, +1
   Output one tune per page.

   This corresponds to the ``%%oneperpage`` formatting parameter.

ADDITIONAL FEATURES
===================

Clefs
   Clefs can be given in ``K:`` and ``V:`` headers.
   The full syntax is::

      clef=<type><line>[+8|-8]

   "clef=" can be omitted when the <type> is a clef name.

   <type> denotes the clef type. It  may be:

   - A note pitch (``G``, ``C``, or ``F``)

      The pitch indicates which clef is meant:
      ``G`` is the treble clef,
      ``C`` the alto clef and
      ``F`` the bass clef.
      It also gives the name of the note that appears
      on the clef's line.

   - A clef name

      The available clef names are
      ``treble`` (clef gives the pitch for ``G``),
      ``alto`` or ``tenor`` (``C``), and
      ``bass`` (``F``).

   - ``perc`` or ``P``

      In percussion mode, accidentals change the glyphs used for
      note heads. By default, sharp notes are drawn as "x" and
      flat notes as circled "x".
      This may be changed by redefining the PostScript functions
      ``pshhd`` and ``pflhd``.

   - ``none``

      No clef will be displayed.

   The <line> gives the number of
   the line within the staff that the base clef will be written
   on. The default values are 2 for the treble clef, 3 for the
   alto clef, and 4 for the tenor and bass clefs.

   The "+8" and "-8"
   options draw an 8 above or below the staff, respectively.

   When no clef is specified, clef changes
   between "bass"
   and "treble" will be inserted
   automatically.

Multi-voice typesetting
   Multiple voices may be defined within the header or the
   tune using::

      V:<name> <definition> ...

   where <name> is a word consisting of letters and digits only
   (like "violin1"). In the tune body, the
   following notes refer to this voice until
   another "V:" is encountered.

   A <definition> can be one of:

   * "clef="...

      See above

   * "name="<name> or "nm="<name>

      The <name> will be
      displayed at the beginning of the first staff. It can
      contain "\\n" sequences
      which will force line breaks. If it contains
      whitespace it must be double-quoted.

   * "subname="<name> or "snm="<name>

      The <name> will be displayed at the beginning of all staves
      except for the first. It can
      contain "\\n" sequences
      which will force line breaks. If it contains
      whitespace it must be double-quoted.

   * "merge"

      The voice goes on the same staff as the previous voice.

   * "up" or "down"

      Forces the direction of the stems for the voice.

   * "dyn=up" or "dyn=down" or "dyn=auto"

      Forces positioning of dynamic marks (above or
      below the staff) or reverts to automatic positioning
      (the default).

   * "gstem=up" or "gstem=down" or "gstem=auto"

      Forces the direction of the stems of grace
      notes (always up or always down) or reverts to
      automatic positioning (the default).

   * "stem=auto"

      Reverts to automatic positioning of note stems
      (up or down) (the default).

   * "lyrics=up" or "lyrics=down" or "lyrics=auto"

      Places lyrics above or below the staff or
      reverts to automatic positioning (the default)

   * "gchord=up" or "gchord=down"

      Places guitar chords above (the default) or below the staff.

   * "stafflines="<value>

      Sets the number of lines on the staff in question. (default: 5)

   * "staffscale="<value>

      Sets the scale of the associated staff up to 3. (default: 1)

   All other definitions are ignored.

Definition of the staff system
   By default, each voice goes on its own
   staff. The ``%%staves <definition>``
   pseudo-comment can be used to control staff
   assignment. The <definition>
   consists of voice names (from ``V:``) and pairs of
   parentheses, braces or brackets.

   - When a voice name is not within a pair of
     special characters, it goes on a separate staff.

   - For voice names enclosed in brackets, a bracket
     is displayed at the beginning of each line that joins
     the staves of the voices in question.

   - For voice names enclosed in braces, all the
     voices go on two staves (keyboard score). There can be
     at most four voices between a single pair of braces.

   - For voice names enclosed in parentheses, all the
     voices appear on a single staff.

   The ``|`` character prevents measure bars from
   being drawn between two staves.
   If ``%%staves`` occurs in a tune, all the
   voices not mentioned will not be output at all.

   The ``%%score`` directive occurs in the ABC
   draft 2.0 standard and is similar to
   the ``%%staves`` specification described
   above. The rules are:

   - Voice names within parentheses form a "voice
     group" and go on a single staff. A voice name that is
     not within parentheses forms its own voice group and
     goes on a staff by itself.

   - Voice groups within braces form a "voice block"
     and are preceded by a big brace in the output. This is
     especially useful for keyboard music.

   - Voice groups or voice blocks within brackets
     form a "voice block" and will be preceded by a big
     bracket in the output.

   - If a ``|`` character occurs between two
     voice groups or voice blocks, the bar lines in all of
     the associated staves will be continuous.

   - A single voice surrounded by two voice groups
     can be preceded by an asterisk to make it into a
     "floating" voice. This means that, for each note of the
     voice, a separate decision is made whether it is printed
     on the preceding or the following voice group's staff.

   - Voices that appear in the tune body but not in
     the ``%%score`` directive will not be output at
     all. If there is no ``%%score`` directive, each
     voice will be output on its own staff.

   - A ``%%score`` directive inside a tune
     resets the mechanism so voices can be removed or added.

Voice overlay
   You can add notes to a staff without introducing a
   complete extra voice by using the ampersand
   (``&``).  A single measure can be split into two voices like::

      |F2A2Bc&F2c2bc|

   The ``(&...&...&)`` construction allows splitting multiple
   measures::

     |!f!(&GG<G|GG F=E| E2  E(_D/E)|_D D  C      D |C4- |C
        &DC<C|CC_D C|=B,2_B,B,   |_A,A,(G,/A,/)B,|F,4-|F,&)zzD=E|

   A double ampersand (``&&``) will allow
   overlaying more than two lines of music but this feature has
   not yet been implemented.

Lyrics
   Aligned lyrics under a staff are written as a
   ``w:`` line directly below the staff line. For example::

      edc2 edc2|
      w:Three blind mice, three blind mice

   Each word in the ``w:`` line (delimited by
   blanks) is associated with one note, in sequence. The
   following special symbols modify this behaviour:

   ``*``
      Skips one note.

   ``-``
      Splits a word into two syllables which are
      associated with two adjacent notes. A "-" is drawn
      between them.

   ``|``
      Advances to the next bar line.

   ``~``
      Is output as a space, but unites two words so
      they appear under a single note.

   ``_``
      Draws a thin underscore from the previous note
      to the next.

   To include more than one line of lyrics, use
   multiple ``w:`` lines. To include hyphens without
   splitting a word over multiple notes,
   use ``\-``.

   If a word starts with a digit, this is interpreted as a
   stanza number and outdented a bit to the left.

Slurs and ties
   The direction of slurs and ties may be controlled using
   the "(," / "('" and "-," / "-'" constructions.

Microtone pitches
   Microtone pitches are indicated by a fraction after an
   accidental, as in ``^3/4c``. When omitted, the
   numerator defaultes to 1 and the denominator to 2
   (so ``^/c`` is the same as ``^1/2c``). The
   numerator and denominator values may not exceed 256. There
   is built-in support for quarter-tone accidentals (1/2 and
   3/2 sharps and flats); for other values, rendering functions
   must be defined using ``%%postscript``.

EPS inclusion
   EPS files may be included inside tunes using the
   pseudo-comment ``%%EPS <file>``.

SEE ALSO
========

A brief introduction referencing further documentation is
installed in <docdir>/abcm2ps/README.md.

The ABC music notation is at http://abcnotation.com/.

Especially, you may find a discussion of differences with the
ABC standard at http://moinejf.free.fr/abcm2ps-doc/features.xhtml
and a list of formatting options at http://moinejf.free.fr/abcm2ps-doc/.

AUTHOR
======

``abcm2ps`` was written by Jean-François Moine <http://moinejf.free.fr/>
starting from ``abc2ps`` by Michael Methfessel.

Parts of this manual have been written by Anselm Lingnau
<lingnau@debian.org> for the ``Debian`` system.

Permission is granted to copy, distribute and/or modify this
document as long as its origin is not misrepresented.
