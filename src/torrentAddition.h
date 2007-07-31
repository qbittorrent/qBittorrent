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

#ifndef TORRENTADDITION_H
#define TORRENTADDITION_H

#include <QDir>
#include <QFileDialog>
#include <QFile>
#include <fstream>
#include <QMessageBox>
#include <QMenu>
#include <QSettings>
#include <QStandardItemModel>
#include <QHeaderView>

#include <libtorrent/session.hpp>
#include <libtorrent/bencode.hpp>
#include "misc.h"
#include "PropListDelegate.h"
#include "ui_addTorrentDialog.h"

using namespace libtorrent;

class torrentAdditionDialog : public QDialog, private Ui_addTorrentDialog{
  Q_OBJECT

  signals:
    void setInfoBarGUI(QString info, QString color);
    void torrentAddition(QString filePath, bool fromScanDir, bool onStartup, QString from_url);

  private:
    QString fileName;
    QString fileHash;
    QString filePath;
    bool fromScanDir;
    QString from_url;
    QStandardItemModel *PropListModel;
    PropListDelegate *PropDelegate;

  public:
    torrentAdditionDialog(QWidget *parent) : QDialog(parent) {
      setupUi(this);
      setAttribute(Qt::WA_DeleteOnClose);
      // Set Properties list model
      PropListModel = new QStandardItemModel(0,4);
      PropListModel->setHeaderData(NAME, Qt::Horizontal, tr("File name"));
      PropListModel->setHeaderData(SIZE, Qt::Horizontal, tr("Size"));
      PropListModel->setHeaderData(PROGRESS, Qt::Horizontal, tr("Progress"));
      PropListModel->setHeaderData(PRIORITY, Qt::Horizontal, tr("Priority"));
      torrentContentList->setModel(PropListModel);
      torrentContentList->hideColumn(PROGRESS);
      PropDelegate = new PropListDelegate();
      torrentContentList->setItemDelegate(PropDelegate);
      connect(torrentContentList, SIGNAL(clicked(const QModelIndex&)), torrentContentList, SLOT(edit(const QModelIndex&)));
      connect(torrentContentList, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayFilesListMenu(const QPoint&)));
      connect(actionIgnored, SIGNAL(triggered()), this, SLOT(ignoreSelection()));
      connect(actionNormal, SIGNAL(triggered()), this, SLOT(normalSelection()));
      connect(actionHigh, SIGNAL(triggered()), this, SLOT(highSelection()));
      connect(actionMaximum, SIGNAL(triggered()), this, SLOT(maximumSelection()));
      torrentContentList->header()->resizeSection(0, 200);
      QString home = QDir::homePath();
      if(home[home.length()-1] != QDir::separator()){
        home += QDir::separator();
      }
      QSettings settings("qBittorrent", "qBittorrent");
      savePathTxt->setText(settings.value("LastDirTorrentAdd", home+"qBT_dir").toString());
    }

    void showLoad(QString filePath, bool fromScanDir=false, QString from_url=QString::null){
      this->filePath = filePath;
      this->fromScanDir = fromScanDir;
      this->from_url = from_url;
      std::ifstream in((const char*)filePath.toUtf8(), std::ios_base::binary);
      in.unsetf(std::ios_base::skipws);
      try{
        // Decode torrent file
        entry e = bdecode(std::istream_iterator<char>(in), std::istream_iterator<char>());
        // Getting torrent file informations
        torrent_info t(e);
        // Setting file name
        fileName = QString(t.name().c_str());
        fileHash = QString(misc::toString(t.info_hash()).c_str());
        // Use left() to remove .old extension
        QString newFileName;
        if(fileName.endsWith(".old")){
          newFileName = fileName.left(fileName.size()-4);
        }else{
          newFileName = fileName;
        }
        fileNameLbl->setText("<center><b>"+newFileName+"</b></center>");
        // List files in torrent
        unsigned int nbFiles = t.num_files();
        for(unsigned int i=0; i<nbFiles; ++i){
          unsigned int row = PropListModel->rowCount();
          PropListModel->insertRow(row);
          PropListModel->setData(PropListModel->index(row, NAME), QVariant(t.file_at(i).path.leaf().c_str()));
          PropListModel->setData(PropListModel->index(row, SIZE), QVariant((qlonglong)t.file_at(i).size));
          PropListModel->setData(PropListModel->index(i, PRIORITY), QVariant(NORMAL));
          setRowColor(i, "green");
        }
      }catch (invalid_torrent_file&){ // Raised by torrent_info constructor
        // Display warning to tell user we can't decode the torrent file
        if(!from_url.isNull()){
          emit setInfoBarGUI(tr("Unable to decode torrent file:")+" '"+from_url+"'", "red");
        }else{
          emit setInfoBarGUI(tr("Unable to decode torrent file:")+" '"+filePath+"'", "red");
        }
        emit setInfoBarGUI(tr("This file is either corrupted or this isn't a torrent."), "red");
        if(fromScanDir){
          // Remove .corrupt file in case it already exists
          QFile::remove(filePath+".corrupt");
          //Rename file extension so that it won't display error message more than once
          QFile::rename(filePath,filePath+".corrupt");
        }
        close();
      }
      catch(invalid_encoding& e){
        std::cerr << "Could not decode file, reason: " << e.what() << '\n';
        // Display warning to tell user we can't decode the torrent file
        if(!from_url.isNull()){
          emit setInfoBarGUI(tr("Unable to decode torrent file:")+" '"+from_url+"'", "red");
        }else{
          emit setInfoBarGUI(tr("Unable to decode torrent file:")+" '"+filePath+"'", "red");
        }
        emit setInfoBarGUI(tr("This file is either corrupted or this isn't a torrent."), "red");
        if(fromScanDir){
          // Remove .corrupt file in case it already exists
          QFile::remove(filePath+".corrupt");
          //Rename file extension so that it won't display error message more than once
          QFile::rename(filePath,filePath+".corrupt");
        }
        close();
      }
      show();
    }

  public slots:
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

    // Set the color of a row in data model
    void setRowColor(int row, QString color){
      for(int i=0; i<PropListModel->columnCount(); ++i){
        PropListModel->setData(PropListModel->index(row, i), QVariant(QColor(color)), Qt::ForegroundRole);
      }
    }

    void displayFilesListMenu(const QPoint& pos){
      QMenu myFilesLlistMenu(this);
      QModelIndex index;
  // Enable/disable pause/start action given the DL state
      QModelIndexList selectedIndexes = torrentContentList->selectionModel()->selectedIndexes();
      myFilesLlistMenu.setTitle(tr("Priority"));
      myFilesLlistMenu.addAction(actionIgnored);
      myFilesLlistMenu.addAction(actionNormal);
      myFilesLlistMenu.addAction(actionHigh);
      myFilesLlistMenu.addAction(actionMaximum);
  // Call menu
  // XXX: why mapToGlobal() is not enough?
      myFilesLlistMenu.exec(mapToGlobal(pos)+QPoint(10,145));
    }

    void ignoreSelection(){
      QModelIndexList selectedIndexes = torrentContentList->selectionModel()->selectedIndexes();
      QModelIndex index;
      foreach(index, selectedIndexes){
        if(index.column() == PRIORITY){
          PropListModel->setData(index, QVariant(IGNORED));
        }
        for(int i=0; i<PropListModel->columnCount(); ++i){
          PropListModel->setData(PropListModel->index(index.row(), i), QVariant(QColor("red")), Qt::ForegroundRole);
        }
      }
    }

    void normalSelection(){
      QModelIndexList selectedIndexes = torrentContentList->selectionModel()->selectedIndexes();
      QModelIndex index;
      foreach(index, selectedIndexes){
        if(index.column() == PRIORITY){
          PropListModel->setData(index, QVariant(NORMAL));
        }
        for(int i=0; i<PropListModel->columnCount(); ++i){
          PropListModel->setData(PropListModel->index(index.row(), i), QVariant(QColor("green")), Qt::ForegroundRole);
        }
      }
    }

    void highSelection(){
      QModelIndexList selectedIndexes = torrentContentList->selectionModel()->selectedIndexes();
      QModelIndex index;
      foreach(index, selectedIndexes){
        if(index.column() == PRIORITY){
          PropListModel->setData(index, QVariant(HIGH));
        }
        for(int i=0; i<PropListModel->columnCount(); ++i){
          PropListModel->setData(PropListModel->index(index.row(), i), QVariant(QColor("green")), Qt::ForegroundRole);
        }
      }
    }

    void maximumSelection(){
      QModelIndexList selectedIndexes = torrentContentList->selectionModel()->selectedIndexes();
      QModelIndex index;
      foreach(index, selectedIndexes){
        if(index.column() == PRIORITY){
          PropListModel->setData(index, QVariant(MAXIMUM));
        }
        for(int i=0; i<PropListModel->columnCount(); ++i){
          PropListModel->setData(PropListModel->index(index.row(), i), QVariant(QColor("green")), Qt::ForegroundRole);
        }
      }
    }

    void savePiecesPriorities(){
      qDebug("Saving pieces priorities");
      QFile pieces_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+fileHash+".priorities");
      // First, remove old file
      pieces_file.remove();
      // Write new files
      if(!pieces_file.open(QIODevice::WriteOnly | QIODevice::Text)){
        std::cerr << "Error: Could not save pieces priorities\n";
        return;
      }
      unsigned int nbRows = PropListModel->rowCount();
      for(unsigned int i=0; i<nbRows; ++i){
        QStandardItem *item = PropListModel->item(i, PRIORITY);
        unsigned short priority = item->text().toInt();
        pieces_file.write(QByteArray((misc::toString(priority)+"\n").c_str()));
      }
      pieces_file.close();
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
      QFile savepath_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+fileHash+".savepath");
      savepath_file.open(QIODevice::WriteOnly | QIODevice::Text);
      savepath_file.write(savePath.path().toUtf8());
      savepath_file.close();
      // Save last dir to remember it
      QSettings settings("qBittorrent", "qBittorrent");
      settings.setValue("LastDirTorrentAdd", savePathTxt->text());
      // Create .incremental file if necessary
      if(checkIncrementalDL->isChecked()){
        QFile incremental_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+fileHash+".incremental");
        incremental_file.open(QIODevice::WriteOnly | QIODevice::Text);
        incremental_file.close();
      }else{
        QFile::remove(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+fileHash+".incremental");
      }
      // Create .paused file if necessary
      if(addInPause->isChecked()){
        QFile paused_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+fileHash+".paused");
        paused_file.open(QIODevice::WriteOnly | QIODevice::Text);
        paused_file.close();
      }else{
        QFile::remove(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+fileHash+".paused");
      }
      // Check if there is at least one selected file
      if(!hasSelectedFiles()){
          QMessageBox::critical(0, tr("Invalid file selection"), tr("You must select at least one file in the torrent"));
          return;
      }
      // save filtered files
      savePiecesPriorities();
      // Add to download list
	// TODO : quick fix
      emit torrentAddition(filePath, fromScanDir, false, from_url);
      close();
    }

    bool hasSelectedFiles(){
      unsigned int nbRows = PropListModel->rowCount();
      for(unsigned int i=0; i<nbRows; ++i){
        QStandardItem *item = PropListModel->item(i, PRIORITY);
        unsigned short priority = item->text().toInt();
        if(priority) {
          return true;
        }
      }
      return false;
    }
};

#endif
