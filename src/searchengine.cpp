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
#include <QTemporaryFile>
#include <QSystemTrayIcon>
#include <iostream>
#include <QTimer>
#include <QDir>
#include <QMenu>
#include <QClipboard>
#include <QMimeData>
#include <QSortFilterProxyModel>

#include "searchengine.h"
#include "bittorrent.h"
#include "downloadthread.h"
#include "misc.h"
#include "searchlistdelegate.h"
#include "GUI.h"

#define SEARCHHISTORY_MAXSIZE 50

/*SEARCH ENGINE START*/
SearchEngine::SearchEngine(GUI *parent, Bittorrent *BTSession) : QWidget(parent), BTSession(BTSession), parent(parent) {
  setupUi(this);
  // new qCompleter to the search pattern
  startSearchHistory();
  createCompleter();
#if QT_VERSION >= 0x040500
  tabWidget->setTabsClosable(true);
  connect(tabWidget, SIGNAL(tabCloseRequested(int)), this, SLOT(closeTab(int)));
#else
  // Add close tab button
  closeTab_button = new QPushButton();
  closeTab_button->setIcon(QIcon(QString::fromUtf8(":/Icons/oxygen/tab-close.png")));
  closeTab_button->setFlat(true);
  tabWidget->setCornerWidget(closeTab_button);
  connect(closeTab_button, SIGNAL(clicked()), this, SLOT(closeTab_button_clicked()));
#endif
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
  connect(searchTimeout, SIGNAL(timeout()), this, SLOT(on_search_button_clicked()));
  // Update nova.py search plugin if necessary
  updateNova();
  supported_engines = new SupportedEngines();
  // Fill in category combobox
  fillCatCombobox();
  connect(search_pattern, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(displayPatternContextMenu(QPoint)));
  connect(search_pattern, SIGNAL(textEdited(QString)), this, SLOT(searchTextEdited(QString)));
}

void SearchEngine::fillCatCombobox() {
  comboCategory->clear();
  comboCategory->addItem(full_cat_names["all"], QVariant("all"));
  QStringList supported_cat = supported_engines->supportedCategories();
  foreach(QString cat, supported_cat) {
    qDebug("Supported category: %s", qPrintable(cat));
    comboCategory->addItem(full_cat_names[cat], QVariant(cat));
  }
}

QString SearchEngine::selectedCategory() const {
  return comboCategory->itemData(comboCategory->currentIndex()).toString();
}

SearchEngine::~SearchEngine(){
  qDebug("Search destruction");
  // save the searchHistory for later uses
  saveSearchHistory();
  searchProcess->kill();
  searchProcess->waitForFinished();
  foreach(QProcess *downloader, downloaders) {
    // Make sure we disconnect the SIGNAL/SLOT first
    // To avoid double free
    downloader->disconnect();
    downloader->kill();
    downloader->waitForFinished();
    delete downloader;
  }
#if QT_VERSION < 0x040500
  delete closeTab_button;
#endif
  delete searchTimeout;
  delete searchProcess;
  delete supported_engines;
  if(searchCompleter)
    delete searchCompleter;
}

void SearchEngine::displayPatternContextMenu(QPoint) {
  QMenu myMenu(this);
  QAction cutAct(QIcon(":/Icons/oxygen/edit-cut.png"), tr("Cut"), &myMenu);
  QAction copyAct(QIcon(":/Icons/oxygen/edit-copy.png"), tr("Copy"), &myMenu);
  QAction pasteAct(QIcon(":/Icons/oxygen/edit-paste.png"), tr("Paste"), &myMenu);
  QAction clearAct(QIcon(":/Icons/oxygen/edit_clear.png"), tr("Clear field"), &myMenu);
  QAction clearHistoryAct(QIcon(":/Icons/oxygen/edit-clear.png"), tr("Clear completion history"), &myMenu);
  bool hasCopyAct = false;
  if(search_pattern->hasSelectedText()) {
    myMenu.addAction(&cutAct);
    myMenu.addAction(&copyAct);
    hasCopyAct = true;
  }
  if(qApp->clipboard()->mimeData()->hasText()) {
    myMenu.addAction(&pasteAct);
    hasCopyAct = true;
  }
  if(hasCopyAct)
    myMenu.addSeparator();
  myMenu.addAction(&clearHistoryAct);
  myMenu.addAction(&clearAct);
  QAction *act = myMenu.exec(QCursor::pos());
  if(act != 0) {
    if(act == &clearHistoryAct) {
      searchHistory.setStringList(QStringList());
    }
    else if (act == &pasteAct) {
      search_pattern->paste();
    }
    else if (act == &cutAct) {
      search_pattern->cut();
    }
    else if (act == &copyAct) {
      search_pattern->copy();
    }
    else if (act == &clearAct) {
      search_pattern->clear();
    }
  }
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
  engineSelectDlg *dlg = new engineSelectDlg(this, supported_engines);
  connect(dlg, SIGNAL(enginesChanged()), this, SLOT(fillCatCombobox()));
}

// get the last searchs from a QSettings to a QStringList
void SearchEngine::startSearchHistory(){
  QSettings settings("qBittorrent", "qBittorrent");
  searchHistory.setStringList(settings.value("Search/searchHistory",QStringList()).toStringList());
}

// Save the history list into the QSettings for the next session
void SearchEngine::saveSearchHistory() {
  QSettings settings("qBittorrent", "qBittorrent");
  settings.setValue("Search/searchHistory",searchHistory.stringList());
}

void SearchEngine::searchTextEdited(QString) {
  // Enable search button
  search_button->setText(tr("Search"));
}

// Function called when we click on search button
void SearchEngine::on_search_button_clicked(){
  if(searchProcess->state() != QProcess::NotRunning){
    searchProcess->terminate();
    search_stopped = true;
    if(searchTimeout->isActive()) {
      searchTimeout->stop();
    }
    if(search_button->text() != tr("Search")) {
      search_button->setText(tr("Search"));
      return;
    }
  }
  searchProcess->waitForFinished();
  // Reload environment variables (proxy)
  searchProcess->setEnvironment(QProcess::systemEnvironment());

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
#if QT_VERSION < 0x040500
  closeTab_button->setEnabled(true);
#endif
  // if the pattern is not in the pattern
  QStringList wordList = searchHistory.stringList();
  if(wordList.indexOf(pattern) == -1){
    //update the searchHistory list
    wordList.append(pattern);
    // verify the max size of the history
    if(wordList.size() > SEARCHHISTORY_MAXSIZE)
      wordList = wordList.mid(wordList.size()/2);
    searchHistory.setStringList(wordList);
  }

  // Getting checked search engines
  QStringList params;
  QStringList engineNames;
  search_stopped = false;
  params << misc::searchEngineLocation()+QDir::separator()+"nova2.py";
  params << supported_engines->enginesEnabled().join(",");
  qDebug("Search with category: %s", qPrintable(selectedCategory()));
  params << selectedCategory();
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

void SearchEngine::createCompleter() {
  if(searchCompleter)
    delete searchCompleter;
  searchCompleter = new QCompleter(&searchHistory);
  searchCompleter->setCaseSensitivity(Qt::CaseInsensitive);
  search_pattern->setCompleter(searchCompleter);
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
        new_width_list << QString::number(treeview->columnWidth(i));
      } else {
        // default width
        treeview->resizeColumnToContents(i);
        new_width_list << QString::number(treeview->columnWidth(i));
      }
    }
    settings.setValue("SearchResultsColsWidth", new_width_list.join(" "));
  }
}

void SearchEngine::downloadTorrent(QString engine_url, QString torrent_url) {
  if(torrent_url.startsWith("magnet:")) {
    QStringList urls;
    urls << torrent_url;
    parent->downloadFromURLList(urls);
  } else {
    QProcess *downloadProcess = new QProcess(this);
    connect(downloadProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(downloadFinished(int,QProcess::ExitStatus)));
    downloaders << downloadProcess;
    QStringList params;
    params << misc::searchEngineLocation()+QDir::separator()+"nova2dl.py";
    params << engine_url;
    params << torrent_url;
    // Launch search
    downloadProcess->start("python", params, QIODevice::ReadOnly);
  }
}

void SearchEngine::searchStarted(){
  // Update SearchEngine widgets
  search_status->setText(tr("Searching..."));
  search_status->repaint();
  search_button->setText("Stop");
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
  if(currentSearchTab)
    currentSearchTab->getCurrentLabel()->setText(tr("Results")+QString::fromUtf8(" <i>(")+QString::number(nb_search_results)+QString::fromUtf8(")</i>:"));
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
  QDir search_dir(misc::searchEngineLocation());
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
  QString filePath = misc::searchEngineLocation()+QDir::separator()+"nova2.py";
  if(getPluginVersion(":/search_engine/nova2.py") > getPluginVersion(filePath)) {
    if(QFile::exists(filePath))
      QFile::remove(filePath);
    QFile::copy(":/search_engine/nova2.py", filePath);
  }
  // Set permissions
  QFile::Permissions perm=QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadUser | QFile::WriteUser | QFile::ExeUser | QFile::ReadGroup | QFile::ReadGroup;
  QFile(misc::searchEngineLocation()+QDir::separator()+"nova2.py").setPermissions(perm);

  filePath = misc::searchEngineLocation()+QDir::separator()+"nova2dl.py";
  if(getPluginVersion(":/search_engine/nova2dl.py") > getPluginVersion(filePath)) {
    if(QFile::exists(filePath)){
      QFile::remove(filePath);
    }
    QFile::copy(":/search_engine/nova2dl.py", filePath);
  }
  QFile(misc::searchEngineLocation()+QDir::separator()+"nova2dl.py").setPermissions(perm);
  filePath = misc::searchEngineLocation()+QDir::separator()+"novaprinter.py";
  if(getPluginVersion(":/search_engine/novaprinter.py") > getPluginVersion(filePath)) {
    if(QFile::exists(filePath)){
      QFile::remove(filePath);
    }
    QFile::copy(":/search_engine/novaprinter.py", filePath);
  }
  QFile(misc::searchEngineLocation()+QDir::separator()+"novaprinter.py").setPermissions(perm);
  filePath = misc::searchEngineLocation()+QDir::separator()+"helpers.py";
  if(getPluginVersion(":/search_engine/helpers.py") > getPluginVersion(filePath)) {
    if(QFile::exists(filePath)){
      QFile::remove(filePath);
    }
    QFile::copy(":/search_engine/helpers.py", filePath);
  }
  QFile(misc::searchEngineLocation()+QDir::separator()+"socks.py").setPermissions(perm);
  filePath = misc::searchEngineLocation()+QDir::separator()+"socks.py";
  if(!QFile::exists(filePath)) {
    QFile::copy(":/search_engine/socks.py", filePath);
  }
  QString destDir = misc::searchEngineLocation()+QDir::separator()+"engines"+QDir::separator();
  QDir shipped_subDir(":/search_engine/engines/");
  QStringList files = shipped_subDir.entryList();
  foreach(const QString &file, files){
    QString shipped_file = shipped_subDir.path()+"/"+file;
    // Copy python classes
    if(file.endsWith(".py")) {
      const QString &dest_file = destDir+file;
      if(getPluginVersion(shipped_file) > getPluginVersion(dest_file) ) {
        qDebug("shippped %s is more recent then local plugin, updating", qPrintable(file));
        if(QFile::exists(dest_file)) {
          qDebug("Removing old %s", qPrintable(dest_file));
          QFile::remove(dest_file);
        }
        qDebug("%s copied to %s", qPrintable(shipped_file), qPrintable(dest_file));
        QFile::copy(shipped_file, dest_file);
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
  if(searchTimeout->isActive()) {
    searchTimeout->stop();
  }
  QSettings settings("qBittorrent", "qBittorrent");
  bool useNotificationBalloons = settings.value("Preferences/General/NotificationBaloons", true).toBool();
  if(useNotificationBalloons && parent->getCurrentTabIndex() != TAB_SEARCH) {
    parent->showNotificationBaloon(tr("Search Engine"), tr("Search has finished"));
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
    currentSearchTab->getCurrentLabel()->setText(tr("Results", "i.e: Search results")+QString::fromUtf8(" <i>(")+QString::number(nb_search_results)+QString::fromUtf8(")</i>:"));
  search_button->setText("Search");
}

// SLOT to append one line to search results list
// Line is in the following form :
// file url | file name | file size | nb seeds | nb leechers | Search engine url
void SearchEngine::appendSearchResult(QString line){
  if(!currentSearchTab) {
    if(searchProcess->state() != QProcess::NotRunning){
      searchProcess->terminate();
    }
    if(searchTimeout->isActive()) {
      searchTimeout->stop();
    }
    search_stopped = true;
    return;
  }
  QStringList parts = line.split("|");
  if(parts.size() != 6){
    return;
  }
  Q_ASSERT(currentSearchTab);
  // Add item to search result list
  QStandardItemModel* cur_model = currentSearchTab->getCurrentSearchListModel();
  Q_ASSERT(cur_model);
  int row = cur_model->rowCount();
  cur_model->insertRow(row);

  cur_model->setData(cur_model->index(row, 5), parts.at(0).trimmed()); // download URL
  cur_model->setData(cur_model->index(row, 0), parts.at(1).trimmed()); // Name
  cur_model->setData(cur_model->index(row, 1), parts.at(2).trimmed().toLongLong()); // Size
  bool ok = false;
  qlonglong nb_seeders = parts.at(3).trimmed().toLongLong(&ok);
  if(!ok || nb_seeders < 0) {
    cur_model->setData(cur_model->index(row, 2), tr("Unknown")); // Seeders
  } else {
    cur_model->setData(cur_model->index(row, 2), nb_seeders); // Seeders
  }
  qlonglong nb_leechers = parts.at(4).trimmed().toLongLong(&ok);
  if(!ok || nb_leechers < 0) {
    cur_model->setData(cur_model->index(row, 3), tr("Unknown")); // Leechers
  } else {
    cur_model->setData(cur_model->index(row, 3), nb_leechers); // Leechers
  }
  cur_model->setData(cur_model->index(row, 4), parts.at(5).trimmed()); // Engine URL

  no_search_results = false;
  ++nb_search_results;
  // Enable clear & download buttons
  download_button->setEnabled(true);
}

#if QT_VERSION >= 0x040500
void SearchEngine::closeTab(int index) {
  if(index == tabWidget->indexOf(currentSearchTab)) {
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
  delete all_tab.takeAt(index);
  if(!all_tab.size()) {
    download_button->setEnabled(false);
  }
}
#else
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
#endif

// Download selected items in search results list
void SearchEngine::on_download_button_clicked(){
  //QModelIndexList selectedIndexes = currentSearchTab->getCurrentTreeView()->selectionModel()->selectedIndexes();
  QModelIndexList selectedIndexes = all_tab.at(tabWidget->currentIndex())->getCurrentTreeView()->selectionModel()->selectedIndexes();
  foreach(const QModelIndex &index, selectedIndexes){
    if(index.column() == NAME){
      // Get Item url
      QSortFilterProxyModel* model = all_tab.at(tabWidget->currentIndex())->getCurrentSearchListProxy();
      QString torrent_url = model->data(model->index(index.row(), URL_COLUMN)).toString();
      QString engine_url = model->data(model->index(index.row(), ENGINE_URL_COLUMN)).toString();
      downloadTorrent(engine_url, torrent_url);
      all_tab.at(tabWidget->currentIndex())->setRowColor(index.row(), "red");
    }
  }
}
