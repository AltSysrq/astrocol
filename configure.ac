#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.6])
AC_INIT([astrocol], [0.0.1], [jason@lin.gl])
AM_INIT_AUTOMAKE
AC_CONFIG_MACRO_DIR([m4])
LT_PREREQ([1.5])
LT_INIT([dlopen])
AC_CONFIG_SRCDIR([src/astrocol.c])
AC_CONFIG_HEADERS([config.h])
AM_SILENT_RULES(yes)
AM_MAINTAINER_MODE

# Checks for programs.
AC_PROG_CC

# Modifying these variables directly is typically considered a bad thing.
# But for whatever reason, Autotools doesn't bother adding the /usr/local paths
# to the search directories on systems (eg, FreeBSD) where *everything* lives
# there, so we'll do it ourselves here, making sure to preserve the flags
# already given.
CPPFLAGS="$CPPFLAGS -I/usr/local/include"
LDFLAGS="$LDFLAGS -L/usr/local/lib"

# Checks for libraries.
AC_SEARCH_LIBS([yaml_parser_initialize], [yaml], [],
               [AC_MSG_ERROR(
[The YAML1.1 library could not be found. Make sure that libyaml[-dev] is installed.])])

# Checks for header files.
AC_CHECK_HEADERS([yaml.h],
                 [],
                 [AC_MSG_ERROR([A required header could not be found.])])
AC_CHECK_HEADERS([sysexits.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_C_CONST
AC_TYPE_SIZE_T

AC_CHECK_FUNCS([memmove memset strerror],
  [],
  AC_MSG_ERROR([A required function could not be found.]))

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
