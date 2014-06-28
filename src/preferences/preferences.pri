INCLUDEPATH += $$PWD

!contains(DEFINES, DISABLE_GUI) {

  HEADERS += $$PWD/options_imp.h \
             $$PWD/advancedsettings.h

  SOURCES += $$PWD/options_imp.cpp

  FORMS += $$PWD/options.ui
}

HEADERS += $$PWD/preferences.h

SOURCES += $$PWD/preferences.cpp
