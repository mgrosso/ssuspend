#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.


AC_PREREQ([2.69])
AC_INIT([ssuspend], [0.1.0], [mgrosso@acm.org])

AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE([foreign])

AC_CONFIG_HEADERS([inc/system_config.h])

if test x$CFLAGS = x ; then
    CFLAGS="-g"
fi

# Checks for programs.
AC_USE_SYSTEM_EXTENSIONS
AC_PROG_CXX
AC_PROG_CC
#add something to do this later, its not an existing macro...
#AC_PROG_AR
AC_PROG_RANLIB
AC_PROG_INSTALL

# Checks for header files.
AC_CHECK_HEADERS([stdio.h stdlib.h sys/types.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_OFF_T
AC_TYPE_SIZE_T

# Checks for library functions.
#AC_FUNC_ERROR_AT_LINE
AC_FUNC_FORK
#AC_TYPE_SIGNAL
#AC_FUNC_STRTOD
#AC_CHECK_FUNCS([dup2 memmove memset strcasecmp strchr strerror strpbrk strstr strtol])
#AC_C_LONG_DOUBLE
#AC_CXX_HAVE_HASH_MAP
#AC_CXX_HAVE_EXT_HASH_MAP
#AC_CXX_HAVE_GNU_CXX_EXT_HASH_MAP


#DEFS="$DEFS -D_REENTRANT -D_GNU_SOURCE"
CFLAGS="$CFLAGS -Wall $DEFS"

echo target is $target
echo CFLAGS = $CFLAGS
echo DEFS = $DEFS

AC_CONFIG_FILES([Makefile
                 src/Makefile])
AC_OUTPUT
