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
PACKAGE_STR = "$(PACKAGE) $(VERSION)"
man1      := 1
ifeq ($(or $(VERSION),ERROR),ERROR)
$(error vcversion.sh failed)
endif
pkgconfig  = $(shell $(PKGCONFIG) $(PKGFLAGS) $(1) || echo n/a)

# ----------------------------------------------------------------------

.DELETE_ON_ERROR:

# ----------------------------------------------------------------------

vpath %.c $(srcdir)
vpath %.pod $(srcdir)

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
#  Configure libpng with pkg-config
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

target     = $(PACKAGE)
targetexe  = $(target)$(EXEEXT)
targetman  = $(target).$(man1)

override DEFS=\
-DPACKAGE_STRING='$(PACKAGE_STR)' \
-DPACKAGE_URL='"https://github.com/benjihan/pngtopi1"'

ifeq ($(or $D,0),0)
override DEFS += -DNDEBUG=1
CFLAGS = -Ofast
else
override DEFS += -DDEBUG=1
CFLAGS = -O0 -g
endif

override CPPFLAGS += $(DEFS)
override CFLAGS   += $(PNGCFLAGS)
override LDLIBS   += $(PNGLIBS)

# ----------------------------------------------------------------------
#  Rules
# ----------------------------------------------------------------------

all: $(targetexe)
man: $(targetman)
clean-all: clean clean-man
clean-man: ; -rm -f -- $(targetman)
clean: ; -rm -f -- $(targetexe)
.PHONY: all man clean-all clean-man clean

# GB: Add a rule in case an executable extension was defined. It's not
#     perfect as such a rule might already exist. This is mitigated by
#     using a double colon (::) rule that won't be called if a default
#     rule already exists.

ifneq (,$(EXEEXT))
ifndef LINK.c
LINK.c = $(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $(TARGET_ARCH)
endif
%$(EXEEXT) :: %.c
	$(LINK.c) $^ $(LOADLIBES) $(LDLIBS) -o $@
endif

# ----------------------------------------------------------------------
#  Manual
# ----------------------------------------------------------------------
POD2MAN = $(or $(pod2man),pod2man)

%.$(man1) : %.pod
	$(POD2MAN) -s$(man1) -c "User Commands" -r $(PACKAGE_STR) $< >$@

# ----------------------------------------------------------------------
#  Distribution
# ----------------------------------------------------------------------

dist_dir := $(PACKAGE)-$(VERSION)
dist_arc = $(dist_dir).tar.xz
dist_lst = LICENSE README.md vcversion.sh Makefile $(target).c $(targetman)

dist: distrib
distcheck: dist-check
distrib: dist-arc
dist-all: all distrib
dist-check: dist-make dist-sweep

$(dist_dir):
	@test ! -d $@ || { chmod -R u+w $@/ && rm -rf -- $@/; }
	mkdir -- $@

$(dist_arc): $(dist_dir)
	tar -cpC $(topdir) $(dist_lst) | tar -xpC $^
	sed -e 's#@VERSION@#$(VERSION)#' -i $^/$(targetman)
	echo $(VERSION) >$^/VERSION
	tar --owner=0 --group=0 -cJpf $(dist_arc) $^/
	rm -rf -- $^/

dist-arc: $(dist_arc)
	@echo '*** distrib file -- "$^" -- is ready'

dist-extract: $(dist_arc)
	@test ! -e $(dist_dir) || { echo $(dist_dir) already exist; false; }
	tar -xf $^
	@chmod -R ugo-w $(dist_dir)/
	@echo '*** extracted -- "$^"'

dist-make: dist-extract
	@rm -rf -- _build-$(dist_dir)
	@mkdir -- _build-$(dist_dir)
	@echo "compiling -- $(dist_dir)"
	$(MAKE) -sC _build-$(dist_dir) -f ../$(dist_dir)/Makefile dist-all

dist-sweep:
	@test ! -d $(dist_dir) \
	|| { chmod -R -- u+w $(dist_dir) && rm -rf -- $(dist_dir); }
	@test ! -d _build-$(dist_dir) \
	|| rm -rf -- _build-$(dist_dir)
	@echo "*** sweep distrib working directories"

.PHONY: dist dist-all dist-arc dist-dir dist-check dist-extract	\
        dist-make dist-sweep distcheck

# ----------------------------------------------------------------------
#  Install / Uninstall
# ----------------------------------------------------------------------

PREFIX = $(or $(abspath $(prefix)),$(realpath install-dir),\
 $(error 'prefix' must be set))
DATADIR  = $(or $(abspath $(datadir)),$(PREFIX)/share)

exec_dir = $(patsubst %/,%,$(abspath $(PREFIX)))
bindir   = $(exec_dir)/bin
libdir   = $(exec_dir)/lib
mandir   = $(DATADIR)/man
man1dir  = $(mandir)/man$(man1)
docdir   = $(DATADIR)/doc/$(PACKAGE)-$(VERSION)
INSTALL  = install $(INSTALL_OPT)

INSTALL_BIN = $(INSTALL) -m755 -t "$(DESTDIR)$(1)" "$(2)"
INSTALL_DOC = $(INSTALL) -m644 -t "$(DESTDIR)$(1)" "$(2)"
INSTALL_MAN = $(INSTALL_DOC)

install-strip: INSTALL_OPT = --strip-program=$(STRIP) -s
install-strip: install
install: install-exec install-data
	@echo "$(PACKAGE) $(VERSION) should be installed"

install-exec: install-bin
install-data: install-man install-doc

install-bin: $(targetexe)
	mkdir -p -- "$(DESTDIR)$(bindir)"
	$(call INSTALL_BIN,$(bindir),$^)

install-man: $(srcdir)/$(targetman)
	mkdir -p -- "$(DESTDIR)$(man1dir)"
	$(call INSTALL_MAN,$(man1dir),$<)

install-doc: INSTALL_OPT = --mode=644
install-doc: $(topdir)/README.md
	mkdir -p -- "$(DESTDIR)$(docdir)"
	$(call INSTALL_DOC,$(docdir),$<)

.PHONY: install-strip install install-exec install-bin
.PHONY: install-data install-man install-doc

uninstall-doc: ; rm -rf -- "$(DESTDIR)$(docdir)/"
uninstall-man: ; rm -f -- "$(DESTDIR)$(man1dir)/$(targetman)"
uninstall-bin: ; rm -f -- "$(DESTDIR)$(bindir)/$(target)"

uninstall-data: uninstall-man uninstall-doc
uninstall-exec: uninstall-bin
uninstall: uninstall-exec uninstall-data
	@echo "$(PACKAGE) $(VERSION) should be uninstalled"

.PHONY: uninstall uninstall-exec uninstall-data
.PHONY: uninstall-bin uninstall-man uninstall-doc
