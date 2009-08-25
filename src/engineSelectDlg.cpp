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

#include "engineSelectDlg.h"
#include "downloadThread.h"
#include "misc.h"
#include "ico.h"
#include "pluginSource.h"
#include <QProcess>
#include <QHeaderView>
#include <QSettings>
#include <QMenu>
#include <QMessageBox>
#include <QFileDialog>
#include <QDropEvent>
#include <QInputDialog>

#ifdef HAVE_ZZIP
#include <zzip/zzip.h>
#endif

#define ENGINE_NAME 0
#define ENGINE_URL 1
#define ENGINE_STATE 2
#define ENGINE_ID 3

engineSelectDlg::engineSelectDlg(QWidget *parent, SupportedEngines *supported_engines) : QDialog(parent), supported_engines(supported_engines) {
  setupUi(this);
  setAttribute(Qt::WA_DeleteOnClose);
  pluginsTree->header()->resizeSection(0, 170);
  pluginsTree->header()->resizeSection(1, 220);
  pluginsTree->hideColumn(ENGINE_ID);
  actionEnable->setIcon(QIcon(QString::fromUtf8(":/Icons/oxygen/button_ok.png")));
  actionDisable->setIcon(QIcon(QString::fromUtf8(":/Icons/oxygen/button_cancel.png")));
  actionUninstall->setIcon(QIcon(QString::fromUtf8(":/Icons/oxygen/list-remove.png")));
  connect(actionEnable, SIGNAL(triggered()), this, SLOT(enableSelection()));
  connect(actionDisable, SIGNAL(triggered()), this, SLOT(disableSelection()));
  connect(pluginsTree, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayContextMenu(const QPoint&)));
  downloader = new downloadThread(this);
  connect(downloader, SIGNAL(downloadFinished(QString, QString)), this, SLOT(processDownloadedFile(QString, QString)));
  connect(downloader, SIGNAL(downloadFailure(QString, QString)), this, SLOT(handleDownloadFailure(QString, QString)));
  loadSupportedSearchEngines();
  connect(supported_engines, SIGNAL(newSupportedEngine(QString)), this, SLOT(addNewEngine(QString)));
  connect(pluginsTree, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(toggleEngineState(QTreeWidgetItem*, int)));
  show();
}

engineSelectDlg::~engineSelectDlg() {
  qDebug("Destroying engineSelectDlg");
  emit enginesChanged();
  qDebug("Before deleting downloader");
  delete downloader;
  qDebug("Engine plugins dialog destroyed");
}

void engineSelectDlg::dropEvent(QDropEvent *event) {
  event->acceptProposedAction();
  QStringList files=event->mimeData()->text().split(QString::fromUtf8("\n"));
  QString file;
  foreach(file, files) {
    qDebug("dropped %s", file.toLocal8Bit().data());
    file = file.replace("file://", "");
    if(file.startsWith("http://", Qt::CaseInsensitive) || file.startsWith("https://", Qt::CaseInsensitive) || file.startsWith("ftp://", Qt::CaseInsensitive)) {
      downloader->downloadUrl(file);
      continue;
    }
    if(file.endsWith(".py", Qt::CaseInsensitive)) {
      QString plugin_name = file.split(QDir::separator()).last();
      plugin_name.replace(".py", "");
      installPlugin(file, plugin_name);
    }
#ifdef HAVE_ZZIP
    if(file.endsWith(".zip", Qt::CaseInsensitive)) {
      installZipPlugin(file);
    }
#endif
  }
}

// Decode if we accept drag 'n drop or not
void engineSelectDlg::dragEnterEvent(QDragEnterEvent *event) {
  QString mime;
  foreach(mime, event->mimeData()->formats()){
    qDebug("mimeData: %s", mime.toLocal8Bit().data());
  }
  if (event->mimeData()->hasFormat(QString::fromUtf8("text/plain")) || event->mimeData()->hasFormat(QString::fromUtf8("text/uri-list"))) {
    event->acceptProposedAction();
  }
}

void engineSelectDlg::on_updateButton_clicked() {
  // Download version file from primary server
  downloader->downloadUrl("http://www.dchris.eu/search_engine2/versions.txt");
}

void engineSelectDlg::toggleEngineState(QTreeWidgetItem *item, int) {
  SupportedEngine *engine = supported_engines->value(item->text(ENGINE_ID));
  engine->setEnabled(!engine->isEnabled());
  if(engine->isEnabled()) {
    item->setText(ENGINE_STATE, tr("Yes"));
    setRowColor(pluginsTree->indexOfTopLevelItem(item), "green");
  } else {
    item->setText(ENGINE_STATE, tr("No"));
    setRowColor(pluginsTree->indexOfTopLevelItem(item), "red");
  }
}

void engineSelectDlg::displayContextMenu(const QPoint&) {
  QMenu myContextMenu(this);
  QModelIndex index;
  // Enable/disable pause/start action given the DL state
  QList<QTreeWidgetItem *> items = pluginsTree->selectedItems();
  bool has_enable = false, has_disable = false;
  QTreeWidgetItem *item;
  foreach(item, items) {
    QString id = item->text(ENGINE_ID);
    if(supported_engines->value(id)->isEnabled() and !has_disable) {
      myContextMenu.addAction(actionDisable);
      has_disable = true;
    }
    if(!supported_engines->value(id)->isEnabled() and !has_enable) {
      myContextMenu.addAction(actionEnable);
      has_enable = true;
    }
    if(has_enable && has_disable) break;
  }
  myContextMenu.addSeparator();
  myContextMenu.addAction(actionUninstall);
  myContextMenu.exec(QCursor::pos());
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
    QString id = item->text(ENGINE_ID);
    if(QFile::exists(":/search_engine/engines/"+id+".py")) {
      error = true;
      // Disable it instead
      supported_engines->value(id)->setEnabled(false);
      item->setText(ENGINE_STATE, tr("No"));
      setRowColor(index, "red");
      continue;
    }else {
      // Proceed with uninstall
      // remove it from hard drive
      QDir enginesFolder(misc::qBittorrentPath()+"search_engine"+QDir::separator()+"engines");
      QStringList filters;
      filters << id+".*";
      QStringList files = enginesFolder.entryList(filters, QDir::Files, QDir::Unsorted);
      QString file;
      foreach(file, files) {
        enginesFolder.remove(file);
      }
      // Remove it from supported engines
      delete supported_engines->take(id);
      delete item;
      change = true;
    }
  }
  if(error)
    QMessageBox::warning(0, tr("Uninstall warning"), tr("Some plugins could not be uninstalled because they are included in qBittorrent.\n Only the ones you added yourself can be uninstalled.\nHowever, those plugins were disabled."));
  else
    QMessageBox::information(0, tr("Uninstall success"), tr("All selected plugins were uninstalled successfully"));
}

void engineSelectDlg::enableSelection() {
  QList<QTreeWidgetItem *> items = pluginsTree->selectedItems();
  QTreeWidgetItem *item;
  foreach(item, items) {
    int index = pluginsTree->indexOfTopLevelItem(item);
    Q_ASSERT(index != -1);
    QString id = item->text(ENGINE_ID);
    supported_engines->value(id)->setEnabled(true);
    item->setText(ENGINE_STATE, tr("Yes"));
    setRowColor(index, "green");
  }
}

void engineSelectDlg::disableSelection() {
  QList<QTreeWidgetItem *> items = pluginsTree->selectedItems();
  QTreeWidgetItem *item;
  foreach(item, items) {
    int index = pluginsTree->indexOfTopLevelItem(item);
    Q_ASSERT(index != -1);
    QString id = item->text(ENGINE_ID);
    supported_engines->value(id)->setEnabled(false);
    item->setText(ENGINE_STATE, tr("No"));
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

QList<QTreeWidgetItem*> engineSelectDlg::findItemsWithUrl(QString url){
  QList<QTreeWidgetItem*> res;
  for(int i=0; i<pluginsTree->topLevelItemCount(); ++i) {
    QTreeWidgetItem *item = pluginsTree->topLevelItem(i);
    if(url.startsWith(item->text(ENGINE_URL), Qt::CaseInsensitive))
      res << item;
  }
  return res;
}

QTreeWidgetItem* engineSelectDlg::findItemWithID(QString id){
  QList<QTreeWidgetItem*> res;
  for(int i=0; i<pluginsTree->topLevelItemCount(); ++i) {
    QTreeWidgetItem *item = pluginsTree->topLevelItem(i);
    if(id == item->text(ENGINE_ID))
      return item;
  }
  return 0;
}

bool engineSelectDlg::isUpdateNeeded(QString plugin_name, float new_version) const {
  float old_version = misc::getPluginVersion(misc::qBittorrentPath()+"search_engine"+QDir::separator()+"engines"+QDir::separator()+plugin_name+".py");
  qDebug("IsUpdate needed? tobeinstalled: %.2f, alreadyinstalled: %.2f", new_version, old_version);
  return (new_version > old_version);
}

#ifdef HAVE_ZZIP
void engineSelectDlg::installZipPlugin(QString path) {
  QStringList plugins;
  QStringList favicons;
  ZZIP_DIR* dir = zzip_dir_open(path.toLocal8Bit().data(), 0);
  if(!dir) {
    QMessageBox::warning(this, tr("Search plugin install")+" -- "+tr("qBittorrent"), tr("Search engine plugin archive could not be read."));
    return;
  }
  ZZIP_DIRENT dirent;
  while(zzip_dir_read(dir, &dirent)) {
    /* show info for first file */
    QString name(dirent.d_name);
    if(name.endsWith(".py", Qt::CaseInsensitive)) {
      plugins << name;
    } else {
      if(name.endsWith(".png", Qt::CaseInsensitive)) {
        favicons << name;
      }
    }
  }
  QString plugin;
  std::cout << dirent.d_name << std::endl;
  ZZIP_FILE* fp = zzip_file_open(dir, dirent.d_name, 0);
  if (fp) {
    char buf[10];
    zzip_ssize_t len = zzip_file_read(fp, buf, 10);
    if (len) {
      /* show head of README */
      std::cout << buf;
    }
    zzip_file_close(fp);
    std::cout << std::endl;
  }
  foreach(plugin, plugins) {
    QString plugin_name = plugin.split(QDir::separator()).last();
    plugin_name.chop(3); // Remove .py extension
    qDebug("Detected plugin %s in archive", plugin_name.toLocal8Bit().data());
    ZZIP_FILE* fp = zzip_file_open(dir, plugin.toLocal8Bit().data(), 0);
    if(fp) {
      QTemporaryFile *tmpfile = new QTemporaryFile();
      QString tmpPath;
      // Write file
      if(tmpfile->open()) {
        tmpPath = tmpfile->fileName();
        char buf[255];
        zzip_ssize_t len = zzip_file_read(fp, buf, 255);
        while(len) {
          tmpfile->write(buf, len);
          len = zzip_file_read(fp, buf, 255);
        }
        zzip_file_close(fp);
        tmpfile->close();
      } else {
        qDebug("Could not open tmp file");
        QMessageBox::warning(this, tr("Search plugin install")+" -- "+tr("qBittorrent"), tr("%1 search engine plugin could not be installed.", "%1 is the name of the search engine").arg(plugin_name.toLocal8Bit().data()));
        delete tmpfile;
        continue;
      }
      // Install plugin
      installPlugin(tmpPath, plugin_name);
      qDebug("installPlugin() finished");
      delete tmpfile;
      qDebug("Deleted tmpfile");
    } else {
      qDebug("Cannot read file in archive");
      QMessageBox::warning(this, tr("Search plugin install")+" -- "+tr("qBittorrent"), tr("%1 search engine plugin could not be installed.", "%1 is the name of the search engine").arg(plugin_name.toLocal8Bit().data()));
    }
  }
  QString favicon;
  foreach(favicon, favicons) {
    qDebug("Detected favicon %s in archive", favicon.toLocal8Bit().data());
    // Ok we have a favicon here
    QString plugin_name = favicon.split(QDir::separator()).last();
    plugin_name.chop(4); // Remove .png extension
    if(!QFile::exists(misc::qBittorrentPath()+"search_engine"+QDir::separator()+"engines"+QDir::separator()+plugin_name+".py"))
      continue;
    // Check if we already have a favicon for this plugin
    QString iconPath = misc::qBittorrentPath()+"search_engine"+QDir::separator()+"engines"+QDir::separator()+plugin_name+".png";
    if(QFile::exists(iconPath)) {
      QFile::remove(iconPath);
    }
    ZZIP_FILE* fp = zzip_file_open(dir, favicon.toLocal8Bit().data(), 0);
    if(fp) {
      QFile dest_icon(iconPath);
      // Write icon
      if(dest_icon.open(QIODevice::WriteOnly | QIODevice::Text)) {
        char buf[255];
        zzip_ssize_t len = zzip_file_read(fp, buf, 255);
        while(len) {
          dest_icon.write(buf, len);
          len = zzip_file_read(fp, buf, 255);
        }
        zzip_file_close(fp);
        dest_icon.close();
        // Update icon in list
        QTreeWidgetItem *item = findItemWithID(plugin_name);
        Q_ASSERT(item);
        item->setData(ENGINE_NAME, Qt::DecorationRole, QVariant(QIcon(iconPath)));
      }
    }
  }
  zzip_dir_close(dir);
}
#endif

void engineSelectDlg::installPlugin(QString path, QString plugin_name) {
  qDebug("Asked to install plugin at %s", path.toLocal8Bit().data());
  float new_version = misc::getPluginVersion(path);
  qDebug("Version to be installed: %.2f", new_version);
  if(!isUpdateNeeded(plugin_name, new_version)) {
    qDebug("Apparently update it not needed, we have a more recent version");
    QMessageBox::information(this, tr("Search plugin install")+" -- "+tr("qBittorrent"), tr("A more recent version of %1 search engine plugin is already installed.", "%1 is the name of the search engine").arg(plugin_name.toLocal8Bit().data()));
    return;
  }
  // Process with install
  QString dest_path = misc::qBittorrentPath()+"search_engine"+QDir::separator()+"engines"+QDir::separator()+plugin_name+".py";
  bool update = false;
  if(QFile::exists(dest_path)) {
    // Backup in case install fails
    QFile::copy(dest_path, dest_path+".bak");
    QFile::remove(dest_path);
    update = true;
  }
  // Copy the plugin
  QFile::copy(path, dest_path);
  // Update supported plugins
  supported_engines->update();
  // Check if this was correctly installed
  if(!supported_engines->contains(plugin_name)) {
    if(update) {
      // Remove broken file
      QFile::remove(dest_path);
      // restore backup
      QFile::copy(dest_path+".bak", dest_path);
      QFile::remove(dest_path+".bak");
      QMessageBox::warning(this, tr("Search plugin install")+" -- "+tr("qBittorrent"), tr("%1 search engine plugin could not be updated, keeping old version.", "%1 is the name of the search engine").arg(plugin_name.toLocal8Bit().data()));
      return;
    } else {
      // Remove broken file
      QFile::remove(dest_path);
      QMessageBox::warning(this, tr("Search plugin install")+" -- "+tr("qBittorrent"), tr("%1 search engine plugin could not be installed.", "%1 is the name of the search engine").arg(plugin_name.toLocal8Bit().data()));
      return;
    }
  }
  // Install was successful, remove backup
  if(update) {
    QFile::remove(dest_path+".bak");
  }
  if(update) {
    QMessageBox::information(this, tr("Search plugin install")+" -- "+tr("qBittorrent"), tr("%1 search engine plugin was successfully updated.", "%1 is the name of the search engine").arg(plugin_name.toLocal8Bit().data()));
    return;
  } else {
    QMessageBox::information(this, tr("Search plugin install")+" -- "+tr("qBittorrent"), tr("%1 search engine plugin was successfully installed.", "%1 is the name of the search engine").arg(plugin_name.toLocal8Bit().data()));
    return;
  }
}

void engineSelectDlg::loadSupportedSearchEngines() {
  // Some clean up first
  pluginsTree->clear();
  foreach(QString name, supported_engines->keys()) {
    addNewEngine(name);
  }
}

void engineSelectDlg::addNewEngine(QString engine_name) {
  QTreeWidgetItem *item = new QTreeWidgetItem(pluginsTree);
  SupportedEngine *engine = supported_engines->value(engine_name);
  item->setText(ENGINE_NAME, engine->getFullName());
  item->setText(ENGINE_URL, engine->getUrl());
  item->setText(ENGINE_ID, engine->getName());
  if(engine->isEnabled()) {
    item->setText(ENGINE_STATE, tr("Yes"));
    setRowColor(pluginsTree->indexOfTopLevelItem(item), "green");
  } else {
    item->setText(ENGINE_STATE, tr("No"));
    setRowColor(pluginsTree->indexOfTopLevelItem(item), "red");
  }
  // Handle icon
  QString iconPath = misc::qBittorrentPath()+"search_engine"+QDir::separator()+"engines"+QDir::separator()+engine->getName()+".png";
  if(QFile::exists(iconPath)) {
    // Good, we already have the icon
    item->setData(ENGINE_NAME, Qt::DecorationRole, QVariant(QIcon(iconPath)));
  } else {
    iconPath = misc::qBittorrentPath()+"search_engine"+QDir::separator()+"engines"+QDir::separator()+engine->getName()+".ico";
    if(QFile::exists(iconPath)) { // ICO support
      item->setData(ENGINE_NAME, Qt::DecorationRole, QVariant(QIcon(iconPath)));
    } else {
      // Icon is missing, we must download it
      downloader->downloadUrl(engine->getUrl()+"/favicon.ico");
    }
  }
}

  void engineSelectDlg::on_installButton_clicked() {
    pluginSourceDlg *dlg = new pluginSourceDlg(this);
    connect(dlg, SIGNAL(askForLocalFile()), this, SLOT(askForLocalPlugin()));
    connect(dlg, SIGNAL(askForUrl()), this, SLOT(askForPluginUrl()));
  }

  void engineSelectDlg::askForPluginUrl() {
    bool ok;
    QString url = QInputDialog::getText(this, tr("New search engine plugin URL"),
                                        tr("URL:"), QLineEdit::Normal,
                                        "http://", &ok);
    if (ok && !url.isEmpty())
      downloader->downloadUrl(url);
  }

  void engineSelectDlg::askForLocalPlugin() {
    QStringList pathsList = QFileDialog::getOpenFileNames(0,
                                                          tr("Select search plugins"), QDir::homePath(),
#ifdef HAVE_ZZIP
                                                          tr("qBittorrent search plugins")+QString::fromUtf8(" (*.py *.zip)"));
#else
    tr("qBittorrent search plugins")+QString::fromUtf8(" (*.py)"));
#endif
    QString path;
    foreach(path, pathsList) {
      if(path.endsWith(".py", Qt::CaseInsensitive)) {
        QString plugin_name = path.split(QDir::separator()).last();
        plugin_name.replace(".py", "", Qt::CaseInsensitive);
        installPlugin(path, plugin_name);
      }
#ifdef HAVE_ZZIP
      else {
        if(path.endsWith(".zip", Qt::CaseInsensitive)) {
          installZipPlugin(path);
        }
      }
#endif
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
      qDebug("read line %s: %.2f", plugin_name.toLocal8Bit().data(), version);
      if(!ok) continue;
      file_correct = true;
      if(isUpdateNeeded(plugin_name, version)) {
        qDebug("Plugin: %s is outdated", plugin_name.toLocal8Bit().data());
        // Downloading update
        downloader->downloadUrl(updateServer+plugin_name+".pyqBT"); // Actually this is really a .py
        downloader->downloadUrl(updateServer+plugin_name+".png");
        updated = true;
      }else {
        qDebug("Plugin: %s is up to date", plugin_name.toLocal8Bit().data());
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
    qDebug("engineSelectDlg received %s", url.toLocal8Bit().data());
    if(url.endsWith("favicon.ico", Qt::CaseInsensitive)){
      // Icon downloaded
      QImage fileIcon;
      if(fileIcon.load(filePath)) {
        QList<QTreeWidgetItem*> items = findItemsWithUrl(url);
        QTreeWidgetItem *item;
        foreach(item, items){
          QString id = item->text(ENGINE_ID);
          QString iconPath;
          QFile icon(filePath);
          icon.open(QIODevice::ReadOnly);
          if(ICOHandler::canRead(&icon))
            iconPath = misc::qBittorrentPath()+"search_engine"+QDir::separator()+"engines"+QDir::separator()+id+".ico";
          else
            iconPath = misc::qBittorrentPath()+"search_engine"+QDir::separator()+"engines"+QDir::separator()+id+".png";
          QFile::copy(filePath, iconPath);
          item->setData(ENGINE_NAME, Qt::DecorationRole, QVariant(QIcon(iconPath)));
        }
      }
      // Delete tmp file
      QFile::remove(filePath);
      return;
    }
    if(url == "http://www.dchris.eu/search_engine2/versions.txt") {
      if(!parseVersionsFile(filePath, "http://www.dchris.eu/search_engine2/")) {
        qDebug("Primary update server failed, try secondary");
        downloader->downloadUrl("http://hydr0g3n.free.fr/search_engine2/versions.txt");
      }
      QFile::remove(filePath);
      return;
    }
    if(url == "http://hydr0g3n.free.fr/search_engine2/versions.txt") {
      if(!parseVersionsFile(filePath, "http://hydr0g3n.free.fr/search_engine2/")) {
        QMessageBox::warning(this, tr("Search plugin update")+" -- "+tr("qBittorrent"), tr("Sorry, update server is temporarily unavailable."));
      }
      QFile::remove(filePath);
      return;
    }
    if(url.endsWith(".pyqBT", Qt::CaseInsensitive) || url.endsWith(".py", Qt::CaseInsensitive)) {
      QString plugin_name = url.split('/').last();
      plugin_name.replace(".pyqBT", "");
      plugin_name.replace(".py", "");
      installPlugin(filePath, plugin_name);
      QFile::remove(filePath);
      return;
    }
#ifdef HAVE_ZZIP
    if(url.endsWith(".zip", Qt::CaseInsensitive)) {
      installZipPlugin(filePath);
      QFile::remove(filePath);
      return;
    }
#endif
  }

  void engineSelectDlg::handleDownloadFailure(QString url, QString reason) {
    if(url.endsWith("favicon.ico", Qt::CaseInsensitive)){
      qDebug("Could not download favicon: %s, reason: %s", url.toLocal8Bit().data(), reason.toLocal8Bit().data());
      return;
    }
    if(url == "http://www.dchris.eu/search_engine2/versions.txt") {
      // Primary update server failed, try secondary
      qDebug("Primary update server failed, try secondary");
      downloader->downloadUrl("http://hydr0g3n.free.fr/search_engine2/versions.txt");
      return;
    }
    if(url == "http://hydr0g3n.free.fr/search_engine2/versions.txt") {
      QMessageBox::warning(this, tr("Search plugin update")+" -- "+tr("qBittorrent"), tr("Sorry, update server is temporarily unavailable."));
      return;
    }
    if(url.endsWith(".pyqBT", Qt::CaseInsensitive) || url.endsWith(".py", Qt::CaseInsensitive)) {
      // a plugin update download has been failed
      QString plugin_name = url.split('/').last();
      plugin_name.replace(".pyqBT", "", Qt::CaseInsensitive);
      plugin_name.replace(".py", "", Qt::CaseInsensitive);
      QMessageBox::warning(this, tr("Search plugin update")+" -- "+tr("qBittorrent"), tr("Sorry, %1 search plugin install failed.", "%1 is the name of the search engine").arg(plugin_name.toLocal8Bit().data()));
    }
#ifdef HAVE_ZZIP
    if(url.endsWith(".zip", Qt::CaseInsensitive)) {
      QString plugin_name = url.split('/').last();
      plugin_name.replace(".zip", "", Qt::CaseInsensitive);
      QMessageBox::warning(this, tr("Search plugin update")+" -- "+tr("qBittorrent"), tr("Sorry, %1 search plugin install failed.", "%1 is the name of the search engine").arg(plugin_name.toLocal8Bit().data()));
    }
#endif
  }
