INCLUDEPATH += $$PWD

usesystemqtsingleapplication {
    nogui {
        CONFIG += qtsinglecoreapplication
    } else {
        CONFIG += qtsingleapplication
    }
} else {
    nogui {
        include(qtsingleapplication/qtsinglecoreapplication.pri)
    } else {
        include(qtsingleapplication/qtsingleapplication.pri)
    }
}

HEADERS += \
    $$PWD/application.h \
    $$PWD/filelogger.h

SOURCES += \
    $$PWD/application.cpp \
    $$PWD/filelogger.cpp \
    $$PWD/main.cpp

unix: HEADERS += $$PWD/stacktrace.h
strace_win {
    HEADERS += $$PWD/stacktrace_win.h
    !nogui {
        HEADERS += $$PWD/stacktrace_win_dlg.h
        FORMS += $$PWD/stacktrace_win_dlg.ui
    }
}

# upgrade code
HEADERS += $$PWD/upgrade.h
