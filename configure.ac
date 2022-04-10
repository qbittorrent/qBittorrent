AC_INIT([qbittorrent], [v4.5.0alpha], [bugs.qbittorrent.org], [], [https://www.qbittorrent.org/])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
: ${CFLAGS=""}
: ${CXXFLAGS=""}
AC_PROG_CC
AC_PROG_CXX
AC_PROG_SED
AC_LANG(C++)
AC_CANONICAL_HOST
AM_INIT_AUTOMAKE([foreign])

# use compiler from env variables if available
QBT_CC="$CC"
QBT_CXX="$CXX"

# Define --wth-* and --enable-* arguments

AC_ARG_ENABLE(debug,
              [AS_HELP_STRING([--enable-debug],
                              [Enable debug build])],
              [],
              [enable_debug=no])

AC_ARG_ENABLE(stacktrace,
              [AS_HELP_STRING([--enable-stacktrace],
                              [Enable stacktrace feature (default=auto)])],
              [],
              [enable_stacktrace=auto])

AC_ARG_ENABLE(gui,
              [AS_HELP_STRING([--disable-gui],
                              [Disable the GUI for headless running. Disables QtDBus and the GeoIP Database.])],
              [],
              [enable_gui=yes])

AC_ARG_ENABLE(systemd,
              [AS_HELP_STRING([--enable-systemd],
                              [Install the systemd service file (headless only).])],
              [],
              [enable_systemd=no])

AC_ARG_ENABLE(webui,
              [AS_HELP_STRING([--disable-webui],
                              [Disable the WebUI.])],
              [],
              [enable_webui=yes])

AC_ARG_ENABLE(qt-dbus,
              [AS_HELP_STRING([--disable-qt-dbus],
                              [Disable use of QtDBus (GUI only)])],
              [],
              [enable_qt_dbus=yes])

# Detect OS
AC_MSG_CHECKING([whether to enable specific tweaks for current host "$host_os"])
case "$host_os" in
    *darwin*)
        AC_MSG_RESULT([yes])
        enable_qt_dbus=no
        ;;

    *freebsd*)
        AC_MSG_RESULT([yes])
        LIBS="-lexecinfo $LIBS"
        ;;

    *haiku*)
        AC_MSG_RESULT([yes])
        LIBS="-lnetwork $LIBS"
        ;;

    *openbsd*)
        AC_MSG_RESULT([yes])
        LIBS="-lexecinfo $LIBS"
        ;;

    *)
        AC_MSG_RESULT([no])
        ;;
esac

# Require 0.23 pkg-config
PKG_PROG_PKG_CONFIG([0.23])
AS_IF([test "x$PKG_CONFIG" = "x"],
      [AC_MSG_ERROR([Could not find pkg-config])])

# Check which arguments were set and act accordingly
AC_MSG_CHECKING([whether to enable the Debug build])
AS_CASE(["x$enable_debug"],
        ["xno"],
              [AC_MSG_RESULT([no])
              QBT_ADD_CONFIG="$QBT_ADD_CONFIG release"
              QBT_REMOVE_CONFIG="$QBT_REMOVE_CONFIG debug"],
        ["xyes"],
               [AC_MSG_RESULT([yes])
               QBT_ADD_CONFIG="$QBT_ADD_CONFIG debug"
               QBT_REMOVE_CONFIG="$QBT_REMOVE_CONFIG release"],
        [AC_MSG_RESULT([$enable_debug])
        AC_MSG_ERROR([Unknown option "$enable_debug". Use either "yes" or "no".])])

AC_MSG_CHECKING([whether to enable the stacktrace feature])
AS_CASE(["x$enable_stacktrace"],
        ["xno"],
              [AC_MSG_RESULT([no])
              QBT_REMOVE_CONFIG="$QBT_REMOVE_CONFIG stacktrace"],
        ["xyes"],
               [AC_MSG_RESULT([yes])
               QBT_ADD_CONFIG="$QBT_ADD_CONFIG stacktrace"],
        ["xauto"],
                [AC_COMPILE_IFELSE([AC_LANG_SOURCE([[#include <execinfo.h>]])],
                [AC_MSG_RESULT([yes])
                QBT_ADD_CONFIG="$QBT_ADD_CONFIG stacktrace"],
                [AC_MSG_RESULT([no])
                QBT_REMOVE_CONFIG="$QBT_REMOVE_CONFIG stacktrace"])],
        [AC_MSG_RESULT([$enable_stacktrace])
        AC_MSG_ERROR([Unknown option "$enable_stacktrace". Use either "yes" or "no".])])

AC_MSG_CHECKING([whether to enable the GUI])
AS_CASE(["x$enable_gui"],
        ["xyes"],
               [AC_MSG_RESULT([yes])
               enable_systemd=[no]
               QBT_REMOVE_CONFIG="$QBT_REMOVE_CONFIG nogui"],
        ["xno"],
              [AC_MSG_RESULT([no])
              enable_qt_dbus=[no]
              QBT_ADD_CONFIG="$QBT_ADD_CONFIG nogui"],
        [AC_MSG_RESULT([$enable_gui])
        AC_MSG_ERROR([Unknown option "$enable_gui". Use either "yes" or "no".])])

AC_MSG_CHECKING([whether to install the systemd service file])
AS_CASE(["x$enable_systemd"],
        ["xyes"],
               [AC_MSG_RESULT([yes])
               QBT_ADD_CONFIG="$QBT_ADD_CONFIG systemd"],
        ["xno"],
              [AC_MSG_RESULT([no])
              QBT_REMOVE_CONFIG="$QBT_REMOVE_CONFIG systemd"],
        [AC_MSG_RESULT([$enable_systemd])
        AC_MSG_ERROR([Unknown option "$enable_systemd". Use either "yes" or "no".])])

AC_MSG_CHECKING([whether to enable the WebUI])
AS_CASE(["x$enable_webui"],
        ["xyes"],
               [AC_MSG_RESULT([yes])
               QBT_REMOVE_CONFIG="$QBT_REMOVE_CONFIG nowebui"],
        ["xno"],
              [AC_MSG_RESULT([no])
              QBT_ADD_CONFIG="$QBT_ADD_CONFIG nowebui"],
        [AC_MSG_RESULT([$enable_webui])
        AC_MSG_ERROR([Unknown option "$enable_webui". Use either "yes" or "no".])])

FIND_QT5()
AS_IF([test "x$QT_QMAKE" = "x"],
      [AC_MSG_ERROR([Could not find qmake])
      ])
AS_IF([test "x$enable_gui" = "xyes"],
      [PKG_CHECK_MODULES(Qt5Svg, [Qt5Svg >= 5.15.2])
      ])
AC_MSG_CHECKING([whether QtDBus should be enabled])
AS_CASE(["x$enable_qt_dbus"],
        ["xyes"],
               [AC_MSG_RESULT([yes])
               FIND_QTDBUS()
               AS_IF([test "x$HAVE_QTDBUS" = "xfalse"],
                     [AC_MSG_ERROR([Could not find QtDBus])],
                     [QBT_ADD_CONFIG="$QBT_ADD_CONFIG dbus"]
                    )],
        ["xno"],
              [AC_MSG_RESULT([no])
              QBT_REMOVE_CONFIG="$QBT_REMOVE_CONFIG dbus"],
        [AC_MSG_RESULT([$enable_qt_dbus])
        AC_MSG_ERROR([Unknown option "$enable_qt_dbus". Use either "yes" or "no".])])


AX_BOOST_BASE([1.65],
              [AC_MSG_NOTICE([Boost CXXFLAGS: "$BOOST_CPPFLAGS"])
               AC_MSG_NOTICE([Boost LDFLAGS: "$BOOST_LDFLAGS"])],
              [AC_MSG_ERROR([Could not find Boost])])
CXXFLAGS="$BOOST_CPPFLAGS $CXXFLAGS"
LDFLAGS="$BOOST_LDFLAGS $LDFLAGS"

# add workaround for problematic boost version
# taken from ax_boost_base.m4
m4_define([DETECT_BOOST_VERSION_PROGRAM],
    [AC_LANG_PROGRAM([[#include <boost/version.hpp>]],
                     [[(void) ((void)sizeof(char[1 - 2*!!((BOOST_VERSION) < ($1))]));]])])

PKG_CHECK_MODULES(libtorrent,
                  [libtorrent-rasterbar >= 2.0.4],
                  [CXXFLAGS="$libtorrent_CFLAGS $CXXFLAGS" LIBS="$libtorrent_LIBS $LIBS" QBT_ADD_DEFINES="$QBT_ADD_DEFINES QBT_USES_LIBTORRENT2"],
                  [PKG_CHECK_MODULES(libtorrent,
                                     [libtorrent-rasterbar >= 1.2.14 libtorrent-rasterbar < 2],
                                     [CXXFLAGS="$libtorrent_CFLAGS $CXXFLAGS" LIBS="$libtorrent_LIBS $LIBS"])])

PKG_CHECK_MODULES(openssl,
                  [openssl >= 1.1.1],
                  [CXXFLAGS="$openssl_CFLAGS $CXXFLAGS"
                  LIBS="$openssl_LIBS $LIBS"])

PKG_CHECK_MODULES(zlib,
                 [zlib >= 1.2.11],
                 [CXXFLAGS="$zlib_CFLAGS $CXXFLAGS"
                 LIBS="$zlib_LIBS $LIBS"])

# Check if already in >= C++17 mode because of the flags returned by one of the above packages
TMP_CXXFLAGS="$CXXFLAGS"
CXXFLAGS=""
AC_MSG_CHECKING([if compiler defaults to C++17 or later mode])
AC_COMPILE_IFELSE([DETECT_CPP17_PROGRAM()],
                  [AC_MSG_RESULT([yes])
                   QBT_CXX17_FOUND="yes"],
                  [AC_MSG_RESULT([no])
                   QBT_CXX17_FOUND="no"])

# In case of no, check if the compiler can support at least C++17
# and if yes, enable it leaving a warning to the user
AS_IF([test "x$QBT_CXX17_FOUND" = "xno"],
      [AC_MSG_CHECKING([if compiler supports C++17])
       CXXFLAGS="-std=c++17"
       AC_COMPILE_IFELSE([DETECT_CPP17_PROGRAM()],
                        [AC_MSG_RESULT([yes])
                         AC_MSG_CHECKING([if C++17 is disabled by the set compiler flags])
                         # prepend the flag so it won't override conflicting user defined flags
                         CXXFLAGS="-std=c++17 $TMP_CXXFLAGS"
                         AC_COMPILE_IFELSE([DETECT_CPP17_PROGRAM()],
                                           [AC_MSG_RESULT([no])
                                            QBT_ADD_CONFIG="$QBT_ADD_CONFIG c++17"
                                            AC_MSG_WARN([C++17 mode is now force enabled. The C++ mode should match the mode that other libraries were built with, otherwise you'll likely get linking errors.])],
                                           [AC_MSG_RESULT([yes])
                                            AC_MSG_ERROR([The compiler supports C++17 but the user or a dependency has explicitly enabled a lower mode.])])],
                        [AC_MSG_RESULT([no])
                         AC_MSG_ERROR([A compiler supporting C++17 is required.])])
      ])
CXXFLAGS="$TMP_CXXFLAGS"

# These are required because autoconf doesn't expand these **particular**
# vars automatically. And qmake cannot autoexpand them.
AX_DEFINE_DIR([EXPAND_PREFIX], [prefix])
AX_DEFINE_DIR([EXPAND_BINDIR], [bindir])
AX_DEFINE_DIR([EXPAND_DATADIR], [datadir])
AX_DEFINE_DIR([EXPAND_MANDIR], [mandir])

# Original extract() function contributed by pmzqla
# $*: Strings to parse
# Set $QBT_CONF_DEFINES, $QBT_CONF_INCLUDES, $QBT_CONF_EXTRA_CFLAGS
extract() {
  if [[ -z "$*" ]]; then
    echo "Input string required"
    return 1
  fi

  # BSD sed needs an actual newline character in the substitute command
  new_line='
'
  # Convert " -" to "\n" if not between quotes and remove possible leading white spaces
  string=$(echo " $*" | $SED -e "s: -:\\${new_line}:g" -e 's:"\(.*\)\n\(.*\)":\"\1 -\2":g' -e "s:'\(.*\)\n\(.*\)':\'\1 -\2':g" -e 's/^[[[:space:]]]*//')
  SAVEIFS=$IFS
  IFS=$(printf "\n\b")
  for i in $string; do
    case "$(echo "$i" | cut -c1)" in
      '') ;;
      D) QBT_CONF_DEFINES="$QBT_CONF_DEFINES $(echo $i | cut -c2-)";;
      I) QBT_CONF_INCLUDES="$QBT_CONF_INCLUDES $(echo $i | cut -c2-)";;
      *) QBT_CONF_EXTRA_CFLAGS="$QBT_CONF_EXTRA_CFLAGS -$i";;
    esac
  done
  IFS=$SAVEIFS
}

extract "$CFLAGS $CXXFLAGS"
QBT_ADD_DEFINES="$QBT_ADD_DEFINES $QBT_CONF_DEFINES"

# Substitute the values of these vars in conf.pri.in
AC_SUBST(QBT_CC)
AC_SUBST(QBT_CXX)
AC_SUBST(QBT_CONF_INCLUDES)
AC_SUBST(QBT_CONF_EXTRA_CFLAGS)
AC_SUBST(QBT_ADD_CONFIG)
AC_SUBST(QBT_REMOVE_CONFIG)
AC_SUBST(QBT_ADD_DEFINES)
AC_SUBST(QBT_REMOVE_DEFINES)

QBT_CONFIG_FILES="conf.pri"

AS_IF([test "x$enable_systemd" = "xyes"],
      [QBT_CONFIG_FILES="$QBT_CONFIG_FILES dist/unix/systemd/qbittorrent-nox@.service"])

AC_CONFIG_FILES(["$QBT_CONFIG_FILES"])
AC_OUTPUT

AC_MSG_NOTICE([Running qmake to generate the makefile...])
TOPDIR="$(cd "$(dirname "$0")" && pwd)"
$QT_QMAKE -r "$TOPDIR/qbittorrent.pro" "QMAKE_LRELEASE=$QMAKE_LRELEASE"
qmake_ret="$?"

AS_ECHO()
AS_IF([test "x$qmake_ret" != "x0"],
      [AC_MSG_ERROR([$QT_QMAKE failed to generate the makefile])])

AC_MSG_NOTICE([Good, the configure finished.])
AS_ECHO()
