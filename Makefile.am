ACLOCAL_AMFLAGS = -I m4

pkgconfdir = $(libdir)/pkgconfig

pkgconf_DATA = libmwaw-@MWAW_MAJOR_VERSION@.@MWAW_MINOR_VERSION@.pc

SUBDIRS = src build docs inc

EXTRA_DIST = \
	autogen.sh \
	libmwaw.spec \
	libmwaw.spec.in \
	libmwaw.pc.in \
	libmwaw-zip.in \
	CHANGES \
	COPYING.LGPL \
	COPYING.MPL \
	CREDITS \
	HACKING \
	INSTALL \
	README

distclean-local:
	rm -rf *.cache *~ *.pc

rpm: dist
	rpmbuild -ta $(PACKAGE)-$(VERSION).tar.gz
	@rm -f $(PACKAGE)-$(VERSION).tar.gz

zip: install
	sh libmwaw-zip

