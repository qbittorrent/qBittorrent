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

#include "engineselectdlg.h"
#include "core/net/downloadmanager.h"
#include "core/net/downloadhandler.h"
#include "core/utils/fs.h"
#include "core/utils/misc.h"
#include "ico.h"
#include "searchengine.h"
#include "pluginsource.h"
#include "guiiconprovider.h"
#include "autoexpandabledialog.h"
#include <QProcess>
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QFileDialog>
#include <QDropEvent>
#include <QTemporaryFile>
#include <QMimeData>

enum EngineColumns {ENGINE_NAME, ENGINE_VERSION, ENGINE_URL, ENGINE_STATE, ENGINE_ID};
const QString UPDATE_URL = QString("https://raw.github.com/qbittorrent/qBittorrent/master/src/searchengine/") + (Utils::Misc::pythonVersion() >= 3 ? "nova3" : "nova") + "/engines/";

engineSelectDlg::engineSelectDlg(QWidget *parent, SupportedEngines *supported_engines) : QDialog(parent), supported_engines(supported_engines) {
  setupUi(this);
  setAttribute(Qt::WA_DeleteOnClose);
  pluginsTree->setRootIsDecorated(false);
  pluginsTree->header()->resizeSection(0, 160);
  pluginsTree->header()->resizeSection(1, 80);
  pluginsTree->header()->resizeSection(2, 200);
  pluginsTree->hideColumn(ENGINE_ID);
  actionUninstall->setIcon(GuiIconProvider::instance()->getIcon("list-remove"));
  connect(actionEnable, SIGNAL(toggled(bool)), this, SLOT(enableSelection(bool)));
  connect(pluginsTree, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayContextMenu(const QPoint&)));
  loadSupportedSearchEngines();
  connect(supported_engines, SIGNAL(newSupportedEngine(QString)), this, SLOT(addNewEngine(QString)));
  connect(pluginsTree, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(toggleEngineState(QTreeWidgetItem*, int)));
  show();
}

engineSelectDlg::~engineSelectDlg() {
  qDebug("Destroying engineSelectDlg");
  emit enginesChanged();
  qDebug("Engine plugins dialog destroyed");
}

void engineSelectDlg::dropEvent(QDropEvent *event) {
  event->acceptProposedAction();
  QStringList files = event->mimeData()->text().split(QString::fromUtf8("\n"));
  foreach (QString file, files) {
    qDebug("dropped %s", qPrintable(file));
    if (Utils::Misc::isUrl(file)) {
      setCursor(QCursor(Qt::WaitCursor));
      downloadFromUrl(file);
      continue;
    }
    if (file.endsWith(".py", Qt::CaseInsensitive)) {
      if (file.startsWith("file:", Qt::CaseInsensitive))
        file = QUrl(file).toLocalFile();
      QString plugin_name = Utils::Fs::fileName(file);
      plugin_name.chop(3); // Remove extension
      installPlugin(file, plugin_name);
    }
  }
}

// Decode if we accept drag 'n drop or not
void engineSelectDlg::dragEnterEvent(QDragEnterEvent *event) {
  QString mime;
  foreach (mime, event->mimeData()->formats()) {
    qDebug("mimeData: %s", qPrintable(mime));
  }
  if (event->mimeData()->hasFormat(QString::fromUtf8("text/plain")) || event->mimeData()->hasFormat(QString::fromUtf8("text/uri-list"))) {
    event->acceptProposedAction();
  }
}

void engineSelectDlg::on_updateButton_clicked() {
  // Download version file from update server on sourceforge
  setCursor(QCursor(Qt::WaitCursor));
  downloadFromUrl(QString(UPDATE_URL) + "versions.txt");
}

void engineSelectDlg::toggleEngineState(QTreeWidgetItem *item, int) {
  SupportedEngine *engine = supported_engines->value(item->text(ENGINE_ID));
  engine->setEnabled(!engine->isEnabled());
  if (engine->isEnabled()) {
    item->setText(ENGINE_STATE, tr("Yes"));
    setRowColor(pluginsTree->indexOfTopLevelItem(item), "green");
  } else {
    item->setText(ENGINE_STATE, tr("No"));
    setRowColor(pluginsTree->indexOfTopLevelItem(item), "red");
  }
}

void engineSelectDlg::displayContextMenu(const QPoint&) {
  QMenu myContextMenu(this);
  // Enable/disable pause/start action given the DL state
  QList<QTreeWidgetItem *> items = pluginsTree->selectedItems();
  if (items.isEmpty()) return;
  QString first_id = items.first()->text(ENGINE_ID);
  actionEnable->setChecked(supported_engines->value(first_id)->isEnabled());
  myContextMenu.addAction(actionEnable);
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
  bool error = false;
  foreach (item, items) {
    int index = pluginsTree->indexOfTopLevelItem(item);
    Q_ASSERT(index != -1);
    QString id = item->text(ENGINE_ID);
    if (QFile::exists(":/nova/engines/"+id+".py")) {
      error = true;
      // Disable it instead
      supported_engines->value(id)->setEnabled(false);
      item->setText(ENGINE_STATE, tr("No"));
      setRowColor(index, "red");
      continue;
    }else {
      // Proceed with uninstall
      // remove it from hard drive
      QDir enginesFolder(Utils::Fs::searchEngineLocation() + "/engines");
      QStringList filters;
      filters << id+".*";
      QStringList files = enginesFolder.entryList(filters, QDir::Files, QDir::Unsorted);
      QString file;
      foreach (file, files) {
        enginesFolder.remove(file);
      }
      // Remove it from supported engines
      delete supported_engines->take(id);
      delete item;
    }
  }
  if (error)
    QMessageBox::warning(0, tr("Uninstall warning"), tr("Some plugins could not be uninstalled because they are included in qBittorrent. Only the ones you added yourself can be uninstalled.\nThose plugins were disabled."));
  else
    QMessageBox::information(0, tr("Uninstall success"), tr("All selected plugins were uninstalled successfully"));
}

void engineSelectDlg::enableSelection(bool enable) {
  QList<QTreeWidgetItem *> items = pluginsTree->selectedItems();
  QTreeWidgetItem *item;
  foreach (item, items) {
    int index = pluginsTree->indexOfTopLevelItem(item);
    Q_ASSERT(index != -1);
    QString id = item->text(ENGINE_ID);
    supported_engines->value(id)->setEnabled(enable);
    if (enable) {
      item->setText(ENGINE_STATE, tr("Yes"));
      setRowColor(index, "green");
    } else {
      item->setText(ENGINE_STATE, tr("No"));
      setRowColor(index, "red");
    }
  }
}

// Set the color of a row in data model
void engineSelectDlg::setRowColor(int row, QString color) {
  QTreeWidgetItem *item = pluginsTree->topLevelItem(row);
  for (int i=0; i<pluginsTree->columnCount(); ++i) {
    item->setData(i, Qt::ForegroundRole, QVariant(QColor(color)));
  }
}

QList<QTreeWidgetItem*> engineSelectDlg::findItemsWithUrl(QString url) {
  QList<QTreeWidgetItem*> res;
  for (int i=0; i<pluginsTree->topLevelItemCount(); ++i) {
    QTreeWidgetItem *item = pluginsTree->topLevelItem(i);
    if (url.startsWith(item->text(ENGINE_URL), Qt::CaseInsensitive))
      res << item;
  }
  return res;
}

QTreeWidgetItem* engineSelectDlg::findItemWithID(QString id) {
  QList<QTreeWidgetItem*> res;
  for (int i=0; i<pluginsTree->topLevelItemCount(); ++i) {
    QTreeWidgetItem *item = pluginsTree->topLevelItem(i);
    if (id == item->text(ENGINE_ID))
      return item;
  }
  return 0;
}

bool engineSelectDlg::isUpdateNeeded(QString plugin_name, qreal new_version) const {
  qreal old_version = SearchEngine::getPluginVersion(Utils::Fs::searchEngineLocation() + "/engines/" + plugin_name + ".py");
  qDebug("IsUpdate needed? tobeinstalled: %.2f, alreadyinstalled: %.2f", new_version, old_version);
  return (new_version > old_version);
}

void engineSelectDlg::installPlugin(QString path, QString plugin_name) {
  qDebug("Asked to install plugin at %s", qPrintable(path));
  qreal new_version = SearchEngine::getPluginVersion(path);
  qDebug("Version to be installed: %.2f", new_version);
  if (!isUpdateNeeded(plugin_name, new_version)) {
    qDebug("Apparently update is not needed, we have a more recent version");
    QMessageBox::information(this, tr("Search plugin install"), tr("A more recent version of %1 search engine plugin is already installed.", "%1 is the name of the search engine").arg(plugin_name));
    return;
  }
  // Process with install
  QString dest_path = Utils::Fs::searchEngineLocation() + "/engines/" + plugin_name + ".py";
  bool update = false;
  if (QFile::exists(dest_path)) {
    // Backup in case install fails
    QFile::copy(dest_path, dest_path+".bak");
    Utils::Fs::forceRemove(dest_path);
    Utils::Fs::forceRemove(dest_path+"c");
    update = true;
  }
  // Copy the plugin
  QFile::copy(path, dest_path);
  // Update supported plugins
  supported_engines->update();
  // Check if this was correctly installed
  if (!supported_engines->contains(plugin_name)) {
    if (update) {
      // Remove broken file
      Utils::Fs::forceRemove(dest_path);
      // restore backup
      QFile::copy(dest_path+".bak", dest_path);
      Utils::Fs::forceRemove(dest_path+".bak");
      QMessageBox::warning(this, tr("Search plugin install"), tr("%1 search engine plugin could not be updated, keeping old version.", "%1 is the name of the search engine").arg(plugin_name));
      return;
    } else {
      // Remove broken file
      Utils::Fs::forceRemove(dest_path);
      QMessageBox::warning(this, tr("Search plugin install"), tr("%1 search engine plugin could not be installed.", "%1 is the name of the search engine").arg(plugin_name));
      return;
    }
  }
  // Install was successful, remove backup and update plugin version
  if (update) {
    Utils::Fs::forceRemove(dest_path+".bak");
    qreal version = SearchEngine::getPluginVersion(Utils::Fs::searchEngineLocation() + "/engines/" + plugin_name + ".py");
    QTreeWidgetItem *item = findItemWithID(plugin_name);
    item->setText(ENGINE_VERSION, QString::number(version, 'f', 2));
    QMessageBox::information(this, tr("Search plugin install"), tr("%1 search engine plugin was successfully updated.", "%1 is the name of the search engine").arg(plugin_name));
    return;
  } else {
    QMessageBox::information(this, tr("Search plugin install"), tr("%1 search engine plugin was successfully installed.", "%1 is the name of the search engine").arg(plugin_name));
    return;
  }
}

void engineSelectDlg::loadSupportedSearchEngines() {
  // Some clean up first
  pluginsTree->clear();
  foreach (QString name, supported_engines->keys()) {
    addNewEngine(name);
  }
}

void engineSelectDlg::addNewEngine(QString engine_name) {
  QTreeWidgetItem *item = new QTreeWidgetItem(pluginsTree);
  SupportedEngine *engine = supported_engines->value(engine_name);
  item->setText(ENGINE_NAME, engine->getFullName());
  item->setText(ENGINE_URL, engine->getUrl());
  item->setText(ENGINE_ID, engine->getName());
  if (engine->isEnabled()) {
    item->setText(ENGINE_STATE, tr("Yes"));
    setRowColor(pluginsTree->indexOfTopLevelItem(item), "green");
  } else {
    item->setText(ENGINE_STATE, tr("No"));
    setRowColor(pluginsTree->indexOfTopLevelItem(item), "red");
  }
  // Handle icon
  QString iconPath = Utils::Fs::searchEngineLocation() + "/engines/" + engine->getName() + ".png";
  if (QFile::exists(iconPath)) {
    // Good, we already have the icon
    item->setData(ENGINE_NAME, Qt::DecorationRole, QVariant(QIcon(iconPath)));
  } else {
    iconPath = Utils::Fs::searchEngineLocation() + "/engines/" + engine->getName() + ".ico";
    if (QFile::exists(iconPath)) { // ICO support
      item->setData(ENGINE_NAME, Qt::DecorationRole, QVariant(QIcon(iconPath)));
    } else {
      // Icon is missing, we must download it
      downloadFromUrl(engine->getUrl() + "/favicon.ico");
    }
  }
  // Load version
  qreal version = SearchEngine::getPluginVersion(Utils::Fs::searchEngineLocation() + "/engines/" + engine->getName() + ".py");
  item->setText(ENGINE_VERSION, QString::number(version, 'f', 2));
}

void engineSelectDlg::on_installButton_clicked() {
  pluginSourceDlg *dlg = new pluginSourceDlg(this);
  connect(dlg, SIGNAL(askForLocalFile()), this, SLOT(askForLocalPlugin()));
  connect(dlg, SIGNAL(askForUrl()), this, SLOT(askForPluginUrl()));
}

void engineSelectDlg::askForPluginUrl() {
  bool ok(false);
  QString url = AutoExpandableDialog::getText(this, tr("New search engine plugin URL"),
                                      tr("URL:"), QLineEdit::Normal,
                                      "http://", &ok);

  while(true) {
    if (!ok || url.isEmpty())
      return;
    if (!url.endsWith(".py")) {
      QMessageBox::warning(this, tr("Invalid link"), tr("The link doesn't seem to point to a search engine plugin."));
      url = AutoExpandableDialog::getText(this, tr("New search engine plugin URL"),
                                            tr("URL:"), QLineEdit::Normal,
                                            url, &ok);
    }
    else
      break;
  }

  setCursor(QCursor(Qt::WaitCursor));
  downloadFromUrl(url);
}

void engineSelectDlg::askForLocalPlugin() {
  QStringList pathsList = QFileDialog::getOpenFileNames(0,
                                                        tr("Select search plugins"), QDir::homePath(),
                                                        tr("qBittorrent search plugin")+QString::fromUtf8(" (*.py)"));
  QString path;
  foreach (path, pathsList) {
    if (path.endsWith(".py", Qt::CaseInsensitive)) {
      QString plugin_name = path.split("/").last();
      plugin_name.replace(".py", "", Qt::CaseInsensitive);
      installPlugin(path, plugin_name);
    }
  }
}

bool engineSelectDlg::parseVersionsFile(QString versions_file) {
  qDebug("Checking if update is needed");
  bool file_correct = false;
  QFile versions(versions_file);
  if (!versions.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qDebug("* Error: Could not read versions.txt file");
    return false;
  }
  bool updated = false;
  while(!versions.atEnd()) {
    QByteArray line = versions.readLine();
    line.replace("\n", "");
    line = line.trimmed();
    if (line.isEmpty()) continue;
    if (line.startsWith("#")) continue;
    QList<QByteArray> list = line.split(' ');
    if (list.size() != 2) continue;
    QString plugin_name = QString(list.first());
    if (!plugin_name.endsWith(":")) continue;
    plugin_name.chop(1); // remove trailing ':'
    bool ok;
    qreal version = list.last().toFloat(&ok);
    qDebug("read line %s: %.2f", qPrintable(plugin_name), version);
    if (!ok) continue;
    file_correct = true;
    if (isUpdateNeeded(plugin_name, version)) {
      qDebug("Plugin: %s is outdated", qPrintable(plugin_name));
      // Downloading update
      setCursor(QCursor(Qt::WaitCursor));
      downloadFromUrl(UPDATE_URL + plugin_name + ".py");
      //downloadFromUrl(UPDATE_URL + plugin_name + ".png");
      updated = true;
    }else {
      qDebug("Plugin: %s is up to date", qPrintable(plugin_name));
    }
  }
  // Close file
  versions.close();
  // Clean up tmp file
  Utils::Fs::forceRemove(versions_file);
  if (file_correct && !updated) {
    QMessageBox::information(this, tr("Search plugin update"), tr("All your plugins are already up to date."));
  }
  return file_correct;
}

void engineSelectDlg::downloadFromUrl(const QString &url)
{
    Net::DownloadHandler *handler = Net::DownloadManager::instance()->downloadUrl(url);
    connect(handler, SIGNAL(downloadFinished(QString, QString)), this, SLOT(processDownloadedFile(QString, QString)));
    connect(handler, SIGNAL(downloadFailed(QString, QString)), this, SLOT(handleDownloadFailure(QString, QString)));
}

void engineSelectDlg::processDownloadedFile(const QString &url, QString filePath) {
  filePath = Utils::Fs::fromNativePath(filePath);
  setCursor(QCursor(Qt::ArrowCursor));
  qDebug("engineSelectDlg received %s", qPrintable(url));
  if (url.endsWith("favicon.ico", Qt::CaseInsensitive)) {
    // Icon downloaded
    QImage fileIcon;
    if (fileIcon.load(filePath)) {
      QList<QTreeWidgetItem*> items = findItemsWithUrl(url);
      QTreeWidgetItem *item;
      foreach (item, items) {
        QString id = item->text(ENGINE_ID);
        QString iconPath;
        QFile icon(filePath);
        icon.open(QIODevice::ReadOnly);
        if (ICOHandler::canRead(&icon))
          iconPath = Utils::Fs::searchEngineLocation() + "/engines/" + id + ".ico";
        else
          iconPath = Utils::Fs::searchEngineLocation() + "/engines/" + id + ".png";
        QFile::copy(filePath, iconPath);
        item->setData(ENGINE_NAME, Qt::DecorationRole, QVariant(QIcon(iconPath)));
      }
    }
    // Delete tmp file
    Utils::Fs::forceRemove(filePath);
    return;
  }
  if (url.endsWith("versions.txt")) {
    if (!parseVersionsFile(filePath)) {
      QMessageBox::warning(this, tr("Search plugin update"), tr("Sorry, update server is temporarily unavailable."));
    }
    Utils::Fs::forceRemove(filePath);
    return;
  }
  if (url.endsWith(".py", Qt::CaseInsensitive)) {
    QString plugin_name = Utils::Fs::fileName(url);
    plugin_name.chop(3); // Remove extension
    installPlugin(filePath, plugin_name);
    Utils::Fs::forceRemove(filePath);
    return;
  }
}

void engineSelectDlg::handleDownloadFailure(const QString &url, const QString &reason) {
  setCursor(QCursor(Qt::ArrowCursor));
  if (url.endsWith("favicon.ico", Qt::CaseInsensitive)) {
    qDebug("Could not download favicon: %s, reason: %s", qPrintable(url), qPrintable(reason));
    return;
  }
  if (url.endsWith("versions.txt")) {
    QMessageBox::warning(this, tr("Search plugin update"), tr("Sorry, update server is temporarily unavailable."));
    return;
  }
  if (url.endsWith(".py", Qt::CaseInsensitive)) {
    // a plugin update download has been failed
    QString plugin_name = url.split('/').last();
    plugin_name.replace(".py", "", Qt::CaseInsensitive);
    QMessageBox::warning(this, tr("Search plugin update"), tr("Sorry, %1 search plugin installation failed.", "%1 is the name of the search engine").arg(plugin_name));
  }
}
