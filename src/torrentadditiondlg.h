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

#ifndef TORRENTADDITION_H
#define TORRENTADDITION_H

#include <QDir>
#include <QFileDialog>
#include <QFile>
#include <fstream>
#include <QMessageBox>
#include <QMenu>
#include <QSettings>
#include <QHeaderView>
#include <QApplication>
#include <QDesktopWidget>
#include <QInputDialog>

#include <libtorrent/session.hpp>
#include <libtorrent/bencode.hpp>
#include "bittorrent.h"
#include "misc.h"
#include "proplistdelegate.h"
#include "ui_torrentadditiondlg.h"
#include "torrentpersistentdata.h"
#include "torrentfilesmodel.h"
#include "preferences.h"

using namespace libtorrent;

class torrentAdditionDialog : public QDialog, private Ui_addTorrentDialog{
  Q_OBJECT

private:
  Bittorrent *BTSession;
  QString fileName;
  QString hash;
  QString filePath;
  QString from_url;
  TorrentFilesModel *PropListModel;
  PropListDelegate *PropDelegate;
  unsigned int nbFiles;
  boost::intrusive_ptr<torrent_info> t;
  QStringList files_path;

public:
  torrentAdditionDialog(QWidget *parent, Bittorrent* _BTSession) : QDialog(parent) {
    setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    BTSession = _BTSession;
    // Set Properties list model
    PropListModel = new TorrentFilesModel();
    torrentContentList->setModel(PropListModel);
    torrentContentList->hideColumn(PROGRESS);
    PropDelegate = new PropListDelegate();
    torrentContentList->setItemDelegate(PropDelegate);
    connect(torrentContentList, SIGNAL(clicked(const QModelIndex&)), torrentContentList, SLOT(edit(const QModelIndex&)));
    connect(torrentContentList, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayContentListMenu(const QPoint&)));
    connect(collapseAllButton, SIGNAL(clicked()), torrentContentList, SLOT(collapseAll()));
    connect(expandAllButton, SIGNAL(clicked()), torrentContentList, SLOT(expandAll()));
    // Remember columns width
    readSettings();
    //torrentContentList->header()->setResizeMode(0, QHeaderView::Stretch);
    savePathTxt->setText(Preferences::getSavePath());
    if(Preferences::addTorrentsInPause()) {
      addInPause->setChecked(true);
      addInPause->setEnabled(false);
    }
#ifndef LIBTORRENT_0_15
    addInSeed->setEnabled(false);
#endif
  }

  ~torrentAdditionDialog() {
    saveSettings();
    delete PropDelegate;
    delete PropListModel;
  }

  void readSettings() {
    QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
    // Restore size and position
    resize(settings.value(QString::fromUtf8("TorrentAdditionDlg/size"), size()).toSize());
    move(settings.value(QString::fromUtf8("TorrentAdditionDlg/pos"), screenCenter()).toPoint());
    // Restore column width
    QVariantList contentColsWidths = settings.value(QString::fromUtf8("TorrentAdditionDlg/filesColsWidth"), QVariantList()).toList();
    if(contentColsWidths.empty()) {
      torrentContentList->header()->resizeSection(0, 200);
    } else {
      for(int i=0; i<contentColsWidths.size(); ++i) {
        torrentContentList->setColumnWidth(i, contentColsWidths.at(i).toInt());
      }
    }
  }

  // Screen center point
  QPoint screenCenter() const{
    int scrn = 0;
    QWidget *w = this->topLevelWidget();

    if(w)
      scrn = QApplication::desktop()->screenNumber(w);
    else if(QApplication::desktop()->isVirtualDesktop())
      scrn = QApplication::desktop()->screenNumber(QCursor::pos());
    else
      scrn = QApplication::desktop()->screenNumber(this);

    QRect desk(QApplication::desktop()->availableGeometry(scrn));
    return QPoint((desk.width() - this->frameGeometry().width()) / 2, (desk.height() - this->frameGeometry().height()) / 2);
  }

  void saveSettings() {
    QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
    QVariantList contentColsWidths;
    // -1 because we hid PROGRESS column
    for(int i=0; i<PropListModel->columnCount()-1; ++i) {
      contentColsWidths.append(torrentContentList->columnWidth(i));
    }
    settings.setValue(QString::fromUtf8("TorrentAdditionDlg/filesColsWidth"), contentColsWidths);
    settings.setValue("TorrentAdditionDlg/size", size());
    settings.setValue("TorrentAdditionDlg/pos", pos());
  }

  void showLoad(QString filePath, QString from_url=QString::null){
    if(!QFile::exists(filePath)) {
      close();
      return;
    }
    this->filePath = filePath;
    this->from_url = from_url;
    // Getting torrent file informations
    try {
      t = new torrent_info(filePath.toLocal8Bit().data());
    } catch(std::exception&) {
      qDebug("Caught error loading torrent");
      if(!from_url.isNull()){
        BTSession->addConsoleMessage(tr("Unable to decode torrent file:")+QString::fromUtf8(" '")+from_url+QString::fromUtf8("'"), QString::fromUtf8("red"));
        QFile::remove(filePath);
      }else{
        BTSession->addConsoleMessage(tr("Unable to decode torrent file:")+QString::fromUtf8(" '")+filePath+QString::fromUtf8("'"), QString::fromUtf8("red"));
      }
      close();
      return;
    }
    nbFiles = t->num_files();
    // Setting file name
    fileName = misc::toQString(t->name());
    hash = misc::toQString(t->info_hash());
    // Use left() to remove .old extension
    QString newFileName;
    if(fileName.endsWith(QString::fromUtf8(".old"))){
      newFileName = fileName.left(fileName.size()-4);
    }else{
      newFileName = fileName;
    }
    fileNameLbl->setText(QString::fromUtf8("<center><b>")+newFileName+QString::fromUtf8("</b></center>"));
    // List files in torrent
    PropListModel->setupModelData(*t);
    // Expand first item if possible
    torrentContentList->expand(PropListModel->index(0, 0));
    connect(PropDelegate, SIGNAL(filteredFilesChanged()), this, SLOT(updateDiskSpaceLabels()));
    //torrentContentList->expandAll();
    connect(savePathTxt, SIGNAL(textChanged(QString)), this, SLOT(updateDiskSpaceLabels()));
    updateDiskSpaceLabels();
    // Load custom labels
    QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
    settings.beginGroup(QString::fromUtf8("TransferListFilters"));
    QStringList customLabels = settings.value("customLabels", QStringList()).toStringList();
    comboLabel->addItem("");
    foreach(const QString& label, customLabels) {
      comboLabel->addItem(label);
    }
    // Loads files path in the torrent
    for(uint i=0; i<nbFiles; ++i) {
      files_path << misc::toQString(t->file_at(i).path.string());
    }
    // Show the dialog
    show();
  }

public slots:

  void displayContentListMenu(const QPoint&) {
    QMenu myFilesLlistMenu;
    QModelIndexList selectedRows = torrentContentList->selectionModel()->selectedRows(0);
    QAction *actRename = 0;
    if(selectedRows.size() == 1) {
      actRename = myFilesLlistMenu.addAction(QIcon(QString::fromUtf8(":/Icons/oxygen/edit_clear.png")), tr("Rename..."));
      //myFilesLlistMenu.addSeparator();
    } else {
      return;
    }
    // Call menu
    QAction *act = myFilesLlistMenu.exec(QCursor::pos());
    if(act) {
      if(act == actRename) {
        renameSelectedFile();
      }
    }
  }

  void renameSelectedFile() {
    QModelIndexList selectedIndexes = torrentContentList->selectionModel()->selectedRows(0);
    Q_ASSERT(selectedIndexes.size() == 1);
    QModelIndex index = selectedIndexes.first();
    // Ask for new name
    bool ok;
    QString new_name_last = QInputDialog::getText(this, tr("Rename torrent file"),
                                                  tr("New name:"), QLineEdit::Normal,
                                                  index.data().toString(), &ok);
    if (ok && !new_name_last.isEmpty()) {
      if(PropListModel->getType(index)==TFILE) {
        // File renaming
        uint file_index = PropListModel->getFileIndex(index);
        QString old_name = files_path.at(file_index);
        QStringList path_items = old_name.split(QDir::separator());
        path_items.removeLast();
        path_items << new_name_last;
        QString new_name = path_items.join(QDir::separator());
        if(old_name == new_name) {
          qDebug("Name did not change");
          return;
        }
        // Check if that name is already used
        for(uint i=0; i<nbFiles; ++i) {
          if(i == file_index) continue;
#ifdef Q_WS_WIN
          if(files_path.at(i).compare(new_name, Qt::CaseInsensitive) == 0) {
#else
            if(files_path.at(i).compare(new_name, Qt::CaseSensitive) == 0) {
#endif
              // Display error message
              QMessageBox::warning(this, tr("The file could not be renamed"),
                                   tr("This name is already in use in this folder. Please use a different name."),
                                   QMessageBox::Ok);
              return;
            }
          }
          qDebug("Renaming %s to %s", old_name.toLocal8Bit().data(), new_name.toLocal8Bit().data());
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
          QString old_path = path_items.join(QDir::separator());
          path_items.removeLast();
          path_items << new_name_last;
          QString new_path = path_items.join(QDir::separator());
          // Check for overwriting
          for(uint i=0; i<nbFiles; ++i) {
            QString current_name = files_path.at(i);
#ifdef Q_WS_WIN
            if(current_name.contains(new_path, Qt::CaseInsensitive)) {
#else
              if(current_name.contains(new_path, Qt::CaseSensitive)) {
#endif
                QMessageBox::warning(this, tr("The folder could not be renamed"),
                                     tr("This name is already in use in this folder. Please use a different name."),
                                     QMessageBox::Ok);
                return;
              }
            }
            // Replace path in all files
            for(uint i=0; i<nbFiles; ++i) {
              QString current_name = files_path.at(i);
              QString new_name = current_name.replace(old_path, new_path);
              qDebug("Rename %s to %s", current_name.toLocal8Bit().data(), new_name.toLocal8Bit().data());
              // Rename in files_path
              files_path.replace(i, new_name);
            }
            // Rename folder in torrent files model too
            PropListModel->setData(index, new_name_last);
          }
        }
      }

      void updateDiskSpaceLabels() {
        long long available = misc::freeDiskSpaceOnPath(misc::expandPath(savePathTxt->text()));
        lbl_disk_space->setText(misc::friendlyUnit(available));

        // Determine torrent size
        qulonglong torrent_size = 0;
        unsigned int nbFiles = t->num_files();
        std::vector<int> priorities = PropListModel->getFilesPriorities(nbFiles);

        for(unsigned int i=0; i<nbFiles; ++i) {
          if(priorities[i] > 0)
            torrent_size += t->file_at(i).size;
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

      void on_browseButton_clicked(){
        QString dir;
        QString save_path = misc::expandPath(savePathTxt->text());
        QDir saveDir(save_path);
        if(!save_path.isEmpty() && saveDir.exists()){
          dir = QFileDialog::getExistingDirectory(this, tr("Choose save path"), saveDir.absolutePath());
        }else{
          dir = QFileDialog::getExistingDirectory(this, tr("Choose save path"), QDir::homePath());
        }
        if(!dir.isNull()){
          savePathTxt->setText(dir);
        }
      }

      void on_CancelButton_clicked(){
        close();
      }

      bool allFiltered() const {
        return PropListModel->allFiltered();
      }

      void savePiecesPriorities(){
        qDebug("Saving pieces priorities");
        std::vector<int> priorities = PropListModel->getFilesPriorities(t->num_files());
        TorrentTempData::setFilesPriority(hash, priorities);
      }

      void on_OkButton_clicked(){
        if(savePathTxt->text().trimmed().isEmpty()){
          QMessageBox::critical(0, tr("Empty save path"), tr("Please enter a save path"));
          return;
        }
        QDir savePath(misc::expandPath(savePathTxt->text()));
        // Check if savePath exists
        if(!savePath.exists()){
          if(!savePath.mkpath(savePath.path())){
            QMessageBox::critical(0, tr("Save path creation error"), tr("Could not create the save path"));
            return;
          }
        }
        // Save savepath
        TorrentTempData::setSavePath(hash, savePath.path());
        qDebug("Torrent label is: %s", comboLabel->currentText().trimmed().toLocal8Bit().data());
        TorrentTempData::setLabel(hash, comboLabel->currentText().trimmed());
        // Is download sequential?
        TorrentTempData::setSequential(hash, checkIncrementalDL->isChecked());
        // Save files path
        // Loads files path in the torrent
        bool path_changed = false;
        for(uint i=0; i<nbFiles; ++i) {
#ifdef Q_WS_WIN
          if(files_path.at(i).compare(misc::toQString(t->file_at(i).path.string()), Qt::CaseInsensitive) != 0) {
#else
            if(files_path.at(i).compare(misc::toQString(t->file_at(i).path.string()), Qt::CaseSensitive) != 0) {
#endif
              path_changed = true;
              break;
            }
          }
          if(path_changed) {
            TorrentTempData::setFilesPath(hash, files_path);
          }
#ifdef LIBTORRENT_0_15
          // Skip file checking and directly start seeding
          if(addInSeed->isChecked()) {
            // Check if local file(s) actually exist
            if(savePath.exists(misc::toQString(t->name()))) {
              TorrentTempData::setSeedingMode(hash, true);
            } else {
              QMessageBox::warning(0, tr("Seeding mode error"), tr("You chose to skip file checking. However, local files do not seem to exist in the current destionation folder. Please disable this feature or update the save path."));
              return;
            }
          }
#endif
          // Check if there is at least one selected file
          if(allFiltered()){
            QMessageBox::warning(0, tr("Invalid file selection"), tr("You must select at least one file in the torrent"));
            return;
          }
          // save filtered files
          savePiecesPriorities();
          // Add to download list
          QTorrentHandle h = BTSession->addTorrent(filePath, false, from_url);
          if(addInPause->isChecked() && h.is_valid())
            h.pause();
          close();
        }
      };

#endif
