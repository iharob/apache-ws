noinst_LIBRARIES = libapache_ws_common.a
libapache_ws_common_a_SOURCES = src/apache-ws-common.c
libapache_ws_common_a_CFLAGS = -I$(top_srcdir)/common/include

CHECK_TARGET=libapache_ws_common
clang-analyze: $(${CHECK_TARGET}_SOURCES:.c=.clang-analyze)
	@true
cppcheck: $(${CHECK_TARGET}_SOURCES:.c=.cppcheck)
	@true
.c.cppcheck:
	@cppcheck $(${CHECK_TARGET}_CFLAGS) $(CPPCHECK_OPTIONS) $(CPPCHECK_LIBRARIES) $<
.c.clang-analyze:
	@echo Analyzing $<
	@clang --analyze $(${CHECK_TARGET}_CFLAGS) $<
