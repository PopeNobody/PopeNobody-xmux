
VERBOSE=1
override CFLAGS +=-Wall -Iinclude -std=c99 -Wpedantic

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

CFILES=$(sort $(wildcard src/*.c))
HFILES=$(sort $(wildcard include/*.h))
OBJECTS=$(CFILES:.c=.o)
LIBRARY=libvterm.a

BINFILES_SRC=$(sort $(wildcard bin/*.c))
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
	$(CC) -rpath $(LIBDIR) -version-info $(VERSION_CURRENT):$(VERSION_REVISION):$(VERSION_AGE) -o $@ $^ $(LDFLAGS)

src/%.o: src/%.c $(HFILES_INT)
	$(CC) $(CFLAGS) -o $@ -c $<

src/encoding/%.inc: src/encoding/%.tbl
	perl -CSD tbl2inc_c.pl $< >$@

src/fullwidth.inc:
	perl find-wide-chars.pl >$@

src/encoding.o: $(INCFILES)

bin/%: bin/%.c $(LIBRARY)
	$(CC) $(CFLAGS) -o $@ $< -lvterm $(LDFLAGS)

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

