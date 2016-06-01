# Makefile source for abcm2ps

VERSION = 8.11.6

CC = gcc
INSTALL = /usr/bin/install -c
INSTALL_DATA = ${INSTALL} -m 644
INSTALL_PROGRAM = ${INSTALL}

CPPFLAGS = -DHAVE_PANGO=1 -I.
CPPPANGO = -I/usr/include/pango-1.0 -pthread -I/usr/include/cairo -I/usr/include/glib-2.0 -I/usr/lib/arm-linux-gnueabihf/glib-2.0/include -I/usr/include/pixman-1  -I/usr/include/libpng12 -I/usr/include/freetype2  
CFLAGS = -g -O2 -Wall -pipe
LDFLAGS =  -lpangocairo-1.0 -lcairo -lpangoft2-1.0 -lpango-1.0 -lgobject-2.0 -lglib-2.0 -lfontconfig -lfreetype   -lm

prefix = /usr/local
exec_prefix = ${prefix}

srcdir = .
VPATH = .
bindir = ${exec_prefix}/bin
libdir = ${exec_prefix}/lib
datadir = ${prefix}/share
docdir = ${prefix}/doc

# unix
OBJECTS=abcm2ps.o \
	abcparse.o buffer.o deco.o draw.o format.o front.o glyph.o music.o parse.o \
	subs.o svg.o syms.o
abcm2ps: $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS) $(LDFLAGS)

$(OBJECTS): config.h Makefile
abcparse.o abcm2ps.o buffer.o deco.o draw.o format.o front.o glyph.o \
	music.o parse.o subs.o svg.o syms.o: abcm2ps.h
subs.o: subs.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(CPPPANGO) -c -o $@ $<

DOCFILES=$(addprefix $(srcdir)/,Changes README *.abc *.eps *.txt)

install: abcm2ps
	mkdir -p $(bindir); \
	mkdir -p $(datadir)/abcm2ps; \
	mkdir -p $(docdir)/abcm2ps; \
	$(INSTALL_PROGRAM) abcm2ps $(bindir)
	for f in $(srcdir)/*.fmt; do \
		$(INSTALL_DATA) $$f $(datadir)/abcm2ps; \
	done
	for f in $(DOCFILES); do \
		$(INSTALL_DATA) $$f $(docdir)/abcm2ps; \
	done

uninstall:
	echo "uninstalling..."; \
	rm -f $(bindir)/abcm2ps; \
	rm -rf $(datadir)/abcm2ps; \
	rm -rf $(docdir)/abcm2ps

DIST_FILES = \
	abcm2ps-$(VERSION)/Changes \
	abcm2ps-$(VERSION)/INSTALL \
	abcm2ps-$(VERSION)/Makefile \
	abcm2ps-$(VERSION)/Makefile.in \
	abcm2ps-$(VERSION)/README \
	abcm2ps-$(VERSION)/abcm2ps.c \
	abcm2ps-$(VERSION)/abcm2ps.h \
	abcm2ps-$(VERSION)/abcparse.c \
	abcm2ps-$(VERSION)/accordion.abc \
	abcm2ps-$(VERSION)/bravura.abc \
	abcm2ps-$(VERSION)/build.ninja \
	abcm2ps-$(VERSION)/buffer.c \
	abcm2ps-$(VERSION)/chinese.abc \
	abcm2ps-$(VERSION)/configure \
	abcm2ps-$(VERSION)/config.h \
	abcm2ps-$(VERSION)/config.h.in \
	abcm2ps-$(VERSION)/deco.c \
	abcm2ps-$(VERSION)/deco.abc \
	abcm2ps-$(VERSION)/draw.c \
	abcm2ps-$(VERSION)/flute.fmt \
	abcm2ps-$(VERSION)/format.c \
	abcm2ps-$(VERSION)/free.abc \
	abcm2ps-$(VERSION)/front.c \
	abcm2ps-$(VERSION)/glyph.c \
	abcm2ps-$(VERSION)/glyphs.abc \
	abcm2ps-$(VERSION)/landscape.fmt \
	abcm2ps-$(VERSION)/music.c \
	abcm2ps-$(VERSION)/musicfont.fmt \
	abcm2ps-$(VERSION)/newfeatures.abc \
	abcm2ps-$(VERSION)/options.txt \
	abcm2ps-$(VERSION)/parse.c \
	abcm2ps-$(VERSION)/sample.abc \
	abcm2ps-$(VERSION)/sample2.abc \
	abcm2ps-$(VERSION)/sample3.abc \
	abcm2ps-$(VERSION)/sample3.eps \
	abcm2ps-$(VERSION)/sample4.abc \
	abcm2ps-$(VERSION)/sample5.abc \
	abcm2ps-$(VERSION)/sample8.html \
	abcm2ps-$(VERSION)/subs.c \
	abcm2ps-$(VERSION)/svg.c \
	abcm2ps-$(VERSION)/syms.c \
	abcm2ps-$(VERSION)/voices.abc

dist: Changes
	ln -s . abcm2ps-$(VERSION); \
	tar -zcvf abcm2ps-$(VERSION).tar.gz $(DIST_FILES); \
	rm abcm2ps-$(VERSION)

zip-dist:
	ln -s . abcm2ps-$(VERSION); \
	zip -r abcm2ps-$(VERSION).zip $(DIST_FILES); \
	rm abcm2ps-$(VERSION)

zip: abcm2ps.exe
	strip abcm2ps.exe; \
	cd ..; zip -r abcm2ps-$(VERSION).zip \
	abcm2ps-$(VERSION)/abcm2ps.exe \
	abcm2ps-$(VERSION)/License \
	abcm2ps-$(VERSION)/Changes \
	abcm2ps-$(VERSION)/INSTALL \
	abcm2ps-$(VERSION)/sample3.eps \
	abcm2ps-$(VERSION)/*.abc \
	abcm2ps-$(VERSION)/*.fmt \
	abcm2ps-$(VERSION)/*.txt ; cd -

EXAMPLES = accordion.ps \
	chinese.ps \
	deco.ps \
	newfeatures.ps \
	sample.ps \
	sample2.ps \
	sample3.ps \
	sample4.ps \
	sample5.ps \
	voices.ps

test:	$(EXAMPLES)
%.ps: %.abc
	./abcm2ps -O $@ $<

clean:
	rm -f *.o $(EXAMPLES) # *.obj
