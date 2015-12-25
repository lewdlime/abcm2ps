VERSION=1.6.12

# general options
CPPFLAGS=
# -DUS_LETTER:	Handle US letter format instead of default european A4.
# -DBSTEM_DOWN: Let the B note have always its stem down.
# -DDEBUG:	Have '-v' working.

# unix
CC=gcc
CFLAGS=-g -O2 -pipe -Wall -DVERSION='"$(VERSION)"'
OBJECTS=abc2ps.o \
	abcparse.o buffer.o deco.o format.o music.o parse.o subs.o syms.o util.o
abcm2ps: $(OBJECTS)
	$(CC) $(CFLAGS) -o abcm2ps $(OBJECTS)
$(OBJECTS): abcparse.h abc2ps.h
music.o: style.h

# win32 with a cross-compiler
CC32=i586-mingw32-gcc
C32FLAGS=-O2 -pipe -Wall -DVERSION='"$(VERSION)"' -DUS_LETTER
OBJ32=abc2ps.obj \
	abcparse.obj buffer.obj deco.obj format.obj music.obj parse.obj \
		subs.obj syms.obj util.obj
%.obj : %.c
	$(CC32) -c $(C32FLAGS) $(CPPFLAGS) $< -o $@
$(OBJ32): abcparse.h abc2ps.h
abcm2ps.exe: $(OBJ32)
	$(CC32) $(C32FLAGS) -o abcm2ps.exe $(OBJ32)

dist:
	ln -s . abcm2ps-$(VERSION); \
	tar -zcvf abcm2ps-$(VERSION).tar.gz \
		abcm2ps-$(VERSION)/Changes \
		abcm2ps-$(VERSION)/License \
		abcm2ps-$(VERSION)/Makefile \
		abcm2ps-$(VERSION)/New.Features \
		abcm2ps-$(VERSION)/README \
		abcm2ps-$(VERSION)/ReadMe.abc2ps \
		abcm2ps-$(VERSION)/abc.txt \
		abcm2ps-$(VERSION)/abc2ps.c \
		abcm2ps-$(VERSION)/abc2ps.h \
		abcm2ps-$(VERSION)/abcparse.c \
		abcm2ps-$(VERSION)/abcparse.h \
		abcm2ps-$(VERSION)/blue_boy_bass.abc \
		abcm2ps-$(VERSION)/buffer.c \
		abcm2ps-$(VERSION)/deco.c \
		abcm2ps-$(VERSION)/desafinado.abc \
		abcm2ps-$(VERSION)/fbook.fmt \
		abcm2ps-$(VERSION)/fonts.fmt \
		abcm2ps-$(VERSION)/format.c \
		abcm2ps-$(VERSION)/journey.abc \
		abcm2ps-$(VERSION)/landscape.fmt \
		abcm2ps-$(VERSION)/layout.txt \
		abcm2ps-$(VERSION)/mtunes1.abc \
		abcm2ps-$(VERSION)/mtunes2.abc \
		abcm2ps-$(VERSION)/music.c \
		abcm2ps-$(VERSION)/newfeatures.abc \
		abcm2ps-$(VERSION)/parse.c \
		abcm2ps-$(VERSION)/sample.abc \
		abcm2ps-$(VERSION)/sample2.abc \
		abcm2ps-$(VERSION)/style.h \
		abcm2ps-$(VERSION)/style.pure \
		abcm2ps-$(VERSION)/subs.c \
		abcm2ps-$(VERSION)/syms.c \
		abcm2ps-$(VERSION)/tight.fmt \
		abcm2ps-$(VERSION)/util.c \
		abcm2ps-$(VERSION)/voices.abc; \
	rm abcm2ps-$(VERSION)
zip: abcm2ps.exe
	zip abcm2ps-$(VERSION).zip abcm2ps.exe License
clean:
	rm -f *.o
