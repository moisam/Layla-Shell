# 
#    Copyright 2019 (c)
#    Mohammed Isam Mohammed [mohammed_isam1984@yahoo.com]
# 
#    file: Makefile
#    This file is part of the Layla shell project.
#
#    Layla shell is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    Layla shell is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with Layla shell.  If not, see <http://www.gnu.org/licenses/>.
#    


# output file name
TARGET=lsh

# version (for the distro target)
PKG_VERSION=1.1-2

# directory definitions
SRCDIR=src
BUILTINS_SRCDIR=$(SRCDIR)/builtins
SYMTAB_SRCDIR=$(SRCDIR)/symtab
BUILD_DIR=build
DIR_PREFIX=/usr/local
EXEC_PREFIX=/usr
BINDIR=$(EXEC_PREFIX)/bin
DATA_ROOTDIR=$(DIR_PREFIX)/share
DATADIR=$(DATA_ROOTDIR)
DOCDIR=$(DATA_ROOTDIR)/doc/$(TARGET)
INFODIR=$(DATA_ROOTDIR)/info
MANDIR=$(DATA_ROOTDIR)/man

# compiler name and flags
CC=gcc
LIBS=-lrt
CFLAGS=-Wall -Wextra -g -I$(SRCDIR)
LDFLAGS=-g

# generate the lists of source and object files
SRCS_BUILTINS=$(shell find $(SRCDIR)/builtins -name "*.c")

# the following line will compile the hashtable implementation of the symbol
# table struct.. if you want to use the linked list implementation instead,
# replace symtab_hash.c with symtab.c in the SRCS_SYMTAB line below (also,
# don't forget to comment the following line in the header file symbtab/symtab.h:
#    #define USE_HASH_TABLES 1
SRCS_SYMTAB=$(SRCDIR)/symtab/symtab_hash.c

SRCS =$(wildcard $(SRCDIR)/*.c)
SRCS+=$(shell find $(SRCDIR)/backend -name "*.c")
SRCS+=$(shell find $(SRCDIR)/parser -name "*.c")
SRCS+=$(SRCDIR)/symtab/string_hash.c                                 \
      $(SRCDIR)/scanner/lexical.c $(SRCDIR)/scanner/source.c         \
      $(SRCDIR)/error/error.c                                        \
      $(SRCS_BUILTINS) $(SRCS_SYMTAB)

OBJS=$(SRCS:%.c=$(BUILD_DIR)/%.o)

# default target (when we call make with no arguments)
.PHONY: all
all: prep-build $(TARGET)

prep-build:
	mkdir -p $(BUILD_DIR)/src/builtins
	mkdir -p $(BUILD_DIR)/src/backend
	mkdir -p $(BUILD_DIR)/src/error
	mkdir -p $(BUILD_DIR)/src/parser
	mkdir -p $(BUILD_DIR)/src/scanner
	mkdir -p $(BUILD_DIR)/src/symtab

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

# target to auto-generate header file dependencies for source files
depend: .depend

.depend: $(SRCS)
	$(RM) ./.depend
	$(CC) $(CFLAGS) -MM $^ > ./.depend;

include .depend

#compile C source files
$(BUILD_DIR)/%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Make sure all installation directories (e.g. $(bindir))
# actually exist by making them if necessary.
installdirs:
	-mkdir -p $(DESTDIR)$(DATADIR) $(DESTDIR)$(INFODIR) \
	$(DESTDIR)$(MANDIR) $(DESTDIR)$(BINDIR)

# compile info and dvi files from the texi source
info: $(TARGET).info

$(TARGET).texi:

$(TARGET).info: $(TARGET).texi
	@echo Creating info file from texi source
	$(MAKEINFO) docs/info/$(TARGET).texi
	@mv $(TARGET).info docs/info/

dvi: $(TARGET).dvi

$(TARGET).dvi: $(TARGET).texi
	@echo Creating dvi file from texi source
	$(TEXI2DVI) docs/info/$(TARGET).texi
	@mv $(TARGET).dvi docs/info/

# install info file system-wide
install-info: $(TARGET).info installdirs
	$(INSTALLDATA) docs/info/$(TARGET).info* $(INFODIR)

$(DESTDIR)$(INFODIR)/$(TARGET).info: $(TARGET).info
	$(POST_INSTALL)
# There may be a newer info file in . than in srcdir.
	-if test -f $(TARGET).info; then d=.; \
	else d=info; fi; \
	$(INSTALL_DATA) $$d/$(TARGET).info $(DESTDIR)$@; \
# Run install-info only if it exists.
# Use `if' instead of just prepending `-' to the
# line so we notice real errors from install-info.
# We use `$(SHELL) -c' because some shells do not
# fail gracefully when there is an unknown command.
	if $(SHELL) -c 'install-info --version' \
	>/dev/null 2>&1; then \
		install-info --dir-file=$(DESTDIR)$(INFODIR)/dir \
		$(DESTDIR)$(INFODIR)/$(TARGET).info; \
	else true; fi

# compile manpage. nothing to do really
man:
	@echo Nothing to do.

# install manpage system-wide
install-man:
	@echo Installing $(TARGET) manual to $(MANDIR)
	@mkdir -p $(MANDIR)/man1
	@gzip -k docs/man/man1/$(TARGET).1
	@mv docs/man/man1/$(TARGET).1.gz $(MANDIR)/man1/
	@chmod 644 $(MANDIR)/man1/$(TARGET).1.gz

# uninstall manpage system-wide
uninstall-man:
	@echo Removing $(TARGET) manual from $(MANDIR)
	-$(RM) $(MANDIR)/man1/$(TARGET).1.gz

# install extra documentation files system-wide
install-doc:
	@echo Installing $(TARGET) documentation to $(DOCDIR)
	@mkdir -p $(DOCDIR)
	@cp AUTHORS $(DOCDIR)
	@cp COPYING $(DOCDIR)
	@cp NEWS $(DOCDIR)
	@cp README $(DOCDIR)
	@cp THANKS $(DOCDIR)

# uninstall extra documentation files
uninstall-doc:
	@echo Removing $(TARGET) documentation from $(DOCDIR)
	-$(RM) $(DOCDIR)/AUTHORS
	-$(RM) $(DOCDIR)/COPYING
	-$(RM) $(DOCDIR)/NEWS
	-$(RM) $(DOCDIR)/README
	-$(RM) $(DOCDIR)/THANKS
	-@rmdir $(DOCDIR)

# install the package
install-strip: install

install: install-man install-doc
	@echo Installing $(TARGET)
	@cp $(TARGET) $(BINDIR)/$(TARGET)
	@chmod a+rx $(BINDIR)/$(TARGET)
	@echo "*******************************************"
	@echo "$(TARGET) installed as $(BINDIR)/$(TARGET)"
	@echo "To run now, invoke: $(TARGET)"
	@echo "*******************************************"

# uninstall the package
uninstall: uninstall-man uninstall-doc
	@echo Uninstalling $(TARGET)
	-$(RM) $(BINDIR)/$(TARGET)

# prepare a distor version of this package
dist: info
	-@mkdir -p $(TARGET)-$(PKG_VERSION)
	-@mkdir -p $(TARGET)-$(PKG_VERSION)/docs
	-@cd $(TARGET)-$(PKG_VERSION)
	-@cp Makefile ChangeLog $(TARGET)-$(PKG_VERSION)/
	-@cp AUTHORS COPYING NEWS README THANKS $(TARGET)-$(PKG_VERSION)/docs/
	-@cp -r src $(TARGET)-$(PKG_VERSION)/
	-@cp -r docs/info $(TARGET)-$(PKG_VERSION)/docs/
	-@cp -r docs/man $(TARGET)-$(PKG_VERSION)/docs/
	@cd ..
	-$(RM) $(TARGET)-$(PKG_VERSION).tar.gz
	@tar -czf $(TARGET)-$(PKG_VERSION).tar.gz $(TARGET)-$(PKG_VERSION)/
	$(RM) -r $(TARGET)-$(PKG_VERSION)

# targets to clean build files and shell executable
.PHONY: clean
clean:
	$(RM) $(OBJS) $(TARGET) core .depend
	$(RM) -r $(BUILD_DIR)

mostlyclean: clean

maintainer-clean: clean

distclean: clean
	$(info Cleaning build tree)
	$(RM) $(TARGET)-$(PKG_VERSION).tar

check:

TAGS:

.PHONY: all
