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
 * Contact : chris@qbittorrent.org
 */

#include <QDir>
#include <QTreeView>
#include <QStandardItemModel>
#include <QHeaderView>
#include <QSettings>

#include "SearchTab.h"
#include "SearchListDelegate.h"
#include "misc.h"
#include "searchEngine.h"

#define SEARCH_NAME 0
#define SEARCH_SIZE 1
#define SEARCH_SEEDERS 2
#define SEARCH_LEECHERS 3
#define SEARCH_ENGINE 4

SearchTab::SearchTab(SearchEngine *parent) : QWidget()
{
    box=new QVBoxLayout();
    results_lbl=new QLabel();
    resultsBrowser = new QTreeView();
    resultsBrowser->setSelectionMode(QAbstractItemView::ExtendedSelection);
    box->addWidget(results_lbl);
    box->addWidget(resultsBrowser);
    
    setLayout(box);
    // Set Search results list model
    SearchListModel = new QStandardItemModel(0,5);
    SearchListModel->setHeaderData(SEARCH_NAME, Qt::Horizontal, tr("Name", "i.e: file name"));
    SearchListModel->setHeaderData(SEARCH_SIZE, Qt::Horizontal, tr("Size", "i.e: file size"));
    SearchListModel->setHeaderData(SEARCH_SEEDERS, Qt::Horizontal, tr("Seeders", "i.e: Number of full sources"));
    SearchListModel->setHeaderData(SEARCH_LEECHERS, Qt::Horizontal, tr("Leechers", "i.e: Number of partial sources"));
    SearchListModel->setHeaderData(SEARCH_ENGINE, Qt::Horizontal, tr("Search engine"));
    resultsBrowser->setModel(SearchListModel);
    SearchDelegate = new SearchListDelegate();
    resultsBrowser->setItemDelegate(SearchDelegate);
    // Make search list header clickable for sorting
    resultsBrowser->header()->setClickable(true);
    resultsBrowser->header()->setSortIndicatorShown(true);
    
    // Connect signals to slots (search part)
    connect(resultsBrowser, SIGNAL(doubleClicked(const QModelIndex&)), parent, SLOT(downloadSelectedItem(const QModelIndex&)));
    connect(resultsBrowser->header(), SIGNAL(sectionPressed(int)), this, SLOT(sortSearchList(int)));
    
    // Load last columns width for search results list
    if(!loadColWidthSearchList()){
      resultsBrowser->header()->resizeSection(0, 275);
    }
}

SearchTab::~SearchTab()
{
    saveColWidthSearchList();
    delete resultsBrowser;
    delete SearchListModel;
    delete SearchDelegate;
}

QLabel* SearchTab::getCurrentLabel()
{
    return results_lbl;
}

QTreeView* SearchTab::getCurrentTreeView()
{
    return resultsBrowser;
}

QStandardItemModel* SearchTab::getCurrentSearchListModel()
{
    return SearchListModel;
}

// Set the color of a row in data model
void SearchTab::setRowColor(int row, QString color){
  for(int i=0; i<SearchListModel->columnCount(); ++i){
    SearchListModel->setData(SearchListModel->index(row, i), QVariant(QColor(color)), Qt::ForegroundRole);
  }
}

void SearchTab::sortSearchList(int index){
  static Qt::SortOrder sortOrder = Qt::AscendingOrder;
  if(resultsBrowser->header()->sortIndicatorSection() == index){
      sortOrder = (sortOrder == Qt::DescendingOrder) ? Qt::AscendingOrder : Qt::DescendingOrder; ;
  }
  resultsBrowser->header()->setSortIndicator(index, sortOrder);
  switch(index){
    case SEEDERS:
    case LEECHERS:
    case SIZE:
      sortSearchListInt(index, sortOrder);
      break;
    default:
      sortSearchListString(index, sortOrder);
  }
}

void SearchTab::sortSearchListInt(int index, Qt::SortOrder sortOrder){
  QList<QPair<int, qlonglong> > lines;
  // Insertion sorting
  for(int i=0; i<SearchListModel->rowCount(); ++i){
    misc::insertSort(lines, QPair<int,qlonglong>(i, SearchListModel->data(SearchListModel->index(i, index)).toLongLong()), sortOrder);
  }
  // Insert items in new model, in correct order
  int nbRows_old = lines.size();
  for(int row=0; row<lines.size(); ++row){
    SearchListModel->insertRow(SearchListModel->rowCount());
    int sourceRow = lines[row].first;
    for(int col=0; col<5; ++col){
      SearchListModel->setData(SearchListModel->index(nbRows_old+row, col), SearchListModel->data(SearchListModel->index(sourceRow, col)));
      SearchListModel->setData(SearchListModel->index(nbRows_old+row, col), SearchListModel->data(SearchListModel->index(sourceRow, col), Qt::ForegroundRole), Qt::ForegroundRole);
    }
  }
  // Remove old rows
  SearchListModel->removeRows(0, nbRows_old);
}

void SearchTab::sortSearchListString(int index, Qt::SortOrder sortOrder){
  QList<QPair<int, QString> > lines;
  // Insetion sorting
  for(int i=0; i<SearchListModel->rowCount(); ++i){
    misc::insertSortString(lines, QPair<int, QString>(i, SearchListModel->data(SearchListModel->index(i, index)).toString()), sortOrder);
  }
  // Insert items in new model, in correct order
  int nbRows_old = lines.size();
  for(int row=0; row<nbRows_old; ++row){
    SearchListModel->insertRow(SearchListModel->rowCount());
    int sourceRow = lines[row].first;
    for(int col=0; col<5; ++col){
      SearchListModel->setData(SearchListModel->index(nbRows_old+row, col), SearchListModel->data(SearchListModel->index(sourceRow, col)));
      SearchListModel->setData(SearchListModel->index(nbRows_old+row, col), SearchListModel->data(SearchListModel->index(sourceRow, col), Qt::ForegroundRole), Qt::ForegroundRole);
    }
  }
  // Remove old rows
  SearchListModel->removeRows(0, nbRows_old);
}

// Save columns width in a file to remember them
// (download list)
void SearchTab::saveColWidthSearchList() const{
  qDebug("Saving columns width in search list");
  QSettings settings("qBittorrent", "qBittorrent");
  QStringList width_list;
  for(int i=0; i<SearchListModel->columnCount(); ++i){
    width_list << misc::toQString(resultsBrowser->columnWidth(i));
  }
  settings.setValue("SearchListColsWidth", width_list.join(" "));
  qDebug("Search list columns width saved");
}

// Load columns width in a file that were saved previously
// (search list)
bool SearchTab::loadColWidthSearchList(){
  qDebug("Loading columns width for search list");
  QSettings settings("qBittorrent", "qBittorrent");
  QString line = settings.value("SearchListColsWidth", QString()).toString();
  if(line.isEmpty())
    return false;
  QStringList width_list = line.split(' ');
  if(width_list.size() != SearchListModel->columnCount())
    return false;
  for(int i=0; i<width_list.size(); ++i){
        resultsBrowser->header()->resizeSection(i, width_list.at(i).toInt());
  }
  qDebug("Search list columns width loaded");
  return true;
}

