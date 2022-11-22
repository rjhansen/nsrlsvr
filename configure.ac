AC_PREREQ([2.69])
AC_INIT([nsrlsvr], [2.0], [https://github.com/rjhansen/nsrlsvr/issues], [nsrlsvr], [https://rjhansen.github.io/nsrlsvr])
AM_INIT_AUTOMAKE([foreign dist-xz])
AC_CONFIG_SRCDIR([src/Makefile.am])
AC_CONFIG_HEADERS([config.h:config.h.in])
AC_CONFIG_FILES([Makefile src/Makefile src/cargo/src/globals.rs])
AC_CHECK_PROG(HAS_CARGO, [cargo], [yes], [no])
if test "x${HAS_CARGO}" = xno; then
	AC_MSG_ERROR([
-----
Cargo (the Rust build tool) could not be found. Please ensure a
complete Rust environment is installed.
-----])
	fi
AC_PROG_CC([clang gcc cl])
AC_PROG_CXX([clang++ g++ cl])
AC_PROG_INSTALL
AC_OUTPUT

