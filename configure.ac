AC_INIT([NSRL Server], [1.4], [Robert J. Hansen <rjh@secret-alchemy.com>], [nsrlsvr], [http://rjhansen.github.com/nsrlsvr/])
AC_ARG_WITH([warning],
    [AS_HELP_STRING([--warning],
        [display a compilation hint @<:@default: yes@:>@])],
    [warning=${withval}], [warning=yes])
AC_PROG_SED
AM_PATH_PYTHON([2.7])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/main.cc])
AC_PREREQ([2.58])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([foreign])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AC_PROG_CXX
AC_TYPE_INT8_T
AC_TYPE_UINT8_T
AC_TYPE_INT16_T
AC_TYPE_UINT16_T
AC_TYPE_INT32_T
AC_TYPE_UINT32_T
AC_TYPE_INTMAX_T
ACX_PTHREAD([], AC_MSG_ERROR([pthreads does not appear usable.]))
AC_CHECK_FUNCS([inet_ntoa])
AC_CHECK_FUNCS([memset])
AC_CHECK_FUNCS([socket])
AC_CHECK_HEADERS([arpa/inet.h])
AC_CHECK_HEADERS([limits.h])
AC_CHECK_HEADERS([netinet/in.h])
AC_CHECK_HEADERS([sys/socket.h])
AC_CHECK_HEADERS([syslog.h])
AC_C_CONST
AC_FUNC_FORK
AC_HEADER_STDBOOL
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T  
AC_OUTPUT([Makefile src/Makefile man/Makefile])
if test "x$warning" == "xyes" ; then
    AC_MSG_NOTICE([


***
*** As of 1.4, nsrlsvr no longer automatically downloads the NSRL RDS.
*** You must do this yourself.  After installation, you must run
*** "nsrlupdate /path/to/rds" to populate the database before running
*** nsrlsvr.
***
]);
fi

