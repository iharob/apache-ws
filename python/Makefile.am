DEFS=

pydir = $(PYTHON_SITE_PACKAGES)
py_PROGRAMS = apachews.so
apachews_so_SOURCES = src/py-apache-ws.c
apachews_so_CFLAGS =               \
      $(PYTHON_CFLAGS)             \
    -I$(top_srcdir)/common/include \
    -I$(srcdir)/include
apachews_so_LDFLAGS = -shared
apachews_so_LDADD =                              \
    $(PYTHON_LIBS)                               \
    $(top_builddir)/common/libapache_ws_common.a \
    $(LDFLAGS)
CHECK_TARGET=apachews_so
clang-analyze: $(${CHECK_TARGET}_SOURCES:.c=.clang-analyze)
	@true
cppcheck: $(${CHECK_TARGET}_SOURCES:.c=.cppcheck)
	@true
.c.cppcheck:
	@cppcheck $(${CHECK_TARGET}_CFLAGS) $(CPPCHECK_OPTIONS) $(CPPCHECK_LIBRARIES) $<
.c.clang-analyze:
	@echo Analyzing $<
	@clang --analyze $(${CHECK_TARGET}_CFLAGS) $<
