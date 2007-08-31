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

#include "engineSelectDlg.h"
#include "downloadThread.h"
#include "misc.h"
#include <QProcess>
#include <QHeaderView>
#include <QSettings>
#include <QMenu>
#include <QMessageBox>
#include <QFileDialog>

#ifdef HAVE_MAGICK
  #include <Magick++.h>
  using namespace Magick;
#endif

#define ENGINE_NAME 0
#define ENGINE_URL 1
#define ENGINE_STATE 2

engineSelectDlg::engineSelectDlg(QWidget *parent) : QDialog(parent) {
  setupUi(this);
  setAttribute(Qt::WA_DeleteOnClose);
  pluginsTree->header()->resizeSection(0, 170);
  pluginsTree->header()->resizeSection(1, 220);
  actionEnable->setIcon(QIcon(QString::fromUtf8(":/Icons/button_ok.png")));
  actionDisable->setIcon(QIcon(QString::fromUtf8(":/Icons/button_cancel.png")));
  actionUninstall->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/remove.png")));
  connect(actionEnable, SIGNAL(triggered()), this, SLOT(enableSelection()));
  connect(actionDisable, SIGNAL(triggered()), this, SLOT(disableSelection()));
  connect(pluginsTree, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayContextMenu(const QPoint&)));
  downloader = new downloadThread(this);
  connect(downloader, SIGNAL(downloadFinished(QString, QString)), this, SLOT(processDownloadedFile(QString, QString)));
  connect(downloader, SIGNAL(downloadFailure(QString, QString)), this, SLOT(handleDownloadFailure(QString, QString)));
  loadSettings();
  loadSupportedSearchEngines();
  connect(pluginsTree, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(toggleEngineState(QTreeWidgetItem*, int)));
  show();
}

engineSelectDlg::~engineSelectDlg() {
  qDebug("Destroying engineSelectDlg");
  saveSettings();
  emit enginesChanged();
  qDebug("Before deleting downloader");
  delete downloader;
  qDebug("Engine plugins dialog destroyed");
}

void engineSelectDlg::loadSettings() {
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  known_engines = settings.value(QString::fromUtf8("SearchEngines/knownEngines"), QStringList()).toStringList();
  known_enginesEnabled = settings.value(QString::fromUtf8("SearchEngines/knownEnginesEnabled"), QList<QVariant>()).toList();
}

void engineSelectDlg::saveSettings() {
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  settings.setValue(QString::fromUtf8("SearchEngines/knownEngines"), installed_engines);
  settings.setValue(QString::fromUtf8("SearchEngines/knownEnginesEnabled"), enginesEnabled);
}

void engineSelectDlg::on_updateButton_clicked() {
  // Download version file from primary server
  downloader->downloadUrl("http://www.dchris.eu/search_engine/versions.txt");
}

void engineSelectDlg::toggleEngineState(QTreeWidgetItem *item, int) {
  int index = pluginsTree->indexOfTopLevelItem(item);
  Q_ASSERT(index != -1);
  bool new_val = !enginesEnabled.at(index).toBool();
  enginesEnabled.replace(index, QVariant(new_val));
  QString enabledTxt;
  if(new_val){
    enabledTxt = tr("True");
    setRowColor(index, "green");
  }else{
    enabledTxt = tr("False");
    setRowColor(index, "red");
  }
  item->setText(ENGINE_STATE, enabledTxt);
}

void engineSelectDlg::displayContextMenu(const QPoint& pos) {
  QMenu myContextMenu(this);
  QModelIndex index;
  // Enable/disable pause/start action given the DL state
  QList<QTreeWidgetItem *> items = pluginsTree->selectedItems();
  bool has_enable = false, has_disable = false;
  QTreeWidgetItem *item;
  foreach(item, items) {
    int index = pluginsTree->indexOfTopLevelItem(item);
    Q_ASSERT(index != -1);
    if(enginesEnabled.at(index).toBool() and !has_disable) {
      myContextMenu.addAction(actionDisable);
      has_disable = true;
    }
    if(!enginesEnabled.at(index).toBool() and !has_enable) {
      myContextMenu.addAction(actionEnable);
      has_enable = true;
    }
    if(has_enable && has_disable) break;
  }
  myContextMenu.addSeparator();
  myContextMenu.addAction(actionUninstall);
  myContextMenu.exec(mapToGlobal(pos)+QPoint(12, 58));
}

void engineSelectDlg::on_closeButton_clicked() {
  close();
}

void engineSelectDlg::on_actionUninstall_triggered() {
  QList<QTreeWidgetItem *> items = pluginsTree->selectedItems();
  QTreeWidgetItem *item;
  bool change = false;
  bool error = false;
  foreach(item, items) {
    int index = pluginsTree->indexOfTopLevelItem(item);
    Q_ASSERT(index != -1);
    QString name = installed_engines.at(index);
    if(QFile::exists(":/search_engine/engines/"+name+".py")) {
      error = true;
      // Disable it instead
      enginesEnabled.replace(index, QVariant(false));
      item->setText(ENGINE_STATE, tr("False"));
      setRowColor(index, "red");
      continue;
    }else {
      // Proceed with uninstall
      // remove it from hard drive
      QFile::remove(misc::qBittorrentPath()+"search_engine"+QDir::separator()+"engines"+QDir::separator()+name+".py");
      if(QFile::exists(misc::qBittorrentPath()+"search_engine"+QDir::separator()+"engines"+QDir::separator()+name+".png")) {
        QFile::remove(misc::qBittorrentPath()+"search_engine"+QDir::separator()+"engines"+QDir::separator()+name+".png");
      }
      // Remove it from lists
      installed_engines.removeAt(index);
      enginesEnabled.removeAt(index);
      pluginsTree->takeTopLevelItem(index);
      change = true;
    }
  }
  if(change)
    saveSettings();
  if(error)
    QMessageBox::warning(0, tr("Uninstall warning"), tr("Some plugins could not be uninstalled because they are included in qBittorrent.\n Only the ones you added yourself can be uninstalled.\nHowever, those plugins were disabled."));
  else
    QMessageBox::information(0, tr("Uninstall success"), tr("All selected plugins were uninstalled successfuly"));
}

void engineSelectDlg::enableSelection() {
  QList<QTreeWidgetItem *> items = pluginsTree->selectedItems();
  QTreeWidgetItem *item;
  foreach(item, items) {
    int index = pluginsTree->indexOfTopLevelItem(item);
    Q_ASSERT(index != -1);
    enginesEnabled.replace(index, QVariant(true));
    item->setText(ENGINE_STATE, tr("True"));
    setRowColor(index, "green");
  }
}

void engineSelectDlg::disableSelection() {
  QList<QTreeWidgetItem *> items = pluginsTree->selectedItems();
  QTreeWidgetItem *item;
  foreach(item, items) {
    int index = pluginsTree->indexOfTopLevelItem(item);
    Q_ASSERT(index != -1);
    enginesEnabled.replace(index, QVariant(false));
    item->setText(ENGINE_STATE, tr("False"));
    setRowColor(index, "red");
  }
}

// Set the color of a row in data model
void engineSelectDlg::setRowColor(int row, QString color){
  QTreeWidgetItem *item = pluginsTree->topLevelItem(row);
  for(int i=0; i<pluginsTree->columnCount(); ++i){
    item->setData(i, Qt::ForegroundRole, QVariant(QColor(color)));
  }
}

void engineSelectDlg::loadSupportedSearchEngines() {
  // Some clean up first
  pluginsTree->clear();
  installed_engines.clear();
  enginesEnabled.clear();
  QStringList params;
  // Ask nova core for the supported search engines
  QProcess nova;
  params << "--supported_engines";
  nova.start(misc::qBittorrentPath()+"search_engine"+QDir::separator()+"nova2.py", params, QIODevice::ReadOnly);
  nova.waitForStarted();
  nova.waitForFinished();
  QByteArray result = nova.readAll();
  result = result.replace("\n", "");
  qDebug("read: %s", result.data());
  QByteArray e;
  foreach(e, result.split(',')) {
    QString en = QString(e);
    installed_engines << en;
    int index = known_engines.indexOf(en);
    if(index == -1)
      enginesEnabled << true;
    else
      enginesEnabled << known_enginesEnabled.at(index).toBool();
  }
  params.clear();
  params << "--supported_engines_infos";
  nova.start(misc::qBittorrentPath()+"search_engine"+QDir::separator()+"nova2.py", params, QIODevice::ReadOnly);
  nova.waitForStarted();
  nova.waitForFinished();
  result = nova.readAll();
  result = result.replace("\n", "");
  qDebug("read: %s", result.data());
  unsigned int i = 0;
  foreach(e, result.split(',')) {
    QString nameUrlCouple(e);
    QStringList line = nameUrlCouple.split('|');
    if(line.size() != 2) continue;
    // Download favicon
    QString enabledTxt;
    if(enginesEnabled.at(i).toBool()){
      enabledTxt = tr("True");
    }else{
      enabledTxt = tr("False");
    }
    line << enabledTxt;
    QTreeWidgetItem *item = new QTreeWidgetItem(pluginsTree, line);
    QString iconPath = misc::qBittorrentPath()+"search_engine"+QDir::separator()+"engines"+QDir::separator()+installed_engines.at(i)+".png";
    if(QFile::exists(iconPath)) {
      // Good, we already have the icon
      item->setData(ENGINE_NAME, Qt::DecorationRole, QVariant(QIcon(iconPath)));
    } else {
      // Icon is missing, we must download it
      downloader->downloadUrl(line.at(1)+"/favicon.ico");
    }
    if(enginesEnabled.at(i).toBool())
      setRowColor(i, "green");
    else
      setRowColor(i, "red");
    ++i;
  }
}

QList<QTreeWidgetItem*> engineSelectDlg::findItemsWithUrl(QString url){
  QList<QTreeWidgetItem*> res;
  for(int i=0; i<pluginsTree->topLevelItemCount(); ++i) {
    QTreeWidgetItem *item = pluginsTree->topLevelItem(i);
    if(url.startsWith(item->text(ENGINE_URL)))
      res << item;
  }
  return res;
}

bool engineSelectDlg::isUpdateNeeded(QString plugin_name, float new_version) {
  float old_version = misc::getPluginVersion(misc::qBittorrentPath()+"search_engine"+QDir::separator()+"engines"+QDir::separator()+plugin_name+".py");
  return (new_version > old_version);
}

void engineSelectDlg::on_installButton_clicked() {
  QStringList pathsList = QFileDialog::getOpenFileNames(0,
              tr("Select search plugins"), QDir::homePath(),
              tr("qBittorrent search plugins")+QString::fromUtf8(" (*.py)"));
  QString path;
  foreach(path, pathsList) {
    if(!path.endsWith(".py")) continue;
    float new_version = misc::getPluginVersion(path);
    QString plugin_name = path.split(QDir::separator()).last();
    plugin_name.replace(".py", "");
    if(!isUpdateNeeded(plugin_name, new_version)) {
      QMessageBox::information(this, tr("Search plugin install")+" -- "+tr("qBittorrent"), tr("A more recent version of %1 search engine plugin is already installed.", "%1 is the name of the search engine").arg(plugin_name.toUtf8().data()));
      continue;
    }
    // Process with install
    QString dest_path = misc::qBittorrentPath()+"search_engine"+QDir::separator()+"engines"+QDir::separator()+plugin_name+".py";
    bool update = false;
    if(QFile::exists(dest_path)) {
      QFile::remove(dest_path);
      update = true;
    }
    // Copy the plugin
    QFile::copy(path, dest_path);
    // Refresh plugin list
    loadSupportedSearchEngines();
    // TODO: do some more checking to be sure it was installed successfuly?
    if(update) {
      QMessageBox::information(this, tr("Search plugin install")+" -- "+tr("qBittorrent"), tr("%1 search engine plugin was successfuly updated.", "%1 is the name of the search engine").arg(plugin_name.toUtf8().data()));
      continue;
    } else {
      QMessageBox::information(this, tr("Search plugin install")+" -- "+tr("qBittorrent"), tr("%1 search engine plugin was successfuly installed.", "%1 is the name of the search engine").arg(plugin_name.toUtf8().data()));
      continue;
    }
  }
}

bool engineSelectDlg::parseVersionsFile(QString versions_file, QString updateServer) {
  qDebug("Checking if update is needed");
  bool file_correct = false;
  QFile versions(versions_file);
  if(!versions.open(QIODevice::ReadOnly | QIODevice::Text)){
    qDebug("* Error: Could not read versions.txt file");
    return false;
  }
  bool updated = false;
  while(!versions.atEnd()) {
    QByteArray line = versions.readLine();
    line.replace("\n", "");
    line = line.trimmed();
    if(line.isEmpty()) continue;
    if(line.startsWith("#")) continue;
    QList<QByteArray> list = line.split(' ');
    if(list.size() != 2) continue;
    QString plugin_name = QString(list.first());
    if(!plugin_name.endsWith(":")) continue;
    plugin_name.chop(1); // remove trailing ':'
    bool ok;
    float version = list.last().toFloat(&ok);
    qDebug("read line %s: %.2f", plugin_name.toUtf8().data(), version);
    if(!ok) continue;
    file_correct = true;
    if(isUpdateNeeded(plugin_name, version)) {
      qDebug("Plugin: %s is outdated", plugin_name.toUtf8().data());
      // Downloading update
      downloader->downloadUrl(updateServer+plugin_name+".zip"); // Actually this is really a .py
      downloader->downloadUrl(updateServer+plugin_name+".png");
      updated = true;
    }else {
      qDebug("Plugin: %s is up to date", plugin_name.toUtf8().data());
    }
  }
  // Close file
  versions.close();
  // Clean up tmp file
  QFile::remove(versions_file);
  if(file_correct && !updated) {
    QMessageBox::information(this, tr("Search plugin update")+" -- "+tr("qBittorrent"), tr("All your plugins are already up to date."));
  }
  return file_correct;
}

void engineSelectDlg::processDownloadedFile(QString url, QString filePath) {
  if(url.endsWith("favicon.ico")){
    // Icon downloaded
    QImage fileIcon;
#ifdef HAVE_MAGICK
    try{
      QFile::copy(filePath, filePath+".ico");
      Image image(QDir::cleanPath(filePath+".ico").toUtf8().data());
        // Convert to PNG since we can't read ICO format
      image.magick("PNG");
        // Resize to 16x16px
      image.sample(Geometry(16, 16));
      image.write(filePath.toUtf8().data());
      QFile::remove(filePath+".ico");
    }catch(Magick::Exception &error_){
      qDebug("favicon conversion to PNG failure: %s", error_.what());
    }
#endif
    if(fileIcon.load(filePath)) {
      QList<QTreeWidgetItem*> items = findItemsWithUrl(url);
      QTreeWidgetItem *item;
      foreach(item, items){
        int index = pluginsTree->indexOfTopLevelItem(item);
        Q_ASSERT(index != -1);
        QString iconPath = misc::qBittorrentPath()+"search_engine"+QDir::separator()+"engines"+QDir::separator()+installed_engines.at(index)+".png";
        QFile::copy(filePath, iconPath);
        item->setData(ENGINE_NAME, Qt::DecorationRole, QVariant(QIcon(iconPath)));
      }
    }
    // Delete tmp file
    QFile::remove(filePath);
    return;
  }
  if(url == "http://www.dchris.eu/search_engine/versions.txt") {
    if(!parseVersionsFile(filePath, "http://www.dchris.eu/search_engine/")) {
      downloader->downloadUrl("http://hydr0g3n.free.fr/search_engine/versions.txt");
      return;
    }
  }
  if(url == "http://hydr0g3n.free.fr/search_engine/versions.txt") {
    if(!parseVersionsFile(filePath, "http://hydr0g3n.free.fr/search_engine/")) {
      QMessageBox::warning(this, tr("Search plugin update")+" -- "+tr("qBittorrent"), tr("Sorry, update server is temporarily unavailable."));
      return;
    }
  }
  if(url.endsWith(".zip")) {
    // a plugin update has been downloaded
    QString plugin_name = url.split('/').last();
    plugin_name.replace(".zip", "");
    QString dest_path = misc::qBittorrentPath()+"search_engine"+QDir::separator()+"engines"+QDir::separator()+plugin_name+".py";
    bool new_plugin = false;
    if(QFile::exists(dest_path)) {
      // Delete the old plugin
      QFile::remove(dest_path);
    } else {
      // This is a new plugin
      new_plugin = true;
    }
    // Copy the new plugin
    QFile::copy(filePath, dest_path);
    if(new_plugin) {
      // if it is new, refresh the list of plugins
      loadSupportedSearchEngines();
    }
    QMessageBox::information(this, tr("Search plugin update")+" -- "+tr("qBittorrent"), tr("%1 search plugin was successfuly updated.", "%1 is the name of the search engine").arg(plugin_name.toUtf8().data()));
  }
}

void engineSelectDlg::handleDownloadFailure(QString url, QString reason) {
  if(url.endsWith("favicon.ico")){
    qDebug("Could not download favicon: %s, reason: %s", url.toUtf8().data(), reason.toUtf8().data());
    return;
  }
  if(url == "http://www.dchris.eu/search_engine/versions.txt") {
    // Primary update server failed, try secondary
    qDebug("Primary update server failed, try secondary");
    downloader->downloadUrl("http://hydr0g3n.free.fr/search_engine/versions.txt");
    return;
  }
  if(url == "http://hydr0g3n.free.fr/search_engine/versions.txt") {
    QMessageBox::warning(this, tr("Search plugin update")+" -- "+tr("qBittorrent"), tr("Sorry, update server is temporarily unavailable."));
    return;
  }
  if(url.endsWith(".zip")) {
    // a plugin update download has been failed
    QString plugin_name = url.split('/').last();
    plugin_name.replace(".zip", "");
    QMessageBox::warning(this, tr("Search plugin update")+" -- "+tr("qBittorrent"), tr("Sorry, %1 search plugin update failed.", "%1 is the name of the search engine").arg(plugin_name.toUtf8().data()));
  }
}
