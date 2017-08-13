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

#include "searchwidget.h"

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
#include <QTreeView>

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

#include "ui_searchwidget.h"

#define SEARCHHISTORY_MAXSIZE 50
#define URL_COLUMN 5

SearchWidget::SearchWidget(MainWindow *mainWindow)
    : QWidget(mainWindow)
    , m_ui(new Ui::SearchWidget())
    , m_mainWindow(mainWindow)
    , m_isNewQueryString(false)
    , m_noSearchResults(true)
{
    m_ui->setupUi(this);

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
    m_ui->m_searchPattern->setToolTip(searchPatternHint);

#ifndef Q_OS_MAC
    // Icons
    m_ui->searchButton->setIcon(GuiIconProvider::instance()->getIcon("edit-find"));
    m_ui->downloadButton->setIcon(GuiIconProvider::instance()->getIcon("download"));
    m_ui->goToDescBtn->setIcon(GuiIconProvider::instance()->getIcon("application-x-mswinurl"));
    m_ui->pluginsButton->setIcon(GuiIconProvider::instance()->getIcon("preferences-system-network"));
    m_ui->copyURLBtn->setIcon(GuiIconProvider::instance()->getIcon("edit-copy"));
#else
    // On macOS the icons overlap the text otherwise
    QSize iconSize = m_ui->tabWidget->iconSize();
    iconSize.setWidth(iconSize.width() + 16);
    m_ui->tabWidget->setIconSize(iconSize);
#endif
    connect(m_ui->tabWidget, &QTabWidget::tabCloseRequested, this, &SearchWidget::closeTab);

    m_searchEngine = new SearchEngine;
    connect(m_searchEngine, &SearchEngine::searchStarted, this, &SearchWidget::searchStarted);
    connect(m_searchEngine, &SearchEngine::newSearchResults, this, &SearchWidget::appendSearchResults);
    connect(m_searchEngine, &SearchEngine::searchFinished, this, &SearchWidget::searchFinished);
    connect(m_searchEngine, &SearchEngine::searchFailed, this, &SearchWidget::searchFailed);
    connect(m_searchEngine, &SearchEngine::torrentFileDownloaded, this, &SearchWidget::addTorrentToSession);

    const auto onPluginChanged = [this]()
    {
        fillCatCombobox();
        fillPluginComboBox();
        selectActivePage();
    };
    connect(m_searchEngine, &SearchEngine::pluginInstalled, this, onPluginChanged);
    connect(m_searchEngine, &SearchEngine::pluginUninstalled, this, onPluginChanged);
    connect(m_searchEngine, &SearchEngine::pluginUpdated, this, onPluginChanged);
    connect(m_searchEngine, &SearchEngine::pluginEnabled, this, onPluginChanged);

    // Fill in category combobox
    onPluginChanged();

    connect(m_ui->m_searchPattern, &LineEdit::returnPressed, m_ui->searchButton, &QPushButton::click);
    connect(m_ui->m_searchPattern, &LineEdit::textEdited, this, &SearchWidget::searchTextEdited);
    connect(m_ui->selectPlugin, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &SearchWidget::selectMultipleBox);
}

void SearchWidget::fillCatCombobox()
{
    m_ui->comboCategory->clear();
    m_ui->comboCategory->addItem(SearchEngine::categoryFullName("all"), QVariant("all"));
    m_ui->comboCategory->insertSeparator(1);

    using QStrPair = QPair<QString, QString>;
    QList<QStrPair> tmpList;
    foreach (const QString &cat, m_searchEngine->supportedCategories())
        tmpList << qMakePair(SearchEngine::categoryFullName(cat), cat);
    std::sort(tmpList.begin(), tmpList.end(), [](const QStrPair &l, const QStrPair &r) { return (QString::localeAwareCompare(l.first, r.first) < 0); });

    foreach (const QStrPair &p, tmpList) {
        qDebug("Supported category: %s", qUtf8Printable(p.second));
        m_ui->comboCategory->addItem(p.first, QVariant(p.second));
    }
}

void SearchWidget::fillPluginComboBox()
{
    m_ui->selectPlugin->clear();
    m_ui->selectPlugin->addItem(tr("Only enabled"), QVariant("enabled"));
    m_ui->selectPlugin->addItem(tr("All plugins"), QVariant("all"));
    m_ui->selectPlugin->addItem(tr("Select..."), QVariant("multi"));
    m_ui->selectPlugin->insertSeparator(3);

    using QStrPair = QPair<QString, QString>;
    QList<QStrPair> tmpList;
    foreach (const QString &name, m_searchEngine->enabledPlugins())
        tmpList << qMakePair(m_searchEngine->pluginFullName(name), name);
    std::sort(tmpList.begin(), tmpList.end(), [](const QStrPair &l, const QStrPair &r) { return (l.first < r.first); } );

    foreach (const QStrPair &p, tmpList)
        m_ui->selectPlugin->addItem(p.first, QVariant(p.second));
}

QString SearchWidget::selectedCategory() const
{
    return m_ui->comboCategory->itemData(m_ui->comboCategory->currentIndex()).toString();
}

QString SearchWidget::selectedPlugin() const
{
    return m_ui->selectPlugin->itemData(m_ui->selectPlugin->currentIndex()).toString();
}

void SearchWidget::selectActivePage()
{
    if (m_searchEngine->allPlugins().isEmpty()) {
        m_ui->stackedPages->setCurrentWidget(m_ui->emptyPage);
        m_ui->m_searchPattern->setEnabled(false);
        m_ui->comboCategory->setEnabled(false);
        m_ui->selectPlugin->setEnabled(false);
        m_ui->searchButton->setEnabled(false);
    }
    else {
        m_ui->stackedPages->setCurrentWidget(m_ui->searchPage);
        m_ui->m_searchPattern->setEnabled(true);
        m_ui->comboCategory->setEnabled(true);
        m_ui->selectPlugin->setEnabled(true);
        m_ui->searchButton->setEnabled(true);
    }
}

SearchWidget::~SearchWidget()
{
    qDebug("Search destruction");
    delete m_searchEngine;
    delete m_ui;
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
        m_currentSearchTab = m_allTabs.at(m_ui->tabWidget->currentIndex());
        if (m_currentSearchTab->getCurrentSearchListModel()->rowCount()) {
            m_ui->downloadButton->setEnabled(true);
            m_ui->goToDescBtn->setEnabled(true);
            m_ui->copyURLBtn->setEnabled(true);
        }
        else {
            m_ui->downloadButton->setEnabled(false);
            m_ui->goToDescBtn->setEnabled(false);
            m_ui->copyURLBtn->setEnabled(false);
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
    new PluginSelectDlg(m_searchEngine, this);
}

void SearchWidget::searchTextEdited(QString)
{
    // Enable search button
    m_ui->searchButton->setText(tr("Search"));
    m_isNewQueryString = true;
}

void SearchWidget::giveFocusToSearchInput()
{
    m_ui->m_searchPattern->setFocus();
}

QTabWidget *SearchWidget::searchTabs() const
{
    return m_ui->tabWidget;
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
            m_ui->searchButton->setText(tr("Search"));
            return;
        }
    }

    m_isNewQueryString = false;

    const QString pattern = m_ui->m_searchPattern->text().trimmed();
    // No search pattern entered
    if (pattern.isEmpty()) {
        QMessageBox::critical(this, tr("Empty search pattern"), tr("Please type a search pattern first"));
        return;
    }

    // Tab Addition
    m_currentSearchTab = new SearchTab(this);
    m_activeSearchTab = m_currentSearchTab;
    m_allTabs.append(m_currentSearchTab);
    QString tabName = pattern;
    tabName.replace(QRegExp("&{1}"), "&&");
    m_ui->tabWidget->addTab(m_currentSearchTab, tabName);
    m_ui->tabWidget->setCurrentWidget(m_currentSearchTab);
    m_currentSearchTab->getCurrentSearchListProxy()->setNameFilter(pattern);

    QStringList plugins;
    if (selectedPlugin() == "all") plugins = m_searchEngine->allPlugins();
    else if (selectedPlugin() == "enabled") plugins = m_searchEngine->enabledPlugins();
    else if (selectedPlugin() == "multi") plugins = m_searchEngine->enabledPlugins();
    else plugins << selectedPlugin();

    qDebug("Search with category: %s", qUtf8Printable(selectedCategory()));

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
    m_ui->searchButton->setText(tr("Stop"));
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
    m_ui->searchButton->setText(tr("Search"));
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
    m_ui->downloadButton->setEnabled(true);
    m_ui->goToDescBtn->setEnabled(true);
    m_ui->copyURLBtn->setEnabled(true);
}

void SearchWidget::closeTab(int index)
{
    // Search is run for active tab so if user decided to close it, then stop search
    if (!m_activeSearchTab.isNull() && index == m_ui->tabWidget->indexOf(m_activeSearchTab)) {
        qDebug("Closed active search Tab");
        if (m_searchEngine->isActive())
            m_searchEngine->cancelSearch();
        m_activeSearchTab = 0;
    }

    delete m_allTabs.takeAt(index);

    if (!m_allTabs.size()) {
        m_ui->downloadButton->setEnabled(false);
        m_ui->goToDescBtn->setEnabled(false);
        m_ui->copyURLBtn->setEnabled(false);
    }
}

// Download selected items in search results list
void SearchWidget::on_downloadButton_clicked()
{
    //QModelIndexList selectedIndexes = currentSearchTab->getCurrentTreeView()->selectionModel()->selectedIndexes();
    QModelIndexList selectedIndexes =
        m_allTabs.at(m_ui->tabWidget->currentIndex())->getCurrentTreeView()->selectionModel()->selectedIndexes();
    foreach (const QModelIndex &index, selectedIndexes) {
        if (index.column() == SearchSortModel::NAME)
            m_allTabs.at(m_ui->tabWidget->currentIndex())->downloadItem(index);
    }
}

void SearchWidget::on_goToDescBtn_clicked()
{
    QModelIndexList selectedIndexes =
        m_allTabs.at(m_ui->tabWidget->currentIndex())->getCurrentTreeView()->selectionModel()->selectedIndexes();
    foreach (const QModelIndex &index, selectedIndexes) {
        if (index.column() == SearchSortModel::NAME) {
            QSortFilterProxyModel *model = m_allTabs.at(m_ui->tabWidget->currentIndex())->getCurrentSearchListProxy();
            const QString descUrl = model->data(model->index(index.row(), SearchSortModel::DESC_LINK)).toString();
            if (!descUrl.isEmpty())
                QDesktopServices::openUrl(QUrl::fromEncoded(descUrl.toUtf8()));
        }
    }
}

void SearchWidget::on_copyURLBtn_clicked()
{
    QStringList urls;
    QModelIndexList selectedIndexes =
        m_allTabs.at(m_ui->tabWidget->currentIndex())->getCurrentTreeView()->selectionModel()->selectedIndexes();

    foreach (const QModelIndex &index, selectedIndexes) {
        if (index.column() == SearchSortModel::NAME) {
            QSortFilterProxyModel *model = m_allTabs.at(m_ui->tabWidget->currentIndex())->getCurrentSearchListProxy();
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
