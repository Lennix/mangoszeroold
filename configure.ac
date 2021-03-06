# Copyright (C) 2005-2010 MaNGOS project <http://getmangos.com/>
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

## Process this file with autoconf to produce a configure script.

# TODO: create m4 directory and put the checks there, because this file got realy poluted ( by Derex )

## Prelude, basic settings for Autoconf
#  PACKAGE:            mangoszero
#  VERSION:            0.12.0 (trunk)
#  BUG-REPORT-ADDRESS: mangos-devs@lists.sourceforge.net
AC_INIT( [mangos], [0.12.0], [mangos-devs@lists.sourceforge.net])
AC_CONFIG_SRCDIR([src/shared/Common.h])
AC_CONFIG_MACRO_DIR([m4])

## Prelude, basic settings for Automake
#  Turn on all warnings and error messages, and enforce GNU
#  standards for the package.
AM_INIT_AUTOMAKE([foreign -Wall -Werror gnu tar-pax])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AM_MAINTAINER_MODE

## Prevent the configure script from continuing any further if
# configuration is being performed in the top-level directory.
# The idea is to prevent this ,because some maintainers tend
# to break parallel build trees (a.k.a. VPATH builds).
if test "$srcdir" = "." && test "$enable_maintainer_mode" != "yes"; then
  AC_MSG_ERROR(
     [
      Please configure and build in a directory other than the
      top-level source directory. This is needed because a lot
      of maintainers tend to break parallel build trees
      (a.k.a. VPATH builds). This is kinda real ensurance they
      will not do it (by enforcing everybody to do VPATH builds).

      For example, try the following from the top-level source
      directory:

          mkdir objdir
          cd objdir
          ../configure
          make

      This will create a build space in the directory 'objdir' and
      start a build in that directory.

      If however you realy want to disable this error,
      use --enable-maintainer-mode switch.
     ])
fi

## Disable building of static libraries by default
AC_DISABLE_STATIC

## Check for required dependencies.

## Check for a valid build environment.
#  Valid equals having:
#  - a C++ compiler compliant with the ISO98 C++ specification.
#  - a working library tool for creating convenience libraries.
#  - a working linker for creating static and shared libraries.
AC_PROG_CC
AC_PROG_CXX
AM_PROG_CC_C_O
AC_PROG_LIBTOOL
AC_PROG_INSTALL
PKG_PROG_PKG_CONFIG

# Check for doxygen
AC_ARG_ENABLE(doxygen, AC_HELP_STRING([--enable-doxygen], [turn on generating documentation]))

enable_doxygen_support=no

if test "x$enable_doxygen" = "xyes";
then
  AC_PATH_PROG(DOXYGEN, doxygen, no)
  if test "x$DOXYGEN" = "xno"; then
    AC_MSG_ERROR([You need to install the doxygen package])
  fi
  enable_doxygen_support=yes
fi
AM_CONDITIONAL(DOXYGEN_ENABLED, test x$enable_doxygen_support = xyes)

## Check for required libraries.
AC_CHECK_LIB( pthread, pthread_create, [],
    [LDFLAGS="-pthread $LDFLAGS"
     AC_TRY_LINK([char pthread_create();],
         pthread_create();,
         [], [AC_MSG_ERROR([Missing pthread])])
    ])
AC_CHECK_LIB( z, compress, [ZLIB=-lz],[AC_MSG_ERROR([Missing zlib])] )
AC_CHECK_LIB( compat, ftime, [COMPATLIB=-lcompat] )
AX_CHECK_OPENSSL([], [AC_MSG_ERROR([Missing OpenSSL])])

AC_ARG_WITH(postgresql,
[  --with-postgresql       Use PostgreSQL as a backend (default: no)],
[case "${withval}" in
	yes) DO_POSTGRESQL=yes ;;
	no)  DO_POSTGRESQL=no ;;
	maybe) DO_POSTGRESQL=maybe ;;
	*)   AC_MSG_ERROR(Bad value ${withval} for --with-postgresql) ;;
    esac],
[DO_POSTGRESQL=no])

AC_ARG_WITH(mysql,
[  --with-mysql            Use MySQL as a backend (default: yes)],
[case "${withval}" in
	yes) DO_MYSQL=yes ;;
	no)  DO_MYSQL=no ;;
	maybe) DO_MYSQL=maybe ;;
	*)   AC_MSG_ERROR(Bad value ${withval} for --with-mysql) ;;
    esac],
[DO_MYSQL=yes])

# here Postgre
AC_MSG_CHECKING(whether to build/link POSTGRESQL)
if test "x$DO_POSTGRESQL" = "xyes"; then
DO_MYSQL=no
POSTGRE_INCLUDES="-I/usr/include/postgresql $POSTGRE_INCLUDES"
POSTGRE_LIBS="-L/usr/lib/postresql -lpq -lz -lpthread -lcrypt -lnsl -lm -lpthread -L/usr/lib $OPENSSL_LIBS $POSTGRE_LIBS "
CXXFLAGS="-DDO_POSTGRESQL $CXXFLAGS"
fi
AC_MSG_RESULT($DO_POSTGRESQL)

# here mysql
AC_MSG_CHECKING(whether to build/link MYSQL)
if test "x$DO_MYSQL" = "xyes"; then
AC_MSG_RESULT($DO_MYSQL)
AC_PATH_PROGS(MYSQL_CONFIG, mysql_config, mysql_config, $PATH)
    if test -x "$MYSQL_CONFIG"
        then
    # MySQL v4 uses --include while v3 uses --cflags
        MYSQL_INCLUDES="`$MYSQL_CONFIG --include`" || \
	    MYSQL_INCLUDES="`$MYSQL_CONFIG --cflags`"
        MYSQL_LIBS="`$MYSQL_CONFIG --libs_r`"
	CXXFLAGS="-DDO_MYSQL $CXXFLAGS"
	# Workaround for --as-needed linker flag that comes sometimes from mysql
	if echo $MYSQL_LIBS|grep "\-Wl,--as-needed" > /dev/null; then
		MYSQL_LIBS=`echo $MYSQL_LIBS|sed -e "s/-Wl,--as-needed//"`;
		AC_MSG_WARN([Your mysql libs contains -Wl,--as-needed, this is not good, problem fixed in more recent mysql versions])
	fi
    fi
else
AC_MSG_RESULT($DO_MYSQL)
fi

## Check for memory allocator
#  Use libc-malloc or libtbb-malloc?
AC_MSG_CHECKING(whether to use libc malloc)
MANGOSD_STD_MALLOC=no
TBB_LIBS="-L\$(top_builddir)/dep/tbb -ltbb -ltbbmalloc"
AC_ARG_WITH(std-malloc,
[
Memory allocation options:

  --with-std-malloc       Use standard malloc],
[
    if test "$withval" = "yes" ; then
        CFLAGS="-DUSE_STANDARD_MALLOC $CFLAGS"
        CXXFLAGS="-DUSE_STANDARD_MALLOC $CXXFLAGS"
        MANGOSD_STD_MALLOC=yes
        TBB_LIBS=""
    elif test "$withval" != "no" ; then
        AC_MSG_ERROR(Please choose yes or no)
    fi
])
AC_MSG_RESULT($MANGOSD_STD_MALLOC)

## Check for options
#  Include debug info in library?
AC_MSG_CHECKING(whether to include debug info in library)
MANGOSD_DEBUG_INFO=no
AC_ARG_WITH(debug-info,
[
Debugging options:

  --with-debug-info       Include debug info in library],
[
    if test "$withval" = "yes" ; then
	CFLAGS="-g -DMANGOS_DEBUG $CFLAGS"
	CXXFLAGS="-g -DMANGOS_DEBUG $CXXFLAGS"
	MANGOSD_DEBUG_INFO=yes
    elif test "$withval" != "no" ; then
	AC_MSG_ERROR(Please choose yes or no)
    fi
])
AC_MSG_RESULT($MANGOSD_DEBUG_INFO)

## Check for required header files.
AC_HEADER_STDC
AC_HEADER_DIRENT
AC_CHECK_HEADERS([ arpa/inet.h fcntl.h limits.h locale.h malloc.h netdb.h netinet/in.h stddef.h stdint.h stdlib.h string.h strings.h sys/ioctl.h sys/param.h sys/socket.h sys/timeb.h sys/time.h termios.h unistd.h  ])

AC_CHECK_HEADERS([pthread.h])
AC_CHECK_HEADERS([mysql.h mysql/mysql.h])
AC_CHECK_HEADERS([libpq-fe.h])
AC_CHECK_HEADERS([zlib.h])

## Check for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE
AC_HEADER_TIME
AC_STRUCT_TM
AC_C_VOLATILE
AC_CHECK_TYPES([ptrdiff_t])

## Check for required library functions.
AC_FUNC_CLOSEDIR_VOID
AC_FUNC_ERROR_AT_LINE
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_REALLOC
AC_FUNC_SELECT_ARGTYPES
AC_TYPE_SIGNAL
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([atexit ftime gethostbyaddr gethostbyname gethostname gettimeofday memmove memset pow realpath select socket sqrt strchr strdup strerror strstr])


# Check for use buildtin ACE sources
AC_ARG_ENABLE(builtin_ace, AC_HELP_STRING([--enable-builtin-ace], [turn on/off using builtin ACE sources]))

enable_builtin_ace_support=yes

if test "x$enable_builtin_ace" = "xno"; then
  PKG_CHECK_MODULES(ACE, [ACE >= 5.5.2], [need_builtin_ace=no], [need_builtin_ace=yes])
  if test X$need_builtin_ace = Xyes ; then
    AC_MSG_ERROR([You need to install ACE package or use built-in ACE sources])
  fi
  enable_builtin_ace_support=no
fi

if test X$enable_builtin_ace_support = Xno; then

  # old ace versions might not have ace/Stack_Trace.h...
  AC_LANG_PUSH([C++])
  AC_CHECK_HEADERS([ace/Stack_Trace.h])
  AC_LANG_POP
else
  # ...but we certainly have it in our included version
  AC_DEFINE(HAVE_ACE_STACK_TRACE_H)
  ACE_INCLUDES="-I\$(top_srcdir)/dep/ACE_wrappers -I\$(top_builddir)/dep/ACE_wrappers"
  ACE_LIBS="\$(top_builddir)/dep/ACE_wrappers/ace/libACE.la"
fi

AM_CONDITIONAL([MANGOS_BUILD_ACE], [test X$enable_builtin_ace_support = Xyes])

## Unify all additional includes/libs in one variable.
# TODO this looks kinda ugly, but when we add m4 folder I will make it look very pritey ( by Derex ).
MANGOS_INCLUDES="$ACE_INCLUDES $POSTGRE_INCLUDES $MYSQL_INCLUDES $OPENSSL_INCLUDES"
MANGOS_LIBS="$ACE_LIBS $POSTGRE_LIBS $MYSQL_LIBS $ZLIB $COMPATLIB $OPENSSL_LDFLAGS $OPENSSL_LIBS $TBB_LIBS"

## Export defined variables
AC_SUBST(DOXYGEN)
AC_SUBST(MANGOSD_DEBUG_INFO)

## Additional CPPFLAGS and LDFLAGS.
AC_SUBST(MANGOS_INCLUDES)
AC_SUBST(MANGOS_LIBS)

## Set output files.
AC_CONFIG_HEADERS([config.h])

AC_SEARCH_LIBS(poll, [poll], [AC_DEFINE(HAVE_POLL, 1, [Define to 1 if the OS is usabl... err, has poll().])])

AH_TOP([
#ifndef AC_CONFIG_H
#define AC_CONFIG_H
])

AH_BOTTOM([#endif /* !AC_CONFIG_H */])

AC_CONFIG_FILES([
   dep/include/Makefile
   dep/lib/Makefile
   dep/src/Makefile
   dep/src/g3dlite/Makefile
   dep/src/zlib/Makefile
   dep/src/gsoap/Makefile
   dep/Makefile
   dep/tbb/Makefile
   doc/Doxyfile
   doc/Makefile
   Makefile
   sql/Makefile
   sql/tools/Makefile
   sql/updates/Makefile
   src/Makefile
   src/tools/Makefile
   src/tools/genrevision/Makefile
   src/framework/Makefile
   src/shared/Makefile
   src/shared/Auth/Makefile
   src/shared/Config/Makefile
   src/shared/Database/Makefile
   src/shared/vmap/Makefile
   src/shared/SystemConfig.h
   src/game/Makefile
   src/realmd/Makefile
   src/realmd/realmd.conf.dist
   src/mangosd/Makefile
   src/mangosd/mangosd.conf.dist
   src/mangosd/mods.conf.dist
   src/bindings/Makefile
   src/bindings/scriptdevzero/Makefile
   src/bindings/scriptdevzero/scriptdevzero.conf.dist
   src/bindings/scriptdevzero/config.h
   src/bindings/scriptdevzero/sql/Makefile
   src/bindings/scriptdevzero/sql/Updates/Makefile
])

## Configure ACE, if needed
if test X$enable_builtin_ace_support = Xyes; then
  AC_CONFIG_SUBDIRS([dep/ACE_wrappers])
fi

AC_CONFIG_COMMANDS([default],[
  echo ""
  echo "Configuration of MaNGOS $PACKAGE_VERSION is now complete."
  echo ""
 ],[PACKAGE_VERSION=$PACKAGE_VERSION])

## Disabled Makefiles, until they are ready for a successful make and
#  make dist run.

## Output files.
AC_OUTPUT
