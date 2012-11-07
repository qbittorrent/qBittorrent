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

#ifdef Q_WS_WIN
#include <stdlib.h>
#endif

#include "searchengine.h"
#include "qbtsession.h"
#include "downloadthread.h"
#include "fs_utils.h"
#include "misc.h"
#include "preferences.h"
#include "searchlistdelegate.h"
#include "qinisettings.h"
#include "mainwindow.h"
#include "iconprovider.h"
#include "lineedit.h"

#define SEARCHHISTORY_MAXSIZE 50

/*SEARCH ENGINE START*/
SearchEngine::SearchEngine(MainWindow* parent)
  : QWidget(parent)
  , search_pattern(new LineEdit)
  , mp_mainWindow(parent)
{
  setupUi(this);
  searchBarLayout->insertWidget(0, search_pattern);
  connect(search_pattern, SIGNAL(returnPressed()), search_button, SLOT(click()));
  // Icons
  search_button->setIcon(IconProvider::instance()->getIcon("edit-find"));
  download_button->setIcon(IconProvider::instance()->getIcon("download"));
  goToDescBtn->setIcon(IconProvider::instance()->getIcon("application-x-mswinurl"));
  enginesButton->setIcon(IconProvider::instance()->getIcon("preferences-system-network"));
  tabWidget->setTabsClosable(true);
  connect(tabWidget, SIGNAL(tabCloseRequested(int)), this, SLOT(closeTab(int)));
  // Boolean initialization
  search_stopped = false;
  // Creating Search Process
#ifdef Q_WS_WIN
  has_python = addPythonPathToEnv();
#endif
  searchProcess = new QProcess(this);
  searchProcess->setEnvironment(QProcess::systemEnvironment());
  connect(searchProcess, SIGNAL(started()), this, SLOT(searchStarted()));
  connect(searchProcess, SIGNAL(readyReadStandardOutput()), this, SLOT(readSearchOutput()));
  connect(searchProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(searchFinished(int,QProcess::ExitStatus)));
  connect(tabWidget, SIGNAL(currentChanged(int)), this, SLOT(tab_changed(int)));
  searchTimeout = new QTimer(this);
  searchTimeout->setSingleShot(true);
  connect(searchTimeout, SIGNAL(timeout()), this, SLOT(on_search_button_clicked()));
  // Update nova.py search plugin if necessary
  updateNova();
  supported_engines = new SupportedEngines(
      #ifdef Q_WS_WIN
        has_python
      #endif
        );
  // Fill in category combobox
  fillCatCombobox();

  connect(search_pattern, SIGNAL(textEdited(QString)), this, SLOT(searchTextEdited(QString)));
}

void SearchEngine::fillCatCombobox() {
  comboCategory->clear();
  comboCategory->addItem(full_cat_names["all"], QVariant("all"));
  QStringList supported_cat = supported_engines->supportedCategories();
  foreach (QString cat, supported_cat) {
    qDebug("Supported category: %s", qPrintable(cat));
    comboCategory->addItem(full_cat_names[cat], QVariant(cat));
  }
}

#ifdef Q_WS_WIN
bool SearchEngine::addPythonPathToEnv() {
  QString python_path = Preferences::getPythonPath();
  if (!python_path.isEmpty()) {
    // Add it to PATH envvar
    QString path_envar = QString::fromLocal8Bit(qgetenv("PATH").constData());
    if (path_envar.isNull()) {
      path_envar = "";
    }
    path_envar = python_path+";"+path_envar;
    qDebug("New PATH envvar is: %s", qPrintable(path_envar));
    qputenv("PATH", path_envar.toLocal8Bit());
    return true;
  }
  return false;
}

void SearchEngine::installPython() {
  setCursor(QCursor(Qt::WaitCursor));
  // Download python
  DownloadThread *pydownloader = new DownloadThread(this);
  connect(pydownloader, SIGNAL(downloadFinished(QString,QString)), this, SLOT(pythonDownloadSuccess(QString,QString)));
  connect(pydownloader, SIGNAL(downloadFailure(QString,QString)), this, SLOT(pythonDownloadFailure(QString,QString)));
  pydownloader->downloadUrl("http://python.org/ftp/python/2.7.3/python-2.7.3.msi");
}

void SearchEngine::pythonDownloadSuccess(QString url, QString file_path) {
  setCursor(QCursor(Qt::ArrowCursor));
  Q_UNUSED(url);
  QFile::rename(file_path, file_path+".msi");
  QProcess installer;
  qDebug("Launching Python installer in passive mode...");

  installer.start("msiexec.exe /passive /i "+file_path.replace("/", "\\")+".msi");
  // Wait for setup to complete
  installer.waitForFinished();

  qDebug("Installer stdout: %s", installer.readAllStandardOutput().data());
  qDebug("Installer stderr: %s", installer.readAllStandardError().data());
  qDebug("Setup should be complete!");
  // Reload search engine
  has_python = addPythonPathToEnv();
  if (has_python) {
    supported_engines->update();
    // Launch the search again
    on_search_button_clicked();
  }
  // Delete temp file
  QFile::remove(file_path+".msi");
}

void SearchEngine::pythonDownloadFailure(QString url, QString error) {
  Q_UNUSED(url);
  setCursor(QCursor(Qt::ArrowCursor));
  QMessageBox::warning(this, tr("Download error"), tr("Python setup could not be downloaded, reason: %1.\nPlease install it manually.").arg(error));
}

#endif

QString SearchEngine::selectedCategory() const {
  return comboCategory->itemData(comboCategory->currentIndex()).toString();
}

SearchEngine::~SearchEngine() {
  qDebug("Search destruction");
  searchProcess->kill();
  searchProcess->waitForFinished();
  foreach (QProcess *downloader, downloaders) {
    // Make sure we disconnect the SIGNAL/SLOT first
    // To avoid qreal free
    downloader->disconnect();
    downloader->kill();
    downloader->waitForFinished();
    delete downloader;
  }
  delete search_pattern;
  delete searchTimeout;
  delete searchProcess;
  delete supported_engines;
}

void SearchEngine::tab_changed(int t)
{//when we switch from a tab that is not empty to another that is empty the download button
  //doesn't have to be available
  if (t>-1)
  {//-1 = no more tab
    if (all_tab.at(tabWidget->currentIndex())->getCurrentSearchListModel()->rowCount()) {
      download_button->setEnabled(true);
      goToDescBtn->setEnabled(true);
    } else {
      download_button->setEnabled(false);
      goToDescBtn->setEnabled(false);
    }
  }
}

void SearchEngine::on_enginesButton_clicked() {
  engineSelectDlg *dlg = new engineSelectDlg(this, supported_engines);
  connect(dlg, SIGNAL(enginesChanged()), this, SLOT(fillCatCombobox()));
}

void SearchEngine::searchTextEdited(QString) {
  // Enable search button
  search_button->setText(tr("Search"));
}

void SearchEngine::giveFocusToSearchInput() {
  search_pattern->setFocus();
}

// Function called when we click on search button
void SearchEngine::on_search_button_clicked() {
#ifdef Q_WS_WIN
  if (!has_python) {
    if (QMessageBox::question(this, tr("Missing Python Interpreter"),
                             tr("Python 2.x is required to use the search engine but it does not seem to be installed.\nDo you want to install it now?"),
                             QMessageBox::Yes|QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes) {
      // Download and Install Python
      installPython();
    }
    return;
  }
#endif
  if (searchProcess->state() != QProcess::NotRunning) {
#ifdef Q_WS_WIN
    searchProcess->kill();
#else
    searchProcess->terminate();
#endif
    search_stopped = true;
    if (searchTimeout->isActive()) {
      searchTimeout->stop();
    }
    if (search_button->text() != tr("Search")) {
      search_button->setText(tr("Search"));
      return;
    }
  }
  searchProcess->waitForFinished();
  // Reload environment variables (proxy)
  searchProcess->setEnvironment(QProcess::systemEnvironment());

  const QString pattern = search_pattern->text().trimmed();
  // No search pattern entered
  if (pattern.isEmpty()) {
    QMessageBox::critical(0, tr("Empty search pattern"), tr("Please type a search pattern first"));
    return;
  }
  // Tab Addition
  currentSearchTab=new SearchTab(this);
  connect(currentSearchTab->header(), SIGNAL(sectionResized(int, int, int)), this, SLOT(propagateSectionResized(int,int,int)));
  all_tab.append(currentSearchTab);
  QString tabName = pattern;
  tabName.replace(QRegExp("&{1}"), "&&");
  tabWidget->addTab(currentSearchTab, tabName);
  tabWidget->setCurrentWidget(currentSearchTab);

  // Getting checked search engines
  QStringList params;
  search_stopped = false;
  params << fsutils::searchEngineLocation()+QDir::separator()+"nova2.py";
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

void SearchEngine::propagateSectionResized(int index, int , int newsize) {
  foreach (SearchTab * tab, all_tab) {
    tab->getCurrentTreeView()->setColumnWidth(index, newsize);
  }
  saveResultsColumnsWidth();
}

void SearchEngine::saveResultsColumnsWidth() {
  if (all_tab.size() > 0) {
    QTreeView* treeview = all_tab.first()->getCurrentTreeView();
    QIniSettings settings("qBittorrent", "qBittorrent");
    QStringList width_list;
    QStringList new_width_list;
    short nbColumns = all_tab.first()->getCurrentSearchListModel()->columnCount();

    QString line = settings.value("SearchResultsColsWidth", QString()).toString();
    if (!line.isEmpty()) {
      width_list = line.split(' ');
    }
    for (short i=0; i<nbColumns; ++i) {
      if (treeview->columnWidth(i)<1 && width_list.size() == nbColumns && width_list.at(i).toInt()>=1) {
        // load the former width
        new_width_list << width_list.at(i);
      } else if (treeview->columnWidth(i)>=1) {
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
  if (torrent_url.startsWith("bc://bt/", Qt::CaseInsensitive)) {
    qDebug("Converting bc link to magnet link");
    torrent_url = misc::bcLinkToMagnet(torrent_url);
  }
  qDebug() << Q_FUNC_INFO << torrent_url;
  if (torrent_url.startsWith("magnet:")) {
    QStringList urls;
    urls << torrent_url;
    mp_mainWindow->downloadFromURLList(urls);
  } else {
    QProcess *downloadProcess = new QProcess(this);
    downloadProcess->setEnvironment(QProcess::systemEnvironment());
    connect(downloadProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(downloadFinished(int,QProcess::ExitStatus)));
    downloaders << downloadProcess;
    QStringList params;
    params << fsutils::searchEngineLocation()+QDir::separator()+"nova2dl.py";
    params << engine_url;
    params << torrent_url;
    // Launch search
    downloadProcess->start("python", params, QIODevice::ReadOnly);
  }
}

void SearchEngine::searchStarted() {
  // Update SearchEngine widgets
  search_status->setText(tr("Searching..."));
  search_status->repaint();
  search_button->setText("Stop");
}

// search Qprocess return output as soon as it gets new
// stuff to read. We split it into lines and add each
// line to search results calling appendSearchResult().
void SearchEngine::readSearchOutput() {
  QByteArray output = searchProcess->readAllStandardOutput();
  output.replace("\r", "");
  QList<QByteArray> lines_list = output.split('\n');
  if (!search_result_line_truncated.isEmpty()) {
    QByteArray end_of_line = lines_list.takeFirst();
    lines_list.prepend(search_result_line_truncated+end_of_line);
  }
  search_result_line_truncated = lines_list.takeLast().trimmed();
  foreach (const QByteArray &line, lines_list) {
    appendSearchResult(QString::fromUtf8(line));
  }
  if (currentSearchTab)
    currentSearchTab->getCurrentLabel()->setText(tr("Results")+QString::fromUtf8(" <i>(")+QString::number(nb_search_results)+QString::fromUtf8(")</i>:"));
}

void SearchEngine::downloadFinished(int exitcode, QProcess::ExitStatus) {
  QProcess *downloadProcess = (QProcess*)sender();
  if (exitcode == 0) {
    QString line = QString::fromUtf8(downloadProcess->readAllStandardOutput()).trimmed();
    QStringList parts = line.split(' ');
    if (parts.size() == 2) {
      QString path = parts[0];
      QString url = parts[1];
      QBtSession::instance()->processDownloadedFile(url, path);
    }
  }
  qDebug("Deleting downloadProcess");
  downloaders.removeOne(downloadProcess);
  delete downloadProcess;
}

static void removePythonScriptIfExists(const QString& script_path)
{
    fsutils::forceRemove(script_path);
    fsutils::forceRemove(script_path+"c");
}

// Update nova.py search plugin if necessary
void SearchEngine::updateNova() {
  qDebug("Updating nova");
  // create nova directory if necessary
  QDir search_dir(fsutils::searchEngineLocation());
  QString nova_folder = misc::pythonVersion() >= 3 ? "nova3" : "nova";
  QFile package_file(search_dir.absoluteFilePath("__init__.py"));
  package_file.open(QIODevice::WriteOnly | QIODevice::Text);
  package_file.close();
  if (!search_dir.exists("engines")) {
    search_dir.mkdir("engines");
  }
  QFile package_file2(search_dir.absolutePath().replace("\\", "/")+"/engines/__init__.py");
  package_file2.open(QIODevice::WriteOnly | QIODevice::Text);
  package_file2.close();
  // Copy search plugin files (if necessary)
  QString filePath = search_dir.absoluteFilePath("nova2.py");
  if (getPluginVersion(":/"+nova_folder+"/nova2.py") > getPluginVersion(filePath)) {
    removePythonScriptIfExists(filePath);
    QFile::copy(":/"+nova_folder+"/nova2.py", filePath);
  }

  filePath = search_dir.absoluteFilePath("nova2dl.py");
  if (getPluginVersion(":/"+nova_folder+"/nova2dl.py") > getPluginVersion(filePath)) {
    removePythonScriptIfExists(filePath);
    QFile::copy(":/"+nova_folder+"/nova2dl.py", filePath);
  }

  filePath = search_dir.absoluteFilePath("novaprinter.py");
  if (getPluginVersion(":/"+nova_folder+"/novaprinter.py") > getPluginVersion(filePath)) {
    removePythonScriptIfExists(filePath);
    QFile::copy(":/"+nova_folder+"/novaprinter.py", filePath);
  }

  filePath = search_dir.absoluteFilePath("helpers.py");
  if (getPluginVersion(":/"+nova_folder+"/helpers.py") > getPluginVersion(filePath)) {
    removePythonScriptIfExists(filePath);
    QFile::copy(":/"+nova_folder+"/helpers.py", filePath);
  }

  filePath = search_dir.absoluteFilePath("socks.py");
  removePythonScriptIfExists(filePath);
  QFile::copy(":/"+nova_folder+"/socks.py", filePath);

  if (nova_folder == "nova") {
    filePath = search_dir.absoluteFilePath("fix_encoding.py");
    removePythonScriptIfExists(filePath);
    QFile::copy(":/"+nova_folder+"/fix_encoding.py", filePath);
  }

  if (nova_folder == "nova3") {
    filePath = search_dir.absoluteFilePath("sgmllib3.py");
    removePythonScriptIfExists(filePath);
    QFile::copy(":/"+nova_folder+"/sgmllib3.py", filePath);
  }
  QDir destDir(QDir(fsutils::searchEngineLocation()).absoluteFilePath("engines"));
  QDir shipped_subDir(":/"+nova_folder+"/engines/");
  QStringList files = shipped_subDir.entryList();
  foreach (const QString &file, files) {
    QString shipped_file = shipped_subDir.absoluteFilePath(file);
    // Copy python classes
    if (file.endsWith(".py")) {
      const QString dest_file = destDir.absoluteFilePath(file);
      if (getPluginVersion(shipped_file) > getPluginVersion(dest_file) ) {
        qDebug("shipped %s is more recent then local plugin, updating...", qPrintable(file));
        removePythonScriptIfExists(dest_file);
        qDebug("%s copied to %s", qPrintable(shipped_file), qPrintable(dest_file));
        QFile::copy(shipped_file, dest_file);
      }
    } else {
      // Copy icons
      if (file.endsWith(".png")) {
        if (!QFile::exists(destDir.absoluteFilePath(file))) {
          QFile::copy(shipped_file, destDir.absoluteFilePath(file));
        }
      }
    }
  }
}

// Slot called when search is Finished
// Search can be finished for 3 reasons :
// Error | Stopped by user | Finished normally
void SearchEngine::searchFinished(int exitcode,QProcess::ExitStatus) {
  if (searchTimeout->isActive()) {
    searchTimeout->stop();
  }
  QIniSettings settings("qBittorrent", "qBittorrent");
  bool useNotificationBalloons = settings.value("Preferences/General/NotificationBaloons", true).toBool();
  if (useNotificationBalloons && mp_mainWindow->getCurrentTabWidget() != this) {
    mp_mainWindow->showNotificationBaloon(tr("Search Engine"), tr("Search has finished"));
  }
  if (exitcode) {
#ifdef Q_WS_WIN
    search_status->setText(tr("Search aborted"));
#else
    search_status->setText(tr("An error occurred during search..."));
#endif
  }else{
    if (search_stopped) {
      search_status->setText(tr("Search aborted"));
    }else{
      if (no_search_results) {
        search_status->setText(tr("Search returned no results"));
      }else{
        search_status->setText(tr("Search has finished"));
      }
    }
  }
  if (currentSearchTab)
    currentSearchTab->getCurrentLabel()->setText(tr("Results", "i.e: Search results")+QString::fromUtf8(" <i>(")+QString::number(nb_search_results)+QString::fromUtf8(")</i>:"));
  search_button->setText("Search");
}

// SLOT to append one line to search results list
// Line is in the following form :
// file url | file name | file size | nb seeds | nb leechers | Search engine url
void SearchEngine::appendSearchResult(const QString &line) {
  if (!currentSearchTab) {
    if (searchProcess->state() != QProcess::NotRunning) {
      searchProcess->terminate();
    }
    if (searchTimeout->isActive()) {
      searchTimeout->stop();
    }
    search_stopped = true;
    return;
  }
  const QStringList parts = line.split("|");
  const int nb_fields = parts.size();
  if (nb_fields < NB_PLUGIN_COLUMNS-1) { //-1 because desc_link is optional
    return;
  }
  Q_ASSERT(currentSearchTab);
  // Add item to search result list
  QStandardItemModel* cur_model = currentSearchTab->getCurrentSearchListModel();
  Q_ASSERT(cur_model);
  int row = cur_model->rowCount();
  cur_model->insertRow(row);

  cur_model->setData(cur_model->index(row, DL_LINK), parts.at(PL_DL_LINK).trimmed()); // download URL
  cur_model->setData(cur_model->index(row, NAME), parts.at(PL_NAME).trimmed()); // Name
  cur_model->setData(cur_model->index(row, SIZE), parts.at(PL_SIZE).trimmed().toLongLong()); // Size
  bool ok = false;
  qlonglong nb_seeders = parts.at(PL_SEEDS).trimmed().toLongLong(&ok);
  if (!ok || nb_seeders < 0) {
    cur_model->setData(cur_model->index(row, SEEDS), tr("Unknown")); // Seeders
  } else {
    cur_model->setData(cur_model->index(row, SEEDS), nb_seeders); // Seeders
  }
  qlonglong nb_leechers = parts.at(PL_LEECHS).trimmed().toLongLong(&ok);
  if (!ok || nb_leechers < 0) {
    cur_model->setData(cur_model->index(row, LEECHS), tr("Unknown")); // Leechers
  } else {
    cur_model->setData(cur_model->index(row, LEECHS), nb_leechers); // Leechers
  }
  cur_model->setData(cur_model->index(row, ENGINE_URL), parts.at(PL_ENGINE_URL).trimmed()); // Engine URL
  // Description Link
  if (nb_fields == NB_PLUGIN_COLUMNS)
    cur_model->setData(cur_model->index(row, DESC_LINK), parts.at(PL_DESC_LINK).trimmed());

  no_search_results = false;
  ++nb_search_results;
  // Enable clear & download buttons
  download_button->setEnabled(true);
  goToDescBtn->setEnabled(true);
}

void SearchEngine::closeTab(int index) {
  if (index == tabWidget->indexOf(currentSearchTab)) {
    qDebug("Deleted current search Tab");
    if (searchProcess->state() != QProcess::NotRunning) {
      searchProcess->terminate();
    }
    if (searchTimeout->isActive()) {
      searchTimeout->stop();
    }
    search_stopped = true;
    currentSearchTab = 0;
  }
  delete all_tab.takeAt(index);
  if (!all_tab.size()) {
    download_button->setEnabled(false);
    goToDescBtn->setEnabled(false);
  }
}

// Download selected items in search results list
void SearchEngine::on_download_button_clicked() {
  //QModelIndexList selectedIndexes = currentSearchTab->getCurrentTreeView()->selectionModel()->selectedIndexes();
  QModelIndexList selectedIndexes = all_tab.at(tabWidget->currentIndex())->getCurrentTreeView()->selectionModel()->selectedIndexes();
  foreach (const QModelIndex &index, selectedIndexes) {
    if (index.column() == NAME) {
      // Get Item url
      QSortFilterProxyModel* model = all_tab.at(tabWidget->currentIndex())->getCurrentSearchListProxy();
      QString torrent_url = model->data(model->index(index.row(), URL_COLUMN)).toString();
      QString engine_url = model->data(model->index(index.row(), ENGINE_URL_COLUMN)).toString();
      downloadTorrent(engine_url, torrent_url);
      all_tab.at(tabWidget->currentIndex())->setRowColor(index.row(), "red");
    }
  }
}

void SearchEngine::on_goToDescBtn_clicked()
{
  QModelIndexList selectedIndexes = all_tab.at(tabWidget->currentIndex())->getCurrentTreeView()->selectionModel()->selectedIndexes();
  foreach (const QModelIndex &index, selectedIndexes) {
    if (index.column() == NAME) {
      QSortFilterProxyModel* model = all_tab.at(tabWidget->currentIndex())->getCurrentSearchListProxy();
      const QString desc_url = model->data(model->index(index.row(), DESC_LINK)).toString();
      if (!desc_url.isEmpty())
        QDesktopServices::openUrl(QUrl::fromEncoded(desc_url.toUtf8()));
    }
  }
}
