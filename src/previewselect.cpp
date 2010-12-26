#include <QStandardItemModel>
#include <QHeaderView>
#include <QMessageBox>
#include <QFile>

#include <libtorrent/version.hpp>
#include <libtorrent/session.hpp>

#include "misc.h"
#include "previewlistdelegate.h"
#include "previewselect.h"

PreviewSelect::PreviewSelect(QWidget* parent, QTorrentHandle h): QDialog(parent), h(h){
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
  std::vector<libtorrent::size_type> fp;
  h.file_progress(fp);
  unsigned int nbFiles = h.num_files();
  for(unsigned int i=0; i<nbFiles; ++i){
    QString fileName = h.filename_at(i);
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
    qDebug("Torrent file only contains one file, no need to display selection dialog before preview");
    // Only one file : no choice
    on_previewButton_clicked();
  }else{
    qDebug("Displaying media file selection dialog for preview");
    show();
  }
}

PreviewSelect::~PreviewSelect(){
  delete previewListModel;
  delete listDelegate;
}


void PreviewSelect::on_previewButton_clicked(){
  QModelIndex index;
  QModelIndexList selectedIndexes = previewList->selectionModel()->selectedRows(NAME);
  if(selectedIndexes.size() == 0) return;
#if LIBTORRENT_VERSION_MINOR > 14
  // Flush data
  h.flush_cache();
#endif
  QString path;
  foreach(index, selectedIndexes){
    path = h.files_path().at(indexes.at(index.row()));
    // File
    if(QFile::exists(path)){
      emit readyToPreviewFile(path);
    } else {
      QMessageBox::critical(0, tr("Preview impossible"), tr("Sorry, we can't preview this file"));
    }
    close();
    return;
  }
  qDebug("Cannot find file: %s", path.toLocal8Bit().data());
  QMessageBox::critical(0, tr("Preview impossible"), tr("Sorry, we can't preview this file"));
  close();
}

void PreviewSelect::on_cancelButton_clicked(){
  close();
}
