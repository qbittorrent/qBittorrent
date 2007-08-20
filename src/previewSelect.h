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
#include "qtorrenthandle.h"

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
    QTorrentHandle h;

  signals:
    void readyToPreviewFile(QString) const;

  protected slots:
    void on_previewButton_clicked(){
      QModelIndex index;
      bool found = false;
      QModelIndexList selectedIndexes = previewList->selectionModel()->selectedIndexes();
      foreach(index, selectedIndexes){
        if(index.column() == NAME){
          QString root_path = h.save_path();
          if(root_path.at(root_path.length()-1) != QDir::separator()){
            root_path += QString::fromUtf8("/");
          }
          // Get the file name
          QString fileName = index.data().toString();
          // File
          if(QFile::exists(root_path+fileName)){
            emit readyToPreviewFile(root_path+fileName);
            found = true;
          }else{
            // Folder
            QString folder_name = h.name();
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
    previewSelect(QWidget* parent, QTorrentHandle h): QDialog(parent){
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
      supported_preview_extensions << QString::fromUtf8("AVI") << QString::fromUtf8("DIVX") << QString::fromUtf8("MPG") << QString::fromUtf8("MPEG") << QString::fromUtf8("MPE") << QString::fromUtf8("MP3") << QString::fromUtf8("OGG") << QString::fromUtf8("WMV") << QString::fromUtf8("WMA") << QString::fromUtf8("RMV") << QString::fromUtf8("RMVB") << QString::fromUtf8("ASF") << QString::fromUtf8("MOV") << QString::fromUtf8("WAV") << QString::fromUtf8("MP2") << QString::fromUtf8("SWF") << QString::fromUtf8("AC3") << QString::fromUtf8("OGM") << QString::fromUtf8("MP4") << QString::fromUtf8("FLV") << QString::fromUtf8("VOB") << QString::fromUtf8("QT") << QString::fromUtf8("MKV") << QString::fromUtf8("AIF") << QString::fromUtf8("AIFF") << QString::fromUtf8("AIFC") << QString::fromUtf8("MID") << QString::fromUtf8("MPG") << QString::fromUtf8("RA") << QString::fromUtf8("RAM") << QString::fromUtf8("AU") << QString::fromUtf8("M4A") << QString::fromUtf8("FLAC") << QString::fromUtf8("M4P") << QString::fromUtf8("3GP") << QString::fromUtf8("AAC") << QString::fromUtf8("RM") << QString::fromUtf8("SWA") << QString::fromUtf8("MPC") << QString::fromUtf8("MPP");
      previewList->header()->resizeSection(0, 200);
      // Fill list in
      this->h = h;
      std::vector<float> fp;
      h.file_progress(fp);
      unsigned int nbFiles = h.num_files();
      for(unsigned int i=0; i<nbFiles; ++i){
        QString fileName = h.file_at(i);
        QString extension = fileName.split(QString::fromUtf8(".")).last().toUpper();
        if(supported_preview_extensions.indexOf(extension) >= 0){
          int row = previewListModel->rowCount();
          previewListModel->insertRow(row);
          previewListModel->setData(previewListModel->index(row, NAME), QVariant(fileName));
          previewListModel->setData(previewListModel->index(row, SIZE), QVariant((qlonglong)h.filesize_at(i)));
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
      connect(this, SIGNAL(readyToPreviewFile(QString)), parent, SLOT(previewFile(QString)));
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
