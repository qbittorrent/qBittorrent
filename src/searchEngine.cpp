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

SearchEngine::SearchEngine(bittorrent *BTSession, QSystemTrayIcon *myTrayIcon, bool systrayIntegration) : QWidget(), systrayIntegration(systrayIntegration){
  setupUi(this);
  this->BTSession = BTSession;
  this->myTrayIcon = myTrayIcon;
  downloader = new downloadThread(this);
  connect(downloader, SIGNAL(downloadFinished(QString, QString)), this, SLOT(novaUpdateDownloaded(QString, QString)));
  connect(downloader, SIGNAL(downloadFailure(QString, QString)), this, SLOT(handleNovaDownloadFailure(QString, QString)));
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
  // Set search engines names
  mininova->setText("Mininova");
  piratebay->setText("ThePirateBay");
//   reactor->setText("TorrentReactor");
  isohunt->setText("Isohunt");
//   btjunkie->setText("BTJunkie");
  meganova->setText("Meganova");
  // Check last checked search engines
  loadCheckedSearchEngines();
  connect(mininova, SIGNAL(stateChanged(int)), this, SLOT(saveCheckedSearchEngines(int)));
  connect(piratebay, SIGNAL(stateChanged(int)), this, SLOT(saveCheckedSearchEngines(int)));
//   connect(reactor, SIGNAL(stateChanged(int)), this, SLOT(saveCheckedSearchEngines(int)));
  connect(isohunt, SIGNAL(stateChanged(int)), this, SLOT(saveCheckedSearchEngines(int)));
//   connect(btjunkie, SIGNAL(stateChanged(int)), this, SLOT(saveCheckedSearchEngines(int)));
  connect(meganova, SIGNAL(stateChanged(int)), this, SLOT(saveCheckedSearchEngines(int)));
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
  delete searchProcess;
  delete searchCompleter;
  delete SearchListModel;
  delete SearchDelegate;
  delete downloader;
}

// Set the color of a row in data model
void SearchEngine::setRowColor(int row, QString color){
  for(int i=0; i<SearchListModel->columnCount(); ++i){
    SearchListModel->setData(SearchListModel->index(row, i), QVariant(QColor(color)), Qt::TextColorRole);
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
      SearchListModel->setData(SearchListModel->index(nbRows_old+row, col), SearchListModel->data(SearchListModel->index(sourceRow, col), Qt::TextColorRole), Qt::TextColorRole);
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
      SearchListModel->setData(SearchListModel->index(nbRows_old+row, col), SearchListModel->data(SearchListModel->index(sourceRow, col), Qt::TextColorRole), Qt::TextColorRole);
    }
  }
  // Remove old rows
  SearchListModel->removeRows(0, nbRows_old);
}

// Save last checked search engines to a file
void SearchEngine::saveCheckedSearchEngines(int) const{
  QSettings settings("qBittorrent", "qBittorrent");
  settings.beginGroup("SearchEngines");
  settings.setValue("mininova", mininova->isChecked());
  settings.setValue("piratebay", piratebay->isChecked());
  settings.setValue("isohunt", isohunt->isChecked());
  settings.setValue("meganova", meganova->isChecked());
  settings.endGroup();
  qDebug("Saved checked search engines");
}

// Save columns width in a file to remember them
// (download list)
void SearchEngine::saveColWidthSearchList() const{
  qDebug("Saving columns width in search list");
  QSettings settings("qBittorrent", "qBittorrent");
  QStringList width_list;
  for(int i=0; i<SearchListModel->columnCount(); ++i){
    width_list << QString(misc::toString(resultsBrowser->columnWidth(i)).c_str());
  }
  settings.setValue("SearchListColsWidth", width_list.join(" "));
  qDebug("Search list columns width saved");
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

// load last checked search engines from a file
void SearchEngine::loadCheckedSearchEngines(){
  qDebug("Loading checked search engines");
  QSettings settings("qBittorrent", "qBittorrent");
  settings.beginGroup("SearchEngines");
  mininova->setChecked(settings.value("mininova", true).toBool());
  piratebay->setChecked(settings.value("piratebay", false).toBool());
  isohunt->setChecked(settings.value("isohunt", false).toBool());
  meganova->setChecked(settings.value("meganova", false).toBool());
  settings.endGroup();
  qDebug("Loaded checked search engines");
}

// get the last searchs from a QSettings to a QStringList
void SearchEngine::startSearchHistory(){
  QSettings settings("qBittorrent", "qBittorrent");
  settings.beginGroup("Search");
  searchHistory = settings.value("searchHistory",-1).toStringList();
  settings.endGroup();
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
  if(!mininova->isChecked() && ! piratebay->isChecked()/* && !reactor->isChecked()*/ && !isohunt->isChecked()/* && !btjunkie->isChecked()*/ && !meganova->isChecked()){
    QMessageBox::critical(0, tr("No search engine selected"), tr("You must select at least one search engine."));
    return;
  }
  QStringList params;
  QStringList engineNames;
  search_stopped = false;
  // Get checked search engines
  if(mininova->isChecked()){
    engineNames << "mininova";
  }
  if(piratebay->isChecked()){
    engineNames << "piratebay";
  }
//   if(reactor->isChecked()){
//     engineNames << "reactor";
//   }
  if(isohunt->isChecked()){
    engineNames << "isohunt";
  }
//   if(btjunkie->isChecked()){
//     engineNames << "btjunkie";
//   }
  if(meganova->isChecked()){
    engineNames << "meganova";
  }
  params << engineNames.join(",");
  params << pattern.split(" ");
  // Update SearchEngine widgets
  no_search_results = true;
  nb_search_results = 0;
  search_result_line_truncated.clear();
  results_lbl->setText(tr("Results")+" <i>(0)</i>:");
  // Launch search
  searchProcess->start(misc::qBittorrentPath()+"nova.py", params, QIODevice::ReadOnly);
}

void SearchEngine::searchStarted(){
  // Update SearchEngine widgets
  search_button->setEnabled(false);
  search_button->repaint();
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
  results_lbl->setText(tr("Results")+" <i>("+QString(misc::toString(nb_search_results).c_str())+")</i>:");
}

// Returns version of nova.py search engine
float SearchEngine::getNovaVersion(QString novaPath) const{
  QFile dest_nova(novaPath);
  if(!dest_nova.exists()){
    return 0.0;
  }
  if(!dest_nova.open(QIODevice::ReadOnly | QIODevice::Text)){
    return 0.0;
  }
  float version = 0.0;
  while (!dest_nova.atEnd()){
    QByteArray line = dest_nova.readLine();
    if(line.startsWith("# Version: ")){
      line = line.split(' ').last();
      line.chop(1); // removes '\n'
      version = line.toFloat();
      qDebug("Search plugin version: %.2f", version);
      break;
    }
  }
  return version;
}

// Returns changelog of nova.py search engine
QByteArray SearchEngine::getNovaChangelog(QString novaPath) const{
  QFile dest_nova(novaPath);
  if(!dest_nova.exists()){
    return QByteArray("None");
  }
  if(!dest_nova.open(QIODevice::ReadOnly | QIODevice::Text)){
    return QByteArray("None");
  }
  QByteArray changelog;
  bool in_changelog = false;
  while (!dest_nova.atEnd()){
    QByteArray line = dest_nova.readLine();
    line = line.trimmed();
    if(line.startsWith("# Changelog:")){
      in_changelog = true;
    }else{
      if(in_changelog && line.isEmpty()){
        // end of changelog
        return changelog;
      }
      if(in_changelog){
        line.remove(0,1);
        changelog.append(line);
      }
    }
  }
  return changelog;
}

// Update nova.py search plugin if necessary
void SearchEngine::updateNova() const{
  qDebug("Updating nova");
  float provided_nova_version = getNovaVersion(":/search_engine/nova.py");
  QFile::Permissions perm=QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadUser | QFile::WriteUser | QFile::ExeUser | QFile::ReadGroup | QFile::ReadGroup;
  QFile(misc::qBittorrentPath()+"nova.py").setPermissions(perm);
  if(provided_nova_version > getNovaVersion(misc::qBittorrentPath()+"nova.py")){
    qDebug("updating local search plugin with shipped one");
    // nova.py needs update
    QFile::remove(misc::qBittorrentPath()+"nova.py");
    qDebug("Old nova removed");
    QFile::copy(":/search_engine/nova.py", misc::qBittorrentPath()+"nova.py");
    qDebug("New nova copied");
    QFile::Permissions perm=QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadUser | QFile::WriteUser | QFile::ExeUser | QFile::ReadGroup | QFile::ReadGroup;
    QFile(misc::qBittorrentPath()+"nova.py").setPermissions(perm);
    qDebug("local search plugin updated");
  }
}

void SearchEngine::novaUpdateDownloaded(QString url, QString filePath){
  float version_on_server = getNovaVersion(filePath);
  qDebug("Version on qbittorrent.org: %.2f", version_on_server);
  if(version_on_server > getNovaVersion(misc::qBittorrentPath()+"nova.py")){
    if(QMessageBox::question(this,
       tr("Search plugin update -- qBittorrent"),
          tr("Search plugin can be updated, do you want to update it?\n\nChangelog:\n")+getNovaChangelog(filePath),
             tr("&Yes"), tr("&No"),
                QString(), 0, 1)){
                  return;
                }else{
                  qDebug("Updating search plugin from qbittorrent.org");
                  QFile::remove(misc::qBittorrentPath()+"nova.py");
                  QFile::copy(filePath, misc::qBittorrentPath()+"nova.py");
                  QFile::Permissions perm=QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadUser | QFile::WriteUser | QFile::ExeUser | QFile::ReadGroup | QFile::ReadGroup;
                  QFile(misc::qBittorrentPath()+"nova.py").setPermissions(perm);
                }
  }else{
    if(version_on_server == 0.0){
      if(url == "http://www.dchris.eu/nova/nova.zip"){
        qDebug("*Warning: Search plugin update download from primary server failed, trying secondary server...");
        downloader->downloadUrl("http://hydr0g3n.free.fr/nova/nova.py");
      }else{
        QMessageBox::information(this, tr("Search plugin update")+" -- "+tr("qBittorrent"),
                               tr("Sorry, update server is temporarily unavailable."));
      }
    }else{
      QMessageBox::information(this, tr("Search plugin update -- qBittorrent"),
                               tr("Your search plugin is already up to date."));
    }
  }
  // Delete tmp file
  QFile::remove(filePath);
}

void SearchEngine::handleNovaDownloadFailure(QString url, QString reason){
  if(url == "http://www.dchris.eu/nova/nova.zip"){
    qDebug("*Warning: Search plugin update download from primary server failed, trying secondary server...");
    downloader->downloadUrl("http://hydr0g3n.free.fr/nova/nova.py");
  }else{
    // Display a message box
    QMessageBox::critical(0, tr("Search plugin download error"), tr("Couldn't download search plugin update at url: %1, reason: %2.").arg(url).arg(reason));
  }
}

// Download nova.py from qbittorrent.org
// Check if our nova.py is outdated and
// ask user for action.
void SearchEngine::on_update_nova_button_clicked(){
  qDebug("Checking for search plugin updates on qbittorrent.org");
  downloader->downloadUrl("http://www.dchris.eu/nova/nova.zip");
}

// Slot called when search is Finished
// Search can be finished for 3 reasons :
// Error | Stopped by user | Finished normally
void SearchEngine::searchFinished(int exitcode,QProcess::ExitStatus){
  QSettings settings("qBittorrent", "qBittorrent");
  int useOSD = settings.value("Options/OSDEnabled", 1).toInt();
  if(systrayIntegration && (useOSD == 1 || (useOSD == 2 && (isMinimized() || isHidden())))) {
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
  results_lbl->setText(tr("Results", "i.e: Search results")+" <i>("+QString(misc::toString(nb_search_results).c_str())+")</i>:");
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
}

// Clear search results list
void SearchEngine::on_clear_button_clicked(){
  searchResultsUrls.clear();
  SearchListModel->removeRows(0, SearchListModel->rowCount());
  // Disable clear & download buttons
  clear_button->setEnabled(false);
  download_button->setEnabled(false);
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
