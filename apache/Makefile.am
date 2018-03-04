apache_moduledir = $(HTTPD_MODULE_DIR)
apache_module_PROGRAMS = mod_apachews.so
mod_apachews_so_SOURCES = src/mod_apachews.c

DEFS =

mod_apachews_so_CFLAGS =         \
  -I$(srcdir)/include	         \
  -I$(top_srcdir)/common/include \
    $(HTTPD_CFLAGS)

mod_apachews_so_LDFLAGS = -shared -Wl,-soname -Wl,mod_apache_ws.so
mod_apachews_so_LDADD = -lrt -lm  $(top_builddir)/common/libapache_ws_common.a

CHECK_TARGET = mod_apachews_so

clang-analyze: $(${CHECK_TARGET}_SOURCES:.c=.clang-analyze)
	@true

cppcheck: $(${CHECK_TARGET}_SOURCES:.c=.cppcheck)
	@true

.c.cppcheck:
	@cppcheck $(${CHECK_TARGET}_CFLAGS) $(CPPCHECK_OPTIONS) $(CPPCHECK_LIBRARIES) $<

.c.clang-analyze:
	@echo Analyzing $<
	@clang --analyze $(${CHECK_TARGET}_CFLAGS) $<
