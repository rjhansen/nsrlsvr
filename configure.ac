AC_INIT([NSRL Server], [1.1.4], [Robert J. Hansen <rjh@secret-alchemy.com>], [nsrlsvr], [http://rjhansen.github.com/nsrlsvr/])
AC_ARG_WITH([nsrl],
    [AS_HELP_STRING([--with-nsrl],
        [use NIST's NSRL RDS @<:@default: use the NSRL RDS@:>@])],
    [nsrl=${withval}], [nsrl=no])
    
AC_ARG_WITH([custom],
    [AS_HELP_STRING([--with-custom],
        [use a custom dataset @<:@default: don't@:>@])],
    [custom=${withval}], [custom=no])

AM_PATH_PYTHON([2.7])

if test "x$nsrl" != "xno" && test "x$custom" != "xno" ; then
    AC_MSG_ERROR([The --with-nsrl and --with-custom flags are mutually exclusive.]);
fi

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
dnl AC_FUNC_GETLOADAVG
AC_HEADER_STDBOOL
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

RDS_URL=http://www.nsrl.nist.gov/RDS/rds_2.39/RDS_239m.zip
nsrl_filename=RDS_239m.zip


if test "x$nsrl" != xno ; then
  if ! test -r $nsrl ; then
    AC_MSG_ERROR([Couldn't find the dataset specified.])
  else
    nsrl_filename = $nsrl
  fi
fi

if ! test -r $nsrl_filename && test "x$custom" = xno ; then
  AC_CHECK_PROG([UNZIP], [unzip], [unzip], AC_MSG_ERROR([unzip not found: this is necessary to use the downloaded NIST NSRL RDS]))
  AC_CHECK_PROG([WGET], [wget], [wget], [no])
  if test "x$WGET" = xwget ; then
    wget $RDS_URL ;
  else
    AC_CHECK_PROG([CURL], [curl], [curl], [no])
    if test "x$CURL" = xcurl ; then
      curl -O $RDS_URL ;
    else
      AC_MSG_ERROR([The NIST NSRL RDS must be downloaded, but neither curl nor wget are in your PATH.  Please fix this, and try again.])
    fi
  fi
  AC_MSG_NOTICE([
***
*** I'm going to leave the file $nsrl_filename around in the toplevel of the
*** build directory.  If you leave it here, the next time you build this it
*** will save you a long download.
***])
fi

if test "x$custom" = xno ; then
  if ! test -r $nsrl_filename ; then
    AC_MSG_ERROR([
***
*** Couldn't open $nsrl_filename for reading.
***
*** If you used a tilde ("~") in the path, try giving a full directory
*** path: sometimes tilde expansion confuses configure.
***]);
  else
    AC_MSG_NOTICE([uncompressing the NSRL RDS -- this may take a while...])
    rm -f NSRLFile.txt src/NSRLFile.txt
    unzip -o $nsrl_filename NSRLFile.txt
    AC_MSG_NOTICE([converting into nsrlsvr's data format -- please wait...])
    $PYTHON ./denistify.py
    rm -f NSRLFile.txt
  fi
else
  if ! test -r $custom ; then
    AC_MSG_ERROR([
***
*** Couldn't open $custom for reading.
***
*** If you used a tilde ("~") in the path, try giving a full directory
*** path: sometimes tilde expansion confuses configure.
***]);
  fi
  AC_MSG_NOTICE([converting $custom to the proper data format -- please wait...])
  rm -f src/NSRLFile.txt
  $PYTHON ./convert-format.py $custom
fi
  
AC_OUTPUT([Makefile src/Makefile man/Makefile])

