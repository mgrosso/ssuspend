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

# Absolute paths for the helpers ssuspend runs as root.  A setuid binary must
# not resolve these through the caller's $PATH.  Each is autodetected, can be
# overridden with --with-<prog>=/path, and falls back to a compiled-in default
# with only a warning if it is not found -- the build host does not have to
# have every tool installed, and src/ssuspend.c carries the same defaults
# behind #ifndef so it still compiles with no config header at all.
#
# mkswap normally lives in /sbin, which is often absent from a non-root $PATH,
# so widen the search for it rather than relying on the caller's environment.

AC_PATH_PROG([ssuspend_grep],[grep],[])
AC_ARG_WITH([grep],
    [AS_HELP_STRING([--with-grep=PATH],
        [absolute path to grep @<:@autodetected@:>@])],
    [ssuspend_grep="$withval"])
if test "x$ssuspend_grep" = x ; then
    ssuspend_grep="/bin/grep"
    AC_MSG_WARN([grep not found, defaulting to $ssuspend_grep])
fi
AC_DEFINE_UNQUOTED([GREP_PATH],["$ssuspend_grep"],[absolute path to grep])

AC_PATH_PROG([ssuspend_dd],[dd],[])
AC_ARG_WITH([dd],
    [AS_HELP_STRING([--with-dd=PATH],
        [absolute path to dd @<:@autodetected@:>@])],
    [ssuspend_dd="$withval"])
if test "x$ssuspend_dd" = x ; then
    ssuspend_dd="/bin/dd"
    AC_MSG_WARN([dd not found, defaulting to $ssuspend_dd])
fi
AC_DEFINE_UNQUOTED([DD_PATH],["$ssuspend_dd"],[absolute path to dd])

AC_PATH_PROG([ssuspend_mkswap],[mkswap],[],
    [$PATH$PATH_SEPARATOR/sbin$PATH_SEPARATOR/usr/sbin])
AC_ARG_WITH([mkswap],
    [AS_HELP_STRING([--with-mkswap=PATH],
        [absolute path to mkswap @<:@autodetected@:>@])],
    [ssuspend_mkswap="$withval"])
if test "x$ssuspend_mkswap" = x ; then
    ssuspend_mkswap="/sbin/mkswap"
    AC_MSG_WARN([mkswap not found, defaulting to $ssuspend_mkswap])
fi
AC_DEFINE_UNQUOTED([MKSWAP_PATH],["$ssuspend_mkswap"],[absolute path to mkswap])

AC_PATH_PROG([ssuspend_rmmod],[rmmod],[],
    [$PATH$PATH_SEPARATOR/sbin$PATH_SEPARATOR/usr/sbin])
AC_ARG_WITH([rmmod],
    [AS_HELP_STRING([--with-rmmod=PATH],
        [absolute path to rmmod @<:@autodetected@:>@])],
    [ssuspend_rmmod="$withval"])
if test "x$ssuspend_rmmod" = x ; then
    ssuspend_rmmod="/sbin/rmmod"
    AC_MSG_WARN([rmmod not found, defaulting to $ssuspend_rmmod])
fi
AC_DEFINE_UNQUOTED([RMMOD_PATH],["$ssuspend_rmmod"],[absolute path to rmmod])

AC_PATH_PROG([ssuspend_modprobe],[modprobe],[],
    [$PATH$PATH_SEPARATOR/sbin$PATH_SEPARATOR/usr/sbin])
AC_ARG_WITH([modprobe],
    [AS_HELP_STRING([--with-modprobe=PATH],
        [absolute path to modprobe @<:@autodetected@:>@])],
    [ssuspend_modprobe="$withval"])
if test "x$ssuspend_modprobe" = x ; then
    ssuspend_modprobe="/sbin/modprobe"
    AC_MSG_WARN([modprobe not found, defaulting to $ssuspend_modprobe])
fi
AC_DEFINE_UNQUOTED([MODPROBE_PATH],["$ssuspend_modprobe"],
    [absolute path to modprobe])

# The before/after hooks are configuration, not installed programs, so they are
# not autodetected.  Note these deliberately default to /etc and are NOT derived
# from $sysconfdir, which would become $prefix/etc and silently relocate them.

AC_ARG_WITH([before-script],
    [AS_HELP_STRING([--with-before-script=PATH],
        [script run before suspending @<:@/etc/ssuspend/before@:>@])],
    [ssuspend_before="$withval"],
    [ssuspend_before="/etc/ssuspend/before"])
AC_DEFINE_UNQUOTED([PRESHELL_PATH],["$ssuspend_before"],
    [script run after locking but before suspending])

AC_ARG_WITH([after-script],
    [AS_HELP_STRING([--with-after-script=PATH],
        [script run after resuming @<:@/etc/ssuspend/after@:>@])],
    [ssuspend_after="$withval"],
    [ssuspend_after="/etc/ssuspend/after"])
AC_DEFINE_UNQUOTED([POSTSHELL_PATH],["$ssuspend_after"],
    [script run after resuming but before releasing the lock])

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
