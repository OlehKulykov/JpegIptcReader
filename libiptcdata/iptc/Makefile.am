SUBDIRS = po

INCLUDES =						\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/libiptcdata			\
	-DIPTC_LOCALEDIR=\""$(datadir)/locale"\"

bin_PROGRAMS = iptc

iptc_SOURCES =			\
	main.c			\
	i18n.h
iptc_LDADD =					\
	-L../libiptcdata -liptcdata		\
	$(LTLIBINTL) $(LTLIBICONV)
