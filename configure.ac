AC_INIT([logcollector], [1.0], [])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_PROG_CC
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
 Makefile
 src/Makefile
])

PKG_CHECK_MODULES([PCRE], [libpcre >= 8.41], [
	_save_LIBS="$LIBS"
	LIBS="$LIBS $PCRE_LIBS"
	AC_CHECK_LIB([pcre], [pcre_compile])
	LIBS="$_save_LIBS"
	_save_CFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS $PCRE_CFLAGS"
	AC_CHECK_HEADER([pcre.h])
	CFLAGS="$_save_CFLAGS"
	echo "PCRE_LIBS=$PCRE_LIBS PCRE_CFLAGS=$PCRE_CFLAGS"
	], [
	AC_MSG_ERROR([Missing pcre. Refer to: https://www.pcre.org/])
])

AC_OUTPUT
