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
#include <QDir>

#include "searchEngine.h"
#include "bittorrent.h"
#include "downloadThread.h"
#include "misc.h"
#include "SearchListDelegate.h"

#define SEARCHHISTORY_MAXSIZE 50

/*SEARCH ENGINE START*/
SearchEngine::SearchEngine(bittorrent *BTSession, QSystemTrayIcon *myTrayIcon, bool systrayIntegration) : QWidget(), BTSession(BTSession), myTrayIcon(myTrayIcon), systrayIntegration(systrayIntegration){
  setupUi(this);
  // new qCompleter to the search pattern
  startSearchHistory();
  searchCompleter = new QCompleter(searchHistory, this);
  searchCompleter->setCaseSensitivity(Qt::CaseInsensitive);
  search_pattern->setCompleter(searchCompleter);
  // Add close tab button
  closeTab_button = new QPushButton();
  closeTab_button->setIcon(QIcon(QString::fromUtf8(":/Icons/gnome-shutdown.png")));
  closeTab_button->setFlat(true);
  connect(closeTab_button, SIGNAL(clicked()), this, SLOT(closeTab_button_clicked()));
  tabWidget->setCornerWidget(closeTab_button);
  // Boolean initialization
  search_stopped = false;
  // Creating Search Process
  searchProcess = new QProcess(this);
  QStringList env = QProcess::systemEnvironment();
  searchProcess->setEnvironment(env);
  connect(searchProcess, SIGNAL(started()), this, SLOT(searchStarted()));
  connect(searchProcess, SIGNAL(readyReadStandardOutput()), this, SLOT(readSearchOutput()));
  connect(searchProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(searchFinished(int,QProcess::ExitStatus)));
  connect(tabWidget, SIGNAL(currentChanged(int)), this, SLOT(tab_changed(int)));
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
  searchProcess->kill();
  searchProcess->waitForFinished();
	foreach(QProcess *downloader, downloaders) {
		downloader->kill();
		downloader->waitForFinished();
		delete downloader;
	}
  delete searchTimeout;
  delete searchProcess;
  delete searchCompleter;
}

void SearchEngine::tab_changed(int t)
{//when we switch from a tab that is not empty to another that is empty the download button 
	//doesn't have to be available
	if(t>-1)
	{//-1 = no more tab
    if(all_tab.at(tabWidget->currentIndex())->getCurrentSearchListModel()->rowCount()) {
        download_button->setEnabled(true);
    } else {
        download_button->setEnabled(false);
    }
	}
}

void SearchEngine::on_enginesButton_clicked() {
  engineSelectDlg *dlg = new engineSelectDlg(this);
  connect(dlg, SIGNAL(enginesChanged()), this, SLOT(loadEngineSettings()));
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
  unsigned int i = 0;
  foreach(const QString &engine, known_engines) {
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
  // Tab Addition
  currentSearchTab=new SearchTab(this);
  connect(currentSearchTab->header(), SIGNAL(sectionResized(int, int, int)), this, SLOT(propagateSectionResized(int,int,int)));
  all_tab.append(currentSearchTab);
  tabWidget->addTab(currentSearchTab, pattern);
  tabWidget->setCurrentWidget(currentSearchTab);
  closeTab_button->setEnabled(true);
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
  params << misc::qBittorrentPath()+"search_engine"+QDir::separator()+"nova2.py";
  params << enabled_engines.join(",");
  params << pattern.split(" ");
  // Update SearchEngine widgets
  no_search_results = true;
  nb_search_results = 0;
  search_result_line_truncated.clear();
  //on change le texte du label courrant
  currentSearchTab->getCurrentLabel()->setText(tr("Results")+" <i>(0)</i>:");
  // Launch search
  searchProcess->start("python", params, QIODevice::ReadOnly);
  searchTimeout->start(180000); // 3min
}

void SearchEngine::propagateSectionResized(int index, int , int newsize) {
  foreach(SearchTab * tab, all_tab) {
    tab->getCurrentTreeView()->setColumnWidth(index, newsize);
  }
  saveResultsColumnsWidth();
}

void SearchEngine::saveResultsColumnsWidth() {
  if(all_tab.size() > 0) {
    QTreeView* treeview = all_tab.first()->getCurrentTreeView();
    QSettings settings("qBittorrent", "qBittorrent");
    QStringList width_list;
    QStringList new_width_list;
    short nbColumns = all_tab.first()->getCurrentSearchListModel()->columnCount();

    QString line = settings.value("SearchResultsColsWidth", QString()).toString();
    if(!line.isEmpty()) {
      width_list = line.split(' ');
    }
    for(short i=0; i<nbColumns; ++i){
      if(treeview->columnWidth(i)<1 && width_list.size() == nbColumns && width_list.at(i).toInt()>=1) {
        // load the former width
        new_width_list << width_list.at(i);
      } else if(treeview->columnWidth(i)>=1) {
        // usual case, save the current width
        new_width_list << QString::fromUtf8(misc::toString(treeview->columnWidth(i)).c_str());
      } else {
        // default width
        treeview->resizeColumnToContents(i);
        new_width_list << QString::fromUtf8(misc::toString(treeview->columnWidth(i)).c_str());
      }
    }
    settings.setValue("SearchResultsColsWidth", new_width_list.join(" "));
  }
}

void SearchEngine::downloadTorrent(QString engine_url, QString torrent_url) {
		QProcess *downloadProcess = new QProcess(this);
		connect(downloadProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(downloadFinished(int,QProcess::ExitStatus)));
		downloaders << downloadProcess;
		QStringList params;
		params << misc::qBittorrentPath()+"search_engine"+QDir::separator()+"nova2dl.py";
		params << engine_url;
		params << torrent_url;
		// Launch search
		downloadProcess->start("python", params, QIODevice::ReadOnly);
}

void SearchEngine::searchStarted(){
  // Update SearchEngine widgets
  search_status->setText(tr("Searching..."));
  search_status->repaint();
  stop_search_button->setEnabled(true);
  stop_search_button->repaint();
  // clear results window ... not needed since we got Tabbed
  //SearchListModel->removeRows(0, SearchListModel->rowCount());
  // Clear previous results urls too
  //searchResultsUrls.clear();
}

// Download the given item from search results list
void SearchEngine::downloadSelectedItem(const QModelIndex& index){
  int row = index.row();
  // Get Item url
  QStandardItemModel *model = all_tab.at(tabWidget->currentIndex())->getCurrentSearchListModel();
	QString engine_url = model->data(model->index(index.row(), ENGINE_URL_COLUMN)).toString();
  QString torrent_url = model->data(model->index(index.row(), URL_COLUMN)).toString();
  // Download from url
  downloadTorrent(engine_url, torrent_url);
  // Set item color to RED
  all_tab.at(tabWidget->currentIndex())->setRowColor(row, "red");
}

// search Qprocess return output as soon as it gets new
// stuff to read. We split it into lines and add each
// line to search results calling appendSearchResult().
void SearchEngine::readSearchOutput(){
  QByteArray output = searchProcess->readAllStandardOutput();
  output.replace("\r", "");
  QList<QByteArray> lines_list = output.split('\n');
  if(!search_result_line_truncated.isEmpty()){
    QByteArray end_of_line = lines_list.takeFirst();
    lines_list.prepend(search_result_line_truncated+end_of_line);
  }
  search_result_line_truncated = lines_list.takeLast().trimmed();
  foreach(const QByteArray &line, lines_list){
    appendSearchResult(QString::fromUtf8(line));
  }
  currentSearchTab->getCurrentLabel()->setText(tr("Results")+QString::fromUtf8(" <i>(")+misc::toQString(nb_search_results)+QString::fromUtf8(")</i>:"));
}

void SearchEngine::downloadFinished(int exitcode, QProcess::ExitStatus) {
		QProcess *downloadProcess = (QProcess*)sender();
		if(exitcode == 0) {
				QString line = QString::fromUtf8(downloadProcess->readAllStandardOutput()).trimmed();
				QStringList parts = line.split(' ');
				if(parts.size() == 2) {
						QString path = parts[0];
						QString url = parts[1];
						BTSession->processDownloadedFile(url, path);
				}
		}
		qDebug("Deleting downloadProcess");
		downloaders.removeOne(downloadProcess);
		delete downloadProcess;
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

	filePath = misc::qBittorrentPath()+"search_engine"+QDir::separator()+"nova2dl.py";
	if(misc::getPluginVersion(":/search_engine/nova2dl.py") > misc::getPluginVersion(filePath)) {
    if(QFile::exists(filePath)){
      QFile::remove(filePath);
    }
    QFile::copy(":/search_engine/nova2dl.py", filePath);
  }
  QFile(misc::qBittorrentPath()+"search_engine"+QDir::separator()+"nova2dl.py").setPermissions(perm);
  filePath = misc::qBittorrentPath()+"search_engine"+QDir::separator()+"novaprinter.py";
  if(misc::getPluginVersion(":/search_engine/novaprinter.py") > misc::getPluginVersion(filePath)) {
    if(QFile::exists(filePath)){
      QFile::remove(filePath);
    }
    QFile::copy(":/search_engine/novaprinter.py", filePath);
  }
  QFile(misc::qBittorrentPath()+"search_engine"+QDir::separator()+"novaprinter.py").setPermissions(perm);
  filePath = misc::qBittorrentPath()+"search_engine"+QDir::separator()+"helpers.py";
  if(misc::getPluginVersion(":/search_engine/helpers.py") > misc::getPluginVersion(filePath)) {
    if(QFile::exists(filePath)){
      QFile::remove(filePath);
    }
    QFile::copy(":/search_engine/helpers.py", filePath);
  }
	QFile(misc::qBittorrentPath()+"search_engine"+QDir::separator()+"helpers.py").setPermissions(perm);
  QString destDir = misc::qBittorrentPath()+"search_engine"+QDir::separator()+"engines"+QDir::separator();
  QDir shipped_subDir(":/search_engine/engines/");
  QStringList files = shipped_subDir.entryList();
  foreach(const QString &file, files){
    QString shipped_file = shipped_subDir.path()+"/"+file;
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
  if(currentSearchTab)
    currentSearchTab->getCurrentLabel()->setText(tr("Results", "i.e: Search results")+QString::fromUtf8(" <i>(")+misc::toQString(nb_search_results)+QString::fromUtf8(")</i>:"));
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
  QString url = parts.takeFirst().trimmed();
  QString filename = parts.first().trimmed();
  parts << url;
  // Add item to search result list
  int row = currentSearchTab->getCurrentSearchListModel()->rowCount();
  currentSearchTab->getCurrentSearchListModel()->insertRow(row);
  for(int i=0; i<6; ++i){
    if(parts.at(i).trimmed().toFloat() == -1 && i != SIZE)
      currentSearchTab->getCurrentSearchListModel()->setData(currentSearchTab->getCurrentSearchListModel()->index(row, i), tr("Unknown"));
    else
      currentSearchTab->getCurrentSearchListModel()->setData(currentSearchTab->getCurrentSearchListModel()->index(row, i), QVariant(parts.at(i).trimmed()));
  }
  // Add url to searchResultsUrls associative array
  no_search_results = false;
  ++nb_search_results;
  // Enable clear & download buttons
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
void SearchEngine::closeTab_button_clicked(){
  if(all_tab.size()) {
    qDebug("currentTab rank: %d", tabWidget->currentIndex());
    qDebug("currentSearchTab rank: %d", tabWidget->indexOf(currentSearchTab));
    if(tabWidget->currentIndex() == tabWidget->indexOf(currentSearchTab)) {
        qDebug("Deleted current search Tab");
        if(searchProcess->state() != QProcess::NotRunning){
            searchProcess->terminate();
        }
        if(searchTimeout->isActive()) {
            searchTimeout->stop();
        }
        search_stopped = true;
        currentSearchTab = 0;
    }
    delete all_tab.takeAt(tabWidget->currentIndex());
    if(!all_tab.size()) {
        closeTab_button->setEnabled(false);
        download_button->setEnabled(false);
    }
  }
}

void SearchEngine::on_clearPatternButton_clicked() {
  search_pattern->clear();
  search_pattern->setFocus();
}

// Download selected items in search results list
void SearchEngine::on_download_button_clicked(){
  //QModelIndexList selectedIndexes = currentSearchTab->getCurrentTreeView()->selectionModel()->selectedIndexes();
  QModelIndexList selectedIndexes = all_tab.at(tabWidget->currentIndex())->getCurrentTreeView()->selectionModel()->selectedIndexes();
  foreach(const QModelIndex &index, selectedIndexes){
    if(index.column() == NAME){
      // Get Item url
      QStandardItemModel *model = all_tab.at(tabWidget->currentIndex())->getCurrentSearchListModel();
      QString torrent_url = model->data(model->index(index.row(), URL_COLUMN)).toString();
			QString engine_url = model->data(model->index(index.row(), ENGINE_URL_COLUMN)).toString();
      downloadTorrent(engine_url, torrent_url);
      all_tab.at(tabWidget->currentIndex())->setRowColor(index.row(), "red");
    }
  }
}
