/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Christophe Dumez
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

#include <QDir>
#include <QTreeView>
#include <QHeaderView>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QClipboard>
#include <QLabel>
#include <QVBoxLayout>
#include <QDesktopServices>
#include <QUrl>
#ifdef QBT_USES_QT5
#include <QTableView>
#endif

#include "base/bittorrent/session.h"
#include "base/utils/misc.h"
#include "base/preferences.h"
#include "base/searchengine.h"
#include "addnewtorrentdialog.h"
#include "searchsortmodel.h"
#include "searchlistdelegate.h"
#include "searchwidget.h"
#include "searchtab.h"

SearchTab::SearchTab(SearchEngine *searchEngine, SearchWidget *parent)
    : QWidget(parent)
    , m_searchEngine(searchEngine)
    , m_parent(parent)
    , m_isActive(true)
    , m_nbSearchResults(0)
{
    m_box = new QVBoxLayout(this);
    m_resultsLbl = new QLabel(this);
    m_resultsBrowser = new QTreeView(this);
#ifdef QBT_USES_QT5
    // This hack fixes reordering of first column with Qt5.
    // https://github.com/qtproject/qtbase/commit/e0fc088c0c8bc61dbcaf5928b24986cd61a22777
    QTableView unused;
    unused.setVerticalHeader(m_resultsBrowser->header());
    m_resultsBrowser->header()->setParent(m_resultsBrowser);
    unused.setVerticalHeader(new QHeaderView(Qt::Horizontal));
#endif
    m_resultsBrowser->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_box->addWidget(m_resultsLbl);
    m_box->addWidget(m_resultsBrowser);

    setLayout(m_box);

    // Set Search results list model
    m_searchListModel = new QStandardItemModel(0, SearchSortModel::NB_SEARCH_COLUMNS, this);
    m_searchListModel->setHeaderData(SearchSortModel::NAME, Qt::Horizontal, tr("Name", "i.e: file name"));
    m_searchListModel->setHeaderData(SearchSortModel::SIZE, Qt::Horizontal, tr("Size", "i.e: file size"));
    m_searchListModel->setHeaderData(SearchSortModel::SEEDS, Qt::Horizontal, tr("Seeders", "i.e: Number of full sources"));
    m_searchListModel->setHeaderData(SearchSortModel::LEECHS, Qt::Horizontal, tr("Leechers", "i.e: Number of partial sources"));
    m_searchListModel->setHeaderData(SearchSortModel::ENGINE_URL, Qt::Horizontal, tr("Search engine"));

    m_proxyModel = new SearchSortModel(this);
    m_proxyModel->setDynamicSortFilter(true);
    m_proxyModel->setSourceModel(m_searchListModel);
    m_resultsBrowser->setModel(m_proxyModel);

    m_searchDelegate = new SearchListDelegate(this);
    m_resultsBrowser->setItemDelegate(m_searchDelegate);

    m_resultsBrowser->hideColumn(SearchSortModel::DL_LINK); // Hide url column
    m_resultsBrowser->hideColumn(SearchSortModel::DESC_LINK);

    m_resultsBrowser->setRootIsDecorated(false);
    m_resultsBrowser->setAllColumnsShowFocus(true);
    m_resultsBrowser->setSortingEnabled(true);

    connect(m_resultsBrowser, SIGNAL(doubleClicked(const QModelIndex&)), SLOT(downloadSelectedItem(const QModelIndex&)));
    connect(m_resultsBrowser->header(), SIGNAL(sectionResized(int, int, int)), SLOT(saveResultsColumnsWidth()));

    // Load last columns width for search results list
    if (!loadColWidthResultsList())
        m_resultsBrowser->header()->resizeSection(0, 275);

    // Sort by Seeds
    m_resultsBrowser->sortByColumn(SearchSortModel::SEEDS, Qt::DescendingOrder);

    m_resultsLbl->setText(tr("Results <i>(%1)</i>:", "i.e: Search results").arg(0));

    connect(m_searchEngine, SIGNAL(searchStarted()), SLOT(searchStarted()));
    connect(m_searchEngine, SIGNAL(newSearchResults(QList<SearchResult>)), SLOT(appendSearchResults(QList<SearchResult>)));
    connect(m_searchEngine, SIGNAL(searchFinished(bool)), SLOT(searchFinished(bool)));
    connect(m_searchEngine, SIGNAL(searchFailed()), SLOT(searchFailed()));
}

void SearchTab::downloadSelectedItem(const QModelIndex &index)
{
    QString torrentUrl = m_proxyModel->data(m_proxyModel->index(index.row(), SearchSortModel::DL_LINK)).toString();
    setRowColor(index.row(), "blue");
    downloadTorrent(torrentUrl);
}

void SearchTab::searchStarted()
{
    // Update SearchEngine widgets
    m_status = tr("Searching...");
    emit statusUpdated();
}

// Slot called when search is Finished
// Search can be finished for 3 reasons :
// Error | Stopped by user | Finished normally
void SearchTab::searchFinished(bool cancelled)
{
    if (cancelled)
        m_status = tr("Search aborted");
    else if (m_nbSearchResults == 0)
        m_status = tr("Search returned no results");
    else
        m_status = tr("Search has finished");

    m_searchEngine->disconnect(this);
    m_isActive = false;
    emit statusUpdated();
}

void SearchTab::searchFailed()
{
#ifdef Q_OS_WIN
    m_status = tr("Search aborted");
#else
    m_status = tr("An error occurred during search...");
#endif

    m_searchEngine->disconnect(this);
    m_isActive = false;
    emit statusUpdated();
}

void SearchTab::appendSearchResults(const QList<SearchResult> &results)
{
    foreach (const SearchResult &result, results) {
        // Add item to search result list
        int row = m_searchListModel->rowCount();
        m_searchListModel->insertRow(row);

        m_searchListModel->setData(m_searchListModel->index(row, SearchSortModel::DL_LINK), result.fileUrl); // download URL
        m_searchListModel->setData(m_searchListModel->index(row, SearchSortModel::NAME), result.fileName); // Name
        m_searchListModel->setData(m_searchListModel->index(row, SearchSortModel::SIZE), result.fileSize); // Size
        m_searchListModel->setData(m_searchListModel->index(row, SearchSortModel::SEEDS), result.nbSeeders); // Seeders
        m_searchListModel->setData(m_searchListModel->index(row, SearchSortModel::LEECHS), result.nbLeechers); // Leechers
        m_searchListModel->setData(m_searchListModel->index(row, SearchSortModel::ENGINE_URL), result.siteUrl); // Search site URL
        m_searchListModel->setData(m_searchListModel->index(row, SearchSortModel::DESC_LINK), result.descrLink); // Description Link
    }

    m_nbSearchResults += results.size();
    m_resultsLbl->setText(tr("Results <i>(%1)</i>:", "i.e: Search results").arg(m_nbSearchResults));

    emit statusUpdated();
}

void SearchTab::saveResultsColumnsWidth()
{
    QStringList newWidthList;
    short nbColumns = m_searchListModel->columnCount();
    for (short i = 0; i < nbColumns; ++i)
        if (m_resultsBrowser->columnWidth(i) > 0)
            newWidthList << QString::number(m_resultsBrowser->columnWidth(i));
    // Don't save the width of the last column (auto column width)
    newWidthList.removeLast();
    Preferences::instance()->setSearchColsWidth(newWidthList.join(" "));
}

// Download selected items in search results list
void SearchTab::download()
{
    foreach (const QModelIndex &index, m_resultsBrowser->selectionModel()->selectedIndexes()) {
        if (index.column() == SearchSortModel::NAME) {
            downloadTorrent(m_proxyModel->data(m_proxyModel->index(index.row(), SearchSortModel::DL_LINK)).toString());
            setRowColor(index.row(), "blue");
        }
    }
}

void SearchTab::goToDescriptionPage()
{
    foreach (const QModelIndex &index, m_resultsBrowser->selectionModel()->selectedIndexes()) {
        if (index.column() == SearchSortModel::NAME) {
            const QString descUrl = m_proxyModel->data(m_proxyModel->index(index.row(), SearchSortModel::DESC_LINK)).toString();
            if (!descUrl.isEmpty())
                QDesktopServices::openUrl(QUrl::fromEncoded(descUrl.toUtf8()));
        }
    }
}

void SearchTab::copyURLs()
{
    QStringList urls;
    foreach (const QModelIndex &index, m_resultsBrowser->selectionModel()->selectedIndexes()) {
        if (index.column() == SearchSortModel::NAME) {
            const QString descUrl = m_proxyModel->data(m_proxyModel->index(index.row(), SearchSortModel::DESC_LINK)).toString();
            if (!descUrl.isEmpty())
                urls << descUrl.toUtf8();
        }
    }

    if (!urls.empty()) {
        QClipboard *clipboard = QApplication::clipboard();
        clipboard->setText(urls.join("\n"));
    }
}

void SearchTab::downloadTorrent(QString url)
{
    if (Preferences::instance()->useAdditionDialog())
        AddNewTorrentDialog::show(url, this);
    else
        BitTorrent::Session::instance()->addTorrent(url);
}

ulong SearchTab::searchResultsCount() const
{
    return m_nbSearchResults;
}

bool SearchTab::isActive() const
{
    return m_isActive;
}

bool SearchTab::loadColWidthResultsList()
{
    QString line = Preferences::instance()->getSearchColsWidth();
    if (line.isEmpty()) return false;

    QStringList widthList = line.split(' ');
    if (widthList.size() > m_searchListModel->columnCount())
        return false;

    unsigned int listSize = widthList.size();
    for (unsigned int i = 0; i < listSize; ++i) {
        m_resultsBrowser->header()->resizeSection(i, widthList.at(i).toInt());
    }

    return true;
}

// Set the color of a row in data model
void SearchTab::setRowColor(int row, QString color)
{
    m_proxyModel->setDynamicSortFilter(false);
    for (int i = 0; i < m_proxyModel->columnCount(); ++i) {
        m_proxyModel->setData(m_proxyModel->index(row, i), QVariant(QColor(color)), Qt::ForegroundRole);
    }

    m_proxyModel->setDynamicSortFilter(true);
}

QString SearchTab::status() const
{
    return m_status;
}
