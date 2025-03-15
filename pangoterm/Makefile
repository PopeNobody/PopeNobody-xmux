CFLAGS  +=-Wall -Iinclude -std=c99 -DGDK_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED -DGSEAL_ENABLE -DGTK_DISABLE_SINGLE_INCLUDES
LDFLAGS +=-lutil

#    DEBUG:=1
ifeq ($(DEBUG),1)
  CFLAGS +=-ggdb -DDEBUG
endif

ifeq ($(PROFILE),1)
  CFLAGS +=-pg
  LDFLAGS+=-pg
endif

CFLAGS  +=$(shell pkg-config --cflags vterm)
LDFLAGS +=$(shell pkg-config --libs   vterm)

CFLAGS  +=$(shell pkg-config --cflags gtk+-2.0)
LDFLAGS +=$(shell pkg-config --libs   gtk+-2.0)

CFLAGS  +=$(shell pkg-config --cflags cairo)
LDFLAGS +=$(shell pkg-config --libs   cairo)

PREFIX=$(shell cd .. && pwd)
BINDIR=$(PREFIX)/bin
SHAREDIR=$(PREFIX)/share

CFLAGS+=-DPANGOTERM_SHAREDIR="\"$(SHAREDIR)\""

HFILES=$(wildcard *.h)
CFILES=$(wildcard *.c)
OBJECTS=$(CFILES:.c=.o)

all: pangoterm

pangoterm: $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c $(HFILES)
	$(CC) $(CFLAGS) -o $@ -c $<

.PHONY: clean
clean:
	rm -f $(OBJECTS) pangoterm

.PHONY: install
install: install-bin install-share

# rm the old binary first in case it's still in use
install-bin: pangoterm
	install -d $(DESTDIR)$(BINDIR)
	rm -f $(DESTDIR)$(BINDIR)/pangoterm
	install -m755 pangoterm $(DESTDIR)$(BINDIR)/pangoterm

install-share:
	install -d $(DESTDIR)$(SHAREDIR)/pixmaps
	install -m644 pangoterm.svg $(DESTDIR)$(SHAREDIR)/pixmaps/
	install -d $(DESTDIR)$(SHAREDIR)/applications
	install -m755 pangoterm.desktop $(DESTDIR)$(SHAREDIR)/applications/

# DIST CUT

VERSION=0

DISTDIR=pangoterm-$(VERSION)

distdir:
	mkdir __distdir
	cp LICENSE __distdir
	cp *.c *.h __distdir
	cp pangoterm.svg pangoterm.desktop pangoterm.cfg __distdir
	sed "/^# DIST CUT/Q" <Makefile >__distdir/Makefile
	mv __distdir $(DISTDIR)

TARBALL=$(DISTDIR).tar.gz

dist: distdir
	tar -czf $(TARBALL) $(DISTDIR)
	rm -rf $(DISTDIR)

dist+bzr:
	$(MAKE) dist VERSION=$(VERSION)+bzr`bzr revno`

distdir+bzr:
	$(MAKE) distdir VERSION=$(VERSION)+bzr`bzr revno`
