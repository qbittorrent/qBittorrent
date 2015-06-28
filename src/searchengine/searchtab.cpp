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
#include <QStandardItemModel>
#include <QHeaderView>
#include <QSortFilterProxyModel>

#include "searchtab.h"
#include "searchlistdelegate.h"
#include "core/utils/misc.h"
#include "searchengine.h"
#include "core/preferences.h"

SearchTab::SearchTab(SearchEngine *parent) : QWidget(), parent(parent)
{
  box=new QVBoxLayout();
  results_lbl=new QLabel();
  resultsBrowser = new QTreeView();
  resultsBrowser->setSelectionMode(QAbstractItemView::ExtendedSelection);
  box->addWidget(results_lbl);
  box->addWidget(resultsBrowser);

  setLayout(box);
  // Set Search results list model
  SearchListModel = new QStandardItemModel(0, SearchSortModel::NB_SEARCH_COLUMNS);
  SearchListModel->setHeaderData(SearchSortModel::NAME, Qt::Horizontal, tr("Name", "i.e: file name"));
  SearchListModel->setHeaderData(SearchSortModel::SIZE, Qt::Horizontal, tr("Size", "i.e: file size"));
  SearchListModel->setHeaderData(SearchSortModel::SEEDS, Qt::Horizontal, tr("Seeders", "i.e: Number of full sources"));
  SearchListModel->setHeaderData(SearchSortModel::LEECHS, Qt::Horizontal, tr("Leechers", "i.e: Number of partial sources"));
  SearchListModel->setHeaderData(SearchSortModel::ENGINE_URL, Qt::Horizontal, tr("Search engine"));

  proxyModel = new SearchSortModel();
  proxyModel->setDynamicSortFilter(true);
  proxyModel->setSourceModel(SearchListModel);
  resultsBrowser->setModel(proxyModel);

  SearchDelegate = new SearchListDelegate();
  resultsBrowser->setItemDelegate(SearchDelegate);

  resultsBrowser->hideColumn(SearchSortModel::DL_LINK); // Hide url column
  resultsBrowser->hideColumn(SearchSortModel::DESC_LINK);

  resultsBrowser->setRootIsDecorated(false);
  resultsBrowser->setAllColumnsShowFocus(true);
  resultsBrowser->setSortingEnabled(true);

  // Connect signals to slots (search part)
  connect(resultsBrowser, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(downloadSelectedItem(const QModelIndex&)));

  // Load last columns width for search results list
  if (!loadColWidthResultsList()) {
    resultsBrowser->header()->resizeSection(0, 275);
  }

  // Sort by Seeds
  resultsBrowser->sortByColumn(SearchSortModel::SEEDS, Qt::DescendingOrder);
}

void SearchTab::downloadSelectedItem(const QModelIndex& index) {
  QString engine_url = proxyModel->data(proxyModel->index(index.row(), SearchSortModel::ENGINE_URL)).toString();
  QString torrent_url = proxyModel->data(proxyModel->index(index.row(), SearchSortModel::DL_LINK)).toString();
  setRowColor(index.row(), "blue");
  parent->downloadTorrent(engine_url, torrent_url);
}

SearchTab::~SearchTab() {
  delete box;
  delete results_lbl;
  delete resultsBrowser;
  delete SearchListModel;
  delete proxyModel;
  delete SearchDelegate;
}

QHeaderView* SearchTab::header() const {
  return resultsBrowser->header();
}

bool SearchTab::loadColWidthResultsList() {
  QString line = Preferences::instance()->getSearchColsWidth();
  if (line.isEmpty())
    return false;
  QStringList width_list = line.split(' ');
  if (width_list.size() > SearchListModel->columnCount())
    return false;
  unsigned int listSize = width_list.size();
  for (unsigned int i=0; i<listSize; ++i) {
    resultsBrowser->header()->resizeSection(i, width_list.at(i).toInt());
  }
  return true;
}

QLabel* SearchTab::getCurrentLabel()
{
  return results_lbl;
}

QTreeView* SearchTab::getCurrentTreeView()
{
  return resultsBrowser;
}

QSortFilterProxyModel* SearchTab::getCurrentSearchListProxy() const
{
  return proxyModel;
}

QStandardItemModel* SearchTab::getCurrentSearchListModel() const
{
  return SearchListModel;
}

// Set the color of a row in data model
void SearchTab::setRowColor(int row, QString color) {
  proxyModel->setDynamicSortFilter(false);
  for (int i=0; i<proxyModel->columnCount(); ++i) {
    proxyModel->setData(proxyModel->index(row, i), QVariant(QColor(color)), Qt::ForegroundRole);
  }
  proxyModel->setDynamicSortFilter(true);
}


