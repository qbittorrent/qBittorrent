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

#include <libtorrent/session.hpp>
#include <libtorrent/bencode.hpp>
#include "misc.h"
#include "ui_addTorrentDialog.h"

#define NAME 0
#define SIZE 1
#define SELECTED 2

using namespace libtorrent;

class torrentAdditionDialog : public QDialog, private Ui_addTorrentDialog{
  Q_OBJECT

  private:
    QString fileName;
    QString filePath;
    QList<bool> selection;
    bool fromScanDir;
    QString from_url;

  public:
    // Check if the torrent in already in download list before calling this dialog
    torrentAdditionDialog(QWidget *parent, QString filePath, bool fromScanDir=false, QString from_url=QString::null) : QDialog(parent), filePath(filePath), fromScanDir(fromScanDir), from_url(from_url){
      setupUi(this);
      setAttribute(Qt::WA_DeleteOnClose);
      //TODO: Remember last entered path
      QString home = QDir::homePath();
      if(home[home.length()-1] != QDir::separator()){
        home += QDir::separator();
      }
      savePathTxt->setText(home+"qBT_dir");
      std::ifstream in(filePath.toStdString().c_str(), std::ios_base::binary);
      in.unsetf(std::ios_base::skipws);
      try{
        // Decode torrent file
        entry e = bdecode(std::istream_iterator<char>(in), std::istream_iterator<char>());
        // Getting torrent file informations
        torrent_info t(e);
        // Setting file name
        fileName = QString(t.name().c_str());
        fileNameLbl->setText("<center><b>"+fileName+"</b></center>");
        // List files in torrent
        for(int i=0; i<t.num_files(); ++i){
          QStringList line;
          line << QString(t.file_at(i).path.leaf().c_str());
          line << misc::friendlyUnit((qlonglong)t.file_at(i).size);
          selection << true;
          line << tr("True");
          torrentContentList->addTopLevelItem(new QTreeWidgetItem(line));
          setLineColor(torrentContentList->topLevelItemCount()-1, "green");
        }
      }catch (invalid_torrent_file&){ // Raised by torrent_info constructor
        // Display warning to tell user we can't decode the torrent file
        if(!from_url.isNull()){
          emit setInfoBar(tr("Unable to decode torrent file:")+" '"+from_url+"'", "red");
        }else{
          emit setInfoBar(tr("Unable to decode torrent file:")+" '"+filePath+"'", "red");
        }
        emit setInfoBar(tr("This file is either corrupted or this isn't a torrent."),"red");
        if(fromScanDir){
          //Rename file extension so that it won't display error message more than once
          QFile::rename(filePath,filePath+".corrupt");
        }
        close();
      }
      //Connects
      connect(torrentContentList, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(toggleSelection(QTreeWidgetItem*, int)));
      show();
    }

  public slots:
    void on_browseButton_clicked(){
      QString dir = QFileDialog::getExistingDirectory(this, tr("Choose save path"), QDir::homePath());
      if(!dir.isNull()){
        savePathTxt->setText(dir);
      }
    }

    void on_CancelButton_clicked(){
      close();
    }

    void toggleSelection(QTreeWidgetItem *item, int){
      int row = torrentContentList->indexOfTopLevelItem(item);
      if(row == -1){
        return;
      }
      if(selection.at(row)){
        setLineColor(row, "red");
        item->setText(SELECTED, tr("False"));
        selection.replace(row, false);
      }else{
        setLineColor(row, "green");
        item->setText(SELECTED, tr("True"));
        selection.replace(row, true);
      }
    }

    void saveFilteredFiles(){
      QFile pieces_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+fileName+".pieces");
      // First, remove old file
      pieces_file.remove();
      // Write new files
      if(!pieces_file.open(QIODevice::WriteOnly | QIODevice::Text)){
        std::cerr << "Error: Could not save filtered pieces\n";
        return;
      }
      for(int i=0; i<torrentContentList->topLevelItemCount(); ++i){
        if(selection.at(i)){
          pieces_file.write(QByteArray("0\n"));
        }else{
          pieces_file.write(QByteArray("1\n"));
        }
      }
      pieces_file.close();
    }

    void setLineColor(int row, QString color){
      QTreeWidgetItem *item = torrentContentList->topLevelItem(row);
      for(int i=0; i<item->columnCount(); ++i){
        item->setData(i, Qt::ForegroundRole, QVariant(QColor(color)));
      }
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
      QFile savepath_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+fileName+".savepath");
      savepath_file.open(QIODevice::WriteOnly | QIODevice::Text);
      savepath_file.write(QByteArray(savePath.path().toStdString().c_str()));
      savepath_file.close();
      // Create .incremental file if necessary
      if(checkIncrementalDL->isChecked()){
        QFile incremental_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+fileName+".incremental");
        incremental_file.open(QIODevice::WriteOnly | QIODevice::Text);
        incremental_file.close();
      }else{
        QFile::remove(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+fileName+".incremental");
      }
      // Create .paused file if necessary
      if(addInPause->isChecked()){
        QFile paused_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+fileName+".paused");
        paused_file.open(QIODevice::WriteOnly | QIODevice::Text);
        paused_file.close();
      }else{
        QFile::remove(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+fileName+".paused");
      }
      // Check if there is at least one selected file
      bool selected_file = false;
      for(int i=0; i<torrentContentList->topLevelItemCount(); ++i){
        if(selection.at(i)){
          selected_file = true;
          break;
        }
      }
      if(!selected_file){
          QMessageBox::critical(0, tr("Invalid file selection"), tr("You must select at least one file in the torrent"));
          return;
      }
      // save filtered files
      saveFilteredFiles();
      // Add to download list
      emit torrentAddition(filePath, fromScanDir, from_url);
      close();
    }

  signals:
    void setInfoBar(QString message, QString color);
    void torrentAddition(const QString& filePath, bool fromScanDir, const QString& from_url);
};

#endif
