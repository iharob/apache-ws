DEFS=

phpdir = $(PHP_EXT_DIR)
php_PROGRAMS = php_apachews.so
php_apachews_so_SOURCES = src/php-apachews.c
php_apachews_so_CFLAGS = \
      $(PHP_CFLAGS)	 \
    -I$(srcdir)/include	 \
    -I$(top_srcdir)/common/include
php_apachews_so_LDFLAGS = -shared
php_apachews_so_LDADD =			         \
    $(PHP_LIBS)				         \
    $(top_builddir)/common/libapache_ws_common.a \
    $(LDFLAGS)

CHECK_TARGET=php_apachews_so
clang-analyze: $(${CHECK_TARGET}_SOURCES:.c=.clang-analyze)
	@true
cppcheck: $(${CHECK_TARGET}_SOURCES:.c=.cppcheck)
	@true
.c.cppcheck:
	@cppcheck $(${CHECK_TARGET}_CFLAGS) $(CPPCHECK_OPTIONS) $(CPPCHECK_LIBRARIES) $<
.c.clang-analyze:
	@echo Analyzing $<
	@clang --analyze $(${CHECK_TARGET}_CFLAGS) $<
