AUTOMAKE_OPTIONS = foreign

lib_LTLIBRARIES = libspindle.la

libspindle_la_SOURCES = spindle.c

libspindle_la_LIBADD = @LTLIBOBJS@
libspindle_la_LDFLAGS = -release @VERSION@

include_HEADERS = spindle.h spindle_version.h
EXTRA_DIST = spindle.h spindle_version.h
noinst_HEADERS = spindle_config.h spindle_internal.h
