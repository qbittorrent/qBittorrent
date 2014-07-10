AC_INIT([qbittorrent], [v3.2.0alpha], [bugs.qbittorrent.org], [], [http://www.qbittorrent.org/])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_PROG_CC
AC_PROG_CXX
AC_LANG(C++)
AC_CANONICAL_HOST
AM_INIT_AUTOMAKE



# Define --wth-* and --enable-* arguments

AC_ARG_WITH(qt5,
            [AS_HELP_STRING([--with-qt5],
                            [Compile using Qt5 (default=no)])],
            [],
            [with_qt5=no])
            
AC_ARG_WITH(geoip-database-embedded,
            [AS_HELP_STRING([--with-geoip-database-embedded],
                            [Embed the GeoIP database in the qBittorrent executable (please follow instructions in src/geoip/README) (default=no)])],
            [],
            [with_geoip_database_embedded=no])
            
AC_ARG_WITH(qtsingleapplication,
            [AS_HELP_STRING([--with-qtsingleapplication=@<:@system|shipped@:>@],
                            [Use the shipped qtsingleapplication library or the system one (default=shipped)])],
            [],
            [with_qtsingleapplication=shipped])
             
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
              
AC_ARG_ENABLE(qt-dbus,
              [AS_HELP_STRING([--disable-qt-dbus],
                              [Disable use of QtDBus (GUI only)])],
              [],
              [enable_qt_dbus=yes])

# Detect OS
AC_MSG_CHECKING([whether OS is FreeBSD])
AS_IF([test "x$host_os" == "x*freebsd*"],
      [AC_MSG_RESULT([yes])
      LIBS="-lexecinfo $LIBS"],
      [AC_MSG_RESULT([no])])

# Require 0.26 pkg-config
PKG_PROG_PKG_CONFIG([0.26])
AS_IF([test "x$PKG_CONFIG" == "x"],
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
               QBT_REMOVE_CONFIG="$QBT_REMOVE_CONFIG nox"],
        ["xno"],
              [AC_MSG_RESULT([no])
              enable_qt_dbus=[no]
              enable_geoip_database=[no]
              QBT_ADD_CONFIG="$QBT_ADD_CONFIG nox"],        
        [AC_MSG_RESULT([$enable_gui])
        AC_MSG_ERROR([Unknown option "$enable_gui". Use either "yes" or "no".])])

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
AS_IF([test "x$QT_QMAKE" == "x"],
      [AC_MSG_ERROR([Could not find qmake])
      ])
      
AC_MSG_CHECKING([whether QtDBus should be enabled])
AS_CASE(["x$enable_qt_dbus"],
        ["xyes"],
               [AC_MSG_RESULT([yes])
               FIND_QTDBUS()
               AS_IF([test "x$HAVE_QTDBUS" == "xfalse"],
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
AS_IF([test "x$BOOST_CPPFLAGS" == "x"],
      [AC_MSG_ERROR([Could not find Boost])],
	  [AC_MSG_NOTICE([Boost CPPFLGAS: $BOOST_CPPFLAGS])
	  CPPFLAGS="$BOOST_CPPFLAGS $CPPFLAGS"
	  LDFLAGS="$BOOST_LDFLAGS $LDFLAGS"])

AX_BOOST_SYSTEM()
# HAVE_BOOST_SYSTEM is set to an empty value when Boost.System is found.
# I don't know how to test for a set vs unset variable.
AS_IF([test "x$BOOST_SYSTEM_LIB" == "x"],
      [AC_MSG_ERROR([Could not find Boost.System])],
	  [AC_MSG_NOTICE([Boost.System LIB: $BOOST_SYSTEM_LIB])
	  LIBS="$BOOST_SYSTEM_LIB $LIBS"])

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

PKG_CHECK_MODULES(zlib,
                 [zlib],
                 [CPPFLAGS="$zlib_CFLAGS $CPPFLAGS"
                 LIBS="$zlib_LIBS $LIBS"])

PKG_CHECK_MODULES(libtorrent,
                 [libtorrent-rasterbar >= 0.16.0],
                 [CPPFLAGS="$libtorrent_CFLAGS $CPPFLAGS"
                 LIBS="$libtorrent_LIBS $LIBS"])
                 
# These are required because autoconf doesn't expand these **particular**
# vars automatically. And qmake cannot autoexpand them.                 
AX_DEFINE_DIR([EXPAND_PREFIX], [prefix])
AX_DEFINE_DIR([EXPAND_BINDIR], [bindir])
AX_DEFINE_DIR([EXPAND_DATADIR], [datadir])
AX_DEFINE_DIR([EXPAND_MANDIR], [mandir])

# Substitute the values of these vars in conf.pri.in
AC_SUBST(QBT_ADD_CONFIG)
AC_SUBST(QBT_REMOVE_CONFIG)
AC_SUBST(QBT_ADD_DEFINES)
AC_SUBST(QBT_REMOVE_DEFINES)

AC_OUTPUT(conf.pri)



AC_MSG_NOTICE([Running qmake to generate the makefile...])
CONFDIR="$( cd "$( dirname "$0" )" && pwd )"

$QT_QMAKE [$CONFDIR]/qbittorrent.pro

ret="$?"

AS_ECHO()
AS_IF([test "x$ret" == "x0"],
      [AC_MSG_NOTICE([Good, your configure finished.])],
      [AC_MSG_ERROR([Failed running $QT_QMAKE to generate the makefile])])
AS_ECHO()

