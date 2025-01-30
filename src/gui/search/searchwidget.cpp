/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015-2025  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2020, Will Da Silva <will@willdasilva.xyz>
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */

#include "searchwidget.h"

#include <QtSystemDetection>

#include <utility>

#include <QCompleter>
#include <QDebug>
#include <QEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QList>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QObject>
#include <QRegularExpression>
#include <QShortcut>
#include <QSortFilterProxyModel>
#include <QStringList>
#include <QStringListModel>
#include <QThread>

#include "base/global.h"
#include "base/logger.h"
#include "base/preferences.h"
#include "base/profile.h"
#include "base/search/searchhandler.h"
#include "base/search/searchpluginmanager.h"
#include "base/utils/bytearray.h"
#include "base/utils/compare.h"
#include "base/utils/datetime.h"
#include "base/utils/fs.h"
#include "base/utils/foreignapps.h"
#include "base/utils/io.h"
#include "gui/desktopintegration.h"
#include "gui/interfaces/iguiapplication.h"
#include "gui/uithememanager.h"
#include "pluginselectdialog.h"
#include "searchjobwidget.h"
#include "ui_searchwidget.h"

const int HISTORY_FILE_MAX_SIZE = 10 * 1024 * 1024;
const int SESSION_FILE_MAX_SIZE = 10 * 1024 * 1024;
const int RESULTS_FILE_MAX_SIZE = 10 * 1024 * 1024;

const QString DATA_FOLDER_NAME = u"SearchUI"_s;
const QString HISTORY_FILE_NAME = u"History.txt"_s;
const QString SESSION_FILE_NAME = u"Session.json"_s;

const QString KEY_SESSION_TABS = u"Tabs"_s;
const QString KEY_SESSION_CURRENTTAB = u"CurrentTab"_s;
const QString KEY_TAB_ID = u"ID"_s;
const QString KEY_TAB_SEARCHPATTERN = u"SearchPattern"_s;
const QString KEY_RESULT_FILENAME = u"FileName"_s;
const QString KEY_RESULT_FILEURL = u"FileURL"_s;
const QString KEY_RESULT_FILESIZE = u"FileSize"_s;
const QString KEY_RESULT_SEEDERSCOUNT = u"SeedersCount"_s;
const QString KEY_RESULT_LEECHERSCOUNT = u"LeechersCount"_s;
const QString KEY_RESULT_ENGINENAME = u"EngineName"_s;
const QString KEY_RESULT_SITEURL = u"SiteURL"_s;
const QString KEY_RESULT_DESCRLINK = u"DescrLink"_s;
const QString KEY_RESULT_PUBDATE = u"PubDate"_s;

namespace
{
    class SearchHistorySortModel final : public QSortFilterProxyModel
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(SearchHistorySortModel)

    public:
        using QSortFilterProxyModel::QSortFilterProxyModel;

    private:
        bool lessThan(const QModelIndex &left, const QModelIndex &right) const override
        {
            const int result = m_naturalCompare(left.data(sortRole()).toString(), right.data(sortRole()).toString());
            return result < 0;
        }

        Utils::Compare::NaturalCompare<Qt::CaseInsensitive> m_naturalCompare;
    };

    struct TabData
    {
        QString tabID;
        QString searchPattern;
    };

    struct SessionData
    {
        QList<TabData> tabs;
        QString currentTabID;
    };

    QString statusIconName(const SearchJobWidget::Status st)
    {
        switch (st)
        {
        case SearchJobWidget::Status::Ongoing:
            return u"queued"_s;
        case SearchJobWidget::Status::Finished:
            return u"task-complete"_s;
        case SearchJobWidget::Status::Aborted:
            return u"task-reject"_s;
        case SearchJobWidget::Status::Error:
        case SearchJobWidget::Status::NoResults:
            return u"dialog-warning"_s;
        default:
            return {};
        }
    }

    Path makeDataFilePath(const QString &fileName)
    {
        return specialFolderLocation(SpecialFolder::Data) / Path(DATA_FOLDER_NAME) / Path(fileName);
    }

    QString makeTabName(SearchJobWidget *searchJobWdget)
    {
        Q_ASSERT(searchJobWdget);
        if (!searchJobWdget) [[unlikely]]
            return {};

        QString tabName = searchJobWdget->searchPattern();
        tabName.replace(QRegularExpression(u"&{1}"_s), u"&&"_s);
        return tabName;
    }

    nonstd::expected<QStringList, QString> loadHistory(const Path &filePath)
    {
        const auto readResult = Utils::IO::readFile(filePath, HISTORY_FILE_MAX_SIZE);
        if (!readResult)
        {
            if (readResult.error().status == Utils::IO::ReadError::NotExist)
                return {};

            return nonstd::make_unexpected(readResult.error().message);
        }

        QStringList history;
        for (const QByteArrayView line : asConst(Utils::ByteArray::splitToViews(readResult.value(), "\n")))
            history.append(QString::fromUtf8(line));

        return history;
    }

    nonstd::expected<SessionData, QString> loadSession(const Path &filePath)
    {
        const auto readResult = Utils::IO::readFile(filePath, SESSION_FILE_MAX_SIZE);
        if (!readResult)
        {
            if (readResult.error().status == Utils::IO::ReadError::NotExist)
                return {};

            return nonstd::make_unexpected(readResult.error().message);
        }

        const QString formatErrorMsg = SearchWidget::tr("Invalid data format.");
        QJsonParseError jsonError;
        const QJsonDocument sessionDoc = QJsonDocument::fromJson(readResult.value(), &jsonError);
        if (jsonError.error != QJsonParseError::NoError)
            return nonstd::make_unexpected(jsonError.errorString());

        if (!sessionDoc.isObject())
            return nonstd::make_unexpected(formatErrorMsg);

        const QJsonObject sessionObj = sessionDoc.object();
        const QJsonValue tabsVal = sessionObj[KEY_SESSION_TABS];
        if (!tabsVal.isArray())
            return nonstd::make_unexpected(formatErrorMsg);

        QList<TabData> tabs;
        QSet<QString> tabIDs;
        for (const QJsonValue &tabVal : asConst(tabsVal.toArray()))
        {
            if (!tabVal.isObject())
                return nonstd::make_unexpected(formatErrorMsg);

            const QJsonObject tabObj = tabVal.toObject();

            const QJsonValue tabIDVal = tabObj[KEY_TAB_ID];
            if (!tabIDVal.isString())
                return nonstd::make_unexpected(formatErrorMsg);

            const QJsonValue patternVal = tabObj[KEY_TAB_SEARCHPATTERN];
            if (!patternVal.isString())
                return nonstd::make_unexpected(formatErrorMsg);

            const QString tabID = tabIDVal.toString();
            tabIDs.insert(tabID);
            tabs.emplaceBack(TabData {tabID, patternVal.toString()});
            if (tabs.size() != tabIDs.size()) // duplicate ID
                return nonstd::make_unexpected(formatErrorMsg);
        }

        const QJsonValue currentTabVal = sessionObj[KEY_SESSION_CURRENTTAB];
        if (!currentTabVal.isString())
            return nonstd::make_unexpected(formatErrorMsg);

        return SessionData {.tabs = tabs, .currentTabID = currentTabVal.toString()};
    }

    nonstd::expected<QList<SearchResult>, QString> loadSearchResults(const Path &filePath)
    {
        const auto readResult = Utils::IO::readFile(filePath, RESULTS_FILE_MAX_SIZE);
        if (!readResult)
        {
            if (readResult.error().status != Utils::IO::ReadError::NotExist)
            {
                return nonstd::make_unexpected(readResult.error().message);
            }

            return {};
        }

        const QString formatErrorMsg = SearchWidget::tr("Invalid data format.");
        QJsonParseError jsonError;
        const QJsonDocument searchResultsDoc = QJsonDocument::fromJson(readResult.value(), &jsonError);
        if (jsonError.error != QJsonParseError::NoError)
            return nonstd::make_unexpected(jsonError.errorString());

        if (!searchResultsDoc.isArray())
            return nonstd::make_unexpected(formatErrorMsg);

        const QJsonArray resultsList = searchResultsDoc.array();
        QList<SearchResult> searchResults;
        for (const QJsonValue &resultVal : resultsList)
        {
            if (!resultVal.isObject())
                return nonstd::make_unexpected(formatErrorMsg);

            const QJsonObject resultObj = resultVal.toObject();
            SearchResult &searchResult = searchResults.emplaceBack();

            if (const QJsonValue fileNameVal = resultObj[KEY_RESULT_FILENAME]; fileNameVal.isString())
                searchResult.fileName = fileNameVal.toString();
            else
                return nonstd::make_unexpected(formatErrorMsg);

            if (const QJsonValue fileURLVal = resultObj[KEY_RESULT_FILEURL]; fileURLVal.isString())
                searchResult.fileUrl= fileURLVal.toString();
            else
                return nonstd::make_unexpected(formatErrorMsg);

            if (const QJsonValue fileSizeVal = resultObj[KEY_RESULT_FILESIZE]; fileSizeVal.isDouble())
                searchResult.fileSize= fileSizeVal.toInteger();
            else
                return nonstd::make_unexpected(formatErrorMsg);

            if (const QJsonValue seedersCountVal = resultObj[KEY_RESULT_SEEDERSCOUNT]; seedersCountVal.isDouble())
                searchResult.nbSeeders = seedersCountVal.toInteger();
            else
                return nonstd::make_unexpected(formatErrorMsg);

            if (const QJsonValue leechersCountVal = resultObj[KEY_RESULT_LEECHERSCOUNT]; leechersCountVal.isDouble())
                searchResult.nbLeechers = leechersCountVal.toInteger();
            else
                return nonstd::make_unexpected(formatErrorMsg);

            if (const QJsonValue engineNameVal = resultObj[KEY_RESULT_ENGINENAME]; engineNameVal.isString())
                searchResult.engineName= engineNameVal.toString();
            else
                return nonstd::make_unexpected(formatErrorMsg);

            if (const QJsonValue siteURLVal = resultObj[KEY_RESULT_SITEURL]; siteURLVal.isString())
                searchResult.siteUrl= siteURLVal.toString();
            else
                return nonstd::make_unexpected(formatErrorMsg);

            if (const QJsonValue descrLinkVal = resultObj[KEY_RESULT_DESCRLINK]; descrLinkVal.isString())
                searchResult.descrLink= descrLinkVal.toString();
            else
                return nonstd::make_unexpected(formatErrorMsg);

            if (const QJsonValue pubDateVal = resultObj[KEY_RESULT_PUBDATE]; pubDateVal.isDouble())
                searchResult.pubDate = QDateTime::fromSecsSinceEpoch(pubDateVal.toInteger());
            else
                return nonstd::make_unexpected(formatErrorMsg);
        }

        return searchResults;
    }
}

class SearchWidget::DataStorage final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(DataStorage)

public:
    using QObject::QObject;

    void loadSession(bool withSearchResults);
    void storeSession(const SessionData &sessionData);
    void removeSession();
    void storeTab(const QString &tabID, const QList<SearchResult> &searchResults);
    void removeTab(const QString &tabID);
    void loadHistory();
    void storeHistory(const QStringList &history);
    void removeHistory();

signals:
    void historyLoaded(const QStringList &history);
    void sessionLoaded(const SessionData &sessionData);
    void tabLoaded(const QString &tabID, const QString &searchPattern, const QList<SearchResult> &searchResults);
};

SearchWidget::SearchWidget(IGUIApplication *app, QWidget *parent)
    : GUIApplicationComponent(app, parent)
    , m_ui {new Ui::SearchWidget()}
    , m_ioThread {new QThread}
    , m_dataStorage {new DataStorage}
{
    m_ui->setupUi(this);

    m_ui->stopButton->hide();
    m_ui->tabWidget->tabBar()->installEventFilter(this);

    const QString searchPatternHint = u"<html><head/><body><p>"
        + tr("A phrase to search for.") + u"<br>"
        + tr("Spaces in a search term may be protected by double quotes.")
        + u"</p><p>"
        + tr("Example:", "Search phrase example")
        + u"<br>"
        + tr("<b>foo bar</b>: search for <b>foo</b> and <b>bar</b>",
                 "Search phrase example, illustrates quotes usage, a pair of "
                 "space delimited words, individual words are highlighted")
        + u"<br>"
        + tr("<b>&quot;foo bar&quot;</b>: search for <b>foo bar</b>",
                 "Search phrase example, illustrates quotes usage, double quoted"
                 "pair of space delimited words, the whole pair is highlighted")
        + u"</p></body></html>";
    m_ui->lineEditSearchPattern->setToolTip(searchPatternHint);

#ifndef Q_OS_MACOS
    // Icons
    m_ui->searchButton->setIcon(UIThemeManager::instance()->getIcon(u"edit-find"_s));
    m_ui->pluginsButton->setIcon(UIThemeManager::instance()->getIcon(u"plugins"_s, u"preferences-system-network"_s));
#else
    // On macOS the icons overlap the text otherwise
    QSize iconSize = m_ui->tabWidget->iconSize();
    iconSize.setWidth(iconSize.width() + 16);
    m_ui->tabWidget->setIconSize(iconSize);
#endif
    connect(m_ui->tabWidget, &QTabWidget::tabCloseRequested, this, &SearchWidget::closeTab);
    connect(m_ui->tabWidget, &QTabWidget::currentChanged, this, &SearchWidget::currentTabChanged);
    connect(m_ui->tabWidget, &QTabWidget::currentChanged, this, &SearchWidget::saveSession);
    connect(m_ui->tabWidget->tabBar(), &QTabBar::tabMoved, this, &SearchWidget::saveSession);

    connect(m_ui->tabWidget, &QTabWidget::tabBarDoubleClicked, this, [this](const int tabIndex)
    {
        if (tabIndex < 0)
            return;

        // Reset current search pattern
        auto *searchJobWidget = static_cast<SearchJobWidget *>(m_ui->tabWidget->widget(tabIndex));
        const QString pattern = searchJobWidget->searchPattern();
        if (pattern != m_ui->lineEditSearchPattern->text())
        {
            m_ui->lineEditSearchPattern->setText(pattern);
            m_isNewQueryString = false;
            adjustSearchButton();
        }
    });

    const auto *searchManager = SearchPluginManager::instance();
    const auto onPluginChanged = [this]()
    {
        fillPluginComboBox();
        fillCatCombobox();
        selectActivePage();
    };
    connect(searchManager, &SearchPluginManager::pluginInstalled, this, onPluginChanged);
    connect(searchManager, &SearchPluginManager::pluginUninstalled, this, onPluginChanged);
    connect(searchManager, &SearchPluginManager::pluginUpdated, this, onPluginChanged);
    connect(searchManager, &SearchPluginManager::pluginEnabled, this, onPluginChanged);

    // Fill in category combobox
    onPluginChanged();

    connect(m_ui->pluginsButton, &QPushButton::clicked, this, &SearchWidget::pluginsButtonClicked);
    connect(m_ui->searchButton, &QPushButton::clicked, this, &SearchWidget::searchButtonClicked);
    connect(m_ui->stopButton, &QPushButton::clicked, this, &SearchWidget::stopButtonClicked);
    connect(m_ui->lineEditSearchPattern, &LineEdit::returnPressed, m_ui->searchButton, &QPushButton::click);
    connect(m_ui->lineEditSearchPattern, &LineEdit::textEdited, this, &SearchWidget::searchTextEdited);
    connect(m_ui->selectPlugin, qOverload<int>(&QComboBox::currentIndexChanged)
            , this, &SearchWidget::selectMultipleBox);
    connect(m_ui->selectPlugin, qOverload<int>(&QComboBox::currentIndexChanged)
            , this, &SearchWidget::fillCatCombobox);

    const auto *focusSearchHotkey = new QShortcut(QKeySequence::Find, this);
    connect(focusSearchHotkey, &QShortcut::activated, this, &SearchWidget::toggleFocusBetweenLineEdits);
    const auto *focusSearchHotkeyAlternative = new QShortcut((Qt::CTRL | Qt::Key_E), this);
    connect(focusSearchHotkeyAlternative, &QShortcut::activated, this, &SearchWidget::toggleFocusBetweenLineEdits);

    m_historyLength = Preferences::instance()->searchHistoryLength();
    m_storeOpenedTabs = Preferences::instance()->storeOpenedSearchTabs();
    m_storeOpenedTabsResults = Preferences::instance()->storeOpenedSearchTabResults();
    connect(Preferences::instance(), &Preferences::changed, this, &SearchWidget::onPreferencesChanged);

    m_dataStorage->moveToThread(m_ioThread.get());
    connect(m_ioThread.get(), &QThread::finished, m_dataStorage, &QObject::deleteLater);
    m_ioThread->setObjectName("SearchWidget m_ioThread");
    m_ioThread->start();

    loadHistory();
    restoreSession();
}

bool SearchWidget::eventFilter(QObject *object, QEvent *event)
{
    if (object == m_ui->tabWidget->tabBar())
    {
        // Close tabs when middle-clicked
        if (event->type() != QEvent::MouseButtonRelease)
            return false;

        const auto *mouseEvent = static_cast<QMouseEvent *>(event);
        const int tabIndex = m_ui->tabWidget->tabBar()->tabAt(mouseEvent->pos());
        if (tabIndex >= 0)
        {
            if (mouseEvent->button() == Qt::MiddleButton)
            {
                closeTab(tabIndex);
                return true;
            }

            if (mouseEvent->button() == Qt::RightButton)
            {
                showTabMenu(tabIndex);
                return true;
            }
        }

        return false;
    }

    return QWidget::eventFilter(object, event);
}

void SearchWidget::onPreferencesChanged()
{
    const auto *pref = Preferences::instance();

    const bool storeOpenedTabs = pref->storeOpenedSearchTabs();
    const bool isStoreOpenedTabsChanged = storeOpenedTabs != m_storeOpenedTabs;
    if (isStoreOpenedTabsChanged)
    {
        m_storeOpenedTabs = storeOpenedTabs;
        if (m_storeOpenedTabs)
        {
            saveSession();
        }
        else
        {
            QMetaObject::invokeMethod(m_dataStorage, &SearchWidget::DataStorage::removeSession);
        }
    }


    const bool storeOpenedTabsResults = pref->storeOpenedSearchTabResults();
    const bool isStoreOpenedTabsResultsChanged = storeOpenedTabsResults != m_storeOpenedTabsResults;
    if (isStoreOpenedTabsResultsChanged)
        m_storeOpenedTabsResults = storeOpenedTabsResults;

    if (isStoreOpenedTabsResultsChanged || isStoreOpenedTabsChanged)
    {
        if (m_storeOpenedTabsResults)
        {
            for (int tabIndex = (m_ui->tabWidget->count() - 1); tabIndex >= 0; --tabIndex)
            {
                const auto *tab = static_cast<SearchJobWidget *>(m_ui->tabWidget->widget(tabIndex));
                QMetaObject::invokeMethod(m_dataStorage, [this, tabID = tab->id(), searchResults = tab->searchResults()]
                {
                    m_dataStorage->storeTab(tabID, searchResults);
                });
            }
        }
        else
        {
            for (int tabIndex = (m_ui->tabWidget->count() - 1); tabIndex >= 0; --tabIndex)
            {
                const auto *tab = static_cast<SearchJobWidget *>(m_ui->tabWidget->widget(tabIndex));
                QMetaObject::invokeMethod(m_dataStorage, [this, tabID = tab->id()] { m_dataStorage->removeTab(tabID); });
            }
        }
    }

    const int historyLength = pref->searchHistoryLength();
    if (historyLength != m_historyLength)
    {
        if (m_historyLength <= 0)
        {
            createSearchPatternCompleter();
        }
        else
        {
            if (historyLength <= 0)
            {
                m_searchPatternCompleterModel->removeRows(0, m_searchPatternCompleterModel->rowCount());
                QMetaObject::invokeMethod(m_dataStorage, &SearchWidget::DataStorage::removeHistory);
            }
            else if (historyLength < m_historyLength)
            {
                if (const int rowCount = m_searchPatternCompleterModel->rowCount(); rowCount > historyLength)
                {
                    m_searchPatternCompleterModel->removeRows(0, (rowCount - historyLength));
                    QMetaObject::invokeMethod(m_dataStorage, [this]
                    {
                        m_dataStorage->storeHistory(m_searchPatternCompleterModel->stringList());
                    });
                }
            }
        }

        m_historyLength = historyLength;
    }
}

void SearchWidget::fillCatCombobox()
{
    m_ui->comboCategory->clear();
    m_ui->comboCategory->addItem(SearchPluginManager::categoryFullName(u"all"_s), u"all"_s);

    using QStrPair = std::pair<QString, QString>;
    QList<QStrPair> tmpList;
    const auto selectedPlugin = m_ui->selectPlugin->itemData(m_ui->selectPlugin->currentIndex()).toString();
    for (const QString &cat : asConst(SearchPluginManager::instance()->getPluginCategories(selectedPlugin)))
        tmpList << std::make_pair(SearchPluginManager::categoryFullName(cat), cat);
    std::sort(tmpList.begin(), tmpList.end(), [](const QStrPair &l, const QStrPair &r) { return (QString::localeAwareCompare(l.first, r.first) < 0); });

    for (const QStrPair &p : asConst(tmpList))
    {
        qDebug("Supported category: %s", qUtf8Printable(p.second));
        m_ui->comboCategory->addItem(p.first, p.second);
    }

    if (m_ui->comboCategory->count() > 1)
        m_ui->comboCategory->insertSeparator(1);
}

void SearchWidget::fillPluginComboBox()
{
    m_ui->selectPlugin->clear();
    m_ui->selectPlugin->addItem(tr("Only enabled"), u"enabled"_s);
    m_ui->selectPlugin->addItem(tr("All plugins"), u"all"_s);
    m_ui->selectPlugin->addItem(tr("Select..."), u"multi"_s);

    using QStrPair = std::pair<QString, QString>;
    QList<QStrPair> tmpList;
    for (const QString &name : asConst(SearchPluginManager::instance()->enabledPlugins()))
        tmpList << std::make_pair(SearchPluginManager::instance()->pluginFullName(name), name);
    std::sort(tmpList.begin(), tmpList.end(), [](const QStrPair &l, const QStrPair &r) { return (l.first < r.first); } );

    for (const QStrPair &p : asConst(tmpList))
        m_ui->selectPlugin->addItem(p.first, p.second);

    if (m_ui->selectPlugin->count() > 3)
        m_ui->selectPlugin->insertSeparator(3);
}

QString SearchWidget::selectedCategory() const
{
    return m_ui->comboCategory->itemData(m_ui->comboCategory->currentIndex()).toString();
}

QStringList SearchWidget::selectedPlugins() const
{
    const auto itemText = m_ui->selectPlugin->itemData(m_ui->selectPlugin->currentIndex()).toString();

    if (itemText == u"all")
        return SearchPluginManager::instance()->allPlugins();

    if ((itemText == u"enabled") || (itemText == u"multi"))
        return SearchPluginManager::instance()->enabledPlugins();

    return {itemText};
}

QString SearchWidget::generateTabID() const
{
    for (;;)
    {
        const QString tabID = QString::number(qHash(QDateTime::currentDateTimeUtc()));
        if (!m_tabWidgets.contains(tabID))
            return tabID;
    }

    return {};
}

int SearchWidget::addTab(const QString &tabID, SearchJobWidget *searchJobWdget)
{
    Q_ASSERT(!m_tabWidgets.contains(tabID));

    connect(searchJobWdget, &SearchJobWidget::statusChanged, this, [this, searchJobWdget]() { tabStatusChanged(searchJobWdget); });
    m_tabWidgets.insert(tabID, searchJobWdget);
    return m_ui->tabWidget->addTab(searchJobWdget, makeTabName(searchJobWdget));
}

void SearchWidget::updateHistory(const QString &newSearchPattern)
{
    if (m_historyLength <= 0)
        return;

    if (m_searchPatternCompleterModel->stringList().contains(newSearchPattern))
        return;

    const int rowNum = m_searchPatternCompleterModel->rowCount();
    m_searchPatternCompleterModel->insertRow(rowNum);
    m_searchPatternCompleterModel->setData(m_searchPatternCompleterModel->index(rowNum, 0), newSearchPattern);
    if (m_searchPatternCompleterModel->rowCount() > m_historyLength)
        m_searchPatternCompleterModel->removeRow(0);

    QMetaObject::invokeMethod(m_dataStorage, [this, history = m_searchPatternCompleterModel->stringList()]
    {
        m_dataStorage->storeHistory(history);
    });
}

void SearchWidget::loadHistory()
{
    if (m_historyLength <= 0)
        return;

    createSearchPatternCompleter();

    connect(m_dataStorage, &DataStorage::historyLoaded, this, [this](const QStringList &storedHistory)
    {
        if (m_historyLength <= 0)
            return;

        QStringList history = storedHistory;
        for (const QString &newPattern : asConst(m_searchPatternCompleterModel->stringList()))
        {
            if (!history.contains(newPattern))
                history.append(newPattern);
        }

        if (history.size() > m_historyLength)
            history = history.mid(history.size() - m_historyLength);

        m_searchPatternCompleterModel->setStringList(history);
    });

    QMetaObject::invokeMethod(m_dataStorage, &SearchWidget::DataStorage::loadHistory);
}

void SearchWidget::saveSession() const
{
    if (!m_storeOpenedTabs)
        return;

    const int currentIndex = m_ui->tabWidget->currentIndex();
    SessionData sessionData;
    for (int tabIndex = 0; tabIndex < m_ui->tabWidget->count(); ++tabIndex)
    {
        auto *searchJobWidget = static_cast<SearchJobWidget *>(m_ui->tabWidget->widget(tabIndex));
        sessionData.tabs.emplaceBack(TabData {searchJobWidget->id(), searchJobWidget->searchPattern()});
        if (currentIndex == tabIndex)
            sessionData.currentTabID = searchJobWidget->id();
    }

    QMetaObject::invokeMethod(m_dataStorage, [this, sessionData] { m_dataStorage->storeSession(sessionData); });
}

void SearchWidget::createSearchPatternCompleter()
{
    Q_ASSERT(!m_ui->lineEditSearchPattern->completer());

    m_searchPatternCompleterModel = new QStringListModel(this);
    auto *sortModel = new SearchHistorySortModel(this);
    sortModel->setSourceModel(m_searchPatternCompleterModel);
    sortModel->sort(0);
    auto *completer = new QCompleter(sortModel, this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setModelSorting(QCompleter::CaseInsensitivelySortedModel);
    m_ui->lineEditSearchPattern->setCompleter(completer);
}

void SearchWidget::restoreSession()
{
    if (!m_storeOpenedTabs)
        return;

    connect(m_dataStorage, &DataStorage::tabLoaded, this
            , [this](const QString &tabID, const QString &searchPattern, const QList<SearchResult> &searchResults)
    {
        auto *restoredTab = new SearchJobWidget(tabID, searchPattern, searchResults, app(), this);
        addTab(tabID, restoredTab);
    });

    connect(m_dataStorage, &DataStorage::sessionLoaded, this, [this](const SessionData &sessionData)
    {
        m_ui->tabWidget->setCurrentWidget(m_tabWidgets.value(sessionData.currentTabID));
    });

    QMetaObject::invokeMethod(m_dataStorage, [this] { m_dataStorage->loadSession(m_storeOpenedTabsResults); });
}

void SearchWidget::selectActivePage()
{
    if (SearchPluginManager::instance()->allPlugins().isEmpty())
    {
        m_ui->stackedPages->setCurrentWidget(m_ui->emptyPage);
        m_ui->lineEditSearchPattern->setEnabled(false);
        m_ui->comboCategory->setEnabled(false);
        m_ui->selectPlugin->setEnabled(false);
        m_ui->searchButton->setEnabled(false);
    }
    else
    {
        m_ui->stackedPages->setCurrentWidget(m_ui->searchPage);
        m_ui->lineEditSearchPattern->setEnabled(true);
        m_ui->comboCategory->setEnabled(true);
        m_ui->selectPlugin->setEnabled(true);
        m_ui->searchButton->setEnabled(true);
    }
}

SearchWidget::~SearchWidget()
{
    qDebug("Search destruction");
    delete m_ui;
}

void SearchWidget::currentTabChanged(const int index)
{
    // when we switch from a tab that is not empty to another that is empty
    // the download button doesn't have to be available
    m_currentSearchTab = (index >= 0)
        ? static_cast<SearchJobWidget *>(m_ui->tabWidget->widget(index))
        : nullptr;

    if (!m_currentSearchTab)
        m_isNewQueryString = true;

    if (!m_isNewQueryString)
        m_ui->lineEditSearchPattern->setText(m_currentSearchTab->searchPattern());

    adjustSearchButton();
}

void SearchWidget::selectMultipleBox([[maybe_unused]] const int index)
{
    const auto itemText = m_ui->selectPlugin->itemData(m_ui->selectPlugin->currentIndex()).toString();
    if (itemText == u"multi")
        pluginsButtonClicked();
}

void SearchWidget::toggleFocusBetweenLineEdits()
{
    if (m_ui->lineEditSearchPattern->hasFocus() && m_currentSearchTab)
    {
        m_currentSearchTab->lineEditSearchResultsFilter()->setFocus();
        m_currentSearchTab->lineEditSearchResultsFilter()->selectAll();
    }
    else
    {
        m_ui->lineEditSearchPattern->setFocus();
        m_ui->lineEditSearchPattern->selectAll();
    }
}

void SearchWidget::adjustSearchButton()
{
    if (!m_isNewQueryString
            && (m_currentSearchTab && (m_currentSearchTab->status() == SearchJobWidget::Status::Ongoing)))
    {
        if (m_ui->searchButton->isVisible())
        {
            m_ui->searchButton->hide();
            m_ui->stopButton->show();
        }
    }
    else
    {
        if (m_ui->stopButton->isVisible())
        {
            m_ui->stopButton->hide();
            m_ui->searchButton->show();
        }
    }
}

void SearchWidget::showTabMenu(const int index)
{
    QMenu *menu = new QMenu(this);

    if (auto *searchJobWidget = static_cast<SearchJobWidget *>(m_ui->tabWidget->widget(index));
            searchJobWidget->status() != SearchJobWidget::Status::Ongoing)
    {
        menu->addAction(tr("Refresh"), this, [this, searchJobWidget] { refreshTab(searchJobWidget); });
    }
    else
    {
        menu->addAction(tr("Stop"), this, [searchJobWidget] { searchJobWidget->cancelSearch(); });
    }

    menu->addSeparator();
    menu->addAction(tr("Close tab"), this, [this, index] { closeTab(index); });
    menu->addAction(tr("Close all tabs"), this, &SearchWidget::closeAllTabs);

    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->popup(QCursor::pos());
}

void SearchWidget::pluginsButtonClicked()
{
    auto *dlg = new PluginSelectDialog(SearchPluginManager::instance(), this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

void SearchWidget::searchTextEdited(const QString &text)
{
    if (m_currentSearchTab)
    {
        m_isNewQueryString = m_currentSearchTab->searchPattern() != text;
        adjustSearchButton();
    }
}

void SearchWidget::giveFocusToSearchInput()
{
    m_ui->lineEditSearchPattern->setFocus();
}

// Function called when we click on search button
void SearchWidget::searchButtonClicked()
{
    m_isNewQueryString = false;

    const QString pattern = m_ui->lineEditSearchPattern->text().trimmed();
    // No search pattern entered
    if (pattern.isEmpty())
    {
        QMessageBox::critical(this, tr("Empty search pattern"), tr("Please type a search pattern first"));
        return;
    }

    if (!Utils::ForeignApps::pythonInfo().isValid())
    {
        app()->desktopIntegration()->showNotification(tr("Search Engine"), tr("Please install Python to use the Search Engine."));
        return;
    }

    qDebug("Search with category: %s", qUtf8Printable(selectedCategory()));

    // Launch search
    auto *searchHandler = SearchPluginManager::instance()->startSearch(pattern, selectedCategory(), selectedPlugins());

    // Tab Addition
    const QString newTabID = generateTabID();
    auto *newTab = new SearchJobWidget(newTabID, searchHandler, app(), this);
    const int tabIndex = addTab(newTabID, newTab);
    m_ui->tabWidget->setTabToolTip(tabIndex, newTab->statusTip());
    m_ui->tabWidget->setTabIcon(tabIndex, UIThemeManager::instance()->getIcon(statusIconName(newTab->status())));
    m_ui->tabWidget->setCurrentWidget(newTab);
    adjustSearchButton();
    updateHistory(pattern);
    saveSession();
}

void SearchWidget::stopButtonClicked()
{
    m_currentSearchTab->cancelSearch();
}

void SearchWidget::tabStatusChanged(SearchJobWidget *tab)
{
    const int tabIndex = m_ui->tabWidget->indexOf(tab);
    m_ui->tabWidget->setTabToolTip(tabIndex, tab->statusTip());
    m_ui->tabWidget->setTabIcon(tabIndex, UIThemeManager::instance()->getIcon(
            statusIconName(static_cast<SearchJobWidget *>(tab)->status())));

    if (tab->status() != SearchJobWidget::Status::Ongoing)
    {
        if (tab == m_currentSearchTab)
            adjustSearchButton();

        emit searchFinished(tab->status() == SearchJobWidget::Status::Error);

        if (m_storeOpenedTabsResults)
        {
            QMetaObject::invokeMethod(m_dataStorage, [this, tabID = tab->id(), searchResults = tab->searchResults()]
            {
                m_dataStorage->storeTab(tabID, searchResults);
            });
        }
    }
}

void SearchWidget::closeTab(const int index)
{
    const auto *tab = static_cast<SearchJobWidget *>(m_ui->tabWidget->widget(index));
    const QString tabID = tab->id();
    delete m_tabWidgets.take(tabID);

    QMetaObject::invokeMethod(m_dataStorage, [this, tabID] { m_dataStorage->removeTab(tabID); });
    saveSession();
}

void SearchWidget::closeAllTabs()
{
    for (int tabIndex = (m_ui->tabWidget->count() - 1); tabIndex >= 0; --tabIndex)
    {
        const auto *tab = static_cast<SearchJobWidget *>(m_ui->tabWidget->widget(tabIndex));
        const QString tabID = tab->id();
        delete m_tabWidgets.take(tabID);
        QMetaObject::invokeMethod(m_dataStorage, [this, tabID] { m_dataStorage->removeTab(tabID); });
    }

    saveSession();
}

void SearchWidget::refreshTab(SearchJobWidget *searchJobWidget)
{
    if (!Utils::ForeignApps::pythonInfo().isValid())
    {
        app()->desktopIntegration()->showNotification(tr("Search Engine"), tr("Please install Python to use the Search Engine."));
        return;
    }

    // Re-launch search
    auto *searchHandler = SearchPluginManager::instance()->startSearch(searchJobWidget->searchPattern(), selectedCategory(), selectedPlugins());
    searchJobWidget->assignSearchHandler(searchHandler);
}

void SearchWidget::DataStorage::loadSession(const bool withSearchResults)
{
    const Path sessionFilePath = makeDataFilePath(SESSION_FILE_NAME);
    const auto loadResult = ::loadSession(sessionFilePath);
    if (!loadResult)
    {
        LogMsg(tr("Failed to load Search UI saved state data. File: \"%1\". Error: \"%2\"")
                .arg(sessionFilePath.toString(), loadResult.error()), Log::WARNING);
        return;
    }

    const SessionData &sessionData = loadResult.value();

    for (const auto &[tabID, searchPattern] : sessionData.tabs)
    {
        QList<SearchResult> searchResults;

        if (withSearchResults)
        {
            const Path tabStateFilePath = makeDataFilePath(tabID + u".json");
            if (const auto loadTabStateResult = loadSearchResults(tabStateFilePath))
            {
                searchResults = loadTabStateResult.value();
            }
            else
            {
                LogMsg(tr("Failed to load saved search results. Tab: \"%1\". File: \"%2\". Error: \"%3\"")
                        .arg(searchPattern, tabStateFilePath.toString(), loadTabStateResult.error()), Log::WARNING);
            }
        }

        emit tabLoaded(tabID, searchPattern, searchResults);
    }

    emit sessionLoaded(sessionData);
}

void SearchWidget::DataStorage::storeSession(const SessionData &sessionData)
{
    QJsonArray tabsList;
    for (const auto &[tabID, searchPattern] : sessionData.tabs)
    {
        const QJsonObject tabObj {
            {u"ID"_s, tabID},
            {u"SearchPattern"_s, searchPattern}
        };
        tabsList.append(tabObj);
    }

    const QJsonObject sessionObj {
        {u"Tabs"_s, tabsList},
        {u"CurrentTab"_s, sessionData.currentTabID}
    };

    const Path sessionFilePath = makeDataFilePath(SESSION_FILE_NAME);
    const auto saveResult = Utils::IO::saveToFile(sessionFilePath, QJsonDocument(sessionObj).toJson(QJsonDocument::Compact));
    if (!saveResult)
    {
        LogMsg(tr("Failed to save Search UI state. File: \"%1\". Error: \"%2\"")
                .arg(sessionFilePath.toString(), saveResult.error()), Log::WARNING);
    }
}

void SearchWidget::DataStorage::removeSession()
{
    Utils::Fs::removeFile(makeDataFilePath(SESSION_FILE_NAME));
}

void SearchWidget::DataStorage::storeTab(const QString &tabID, const QList<SearchResult> &searchResults)
{
    QJsonArray searchResultsArray;
    for (const SearchResult &searchResult : searchResults)
    {
        searchResultsArray.append(QJsonObject {
            {KEY_RESULT_FILENAME, searchResult.fileName},
            {KEY_RESULT_FILEURL, searchResult.fileUrl},
            {KEY_RESULT_FILESIZE, searchResult.fileSize},
            {KEY_RESULT_SEEDERSCOUNT, searchResult.nbSeeders},
            {KEY_RESULT_LEECHERSCOUNT, searchResult.nbLeechers},
            {KEY_RESULT_ENGINENAME, searchResult.engineName},
            {KEY_RESULT_SITEURL, searchResult.siteUrl},
            {KEY_RESULT_DESCRLINK, searchResult.descrLink},
            {KEY_RESULT_PUBDATE, Utils::DateTime::toSecsSinceEpoch(searchResult.pubDate)}
        });
    }

    const Path filePath = makeDataFilePath(tabID + u".json");
    const auto saveResult = Utils::IO::saveToFile(filePath, QJsonDocument(searchResultsArray).toJson(QJsonDocument::Compact));
    if (!saveResult)
    {
        LogMsg(tr("Failed to save search results. Tab: \"%1\". File: \"%2\". Error: \"%3\"")
                .arg(tabID, filePath.toString(), saveResult.error()), Log::WARNING);
    }
}

void SearchWidget::DataStorage::removeTab(const QString &tabID)
{
    Utils::Fs::removeFile(makeDataFilePath(tabID + u".json"));
}

void SearchWidget::DataStorage::loadHistory()
{
    const Path historyFilePath = makeDataFilePath(HISTORY_FILE_NAME);
    const auto loadResult = ::loadHistory(historyFilePath);
    if (!loadResult)
    {
        LogMsg(tr("Failed to load Search UI history. File: \"%1\". Error: \"%2\"")
                .arg(historyFilePath.toString(), loadResult.error()), Log::WARNING);
        return;
    }

    emit historyLoaded(loadResult.value());
}

void SearchWidget::DataStorage::storeHistory(const QStringList &history)
{
    const Path filePath = makeDataFilePath(HISTORY_FILE_NAME);
    const auto saveResult = Utils::IO::saveToFile(filePath, history.join(u'\n').toUtf8());
    if (!saveResult)
    {
        LogMsg(tr("Failed to save search history. File: \"%1\". Error: \"%2\"")
                .arg(filePath.toString(), saveResult.error()), Log::WARNING);
    }
}

void SearchWidget::DataStorage::removeHistory()
{
    Utils::Fs::removeFile(makeDataFilePath(HISTORY_FILE_NAME));
}

#include "searchwidget.moc"
