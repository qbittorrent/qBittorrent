INCLUDEPATH += $$PWD

include(lineedit/lineedit.pri)
include(properties/properties.pri)
include(rss/rss.pri)
include(geoip/geoip.pri)
include(powermanagement/powermanagement.pri)
unix:!macx:dbus: include(qtnotify/qtnotify.pri)

HEADERS += \
    $$PWD/mainwindow.h \
    $$PWD/transferlistwidget.h \
    $$PWD/transferlistdelegate.h \
    $$PWD/transferlistfilterswidget.h \
    $$PWD/transferlistsortmodel.h \
    $$PWD/torrentcontentmodel.h \
    $$PWD/torrentcontentmodelitem.h \
    $$PWD/torrentcontentmodelfolder.h \
    $$PWD/torrentcontentmodelfile.h \
    $$PWD/torrentcontentfiltermodel.h \
    $$PWD/torrentcontenttreeview.h \
    $$PWD/deletionconfirmationdlg.h \
    $$PWD/statusbar.h \
    $$PWD/ico.h \
    $$PWD/speedlimitdlg.h \
    $$PWD/about_imp.h \
    $$PWD/previewselect.h \
    $$PWD/previewlistdelegate.h \
    $$PWD/downloadfromurldlg.h \
    $$PWD/trackerlogin.h \
    $$PWD/hidabletabwidget.h \
    $$PWD/torrentimportdlg.h \
    $$PWD/executionlog.h \
    $$PWD/guiiconprovider.h \
    $$PWD/updownratiodlg.h \
    $$PWD/loglistwidget.h \
    $$PWD/addnewtorrentdialog.h \
    $$PWD/autoexpandabledialog.h \
    $$PWD/statsdialog.h \
    $$PWD/messageboxraised.h \
    $$PWD/options_imp.h \
    $$PWD/advancedsettings.h \
    $$PWD/shutdownconfirm.h \
    $$PWD/torrentmodel.h \
    $$PWD/torrentcreatordlg.h

SOURCES += \
    $$PWD/mainwindow.cpp \
    $$PWD/ico.cpp \
    $$PWD/transferlistwidget.cpp \
    $$PWD/transferlistsortmodel.cpp \
    $$PWD/transferlistdelegate.cpp \
    $$PWD/transferlistfilterswidget.cpp \
    $$PWD/torrentcontentmodel.cpp \
    $$PWD/torrentcontentmodelitem.cpp \
    $$PWD/torrentcontentmodelfolder.cpp \
    $$PWD/torrentcontentmodelfile.cpp \
    $$PWD/torrentcontentfiltermodel.cpp \
    $$PWD/torrentcontenttreeview.cpp \
    $$PWD/torrentimportdlg.cpp \
    $$PWD/executionlog.cpp \
    $$PWD/speedlimitdlg.cpp \
    $$PWD/previewselect.cpp \
    $$PWD/guiiconprovider.cpp \
    $$PWD/updownratiodlg.cpp \
    $$PWD/loglistwidget.cpp \
    $$PWD/addnewtorrentdialog.cpp \
    $$PWD/autoexpandabledialog.cpp \
    $$PWD/statsdialog.cpp \
    $$PWD/messageboxraised.cpp \
    $$PWD/statusbar.cpp \
    $$PWD/trackerlogin.cpp \
    $$PWD/options_imp.cpp \
    $$PWD/shutdownconfirm.cpp \
    $$PWD/torrentmodel.cpp \
    $$PWD/torrentcreatordlg.cpp

win32|macx {
    HEADERS += $$PWD/programupdater.h
    SOURCES += $$PWD/programupdater.cpp
}

FORMS += \
    $$PWD/mainwindow.ui \
    $$PWD/about.ui \
    $$PWD/preview.ui \
    $$PWD/login.ui \
    $$PWD/downloadfromurldlg.ui \
    $$PWD/bandwidth_limit.ui \
    $$PWD/updownratiodlg.ui \
    $$PWD/confirmdeletiondlg.ui \
    $$PWD/torrentimportdlg.ui \
    $$PWD/executionlog.ui \
    $$PWD/addnewtorrentdialog.ui \
    $$PWD/autoexpandabledialog.ui \
    $$PWD/statsdialog.ui \
    $$PWD/options.ui \
    $$PWD/torrentcreatordlg.ui

RESOURCES += $$PWD/about.qrc
