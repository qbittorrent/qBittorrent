INCLUDEPATH += $$PWD

include(lineedit/lineedit.pri)
include(powermanagement/powermanagement.pri)
include(properties/properties.pri)
unix:!macx:dbus: include(qtnotify/qtnotify.pri)

HEADERS += \
    $$PWD/about_imp.h \
    $$PWD/addnewtorrentdialog.h \
    $$PWD/advancedsettings.h \
    $$PWD/autoexpandabledialog.h \
    $$PWD/banlistoptions.h \
    $$PWD/categoryfiltermodel.h \
    $$PWD/categoryfilterproxymodel.h \
    $$PWD/categoryfilterwidget.h \
    $$PWD/cookiesdialog.h \
    $$PWD/cookiesmodel.h \
    $$PWD/deletionconfirmationdlg.h \
    $$PWD/downloadfromurldlg.h \
    $$PWD/executionlog.h \
    $$PWD/fspathedit.h \
    $$PWD/fspathedit_p.h \
    $$PWD/guiiconprovider.h \
    $$PWD/hidabletabwidget.h \
    $$PWD/ipsubnetwhitelistoptionsdialog.h \
    $$PWD/loglistwidget.h \
    $$PWD/mainwindow.h \
    $$PWD/messageboxraised.h \
    $$PWD/optionsdlg.h \
    $$PWD/previewlistdelegate.h \
    $$PWD/previewselectdialog.h \
    $$PWD/rss/articlelistwidget.h \
    $$PWD/rss/automatedrssdownloader.h \
    $$PWD/rss/feedlistwidget.h \
    $$PWD/rss/htmlbrowser.h \
    $$PWD/rss/rsswidget.h \
    $$PWD/scanfoldersdelegate.h \
    $$PWD/search/pluginselectdlg.h \
    $$PWD/search/pluginsourcedlg.h \
    $$PWD/search/searchlistdelegate.h \
    $$PWD/search/searchsortmodel.h \
    $$PWD/search/searchtab.h \
    $$PWD/search/searchwidget.h \
    $$PWD/shutdownconfirmdlg.h \
    $$PWD/speedlimitdlg.h \
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
    $$PWD/torrentcreatordlg.h \
    $$PWD/torrentmodel.h \
    $$PWD/trackerlogin.h \
    $$PWD/transferlistdelegate.h \
    $$PWD/transferlistfilterswidget.h \
    $$PWD/transferlistsortmodel.h \
    $$PWD/transferlistwidget.h \
    $$PWD/updownratiodlg.h \
    $$PWD/utils.h

SOURCES += \
    $$PWD/addnewtorrentdialog.cpp \
    $$PWD/advancedsettings.cpp \
    $$PWD/autoexpandabledialog.cpp \
    $$PWD/banlistoptions.cpp \
    $$PWD/categoryfiltermodel.cpp \
    $$PWD/categoryfilterproxymodel.cpp \
    $$PWD/categoryfilterwidget.cpp \
    $$PWD/cookiesdialog.cpp \
    $$PWD/cookiesmodel.cpp \
    $$PWD/executionlog.cpp \
    $$PWD/fspathedit.cpp \
    $$PWD/fspathedit_p.cpp \
    $$PWD/guiiconprovider.cpp \
    $$PWD/ipsubnetwhitelistoptionsdialog.cpp \
    $$PWD/loglistwidget.cpp \
    $$PWD/mainwindow.cpp \
    $$PWD/messageboxraised.cpp \
    $$PWD/optionsdlg.cpp \
    $$PWD/previewselectdialog.cpp \
    $$PWD/rss/articlelistwidget.cpp \
    $$PWD/rss/automatedrssdownloader.cpp \
    $$PWD/rss/feedlistwidget.cpp \
    $$PWD/rss/htmlbrowser.cpp \
    $$PWD/rss/rsswidget.cpp \
    $$PWD/scanfoldersdelegate.cpp \
    $$PWD/search/pluginselectdlg.cpp \
    $$PWD/search/pluginsourcedlg.cpp \
    $$PWD/search/searchlistdelegate.cpp \
    $$PWD/search/searchsortmodel.cpp \
    $$PWD/search/searchtab.cpp \
    $$PWD/search/searchwidget.cpp \
    $$PWD/shutdownconfirmdlg.cpp \
    $$PWD/speedlimitdlg.cpp \
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
    $$PWD/torrentcreatordlg.cpp \
    $$PWD/torrentmodel.cpp \
    $$PWD/trackerlogin.cpp \
    $$PWD/transferlistdelegate.cpp \
    $$PWD/transferlistfilterswidget.cpp \
    $$PWD/transferlistsortmodel.cpp \
    $$PWD/transferlistwidget.cpp \
    $$PWD/updownratiodlg.cpp \
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
    $$PWD/about.ui \
    $$PWD/addnewtorrentdialog.ui \
    $$PWD/autoexpandabledialog.ui \
    $$PWD/bandwidth_limit.ui \
    $$PWD/banlistoptions.ui \
    $$PWD/confirmdeletiondlg.ui \
    $$PWD/cookiesdialog.ui \
    $$PWD/downloadfromurldlg.ui \
    $$PWD/executionlog.ui \
    $$PWD/ipsubnetwhitelistoptionsdialog.ui \
    $$PWD/login.ui \
    $$PWD/mainwindow.ui \
    $$PWD/optionsdlg.ui \
    $$PWD/previewselectdialog.ui \
    $$PWD/rss/automatedrssdownloader.ui \
    $$PWD/rss/rsswidget.ui \
    $$PWD/search/pluginselectdlg.ui \
    $$PWD/search/pluginsourcedlg.ui \
    $$PWD/search/searchtab.ui \
    $$PWD/search/searchwidget.ui \
    $$PWD/shutdownconfirmdlg.ui \
    $$PWD/statsdialog.ui \
    $$PWD/torrentcategorydialog.ui \
    $$PWD/torrentcreatordlg.ui \
    $$PWD/updownratiodlg.ui

RESOURCES += $$PWD/about.qrc
