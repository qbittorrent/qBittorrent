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
    $$PWD/cmdoptions.h \
    $$PWD/filelogger.h \
    $$PWD/upgrade.h

SOURCES += \
    $$PWD/application.cpp \
    $$PWD/cmdoptions.cpp \
    $$PWD/filelogger.cpp \
    $$PWD/main.cpp \
    $$PWD/upgrade.cpp

stacktrace {
    unix {
        HEADERS += $$PWD/stacktrace.h
    }
    else {
        HEADERS += $$PWD/stacktrace_win.h
        !nogui {
            HEADERS += $$PWD/stacktracedialog.h
            FORMS += $$PWD/stacktracedialog.ui
        }
    }
}
