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

#include "transferlistwidget.h"
#include "qbtsession.h"
#include "torrentpersistentdata.h"
#include "transferlistdelegate.h"
#include "previewselect.h"
#include "speedlimitdlg.h"
#include "options_imp.h"
#include "mainwindow.h"
#include "preferences.h"
#include "torrentmodel.h"
#include "deletionconfirmationdlg.h"
#include "propertieswidget.h"
#include <libtorrent/version.hpp>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QDesktopServices>
#include <QTimer>
#include <QClipboard>
#include <QInputDialog>
#include <QColor>
#include <QUrl>
#include <QMenu>
#include <QRegExp>
#include <QFileDialog>
#include <vector>

#include "qinisettings.h"

TransferListWidget::TransferListWidget(QWidget *parent, MainWindow *main_window, QBtSession *_BTSession):
  QTreeView(parent), BTSession(_BTSession), main_window(main_window) {
  QIniSettings settings("qBittorrent", "qBittorrent");
  // Create and apply delegate
  listDelegate = new TransferListDelegate(this);
  setItemDelegate(listDelegate);

  // Create transfer list model
  listModel = new TorrentModel(this);

  // Set Sort/Filter proxy
  labelFilterModel = new QSortFilterProxyModel();
  labelFilterModel->setDynamicSortFilter(true);
  labelFilterModel->setSourceModel(listModel);
  labelFilterModel->setFilterKeyColumn(TorrentModelItem::TR_LABEL);
  labelFilterModel->setFilterRole(Qt::DisplayRole);

  statusFilterModel = new QSortFilterProxyModel();
  statusFilterModel->setDynamicSortFilter(true);
  statusFilterModel->setSourceModel(labelFilterModel);
  statusFilterModel->setFilterKeyColumn(TorrentModelItem::TR_STATUS);
  statusFilterModel->setFilterRole(Qt::DisplayRole);

  nameFilterModel = new QSortFilterProxyModel();
  nameFilterModel->setDynamicSortFilter(true);
  nameFilterModel->setSourceModel(statusFilterModel);
  nameFilterModel->setFilterKeyColumn(TorrentModelItem::TR_NAME);
  nameFilterModel->setFilterRole(Qt::DisplayRole);

  setModel(nameFilterModel);


  // Visual settings
  setRootIsDecorated(false);
  setAllColumnsShowFocus(true);
  setSortingEnabled(true);
  setSelectionMode(QAbstractItemView::ExtendedSelection);
  setItemsExpandable(false);
  setAutoScroll(true);
  setDragDropMode(QAbstractItemView::DragOnly);

  hideColumn(TorrentModelItem::TR_PRIORITY);
  loadHiddenColumns();
  // Load last columns width for transfer list
  if(!loadColWidthList()) {
    header()->resizeSection(0, 200);
  }
  loadLastSortedColumn();
  setContextMenuPolicy(Qt::CustomContextMenu);


  // Listen for list events
  connect(this, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(torrentDoubleClicked(QModelIndex)));
  connect(this, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayListMenu(const QPoint&)));
  header()->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(header(), SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayDLHoSMenu(const QPoint&)));
}

TransferListWidget::~TransferListWidget() {
  // Save settings
  saveLastSortedColumn();
  saveColWidthList();
  saveHiddenColumns();
  // Clean up
  delete labelFilterModel;
  delete statusFilterModel;
  delete nameFilterModel;
  delete listModel;
  delete listDelegate;
}

TorrentModel* TransferListWidget::getSourceModel() const {
  return listModel;
}

void TransferListWidget::previewFile(QString filePath) {
#ifdef Q_WS_WIN
  QDesktopServices::openUrl(QUrl(QString("file:///")+filePath));
#else
  QDesktopServices::openUrl(QUrl(QString("file://")+filePath));
#endif
}

void TransferListWidget::setRefreshInterval(int t) {
  qDebug("Settings transfer list refresh interval to %dms", t);
  listModel->setRefreshInterval(t);
}

int TransferListWidget::getRowFromHash(QString hash) const{
  return listModel->torrentRow(hash);
}

inline QString TransferListWidget::getHashFromRow(int row) const {
  return listModel->torrentHash(row);
}

inline QModelIndex TransferListWidget::mapToSource(const QModelIndex &index) const {
  Q_ASSERT(index.isValid());
  if(index.model() == nameFilterModel)
    return labelFilterModel->mapToSource(statusFilterModel->mapToSource(nameFilterModel->mapToSource(index)));
  if(index.model() == statusFilterModel)
    return labelFilterModel->mapToSource(statusFilterModel->mapToSource(index));
  return labelFilterModel->mapToSource(index);
}

inline QModelIndex TransferListWidget::mapFromSource(const QModelIndex &index) const {
  Q_ASSERT(index.isValid());
  Q_ASSERT(index.model() == labelFilterModel);
  return nameFilterModel->mapFromSource(statusFilterModel->mapFromSource(labelFilterModel->mapFromSource(index)));
}


QStringList TransferListWidget::getCustomLabels() const {
  QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  return settings.value("TransferListFilters/customLabels", QStringList()).toStringList();
}

void TransferListWidget::torrentDoubleClicked(const QModelIndex& index) {
  const int row = mapToSource(index).row();
  const QString hash = getHashFromRow(row);
  QTorrentHandle h = BTSession->getTorrentHandle(hash);
  if(!h.is_valid()) return;
  int action;
  if(h.is_seed()) {
    action = Preferences().getActionOnDblClOnTorrentFn();
  } else {
    action = Preferences().getActionOnDblClOnTorrentDl();
  }

  switch(action) {
  case TOGGLE_PAUSE:
    if(h.is_paused()) {
      h.resume();
    } else {
      h.pause();
    }
    break;
  case OPEN_DEST:
#ifdef Q_WS_WIN
    QDesktopServices::openUrl(QUrl("file:///" + h.save_path()));
#else
    QDesktopServices::openUrl(QUrl("file://" + h.save_path()));
#endif
    break;
  }
}

QStringList TransferListWidget::getSelectedTorrentsHashes() const {
  QStringList hashes;
  const QModelIndexList selectedIndexes = selectionModel()->selectedRows();
  foreach(const QModelIndex &index, selectedIndexes) {
    hashes << getHashFromRow(mapToSource(index).row());
  }
  return hashes;
}

void TransferListWidget::setSelectedTorrentsLocation() {
  const QStringList hashes = getSelectedTorrentsHashes();
  if(hashes.isEmpty()) return;
  QString dir;
  const QDir saveDir(TorrentPersistentData::getSavePath(hashes.first()));
  qDebug("Old save path is %s", qPrintable(saveDir.absolutePath()));
  QFileDialog dlg(this, tr("Choose save path"), saveDir.absolutePath());
  dlg.setConfirmOverwrite(false);
  dlg.setFileMode(QFileDialog::Directory);
  dlg.setOption(QFileDialog::ShowDirsOnly, true);
  dlg.setFilter(QDir::AllDirs);
  dlg.setAcceptMode(QFileDialog::AcceptSave);
  dlg.setNameFilterDetailsVisible(false);
  if(dlg.exec())
    dir = dlg.selectedFiles().first();
  if(!dir.isNull()) {
    qDebug("New path is %s", qPrintable(dir));
    // Check if savePath exists
    QDir savePath(misc::expandPath(dir));
    qDebug("New path after clean up is %s", qPrintable(savePath.absolutePath()));
    foreach(const QString & hash, hashes) {
      // Actually move storage
      QTorrentHandle h = BTSession->getTorrentHandle(hash);
      if(!BTSession->useTemporaryFolder() || h.is_seed()) {
        if(!savePath.exists()) savePath.mkpath(savePath.absolutePath());
        h.move_storage(savePath.absolutePath());
      } else {
        TorrentPersistentData::saveSavePath(h.hash(), savePath.absolutePath());
        main_window->getProperties()->updateSavePath(h);
      }
    }
  }
}

void TransferListWidget::startSelectedTorrents() {
  const QStringList hashes = getSelectedTorrentsHashes();
  foreach(const QString &hash, hashes) {
    BTSession->resumeTorrent(hash);
  }
}

void TransferListWidget::startVisibleTorrents() {
  QStringList hashes;
  for(int i=0; i<nameFilterModel->rowCount(); ++i) {
    const int row = mapToSource(nameFilterModel->index(i, 0)).row();
    hashes << getHashFromRow(row);
  }
  foreach(const QString &hash, hashes) {
    BTSession->resumeTorrent(hash);
  }
}

void TransferListWidget::pauseSelectedTorrents() {
  const QStringList hashes = getSelectedTorrentsHashes();
  foreach(const QString &hash, hashes) {
    BTSession->pauseTorrent(hash);
  }
}

void TransferListWidget::pauseVisibleTorrents() {
  QStringList hashes;
  for(int i=0; i<nameFilterModel->rowCount(); ++i) {
    const int row = mapToSource(nameFilterModel->index(i, 0)).row();
    hashes << getHashFromRow(row);
  }
  foreach(const QString &hash, hashes) {
    BTSession->pauseTorrent(hash);
  }
}

void TransferListWidget::deleteSelectedTorrents() {
  if(main_window->getCurrentTabWidget() != this) return;
  const QStringList& hashes = getSelectedTorrentsHashes();
  if(!hashes.empty()) {
    bool delete_local_files = false;
    if(DeletionConfirmationDlg::askForDeletionConfirmation(&delete_local_files)) {
      foreach(const QString &hash, hashes) {
        BTSession->deleteTorrent(hash, delete_local_files);
      }
    }
  }
}

void TransferListWidget::deleteVisibleTorrents() {
  if(nameFilterModel->rowCount() <= 0) return;
  bool delete_local_files = false;
  if(DeletionConfirmationDlg::askForDeletionConfirmation(&delete_local_files)) {
    QStringList hashes;
    for(int i=0; i<nameFilterModel->rowCount(); ++i) {
      const int row = mapToSource(nameFilterModel->index(i, 0)).row();
      hashes << getHashFromRow(row);
    }
    foreach(const QString &hash, hashes) {
      BTSession->deleteTorrent(hash, delete_local_files);
    }
  }
}

void TransferListWidget::increasePrioSelectedTorrents() {
  if(main_window->getCurrentTabWidget() != this) return;
  const QStringList hashes = getSelectedTorrentsHashes();
  foreach(const QString &hash, hashes) {
    QTorrentHandle h = BTSession->getTorrentHandle(hash);
    if(h.is_valid() && !h.is_seed()) {
      h.queue_position_up();
    }
  }
}

void TransferListWidget::decreasePrioSelectedTorrents() {
  if(main_window->getCurrentTabWidget() != this) return;
  const QStringList hashes = getSelectedTorrentsHashes();
  foreach(const QString &hash, hashes) {
    QTorrentHandle h = BTSession->getTorrentHandle(hash);
    if(h.is_valid() && !h.is_seed()) {
      h.queue_position_down();
    }
  }
}

void TransferListWidget::topPrioSelectedTorrents() {
  if(main_window->getCurrentTabWidget() != this) return;
  const QStringList hashes = getSelectedTorrentsHashes();
  foreach(const QString &hash, hashes) {
    QTorrentHandle h = BTSession->getTorrentHandle(hash);
    if(h.is_valid() && !h.is_seed()) {
      h.queue_position_top();
    }
  }
}

void TransferListWidget::bottomPrioSelectedTorrents() {
  if(main_window->getCurrentTabWidget() != this) return;
  const QStringList hashes = getSelectedTorrentsHashes();
  foreach(const QString &hash, hashes) {
    QTorrentHandle h = BTSession->getTorrentHandle(hash);
    if(h.is_valid() && !h.is_seed()) {
      h.queue_position_bottom();
    }
  }
}

void TransferListWidget::copySelectedMagnetURIs() const {
  QStringList magnet_uris;
  const QStringList hashes = getSelectedTorrentsHashes();
  foreach(const QString &hash, hashes) {
    const QTorrentHandle h = BTSession->getTorrentHandle(hash);
    if(h.is_valid() && h.has_metadata())
      magnet_uris << misc::toQString(make_magnet_uri(h.get_torrent_info()));
  }
  qApp->clipboard()->setText(magnet_uris.join("\n"));
}

void TransferListWidget::hidePriorityColumn(bool hide) {
  qDebug("hidePriorityColumn(%d)", hide);
  setColumnHidden(TorrentModelItem::TR_PRIORITY, hide);
}

void TransferListWidget::openSelectedTorrentsFolder() const {
  QStringList pathsList;
  const QStringList hashes = getSelectedTorrentsHashes();
  foreach(const QString &hash, hashes) {
    const QTorrentHandle h = BTSession->getTorrentHandle(hash);
    if(h.is_valid()) {
      const QString savePath = h.save_path();
      qDebug("Opening path at %s", qPrintable(savePath));
      if(!pathsList.contains(savePath)) {
        pathsList.append(savePath);
#ifdef Q_WS_WIN
        QDesktopServices::openUrl(QUrl(QString("file:///")+savePath));
#else
        QDesktopServices::openUrl(QUrl(QString("file://")+savePath));
#endif
      }
    }
  }
}

void TransferListWidget::previewSelectedTorrents() {
  const QStringList hashes = getSelectedTorrentsHashes();
  foreach(const QString &hash, hashes) {
    const QTorrentHandle h = BTSession->getTorrentHandle(hash);
    if(h.is_valid() && h.has_metadata()) {
      new previewSelect(this, h);
    }
  }
}

void TransferListWidget::setDlLimitSelectedTorrents() {
  QList<QTorrentHandle> selected_torrents;
  bool first = true;
  bool all_same_limit = true;
  const QStringList hashes = getSelectedTorrentsHashes();
  foreach(const QString &hash, hashes) {
    const QTorrentHandle h = BTSession->getTorrentHandle(hash);
    if(h.is_valid() && !h.is_seed()) {
      selected_torrents << h;
      // Determine current limit for selected torrents
      if(first) {
        first = false;
      } else {
        if(all_same_limit && h.download_limit() != selected_torrents.first().download_limit())
          all_same_limit = false;
      }
    }
  }
  if(selected_torrents.empty()) return;

  bool ok=false;
  int default_limit = -1;
  if(all_same_limit)
    default_limit = selected_torrents.first().download_limit();
  const long new_limit = SpeedLimitDialog::askSpeedLimit(&ok, tr("Torrent Download Speed Limiting"), default_limit, Preferences().getGlobalDownloadLimit()*1024.);
  if(ok) {
    foreach(const QTorrentHandle &h, selected_torrents) {
      qDebug("Applying download speed limit of %ld Kb/s to torrent %s", (long)(new_limit/1024.), qPrintable(h.hash()));
      BTSession->setDownloadLimit(h.hash(), new_limit);
    }
  }
}

void TransferListWidget::setUpLimitSelectedTorrents() {
  QList<QTorrentHandle> selected_torrents;
  bool first = true;
  bool all_same_limit = true;
  const QStringList hashes = getSelectedTorrentsHashes();
  foreach(const QString &hash, hashes) {
    const QTorrentHandle h = BTSession->getTorrentHandle(hash);
    if(h.is_valid()) {
      selected_torrents << h;
      // Determine current limit for selected torrents
      if(first) {
        first = false;
      } else {
        if(all_same_limit && h.upload_limit() != selected_torrents.first().upload_limit())
          all_same_limit = false;
      }
    }
  }
  if(selected_torrents.empty()) return;

  bool ok=false;
  int default_limit = -1;
  if(all_same_limit)
    default_limit = selected_torrents.first().upload_limit();
  const long new_limit = SpeedLimitDialog::askSpeedLimit(&ok, tr("Torrent Upload Speed Limiting"), default_limit, Preferences().getGlobalUploadLimit()*1024.);
  if(ok) {
    foreach(const QTorrentHandle &h, selected_torrents) {
      qDebug("Applying upload speed limit of %ld Kb/s to torrent %s", (long)(new_limit/1024.), qPrintable(h.hash()));
      BTSession->setUploadLimit(h.hash(), new_limit);
    }
  }
}

void TransferListWidget::recheckSelectedTorrents() {
  const QStringList hashes = getSelectedTorrentsHashes();
  foreach(const QString &hash, hashes) {
    BTSession->recheckTorrent(hash);
  }
}

// save the hidden columns in settings
void TransferListWidget::saveHiddenColumns() const {
  QIniSettings settings("qBittorrent", "qBittorrent");
  QStringList ishidden_list;
  const short nbColumns = listModel->columnCount()-1;//hash column is hidden

  for(short i=0; i<nbColumns; ++i){
    if(isColumnHidden(i)) {
      //qDebug("Column named %s is hidden.", qPrintable(listModel->headerData(i, Qt::Horizontal).toString()));
      ishidden_list << "0";
    } else {
      ishidden_list << "1";
    }
  }
  settings.setValue("TransferListColsHoS", ishidden_list.join(" "));
}

// load the previous settings, and hide the columns
bool TransferListWidget::loadHiddenColumns() {
  QIniSettings settings("qBittorrent", "qBittorrent");
  const QString line = settings.value("TransferListColsHoS", "").toString();
  bool loaded = false;
  const QStringList ishidden_list = line.split(' ');
  const unsigned int nbCol = ishidden_list.size();
  if(nbCol == (unsigned int)listModel->columnCount()-1) {
    for(unsigned int i=0; i<nbCol; ++i){
      if(ishidden_list.at(i) == "0") {
        setColumnHidden(i, true);
      }
    }
    loaded = true;
  }
  // As a default, hide less useful columns
  if(!loaded) {
    setColumnHidden(TorrentModelItem::TR_ADD_DATE, true);
    setColumnHidden(TorrentModelItem::TR_SEED_DATE, true);
    setColumnHidden(TorrentModelItem::TR_UPLIMIT, true);
    setColumnHidden(TorrentModelItem::TR_DLLIMIT, true);
    setColumnHidden(TorrentModelItem::TR_TRACKER, true);
    setColumnHidden(TorrentModelItem::TR_AMOUNT_DOWNLOADED, true);
    setColumnHidden(TorrentModelItem::TR_AMOUNT_LEFT, true);
  }
  return loaded;
}

// hide/show columns menu
void TransferListWidget::displayDLHoSMenu(const QPoint&){
  QMenu hideshowColumn(this);
  hideshowColumn.setTitle(tr("Column visibility"));
  QList<QAction*> actions;
  for(int i=0; i < listModel->columnCount(); ++i) {
    if(!BTSession->isQueueingEnabled() && i == TorrentModelItem::TR_PRIORITY) {
      actions.append(0);
      continue;
    }
    QAction *myAct = hideshowColumn.addAction(listModel->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString());
    myAct->setCheckable(true);
    myAct->setChecked(!isColumnHidden(i));
    actions.append(myAct);
  }
  // Call menu
  QAction *act = hideshowColumn.exec(QCursor::pos());
  if(act) {
    int col = actions.indexOf(act);
    Q_ASSERT(col >= 0);
    qDebug("Toggling column %d visibility", col);
    setColumnHidden(col, !isColumnHidden(col));
    if(!isColumnHidden(col) && columnWidth(col) <= 5)
      setColumnWidth(col, 100);
  }
}

#if LIBTORRENT_VERSION_MINOR > 14
void TransferListWidget::toggleSelectedTorrentsSuperSeeding() const {
  const QStringList hashes = getSelectedTorrentsHashes();
  foreach(const QString &hash, hashes) {
    QTorrentHandle h = BTSession->getTorrentHandle(hash);
    if(h.is_valid() && h.has_metadata()) {
      h.super_seeding(!h.super_seeding());
    }
  }
}
#endif

void TransferListWidget::toggleSelectedTorrentsSequentialDownload() const {
  const QStringList hashes = getSelectedTorrentsHashes();
  foreach(const QString &hash, hashes) {
    QTorrentHandle h = BTSession->getTorrentHandle(hash);
    if(h.is_valid() && h.has_metadata()) {
      h.set_sequential_download(!h.is_sequential_download());
    }
  }
}

void TransferListWidget::toggleSelectedFirstLastPiecePrio() const {
  QStringList hashes = getSelectedTorrentsHashes();
  foreach(const QString &hash, hashes) {
    QTorrentHandle h = BTSession->getTorrentHandle(hash);
    if(h.is_valid() && h.has_metadata()) {
      h.prioritize_first_last_piece(!h.first_last_piece_first());
    }
  }
}

void TransferListWidget::askNewLabelForSelection() {
  // Ask for label
  bool ok;
  bool invalid;
  do {
    invalid = false;
    const QString label = QInputDialog::getText(this, tr("New Label"), tr("Label:"), QLineEdit::Normal, "", &ok);
    if (ok && !label.isEmpty()) {
      if(misc::isValidFileSystemName(label)) {
        setSelectionLabel(label);
      } else {
        QMessageBox::warning(this, tr("Invalid label name"), tr("Please don't use any special characters in the label name."));
        invalid = true;
      }
    }
  }while(invalid);
}

void TransferListWidget::renameSelectedTorrent() {
  const QModelIndexList selectedIndexes = selectionModel()->selectedRows();
  if(selectedIndexes.size() != 1) return;
  if(!selectedIndexes.first().isValid()) return;
  const QString hash = getHashFromRow(mapToSource(selectedIndexes.first()).row());
  const QTorrentHandle h = BTSession->getTorrentHandle(hash);
  if(!h.is_valid()) return;
  // Ask for a new Name
  bool ok;
  const QString name = QInputDialog::getText(this, tr("Rename"), tr("New name:"), QLineEdit::Normal, h.name(), &ok);
  if (ok && !name.isEmpty()) {
    // Rename the torrent
    nameFilterModel->setData(selectedIndexes.first(), name, Qt::DisplayRole);
  }
}

void TransferListWidget::setSelectionLabel(QString label) {
  const QStringList& hashes = getSelectedTorrentsHashes();
  foreach(const QString &hash, hashes) {
    Q_ASSERT(!hash.isEmpty());
    const int row = getRowFromHash(hash);
    const QString old_label = listModel->data(listModel->index(row, TorrentModelItem::TR_LABEL)).toString();
    listModel->setData(listModel->index(row, TorrentModelItem::TR_LABEL), QVariant(label), Qt::DisplayRole);
    // Update save path if necessary
    QTorrentHandle h = BTSession->getTorrentHandle(hash);
    BTSession->changeLabelInTorrentSavePath(h, old_label, label);
  }
}

void TransferListWidget::removeLabelFromRows(QString label) {
  for(int i=0; i<listModel->rowCount(); ++i) {
    if(listModel->data(listModel->index(i, TorrentModelItem::TR_LABEL)) == label) {
      const QString hash = getHashFromRow(i);
      listModel->setData(listModel->index(i, TorrentModelItem::TR_LABEL), "", Qt::DisplayRole);
      // Update save path if necessary
      QTorrentHandle h = BTSession->getTorrentHandle(hash);
      BTSession->changeLabelInTorrentSavePath(h, label, "");
    }
  }
}

void TransferListWidget::displayListMenu(const QPoint&) {
  // Create actions
  QAction actionStart(QIcon(QString::fromUtf8(":/Icons/skin/play.png")), tr("Resume", "Resume/start the torrent"), 0);
  connect(&actionStart, SIGNAL(triggered()), this, SLOT(startSelectedTorrents()));
  QAction actionPause(QIcon(QString::fromUtf8(":/Icons/skin/pause.png")), tr("Pause", "Pause the torrent"), 0);
  connect(&actionPause, SIGNAL(triggered()), this, SLOT(pauseSelectedTorrents()));
  QAction actionDelete(QIcon(QString::fromUtf8(":/Icons/skin/delete.png")), tr("Delete", "Delete the torrent"), 0);
  connect(&actionDelete, SIGNAL(triggered()), this, SLOT(deleteSelectedTorrents()));
  QAction actionPreview_file(QIcon(QString::fromUtf8(":/Icons/skin/preview.png")), tr("Preview file..."), 0);
  connect(&actionPreview_file, SIGNAL(triggered()), this, SLOT(previewSelectedTorrents()));
  QAction actionSet_upload_limit(QIcon(QString::fromUtf8(":/Icons/skin/seeding.png")), tr("Limit upload rate..."), 0);
  connect(&actionSet_upload_limit, SIGNAL(triggered()), this, SLOT(setUpLimitSelectedTorrents()));
  QAction actionSet_download_limit(QIcon(QString::fromUtf8(":/Icons/skin/download.png")), tr("Limit download rate..."), 0);
  connect(&actionSet_download_limit, SIGNAL(triggered()), this, SLOT(setDlLimitSelectedTorrents()));
  QAction actionOpen_destination_folder(QIcon(QString::fromUtf8(":/Icons/oxygen/folder.png")), tr("Open destination folder"), 0);
  connect(&actionOpen_destination_folder, SIGNAL(triggered()), this, SLOT(openSelectedTorrentsFolder()));
  //QAction actionBuy_it(QIcon(QString::fromUtf8(":/Icons/oxygen/wallet.png")), tr("Buy it"), 0);
  //connect(&actionBuy_it, SIGNAL(triggered()), this, SLOT(buySelectedTorrents()));
  QAction actionIncreasePriority(QIcon(QString::fromUtf8(":/Icons/oxygen/go-up.png")), tr("Move up", "i.e. move up in the queue"), 0);
  connect(&actionIncreasePriority, SIGNAL(triggered()), this, SLOT(increasePrioSelectedTorrents()));
  QAction actionDecreasePriority(QIcon(QString::fromUtf8(":/Icons/oxygen/go-down.png")), tr("Move down", "i.e. Move down in the queue"), 0);
  connect(&actionDecreasePriority, SIGNAL(triggered()), this, SLOT(decreasePrioSelectedTorrents()));
  QAction actionTopPriority(QIcon(QString::fromUtf8(":/Icons/oxygen/go-top.png")), tr("Move to top", "i.e. Move to top of the queue"), 0);
  connect(&actionTopPriority, SIGNAL(triggered()), this, SLOT(topPrioSelectedTorrents()));
  QAction actionBottomPriority(QIcon(QString::fromUtf8(":/Icons/oxygen/go-bottom.png")), tr("Move to bottom", "i.e. Move to bottom of the queue"), 0);
  connect(&actionBottomPriority, SIGNAL(triggered()), this, SLOT(bottomPrioSelectedTorrents()));
  QAction actionSetTorrentPath(QIcon(QString::fromUtf8(":/Icons/skin/folder.png")), tr("Set location..."), 0);
  connect(&actionSetTorrentPath, SIGNAL(triggered()), this, SLOT(setSelectedTorrentsLocation()));
  QAction actionForce_recheck(QIcon(QString::fromUtf8(":/Icons/oxygen/gear.png")), tr("Force recheck"), 0);
  connect(&actionForce_recheck, SIGNAL(triggered()), this, SLOT(recheckSelectedTorrents()));
  QAction actionCopy_magnet_link(QIcon(QString::fromUtf8(":/Icons/magnet.png")), tr("Copy magnet link"), 0);
  connect(&actionCopy_magnet_link, SIGNAL(triggered()), this, SLOT(copySelectedMagnetURIs()));
#if LIBTORRENT_VERSION_MINOR > 14
  QAction actionSuper_seeding_mode(tr("Super seeding mode"), 0);
  actionSuper_seeding_mode.setCheckable(true);
  connect(&actionSuper_seeding_mode, SIGNAL(triggered()), this, SLOT(toggleSelectedTorrentsSuperSeeding()));
#endif
  QAction actionRename(QIcon(QString::fromUtf8(":/Icons/oxygen/edit_clear.png")), tr("Rename..."), 0);
  connect(&actionRename, SIGNAL(triggered()), this, SLOT(renameSelectedTorrent()));
  QAction actionSequential_download(tr("Download in sequential order"), 0);
  actionSequential_download.setCheckable(true);
  connect(&actionSequential_download, SIGNAL(triggered()), this, SLOT(toggleSelectedTorrentsSequentialDownload()));
  QAction actionFirstLastPiece_prio(tr("Download first and last piece first"), 0);
  actionFirstLastPiece_prio.setCheckable(true);
  connect(&actionFirstLastPiece_prio, SIGNAL(triggered()), this, SLOT(toggleSelectedFirstLastPiecePrio()));
  // End of actions
  QMenu listMenu(this);
  // Enable/disable pause/start action given the DL state
  QModelIndexList selectedIndexes = selectionModel()->selectedRows();
  bool has_pause = false, has_start = false, has_preview = false;
#if LIBTORRENT_VERSION_MINOR > 14
  bool all_same_super_seeding = true;
  bool super_seeding_mode = false;
#endif
  bool all_same_sequential_download_mode = true, all_same_prio_firstlast = true;
  bool sequential_download_mode = false, prioritize_first_last = false;
  bool one_has_metadata = false, one_not_seed = false;
  bool first = true;
  QTorrentHandle h;
  qDebug("Displaying menu");
  foreach(const QModelIndex &index, selectedIndexes) {
    // Get the file name
    QString hash = getHashFromRow(mapToSource(index).row());
    // Get handle and pause the torrent
    h = BTSession->getTorrentHandle(hash);
    if(!h.is_valid()) continue;
    if(h.has_metadata())
      one_has_metadata = true;
    if(!h.is_seed()) {
      one_not_seed = true;
      if(h.has_metadata()) {
        if(first) {
          sequential_download_mode = h.is_sequential_download();
          prioritize_first_last = h.first_last_piece_first();
        } else {
          if(sequential_download_mode != h.is_sequential_download()) {
            all_same_sequential_download_mode = false;
          }
          if(prioritize_first_last != h.first_last_piece_first()) {
            all_same_prio_firstlast = false;
          }
        }
      }
    }
#if LIBTORRENT_VERSION_MINOR > 14
    else {
      if(!one_not_seed && all_same_super_seeding && h.has_metadata()) {
        if(first) {
          super_seeding_mode = h.super_seeding();
        } else {
          if(super_seeding_mode != h.super_seeding()) {
            all_same_super_seeding = false;
          }
        }
      }
    }
#endif
    if(h.is_paused()) {
      if(!has_start) {
        listMenu.addAction(&actionStart);
        has_start = true;
      }
    }else{
      if(!has_pause) {
        listMenu.addAction(&actionPause);
        has_pause = true;
      }
    }
    if(h.has_metadata() && BTSession->isFilePreviewPossible(hash) && !has_preview) {
      has_preview = true;
    }
    first = false;
    if(has_pause && has_start && has_preview && one_not_seed) break;
  }
  listMenu.addSeparator();
  listMenu.addAction(&actionDelete);
  listMenu.addSeparator();
  listMenu.addAction(&actionSetTorrentPath);
  if(selectedIndexes.size() == 1)
    listMenu.addAction(&actionRename);
  // Label Menu
  QStringList customLabels = getCustomLabels();
  customLabels.sort();
  QList<QAction*> labelActions;
  QMenu *labelMenu = listMenu.addMenu(QIcon(":/Icons/oxygen/feed-subscribe.png"), tr("Label"));
  labelActions << labelMenu->addAction(QIcon(":/Icons/oxygen/list-add.png"), tr("New...", "New label..."));
  labelActions << labelMenu->addAction(QIcon(":/Icons/oxygen/edit-clear.png"), tr("Reset", "Reset label"));
  labelMenu->addSeparator();
  foreach(const QString &label, customLabels) {
    labelActions << labelMenu->addAction(QIcon(":/Icons/oxygen/folder.png"), label);
  }
  listMenu.addSeparator();
  if(one_not_seed)
    listMenu.addAction(&actionSet_download_limit);
  listMenu.addAction(&actionSet_upload_limit);
#if LIBTORRENT_VERSION_MINOR > 14
  if(!one_not_seed && all_same_super_seeding && one_has_metadata) {
    actionSuper_seeding_mode.setChecked(super_seeding_mode);
    listMenu.addAction(&actionSuper_seeding_mode);
  }
#endif
  listMenu.addSeparator();
  bool added_preview_action = false;
  if(has_preview) {
    listMenu.addAction(&actionPreview_file);
    added_preview_action = true;
  }
  if(one_not_seed && one_has_metadata) {
    if(all_same_sequential_download_mode) {
      actionSequential_download.setChecked(sequential_download_mode);
      listMenu.addAction(&actionSequential_download);
      added_preview_action = true;
    }
    if(all_same_prio_firstlast) {
      actionFirstLastPiece_prio.setChecked(prioritize_first_last);
      listMenu.addAction(&actionFirstLastPiece_prio);
      added_preview_action = true;
    }
  }
  if(added_preview_action)
    listMenu.addSeparator();
  if(one_has_metadata) {
    listMenu.addAction(&actionForce_recheck);
    listMenu.addSeparator();
  }
  listMenu.addAction(&actionOpen_destination_folder);
  if(BTSession->isQueueingEnabled() && one_not_seed) {
    listMenu.addSeparator();
    QMenu *prioMenu = listMenu.addMenu(tr("Priority"));
    prioMenu->addAction(&actionTopPriority);
    prioMenu->addAction(&actionIncreasePriority);
    prioMenu->addAction(&actionDecreasePriority);
    prioMenu->addAction(&actionBottomPriority);
  }
  listMenu.addSeparator();
  if(one_has_metadata)
    listMenu.addAction(&actionCopy_magnet_link);
  //listMenu.addAction(&actionBuy_it);
  // Call menu
  QAction *act = 0;
  act = listMenu.exec(QCursor::pos());
  if(act) {
    // Parse label actions only (others have slots assigned)
    int i = labelActions.indexOf(act);
    if(i >= 0) {
      // Label action
      if(i == 0) {
        // New Label
        askNewLabelForSelection();
      } else {
        QString label = "";
        if(i > 1)
          label = customLabels.at(i-2);
        // Update Label
        setSelectionLabel(label);
      }
    }
  }
}

// Save columns width in a file to remember them
void TransferListWidget::saveColWidthList() {
  qDebug("Saving columns width in transfer list");
  QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  QStringList width_list;
  QStringList new_width_list;
  const short nbColumns = listModel->columnCount()-1; // HASH is hidden
  if(nbColumns <= 0) return;
  const QString line = settings.value("TransferListColsWidth", QString()).toString();
  if(!line.isEmpty()) {
    width_list = line.split(' ');
  }
  for(short i=0; i<nbColumns; ++i){
    if(columnWidth(i)<1 && width_list.size() == nbColumns && width_list.at(i).toInt()>=1) {
      // load the former width
      new_width_list << width_list.at(i);
    } else if(columnWidth(i)>=1) {
      // usual case, save the current width
      new_width_list << QString::number(columnWidth(i));
    } else {
      // default width
      resizeColumnToContents(i);
      new_width_list << QString::number(columnWidth(i));
    }
  }
  settings.setValue(QString::fromUtf8("TransferListColsWidth"), new_width_list.join(QString::fromUtf8(" ")));
  QStringList visualIndexes;
  for(int i=0; i<nbColumns; ++i) {
    visualIndexes << QString::number(header()->visualIndex(i));
  }
  settings.setValue(QString::fromUtf8("TransferListVisualIndexes"), visualIndexes);
  qDebug("Download list columns width saved");
}

// Load columns width in a file that were saved previously
bool TransferListWidget::loadColWidthList() {
  qDebug("Loading columns width for download list");
  QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  QString line = settings.value(QString::fromUtf8("TransferListColsWidth"), QString()).toString();
  if(line.isEmpty())
    return false;
  const QStringList width_list = line.split(" ");
  if(width_list.size() != listModel->columnCount()-1) {
    qDebug("Corrupted values for transfer list columns sizes");
    return false;
  }
  const unsigned int listSize = width_list.size();
  for(unsigned int i=0; i<listSize; ++i) {
    header()->resizeSection(i, width_list.at(i).toInt());
  }
  const QList<int> visualIndexes = misc::intListfromStringList(settings.value(QString::fromUtf8("TransferListVisualIndexes")).toStringList());
  if(visualIndexes.size() != listModel->columnCount()-1) {
    qDebug("Corrupted values for transfer list columns indexes");
    return false;
  }
  bool change = false;
  do {
    change = false;
    for(int i=0;i<visualIndexes.size(); ++i) {
      const int new_visual_index = visualIndexes.at(header()->logicalIndex(i));
      if(i != new_visual_index) {
        qDebug("Moving column from %d to %d", header()->logicalIndex(i), new_visual_index);
        header()->moveSection(i, new_visual_index);
        change = true;
      }
    }
  }while(change);
  qDebug("Transfer list columns width loaded");
  return true;
}

void TransferListWidget::saveLastSortedColumn() {
  QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  const Qt::SortOrder sortOrder = header()->sortIndicatorOrder();
  QString sortOrderLetter;
  if(sortOrder == Qt::AscendingOrder)
    sortOrderLetter = QString::fromUtf8("a");
  else
    sortOrderLetter = QString::fromUtf8("d");
  const int index = header()->sortIndicatorSection();
  settings.setValue(QString::fromUtf8("TransferListSortedCol"), QVariant(QString::number(index)+sortOrderLetter));
}

void TransferListWidget::loadLastSortedColumn() {
  // Loading last sorted column
  QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  QString sortedCol = settings.value(QString::fromUtf8("TransferListSortedCol"), QString()).toString();
  if(!sortedCol.isEmpty()) {
    Qt::SortOrder sortOrder;
    if(sortedCol.endsWith(QString::fromUtf8("d")))
      sortOrder = Qt::DescendingOrder;
    else
      sortOrder = Qt::AscendingOrder;
    sortedCol.chop(1);
    const int index = sortedCol.toInt();
    sortByColumn(index, sortOrder);
  }
}

void TransferListWidget::currentChanged(const QModelIndex& current, const QModelIndex&) {
  qDebug("CURRENT CHANGED");
  QTorrentHandle h;
  if(current.isValid()) {
    const int row = mapToSource(current).row();
    h = BTSession->getTorrentHandle(getHashFromRow(row));
    // Scroll Fix
    scrollTo(current);
  }
  emit currentTorrentChanged(h);
}

void TransferListWidget::applyLabelFilter(QString label) {
  if(label == "all") {
    labelFilterModel->setFilterRegExp(QRegExp());
    return;
  }
  if(label == "none") {
    labelFilterModel->setFilterRegExp(QRegExp("^$"));
    return;
  }
  qDebug("Applying Label filter: %s", qPrintable(label));
  labelFilterModel->setFilterRegExp(QRegExp("^"+label+"$", Qt::CaseSensitive));
}

void TransferListWidget::applyNameFilter(QString name) {
  nameFilterModel->setFilterRegExp(QRegExp(name, Qt::CaseInsensitive));
}

void TransferListWidget::applyStatusFilter(int f) {
  switch(f) {
  case FILTER_DOWNLOADING:
    statusFilterModel->setFilterRegExp(QRegExp(QString::number(TorrentModelItem::STATE_DOWNLOADING)+"|"+QString::number(TorrentModelItem::STATE_STALLED_DL)+"|"+
                                               QString::number(TorrentModelItem::STATE_PAUSED_DL)+"|"+QString::number(TorrentModelItem::STATE_CHECKING_DL)+"|"+
                                               QString::number(TorrentModelItem::STATE_QUEUED_DL), Qt::CaseSensitive));
    break;
  case FILTER_COMPLETED:
    statusFilterModel->setFilterRegExp(QRegExp(QString::number(TorrentModelItem::STATE_SEEDING)+"|"+QString::number(TorrentModelItem::STATE_STALLED_UP)+"|"+
                                               QString::number(TorrentModelItem::STATE_PAUSED_UP)+"|"+QString::number(TorrentModelItem::STATE_CHECKING_UP)+"|"+
                                               QString::number(TorrentModelItem::STATE_QUEUED_UP), Qt::CaseSensitive));
    break;
  case FILTER_ACTIVE:
    statusFilterModel->setFilterRegExp(QRegExp(QString::number(TorrentModelItem::STATE_DOWNLOADING)+"|"+QString::number(TorrentModelItem::STATE_SEEDING), Qt::CaseSensitive));
    break;
  case FILTER_INACTIVE:
    statusFilterModel->setFilterRegExp(QRegExp("[^"+QString::number(TorrentModelItem::STATE_DOWNLOADING)+QString::number(TorrentModelItem::STATE_SEEDING)+"]", Qt::CaseSensitive));
    break;
  case FILTER_PAUSED:
    statusFilterModel->setFilterRegExp(QRegExp(QString::number(TorrentModelItem::STATE_PAUSED_UP)+"|"+QString::number(TorrentModelItem::STATE_PAUSED_DL)));
    break;
  default:
    statusFilterModel->setFilterRegExp(QRegExp());
  }
  // Select first item if nothing is selected
  if(selectionModel()->selectedRows(0).empty() && nameFilterModel->rowCount() > 0) {
    qDebug("Nothing is selected, selecting first row: %s", qPrintable(nameFilterModel->index(0, TorrentModelItem::TR_NAME).data().toString()));
    selectionModel()->setCurrentIndex(nameFilterModel->index(0, TorrentModelItem::TR_NAME), QItemSelectionModel::SelectCurrent|QItemSelectionModel::Rows);
  }
}

