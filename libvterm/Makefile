MAKEFLAGS+= -rR
VERBOSE=1
override CFLAGS +=-Wall -Iinclude -std=gnu17 -Wno-deprecated-declarations
CC:=gcc
LD:=ld
AR:=ar

ifeq ($(shell uname),SunOS)
  override CFLAGS +=-D__EXTENSIONS__ -D_XPG6 -D__XOPEN_OR_POSIX
endif

DEBUG=1
ifeq ($(DEBUG),1)
  override CFLAGS +=-ggdb -DDEBUG
endif

ifeq ($(PROFILE),1)
  override CFLAGS +=-pg
  override LDFLAGS+=-pg
endif
TERM_CFLAGS  +=$(shell pkg-config --cflags gtk+-2.0)
TERM_LDFLAGS +=$(shell pkg-config --libs   gtk+-2.0)

TERM_CFLAGS  +=$(shell pkg-config --cflags cairo)
TERM_LDFLAGS +=$(shell pkg-config --libs   cairo)
TERM_CFLAGS += -DPANGOTERM_SHAREDIR="\"$(SHAREDIR)\""
LDFLAGS:= -L $(PWD)
CFILES=$(sort $(wildcard src/*.c))
HFILES=$(sort $(wildcard include/*.h))
OBJECTS=$(CFILES:.c=.o)
LIBRARY=libvterm.a

BINFILES_SRC=$(sort $(wildcard bin/*.c))
BINFILES_OBJ=$(BINFILES_SRC:.c=.o)
.PRECIOUS: $(OBJECTS) $(BINFILES_OBJ)
BINFILES=$(BINFILES_SRC:.c=)

TBLFILES=$(sort $(wildcard src/encoding/*.tbl))
INCFILES=$(TBLFILES:.tbl=.inc)

HFILES_INT=$(sort $(wildcard src/*.h)) $(HFILES)

PREFIX=$(shell cd .. && pwd)
BINDIR=$(PREFIX)/bin
LIBDIR=$(PREFIX)/lib
INCDIR=$(PREFIX)/include
MANDIR=$(PREFIX)/share/man
MAN3DIR=$(MANDIR)/man3

all: $(LIBRARY) $(BINFILES)

$(LIBRARY): $(OBJECTS)
	$(AR) r $@ $^

src/%.i: src/%.c $(HFILES_INT)
	$(CC) $(CFLAGS) -o $@ -E $<

src/pango.i src/conf.i bin/pangoterm.i: CFLAGS+=$(TERM_CFLAGS)
src/pango.o src/conf.o bin/pangoterm.o: CFLAGS+=$(TERM_CFLAGS)

bin/pangoterm: LDFLAGS+=$(TERM_LDFLAGS)

bin/%.o: bin/%.c $(HFILES_INT)
	$(CC) $(CFLAGS) -o $@ -c $<

bin/%.i: bin/%.c $(HFILES_INT)
	$(CC) $(CFLAGS) -o $@ -E $<

src/%.o: src/%.c $(HFILES_INT)
	$(CC) $(CFLAGS) -o $@ -c $<

src/encoding/%.inc: src/encoding/%.tbl
	perl -CSD tbl2inc_c.pl $< >$@

src/fullwidth.inc:
	perl find-wide-chars.pl >$@

src/encoding.o: $(INCFILES)

bin/%: bin/%.o $(LIBRARY)
	$(CC) $(CFLAGS) -o $@ $< -lvterm -lncurses $(LDFLAGS)

t/harness.o: t/harness.c $(HFILES)
	$(CC) $(CFLAGS) -o $@ -c $<

t/harness: t/harness.o $(LIBRARY)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

.PHONY: test
test: $(LIBRARY) t/harness
	for T in `ls t/[0-9]*.test`; do echo "** $$T **"; perl t/run-test.pl $$T $(if $(VALGRIND),--valgrind) || exit 1; done

.PHONY: clean
clean:
	rm -f $(OBJECTS) $(INCFILES)
	rm -f t/harness.o t/harness
	rm -f $(LIBRARY) $(BINFILES)

.PHONY: install
install: install-inc install-lib install-bin

install-inc:
	install -d $(DESTDIR)$(INCDIR)
	install -m644 $(HFILES) $(DESTDIR)$(INCDIR)
	install -d $(DESTDIR)$(LIBDIR)/pkgconfig
	sed \
		-e "s,@PREFIX@,$(PREFIX)," \
		-e "s,@LIBDIR@,$(LIBDIR)," \
		-e "s,@VERSION@,$(VERSION)," \
		<vterm.pc.in \
		>$(DESTDIR)$(LIBDIR)/pkgconfig/vterm.pc

install-lib: $(LIBRARY)
	install -d $(DESTDIR)$(LIBDIR)
	install $(LIBRARY) $(DESTDIR)$(LIBDIR)/$(LIBRARY)

install-bin: $(BINFILES)
	install -d $(DESTDIR)$(BINDIR)
	install $(BINFILES) $(DESTDIR)$(BINDIR)/

