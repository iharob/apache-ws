AC_INIT([apache-ws], [0.0.0], [iharob@gmail.com])
AM_INIT_AUTOMAKE([subdir-objects foreign])

AC_PROG_CC
AC_PROG_RANLIB

AM_EXTRA_RECURSIVE_TARGETS([cppcheck])
AM_EXTRA_RECURSIVE_TARGETS([clang-analyze])

AC_CHECK_PROG([APXS], [apxs], [apxs], [no])
AS_IF([test "x$APXS" == "xno"],
[
    AC_MSG_FAILURE([
        No puedo encontrar 'apxs' necesario para construir
        el módulo.
    ])
])

AC_CHECK_PROG([PYTHON_CONFIG], [python3-config], [python3-config], [no])
AS_IF([test "x$PYTHON_CONFIG" == "xno"],
[
    AC_MSG_FAILURE([
        No puedo encontrar 'python3-config' necesario para construir
        el módulo de python.
    ])
])

AC_CHECK_PROG([PYTHON], [python3], [python3], [no])
AS_IF([test "x$PYTHON" == "xno"],
[
    AC_MSG_FAILURE([
        No puedo encontrar 'python3' necesario para construir
        el módulo de python.
    ])
])

AC_SUBST([HTTPD_MODULE_DIR], [`$APXS -q exp_libexecdir`])
AC_SUBST([HTTPD_INCLUDES_VARIABLE], [`$APXS -q EXTRA_INCLUDES`])
AC_SUBST([HTTPD_CFLAGS_VARIABLE], [`$APXS -q CFLAGS`])

AC_SUBST([PYTHON_CFLAGS], [`$PYTHON_CONFIG --cflags`])
AC_SUBST([PYTHON_LIBS], [`$PYTHON_CONFIG --libs`])
AC_SUBST([PYTHON_SITE_PACKAGES], [`$PYTHON -c "from distutils.sysconfig import get_python_lib; print(get_python_lib())"`])

AC_ARG_ENABLE(
    [debug],
    AS_HELP_STRING([--enable-debug], [Habilitar depuración en los binarios]),
    [
        AC_SUBST([CFLAGS], ["-D_DEBUG -O0 -g3 -Wall -Werror -std=gnu99 -fPIC"])
        AC_SUBST([HTTPD_CFLAGS], ["$HTTPD_INCLUDES_VARIABLE"])
        AC_SUBST([LDFLAGS], ["-g3 -O0"])
    ],
    [
        AC_SUBST([CFLAGS], ["-O2 -std=gnu99 -fPIC"])
        AC_SUBST([HTTPD_CFLAGS], ["$HTTPD_INCLUDES_VARIABLE $HTTPD_CFLAGS_VARIABLE"])
        AC_SUBST([LDFLAGS], ["-O2 -std=gnu99"])
    ]
)
AC_SUBST([ARFLAGS], ["crD"])

AC_CHECK_PROG([PHP_CONFIG], [php-config], [php-config], [no])
AS_IF([test "x$PHP_CONFIG" == "xno"],
    AC_MSG_FAILURE([
        No pude encontrar 'php-config' necesario
        para construir el módulo php
    ])
)

AC_SUBST([CPPCHECK_OPTIONS], ["-D_DEBUG --template='{file}:{line}: {severity}: {message}' --suppress=varFuncNullUB --check-library --language=c --enable=warning,style,performance"])
result="";
for x in `dirname $0`/quality/*.cfg;
do
    top_srcdir='$(top_srcdir)'
    file=`basename $x`
    result="$result --library=$top_srcdir/quality/$x";
done;
AC_SUBST([CPPCHECK_LIBRARIES], [$result])

AC_SUBST([PHP_CFLAGS], [`$PHP_CONFIG --includes`])
AC_SUBST([PHP_LIBS], [`$PHP_CONFIG --libs`])
AC_SUBST([PHP_EXT_DIR], [`$PHP_CONFIG --extension-dir`])

AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([common/Makefile])
AC_CONFIG_FILES([apache/Makefile])
AC_CONFIG_FILES([php/Makefile])
AC_CONFIG_FILES([python/Makefile])
AC_OUTPUT
