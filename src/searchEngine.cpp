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

#include <QStandardItemModel>
#include <QHeaderView>
#include <QCompleter>
#include <QSettings>
#include <QMessageBox>
#include <QSystemTrayIcon>
#include <QTemporaryFile>
#include <QSystemTrayIcon>
#include <iostream>
#include <QTimer>

#include "SearchListDelegate.h"
#include "searchEngine.h"
#include "bittorrent.h"
#include "downloadThread.h"

#define SEARCH_NAME 0
#define SEARCH_SIZE 1
#define SEARCH_SEEDERS 2
#define SEARCH_LEECHERS 3
#define SEARCH_ENGINE 4

#define SEARCHHISTORY_MAXSIZE 50

SearchEngine::SearchEngine(bittorrent *BTSession, QSystemTrayIcon *myTrayIcon, bool systrayIntegration) : QWidget(), BTSession(BTSession), myTrayIcon(myTrayIcon), systrayIntegration(systrayIntegration){
  setupUi(this);
  downloader = new downloadThread(this);
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
  // Load last columns width for search results list
  if(!loadColWidthSearchList()){
    resultsBrowser->header()->resizeSection(0, 275);
  }

  // new qCompleter to the search pattern
  startSearchHistory();
  searchCompleter = new QCompleter(searchHistory, this);
  searchCompleter->setCaseSensitivity(Qt::CaseInsensitive);
  search_pattern->setCompleter(searchCompleter);

  // Boolean initialization
  search_stopped = false;
  // Connect signals to slots (search part)
  connect(resultsBrowser, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(downloadSelectedItem(const QModelIndex&)));
  connect(resultsBrowser->header(), SIGNAL(sectionPressed(int)), this, SLOT(sortSearchList(int)));
  // Creating Search Process
  searchProcess = new QProcess(this);
  connect(searchProcess, SIGNAL(started()), this, SLOT(searchStarted()));
  connect(searchProcess, SIGNAL(readyReadStandardOutput()), this, SLOT(readSearchOutput()));
  connect(searchProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(searchFinished(int,QProcess::ExitStatus)));
  searchTimeout = new QTimer(this);
  searchTimeout->setSingleShot(true);
  connect(searchTimeout, SIGNAL(timeout()), this, SLOT(on_stop_search_button_clicked()));
  // Check last enabled search engines
  loadEngineSettings();
  // Update nova.py search plugin if necessary
  updateNova();
}

SearchEngine::~SearchEngine(){
  qDebug("Search destruction");
  // save the searchHistory for later uses
  saveSearchHistory();
  saveColWidthSearchList();
  searchProcess->kill();
  searchProcess->waitForFinished();
  delete searchTimeout;
  delete searchProcess;
  delete searchCompleter;
  delete SearchListModel;
  delete SearchDelegate;
  delete downloader;
}

// Set the color of a row in data model
void SearchEngine::setRowColor(int row, QString color){
  for(int i=0; i<SearchListModel->columnCount(); ++i){
    SearchListModel->setData(SearchListModel->index(row, i), QVariant(QColor(color)), Qt::ForegroundRole);
  }
}

void SearchEngine::sortSearchList(int index){
  static Qt::SortOrder sortOrder = Qt::AscendingOrder;
  if(resultsBrowser->header()->sortIndicatorSection() == index){
    if(sortOrder == Qt::AscendingOrder){
      sortOrder = Qt::DescendingOrder;
    }else{
      sortOrder = Qt::AscendingOrder;
    }
  }
  resultsBrowser->header()->setSortIndicator(index, sortOrder);
  switch(index){
    //case SIZE:
    case SEEDERS:
    case LEECHERS:
    case SIZE:
      sortSearchListInt(index, sortOrder);
      break;
    default:
      sortSearchListString(index, sortOrder);
  }
}

void SearchEngine::sortSearchListInt(int index, Qt::SortOrder sortOrder){
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

void SearchEngine::sortSearchListString(int index, Qt::SortOrder sortOrder){
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
void SearchEngine::saveColWidthSearchList() const{
  qDebug("Saving columns width in search list");
  QSettings settings("qBittorrent", "qBittorrent");
  QStringList width_list;
  for(int i=0; i<SearchListModel->columnCount(); ++i){
    width_list << misc::toQString(resultsBrowser->columnWidth(i));
  }
  settings.setValue("SearchListColsWidth", width_list.join(" "));
  qDebug("Search list columns width saved");
}

void SearchEngine::on_enginesButton_clicked() {
  engineSelectDlg *dlg = new engineSelectDlg(this);
  connect(dlg, SIGNAL(enginesChanged()), this, SLOT(loadEngineSettings()));
}

// Load columns width in a file that were saved previously
// (search list)
bool SearchEngine::loadColWidthSearchList(){
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

// get the last searchs from a QSettings to a QStringList
void SearchEngine::startSearchHistory(){
  QSettings settings("qBittorrent", "qBittorrent");
  settings.beginGroup("Search");
  searchHistory = settings.value("searchHistory",-1).toStringList();
  settings.endGroup();
}

void SearchEngine::loadEngineSettings() {
  qDebug("Loading engine settings");
  enabled_engines.clear();
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  QStringList known_engines = settings.value(QString::fromUtf8("SearchEngines/knownEngines"), QStringList()).toStringList();
  QVariantList known_enginesEnabled = settings.value(QString::fromUtf8("SearchEngines/knownEnginesEnabled"), QList<QVariant>()).toList();
  QString engine;
  unsigned int i = 0;
  foreach(engine, known_engines) {
    if(known_enginesEnabled.at(i).toBool())
      enabled_engines << engine;
    ++i;
  }
  if(enabled_engines.empty())
    enabled_engines << "all";
  qDebug("Engine settings loaded");
}

// Save the history list into the QSettings for the next session
void SearchEngine::saveSearchHistory()
{
  QSettings settings("qBittorrent", "qBittorrent");
  settings.beginGroup("Search");
  settings.setValue("searchHistory",searchHistory);
  settings.endGroup();
}

// Function called when we click on search button
void SearchEngine::on_search_button_clicked(){
  if(searchProcess->state() != QProcess::NotRunning){
    searchProcess->kill();
    searchProcess->waitForFinished();
  }
  if(searchTimeout->isActive()) {
    searchTimeout->stop();
  }
  QString pattern = search_pattern->text().trimmed();
  // No search pattern entered
  if(pattern.isEmpty()){
    QMessageBox::critical(0, tr("Empty search pattern"), tr("Please type a search pattern first"));
    return;
  }
  // if the pattern is not in the pattern
  if(searchHistory.indexOf(pattern) == -1){
    //update the searchHistory list
    searchHistory.append(pattern);
    // verify the max size of the history
    if(searchHistory.size() > SEARCHHISTORY_MAXSIZE)
      searchHistory = searchHistory.mid(searchHistory.size()/2,searchHistory.size()/2);
    searchCompleter = new QCompleter(searchHistory, this);
    searchCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    search_pattern->setCompleter(searchCompleter);
  }


  // Getting checked search engines
  Q_ASSERT(!enabled_engines.empty());
  QStringList params;
  QStringList engineNames;
  search_stopped = false;

  params << enabled_engines.join(",");
  params << pattern.split(" ");
  // Update SearchEngine widgets
  no_search_results = true;
  nb_search_results = 0;
  search_result_line_truncated.clear();
  results_lbl->setText(tr("Results")+" <i>(0)</i>:");
  // Launch search
  searchProcess->start(misc::qBittorrentPath()+"search_engine"+QDir::separator()+"nova2.py", params, QIODevice::ReadOnly);
  searchTimeout->start(180000); // 3min
}

void SearchEngine::searchStarted(){
  // Update SearchEngine widgets
  search_status->setText(tr("Searching..."));
  search_status->repaint();
  stop_search_button->setEnabled(true);
  stop_search_button->repaint();
  // clear results window
  SearchListModel->removeRows(0, SearchListModel->rowCount());
  // Clear previous results urls too
  searchResultsUrls.clear();
}

// Download the given item from search results list
void SearchEngine::downloadSelectedItem(const QModelIndex& index){
  int row = index.row();
  // Get Item url
  QString url = searchResultsUrls.value(SearchListModel->data(SearchListModel->index(row, NAME)).toString());
  // Download from url
  BTSession->downloadFromUrl(url);
  // Set item color to RED
  setRowColor(row, "red");
}

// search Qprocess return output as soon as it gets new
// stuff to read. We split it into lines and add each
// line to search results calling appendSearchResult().
void SearchEngine::readSearchOutput(){
  QByteArray output = searchProcess->readAllStandardOutput();
  QList<QByteArray> lines_list = output.split('\n');
  QByteArray line;
  if(!search_result_line_truncated.isEmpty()){
    QByteArray end_of_line = lines_list.takeFirst();
    lines_list.prepend(search_result_line_truncated+end_of_line);
  }
  search_result_line_truncated = lines_list.takeLast().trimmed();
  foreach(line, lines_list){
    appendSearchResult(QString(line));
  }
  results_lbl->setText(tr("Results")+QString::fromUtf8(" <i>(")+misc::toQString(nb_search_results)+QString::fromUtf8(")</i>:"));
}

// Update nova.py search plugin if necessary
void SearchEngine::updateNova() {
  qDebug("Updating nova");
  // create search_engine directory if necessary
  QDir search_dir(misc::qBittorrentPath()+"search_engine");
  if(!search_dir.exists()){
    search_dir.mkdir(misc::qBittorrentPath()+"search_engine");
  }
  QFile package_file(search_dir.path()+QDir::separator()+"__init__.py");
  package_file.open(QIODevice::WriteOnly | QIODevice::Text);
  package_file.close();
  if(!search_dir.exists("engines")){
    search_dir.mkdir("engines");
  }
  QFile package_file2(search_dir.path()+QDir::separator()+"engines"+QDir::separator()+"__init__.py");
  package_file2.open(QIODevice::WriteOnly | QIODevice::Text);
  package_file2.close();
  // Copy search plugin files (if necessary)
  QString filePath = misc::qBittorrentPath()+"search_engine"+QDir::separator()+"nova2.py";
  if(misc::getPluginVersion(":/search_engine/nova2.py") > misc::getPluginVersion(filePath)) {
    if(QFile::exists(filePath))
      QFile::remove(filePath);
    QFile::copy(":/search_engine/nova2.py", filePath);
  }
  // Set permissions
  QFile::Permissions perm=QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadUser | QFile::WriteUser | QFile::ExeUser | QFile::ReadGroup | QFile::ReadGroup;
  QFile(misc::qBittorrentPath()+"search_engine"+QDir::separator()+"nova2.py").setPermissions(perm);
  filePath = misc::qBittorrentPath()+"search_engine"+QDir::separator()+"novaprinter.py";
  if(misc::getPluginVersion(":/search_engine/novaprinter.py") > misc::getPluginVersion(filePath)) {
    if(QFile::exists(filePath)){
      QFile::remove(filePath);
    }
    QFile::copy(":/search_engine/novaprinter.py", filePath);
  }
  QString destDir = misc::qBittorrentPath()+"search_engine"+QDir::separator()+"engines"+QDir::separator();
  QDir shipped_subDir(":/search_engine/engines/");
  QStringList files = shipped_subDir.entryList();
  QString file;
  foreach(file, files){
    QString shipped_file = shipped_subDir.path()+QDir::separator()+file;
    // Copy python classes
    if(file.endsWith(".py")) {
      if(misc::getPluginVersion(shipped_file) > misc::getPluginVersion(destDir+file) ) {
        qDebug("shippped %s is more recent then local plugin, updating", file.toUtf8().data());
        if(QFile::exists(destDir+file)) {
          qDebug("Removing old %s", (destDir+file).toUtf8().data());
          QFile::remove(destDir+file);
        }
        qDebug("%s copied to %s", shipped_file.toUtf8().data(), (destDir+file).toUtf8().data());
        QFile::copy(shipped_file, destDir+file);
      }
    } else {
      // Copy icons
      if(file.endsWith(".png")) {
        if(!QFile::exists(destDir+file)) {
          QFile::copy(shipped_file, destDir+file);
        }
      }
    }
  }
}

// Slot called when search is Finished
// Search can be finished for 3 reasons :
// Error | Stopped by user | Finished normally
void SearchEngine::searchFinished(int exitcode,QProcess::ExitStatus){
  QSettings settings("qBittorrent", "qBittorrent");
  bool useNotificationBalloons = settings.value("Preferences/General/NotificationBaloons", true).toBool();
  if(systrayIntegration && useNotificationBalloons) {
    myTrayIcon->showMessage(tr("Search Engine"), tr("Search has finished"), QSystemTrayIcon::Information, TIME_TRAY_BALLOON);
  }
  if(exitcode){
    search_status->setText(tr("An error occured during search..."));
  }else{
    if(search_stopped){
      search_status->setText(tr("Search aborted"));
    }else{
      if(no_search_results){
        search_status->setText(tr("Search returned no results"));
      }else{
        search_status->setText(tr("Search has finished"));
      }
    }
  }
  results_lbl->setText(tr("Results", "i.e: Search results")+QString::fromUtf8(" <i>(")+misc::toQString(nb_search_results)+QString::fromUtf8(")</i>:"));
  search_button->setEnabled(true);
  stop_search_button->setEnabled(false);
}

// SLOT to append one line to search results list
// Line is in the following form :
// file url | file name | file size | nb seeds | nb leechers | Search engine url
void SearchEngine::appendSearchResult(QString line){
  QStringList parts = line.split("|");
  if(parts.size() != 6){
    return;
  }
  QString url = parts.takeFirst();
  QString filename = parts.first();
  // XXX: Two results can't have the same name (right?)
  if(searchResultsUrls.contains(filename)){
    return;
  }
  // Add item to search result list
  int row = SearchListModel->rowCount();
  SearchListModel->insertRow(row);
  for(int i=0; i<5; ++i){
    if(parts.at(i).toFloat() == -1 && i != SIZE)
      SearchListModel->setData(SearchListModel->index(row, i), tr("Unknown"));
    else
      SearchListModel->setData(SearchListModel->index(row, i), QVariant(parts.at(i)));
  }
  // Add url to searchResultsUrls associative array
  searchResultsUrls.insert(filename, url);
  no_search_results = false;
  ++nb_search_results;
  // Enable clear & download buttons
  clear_button->setEnabled(true);
  download_button->setEnabled(true);
}

// Stop search while it is working in background
void SearchEngine::on_stop_search_button_clicked(){
  // Kill process
  searchProcess->terminate();
  search_stopped = true;
  searchTimeout->stop();
}

// Clear search results list
void SearchEngine::on_clear_button_clicked(){
  // Kill process
  searchProcess->terminate();
  search_stopped = true;

  searchResultsUrls.clear();
  SearchListModel->removeRows(0, SearchListModel->rowCount());
  // Disable clear & download buttons
  clear_button->setEnabled(false);
  download_button->setEnabled(false);
  results_lbl->setText(tr("Results")+" <i>(0)</i>:");
  // focus on search pattern
  search_pattern->clear();
  search_pattern->setFocus();
}

void SearchEngine::on_clearPatternButton_clicked() {
  search_pattern->clear();
  search_pattern->setFocus();
}

// Download selected items in search results list
void SearchEngine::on_download_button_clicked(){
  QModelIndexList selectedIndexes = resultsBrowser->selectionModel()->selectedIndexes();
  QModelIndex index;
  foreach(index, selectedIndexes){
    if(index.column() == NAME){
      // Get Item url
      QString url = searchResultsUrls.value(index.data().toString());
      BTSession->downloadFromUrl(url);
      setRowColor(index.row(), "red");
    }
  }
}
