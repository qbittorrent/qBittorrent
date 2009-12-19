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

#include <libtorrent/session.hpp>
#include <libtorrent/bencode.hpp>
#include "bittorrent.h"
#include "misc.h"
#include "proplistdelegate.h"
#include "ui_torrentadditiondlg.h"
#include "torrentpersistentdata.h"
#include "torrentfilesmodel.h"

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
    connect(collapseAllButton, SIGNAL(clicked()), torrentContentList, SLOT(collapseAll()));
    connect(expandAllButton, SIGNAL(clicked()), torrentContentList, SLOT(expandAll()));
    // Remember columns width
    readSettings();
    //torrentContentList->header()->setResizeMode(0, QHeaderView::Stretch);
    QString home = QDir::homePath();
    if(home[home.length()-1] != QDir::separator()){
      home += QDir::separator();
    }
    QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
    savePathTxt->setText(settings.value(QString::fromUtf8("LastDirTorrentAdd"), home+QString::fromUtf8("qBT_dir")).toString());
    if(settings.value("Preferences/Downloads/StartInPause", false).toBool()) {
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
    // Show the dialog
    show();
  }

public slots:

  void updateDiskSpaceLabels() {
    long long available = misc::freeDiskSpaceOnPath(savePathTxt->text());
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
    QDir saveDir(savePathTxt->text());
    if(saveDir.exists()){
      dir = QFileDialog::getExistingDirectory(this, tr("Choose save path"), savePathTxt->text());
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
    QDir savePath(savePathTxt->text());
    if(savePathTxt->text().trimmed().isEmpty()){
      QMessageBox::critical(0, tr("Empty save path"), tr("Please enter a save path"));
      return;
    }
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
    // Save last dir to remember it
    QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
    settings.setValue(QString::fromUtf8("LastDirTorrentAdd"), savePathTxt->text());
    // Create .incremental file if necessary
    TorrentTempData::setSequential(hash, checkIncrementalDL->isChecked());
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
