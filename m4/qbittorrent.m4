# Checking for pkg-config. If found, check for QtCore and query pkg-config
# for its exec-prefix variable.

# FIND_QT4()
# Sets the QT_QMAKE variable to the path of Qt4 qmake if found.
# --------------------------------------
AC_DEFUN([FIND_QT4],
[PKG_CHECK_EXISTS([QtCore >= 4.8.0],
                 [PKG_CHECK_VAR(QT_QMAKE,
                                [QtCore >= 4.8.0],
                                [moc_location],
                                [QT_QMAKE=`AS_DIRNAME(["$QT_QMAKE"])`])
                 ])

AC_CHECK_FILE([$QT_QMAKE/qmake],
              [QT_QMAKE="$QT_QMAKE/qmake"],
              [AC_CHECK_FILE([$QT_QMAKE/qmake-qt4],
                             [QT_QMAKE="$QT_QMAKE/qmake-qt4"],
                             [QT_QMAKE=""])
              ])

AC_MSG_CHECKING([for Qt4 qmake >= 4.8.0])
AS_IF([test "x$QT_QMAKE" != "x"],
      [AC_MSG_RESULT([$QT_QMAKE])],
      [AC_MSG_RESULT([not found])]
      )
])

# FIND_QT5()
# Sets the QT_QMAKE variable to the path of Qt5 qmake if found.
# --------------------------------------
AC_DEFUN([FIND_QT5],
[PKG_CHECK_EXISTS([Qt5Core >= 5.2.0],
                 [PKG_CHECK_VAR(QT_QMAKE,
                                [Qt5Core >= 5.2.0],
                                [host_bins])
                 ])

AC_CHECK_FILE([$QT_QMAKE/qmake],
              [QT_QMAKE="$QT_QMAKE/qmake"],
              [AC_CHECK_FILE([$QT_QMAKE/qmake-qt5],
                             [QT_QMAKE="$QT_QMAKE/qmake-qt5"],
                             [QT_QMAKE=""])
              ])

AC_MSG_CHECKING([for Qt5 qmake >= 5.2.0])
AS_IF([test "x$QT_QMAKE" != "x"],
      [AC_MSG_RESULT([$QT_QMAKE])],
      [AC_MSG_RESULT([not found])]
      )
])

# FIND_QTDBUS()
# Sets the HAVE_QTDBUS variable to true or false.
# --------------------------------------
AC_DEFUN([FIND_QTDBUS],
[AS_IF([test "x$with_qt5" = "xyes"],
       [AC_MSG_CHECKING([for Qt5DBus >= 5.2.0])
       PKG_CHECK_EXISTS([Qt5DBus >= 5.2.0],
                        [AC_MSG_RESULT([found])
                         HAVE_QTDBUS=[true]],
                        [AC_MSG_RESULT([not found])
                         HAVE_QTDBUS=[false]])
       ],
       [AC_MSG_CHECKING([for QtDBus >= 4.8.0])
       PKG_CHECK_EXISTS([QtDBus >= 4.8.0],
                        [AC_MSG_RESULT([found])
                         HAVE_QTDBUS=[true]],
                        [AC_MSG_RESULT([not found])
                         HAVE_QTDBUS=[false]])
       ])
])
