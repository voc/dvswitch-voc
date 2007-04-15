prefix := /usr/local

bindir := $(prefix)/bin
sharedir := $(prefix)/share
docdir := $(sharedir)/doc
mandir := $(sharedir)/man

all :
	$(MAKE) -C src all

clean :
	$(MAKE) -C src clean
	$(MAKE) -C tests clean

install :
	$(MAKE) -C data install
	$(MAKE) -C src install
	mkdir -p -m 755 $(DESTDIR)$(docdir)/dvswitch
	gzip -c9 doc/README >$(DESTDIR)$(docdir)/dvswitch/README.gz
	gzip -c9 ChangeLog >$(DESTDIR)$(docdir)/dvswitch/ChangeLog.gz

.PHONY : all clean install
