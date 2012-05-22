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

#include <QDir>
#include <QFileDialog>
#include <QFile>
#include <fstream>
#include <QMessageBox>
#include <QMenu>
#include <QHeaderView>
#include <QApplication>
#include <QDesktopWidget>
#include <QInputDialog>
#include <QDebug>

#include <libtorrent/version.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/bencode.hpp>

#include "qbtsession.h"
#include "torrentfilesmodel.h"
#include "preferences.h"
#include "transferlistwidget.h"
#include "qinisettings.h"
#include "misc.h"
#include "proplistdelegate.h"
#include "torrentpersistentdata.h"
#include "iconprovider.h"
#include "torrentadditiondlg.h"
#include "lineedit.h"

using namespace libtorrent;

torrentAdditionDialog::torrentAdditionDialog(QWidget *parent) :
  QDialog(parent), old_label(""), hidden_height(0), m_showContentList(true) {
  Preferences pref;
  setupUi(this);
  setAttribute(Qt::WA_DeleteOnClose);
  // Icons
  CancelButton->setIcon(IconProvider::instance()->getIcon("dialog-cancel"));
  OkButton->setIcon(IconProvider::instance()->getIcon("list-add"));
  // Set Properties list model
  PropListModel = new TorrentFilesFilterModel(this);
  connect(PropListModel, SIGNAL(filteredFilesChanged()), SLOT(updateDiskSpaceLabels()));
  torrentContentList->setModel(PropListModel);
  torrentContentList->hideColumn(PROGRESS);
  PropDelegate = new PropListDelegate();
  torrentContentList->setItemDelegate(PropDelegate);
  connect(torrentContentList, SIGNAL(clicked(const QModelIndex&)), torrentContentList, SLOT(edit(const QModelIndex&)));
  connect(torrentContentList, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayContentListMenu(const QPoint&)));
  connect(selectAllButton, SIGNAL(clicked()), PropListModel, SLOT(selectAll()));
  connect(selectNoneButton, SIGNAL(clicked()), PropListModel, SLOT(selectNone()));
  connect(comboLabel, SIGNAL(editTextChanged(QString)), this, SLOT(resetComboLabelIndex(QString)));
  connect(comboLabel, SIGNAL(editTextChanged(QString)), this, SLOT(updateLabelInSavePath(QString)));
  connect(comboLabel, SIGNAL(currentIndexChanged(QString)), this, SLOT(updateLabelInSavePath(QString)));
  LineEdit *contentFilterLine = new LineEdit(this);
  connect(contentFilterLine, SIGNAL(textChanged(QString)), PropListModel, SLOT(setFilterFixedString(QString)));
  contentFilterLayout->insertWidget(1, contentFilterLine);
  // Important: as a default, it inserts at the bottom which is not desirable
  savePathTxt->setInsertPolicy(QComboBox::InsertAtCurrent);
  //torrentContentList->header()->setResizeMode(0, QHeaderView::Stretch);
  defaultSavePath = pref.getSavePath();
  //In case of the LastLocationPath doesn't exist anymore, empty the string

  appendLabelToSavePath = pref.appendTorrentLabel();
  QString display_path = defaultSavePath.replace("\\", "/");
  if(!display_path.endsWith("/"))
    display_path += "/";
  path_history << display_path;
#if defined(Q_WS_WIN) || defined(Q_OS_OS2)
  display_path.replace("/", "\\");
#endif
  savePathTxt->addItem(display_path);

  if(pref.addTorrentsInPause()) {
    addInPause->setChecked(true);
    //addInPause->setEnabled(false);
  }
  // Load custom labels
  QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  settings.beginGroup(QString::fromUtf8("TransferListFilters"));
  const QStringList customLabels = settings.value("customLabels", QStringList()).toStringList();
  comboLabel->addItem("");
  foreach(const QString& label, customLabels) {
    comboLabel->addItem(label);
  }

#if LIBTORRENT_VERSION_MINOR < 15
  addInSeed->setVisible(false);
#endif
  // Set Add button as default
  OkButton->setDefault(true);
}

torrentAdditionDialog::~torrentAdditionDialog() {
  delete PropDelegate;
  delete PropListModel;
}

void torrentAdditionDialog::closeEvent(QCloseEvent *event)
{
  qDebug() << Q_FUNC_INFO;
  saveSettings();
  QDialog::closeEvent(event);
}

void torrentAdditionDialog::readSettings() {
  // The window should NOT be shown before calling this method
  Q_ASSERT(!isVisible());
  QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  QString mode = m_showContentList ? "Full" : "Short";
  qDebug() << Q_FUNC_INFO << "mode:" << mode;
  if (!restoreGeometry(settings.value("TorrentAdditionDlg/geometry" + mode).toByteArray())) {
    qDebug() << Q_FUNC_INFO << "Failed to load last known dialog geometry";
    if (!m_showContentList) {
      qDebug() << Q_FUNC_INFO << "Short mode: adapting default size";
      resize(width(), height() - 220); // Default size is for full mode
    }
  }
  if (m_showContentList) {
    if(!torrentContentList->header()->restoreState(settings.value("TorrentAdditionDlg/ContentHeaderState").toByteArray())) {
      qDebug() << Q_FUNC_INFO << "First executation, resize first section to 400px...";
      torrentContentList->header()->resizeSection(0, 400); // Default
    }
  }
}

void torrentAdditionDialog::saveSettings() {
  QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  QString mode = m_showContentList ? "Full" : "Short";
  settings.setValue("TorrentAdditionDlg/geometry" + mode, saveGeometry());
  if (m_showContentList) {
    settings.setValue("TorrentAdditionDlg/ContentHeaderState", torrentContentList->header()->saveState());
  }
}

void torrentAdditionDialog::limitDialogWidth() {
  int scrn = 0;
  const QWidget *w = this->window();

  if(w)
    scrn = QApplication::desktop()->screenNumber(w);
  else if(QApplication::desktop()->isVirtualDesktop())
    scrn = QApplication::desktop()->screenNumber(QCursor::pos());
  else
    scrn = QApplication::desktop()->screenNumber(this);

  QRect desk(QApplication::desktop()->availableGeometry(scrn));
  int max_width = desk.width();
  if(max_width > 0)
    setMaximumWidth(max_width);
}

void torrentAdditionDialog::hideTorrentContent() {
  // Disable useless widgets
  torrentContentList->setVisible(false);
  //torrentContentLbl->setVisible(false);
  for(int i=0; i<selectionBtnsLayout->count(); ++i) {
    if(selectionBtnsLayout->itemAt(i)->widget())
      selectionBtnsLayout->itemAt(i)->widget()->setVisible(false);
  }
  for(int i=0; i<contentFilterLayout->count(); ++i) {
    if(contentFilterLayout->itemAt(i)->widget())
      contentFilterLayout->itemAt(i)->widget()->setVisible(false);
  }
  contentFilterLayout->update();

  m_showContentList = false;
}

void torrentAdditionDialog::showLoadMagnetURI(QString magnet_uri) {
  is_magnet = true;
  this->from_url = magnet_uri;
  checkLastFolder->setEnabled(false);

  // Disable Save path combox and browse button
  // Save path should be default for magnet links
  savePathTxt->setEnabled(false);
  browseButton->setEnabled(false);

  // Get torrent hash
  hash = misc::magnetUriToHash(magnet_uri);
  if(hash.isEmpty()) {
    QBtSession::instance()->addConsoleMessage(tr("Unable to decode magnet link:")+QString::fromUtf8(" '")+from_url+QString::fromUtf8("'"), QString::fromUtf8("red"));
    return;
  }
  // Set torrent name
  fileName = misc::magnetUriToName(magnet_uri);
  if(fileName.isEmpty()) {
    fileName = tr("Magnet Link");
  }
  fileNameLbl->setText(QString::fromUtf8("<center><b>")+fileName+QString::fromUtf8("</b></center>"));
  // Update display
  updateDiskSpaceLabels();

  // No need to display torrent content
  hideTorrentContent();

  // Remember dialog geometry
  readSettings();

  // Limit dialog width
  limitDialogWidth();

  // Display dialog
  show();
}

void torrentAdditionDialog::showLoad(QString filePath, QString from_url) {
  is_magnet = false;

  // This is an URL to a local file, switch to local path
  if(filePath.startsWith("file:", Qt::CaseInsensitive))
    filePath = QUrl::fromEncoded(filePath.toLocal8Bit()).toLocalFile();

  if(!QFile::exists(filePath)) {
    close();
    return;
  }

  qDebug() << Q_FUNC_INFO << filePath;

  this->filePath = filePath;
  this->from_url = from_url;
  // Getting torrent file informations
  try {
    t = new torrent_info(filePath.toUtf8().data());
    if(!t->is_valid())
      throw std::exception();
  } catch(std::exception&) {
    qDebug("Caught error loading torrent");
    if(!from_url.isNull()){
      QBtSession::instance()->addConsoleMessage(tr("Unable to decode torrent file:")+QString::fromUtf8(" '")+from_url+QString::fromUtf8("'"), QString::fromUtf8("red"));
      misc::safeRemove(filePath);
    }else{
      QBtSession::instance()->addConsoleMessage(tr("Unable to decode torrent file:")+QString::fromUtf8(" '")+filePath+QString::fromUtf8("'"), QString::fromUtf8("red"));
    }
    close();
    return;
  }
  nbFiles = t->num_files();
  if(nbFiles == 0) {
    // Empty torrent file!?
    close();
    return;
  }
#if LIBTORRENT_VERSION_MINOR >= 16
  file_storage fs = t->files();
#endif
  // Truncate root folder
  QString root_folder = misc::truncateRootFolder(t);
  // Setting file name
  fileName = misc::toQStringU(t->name());
  hash = misc::toQString(t->info_hash());
  // Use left() to remove .old extension
  QString newFileName;
  if(fileName.endsWith(QString::fromUtf8(".old"))){
    newFileName = fileName.left(fileName.size()-4);
  }else{
    newFileName = fileName;
  }
  fileNameLbl->setText(QString::fromUtf8("<center><b>")+newFileName+QString::fromUtf8("</b></center>"));
  if(t->num_files() > 1) {
    // List files in torrent
    PropListModel->model()->setupModelData(*t);
    connect(PropDelegate, SIGNAL(filteredFilesChanged()), this, SLOT(updateDiskSpaceLabels()));
    // Loads files path in the torrent
    for(uint i=0; i<nbFiles; ++i) {
#if LIBTORRENT_VERSION_MINOR >= 16
      files_path << misc::toQStringU(fs.file_path(t->file_at(i)));
#else
      files_path << misc::toQStringU(t->file_at(i).path.string());
#endif
    }

  }

  // Load save path history
  loadSavePathHistory();

  // Connect slots
  connect(savePathTxt->lineEdit(), SIGNAL(editingFinished()), this, SLOT(updateDiskSpaceLabels()));
  connect(savePathTxt->lineEdit(), SIGNAL(editingFinished()), this, SLOT(updateSavePathCurrentText()));

  QString save_path = savePathTxt->currentText();
#if defined(Q_WS_WIN) || defined(Q_OS_OS2)
  save_path.replace("/", "\\");
#endif
  if(!save_path.endsWith(QDir::separator()))
    save_path += QDir::separator();
  // If the torrent has a root folder, append it to the save path
  if(!root_folder.isEmpty()) {
    save_path += root_folder;
  }
  if(nbFiles == 1) {
    // single file torrent
#if LIBTORRENT_VERSION_MINOR >= 16
    QString single_file_relpath = misc::toQStringU(fs.file_path(t->file_at(0)));
#else
    QString single_file_relpath = misc::toQStringU(t->file_at(0).path.string());
#endif
#if defined(Q_WS_WIN) || defined(Q_OS_OS2)
    single_file_relpath.replace("/", "\\");
#endif
    save_path += single_file_relpath;
  }
  savePathTxt->setEditText(save_path);

  // Update size labels
  updateDiskSpaceLabels();

  // Hide useless widgets
  if(t->num_files() <= 1)
    hideTorrentContent();

  // Remember dialog geometry
  readSettings();

  // Limit dialog width
  limitDialogWidth();

  // Show the dialog
  show();
}

void torrentAdditionDialog::displayContentListMenu(const QPoint&) {
  Q_ASSERT(!is_magnet && t->num_files() > 1);
  QMenu myFilesLlistMenu;
  const QModelIndexList selectedRows = torrentContentList->selectionModel()->selectedRows(0);
  QAction *actRename = 0;
  if(selectedRows.size() == 1 && t->num_files() > 1) {
    actRename = myFilesLlistMenu.addAction(IconProvider::instance()->getIcon("edit-rename"), tr("Rename..."));
    myFilesLlistMenu.addSeparator();
  }
  QMenu subMenu;
  subMenu.setTitle(tr("Priority"));
  subMenu.addAction(actionNot_downloaded);
  subMenu.addAction(actionNormal);
  subMenu.addAction(actionHigh);
  subMenu.addAction(actionMaximum);
  myFilesLlistMenu.addMenu(&subMenu);
  // Call menu
  QAction *act = myFilesLlistMenu.exec(QCursor::pos());
  if(act) {
    if(act == actRename) {
      renameSelectedFile();
    } else {
      int prio = 1;
      if(act == actionHigh) {
        prio = prio::HIGH;
      } else {
        if(act == actionMaximum) {
          prio = prio::MAXIMUM;
        } else {
          if(act == actionNot_downloaded) {
            prio = prio::IGNORED;
          }
        }
      }
      qDebug("Setting files priority");
      foreach(const QModelIndex &index, selectedRows) {
        qDebug("Setting priority(%d) for file at row %d", prio, index.row());
        PropListModel->setData(PropListModel->index(index.row(), PRIORITY, index.parent()), prio);
      }
    }
  }
}

void torrentAdditionDialog::renameSelectedFile() {
  Q_ASSERT(!is_magnet && t->num_files() > 1);
  const QModelIndexList selectedIndexes = torrentContentList->selectionModel()->selectedRows(0);
  Q_ASSERT(selectedIndexes.size() == 1);
  const QModelIndex &index = selectedIndexes.first();
  // Ask for new name
  bool ok;
  const QString new_name_last = QInputDialog::getText(this, tr("Rename the file"),
                                                      tr("New name:"), QLineEdit::Normal,
                                                      index.data().toString(), &ok);
  if (ok && !new_name_last.isEmpty()) {
    if(!misc::isValidFileSystemName(new_name_last)) {
      QMessageBox::warning(this, tr("The file could not be renamed"),
                           tr("This file name contains forbidden characters, please choose a different one."),
                           QMessageBox::Ok);
      return;
    }
    if(PropListModel->getType(index) == TorrentFileItem::TFILE) {
      // File renaming
      const uint file_index = PropListModel->getFileIndex(index);
      QString old_name = files_path.at(file_index);
      old_name.replace("\\", "/");
      qDebug("Old name: %s", qPrintable(old_name));
      QStringList path_items = old_name.split("/");
      path_items.removeLast();
      path_items << new_name_last;
      QString new_name = path_items.join("/");
      if(old_name == new_name) {
        qDebug("Name did not change");
        return;
      }
      new_name = QDir::cleanPath(new_name);
      qDebug("New name: %s", qPrintable(new_name));
      // Check if that name is already used
      for(uint i=0; i<nbFiles; ++i) {
        if(i == file_index) continue;
#if defined(Q_WS_X11) || defined(Q_WS_MAC) || defined(Q_WS_QWS)
        if(files_path.at(i).compare(new_name, Qt::CaseSensitive) == 0) {
#else
        if(files_path.at(i).compare(new_name, Qt::CaseInsensitive) == 0) {
#endif
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
      files_path.replace(file_index, new_name);
      // Rename in torrent files model too
      PropListModel->setData(index, new_name_last);
    } else {
      // Folder renaming
      QStringList path_items;
      path_items << index.data().toString();
      QModelIndex parent = PropListModel->parent(index);
      while(parent.isValid()) {
        path_items.prepend(parent.data().toString());
        parent = PropListModel->parent(parent);
      }
      const QString old_path = path_items.join("/");
      path_items.removeLast();
      path_items << new_name_last;
      QString new_path = path_items.join("/");
      if(!new_path.endsWith("/")) new_path += "/";
      // Check for overwriting
      for(uint i=0; i<nbFiles; ++i) {
        const QString &current_name = files_path.at(i);
#if defined(Q_WS_X11) || defined(Q_WS_MAC) || defined(Q_WS_QWS)
        if(current_name.startsWith(new_path, Qt::CaseSensitive)) {
#else
        if(current_name.startsWith(new_path, Qt::CaseInsensitive)) {
#endif
          QMessageBox::warning(this, tr("The folder could not be renamed"),
                               tr("This name is already in use in this folder. Please use a different name."),
                               QMessageBox::Ok);
          return;
        }
      }
      // Replace path in all files
      for(uint i=0; i<nbFiles; ++i) {
        const QString &current_name = files_path.at(i);
        if(current_name.startsWith(old_path)) {
          QString new_name = current_name;
          new_name.replace(0, old_path.length(), new_path);
          new_name = QDir::cleanPath(new_name);
          qDebug("Rename %s to %s", qPrintable(current_name), qPrintable(new_name));
          // Rename in files_path
          files_path.replace(i, new_name);
        }
      }
      // Rename folder in torrent files model too
      PropListModel->setData(index, new_name_last);
    }
  }
}

void torrentAdditionDialog::updateDiskSpaceLabels() {
  qDebug("Updating disk space label...");
  const long long available = misc::freeDiskSpaceOnPath(misc::expandPath(savePathTxt->currentText()));
  lbl_disk_space->setText(misc::friendlyUnit(available));
  if(!is_magnet) {
    // Determine torrent size
    qulonglong torrent_size = 0;
    if(t->num_files() > 1) {
      const std::vector<int> priorities = PropListModel->model()->getFilesPriorities();
      for(unsigned int i=0; i<priorities.size(); ++i) {
        if(priorities[i] > 0)
          torrent_size += t->file_at(i).size;
      }
    } else {
      torrent_size = t->total_size();
    }
    lbl_torrent_size->setText(misc::friendlyUnit(torrent_size));

    // Check if free space is sufficient
    if(available > 0) {
      if((unsigned long long)available > torrent_size) {
        // Space is sufficient
        label_space_msg->setText(tr("(%1 left after torrent download)", "e.g. (100MiB left after torrent download)").arg(misc::friendlyUnit(available-torrent_size)));
      } else {
        // Space is unsufficient
        label_space_msg->setText("<font color=\"red\">"+tr("(%1 more are required to download)", "e.g. (100MiB more are required to download)").arg(misc::friendlyUnit(torrent_size-available))+"</font>");
      }
    } else {
      // Available disk space is unknown
      label_space_msg->setText("");
    }
  }
}

void torrentAdditionDialog::on_browseButton_clicked(){
  Q_ASSERT(!is_magnet);
  QString new_path;
  QString root_folder;
  const QString label_name = comboLabel->currentText();
  if(t->num_files() == 1) {
    new_path = QFileDialog::getSaveFileName(this, tr("Choose save path"), savePathTxt->currentText(), QString(), 0, QFileDialog::DontConfirmOverwrite);
    if(!new_path.isEmpty()) {
      QStringList path_parts = new_path.replace("\\", "/").split("/");
      const QString filename = path_parts.takeLast();
      // Append label
      if(QDir(path_parts.join(QDir::separator())) == QDir(defaultSavePath) && !label_name.isEmpty())
        path_parts << label_name;
      // Append file name
      path_parts << filename;
      // Construct new_path
      new_path = path_parts.join(QDir::separator());
    }
  } else {
    QString truncated_path = getCurrentTruncatedSavePath(&root_folder);
    if(!truncated_path.isEmpty() && QDir(truncated_path).exists()){
      new_path = QFileDialog::getExistingDirectory(this, tr("Choose save path"), truncated_path);
    }else{
      new_path = QFileDialog::getExistingDirectory(this, tr("Choose save path"), QDir::homePath());
    }
    if(!new_path.isEmpty()) {
      QStringList path_parts = new_path.replace("\\", "/").split("/");
      if(path_parts.last().isEmpty())
        path_parts.removeLast();
      // Append label
      if(QDir(new_path) == QDir(defaultSavePath) && !label_name.isEmpty())
        path_parts << label_name;
      // Append root folder
      if(!root_folder.isEmpty())
        path_parts << root_folder;
      // Construct new_path
      new_path = path_parts.join(QDir::separator());
    }
  }
  if(!new_path.isEmpty()) {
    // Check if this new path already exists in the list
    QString new_truncated_path = getTruncatedSavePath(new_path);
#if defined(Q_WS_X11) || defined(Q_WS_MAC) || defined(Q_WS_QWS)
    const int cur_index = path_history.indexOf(QRegExp(new_truncated_path, Qt::CaseInsensitive));
#else
    const int cur_index = path_history.indexOf(QRegExp(new_truncated_path, Qt::CaseSensitive));
#endif
    if(cur_index >= 0) {
      savePathTxt->setCurrentIndex(cur_index);
    }
#if defined(Q_WS_WIN) || defined(Q_OS_OS2)
    new_path.replace("/", "\\");
#endif
    savePathTxt->setEditText(new_path);
  }
}

void torrentAdditionDialog::on_CancelButton_clicked(){
  close();
}

bool torrentAdditionDialog::allFiltered() const {
  Q_ASSERT(!is_magnet);
  return PropListModel->model()->allFiltered();
}

void torrentAdditionDialog::savePiecesPriorities(){
  qDebug("Saving pieces priorities");
  Q_ASSERT(!is_magnet);
  const std::vector<int> priorities = PropListModel->model()->getFilesPriorities();
  TorrentTempData::setFilesPriority(hash, priorities);
}

void torrentAdditionDialog::on_OkButton_clicked(){
  Preferences pref;
  qDebug() << "void torrentAdditionDialog::on_OkButton_clicked() - ENTER";
  if(savePathTxt->currentText().trimmed().isEmpty()){
    QMessageBox::critical(0, tr("Empty save path"), tr("Please enter a save path"));
    return;
  }
  QString save_path = savePathTxt->currentText();
#if defined(Q_WS_WIN) || defined(Q_OS_OS2)
  save_path.replace("\\", "/");
#endif
  save_path = misc::expandPath(save_path);
  qDebug("Save path is %s", qPrintable(save_path));
  if(!is_magnet && t->num_files() == 1) {
    // Remove file name
    QStringList parts = save_path.split("/");
    const QString single_file_name = parts.takeLast();
    Q_ASSERT(files_path.isEmpty());
    files_path << single_file_name;
    save_path = parts.join("/");
  }
  qDebug("Save path dir is %s", qPrintable(save_path));
  QDir savePath(save_path);
  const QString current_label = comboLabel->currentText().trimmed();
  if (!current_label.isEmpty() && !misc::isValidFileSystemName(current_label)) {
    QMessageBox::warning(this, tr("Invalid label name"), tr("Please don't use any special characters in the label name."));
    return;
  }
  // Save savepath
  qDebug("Saving save path to temp data: %s", qPrintable(savePath.absolutePath()));
  TorrentTempData::setSavePath(hash, savePath.absolutePath());
  qDebug("Torrent label is: %s", qPrintable(comboLabel->currentText().trimmed()));
  if(!current_label.isEmpty())
    TorrentTempData::setLabel(hash, current_label);
  // Is download sequential?
  TorrentTempData::setSequential(hash, checkIncrementalDL->isChecked());
  // Save files path
  // Loads files path in the torrent
  if(!is_magnet) {
    bool path_changed = false;
    for(uint i=0; i<nbFiles; ++i) {
#if LIBTORRENT_VERSION_MINOR >= 16
      file_storage fs = t->files();
      QString old_path = misc::toQStringU(fs.file_path(t->file_at(i)));
#else
      QString old_path = misc::toQStringU(t->file_at(i).path.string());
#endif
#if defined(Q_WS_X11) || defined(Q_WS_MAC) || defined(Q_WS_QWS)
      if(files_path.at(i).compare(old_path, Qt::CaseSensitive) != 0) {
#else
      if(files_path.at(i).compare(old_path, Qt::CaseInsensitive) != 0) {
#endif
        path_changed = true;
        break;
      }
    }
    if(path_changed) {
      qDebug("Changing files paths");
      TorrentTempData::setFilesPath(hash, files_path);
    }
  }
#if LIBTORRENT_VERSION_MINOR > 14
  // Skip file checking and directly start seeding
  if(addInSeed->isChecked()) {
    // Check if local file(s) actually exist
    if(is_magnet || QFile::exists(savePathTxt->currentText())) {
      TorrentTempData::setSeedingMode(hash, true);
    } else {
      QMessageBox::warning(0, tr("Seeding mode error"), tr("You chose to skip file checking. However, local files do not seem to exist in the current destionation folder. Please disable this feature or update the save path."));
      return;
    }
  }
#endif
  // Check if there is at least one selected file
  if(!is_magnet && t->num_files() > 1 && allFiltered()){
    QMessageBox::warning(0, tr("Invalid file selection"), tr("You must select at least one file in the torrent"));
    return;
  }
  // Save path history
  saveTruncatedPathHistory();

  // Set as default save path if necessary
  if (checkLastFolder->isChecked())
    pref.setSavePath(getCurrentTruncatedSavePath());

  // Check if savePath exists
  if(!savePath.exists()){
    if(!savePath.mkpath(savePath.path())){
      QMessageBox::critical(0, tr("Save path creation error"), tr("Could not create the save path"));
      return;
    }
  }

  // save filtered files
  if(!is_magnet && t->num_files() > 1)
    savePiecesPriorities();
  // Add to download list
  QTorrentHandle h;
  if(is_magnet)
    h = QBtSession::instance()->addMagnetUri(from_url, false);
  else
    h = QBtSession::instance()->addTorrent(filePath, false, from_url);
  if(addInPause->isChecked() && h.is_valid()) {
    h.pause();
  }
  // Close the dialog
  qDebug("Closing torrent addition dialog...");
  close();
  qDebug("Closed");
}

void torrentAdditionDialog::resetComboLabelIndex(QString text) {
  // Select first index
  if(text != comboLabel->itemText(comboLabel->currentIndex())) {
    comboLabel->setItemText(0, text);
    comboLabel->setCurrentIndex(0);
  }
}

void torrentAdditionDialog::updateLabelInSavePath(QString label) {
  if(appendLabelToSavePath) {
    // Update Label in combobox
    savePathTxt->setItemText(0, misc::updateLabelInSavePath(defaultSavePath, savePathTxt->itemText(0), old_label, label));
    // update edit text
    savePathTxt->setEditText(misc::updateLabelInSavePath(defaultSavePath, savePathTxt->currentText(), old_label, label));
    old_label = label;
  }
}

void torrentAdditionDialog::updateSavePathCurrentText() {
  qDebug("updateSavePathCurrentText() - ENTER");
  savePathTxt->setItemText(savePathTxt->currentIndex(), savePathTxt->currentText());
  qDebug("path_history.size() == %d", path_history.size());
  qDebug("savePathTxt->currentIndex() == %d", savePathTxt->currentIndex());
  path_history.replace(savePathTxt->currentIndex(), getCurrentTruncatedSavePath());
  QString root_folder_or_file_name = "";
  getCurrentTruncatedSavePath(&root_folder_or_file_name);
  // Update other combo items
  for(int i=0; i<savePathTxt->count(); ++i) {
    if(i == savePathTxt->currentIndex()) continue;
    QString item_path = path_history.at(i);
    if(item_path.isEmpty()) continue;
    // Append label
    if(i == 0 && appendLabelToSavePath && QDir(item_path) == QDir(defaultSavePath) && !comboLabel->currentText().isEmpty())
      item_path += comboLabel->currentText() + "/";
    // Append root_folder or filename
    if(!root_folder_or_file_name.isEmpty())
      item_path += root_folder_or_file_name;
#if defined(Q_WS_WIN) || defined(Q_OS_OS2)
    item_path.replace("/", "\\");
#endif
    savePathTxt->setItemText(i, item_path);
  }
}

QString torrentAdditionDialog::getCurrentTruncatedSavePath(QString* root_folder_or_file_name) const {
  QString save_path = savePathTxt->currentText();
  return getTruncatedSavePath(save_path, root_folder_or_file_name);
}

// Get current save path without the torrent root folder nor the label label
QString torrentAdditionDialog::getTruncatedSavePath(QString save_path, QString* root_folder_or_file_name) const {
  // Expand and clean path (~, .., .)
  save_path = misc::expandPath(save_path);
  QStringList parts = save_path.replace("\\", "/").split("/");
  // Remove torrent root folder
  if(!QDir(save_path).exists() || (!is_magnet && t->num_files() == 1)) {
    QString tmp = parts.takeLast();
    if(root_folder_or_file_name)
      *root_folder_or_file_name = tmp;
  }
  // Remove label
  if(appendLabelToSavePath && savePathTxt->currentIndex() == 0 && parts.last() == comboLabel->currentText()) {
    parts.removeLast();
  }
  QString truncated_path = parts.join("/");
  if(!truncated_path.endsWith("/"))
    truncated_path += "/";
  qDebug("Truncated save path: %s", qPrintable(truncated_path));
  return truncated_path;
}

void torrentAdditionDialog::saveTruncatedPathHistory() {
  QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  const QString current_save_path = getCurrentTruncatedSavePath();
  // Get current history
  QStringList history = settings.value("TorrentAdditionDlg/save_path_history").toStringList();
#if defined(Q_WS_X11) || defined(Q_WS_MAC) || defined(Q_WS_QWS)
  if(!history.contains(current_save_path, Qt::CaseSensitive)) {
#else
  if(!history.contains(current_save_path, Qt::CaseInsensitive)) {
#endif
    // Add save path to history
    history << current_save_path;
    // Limit list size
    if(history.size() > 8)
      history.removeFirst();
    // Save history
    settings.setValue("TorrentAdditionDlg/save_path_history", history);
  }
}

void torrentAdditionDialog::loadSavePathHistory() {
  QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  // Load save path history
  QStringList raw_path_history = settings.value("TorrentAdditionDlg/save_path_history").toStringList();
  foreach(const QString &sp, raw_path_history) {
    if(QDir(sp) != QDir(defaultSavePath)) {
      QString dsp = sp;
#if defined(Q_WS_WIN) || defined(Q_OS_OS2)
      dsp.replace("/", "\\");
#endif
      path_history << sp;
      savePathTxt->addItem(dsp);
    }
  }
}
