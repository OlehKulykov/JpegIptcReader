EXTRA_DIST = README examples/set_iptc.py examples/show_iptc.py

pyexec_LTLIBRARIES = iptcdata.la

iptcdata_la_SOURCES = pyiptcdatamod.c pyiptcdata.c pyiptcdataset.c pyiptcdata.h
iptcdata_la_LIBADD = ../libiptcdata/libiptcdata.la
iptcdata_la_LDFLAGS = -module -avoid-version
iptcdata_la_CPPFLAGS = $(PYTHON_CPPFLAGS) -I$(top_srcdir) -fno-strict-aliasing -DNDEBUG -g -O2 -Wall -Wstrict-prototypes
