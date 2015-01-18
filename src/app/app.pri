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

!nogui {
    macx {
        HEADERS += $$PWD/qmacapplication.h
        SOURCES += $$PWD/qmacapplication.cpp
    }
    HEADERS += $$PWD/sessionapplication.h
    SOURCES += $$PWD/sessionapplication.cpp
}

HEADERS += $$PWD/application.h
SOURCES += $$PWD/application.cpp

nogui: HEADERS += $$PWD/headlessloader.h

unix: HEADERS += $$PWD/stacktrace.h
strace_win {
    HEADERS += $$PWD/stacktrace_win.h
    !nogui {
        HEADERS += $$PWD/stacktrace_win_dlg.h
        FORMS += $$PWD/stacktrace_win_dlg.ui
    }
}

SOURCES += $$PWD/main.cpp
