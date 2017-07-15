# abcm2ps

### Overview

abcm2ps is a program which converts music tunes from the ABC music
notation to PostScript or SVG.  
Based on the abc2ps version 1.2.5 from Michael Methfessel,
it was first developped to print barock organ scores that have
independant voices played on one or many keyboards and a pedal-board
(the 'm' of abcm2ps stands for many or multi staves/voices).  
Since this time, it has evolved so it can render many more music kinds.

### Features

The main features of abcm2ps are quite the same as the abc2ps ones,
but they are closer to the ABC draft 2.2 (February 2013):

    http://abcnotation.com/wiki/abc:standard:v2.2

### Installation and usage

The installation procedure is described in the file INSTALL.

Basically, the program usage is:

    abcm2ps [options] file1 [file1_options] file2 [file2_options] ...

where file1, file2, .. are the ABC input files. This will generate
a Postscript file (default name: `Out.ps` - run `abcm2ps -h` to
know the list of the command line options).

### Documentation

- options.txt contains the list of the command line options.

- the format parameters are described in:
    http://moinejf.free.fr/abcm2ps-doc/index.html

- the differences from the current ABC standard are described in:
    http://moinejf.free.fr/abcm2ps-doc/features.html

### Links

To know more about the ABC music notation, have a look at:
    http://abcnotation.com/

Guido Gonzatto maintains many abcm2ps binaries and more documentation at:
    http://abcplus.sourceforge.net/
