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

#include <QStandardItemModel>
#include <QHeaderView>
#include <QMessageBox>
#include <QTemporaryFile>
#include <QSystemTrayIcon>
#include <iostream>
#include <QTimer>
#include <QDir>
#include <QMenu>
#include <QClipboard>
#include <QMimeData>
#include <QSortFilterProxyModel>
#include <QFileDialog>
#include <QDesktopServices>
#include <QClipboard>
#include <QProcess>
#include <QDebug>

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
#include "lineedit.h"
#include "searchwidget.h"

#define SEARCHHISTORY_MAXSIZE 50

/*SEARCH ENGINE START*/
SearchWidget::SearchWidget(MainWindow* parent)
    : QWidget(parent)
    , search_pattern(new LineEdit(this))
    , mp_mainWindow(parent)
{
    setupUi(this);
    searchBarLayout->insertWidget(0, search_pattern);
    connect(search_pattern, SIGNAL(returnPressed()), search_button, SLOT(click()));
    // Icons
    search_button->setIcon(GuiIconProvider::instance()->getIcon("edit-find"));
    download_button->setIcon(GuiIconProvider::instance()->getIcon("download"));
    goToDescBtn->setIcon(GuiIconProvider::instance()->getIcon("application-x-mswinurl"));
    pluginsButton->setIcon(GuiIconProvider::instance()->getIcon("preferences-system-network"));
    copyURLBtn->setIcon(GuiIconProvider::instance()->getIcon("edit-copy"));
    tabWidget->setTabsClosable(true);
    connect(tabWidget, SIGNAL(tabCloseRequested(int)), this, SLOT(closeTab(int)));

    m_searchEngine = new SearchEngine;
    connect(m_searchEngine, SIGNAL(searchStarted()), SLOT(searchStarted()));
    connect(m_searchEngine, SIGNAL(newSearchResults(QList<SearchResult>)), SLOT(appendSearchResults(QList<SearchResult>)));
    connect(m_searchEngine, SIGNAL(searchFinished(bool)), SLOT(searchFinished(bool)));
    connect(m_searchEngine, SIGNAL(searchFailed()), SLOT(searchFailed()));

    // Fill in category combobox
    fillCatCombobox();
    fillPluginComboBox();

    connect(search_pattern, SIGNAL(textEdited(QString)), this, SLOT(searchTextEdited(QString)));
    connect(selectPlugin, SIGNAL(currentIndexChanged(const QString &)), this, SLOT(selectMultipleBox(const QString &)));
}

void SearchWidget::fillCatCombobox()
{
    comboCategory->clear();
    comboCategory->addItem(SearchEngine::categoryFullName("all"), QVariant("all"));
    QStringList supported_cat = m_searchEngine->supportedCategories();
    foreach (QString cat, supported_cat) {
        qDebug("Supported category: %s", qPrintable(cat));
        comboCategory->addItem(SearchEngine::categoryFullName(cat), QVariant(cat));
    }
}

void SearchWidget::fillPluginComboBox()
{
    selectPlugin->clear();
    selectPlugin->addItem(tr("All enabled"), QVariant("enabled"));
    selectPlugin->addItem(tr("All plugins"), QVariant("all"));
    foreach (QString name, m_searchEngine->enabledPlugins())
        selectPlugin->addItem(name, QVariant(name));
    selectPlugin->addItem(tr("Multiple..."), QVariant("multi"));
}

QString SearchWidget::selectedCategory() const
{
    return comboCategory->itemData(comboCategory->currentIndex()).toString();
}

QString SearchWidget::selectedEngine() const
{
    return selectPlugin->itemData(selectPlugin->currentIndex()).toString();
}

SearchWidget::~SearchWidget()
{
    qDebug("Search destruction");

    delete search_pattern;
    delete m_searchEngine;
}

void SearchWidget::tab_changed(int t)
{
    //when we switch from a tab that is not empty to another that is empty the download button
    //doesn't have to be available
    if (t > -1) {
        //-1 = no more tab
        currentSearchTab = all_tab.at(tabWidget->currentIndex());
        if (currentSearchTab->getCurrentSearchListModel()->rowCount()) {
            download_button->setEnabled(true);
            goToDescBtn->setEnabled(true);
            copyURLBtn->setEnabled(true);
        }
        else {
            download_button->setEnabled(false);
            goToDescBtn->setEnabled(false);
            copyURLBtn->setEnabled(false);
        }
        search_status->setText(currentSearchTab->status);
    }
}

void SearchWidget::selectMultipleBox(const QString &text)
{
    if (text == tr("Multiple...")) on_pluginsButton_clicked();
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
    search_button->setText(tr("Search"));
    newQueryString = true;
}

void SearchWidget::giveFocusToSearchInput()
{
    search_pattern->setFocus();
}

// Function called when we click on search button
void SearchWidget::on_search_button_clicked()
{
    if (Utils::Misc::pythonVersion() < 0) {
        mp_mainWindow->showNotificationBaloon(tr("Search Engine"), tr("Please install Python to use the Search Engine."));
        return;
    }

    if (m_searchEngine->isActive()) {
        m_searchEngine->cancelSearch();

        if (!newQueryString) {
            search_button->setText(tr("Search"));
            return;
        }
    }

    newQueryString = false;

    const QString pattern = search_pattern->text().trimmed();
    // No search pattern entered
    if (pattern.isEmpty()) {
        QMessageBox::critical(0, tr("Empty search pattern"), tr("Please type a search pattern first"));
        return;
    }

    // Tab Addition
    currentSearchTab = new SearchTab(this);
    activeSearchTab = currentSearchTab;
    connect(currentSearchTab->header(), SIGNAL(sectionResized(int, int, int)), this, SLOT(saveResultsColumnsWidth()));
    all_tab.append(currentSearchTab);
    QString tabName = pattern;
    tabName.replace(QRegExp("&{1}"), "&&");
    tabWidget->addTab(currentSearchTab, tabName);
    tabWidget->setCurrentWidget(currentSearchTab);

    QStringList plugins;
    if (selectedEngine() == "all") plugins = m_searchEngine->allPlugins();
    else if (selectedEngine() == "enabled") plugins = m_searchEngine->enabledPlugins();
    else if (selectedEngine() == "multi") plugins = m_searchEngine->enabledPlugins();
    else plugins << selectedEngine();

    qDebug("Search with category: %s", qPrintable(selectedCategory()));

    // Update SearchEngine widgets
    no_search_results = true;
    nb_search_results = 0;

    // Changing the text of the current label
    activeSearchTab->getCurrentLabel()->setText(tr("Results <i>(%1)</i>:", "i.e: Search results").arg(0));

    // Launch search
    m_searchEngine->startSearch(pattern, selectedCategory(), plugins);
}

void SearchWidget::saveResultsColumnsWidth()
{
    if (all_tab.isEmpty())
        return;
    QTreeView* treeview = all_tab.first()->getCurrentTreeView();
    Preferences* const pref = Preferences::instance();
    QStringList new_width_list;
    short nbColumns = all_tab.first()->getCurrentSearchListModel()->columnCount();
    for (short i = 0; i < nbColumns; ++i)
        if (treeview->columnWidth(i) > 0)
            new_width_list << QString::number(treeview->columnWidth(i));
    // Don't save the width of the last column (auto column width)
    new_width_list.removeLast();
    pref->setSearchColsWidth(new_width_list.join(" "));
}

void SearchWidget::downloadTorrent(QString url)
{
    if (Preferences::instance()->useAdditionDialog())
        AddNewTorrentDialog::show(url, this);
    else
        BitTorrent::Session::instance()->addTorrent(url);
}

void SearchWidget::searchStarted()
{
    // Update SearchEngine widgets
    activeSearchTab->status = tr("Searching...");
    search_status->setText(currentSearchTab->status);
    search_status->repaint();
    search_button->setText(tr("Stop"));
}

// Slot called when search is Finished
// Search can be finished for 3 reasons :
// Error | Stopped by user | Finished normally
void SearchWidget::searchFinished(bool cancelled)
{
    bool useNotificationBalloons = Preferences::instance()->useProgramNotification();
    if (useNotificationBalloons && mp_mainWindow->getCurrentTabWidget() != this)
        mp_mainWindow->showNotificationBaloon(tr("Search Engine"), tr("Search has finished"));

    if (activeSearchTab.isNull()) return; // The active tab was closed

    if (cancelled)
        activeSearchTab->status = tr("Search aborted");
    else if (no_search_results)
        activeSearchTab->status = tr("Search returned no results");
    else
        activeSearchTab->status = tr("Search has finished");

    search_status->setText(currentSearchTab->status);
    activeSearchTab = 0;
    search_button->setText(tr("Search"));
}

void SearchWidget::searchFailed()
{
    bool useNotificationBalloons = Preferences::instance()->useProgramNotification();
    if (useNotificationBalloons && mp_mainWindow->getCurrentTabWidget() != this)
        mp_mainWindow->showNotificationBaloon(tr("Search Engine"), tr("Search has failed"));

    if (activeSearchTab.isNull()) return; // The active tab was closed

#ifdef Q_OS_WIN
    activeSearchTab->status = tr("Search aborted");
#else
    activeSearchTab->status = tr("An error occurred during search...");
#endif
}

// SLOT to append one line to search results list
void SearchWidget::appendSearchResults(const QList<SearchResult> &results)
{
    if (activeSearchTab.isNull()) {
        m_searchEngine->cancelSearch();
        return;
    }

    Q_ASSERT(activeSearchTab);

    QStandardItemModel* cur_model = activeSearchTab->getCurrentSearchListModel();
    Q_ASSERT(cur_model);

    foreach (const SearchResult &result, results) {
        // Add item to search result list
        int row = cur_model->rowCount();
        cur_model->insertRow(row);

        cur_model->setData(cur_model->index(row, SearchSortModel::DL_LINK), result.fileUrl); // download URL
        cur_model->setData(cur_model->index(row, SearchSortModel::NAME), result.fileName); // Name
        cur_model->setData(cur_model->index(row, SearchSortModel::SIZE), result.fileSize); // Size
        cur_model->setData(cur_model->index(row, SearchSortModel::SEEDS), result.nbSeeders); // Seeders
        cur_model->setData(cur_model->index(row, SearchSortModel::LEECHS), result.nbLeechers); // Leechers
        cur_model->setData(cur_model->index(row, SearchSortModel::ENGINE_URL), result.siteUrl); // Search site URL
        cur_model->setData(cur_model->index(row, SearchSortModel::DESC_LINK), result.descrLink); // Description Link
    }

    no_search_results = false;
    nb_search_results += results.size();
    activeSearchTab->getCurrentLabel()->setText(tr("Results <i>(%1)</i>:", "i.e: Search results").arg(nb_search_results));

    // Enable clear & download buttons
    download_button->setEnabled(true);
    goToDescBtn->setEnabled(true);
    copyURLBtn->setEnabled(true);
}

void SearchWidget::closeTab(int index)
{
    // Search is run for active tab so if user decided to close it, then stop search
    if (!activeSearchTab.isNull() && index == tabWidget->indexOf(activeSearchTab)) {
        qDebug("Closed active search Tab");
        if (m_searchEngine->isActive())
            m_searchEngine->cancelSearch();
        activeSearchTab = 0;
    }
    delete all_tab.takeAt(index);
    if (!all_tab.size()) {
        download_button->setEnabled(false);
        goToDescBtn->setEnabled(false);
        search_status->setText(tr("Stopped"));
        copyURLBtn->setEnabled(false);
    }
}

// Download selected items in search results list
void SearchWidget::on_download_button_clicked()
{
    //QModelIndexList selectedIndexes = currentSearchTab->getCurrentTreeView()->selectionModel()->selectedIndexes();
    QModelIndexList selectedIndexes = all_tab.at(tabWidget->currentIndex())->getCurrentTreeView()->selectionModel()->selectedIndexes();
    foreach (const QModelIndex &index, selectedIndexes) {
        if (index.column() == SearchSortModel::NAME) {
            // Get Item url
            QSortFilterProxyModel* model = all_tab.at(tabWidget->currentIndex())->getCurrentSearchListProxy();
            QString torrent_url = model->data(model->index(index.row(), URL_COLUMN)).toString();
            downloadTorrent(torrent_url);
            all_tab.at(tabWidget->currentIndex())->setRowColor(index.row(), "blue");
        }
    }
}

void SearchWidget::on_goToDescBtn_clicked()
{
    QModelIndexList selectedIndexes = all_tab.at(tabWidget->currentIndex())->getCurrentTreeView()->selectionModel()->selectedIndexes();
    foreach (const QModelIndex &index, selectedIndexes) {
        if (index.column() == SearchSortModel::NAME) {
            QSortFilterProxyModel* model = all_tab.at(tabWidget->currentIndex())->getCurrentSearchListProxy();
            const QString desc_url = model->data(model->index(index.row(), SearchSortModel::DESC_LINK)).toString();
            if (!desc_url.isEmpty())
                QDesktopServices::openUrl(QUrl::fromEncoded(desc_url.toUtf8()));
        }
    }
}

void SearchWidget::on_copyURLBtn_clicked()
{
    QStringList urls;
    QModelIndexList selectedIndexes = all_tab.at(tabWidget->currentIndex())->getCurrentTreeView()->selectionModel()->selectedIndexes();
    foreach (const QModelIndex &index, selectedIndexes) {
        if (index.column() == SearchSortModel::NAME) {
            QSortFilterProxyModel* model = all_tab.at(tabWidget->currentIndex())->getCurrentSearchListProxy();
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
