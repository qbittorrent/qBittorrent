/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2011  Christophe Dumez
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

#include <QStandardItemModel>
#include <QHeaderView>
#include <QMessageBox>
#include <QFile>

#include <libtorrent/version.hpp>
#include <libtorrent/session.hpp>

#include "misc.h"
#include "previewlistdelegate.h"
#include "previewselect.h"
#include "fs_utils.h"
#include "preferences.h"

PreviewSelect::PreviewSelect(QWidget* parent, QTorrentHandle h): QDialog(parent), h(h) {
  setupUi(this);
  setAttribute(Qt::WA_DeleteOnClose);
  Preferences pref;
  // Preview list
  previewListModel = new QStandardItemModel(0, NB_COLUMNS);
  previewListModel->setHeaderData(NAME, Qt::Horizontal, tr("Name"));
  previewListModel->setHeaderData(SIZE, Qt::Horizontal, tr("Size"));
  previewListModel->setHeaderData(PROGRESS, Qt::Horizontal, tr("Progress"));
  previewList->setModel(previewListModel);
  previewList->hideColumn(FILE_INDEX);
  listDelegate = new PreviewListDelegate(this);
  previewList->setItemDelegate(listDelegate);
  previewList->header()->resizeSection(0, 200);
  previewList->setAlternatingRowColors(pref.useAlternatingRowColors());
  // Fill list in
  std::vector<libtorrent::size_type> fp;
  h.file_progress(fp);
  unsigned int nbFiles = h.num_files();
  for (unsigned int i=0; i<nbFiles; ++i) {
    QString fileName = h.filename_at(i);
    if (fileName.endsWith(".!qB"))
      fileName.chop(4);
    QString extension = fsutils::fileExtension(fileName).toUpper();
    if (misc::isPreviewable(extension)) {
      int row = previewListModel->rowCount();
      previewListModel->insertRow(row);
      previewListModel->setData(previewListModel->index(row, NAME), QVariant(fileName));
      previewListModel->setData(previewListModel->index(row, SIZE), QVariant((qlonglong)h.filesize_at(i)));
      previewListModel->setData(previewListModel->index(row, PROGRESS), QVariant((double)fp[i]/h.filesize_at(i)));
      previewListModel->setData(previewListModel->index(row, FILE_INDEX), QVariant(i));
    }
  }

  previewList->selectionModel()->select(previewListModel->index(0, NAME), QItemSelectionModel::Select);
  previewList->selectionModel()->select(previewListModel->index(0, SIZE), QItemSelectionModel::Select);
  previewList->selectionModel()->select(previewListModel->index(0, PROGRESS), QItemSelectionModel::Select);
  if (!previewListModel->rowCount()) {
    QMessageBox::critical(0, tr("Preview impossible"), tr("Sorry, we can't preview this file"));
    close();
  }
  connect(this, SIGNAL(readyToPreviewFile(QString)), parent, SLOT(previewFile(QString)));
  previewListModel->sort(NAME);
  previewList->header()->setSortIndicator(0, Qt::AscendingOrder);

  if (previewListModel->rowCount() == 1) {
    qDebug("Torrent file only contains one file, no need to display selection dialog before preview");
    // Only one file : no choice
    on_previewButton_clicked();
  }else{
    qDebug("Displaying media file selection dialog for preview");
    show();
  }
}

PreviewSelect::~PreviewSelect() {
  delete previewListModel;
  delete listDelegate;
}


void PreviewSelect::on_previewButton_clicked() {
  QModelIndex index;
  QModelIndexList selectedIndexes = previewList->selectionModel()->selectedRows(FILE_INDEX);
  if (selectedIndexes.size() == 0) return;
  // Flush data
  h.flush_cache();

  QStringList absolute_paths(h.absolute_files_path());
  //only one file should be selected
  QString path = absolute_paths.at(selectedIndexes.at(0).data().toInt());
  // File
  if (QFile::exists(path))
    emit readyToPreviewFile(path);
  else
    QMessageBox::critical(0, tr("Preview impossible"), tr("Sorry, we can't preview this file"));
  close();
}

void PreviewSelect::on_cancelButton_clicked() {
  close();
}
