# 
#    Copyright 2019 (c)
#    Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
# 
#    file: Makefilee
#    This file is part of the Layla Shell project.
#
#    Layla Shell is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    Layla Shell is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with Layla Shell.  If not, see <http://www.gnu.org/licenses/>.
#    

#Some basic definitions
pkgname=lsh
pkgversion=1.0
prefix=/usr/local
exec_prefix=/usr
bindir=$(exec_prefix)/bin
datarootdir=$(prefix)/share
datadir=$(datarootdir)
docdir=$(datarootdir)/doc/$(pkgname)
infodir=$(datarootdir)/info
mandir=$(datarootdir)/man

TARGET=$(pkgname)
SRCDIR=src
BUILDDIR=build
CC=gcc
# librt is needed for timer_create() and timer_settime()
CFLAGS=-Wall -lrt -g
SRCFILES=main.c braceexp.c functab.c strbuf.c popen.c cmd_args.c cmdline.c helpfunc.c initsh.c jobs.c prompt.c args.c params.c signames.c tab.c kbdevent2.c early_environ.h cmd.h cpu.h ostype.h signames.h kbdevent.h vars.c vi.c vi.h vi_keys.c scanner/lexical.c scanner/source.c scanner/scanner.h scanner/source.h scanner/keywords.h parser/node.c parser/parser.c parser/node.h backend/shunt.c backend/backend.c backend/backend.h backend/pattern.c backend/redirect.c symtab/symtab_hash.c symtab/symtab_hash.h symtab/symtab.h symtab/string_hash.c symtab/string_hash.h error/error.c error/error.h builtins/builtins.c builtins/help.c builtins/history.c builtins/alias.c builtins/bg.c builtins/cd.c builtins/command.c builtins/echo.c builtins/false.c builtins/fc.c builtins/fg.c builtins/getopts.c builtins/hash.c builtins/kill.c builtins/newgrp.c builtins/pwd.c builtins/read.c builtins/type.c builtins/true.c builtins/ulimit.c builtins/umask.c builtins/unalias.c builtins/wait.c builtins/colon.c builtins/dot.c builtins/disown.c builtins/eval.c builtins/exec.c builtins/exit.c builtins/export.c builtins/mailcheck.c builtins/readonly.c builtins/return.c builtins/set.c builtins/shift.c builtins/source.c builtins/time.c builtins/times.c builtins/trap.c builtins/test.c builtins/unset.c builtins/ver.c builtins/dump.c builtins/let.c builtins/whence.c builtins/setx.c builtins/setx.h builtins/coproc.c builtins/local.c builtins/caller.c builtins/declare.c builtins/enable.c builtins/logout.c builtins/memusage.c builtins/dirstack.c builtins/suspend.c builtins/hist_expand.c builtins/bugreport.c builtins/nice.c builtins/hup.c builtins/notify.c builtins/glob.c builtins/printenv.c builtins/repeat.c builtins/setenv.c builtins/stop.c builtins/unlimit.c builtins/unsetenv.c comptype.h debug.c debug.h

.SUFFIXES:

# 
# Targets:
# 
all: stable info

# Make sure all installation directories (e.g. $(bindir))
# actually exist by making them if necessary.
installdirs:
	-mkdir -p $(DESTDIR)$(datadir) $(DESTDIR)$(infodir) \
	$(DESTDIR)$(mandir) $(DESTDIR)$(bindir) $(DESTDIR)$(javadir)

build/setup:
	$(info Preparing build tree)
	-mkdir -p $(BUILDDIR)
	@cp -R $(SRCDIR) $(BUILDDIR)

stable:clean build/setup
	@echo -> Creating stable version executable
ifeq ($(CC),gcc)
	@cd $(BUILDDIR)/$(SRCDIR) && $(CC) $(CFLAGS) $(SRCFILES) -o $(TARGET); [ $$? -eq 0 ] || exit 2
else
# Clang/LLMV will produce an a.out file
	@cd $(BUILDDIR)/$(SRCDIR) && $(CC) $(CFLAGS) $(SRCFILES); [ $$? -eq 0 ] || exit 2
	mv $(BUILDDIR)/$(SRCDIR)/a.out $(BUILDDIR)/$(SRCDIR)/$(TARGET)
endif
	mv $(BUILDDIR)/$(SRCDIR)/$(TARGET) $(BUILDDIR)/
	rm -rf $(BUILDDIR)/$(SRCDIR)
	@echo "*****************************************************"
	@echo "Layla shell executable successfully created."
	@echo ""
	@echo "To start the shell, invoke:"
	@echo "  build/$(TARGET)"
	@echo ""
	@echo "If you want to install the shell on your system, run:"
	@echo "  sudo make install"
	@echo "*****************************************************"

info: $(pkgname).info

$(pkgname).texi:

$(pkgname).info: $(pkgname).texi
	@echo Creating info file from texi source
	$(MAKEINFO) docs/info/$(pkgname).texi
	@mv $(pkgname).info docs/info/

dvi: $(pkgname).dvi

$(pkgname).dvi: $(pkgname).texi
	@echo Creating dvi file from texi source
	$(TEXI2DVI) docs/info/$(pkgname).texi

install-info: $(pkgname).info installdirs
	$(INSTALLDATA) docs/info/$(pkgname).info* $(infodir)

$(DESTDIR)$(infodir)/$(pkgname).info: $(pkgname).info
	$(POST_INSTALL)
# There may be a newer info file in . than in srcdir.
	-if test -f $(pkgname).info; then d=.; \
	else d=info; fi; \
	$(INSTALL_DATA) $$d/$(pkgname).info $(DESTDIR)$@; \
# Run install-info only if it exists.
# Use `if' instead of just prepending `-' to the
# line so we notice real errors from install-info.
# We use `$(SHELL) -c' because some shells do not
# fail gracefully when there is an unknown command.
	if $(SHELL) -c 'install-info --version' \
	>/dev/null 2>&1; then \
		install-info --dir-file=$(DESTDIR)$(infodir)/dir \
		$(DESTDIR)$(infodir)/$(pkgname).info; \
	else true; fi

man:
	@echo Nothing to do.

install-man:
	@echo Installing $(pkgname) manual to $(mandir)
	@mkdir -p $(mandir)/man1
	@gzip -k docs/man/man1/$(pkgname).1
	@mv docs/man/man1/$(pkgname).1.gz $(mandir)/man1/
	@chmod 644 $(mandir)/man1/$(pkgname).1.gz

uninstall-man:
	@echo Removing $(pkgname) manual from $(mandir)
	-$(RM) $(mandir)/man1/$(pkgname).1.gz

install-doc:
	@echo Installing $(pkgname) documentation to $(docdir)
	@mkdir -p $(docdir)
	@cp AUTHORS $(docdir)
	@cp COPYING $(docdir)
	@cp NEWS $(docdir)
	@cp README $(docdir)
	@cp THANKS $(docdir)

uninstall-doc:
	@echo Removing $(pkgname) documentation from $(docdir)
	-$(RM) $(docdir)/AUTHORS
	-$(RM) $(docdir)/COPYING
	-$(RM) $(docdir)/NEWS
	-$(RM) $(docdir)/README
	-$(RM) $(docdir)/THANKS
	-@rmdir $(docdir)

install-strip: install

install: install-man install-doc
	@echo Installing $(pkgname)
	@cp $(BUILDDIR)/$(TARGET) $(bindir)/$(TARGET)
	@chmod a+rx $(bindir)/$(TARGET)
# @echo "Removing build directory"
# @rm -rf $(BUILDDIR)
	@echo "*******************************************"
	@echo "$(pkgname) installed as $(bindir)/$(TARGET)"
	@echo "To run now, invoke: $(TARGET)"
	@echo "*******************************************"

uninstall: uninstall-man uninstall-doc
	@echo Uninstalling $(pkgname)
	-$(RM) $(bindir)/$(TARGET)

dist: info
	-@mkdir -p $(pkgname)-$(pkgversion)
	-@mkdir -p $(pkgname)-$(pkgversion)/docs
	-@cd $(pkgname)-$(pkgversion)
	-@cp Makefile ChangeLog $(pkgname)-$(pkgversion)/
	-@cp AUTHORS COPYING NEWS README THANKS $(pkgname)-$(pkgversion)/docs/
	-@cp -r src $(pkgname)-$(pkgversion)/
	-@cp -r docs/info $(pkgname)-$(pkgversion)/docs/
	-@cp -r docs/man $(pkgname)-$(pkgversion)/docs/
	@cd ..
	-$(RM) $(pkgname)-$(pkgversion).tar.gz
	@tar -czf $(pkgname)-$(pkgversion).tar.gz $(pkgname)-$(pkgversion)/
	$(RM) -r $(pkgname)-$(pkgversion)

clean:
	$(RM) -r build

mostlyclean: clean

maintainer-clean: clean

distclean: clean
	$(info Cleaning build tree)
	$(RM) $(pkgname)-$(pkgversion).tar

check:

TAGS:

.PHONY: all
