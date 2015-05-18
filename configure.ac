AC_INIT([qbittorrent], [v3.2.0alpha], [bugs.qbittorrent.org], [], [http://www.qbittorrent.org/])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_PROG_CC
AC_PROG_CXX
AC_PROG_SED
AC_LANG(C++)
AC_CANONICAL_HOST
AM_INIT_AUTOMAKE



# Define --wth-* and --enable-* arguments

AC_ARG_WITH(qt5,
            [AS_HELP_STRING([--with-qt5],
                            [Compile using Qt5 (default=no)])],
            [],
            [with_qt5=no])

AC_ARG_WITH(libtorrent-rasterbar0.16,
            [AS_HELP_STRING([--with-libtorrent-rasterbar0.16],
                            [Compile using libtorrent-rasterbar 0.16.x series (default=no)])],
            [],
            [with_libtorrent_rasterbar0_16=no])

AC_ARG_WITH(geoip-database-embedded,
            [AS_HELP_STRING([--with-geoip-database-embedded],
                            [Embed the GeoIP database in the qBittorrent executable (please follow instructions in src/gui/geoip/README) (default=no)])],
            [],
            [with_geoip_database_embedded=no])

AC_ARG_WITH(qtsingleapplication,
            [AS_HELP_STRING([--with-qtsingleapplication=@<:@system|shipped@:>@],
                            [Use the shipped qtsingleapplication library or the system one (default=shipped)])],
            [],
            [with_qtsingleapplication=shipped])

AC_ARG_WITH(qjson,
            [AS_HELP_STRING([--with-qjson=@<:@system|shipped@:>@],
                            [Use the shipped qjson library or the system one (default=shipped) (Qt4 only)])],
            [],
            [with_qjson=shipped])

AC_ARG_ENABLE(debug,
              [AS_HELP_STRING([--enable-debug],
                              [Enable debug build])],
              [],
              [enable_debug=no])

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
AC_MSG_CHECKING([whether OS is FreeBSD])
AS_IF([expr "$host_os" : ".*freebsd.*" > /dev/null],
      [AC_MSG_RESULT([yes])
      LIBS="-lexecinfo $LIBS"],
      [AC_MSG_RESULT([no])])

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

AC_MSG_CHECKING([whether to enable the GUI])
AS_CASE(["x$enable_gui"],
        ["xyes"],
               [AC_MSG_RESULT([yes])
               enable_systemd=[no]
               QBT_REMOVE_CONFIG="$QBT_REMOVE_CONFIG nogui"],
        ["xno"],
              [AC_MSG_RESULT([no])
              enable_qt_dbus=[no]
              enable_geoip_database=[no]
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

AC_MSG_CHECKING([whether Qt5 should be enabled])
AS_CASE(["x$with_qt5"],
        ["xno"],
              [AC_MSG_RESULT([no])
              FIND_QT4()],
        ["xyes"],
               [AC_MSG_RESULT([yes])
               FIND_QT5()],
        [AC_MSG_RESULT([$with_qt5])
        AC_MSG_ERROR([Unknown option "$with_qt5". Use either "yes" or "no".])])
AS_IF([test "x$QT_QMAKE" = "x"],
      [AC_MSG_ERROR([Could not find qmake])
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


AX_BOOST_BASE([1.35])
# HAVE_BOOST is set to an empty value when Boost is found. I don't know
# how to test for a set vs unset variable.
AS_IF([test "x$BOOST_CPPFLAGS" = "x"],
      [AC_MSG_ERROR([Could not find Boost])],
      [AC_MSG_NOTICE([Boost CPPFLGAS: $BOOST_CPPFLAGS])
      CPPFLAGS="$BOOST_CPPFLAGS $CPPFLAGS"
      LDFLAGS="$BOOST_LDFLAGS $LDFLAGS"])

AX_BOOST_SYSTEM()
# HAVE_BOOST_SYSTEM is set to an empty value when Boost.System is found.
# I don't know how to test for a set vs unset variable.
AS_IF([test "x$BOOST_SYSTEM_LIB" = "x"],
      [AC_MSG_ERROR([Could not find Boost.System])],
      [AC_MSG_NOTICE([Boost.System LIB: $BOOST_SYSTEM_LIB])
      LIBS="$BOOST_SYSTEM_LIB $LIBS"])

AC_MSG_CHECKING([whether to compile using libtorrent-rasterbar 0.16.x])
AS_CASE(["x$with_libtorrent_rasterbar0_16"],
        ["xno"],
              [AC_MSG_RESULT([no])
              PKG_CHECK_MODULES(libtorrent,
                               [libtorrent-rasterbar >= 1.0.0],
                               [CPPFLAGS="$libtorrent_CFLAGS $CPPFLAGS"
                               LIBS="$libtorrent_LIBS $LIBS"])],
        ["xyes"],
               [AC_MSG_RESULT([yes])
               PKG_CHECK_MODULES(libtorrent,
                                [libtorrent-rasterbar >= 0.16.0],
                                [CPPFLAGS="$libtorrent_CFLAGS $CPPFLAGS"
                                LIBS="$libtorrent_LIBS $LIBS"])],
        [AC_MSG_RESULT([$with_libtorrent_rasterbar0_16])
        AC_MSG_ERROR([Unknown option "$with_libtorrent_rasterbar0_16". Use either "yes" or "no".])])

AC_MSG_CHECKING([whether to embed the GeoIP database])
AS_CASE(["x$with_geoip_database_embedded"],
        ["xno"],
              [AC_MSG_RESULT([no])
              QBT_REMOVE_DEFINES="$QBT_REMOVE_DEFINES WITH_GEOIP_EMBEDDED"],
        ["xyes"],
               [AC_MSG_RESULT([yes])
               QBT_ADD_DEFINES="$QBT_ADD_DEFINES WITH_GEOIP_EMBEDDED"],
        [AC_MSG_RESULT([$with_geoip_database_embedded])
        AC_MSG_ERROR([Unknown option "$with_geoip_database_embedded". Use either "yes" or "no".])])

AC_MSG_CHECKING([which qtsingleapplication to use])
AS_CASE(["x$with_qtsingleapplication"],
        ["xshipped"],
                   [AC_MSG_RESULT([shipped])
                   QBT_REMOVE_CONFIG="$QBT_REMOVE_CONFIG usesystemqtsingleapplication"],
        ["xsystem"],
                  [AC_MSG_RESULT([system])
                  QBT_ADD_CONFIG="$QBT_ADD_CONFIG usesystemqtsingleapplication"],
        [AC_MSG_RESULT([$with_qtsingleapplication])
        AC_MSG_ERROR([Unknown option "$with_qtsingleapplication". Use either "system" or "shipped".])])

AS_IF([test "x$with_qt5" = "xno"],
      [AC_MSG_CHECKING([which qjson to use])
      AS_CASE(["x$with_qjson"],
              ["xshipped"],
                         [AC_MSG_RESULT([shipped])
                         QBT_REMOVE_CONFIG="$QBT_REMOVE_CONFIG usesystemqjson"],
              ["xsystem"],
                         [AC_MSG_RESULT([system])
                         PKG_CHECK_MODULES(qjson,
                                          [QJson >= 0.8.1],
                                          [CPPFLAGS="$qjson_CFLAGS $CPPFLAGS"
                                          LIBS="$qjson_LIBS $LIBS"])
                         QBT_ADD_CONFIG="$QBT_ADD_CONFIG usesystemqjson"],
              [AC_MSG_RESULT([$with_qjson])
              AC_MSG_ERROR([Unknown option "$with_qjson". Use either "system" or "shipped".])])
     ])

PKG_CHECK_MODULES(zlib,
                 [zlib],
                 [CPPFLAGS="$zlib_CFLAGS $CPPFLAGS"
                 LIBS="$zlib_LIBS $LIBS"])

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

  # Convert " -" to "\n" if not between quotes and remove possible leading white spaces
  string=$(echo " $*" | $SED -e 's: -:\n:g' -e 's:"\(.*\)\n\(.*\)":\"\1 -\2":g' -e "s:'\(.*\)\n\(.*\)':\'\1 -\2':g" -e 's/^[[:space:]]*//')
  SAVEIFS=$IFS
  IFS=$(printf "\n\b")
  for i in $string; do
    case "$(echo "$i" | cut -c1)" in
      '') ;;
      D) QBT_CONF_DEFINES="$(echo $i | cut -c2-) $QBT_CONF_DEFINES";;
      I) QBT_CONF_INCLUDES="$(echo $i | cut -c2-) $QBT_CONF_INCLUDES";;
      *) QBT_CONF_EXTRA_CFLAGS="-$i $QBT_CONF_EXTRA_CFLAGS";;
    esac
  done
  IFS=$SAVEIFS
}

extract $CPPFLAGS
QBT_ADD_DEFINES="$QBT_ADD_DEFINES $QBT_CONF_DEFINES"
QBT_CONF_EXTRA_CFLAGS="$QBT_CONF_EXTRA_CFLAGS $CXXFLAGS"

# Substitute the values of these vars in conf.pri.in
AC_SUBST(QBT_CONF_INCLUDES)
AC_SUBST(QBT_CONF_EXTRA_CFLAGS)
AC_SUBST(QBT_ADD_CONFIG)
AC_SUBST(QBT_REMOVE_CONFIG)
AC_SUBST(QBT_ADD_DEFINES)
AC_SUBST(QBT_REMOVE_DEFINES)

AC_OUTPUT(conf.pri)
AS_IF([test "x$enable_systemd" = "xyes"],
      [AC_OUTPUT(dist/unix/systemd/qbittorrent-nox.service)])



AC_MSG_NOTICE([Running qmake to generate the makefile...])
CONFDIR="$( cd "$( dirname "$0" )" && pwd )"

$QT_QMAKE -r [$CONFDIR]/qbittorrent.pro

ret="$?"

AS_ECHO()
AS_IF([test "x$ret" = "x0"],
      [AC_MSG_NOTICE([Good, your configure finished.])],
      [AC_MSG_ERROR([Failed running $QT_QMAKE to generate the makefile])])
AS_ECHO()

