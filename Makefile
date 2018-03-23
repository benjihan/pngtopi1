#! /usr/bin/make -f
#
# ----------------------------------------------------------------------
#
# GNU/makefile for pngtopi1
#
# By Benjamin Gerard
#

PACKAGE   := pngtopi1
srcdir    := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))
topdir    := $(realpath $(srcdir))
VERSION   := $(shell $(topdir)/vcversion.sh || echo ERROR)
ifeq ($(VERSION),ERROR)
$(error vcversion.sh failed)
endif
pkgconfig  = $(shell $(PKGCONFIG) $(1) || echo n/a)

# ----------------------------------------------------------------------

.ONESHELL:
.DELETE_ON_ERROR:

# ----------------------------------------------------------------------

vpath %.c $(srcdir)

# ----------------------------------------------------------------------
#  Toolchain
# ----------------------------------------------------------------------

ifndef STRIP
ifeq ($(CC:%gcc=gcc),gcc)
STRIP = $(CC:%gcc=%strip)
else
STRIP = strip
endif
endif

ifndef PKGCONFIG
ifeq ($(CC:%gcc=gcc),gcc)
PKGCONFIG = $(CC:%gcc=%pkg-config)
else
PKGCONFIG = pkg-config
endif
endif

# ----------------------------------------------------------------------
#  Confgure libpng with pkg-config
# ----------------------------------------------------------------------

ifndef PNGVERSION
PNGVERSION := $(call pkgconfig,--modversion libpng)
ifeq ($(PNGVERSION),n/a)
$(error unable to configure libpng with $(PKGCONFIG))
endif
endif

ifndef PNGLIBS
PNGLIBS := $(call pkgconfig,--libs libpng)
endif

ifndef PNGCFLAGS
PNGCFLAGS := $(call pkgconfig,--cflags libpng)
endif

# ----------------------------------------------------------------------
#  Build variables
# ----------------------------------------------------------------------

target   := $(PACKAGE)
CPPFLAGS += -DPACKAGE_STRING='"$(PACKAGE) $(VERSION)"'
CFLAGS   += $(PNGCLAGS)
LDLIBS   += $(PNGLIBS)

# ----------------------------------------------------------------------
#  Rules
# ----------------------------------------------------------------------

all: $(target)
.PHONY: all

clean: ; -rm -f -- $(target)
.PHONY: clean

# ----------------------------------------------------------------------
#  Distrib
# ----------------------------------------------------------------------

dist_dir := $(PACKAGE)-$(VERSION)
dist_arc = $(dist_dir).tar.xz
dist_lst = LICENSE README.md vcversion.sh Makefile pngtopi1.c pngtopi1.1

dist: distrib
distcheck: dist-check
distrib: dist-arc
dist-all: all distrib
dist-check: dist-make dist-sweep

dist-dir:
	@test ! -d "$(dist_dir)" || \
	chmod -R -- u+w $(dist_dir)/ && \
	rm -rf -- $(dist_dir)/
	mkdir -- "$(dist_dir)"	# Fail if exist or whatever

dist-arc: dist-dir
	@set -o pipefail; tar -cpC $(topdir) $(dist_lst) | tar -xpC $(dist_dir)
	echo $(VERSION) >$(dist_dir)/VERSION
	tar --owner=0 --group=0 -czpf $(dist_arc) $(dist_dir)/
	rm -rf -- $(dist_dir)/
	echo "distrib file -- \"$(dist_arc)\" -- is ready"

dist-extract: dist-arc
	@[ ! -e $(dist_dir) ] || { echo $(dist_dir) already exist; false; }
	@[ ! -e _build-$(dist_dir) ] || rm -rf -- _build-$(dist_dir)
	@tar xf $(dist_arc)
	@chmod -R ug-w $(dist_dir)/
	@echo "extracted -- \"$(dist_arc)\""

.IGNORE: dist-make    # detect error on presence of _build-$(dist_dir)
dist-make: dist-extract
	@mkdir -- _build-$(dist_dir) && \
	echo "compiling -- $(dist_dir)" && \
	{ test "x$(MAKERULES)" = x || \
	cp -- "$(MAKERULES)" _build-$(dist_dir)/; } && \
	$(MAKE) -sC _build-$(dist_dir) \
		-f ../$(dist_dir)/Makefile dist-all && \
	rm -rf -- _build-$(dist_dir)

dist-sweep:
	@test ! -d $(dist_dir) || \
	chmod -R -- u+w $(dist_dir) && rm -rf -- $(dist_dir)
	@test ! -d _build-$(dist_dir) || {\
		rm -rf -- _build-$(dist_dir); \
		echo "compilation of $(dist_dir) has failed"; \
		false; }

.PHONY: dist dist-all dist-arc dist-dir dist-check dist-extract	\
        dist-make dist-sweep distcheck

# ----------------------------------------------------------------------
#  Install / Uninstall
# ----------------------------------------------------------------------

ifndef prefix
PREFIX = $(error 'prefix' must be set to install)
else
PREFIX = $(prefix)
endif

ifndef datadir
DATADIR = $(PREFIX)/share
else
DATADIR = $(datadir)
endif

exec_dir = $(PREFIX)
bindir   = $(exec_dir)/bin
libdir   = $(exec_dir)/lib
mandir   = $(DATADIR)/man
man1dir  = $(mandir)/man1
man1ext  = .1
docdir   = $(DATADIR)/doc/$(PACKAGE)-$(VERSION)
INSTALL  = install $(INSTALL_OPT)

INSTALL_BIN = $(INSTALL) -m755 -t "$(DESTDIR)$(1)" "$(2)"
INSTALL_DOC = $(INSTALL) -m644 -t "$(DESTDIR)$(1)" "$(2)"
INSTALL_MAN = sed -e 's/@VERSION@/$(VERSION)/' "$(2)" \
	>"$(DESTDIR)$(1)/$(notdir $2)"

install-strip: INSTALL_OPT = --strip-program=$(STRIP) -s
install-strip: install
install: install-exec install-data
	@echo "$(PACKAGE) $(VERSION) should be installed"

install-exec: install-bin
install-data: install-man install-doc

install-bin: $(target)
	mkdir -p -- "$(DESTDIR)$(bindir)"
	$(call INSTALL_BIN,$(bindir),$^)

install-man: $(srcdir)/$(target)$(man1ext)
	mkdir -p -- "$(DESTDIR)$(man1dir)"
	$(call INSTALL_MAN,$(man1dir),$<)

install-doc: INSTALL_OPT = --mode=644
install-doc: $(topdir)/README.md
	mkdir -p -- "$(DESTDIR)$(docdir)"
	$(call INSTALL_DOC,$(docdir),$<)

.PHONY: install-strip install install-exec install-bin
.PHONY: install-data install-man install-doc

uninstall-doc: ; rm -rf -- "$(DESTDIR)$(docdir)/"
uninstall-man: ; rm -f -- "$(DESTDIR)$(man1dir)/$(target)$(man1ext)"
uninstall-bin: ; rm -f -- "$(DESTDIR)$(bindir)/$(target)"

uninstall-data: uninstall-man uninstall-doc
uninstall-exec: uninstall-bin
uninstall: uninstall-exec uninstall-data
	@echo "$(PACKAGE) $(VERSION) should be uninstalled"

.PHONY: uninstall uninstall-exec uninstall-data
.PHONY: uninstall-bin uninstall-man uninstall-doc
