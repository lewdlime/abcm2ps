# Generated automatically from Makefile.in by configure.
# Makefile source for abcm2ps

VERSION = 3.7.21

CC = gcc
INSTALL = /usr/bin//install -c
INSTALL_DATA = ${INSTALL} -m 644
INSTALL_PROGRAM = ${INSTALL}

CPPFLAGS = -g -O2 -Wall -pipe -ansi -DHAVE_CONFIG_H -I.
LDFLAGS = -lm	# 

prefix = /usr/local
exec_prefix = ${prefix}

srcdir = .
bindir = ${exec_prefix}/bin
libdir = ${exec_prefix}/lib
datadir = ${prefix}/share

# unix
OBJECTS=abc2ps.o \
	abcparse.o buffer.o deco.o draw.o format.o music.o parse.o \
	subs.o syms.o
abcm2ps: $(OBJECTS)
	$(CC) -o abcm2ps $(OBJECTS) $(LDFLAGS)
$(OBJECTS): abcparse.h abc2ps.h config.h

# win32 with a cross-compiler
#CC32=i586-mingw32-gcc
#C32FLAGS=-O2 -pipe -Wall -DVERSION='"$(VERSION)"'
#OBJ32=abc2ps.obj \
#	abcparse.obj buffer.obj deco.obj draw.obj format.obj music.obj parse.obj \
#		subs.obj syms.obj
#%.obj : %.c
#	$(CC32) -c $(C32FLAGS) $(CPPFLAGS) $< -o $@
#$(OBJ32): abcparse.h abc2ps.h config.h
#abcm2ps.exe: $(OBJ32)
#	$(CC32) $(C32FLAGS) -o abcm2ps.exe $(OBJ32)

install: abcm2ps
	mkdir -p $(bindir); \
	mkdir -p $(datadir)/abcm2ps; \
	$(INSTALL_PROGRAM) abcm2ps $(bindir)
	for f in *.fmt; do \
		$(INSTALL_DATA) $$f $(datadir)/abcm2ps; \
	done

uninstall:
	echo "uninstalling..."; \
	rm -f $(bindir)/abcm2ps; \
	rm -rf $(datadir)/abcm2ps

dist:
	ln -s . abcm2ps-$(VERSION); \
	tar -zcvf abcm2ps-$(VERSION).tar.gz \
		abcm2ps-$(VERSION)/Changes \
		abcm2ps-$(VERSION)/INSTALL \
		abcm2ps-$(VERSION)/License \
		abcm2ps-$(VERSION)/Makefile \
		abcm2ps-$(VERSION)/Makefile.w32 \
		abcm2ps-$(VERSION)/Makefile.in \
		abcm2ps-$(VERSION)/README \
		abcm2ps-$(VERSION)/abc-draft.txt \
		abcm2ps-$(VERSION)/abc2ps.c \
		abcm2ps-$(VERSION)/abc2ps.h \
		abcm2ps-$(VERSION)/abcparse.c \
		abcm2ps-$(VERSION)/abcparse.h \
		abcm2ps-$(VERSION)/buffer.c \
		abcm2ps-$(VERSION)/configure \
		abcm2ps-$(VERSION)/configure.in \
		abcm2ps-$(VERSION)/config.h \
		abcm2ps-$(VERSION)/config.h.in \
		abcm2ps-$(VERSION)/config.guess \
		abcm2ps-$(VERSION)/config.sub \
		abcm2ps-$(VERSION)/deco.c \
		abcm2ps-$(VERSION)/deco.abc \
		abcm2ps-$(VERSION)/draw.c \
		abcm2ps-$(VERSION)/fbook.fmt \
		abcm2ps-$(VERSION)/features.txt \
		abcm2ps-$(VERSION)/fonts.fmt \
		abcm2ps-$(VERSION)/format.c \
		abcm2ps-$(VERSION)/format.txt \
		abcm2ps-$(VERSION)/install.sh \
		abcm2ps-$(VERSION)/journey.abc \
		abcm2ps-$(VERSION)/landscape.fmt \
		abcm2ps-$(VERSION)/mtunes1.abc \
		abcm2ps-$(VERSION)/mtunes2.abc \
		abcm2ps-$(VERSION)/music.c \
		abcm2ps-$(VERSION)/newfeatures.abc \
		abcm2ps-$(VERSION)/options.txt \
		abcm2ps-$(VERSION)/parse.c \
		abcm2ps-$(VERSION)/sample.abc \
		abcm2ps-$(VERSION)/sample2.abc \
		abcm2ps-$(VERSION)/sample3.abc \
		abcm2ps-$(VERSION)/sample3.eps \
		abcm2ps-$(VERSION)/subs.c \
		abcm2ps-$(VERSION)/syms.c \
		abcm2ps-$(VERSION)/tight.fmt \
		abcm2ps-$(VERSION)/voices.abc; \
	rm abcm2ps-$(VERSION)
#zip: abcm2ps.exe
#	zip abcm2ps-$(VERSION).zip abcm2ps.exe License

EXAMPLES = deco.ps \
	journey.ps \
	mtunes1.ps \
	mtunes2.ps \
	newfeatures.ps \
	sample.ps \
	sample2.ps \
	sample3.ps \
	voices.ps

test:	$(EXAMPLES)
%.ps: %.abc
	./abcm2ps -O $@ $<

clean:
	rm -f *.o # *.obj
