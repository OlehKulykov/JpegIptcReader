if PYTHONLIB
MAYBE_PYTHONLIB = python
endif

SUBDIRS = m4 libiptcdata po iptc docs win $(MAYBE_PYTHONLIB)

EXTRA_DIST = @PACKAGE@.spec

ACLOCAL_AMFLAGS = -I m4
DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc
