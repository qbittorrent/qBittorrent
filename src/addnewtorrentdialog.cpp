/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2012  Christophe Dumez
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

#include "addnewtorrentdialog.h"
#include "ui_addnewtorrentdialog.h"
#include "proplistdelegate.h"
#include "torrentcontentmodel.h"
#include "torrentcontentfiltermodel.h"
#include "preferences.h"
#include "qinisettings.h"
#include "torrentpersistentdata.h"
#include "qbtsession.h"
#include "iconprovider.h"
#include "fs_utils.h"

#include <QString>
#include <QFile>
#include <QInputDialog>
#include <QUrl>
#include <QMenu>
#include <QMessageBox>
#include <QTimer>
#include <QFileDialog>
#include <libtorrent/version.hpp>

using namespace libtorrent;

AddNewTorrentDialog::AddNewTorrentDialog(QWidget *parent) :
  QDialog(parent),
  ui(new Ui::AddNewTorrentDialog),
  m_contentModel(0),
  m_contentDelegate(0),
  m_isMagnet(false),
  m_hasRenamedFile(false)
{
  ui->setupUi(this);

  QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  Preferences pref;
  ui->start_torrent_cb->setChecked(!pref.addTorrentsInPause());
  ui->save_path_combo->addItem(fsutils::toDisplayPath(pref.getSavePath()));
  loadSavePathHistory();
  ui->save_path_combo->insertSeparator(ui->save_path_combo->count());
  ui->save_path_combo->addItem(tr("Other...", "Other save path..."));
  connect(ui->save_path_combo, SIGNAL(currentIndexChanged(int)), SLOT(onSavePathChanged(int)));
  ui->default_save_path_cb->setVisible(false); // Default path is selected by default

  // Load labels
  const QStringList customLabels = settings.value("TransferListFilters/customLabels", QStringList()).toStringList();
  ui->label_combo->addItem("");
  foreach (const QString& label, customLabels) {
    ui->label_combo->addItem(label);
  }
  loadState();
  // Signal / slots
  connect(ui->adv_button, SIGNAL(clicked(bool)), SLOT(showAdvancedSettings(bool)));
}

AddNewTorrentDialog::~AddNewTorrentDialog()
{
  saveState();
  delete ui;
  if (m_contentModel)
    delete m_contentModel;
}

void AddNewTorrentDialog::loadState()
{
  QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  settings.beginGroup(QString::fromUtf8("AddNewTorrentDialog"));
  QByteArray state = settings.value("treeHeaderState").toByteArray();
  if (!state.isEmpty())
    ui->content_tree->header()->restoreState(state);
  int width = settings.value("width", -1).toInt();
  if (width >= 0) {
    QRect geo = geometry();
    geo.setWidth(width);
    setGeometry(geo);
  }
  ui->adv_button->setChecked(settings.value("expanded", false).toBool());
}

void AddNewTorrentDialog::saveState()
{
  QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  settings.beginGroup(QString::fromUtf8("AddNewTorrentDialog"));
  if (m_contentModel)
    settings.setValue("treeHeaderState", ui->content_tree->header()->saveState());
  settings.setValue("y", pos().y());
  settings.setValue("width", width());
  settings.setValue("expanded", ui->adv_button->isChecked());
}

void AddNewTorrentDialog::showTorrent(const QString &torrent_path, const QString& from_url)
{
  AddNewTorrentDialog dlg;
  if (dlg.loadTorrent(torrent_path, from_url))
    dlg.exec();
}

void AddNewTorrentDialog::showMagnet(const QString& link)
{
  AddNewTorrentDialog dlg;
  if (dlg.loadMagnet(link))
    dlg.exec();
}

void AddNewTorrentDialog::showAdvancedSettings(bool show)
{
  if (show) {
    ui->adv_button->setText(QString::fromUtf8("▲"));
    ui->settings_group->setVisible(true);
    ui->info_group->setVisible(!m_isMagnet);
    if (!m_isMagnet && (m_torrentInfo->num_files() > 1)) {
      ui->content_tree->setVisible(true);
      setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    } else {
      ui->content_tree->setVisible(false);
      setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }
    static_cast<QVBoxLayout*>(layout())->insertWidget(layout()->indexOf(ui->never_show_cb)+1, ui->adv_button);
  } else {
    ui->adv_button->setText(QString::fromUtf8("▼"));
    ui->settings_group->setVisible(false);
    ui->info_group->setVisible(false);
    ui->buttonsHLayout->insertWidget(0, layout()->takeAt(layout()->indexOf(ui->never_show_cb)+1)->widget());
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  }
  relayout();
}

bool AddNewTorrentDialog::loadTorrent(const QString& torrent_path, const QString& from_url)
{
  m_isMagnet = false;
  m_url = from_url;
  if (torrent_path.startsWith("file://", Qt::CaseInsensitive))
    m_filePath = QUrl::fromEncoded(torrent_path.toLocal8Bit()).toLocalFile();
  else
    m_filePath = torrent_path;

  if (!QFile::exists(m_filePath)) {
    QMessageBox::critical(0, tr("I/O Error"), tr("The torrent file does not exist."));
    return false;
  }

  try {
    m_torrentInfo = new torrent_info(m_filePath.toUtf8().data());
    m_hash = misc::toQString(m_torrentInfo->info_hash());
  } catch(const std::exception& e) {
    QMessageBox::critical(0, tr("Invalid torrent"), tr("Failed to load the torrent: %1").arg(e.what()));
    return false;
  }

  // Set dialog title
  setWindowTitle(misc::toQStringU(m_torrentInfo->name()));

  // Set torrent information
  QString comment = misc::toQString(m_torrentInfo->comment());
  ui->comment_lbl->setText(comment.replace('\n', ' '));
  ui->date_lbl->setText(m_torrentInfo->creation_date() ? misc::toQString(*m_torrentInfo->creation_date()) : tr("Not available"));
  updateDiskSpaceLabel();

#if LIBTORRENT_VERSION_MINOR >= 16
  file_storage fs = m_torrentInfo->files();
#endif

  // Populate m_filesList
  for (int i = 0; i < m_torrentInfo->num_files(); ++i) {
#if LIBTORRENT_VERSION_MINOR >= 16
    m_filesPath << misc::toQStringU(fs.file_path(m_torrentInfo->file_at(i)));
#else
    m_filesPath << misc::toQStringU(m_torrentInfo->file_at(i).path.string());
#endif
  }

  // Prepare content tree
  if (m_torrentInfo->num_files() > 1) {
    m_contentModel = new TorrentContentFilterModel(this);
    connect(m_contentModel->model(), SIGNAL(filteredFilesChanged()), SLOT(updateDiskSpaceLabel()));
    ui->content_tree->setModel(m_contentModel);
    ui->content_tree->hideColumn(PROGRESS);
    m_contentDelegate = new PropListDelegate();
    ui->content_tree->setItemDelegate(m_contentDelegate);
    connect(ui->content_tree, SIGNAL(clicked(const QModelIndex&)), ui->content_tree, SLOT(edit(const QModelIndex&)));
    connect(ui->content_tree, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayContentTreeMenu(const QPoint&)));

    // List files in torrent
    m_contentModel->model()->setupModelData(*m_torrentInfo);

    // Expand root folder
    ui->content_tree->setExpanded(m_contentModel->index(0, 0), true);
    ui->content_tree->header()->setResizeMode(0, QHeaderView::Stretch);
  } else {
    // Update save paths (append file name to them)
#if LIBTORRENT_VERSION_MINOR >= 16
    QString single_file_relpath = misc::toQStringU(fs.file_path(m_torrentInfo->file_at(0)));
#else
    QString single_file_relpath = misc::toQStringU(m_torrentInfo->file_at(0).path.string());
#endif
    for (int i=0; i<ui->save_path_combo->count()-1; ++i) {
      ui->save_path_combo->setItemText(i, QDir(ui->save_path_combo->itemText(i)).absoluteFilePath(single_file_relpath));
    }
  }

  QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  showAdvancedSettings(settings.value("AddNewTorrentDialog/expanded").toBool());
  // Set dialog position
  setdialogPosition();

  return true;
}

bool AddNewTorrentDialog::loadMagnet(const QString &magnet_uri)
{
  m_isMagnet = true;
  m_url = magnet_uri;
  m_hash = misc::magnetUriToHash(m_url);
  if (m_hash.isEmpty()) {
    QMessageBox::critical(0, tr("Invalid magnet link"), tr("This magnet link was not recognized"));
    return false;
  }

  // Set dialog title
  QString torrent_name = misc::magnetUriToName(m_url);
  setWindowTitle(torrent_name.isEmpty() ? tr("Magnet link") : torrent_name);

  QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  showAdvancedSettings(settings.value("AddNewTorrentDialog/expanded").toBool());
  // Set dialog position
  setdialogPosition();

  return true;
}

void AddNewTorrentDialog::saveSavePathHistory() const
{
  QDir selected_save_path(ui->save_path_combo->itemData(ui->save_path_combo->currentIndex()).toString());
  QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  // Get current history
  QStringList history = settings.value("TorrentAdditionDlg/save_path_history").toStringList();
  QList<QDir> history_dirs;
  foreach(const QString dir, history)
    history_dirs << QDir(dir);
  if (!history_dirs.contains(selected_save_path)) {
    // Add save path to history
    history << selected_save_path.absolutePath();
    // Limit list size
    if (history.size() > 8)
      history.removeFirst();
    // Save history
    settings.setValue("TorrentAdditionDlg/save_path_history", history);
  }
}

// save_path is a folder, not an absolute file path
int AddNewTorrentDialog::indexOfSavePath(const QString &save_path)
{
  QDir saveDir(save_path);
  for(int i=0; i<ui->save_path_combo->count()-1; ++i) {
    if (QDir(ui->save_path_combo->itemData(i).toString()) == saveDir)
      return i;
  }
  return -1;
}

void AddNewTorrentDialog::updateFileNameInSavePaths(const QString &new_filename)
{
  for(int i=0; i<ui->save_path_combo->count()-1; ++i) {
    const QDir folder(ui->save_path_combo->itemData(i).toString());
    ui->save_path_combo->setItemText(i, fsutils::toDisplayPath(folder.absoluteFilePath(new_filename)));
  }
}

void AddNewTorrentDialog::updateDiskSpaceLabel()
{
  Q_ASSERT(!m_isMagnet);
  // Determine torrent size
  qulonglong torrent_size = 0;
  if (m_contentModel) {
    const std::vector<int> priorities = m_contentModel->model()->getFilesPriorities();
    Q_ASSERT(priorities.size() == (uint) m_torrentInfo->num_files());
    for (uint i=0; i<priorities.size(); ++i) {
      if (priorities[i] > 0)
        torrent_size += m_torrentInfo->file_at(i).size;
    }
  } else {
    torrent_size = m_torrentInfo->total_size();
  }
  QString size_string = misc::friendlyUnit(torrent_size);
  size_string += " (";
  size_string += tr("Disk space: %1").arg(misc::friendlyUnit(fsutils::freeDiskSpaceOnPath(ui->save_path_combo->currentText())));
  size_string += ")";
  ui->size_lbl->setText(size_string);
}

void AddNewTorrentDialog::onSavePathChanged(int index)
{
  static int old_index = 0;
  static QDir defaultSaveDir(ui->save_path_combo->itemData(0).toString());

  if (index == (ui->save_path_combo->count() - 1)) {
    disconnect(ui->save_path_combo, SIGNAL(currentIndexChanged(int)), this, SLOT(onSavePathChanged(int)));
    // User is asking for a new save path
    QString cur_save_path = ui->save_path_combo->itemText(old_index);
    QString new_path, old_filename, new_filename;

    if (m_torrentInfo && m_torrentInfo->num_files() == 1) {
      old_filename = fsutils::fileName(cur_save_path);
      new_path = QFileDialog::getSaveFileName(this, tr("Choose save path"), cur_save_path, QString(), 0, QFileDialog::DontConfirmOverwrite);
      if (!new_path.isEmpty())
        new_path = fsutils::branchPath(new_path, &new_filename);
      qDebug() << "new_path: " << new_path;
      qDebug() << "new_filename: " << new_filename;
    } else {
      if (!cur_save_path.isEmpty() && QDir(cur_save_path).exists())
        new_path = QFileDialog::getExistingDirectory(this, tr("Choose save path"), cur_save_path);
      else
        new_path = QFileDialog::getExistingDirectory(this, tr("Choose save path"), QDir::homePath());
    }
    if (!new_path.isEmpty()) {
      const int existing_index = indexOfSavePath(new_path);
      if (existing_index >= 0)
        ui->save_path_combo->setCurrentIndex(existing_index);
      else {
        // New path, prepend to combo box
        if (!new_filename.isEmpty())
          ui->save_path_combo->insertItem(0, fsutils::toDisplayPath(QDir(new_path).absoluteFilePath(new_filename)), new_path);
        else
          ui->save_path_combo->insertItem(0, fsutils::toDisplayPath(new_path), new_path);
        ui->save_path_combo->setCurrentIndex(0);
      }
      // Update file name in all save_paths
      if (!new_filename.isEmpty() && !fsutils::sameFileNames(old_filename, new_filename)) {
        m_hasRenamedFile = true;
        m_filesPath[0] = new_filename;
        updateFileNameInSavePaths(new_filename);
      }
    } else {
      // Restore index
      ui->save_path_combo->setCurrentIndex(old_index);
    }
    connect(ui->save_path_combo, SIGNAL(currentIndexChanged(int)), SLOT(onSavePathChanged(int)));
  }
  // Toggle default save path setting checkbox visibility
  ui->default_save_path_cb->setChecked(false);
  ui->default_save_path_cb->setVisible(QDir(ui->save_path_combo->itemData(ui->save_path_combo->currentIndex()).toString()) != defaultSaveDir);
  relayout();
  // Remember index
  old_index = ui->save_path_combo->currentIndex();
}

void AddNewTorrentDialog::relayout()
{
  qApp->processEvents();
  int min_width = minimumWidth();
  setMinimumWidth(width());
  adjustSize();
  setMinimumWidth(min_width);
}

void AddNewTorrentDialog::renameSelectedFile()
{
  const QModelIndexList selectedIndexes = ui->content_tree->selectionModel()->selectedRows(0);
  Q_ASSERT(selectedIndexes.size() == 1);
  const QModelIndex &index = selectedIndexes.first();
  // Ask for new name
  bool ok;
  const QString new_name_last = QInputDialog::getText(this, tr("Rename the file"),
                                                      tr("New name:"), QLineEdit::Normal,
                                                      index.data().toString(), &ok);
  if (ok && !new_name_last.isEmpty()) {
    if (!fsutils::isValidFileSystemName(new_name_last)) {
      QMessageBox::warning(this, tr("The file could not be renamed"),
                           tr("This file name contains forbidden characters, please choose a different one."),
                           QMessageBox::Ok);
      return;
    }
    if (m_contentModel->itemType(index) == TorrentContentModelItem::FileType) {
      // File renaming
      const int file_index = m_contentModel->getFileIndex(index);
      QString old_name = m_filesPath.at(file_index);
      old_name.replace("\\", "/");
      qDebug("Old name: %s", qPrintable(old_name));
      QStringList path_items = old_name.split("/");
      path_items.removeLast();
      path_items << new_name_last;
      QString new_name = path_items.join("/");
      if (fsutils::sameFileNames(old_name, new_name)) {
        qDebug("Name did not change");
        return;
      }
      new_name = QDir::cleanPath(new_name);
      qDebug("New name: %s", qPrintable(new_name));
      // Check if that name is already used
      for (int i=0; i<m_torrentInfo->num_files(); ++i) {
        if (i == file_index) continue;
        if (fsutils::sameFileNames(m_filesPath.at(i), new_name)) {
          // Display error message
          QMessageBox::warning(this, tr("The file could not be renamed"),
                               tr("This name is already in use in this folder. Please use a different name."),
                               QMessageBox::Ok);
          return;
        }
      }
      new_name = QDir::cleanPath(new_name);
      qDebug("Renaming %s to %s", qPrintable(old_name), qPrintable(new_name));
      // Rename file in files_path
      m_filesPath.replace(file_index, new_name);
      m_hasRenamedFile = true;
      // Rename in torrent files model too
      m_contentModel->setData(index, new_name_last);
    } else {
      // Folder renaming
      QStringList path_items;
      path_items << index.data().toString();
      QModelIndex parent = m_contentModel->parent(index);
      while(parent.isValid()) {
        path_items.prepend(parent.data().toString());
        parent = m_contentModel->parent(parent);
      }
      const QString old_path = path_items.join("/");
      path_items.removeLast();
      path_items << new_name_last;
      QString new_path = path_items.join("/");
      if (!new_path.endsWith("/")) new_path += "/";
      // Check for overwriting
      for (int i=0; i<m_torrentInfo->num_files(); ++i) {
        const QString &current_name = m_filesPath.at(i);
#if defined(Q_WS_X11) || defined(Q_WS_MAC) || defined(Q_WS_QWS)
        if (current_name.startsWith(new_path, Qt::CaseSensitive)) {
#else
        if (current_name.startsWith(new_path, Qt::CaseInsensitive)) {
#endif
          QMessageBox::warning(this, tr("The folder could not be renamed"),
                               tr("This name is already in use in this folder. Please use a different name."),
                               QMessageBox::Ok);
          return;
        }
      }
      // Replace path in all files
      for (int i=0; i<m_torrentInfo->num_files(); ++i) {
        const QString &current_name = m_filesPath.at(i);
        if (current_name.startsWith(old_path)) {
          QString new_name = current_name;
          new_name.replace(0, old_path.length(), new_path);
          new_name = QDir::cleanPath(new_name);
          qDebug("Rename %s to %s", qPrintable(current_name), qPrintable(new_name));
          // Rename in files_path
          m_filesPath.replace(i, new_name);
        }
      }
      m_hasRenamedFile = true;
      // Rename folder in torrent files model too
      m_contentModel->setData(index, new_name_last);
    }
  }
}

void AddNewTorrentDialog::setdialogPosition()
{
  qApp->processEvents();
  QPoint center(misc::screenCenter(this));
  // Adjust y
  QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  int y = settings.value("AddNewTorrentDialog/y", -1).toInt();
  if (y >= 0) {
    center.setY(y);
  } else {
    center.ry() -= 120;
    if (center.y() < 0)
      center.setY(0);
  }
  move(center);
}

void AddNewTorrentDialog::loadSavePathHistory()
{
  QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  QDir default_save_path(Preferences().getSavePath());
  // Load save path history
  QStringList raw_path_history = settings.value("TorrentAdditionDlg/save_path_history").toStringList();
  foreach (const QString &sp, raw_path_history) {
    if (QDir(sp) != default_save_path)
      ui->save_path_combo->addItem(fsutils::toDisplayPath(sp), sp);
  }
}

void AddNewTorrentDialog::displayContentTreeMenu(const QPoint&) {
  QMenu myFilesLlistMenu;
  const QModelIndexList selectedRows = ui->content_tree->selectionModel()->selectedRows(0);
  QAction *actRename = 0;
  if (selectedRows.size() == 1 && m_torrentInfo->num_files() > 1) {
    actRename = myFilesLlistMenu.addAction(IconProvider::instance()->getIcon("edit-rename"), tr("Rename..."));
    myFilesLlistMenu.addSeparator();
  }
  QMenu subMenu;
  subMenu.setTitle(tr("Priority"));
  subMenu.addAction(ui->actionNot_downloaded);
  subMenu.addAction(ui->actionNormal);
  subMenu.addAction(ui->actionHigh);
  subMenu.addAction(ui->actionMaximum);
  myFilesLlistMenu.addMenu(&subMenu);
  // Call menu
  QAction *act = myFilesLlistMenu.exec(QCursor::pos());
  if (act) {
    if (act == actRename) {
      renameSelectedFile();
    } else {
      int prio = prio::NORMAL;
      if (act == ui->actionHigh)
        prio = prio::HIGH;
      else {
        if (act == ui->actionMaximum) {
          prio = prio::MAXIMUM;
        } else {
          if (act == ui->actionNot_downloaded)
            prio = prio::IGNORED;
        }
      }
      qDebug("Setting files priority");
      foreach (const QModelIndex &index, selectedRows) {
        qDebug("Setting priority(%d) for file at row %d", prio, index.row());
        m_contentModel->setData(m_contentModel->index(index.row(), PRIORITY, index.parent()), prio);
      }
    }
  }
}

void AddNewTorrentDialog::on_buttonBox_accepted()
{
  Preferences pref;
  // Save Temporary data about torrent
  QString save_path = ui->save_path_combo->itemData(ui->save_path_combo->currentIndex()).toString();
  TorrentTempData::setSavePath(m_hash, save_path);
  if (ui->skip_check_cb->isChecked()) {
    // TODO: Check if destination actually exists
    TorrentTempData::setSeedingMode(m_hash, true);
  }

  // Label
  const QString label = ui->label_combo->currentText();
  if (!label.isEmpty())
    TorrentTempData::setLabel(m_hash, label);

  // Save file priorities
  if (m_contentModel)
    TorrentTempData::setFilesPriority(m_hash, m_contentModel->model()->getFilesPriorities());

  // Rename files if necessary
  if (m_hasRenamedFile)
    TorrentTempData::setFilesPath(m_hash, m_filesPath);

  // Temporary override of addInPause setting
  bool old_addInPause = pref.addTorrentsInPause();
  pref.addTorrentsInPause(!ui->start_torrent_cb->isChecked());
  pref.sync();

  // Add torrent
  if (m_isMagnet)
    QBtSession::instance()->addMagnetUri(m_url, false);
  else
    QBtSession::instance()->addTorrent(m_filePath, false, m_url);

  // Restore addInPause setting
  pref.addTorrentsInPause(old_addInPause);

  saveSavePathHistory();
  // Save settings
  pref.useAdditionDialog(!ui->never_show_cb->isChecked());
  if (ui->default_save_path_cb->isChecked())
    pref.setSavePath(ui->save_path_combo->itemData(ui->save_path_combo->currentIndex()).toString());
}
