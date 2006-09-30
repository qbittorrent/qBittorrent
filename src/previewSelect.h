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

#ifndef PREVIEWSELECT_H
#define PREVIEWSELECT_H

#include <QDialog>
#include <QStandardItemModel>
#include <QHeaderView>
#include <QMessageBox>
#include <libtorrent/session.hpp>
#include "ui_preview.h"
#include "PreviewListDelegate.h"
#include "misc.h"

#define NAME 0
#define SIZE 1
#define PROGRESS 2

using namespace libtorrent;

class previewSelect: public QDialog, private Ui::preview {
  Q_OBJECT

  private:
    QStandardItemModel *previewListModel;
    PreviewListDelegate *listDelegate;
    QStringList supported_preview_extensions;
    torrent_handle h;

  signals:
    void readyToPreviewFile(const QString&) const;

  protected slots:
    void on_previewButton_clicked(){
      QModelIndex index;
      bool found = false;
      QModelIndexList selectedIndexes = previewList->selectionModel()->selectedIndexes();
      foreach(index, selectedIndexes){
        if(index.column() == NAME){
          QString root_path = QString(h.save_path().string().c_str());
          if(root_path.at(root_path.length()-1) != QDir::separator()){
            root_path += '/';
          }
          // Get the file name
          QString fileName = index.data().toString();
          // File
          if(QFile::exists(root_path+fileName)){
            emit readyToPreviewFile(root_path+fileName);
            found = true;
          }else{
            // Folder
            QString folder_name = QString(h.get_torrent_info().name().c_str());
            // Will find the file even if it is in a sub directory
            QString result = misc::findFileInDir(root_path+folder_name, fileName);
            if(!result.isNull()){
              emit readyToPreviewFile(result);
              found = true;
            }
          }
          break;
        }
      }
      if(!found){
        QMessageBox::critical(0, tr("Preview impossible"), tr("Sorry, we can't preview this file"));
      }
      close();
    }

    void on_cancelButton_clicked(){
      close();
    }

  public:
    previewSelect(QWidget* parent, torrent_handle h): QDialog(parent){
      setupUi(this);
      setAttribute(Qt::WA_DeleteOnClose);
      // Preview list
      previewListModel = new QStandardItemModel(0, 3);
      previewListModel->setHeaderData(NAME, Qt::Horizontal, tr("Name"));
      previewListModel->setHeaderData(SIZE, Qt::Horizontal, tr("Size"));
      previewListModel->setHeaderData(PROGRESS, Qt::Horizontal, tr("Progress"));
      previewList->setModel(previewListModel);
      listDelegate = new PreviewListDelegate(this);
      previewList->setItemDelegate(listDelegate);
      supported_preview_extensions<<"AVI"<<"DIVX"<<"MPG"<<"MPEG"<<"MP3"<<"OGG"<<"WMV"<<"WMA"<<"RMV"<<"RMVB"<<"ASF"<<"MOV"<<"WAV"<<"MP2"<<"SWF"<<"AC3";
      previewList->header()->resizeSection(0, 200);
      // Fill list in
      this->h = h;
      torrent_info torrentInfo = h.get_torrent_info();
      std::vector<float> fp;
      h.file_progress(fp);
      for(int i=0; i<torrentInfo.num_files(); ++i){
        QString fileName = QString(torrentInfo.file_at(i).path.leaf().c_str());
        QString extension = fileName.split('.').last().toUpper();
        if(supported_preview_extensions.indexOf(extension) >= 0){
          int row = previewListModel->rowCount();
          previewListModel->insertRow(row);
          previewListModel->setData(previewListModel->index(row, NAME), QVariant(fileName));
          previewListModel->setData(previewListModel->index(row, SIZE), QVariant((qlonglong)torrentInfo.file_at(i).size));
          previewListModel->setData(previewListModel->index(row, PROGRESS), QVariant((double)fp[i]));
        }
      }
      previewList->selectionModel()->select(previewListModel->index(0, NAME), QItemSelectionModel::Select);
      previewList->selectionModel()->select(previewListModel->index(0, SIZE), QItemSelectionModel::Select);
      previewList->selectionModel()->select(previewListModel->index(0, PROGRESS), QItemSelectionModel::Select);
      if(!previewListModel->rowCount()){
        QMessageBox::critical(0, tr("Preview impossible"), tr("Sorry, we can't preview this file"));
        close();
      }
      connect(this, SIGNAL(readyToPreviewFile(const QString&)), parent, SLOT(previewFile(const QString&)));
      if(previewListModel->rowCount() == 1){
        // Only one file : no choice
        on_previewButton_clicked();
      }else{
        show();
      }
    }

    ~previewSelect(){
      delete previewListModel;
      delete listDelegate;
    }
};

#endif
