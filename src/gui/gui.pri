INCLUDEPATH += $$PWD

include(lineedit/lineedit.pri)
include(properties/properties.pri)
include(powermanagement/powermanagement.pri)
unix:!macx:dbus: include(qtnotify/qtnotify.pri)

HEADERS += \
    $$PWD/mainwindow.h \
    $$PWD/transferlistwidget.h \
    $$PWD/transferlistdelegate.h \
    $$PWD/transferlistfilterswidget.h \
    $$PWD/transferlistsortmodel.h \
    $$PWD/torrentcategorydialog.h \
    $$PWD/torrentcontentmodel.h \
    $$PWD/torrentcontentmodelitem.h \
    $$PWD/torrentcontentmodelfolder.h \
    $$PWD/torrentcontentmodelfile.h \
    $$PWD/torrentcontentfiltermodel.h \
    $$PWD/torrentcontenttreeview.h \
    $$PWD/deletionconfirmationdlg.h \
    $$PWD/statusbar.h \
    $$PWD/speedlimitdlg.h \
    $$PWD/about_imp.h \
    $$PWD/previewlistdelegate.h \
    $$PWD/downloadfromurldlg.h \
    $$PWD/trackerlogin.h \
    $$PWD/hidabletabwidget.h \
    $$PWD/executionlog.h \
    $$PWD/guiiconprovider.h \
    $$PWD/updownratiodlg.h \
    $$PWD/loglistwidget.h \
    $$PWD/addnewtorrentdialog.h \
    $$PWD/autoexpandabledialog.h \
    $$PWD/statsdialog.h \
    $$PWD/messageboxraised.h \
    $$PWD/optionsdlg.h \
    $$PWD/advancedsettings.h \
    $$PWD/shutdownconfirmdlg.h \
    $$PWD/torrentmodel.h \
    $$PWD/torrentcreatordlg.h \
    $$PWD/scanfoldersdelegate.h \
    $$PWD/search/searchwidget.h \
    $$PWD/search/searchtab.h \
    $$PWD/search/pluginselectdlg.h \
    $$PWD/search/pluginsourcedlg.h \
    $$PWD/search/searchlistdelegate.h \
    $$PWD/search/searchsortmodel.h \
    $$PWD/cookiesmodel.h \
    $$PWD/cookiesdialog.h \
    $$PWD/categoryfiltermodel.h \
    $$PWD/categoryfilterproxymodel.h \
    $$PWD/categoryfilterwidget.h \
    $$PWD/tagfiltermodel.h \
    $$PWD/tagfilterproxymodel.h \
    $$PWD/tagfilterwidget.h \
    $$PWD/banlistoptions.h \
    $$PWD/rss/rsswidget.h \
    $$PWD/rss/articlelistwidget.h \
    $$PWD/rss/feedlistwidget.h \
    $$PWD/rss/automatedrssdownloader.h \
    $$PWD/rss/htmlbrowser.h \
    $$PWD/fspathedit.h \
    $$PWD/fspathedit_p.h \
    $$PWD/previewselectdialog.h \
    $$PWD/theme/colorprovider_p.h \
    $$PWD/theme/colorproviders.h \
    $$PWD/theme/colortheme.h \
    $$PWD/theme/fontprovider_p.h \
    $$PWD/theme/fontproviders.h \
    $$PWD/theme/fonttheme.h \
    $$PWD/theme/provider_p.h \
    $$PWD/theme/serializabletheme.h \
    $$PWD/theme/serializablecolortheme.h \
    $$PWD/theme/serializablefonttheme.h \
    $$PWD/theme/themecommon.h \
    $$PWD/theme/themeexceptions.h \
    $$PWD/theme/themeinfo.h \
    $$PWD/theme/themeprovider.h \

SOURCES += \
    $$PWD/mainwindow.cpp \
    $$PWD/transferlistwidget.cpp \
    $$PWD/transferlistsortmodel.cpp \
    $$PWD/transferlistdelegate.cpp \
    $$PWD/transferlistfilterswidget.cpp \
    $$PWD/torrentcategorydialog.cpp \
    $$PWD/torrentcontentmodel.cpp \
    $$PWD/torrentcontentmodelitem.cpp \
    $$PWD/torrentcontentmodelfolder.cpp \
    $$PWD/torrentcontentmodelfile.cpp \
    $$PWD/torrentcontentfiltermodel.cpp \
    $$PWD/torrentcontenttreeview.cpp \
    $$PWD/executionlog.cpp \
    $$PWD/speedlimitdlg.cpp \
    $$PWD/guiiconprovider.cpp \
    $$PWD/updownratiodlg.cpp \
    $$PWD/loglistwidget.cpp \
    $$PWD/addnewtorrentdialog.cpp \
    $$PWD/autoexpandabledialog.cpp \
    $$PWD/statsdialog.cpp \
    $$PWD/messageboxraised.cpp \
    $$PWD/statusbar.cpp \
    $$PWD/advancedsettings.cpp \
    $$PWD/trackerlogin.cpp \
    $$PWD/optionsdlg.cpp \
    $$PWD/shutdownconfirmdlg.cpp \
    $$PWD/torrentmodel.cpp \
    $$PWD/torrentcreatordlg.cpp \
    $$PWD/scanfoldersdelegate.cpp \
    $$PWD/search/searchwidget.cpp \
    $$PWD/search/searchtab.cpp \
    $$PWD/search/pluginselectdlg.cpp \
    $$PWD/search/pluginsourcedlg.cpp \
    $$PWD/search/searchlistdelegate.cpp \
    $$PWD/search/searchsortmodel.cpp \
    $$PWD/cookiesmodel.cpp \
    $$PWD/cookiesdialog.cpp \
    $$PWD/categoryfiltermodel.cpp \
    $$PWD/categoryfilterproxymodel.cpp \
    $$PWD/categoryfilterwidget.cpp \
    $$PWD/tagfiltermodel.cpp \
    $$PWD/tagfilterproxymodel.cpp \
    $$PWD/tagfilterwidget.cpp \
    $$PWD/banlistoptions.cpp \
    $$PWD/rss/rsswidget.cpp \
    $$PWD/rss/articlelistwidget.cpp \
    $$PWD/rss/feedlistwidget.cpp \
    $$PWD/rss/automatedrssdownloader.cpp \
    $$PWD/rss/htmlbrowser.cpp \
    $$PWD/fspathedit.cpp \
    $$PWD/fspathedit_p.cpp \
    $$PWD/previewselectdialog.cpp \
    $$PWD/theme/colorprovider_p.cpp \
    $$PWD/theme/colortheme.cpp \
    $$PWD/theme/fonttheme.cpp \
    $$PWD/theme/fontprovider_p.cpp \
    $$PWD/theme/colorproviders.cpp \
    $$PWD/theme/fontproviders.cpp \
    $$PWD/theme/provider_p.cpp \
    $$PWD/theme/serializabletheme.cpp \
    $$PWD/theme/serializablecolortheme.cpp \
    $$PWD/theme/serializablefonttheme.cpp \
    $$PWD/theme/themecommon.cpp \
    $$PWD/theme/themeexceptions.cpp \
    $$PWD/theme/themeinfo.cpp \
    $$PWD/theme/themeprovider.cpp \

win32|macx {
    HEADERS += $$PWD/programupdater.h
    SOURCES += $$PWD/programupdater.cpp
}

macx {
    HEADERS += $$PWD/macutilities.h
    OBJECTIVE_SOURCES += $$PWD/macutilities.mm
}

plasma_integration {
    HEADERS += $$PWD/theme/plasmacolorprovider.h \
               $$PWD/utils/colorutils.h \
               $$PWD/utils/plasmacolorscheme.h
    SOURCES += $$PWD/theme/plasmacolorprovider.cpp \
               $$PWD/utils/colorutils.cpp \
               $$PWD/utils/plasmacolorscheme.cpp
}

FORMS += \
    $$PWD/mainwindow.ui \
    $$PWD/about.ui \
    $$PWD/previewselectdialog.ui \
    $$PWD/login.ui \
    $$PWD/downloadfromurldlg.ui \
    $$PWD/bandwidth_limit.ui \
    $$PWD/updownratiodlg.ui \
    $$PWD/confirmdeletiondlg.ui \
    $$PWD/shutdownconfirmdlg.ui \
    $$PWD/executionlog.ui \
    $$PWD/addnewtorrentdialog.ui \
    $$PWD/autoexpandabledialog.ui \
    $$PWD/statsdialog.ui \
    $$PWD/optionsdlg.ui \
    $$PWD/torrentcreatordlg.ui \
    $$PWD/search/searchwidget.ui \
    $$PWD/search/pluginselectdlg.ui \
    $$PWD/search/pluginsourcedlg.ui \
    $$PWD/search/searchtab.ui \
    $$PWD/cookiesdialog.ui \
    $$PWD/banlistoptions.ui \
    $$PWD/rss/rsswidget.ui \
    $$PWD/rss/automatedrssdownloader.ui \
    $$PWD/torrentcategorydialog.ui

RESOURCES += \
    $$PWD/about.qrc \
    $$PWD/theme/builtinthemes.qrc
