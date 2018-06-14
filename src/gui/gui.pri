INCLUDEPATH += $$PWD

include(lineedit/lineedit.pri)
include(powermanagement/powermanagement.pri)
include(properties/properties.pri)
unix:!macx:dbus: include(qtnotify/qtnotify.pri)

HEADERS += \
    $$PWD/aboutdialog.h \
    $$PWD/addnewtorrentdialog.h \
    $$PWD/advancedsettings.h \
    $$PWD/autoexpandabledialog.h \
    $$PWD/banlistoptionsdialog.h \
    $$PWD/categoryfiltermodel.h \
    $$PWD/categoryfilterproxymodel.h \
    $$PWD/categoryfilterwidget.h \
    $$PWD/cookiesdialog.h \
    $$PWD/cookiesmodel.h \
    $$PWD/deletionconfirmationdialog.h \
    $$PWD/downloadfromurldialog.h \
    $$PWD/executionlogwidget.h \
    $$PWD/fspathedit.h \
    $$PWD/fspathedit_p.h \
    $$PWD/guiiconprovider.h \
    $$PWD/hidabletabwidget.h \
    $$PWD/ipsubnetwhitelistoptionsdialog.h \
    $$PWD/loglistwidget.h \
    $$PWD/mainwindow.h \
    $$PWD/optionsdialog.h \
    $$PWD/previewlistdelegate.h \
    $$PWD/previewselectdialog.h \
    $$PWD/raisedmessagebox.h \
    $$PWD/rss/articlelistwidget.h \
    $$PWD/rss/automatedrssdownloader.h \
    $$PWD/rss/feedlistwidget.h \
    $$PWD/rss/htmlbrowser.h \
    $$PWD/rss/rsswidget.h \
    $$PWD/scanfoldersdelegate.h \
    $$PWD/search/pluginselectdialog.h \
    $$PWD/search/pluginsourcedialog.h \
    $$PWD/search/searchjobwidget.h \
    $$PWD/search/searchlistdelegate.h \
    $$PWD/search/searchsortmodel.h \
    $$PWD/search/searchwidget.h \
    $$PWD/shutdownconfirmdialog.h \
    $$PWD/speedlimitdialog.h \
    $$PWD/statsdialog.h \
    $$PWD/statusbar.h \
    $$PWD/tagfiltermodel.h \
    $$PWD/tagfilterproxymodel.h \
    $$PWD/tagfilterwidget.h \
    $$PWD/torrentcategorydialog.h \
    $$PWD/torrentcontentfiltermodel.h \
    $$PWD/torrentcontentmodel.h \
    $$PWD/torrentcontentmodelfile.h \
    $$PWD/torrentcontentmodelfolder.h \
    $$PWD/torrentcontentmodelitem.h \
    $$PWD/torrentcontenttreeview.h \
    $$PWD/torrentcreatordialog.h \
    $$PWD/trackerlogindialog.h \
    $$PWD/transferlistdelegate.h \
    $$PWD/transferlistfilterswidget.h \
    $$PWD/transferlistmodel.h \
    $$PWD/transferlistsortmodel.h \
    $$PWD/transferlistwidget.h \
    $$PWD/updownratiodialog.h \
    $$PWD/utils.h

SOURCES += \
    $$PWD/addnewtorrentdialog.cpp \
    $$PWD/advancedsettings.cpp \
    $$PWD/autoexpandabledialog.cpp \
    $$PWD/banlistoptionsdialog.cpp \
    $$PWD/categoryfiltermodel.cpp \
    $$PWD/categoryfilterproxymodel.cpp \
    $$PWD/categoryfilterwidget.cpp \
    $$PWD/cookiesdialog.cpp \
    $$PWD/cookiesmodel.cpp \
    $$PWD/downloadfromurldialog.cpp \
    $$PWD/executionlogwidget.cpp \
    $$PWD/fspathedit.cpp \
    $$PWD/fspathedit_p.cpp \
    $$PWD/guiiconprovider.cpp \
    $$PWD/ipsubnetwhitelistoptionsdialog.cpp \
    $$PWD/loglistwidget.cpp \
    $$PWD/mainwindow.cpp \
    $$PWD/optionsdialog.cpp \
    $$PWD/previewselectdialog.cpp \
    $$PWD/raisedmessagebox.cpp \
    $$PWD/rss/articlelistwidget.cpp \
    $$PWD/rss/automatedrssdownloader.cpp \
    $$PWD/rss/feedlistwidget.cpp \
    $$PWD/rss/htmlbrowser.cpp \
    $$PWD/rss/rsswidget.cpp \
    $$PWD/scanfoldersdelegate.cpp \
    $$PWD/search/pluginselectdialog.cpp \
    $$PWD/search/pluginsourcedialog.cpp \
    $$PWD/search/searchjobwidget.cpp \
    $$PWD/search/searchlistdelegate.cpp \
    $$PWD/search/searchsortmodel.cpp \
    $$PWD/search/searchwidget.cpp \
    $$PWD/shutdownconfirmdialog.cpp \
    $$PWD/speedlimitdialog.cpp \
    $$PWD/statsdialog.cpp \
    $$PWD/statusbar.cpp \
    $$PWD/tagfiltermodel.cpp \
    $$PWD/tagfilterproxymodel.cpp \
    $$PWD/tagfilterwidget.cpp \
    $$PWD/torrentcategorydialog.cpp \
    $$PWD/torrentcontentfiltermodel.cpp \
    $$PWD/torrentcontentmodel.cpp \
    $$PWD/torrentcontentmodelfile.cpp \
    $$PWD/torrentcontentmodelfolder.cpp \
    $$PWD/torrentcontentmodelitem.cpp \
    $$PWD/torrentcontenttreeview.cpp \
    $$PWD/torrentcreatordialog.cpp \
    $$PWD/trackerlogindialog.cpp \
    $$PWD/transferlistdelegate.cpp \
    $$PWD/transferlistfilterswidget.cpp \
    $$PWD/transferlistmodel.cpp \
    $$PWD/transferlistsortmodel.cpp \
    $$PWD/transferlistwidget.cpp \
    $$PWD/updownratiodialog.cpp \
    $$PWD/utils.cpp

win32|macx {
    HEADERS += $$PWD/programupdater.h
    SOURCES += $$PWD/programupdater.cpp
}

macx {
    HEADERS += $$PWD/macutilities.h
    OBJECTIVE_SOURCES += $$PWD/macutilities.mm
}

FORMS += \
    $$PWD/aboutdialog.ui \
    $$PWD/addnewtorrentdialog.ui \
    $$PWD/autoexpandabledialog.ui \
    $$PWD/banlistoptionsdialog.ui \
    $$PWD/cookiesdialog.ui \
    $$PWD/deletionconfirmationdialog.ui \
    $$PWD/downloadfromurldialog.ui \
    $$PWD/executionlogwidget.ui \
    $$PWD/ipsubnetwhitelistoptionsdialog.ui \
    $$PWD/mainwindow.ui \
    $$PWD/optionsdialog.ui \
    $$PWD/previewselectdialog.ui \
    $$PWD/rss/automatedrssdownloader.ui \
    $$PWD/rss/rsswidget.ui \
    $$PWD/search/pluginselectdialog.ui \
    $$PWD/search/pluginsourcedialog.ui \
    $$PWD/search/searchjobwidget.ui \
    $$PWD/search/searchwidget.ui \
    $$PWD/shutdownconfirmdialog.ui \
    $$PWD/speedlimitdialog.ui \
    $$PWD/statsdialog.ui \
    $$PWD/torrentcategorydialog.ui \
    $$PWD/torrentcreatordialog.ui \
    $$PWD/trackerlogindialog.ui \
    $$PWD/updownratiodialog.ui

RESOURCES += $$PWD/about.qrc
