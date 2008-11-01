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
    QTorrentHandle h;
    QList<int> indexes;

  signals:
    void readyToPreviewFile(QString) const;

  protected slots:
    void on_previewButton_clicked(){
      QModelIndex index;
      QModelIndexList selectedIndexes = previewList->selectionModel()->selectedIndexes();
      if(selectedIndexes.size() == 0) return;
      QString path;
      foreach(index, selectedIndexes){
        if(index.column() == NAME){
          path = h.files_path().at(indexes.at(index.row()));
          // File
          if(QFile::exists(path)){
            emit readyToPreviewFile(path);
          }
          close();
          return;
        }
      }
      qDebug("Cannot find file: %s", path.toUtf8().data());
      QMessageBox::critical(0, tr("Preview impossible"), tr("Sorry, we can't preview this file"));
      close();
    }

    void on_cancelButton_clicked(){
      close();
    }

  public:
    previewSelect(QWidget* parent, QTorrentHandle h): QDialog(parent), h(h){
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
      previewList->header()->resizeSection(0, 200);
      // Fill list in
      std::vector<size_type> fp;
      h.file_progress(fp);
      unsigned int nbFiles = h.num_files();
      for(unsigned int i=0; i<nbFiles; ++i){
        QString fileName = h.file_at(i);
        QString extension = fileName.split(QString::fromUtf8(".")).last().toUpper();
        if(misc::isPreviewable(extension)) {
          int row = previewListModel->rowCount();
          previewListModel->insertRow(row);
          previewListModel->setData(previewListModel->index(row, NAME), QVariant(fileName));
          previewListModel->setData(previewListModel->index(row, SIZE), QVariant((qlonglong)h.filesize_at(i)));
          previewListModel->setData(previewListModel->index(row, PROGRESS), QVariant((double)fp[i]/h.filesize_at(i)));
          indexes << i;
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
