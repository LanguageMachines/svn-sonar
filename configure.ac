#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
# $Id: configure.ac 13941 2012-01-10 16:43:46Z sloot $
# $URL $

AC_PREREQ(2.61)
AC_INIT([sonartools],[0.1],[ko.vandersloot@uvt.nl])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([.])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADER([config.h])

if test x"${CXXFLAGS+set}" = xset; then
  # the user set CXXFLAGS; don't override it.
  cxx_flags_were_set=true
else
  cxx_flags_were_set=false
fi
if $cxx_flags_were_set; then
  CXXFLAGS=$CXXFLAGS
fi

# Checks for programs.
AC_PROG_CXX( [g++] )

# when running tests, use CXX
AC_LANG([C++])

# use libtool
AC_PROG_LIBTOOL
AC_SUBST([LIBTOOL_DEPS])

#checks for libraries.

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

# ugly hack when PKG_CONFIG_PATH isn't defined.
# couldn't get it to work otherwise
if test "x$PKG_CONFIG_PATH" = x; then
    export PKG_CONFIG_PATH="/usr/lib/pkgconfig"
fi

PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES([XML2], [libxml-2.0 >= 2.6.16] )
CXXFLAGS="$CXXFLAGS $XML2_CFLAGS"
LIBS="$XML2_LIBS $LIBS"

OPENMP_CHECK( [CXXFLAGS="$CXXFLAGS $OPENMP_CXXFLAGS"
	       AC_DEFINE(HAVE_OPENMP, 1 , Define to 1 if you have OpenMP )],
	      [AC_MSG_NOTICE(We don't have OpenMP. Multithreaded testing is disabled)] )

AC_SUBST([OPENMPFLAGS],[$OPENMP_CXXFLAGS])
## from ucto
#
useICU=1;
# inspired by feh-1.3.4/configure.ac.  Tnx Tom Gilbert and feh hackers.
AC_ARG_WITH(icu,
       [  --with-icu=DIR          use ICU installed in <DIR>],
       [if test "$with_icu" = "no"; then
           useICU=0
        else
           CXXFLAGS="$CXXFLAGS -I$withval/include"
           LIBS="-L$withval/lib $LIBS"
        fi] )

if test "$useICU" = "1"; then
  AX_ICU_CHECK( [3.6],
                [CXXFLAGS="$CXXFLAGS $ICU_CPPSEARCHPATH"
                LIBS="$LIBS $ICU_LIBPATH $ICU_LIBS $ICU_IOLIBS"],
                [AC_MSG_FAILURE( "No ICU development environment found. Please check if libicu-dev or the like is instal
led" )] )
  AC_DEFINE(HAVE_ICU, 1, we want to use ICU )
else
  AC_MSG_ERROR("ICU support is required")
fi

AC_ARG_WITH(ticcutils,
       [  --with-ticcutils=DIR       use ticcutils installed in <DIR>;
               note that you can install ticcutils in a non-default directory with
               ./configure --prefix=<DIR> in the ticcutils installation directory],
       [PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$withval/lib/pkgconfig"],
       [PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$prefix/lib/pkgconfig"])
#  AC_MSG_NOTICE( [pkg-config search path: $PKG_CONFIG_PATH] )
PKG_CHECK_MODULES([ticcutils], [ticcutils >= 0.5] )
CXXFLAGS="$CXXFLAGS $ticcutils_CFLAGS"
LIBS="$ticcutils_LIBS $LIBS"

AC_ARG_WITH(timbl,
       [  --with-timbl=DIR       use timbl installed in <DIR>;
               note that you can install timbl in a non-default directory with
               ./configure --prefix=<DIR> in the timbl installation directory],
       [PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$withval/lib/pkgconfig"],
       [PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$prefix/lib/pkgconfig"])
#  AC_MSG_NOTICE( [pkg-config search path: $PKG_CONFIG_PATH] )
PKG_CHECK_MODULES([timbl], [timbl >= 6.4.2] )
CXXFLAGS="$CXXFLAGS $timbl_CFLAGS"
LIBS="$timbl_LIBS $LIBS"

AC_ARG_WITH(timblserver,
       [  --with-timblserver=DIR       use timblserver installed in <DIR>;
               note that you can install timbl in a non-default directory with
               ./configure --prefix=<DIR> in the timbl installation directory],
       [PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$withval/lib/pkgconfig"])
#  AC_MSG_NOTICE( [pkg-config search path: $PKG_CONFIG_PATH] )
PKG_CHECK_MODULES([timblserver], [timblserver >= 1.4] )
CXXFLAGS="$CXXFLAGS $timblserver_CFLAGS"
LIBS="$timblserver_LIBS $LIBS"

AC_CONFIG_FILES([
  Makefile
  m4/Makefile
  src/Makefile
  include/Makefile
  include/sonar/Makefile
  docs/Makefile
])
AC_OUTPUT
