## Process this file with automake to produce Makefile.in

# The name of the module, e.g. 'glib'.
DOC_MODULE=libiptcdata

# The top-level SGML file. Change it if you want.
DOC_MAIN_SGML_FILE=$(DOC_MODULE)-docs.sgml

# The directory containing the source code. Relative to $(srcdir).
# gtk-doc will search all .c & .h files beneath here for inline comments
# documenting functions and macros.
DOC_SOURCE_DIR=../../libiptcdata

# Extra options to pass to gtkdoc-scanobj or gtkdoc-scangobj.
SCANOBJ_OPTIONS=

# Extra options to supply to gtkdoc-scan.
SCAN_OPTIONS=

# Extra options to supply to gtkdoc-mkdb.
MKDB_OPTIONS= --sgml-mode --output-format=xml

# Extra options to supply to gtkdoc-fixref.
FIXXREF_OPTIONS=

# Used for dependencies.
HFILE_GLOB=
CFILE_GLOB=

# Header files to ignore when scanning.
IGNORE_HFILES= i18n.h

# Images to copy into HTML directory.
HTML_IMAGES =

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE).
content_files = \
	version.xml \
	about.sgml \
	commandline.sgml \
	i18n.sgml \
	libjpeg.sgml \
	overview.sgml

include $(top_srcdir)/gtk-doc.make

EXTRA_DIST += \
	version.xml.in
