/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
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
 *
 * Contact : chris@qbittorrent.org
 */

#include <QHeaderView>
#include <QMessageBox>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QDir>
#include <QMenu>
#include <QClipboard>
#include <QMimeData>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QFileDialog>
#include <QDesktopServices>
#include <QClipboard>
#include <QProcess>
#include <QDebug>
#include <QTextStream>

#include <iostream>
#ifdef Q_OS_WIN
#include <stdlib.h>
#endif

#include "base/bittorrent/session.h"
#include "base/utils/fs.h"
#include "base/utils/misc.h"
#include "base/preferences.h"
#include "base/searchengine.h"
#include "searchlistdelegate.h"
#include "mainwindow.h"
#include "addnewtorrentdialog.h"
#include "guiiconprovider.h"
#include "pluginselectdlg.h"
#include "searchsortmodel.h"
#include "searchtab.h"
#include "searchwidget.h"

#define SEARCHHISTORY_MAXSIZE 50
#define URL_COLUMN 5

SearchWidget::SearchWidget(MainWindow *mainWindow)
    : QWidget(mainWindow)
    , m_mainWindow(mainWindow)
    , m_isNewQueryString(false)
    , m_noSearchResults(true)
{
    setupUi(this);

    QString searchPatternHint;
    QTextStream stream(&searchPatternHint, QIODevice::WriteOnly);
    stream << "<html><head/><body><p>"
           << tr("A phrase to search for.") << "<br>"
           << tr("Spaces in a search term may be protected by double quotes.")
           << "</p><p>"
           << tr("Example:", "Search phrase example")
           << "<br>"
           << tr("<b>foo bar</b>: search for <b>foo</b> and <b>bar</b>",
                 "Search phrase example, illustrates quotes usage, a pair of "
                 "space delimited words, individal words are highlighted")
           << "<br>"
           << tr("<b>&quot;foo bar&quot;</b>: search for <b>foo bar</b>",
                 "Search phrase example, illustrates quotes usage, double quoted"
                 "pair of space delimited words, the whole pair is highlighted")
           << "</p></body></html>" << flush;
    m_searchPattern->setToolTip(searchPatternHint);

    // Icons
    searchButton->setIcon(GuiIconProvider::instance()->getIcon("edit-find"));
    downloadButton->setIcon(GuiIconProvider::instance()->getIcon("download"));
    goToDescBtn->setIcon(GuiIconProvider::instance()->getIcon("application-x-mswinurl"));
    pluginsButton->setIcon(GuiIconProvider::instance()->getIcon("preferences-system-network"));
    copyURLBtn->setIcon(GuiIconProvider::instance()->getIcon("edit-copy"));
    connect(tabWidget, SIGNAL(tabCloseRequested(int)), this, SLOT(closeTab(int)));

    m_searchEngine = new SearchEngine;
    connect(m_searchEngine, SIGNAL(searchStarted()), SLOT(searchStarted()));
    connect(m_searchEngine, SIGNAL(newSearchResults(QList<SearchResult>)), SLOT(appendSearchResults(QList<SearchResult>)));
    connect(m_searchEngine, SIGNAL(searchFinished(bool)), SLOT(searchFinished(bool)));
    connect(m_searchEngine, SIGNAL(searchFailed()), SLOT(searchFailed()));
    connect(m_searchEngine, SIGNAL(torrentFileDownloaded(QString)), SLOT(addTorrentToSession(QString)));

    // Fill in category combobox
    fillCatCombobox();
    fillPluginComboBox();

    connect(m_searchPattern, SIGNAL(returnPressed()), searchButton, SLOT(click()));
    connect(m_searchPattern, SIGNAL(textEdited(QString)), this, SLOT(searchTextEdited(QString)));
    connect(selectPlugin, SIGNAL(currentIndexChanged(int)), this, SLOT(selectMultipleBox(int)));
}

void SearchWidget::fillCatCombobox()
{
    comboCategory->clear();
    comboCategory->addItem(SearchEngine::categoryFullName("all"), QVariant("all"));
    comboCategory->insertSeparator(1);

    using QStrPair = QPair<QString, QString>;
    QList<QStrPair> tmpList;
    foreach (const QString &cat, m_searchEngine->supportedCategories())
        tmpList << qMakePair(SearchEngine::categoryFullName(cat), cat);
    std::sort(tmpList.begin(), tmpList.end(), [](const QStrPair &l, const QStrPair &r) { return (QString::localeAwareCompare(l.first, r.first) < 0); });

    foreach (const QStrPair &p, tmpList) {
        qDebug("Supported category: %s", qPrintable(p.second));
        comboCategory->addItem(p.first, QVariant(p.second));
    }
}

void SearchWidget::fillPluginComboBox()
{
    selectPlugin->clear();
    selectPlugin->addItem(tr("Only enabled"), QVariant("enabled"));
    selectPlugin->addItem(tr("All plugins"), QVariant("all"));
    selectPlugin->addItem(tr("Select..."), QVariant("multi"));
    selectPlugin->insertSeparator(3);

    using QStrPair = QPair<QString, QString>;
    QList<QStrPair> tmpList;
    foreach (const QString &name, m_searchEngine->enabledPlugins())
        tmpList << qMakePair(m_searchEngine->pluginFullName(name), name);
    std::sort(tmpList.begin(), tmpList.end(), [](const QStrPair &l, const QStrPair &r) { return (l.first < r.first); } );

    foreach (const QStrPair &p, tmpList)
        selectPlugin->addItem(p.first, QVariant(p.second));
}

QString SearchWidget::selectedCategory() const
{
    return comboCategory->itemData(comboCategory->currentIndex()).toString();
}

QString SearchWidget::selectedPlugin() const
{
    return selectPlugin->itemData(selectPlugin->currentIndex()).toString();
}

SearchWidget::~SearchWidget()
{
    qDebug("Search destruction");
    delete m_searchEngine;
}

void SearchWidget::downloadTorrent(const QString &siteUrl, const QString &url)
{
    if (url.startsWith("bc://bt/", Qt::CaseInsensitive) || url.startsWith("magnet:", Qt::CaseInsensitive))
        addTorrentToSession(url);
    else
        m_searchEngine->downloadTorrent(siteUrl, url);
}

void SearchWidget::tab_changed(int t)
{
    //when we switch from a tab that is not empty to another that is empty the download button
    //doesn't have to be available
    if (t > -1) {
        //-1 = no more tab
        m_currentSearchTab = m_allTabs.at(tabWidget->currentIndex());
        if (m_currentSearchTab->getCurrentSearchListModel()->rowCount()) {
            downloadButton->setEnabled(true);
            goToDescBtn->setEnabled(true);
            copyURLBtn->setEnabled(true);
        }
        else {
            downloadButton->setEnabled(false);
            goToDescBtn->setEnabled(false);
            copyURLBtn->setEnabled(false);
        }
    }
}

void SearchWidget::selectMultipleBox(int index)
{
    Q_UNUSED(index);
    if (selectedPlugin() == "multi")
        on_pluginsButton_clicked();
}

void SearchWidget::addTorrentToSession(const QString &source)
{
    if (AddNewTorrentDialog::isEnabled())
        AddNewTorrentDialog::show(source, this);
    else
        BitTorrent::Session::instance()->addTorrent(source);
}

void SearchWidget::on_pluginsButton_clicked()
{
    PluginSelectDlg *dlg = new PluginSelectDlg(m_searchEngine, this);
    connect(dlg, SIGNAL(pluginsChanged()), this, SLOT(fillCatCombobox()));
    connect(dlg, SIGNAL(pluginsChanged()), this, SLOT(fillPluginComboBox()));
}

void SearchWidget::searchTextEdited(QString)
{
    // Enable search button
    searchButton->setText(tr("Search"));
    m_isNewQueryString = true;
}

void SearchWidget::giveFocusToSearchInput()
{
    m_searchPattern->setFocus();
}

QTabWidget *SearchWidget::searchTabs() const
{
    return tabWidget;
}

// Function called when we click on search button
void SearchWidget::on_searchButton_clicked()
{
    if (Utils::Misc::pythonVersion() < 0) {
        m_mainWindow->showNotificationBaloon(tr("Search Engine"), tr("Please install Python to use the Search Engine."));
        return;
    }

    if (m_searchEngine->isActive()) {
        m_searchEngine->cancelSearch();

        if (!m_isNewQueryString) {
            searchButton->setText(tr("Search"));
            return;
        }
    }

    m_isNewQueryString = false;

    const QString pattern = m_searchPattern->text().trimmed();
    // No search pattern entered
    if (pattern.isEmpty()) {
        QMessageBox::critical(0, tr("Empty search pattern"), tr("Please type a search pattern first"));
        return;
    }

    // Tab Addition
    m_currentSearchTab = new SearchTab(this);
    m_activeSearchTab = m_currentSearchTab;
    m_allTabs.append(m_currentSearchTab);
    QString tabName = pattern;
    tabName.replace(QRegExp("&{1}"), "&&");
    tabWidget->addTab(m_currentSearchTab, tabName);
    tabWidget->setCurrentWidget(m_currentSearchTab);
    m_currentSearchTab->getCurrentSearchListProxy()->setNameFilter(pattern);

    QStringList plugins;
    if (selectedPlugin() == "all") plugins = m_searchEngine->allPlugins();
    else if (selectedPlugin() == "enabled") plugins = m_searchEngine->enabledPlugins();
    else if (selectedPlugin() == "multi") plugins = m_searchEngine->enabledPlugins();
    else plugins << selectedPlugin();

    qDebug("Search with category: %s", qPrintable(selectedCategory()));

    // Update SearchEngine widgets
    m_noSearchResults = true;

    // Changing the text of the current label
    m_activeSearchTab->updateResultsCount();

    // Launch search
    m_searchEngine->startSearch(pattern, selectedCategory(), plugins);
}

void SearchWidget::searchStarted()
{
    // Update SearchEngine widgets
    m_activeSearchTab->setStatus(SearchTab::Status::Ongoing);
    searchButton->setText(tr("Stop"));
}

// Slot called when search is Finished
// Search can be finished for 3 reasons :
// Error | Stopped by user | Finished normally
void SearchWidget::searchFinished(bool cancelled)
{
    if (m_mainWindow->isNotificationsEnabled() && (m_mainWindow->currentTabWidget() != this))
        m_mainWindow->showNotificationBaloon(tr("Search Engine"), tr("Search has finished"));

    if (m_activeSearchTab.isNull()) return; // The active tab was closed

    if (cancelled)
        m_activeSearchTab->setStatus(SearchTab::Status::Aborted);
    else if (m_noSearchResults)
        m_activeSearchTab->setStatus(SearchTab::Status::NoResults);
    else
        m_activeSearchTab->setStatus(SearchTab::Status::Finished);

    m_activeSearchTab = 0;
    searchButton->setText(tr("Search"));
}

void SearchWidget::searchFailed()
{
    if (m_mainWindow->isNotificationsEnabled() && (m_mainWindow->currentTabWidget() != this))
        m_mainWindow->showNotificationBaloon(tr("Search Engine"), tr("Search has failed"));

    if (m_activeSearchTab.isNull()) return; // The active tab was closed

#ifdef Q_OS_WIN
    m_activeSearchTab->setStatus(SearchTab::Status::Aborted);
#else
    m_activeSearchTab->setStatus(SearchTab::Status::Error);
#endif
}

void SearchWidget::appendSearchResults(const QList<SearchResult> &results)
{
    if (m_activeSearchTab.isNull()) {
        m_searchEngine->cancelSearch();
        return;
    }

    Q_ASSERT(m_activeSearchTab);

    QStandardItemModel *curModel = m_activeSearchTab->getCurrentSearchListModel();
    Q_ASSERT(curModel);

    foreach (const SearchResult &result, results) {
        // Add item to search result list
        int row = curModel->rowCount();
        curModel->insertRow(row);

        curModel->setData(curModel->index(row, SearchSortModel::DL_LINK), result.fileUrl); // download URL
        curModel->setData(curModel->index(row, SearchSortModel::NAME), result.fileName); // Name
        curModel->setData(curModel->index(row, SearchSortModel::SIZE), result.fileSize); // Size
        curModel->setData(curModel->index(row, SearchSortModel::SEEDS), result.nbSeeders); // Seeders
        curModel->setData(curModel->index(row, SearchSortModel::LEECHES), result.nbLeechers); // Leechers
        curModel->setData(curModel->index(row, SearchSortModel::ENGINE_URL), result.siteUrl); // Search site URL
        curModel->setData(curModel->index(row, SearchSortModel::DESC_LINK), result.descrLink); // Description Link
    }

    m_noSearchResults = false;
    m_activeSearchTab->updateResultsCount();

    // Enable clear & download buttons
    downloadButton->setEnabled(true);
    goToDescBtn->setEnabled(true);
    copyURLBtn->setEnabled(true);
}

void SearchWidget::closeTab(int index)
{
    // Search is run for active tab so if user decided to close it, then stop search
    if (!m_activeSearchTab.isNull() && index == tabWidget->indexOf(m_activeSearchTab)) {
        qDebug("Closed active search Tab");
        if (m_searchEngine->isActive())
            m_searchEngine->cancelSearch();
        m_activeSearchTab = 0;
    }

    delete m_allTabs.takeAt(index);

    if (!m_allTabs.size()) {
        downloadButton->setEnabled(false);
        goToDescBtn->setEnabled(false);
        copyURLBtn->setEnabled(false);
    }
}

// Download selected items in search results list
void SearchWidget::on_downloadButton_clicked()
{
    //QModelIndexList selectedIndexes = currentSearchTab->getCurrentTreeView()->selectionModel()->selectedIndexes();
    QModelIndexList selectedIndexes = m_allTabs.at(tabWidget->currentIndex())->getCurrentTreeView()->selectionModel()->selectedIndexes();
    foreach (const QModelIndex &index, selectedIndexes) {
        if (index.column() == SearchSortModel::NAME)
            m_allTabs.at(tabWidget->currentIndex())->downloadItem(index);
    }
}

void SearchWidget::on_goToDescBtn_clicked()
{
    QModelIndexList selectedIndexes = m_allTabs.at(tabWidget->currentIndex())->getCurrentTreeView()->selectionModel()->selectedIndexes();
    foreach (const QModelIndex &index, selectedIndexes) {
        if (index.column() == SearchSortModel::NAME) {
            QSortFilterProxyModel *model = m_allTabs.at(tabWidget->currentIndex())->getCurrentSearchListProxy();
            const QString descUrl = model->data(model->index(index.row(), SearchSortModel::DESC_LINK)).toString();
            if (!descUrl.isEmpty())
                QDesktopServices::openUrl(QUrl::fromEncoded(descUrl.toUtf8()));
        }
    }
}

void SearchWidget::on_copyURLBtn_clicked()
{
    QStringList urls;
    QModelIndexList selectedIndexes = m_allTabs.at(tabWidget->currentIndex())->getCurrentTreeView()->selectionModel()->selectedIndexes();

    foreach (const QModelIndex &index, selectedIndexes) {
        if (index.column() == SearchSortModel::NAME) {
            QSortFilterProxyModel *model = m_allTabs.at(tabWidget->currentIndex())->getCurrentSearchListProxy();
            const QString descUrl = model->data(model->index(index.row(), SearchSortModel::DESC_LINK)).toString();
            if (!descUrl.isEmpty())
                urls << descUrl.toUtf8();
        }
    }

    if (!urls.empty()) {
        QClipboard *clipboard = QApplication::clipboard();
        clipboard->setText(urls.join("\n"));
    }
}
