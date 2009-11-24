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
#include "bittorrent.h"
#include "torrentpersistentdata.h"
#include "transferlistdelegate.h"
#include "previewselect.h"
#include "speedlimitdlg.h"
#include "options_imp.h"
#include "GUI.h"
#include "deletionconfirmationdlg.h"
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QDesktopServices>
#include <QTimer>
#include <QSettings>
#include <QClipboard>
#include <QColor>
#include <QUrl>
#include <QMenu>
#include <QRegExp>
#include <vector>

TransferListWidget::TransferListWidget(QWidget *parent, GUI *main_window, Bittorrent *_BTSession):
    QTreeView(parent), BTSession(_BTSession), main_window(main_window) {

  QSettings settings("qBittorrent", "qBittorrent");
  // Create and apply delegate
  listDelegate = new TransferListDelegate(this);
  setItemDelegate(listDelegate);

  // Create transfer list model
  listModel = new QStandardItemModel(0,12);
  listModel->setHeaderData(TR_NAME, Qt::Horizontal, tr("Name", "i.e: torrent name"));
  listModel->setHeaderData(TR_PRIORITY, Qt::Horizontal, "#");
  listModel->horizontalHeaderItem(TR_PRIORITY)->setTextAlignment(Qt::AlignRight);
  listModel->setHeaderData(TR_SIZE, Qt::Horizontal, tr("Size", "i.e: torrent size"));
  listModel->horizontalHeaderItem(TR_SIZE)->setTextAlignment(Qt::AlignRight);
  listModel->setHeaderData(TR_PROGRESS, Qt::Horizontal, tr("Done", "% Done"));
  listModel->horizontalHeaderItem(TR_PROGRESS)->setTextAlignment(Qt::AlignHCenter);
  listModel->setHeaderData(TR_STATUS, Qt::Horizontal, tr("Status", "Torrent status (e.g. downloading, seeding, paused)"));
  listModel->setHeaderData(TR_SEEDS, Qt::Horizontal, tr("Seeds", "i.e. full sources (often untranslated)"));
  listModel->horizontalHeaderItem(TR_SEEDS)->setTextAlignment(Qt::AlignRight);
  listModel->setHeaderData(TR_PEERS, Qt::Horizontal, tr("Peers", "i.e. partial sources (often untranslated)"));
  listModel->horizontalHeaderItem(TR_PEERS)->setTextAlignment(Qt::AlignRight);
  listModel->setHeaderData(TR_DLSPEED, Qt::Horizontal, tr("Down Speed", "i.e: Download speed"));
  listModel->horizontalHeaderItem(TR_DLSPEED)->setTextAlignment(Qt::AlignRight);
  listModel->setHeaderData(TR_UPSPEED, Qt::Horizontal, tr("Up Speed", "i.e: Upload speed"));;
  listModel->horizontalHeaderItem(TR_UPSPEED)->setTextAlignment(Qt::AlignRight);
  listModel->setHeaderData(TR_RATIO, Qt::Horizontal, tr("Ratio", "Share ratio"));
  listModel->horizontalHeaderItem(TR_RATIO)->setTextAlignment(Qt::AlignRight);
  listModel->setHeaderData(TR_ETA, Qt::Horizontal, tr("ETA", "i.e: Estimated Time of Arrival / Time left"));

  // Set Sort/Filter proxy
  proxyModel = new QSortFilterProxyModel();
  proxyModel->setDynamicSortFilter(true);
  proxyModel->setSourceModel(listModel);
  proxyModel->setFilterKeyColumn(TR_STATUS);
  proxyModel->setFilterRole(Qt::DisplayRole);
  setModel(proxyModel);

  // Visual settings
  setRootIsDecorated(false);
  setAllColumnsShowFocus(true);
  setSortingEnabled(true);
  setSelectionMode(QAbstractItemView::ExtendedSelection);
  setItemsExpandable(false);
  setAutoScroll(true);

  hideColumn(TR_PRIORITY);
  hideColumn(TR_HASH);
  loadHiddenColumns();
  // Load last columns width for transfer list
  if(!loadColWidthList()) {
    header()->resizeSection(0, 200);
  }
  loadLastSortedColumn();
  setContextMenuPolicy(Qt::CustomContextMenu);

  // Listen for BTSession events
  connect(BTSession, SIGNAL(addedTorrent(QTorrentHandle&)), this, SLOT(addTorrent(QTorrentHandle&)));
  connect(BTSession, SIGNAL(finishedTorrent(QTorrentHandle&)), this, SLOT(setFinished(QTorrentHandle&)));
  connect(BTSession, SIGNAL(metadataReceived(QTorrentHandle&)), this, SLOT(updateMetadata(QTorrentHandle&)));
  connect(BTSession, SIGNAL(pausedTorrent(QTorrentHandle&)), this, SLOT(pauseTorrent(QTorrentHandle&)));
  connect(BTSession, SIGNAL(resumedTorrent(QTorrentHandle&)), this, SLOT(resumeTorrent(QTorrentHandle&)));

  // Listen for list events
  connect(this, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(torrentDoubleClicked(QModelIndex)));
  connect(this, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayListMenu(const QPoint&)));
  header()->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(header(), SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayDLHoSMenu(const QPoint&)));

  // Refresh timer
  refreshTimer = new QTimer();
  refreshTimer->start(settings.value("Preferences/General/RefreshInterval", 1500).toInt());
  connect(refreshTimer, SIGNAL(timeout()), this, SLOT(refreshList()));
}

TransferListWidget::~TransferListWidget() {
  // Save settings
  saveLastSortedColumn();
  saveColWidthList();
  saveHiddenColumns();
  // Clean up
  delete refreshTimer;
  delete proxyModel;
  delete listModel;
  delete listDelegate;
}

void TransferListWidget::addTorrent(QTorrentHandle& h) {
  if(!h.is_valid()) return;
  // Check that the torrent is not already there
  if(getRowFromHash(h.hash()) >= 0) return;
  // Actuall add the torrent
  int row = listModel->rowCount();
  try {
    // Adding torrent to transfer list
    listModel->insertRow(row);
    listModel->setData(listModel->index(row, TR_NAME), QVariant(h.name()));
    listModel->setData(listModel->index(row, TR_SIZE), QVariant((qlonglong)h.actual_size()));
    listModel->setData(listModel->index(row, TR_ETA), QVariant((qlonglong)-1));
    listModel->setData(listModel->index(row, TR_SEEDS), QVariant((double)0.0));
    listModel->setData(listModel->index(row, TR_PEERS), QVariant((double)0.0));
    if(BTSession->isQueueingEnabled())
      listModel->setData(listModel->index(row, TR_PRIORITY), QVariant((int)h.queue_position()));
    listModel->setData(listModel->index(row, TR_HASH), QVariant(h.hash()));
    // Pause torrent if it is
    if(h.is_paused()) {
      if(h.is_seed()) {
        listModel->setData(listModel->index(row, TR_STATUS), STATE_PAUSED_UP);
        listModel->setData(listModel->index(row, TR_NAME), QVariant(QIcon(QString::fromUtf8(":/Icons/skin/pausedUP.png"))), Qt::DecorationRole);
      } else {
        listModel->setData(listModel->index(row, TR_STATUS), STATE_PAUSED_DL);
        listModel->setData(listModel->index(row, TR_NAME), QVariant(QIcon(QString::fromUtf8(":/Icons/skin/pausedDL.png"))), Qt::DecorationRole);
      }
      //setRowColor(row, QString::fromUtf8("red"));
    }else{
      if(h.is_seed()) {
        listModel->setData(listModel->index(row, TR_NAME), QVariant(QIcon(QString::fromUtf8(":/Icons/skin/stalledUP.png"))), Qt::DecorationRole);
        listModel->setData(listModel->index(row, TR_STATUS), STATE_STALLED_UP);
      } else {
        listModel->setData(listModel->index(row, TR_NAME), QVariant(QIcon(QString::fromUtf8(":/Icons/skin/stalledDL.png"))), Qt::DecorationRole);
        listModel->setData(listModel->index(row, TR_STATUS), STATE_STALLED_DL);
      }
      //setRowColor(row, QString::fromUtf8("grey"));
    }
    // Select first torrent to be added
    if(listModel->rowCount() == 1)
      selectionModel()->setCurrentIndex(proxyModel->index(row, TR_NAME), QItemSelectionModel::SelectCurrent|QItemSelectionModel::Rows);
    refreshList();
  } catch(invalid_handle e) {
    // Remove added torrent
    listModel->removeRow(row);
  }
}

/*void TransferListWidget::setRowColor(int row, QColor color) {
  unsigned int nbColumns = listModel->columnCount()-1;
  for(unsigned int i=0; i<nbColumns; ++i) {
    listModel->setData(listModel->index(row, i), QVariant(color), Qt::ForegroundRole);
  }
}*/

void TransferListWidget::deleteTorrent(int row, bool refresh_list) {
  listModel->removeRow(row);
  if(refresh_list)
    refreshList();
}

// Wrapper slot for bittorrent signal
void TransferListWidget::pauseTorrent(QTorrentHandle &h) {
  pauseTorrent(getRowFromHash(h.hash()));
}

void TransferListWidget::pauseTorrent(int row, bool refresh_list) {
  QTorrentHandle h = BTSession->getTorrentHandle(getHashFromRow(row));
  listModel->setData(listModel->index(row, TR_DLSPEED), QVariant((double)0.0));
  listModel->setData(listModel->index(row, TR_UPSPEED), QVariant((double)0.0));
  listModel->setData(listModel->index(row, TR_ETA), QVariant((qlonglong)-1));
  if(h.is_seed()) {
    listModel->setData(listModel->index(row, TR_STATUS), STATE_PAUSED_UP);
    listModel->setData(listModel->index(row, TR_NAME), QIcon(QString::fromUtf8(":/Icons/skin/pausedUP.png")), Qt::DecorationRole);
  } else {
    listModel->setData(listModel->index(row, TR_STATUS), STATE_PAUSED_DL);
    listModel->setData(listModel->index(row, TR_NAME), QIcon(QString::fromUtf8(":/Icons/skin/pausedDL.png")), Qt::DecorationRole);
  }
  listModel->setData(listModel->index(row, TR_SEEDS), QVariant(0.0));
  listModel->setData(listModel->index(row, TR_PEERS), QVariant(0.0));
  //setRowColor(row, QString::fromUtf8("red"));
  if(refresh_list)
    refreshList();
}

// Wrapper slot for bittorrent signal
void TransferListWidget::resumeTorrent(QTorrentHandle &h) {
  resumeTorrent(getRowFromHash(h.hash()));
}

void TransferListWidget::resumeTorrent(int row, bool refresh_list) {
  QTorrentHandle h = BTSession->getTorrentHandle(getHashFromRow(row));
  if(!h.is_valid()) return;
  if(h.is_seed()) {
    listModel->setData(listModel->index(row, TR_NAME), QVariant(QIcon(":/Icons/skin/stalledUP.png")), Qt::DecorationRole);
    listModel->setData(listModel->index(row, TR_STATUS), STATE_STALLED_UP);
  } else {
    listModel->setData(listModel->index(row, TR_NAME), QVariant(QIcon(":/Icons/skin/stalledDL.png")), Qt::DecorationRole);
    listModel->setData(listModel->index(row, TR_STATUS), STATE_STALLED_DL);
  }
  if(refresh_list)
    refreshList();
}

void TransferListWidget::updateMetadata(QTorrentHandle &h) {
  QString hash = h.hash();
  int row = getRowFromHash(hash);
  if(row != -1) {
    qDebug("Updating torrent metadata in download list");
    listModel->setData(listModel->index(row, TR_NAME), QVariant(h.name()));
    listModel->setData(listModel->index(row, TR_SIZE), QVariant((qlonglong)h.actual_size()));
  }
}

int TransferListWidget::updateTorrent(int row) {
  TorrentState s = STATE_INVALID;
  QString hash = getHashFromRow(row);
  QTorrentHandle h = BTSession->getTorrentHandle(hash);
  if(!h.is_valid()) {
    // Delete torrent
    deleteTorrent(row, false);
    return s;
  }
  try {
    // Connected_seeds*100000+total_seeds*10 (if total_seeds is available)
    // Connected_seeds*100000+1 (if total_seeds is unavailable)
    qulonglong seeds = h.num_seeds()*1000000;
    if(h.num_complete() >= h.num_seeds())
      seeds += h.num_complete()*10;
    else
      seeds += 1;
    listModel->setData(listModel->index(row, TR_SEEDS), QVariant(seeds));
    qulonglong peers = (h.num_peers()-h.num_seeds())*1000000;
    if(h.num_incomplete() >= (h.num_peers()-h.num_seeds()))
      peers += h.num_incomplete()*10;
    else
      peers += 1;
    listModel->setData(listModel->index(row, TR_PEERS), QVariant(peers));
    // Update torrent size. It changes when files are filtered from torrent properties
    // or Web UI
    listModel->setData(listModel->index(row, TR_SIZE), QVariant((qlonglong)h.actual_size()));
    // Queueing code
    if(!h.is_seed() && BTSession->isQueueingEnabled()) {
      listModel->setData(listModel->index(row, TR_PRIORITY), QVariant((int)h.queue_position()));
      if(h.is_queued()) {
        if(h.state() == torrent_status::checking_files || h.state() == torrent_status::queued_for_checking) {
          listModel->setData(listModel->index(row, TR_PROGRESS), QVariant((double)h.progress()));
          if(h.is_seed()) {
            s = STATE_CHECKING_UP;
            listModel->setData(listModel->index(row, TR_NAME), QVariant(QIcon(QString::fromUtf8(":/Icons/skin/checkingUP.png"))), Qt::DecorationRole);
          } else {
            s = STATE_CHECKING_DL;
            listModel->setData(listModel->index(row, TR_NAME), QVariant(QIcon(QString::fromUtf8(":/Icons/skin/checkingDL.png"))), Qt::DecorationRole);
          }
          listModel->setData(listModel->index(row, TR_STATUS), s);
        }else {
          listModel->setData(listModel->index(row, TR_ETA), QVariant((qlonglong)-1));
          if(h.is_seed()) {
            s = STATE_QUEUED_UP;            
            listModel->setData(listModel->index(row, TR_NAME), QVariant(QIcon(QString::fromUtf8(":/Icons/skin/queuedUP.png"))), Qt::DecorationRole);
          } else {
            s =STATE_QUEUED_DL;
            listModel->setData(listModel->index(row, TR_NAME), QVariant(QIcon(QString::fromUtf8(":/Icons/skin/queuedDL.png"))), Qt::DecorationRole);
          }
          listModel->setData(listModel->index(row, TR_STATUS), s);
        }
        // Reset speeds and seeds/leech
        listModel->setData(listModel->index(row, TR_DLSPEED), QVariant((double)0.));
        listModel->setData(listModel->index(row, TR_UPSPEED), QVariant((double)0.));
        listModel->setData(listModel->index(row, TR_SEEDS), QVariant(0.0));
        listModel->setData(listModel->index(row, TR_PEERS), QVariant(0.0));
        //setRowColor(row, QString::fromUtf8("grey"));
        return s;
      }
    }

    if(h.is_paused()) {
      if(h.is_seed())
        return STATE_PAUSED_UP;
      return STATE_PAUSED_DL;
    }

    // Parse download state
    switch(h.state()) {
    case torrent_status::allocating:
    case torrent_status::checking_files:
    case torrent_status::queued_for_checking:
    case torrent_status::checking_resume_data:
      if(h.is_seed()) {
        s = STATE_CHECKING_UP;

      listModel->setData(listModel->index(row, TR_NAME), QVariant(QIcon(QString::fromUtf8(":/Icons/skin/checkingUP.png"))), Qt::DecorationRole);
      } else {
        s = STATE_CHECKING_DL;

      listModel->setData(listModel->index(row, TR_NAME), QVariant(QIcon(QString::fromUtf8(":/Icons/skin/checkingDL.png"))), Qt::DecorationRole);
      }
      listModel->setData(listModel->index(row, TR_PROGRESS), QVariant((double)h.progress()));
      listModel->setData(listModel->index(row, TR_ETA), QVariant((qlonglong)-1));
      //setRowColor(row, QString::fromUtf8("grey"));
      break;
    case torrent_status::downloading:
    case torrent_status::downloading_metadata:
      if(h.download_payload_rate() > 0) {
        listModel->setData(listModel->index(row, TR_NAME), QVariant(QIcon(QString::fromUtf8(":/Icons/skin/downloading.png"))), Qt::DecorationRole);
        listModel->setData(listModel->index(row, TR_ETA), QVariant((qlonglong)BTSession->getETA(hash)));
        s = STATE_DOWNLOADING;
        //setRowColor(row, QString::fromUtf8("green"));
      }else{
        listModel->setData(listModel->index(row, TR_NAME), QVariant(QIcon(QString::fromUtf8(":/Icons/skin/stalledDL.png"))), Qt::DecorationRole);
        listModel->setData(listModel->index(row, TR_ETA), QVariant((qlonglong)-1));
        s = STATE_STALLED_DL;
        //setRowColor(row, QApplication::palette().color(QPalette::WindowText));
      }
      listModel->setData(listModel->index(row, TR_DLSPEED), QVariant((double)h.download_payload_rate()));
      listModel->setData(listModel->index(row, TR_PROGRESS), QVariant((double)h.progress()));
      break;
    case torrent_status::finished:
    case torrent_status::seeding:
      if(h.upload_payload_rate() > 0) {
        s = STATE_SEEDING;
      } else {
        s = STATE_STALLED_UP;
      }
    }
    // Common to both downloads and uploads
    listModel->setData(listModel->index(row, TR_STATUS), s);
    listModel->setData(listModel->index(row, TR_UPSPEED), QVariant((double)h.upload_payload_rate()));
    // Share ratio
    listModel->setData(listModel->index(row, TR_RATIO), QVariant(BTSession->getRealRatio(hash)));
  }catch(invalid_handle e) {
    deleteTorrent(row, false);
    s = STATE_INVALID;
    qDebug("Caught Invalid handle exception, lucky us.");
  }
  return s;
}

void TransferListWidget::setFinished(QTorrentHandle &h) {
  int row = -1;
  try {
    row = getRowFromHash(h.hash());
    if(row >= 0) {
      if(h.is_paused()) {
        listModel->setData(listModel->index(row, TR_NAME), QIcon(":/Icons/skin/pausedUP.png"), Qt::DecorationRole);
        listModel->setData(listModel->index(row, TR_STATUS), STATE_PAUSED_UP);
        //setRowColor(row, "red");
      }else{
        listModel->setData(listModel->index(row, TR_NAME), QVariant(QIcon(":/Icons/skin/stalledUP.png")), Qt::DecorationRole);
        listModel->setData(listModel->index(row, TR_STATUS), STATE_STALLED_UP);
        //setRowColor(row, "orange");
      }
      listModel->setData(listModel->index(row, TR_ETA), QVariant((qlonglong)-1));
      listModel->setData(listModel->index(row, TR_DLSPEED), QVariant((double)0.));
      listModel->setData(listModel->index(row, TR_PROGRESS), QVariant((double)1.));
      listModel->setData(listModel->index(row, TR_PRIORITY), QVariant((int)-1));
    }
  } catch(invalid_handle e) {
    if(row >= 0) deleteTorrent(row);
  }
}

void TransferListWidget::setRefreshInterval(int t) {
  qDebug("Settings transfer list refresh interval to %dms", t);
  refreshTimer->start(t);
}

void TransferListWidget::refreshList() {
  // Refresh only if displayed
  if(main_window->getCurrentTabIndex() != TAB_TRANSFER) return;
  unsigned int nb_downloading = 0, nb_seeding=0, nb_active=0, nb_inactive = 0;
  for(int i=0; i<listModel->rowCount(); ++i) {
    int s = updateTorrent(i);
    switch(s) {
    case STATE_DOWNLOADING:
      ++nb_active;
      ++nb_downloading;
      break;
    case STATE_STALLED_DL:
    case STATE_CHECKING_DL:
    case STATE_PAUSED_DL:
    case STATE_QUEUED_DL:
      ++nb_inactive;
      ++nb_downloading;
      break;
    case STATE_SEEDING:
      ++nb_active;
      ++nb_seeding;
      break;
    case STATE_STALLED_UP:
    case STATE_CHECKING_UP:
    case STATE_PAUSED_UP:
    case STATE_QUEUED_UP:
      ++nb_seeding;
      ++nb_inactive;
      break;
    default:
      break;
    }
  }
  emit torrentStatusUpdate(nb_downloading, nb_seeding, nb_active, nb_inactive);
  repaint();
}

int TransferListWidget::getRowFromHash(QString hash) const{
  QList<QStandardItem *> items = listModel->findItems(hash, Qt::MatchExactly, TR_HASH);
  if(items.empty()) return -1;
  Q_ASSERT(items.size() == 1);
  return items.first()->row();
}

QString TransferListWidget::getHashFromRow(int row) const {
  return listModel->data(listModel->index(row, TR_HASH)).toString();
}

void TransferListWidget::torrentDoubleClicked(QModelIndex index) {
  int row = proxyModel->mapToSource(index).row();
  QString hash = getHashFromRow(row);
  QTorrentHandle h = BTSession->getTorrentHandle(hash);
  if(!h.is_valid()) return;
  int action;
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  if(h.is_seed()) {
    action =  settings.value(QString::fromUtf8("Preferences/Downloads/DblClOnTorFN"), 0).toInt();
  } else {
    action = settings.value(QString::fromUtf8("Preferences/Downloads/DblClOnTorDl"), 0).toInt();
  }

  switch(action) {
  case TOGGLE_PAUSE:
    if(h.is_paused()) {
      h.resume();
      resumeTorrent(row);
    } else {
      h.pause();
      pauseTorrent(row);
    }
    break;
  case OPEN_DEST:
    QDesktopServices::openUrl(QUrl(h.save_path()));
    break;
  }
}

void TransferListWidget::startSelectedTorrents() {
  QModelIndexList selectedIndexes = selectionModel()->selectedRows();
  foreach(const QModelIndex &index, selectedIndexes) {
    // Get the file hash
    int row = proxyModel->mapToSource(index).row();
    QString hash = getHashFromRow(row);
    QTorrentHandle h = BTSession->getTorrentHandle(hash);
    if(h.is_valid() && h.is_paused()) {
      h.resume();
      resumeTorrent(row, false);
    }
  }
  if(!selectedIndexes.empty())
    refreshList();
}

void TransferListWidget::startAllTorrents() {
  for(int i=0; i<listModel->rowCount(); ++i) {
    QTorrentHandle h = BTSession->getTorrentHandle(getHashFromRow(i));
    if(h.is_valid() && h.is_paused()) {
      h.resume();
      resumeTorrent(i, false);
    }
  }
  refreshList();
}

void TransferListWidget::pauseSelectedTorrents() {
  QModelIndexList selectedIndexes = selectionModel()->selectedRows();
  foreach(const QModelIndex &index, selectedIndexes) {
    // Get the file hash
    int row = proxyModel->mapToSource(index).row();
    QString hash = getHashFromRow(row);
    QTorrentHandle h = BTSession->getTorrentHandle(hash);
    if(h.is_valid() && !h.is_paused()) {
      h.pause();
      pauseTorrent(row, false);
    }
  }
  if(!selectedIndexes.empty())
    refreshList();
}

void TransferListWidget::pauseAllTorrents() {
  for(int i=0; i<listModel->rowCount(); ++i) {
    QTorrentHandle h = BTSession->getTorrentHandle(getHashFromRow(i));
    if(h.is_valid() && !h.is_paused()) {
      h.pause();
      pauseTorrent(i, false);
    }
  }
  refreshList();
}

void TransferListWidget::deleteSelectedTorrents() {
  if(main_window->getCurrentTabIndex() != TAB_TRANSFER) return;
  QModelIndexList selectedIndexes = selectionModel()->selectedRows();
  if(!selectedIndexes.empty()) {
    bool delete_local_files = false;
    if(DeletionConfirmationDlg::askForDeletionConfirmation(&delete_local_files)) {
      QStringList hashes;
      foreach(const QModelIndex &index, selectedIndexes) {
        // Get the file hash
        hashes << getHashFromRow(proxyModel->mapToSource(index).row());
      }
      foreach(const QString &hash, hashes) {
        deleteTorrent(getRowFromHash(hash), false);
        BTSession->deleteTorrent(hash, delete_local_files);
      }
      refreshList();
    }
  }
}

// FIXME: Should work only if the tab is displayed
void TransferListWidget::increasePrioSelectedTorrents() {
  QModelIndexList selectedIndexes = selectionModel()->selectedRows();
  foreach(const QModelIndex &index, selectedIndexes) {
    QTorrentHandle h = BTSession->getTorrentHandle(getHashFromRow(proxyModel->mapToSource(index).row()));
    if(h.is_valid() && !h.is_seed()) {
      h.queue_position_up();
    }
  }
  refreshList();
}

// FIXME: Should work only if the tab is displayed
void TransferListWidget::decreasePrioSelectedTorrents() {
  QModelIndexList selectedIndexes = selectionModel()->selectedRows();
  foreach(const QModelIndex &index, selectedIndexes) {
    QTorrentHandle h = BTSession->getTorrentHandle(getHashFromRow(proxyModel->mapToSource(index).row()));
    if(h.is_valid() && !h.is_seed()) {
      h.queue_position_down();
    }
  }
  refreshList();
}

void TransferListWidget::buySelectedTorrents() const {
  QModelIndexList selectedIndexes = selectionModel()->selectedRows();
  foreach(const QModelIndex &index, selectedIndexes) {
    QTorrentHandle h = BTSession->getTorrentHandle(getHashFromRow(proxyModel->mapToSource(index).row()));
    if(h.is_valid())
      QDesktopServices::openUrl("http://match.sharemonkey.com/?info_hash="+h.hash()+"&n="+h.name()+"&cid=33");
  }
}

void TransferListWidget::copySelectedMagnetURIs() const {
  QModelIndexList selectedIndexes = selectionModel()->selectedRows();
  QStringList magnet_uris;
  foreach(const QModelIndex &index, selectedIndexes) {
    QTorrentHandle h = BTSession->getTorrentHandle(getHashFromRow(proxyModel->mapToSource(index).row()));
    if(h.is_valid())
      magnet_uris << misc::toQString(make_magnet_uri(h.get_torrent_info()));
  }
  qApp->clipboard()->setText(magnet_uris.join("\n"));
}

void TransferListWidget::hidePriorityColumn(bool hide) {
  setColumnHidden(TR_PRIORITY, hide);
}

void TransferListWidget::openSelectedTorrentsFolder() const {
  QModelIndexList selectedIndexes = selectionModel()->selectedRows();
  QStringList pathsList;
  foreach(const QModelIndex &index, selectedIndexes) {
    QTorrentHandle h = BTSession->getTorrentHandle(getHashFromRow(proxyModel->mapToSource(index).row()));
    if(h.is_valid()) {
      QString savePath = h.save_path();
      if(!pathsList.contains(savePath)) {
        pathsList.append(savePath);
        QDesktopServices::openUrl(QUrl(QString("file://")+savePath));
      }
    }
  }
}

void TransferListWidget::previewSelectedTorrents() {
  QModelIndexList selectedIndexes = selectionModel()->selectedRows();
  QStringList pathsList;
  foreach(const QModelIndex &index, selectedIndexes) {
    QTorrentHandle h = BTSession->getTorrentHandle(getHashFromRow(proxyModel->mapToSource(index).row()));
    if(h.is_valid()) {
      new previewSelect(this, h);
    }
  }
}

void TransferListWidget::setDlLimitSelectedTorrents() {
  QModelIndexList selectedIndexes = selectionModel()->selectedRows();
  QStringList hashes;
  QList<QTorrentHandle> selected_torrents;
  bool first = true;
  bool all_same_limit = true;
  foreach(const QModelIndex &index, selectedIndexes) {
    // Get the file hash
    QString hash = getHashFromRow(proxyModel->mapToSource(index).row());
    QTorrentHandle h = BTSession->getTorrentHandle(hash);
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
  long new_limit = SpeedLimitDialog::askSpeedLimit(&ok, tr("Torrent Download Speed Limiting"), default_limit);
  if(ok) {
    foreach(QTorrentHandle h, selected_torrents) {
      qDebug("Applying download speed limit of %ld Kb/s to torrent %s", (long)(new_limit/1024.), h.hash().toLocal8Bit().data());
      BTSession->setDownloadLimit(h.hash(), new_limit);
    }
  }
}

void TransferListWidget::setUpLimitSelectedTorrents() {
  QModelIndexList selectedIndexes = selectionModel()->selectedRows();
  QStringList hashes;
  QList<QTorrentHandle> selected_torrents;
  bool first = true;
  bool all_same_limit = true;
  foreach(const QModelIndex &index, selectedIndexes) {
    // Get the file hash
    QString hash = getHashFromRow(proxyModel->mapToSource(index).row());
    QTorrentHandle h = BTSession->getTorrentHandle(hash);
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
  long new_limit = SpeedLimitDialog::askSpeedLimit(&ok, tr("Torrent Upload Speed Limiting"), default_limit);
  if(ok) {
    foreach(QTorrentHandle h, selected_torrents) {
      qDebug("Applying upload speed limit of %ld Kb/s to torrent %s", (long)(new_limit/1024.), h.hash().toLocal8Bit().data());
      BTSession->setUploadLimit(h.hash(), new_limit);
    }
  }
}

void TransferListWidget::recheckSelectedTorrents() {
  QModelIndexList selectedIndexes = selectionModel()->selectedRows();
  foreach(const QModelIndex &index, selectedIndexes){
    QString hash = getHashFromRow(proxyModel->mapToSource(index).row());
    QTorrentHandle h = BTSession->getTorrentHandle(hash);
    if(h.is_valid() && h.has_metadata())
      h.force_recheck();
  }
}

// save the hidden columns in settings
void TransferListWidget::saveHiddenColumns() {
  QSettings settings("qBittorrent", "qBittorrent");
  QStringList ishidden_list;
  short nbColumns = listModel->columnCount()-1;//hash column is hidden

  for(short i=0; i<nbColumns; ++i){
    if(isColumnHidden(i)) {
      ishidden_list << "0";
    } else {
      ishidden_list << "1";
    }
  }
  settings.setValue("TransferListColsHoS", ishidden_list.join(" "));
}

// load the previous settings, and hide the columns
bool TransferListWidget::loadHiddenColumns() {
  QSettings settings("qBittorrent", "qBittorrent");
  QString line = settings.value("TransferListColsHoS", QString()).toString();
  if(line.isEmpty()) return false;
  bool loaded = false;
  QStringList ishidden_list;
  ishidden_list = line.split(' ');
  unsigned int nbCol = ishidden_list.size();
  if(nbCol == (unsigned int)listModel->columnCount()-1) {
    for(unsigned int i=0; i<nbCol; ++i){
      if(ishidden_list.at(i) == "0") {
        setColumnHidden(i, true);
      }
    }
    loaded = true;
  }
  return loaded;
}

// hide/show columns menu
void TransferListWidget::displayDLHoSMenu(const QPoint&){
  QMenu hideshowColumn(this);
  hideshowColumn.setTitle(tr("Column visibility"));
  QList<QAction*> actions;
  for(int i=0; i < TR_HASH; ++i) {
    if(!BTSession->isQueueingEnabled() && i == TR_PRIORITY) continue;
    QIcon icon;
    if(isColumnHidden(i))
      icon = QIcon(QString::fromUtf8(":/Icons/oxygen/button_cancel.png"));
    else
      icon = QIcon(QString::fromUtf8(":/Icons/oxygen/button_ok.png"));
    actions.append(hideshowColumn.addAction(icon, listModel->headerData(i, Qt::Horizontal).toString()));
  }
  // Call menu
  QAction *act = hideshowColumn.exec(QCursor::pos());
  int col = actions.indexOf(act);
  setColumnHidden(col, !isColumnHidden(col));
}

#ifdef LIBTORRENT_0_15
void TransferListWidget::toggleSelectedTorrentsSuperSeeding() {
  QModelIndexList selectedIndexes = selectionModel()->selectedRows();
  foreach(const QModelIndex &index, selectedIndexes) {
    // Get the file hash
    QString hash = getHashFromRow(proxyModel->mapToSource(index).row());
    QTorrentHandle h = BTSession->getTorrentHandle(hash);
    if(h.is_valid()) {
      h.super_seeding(!h.super_seeding());
    }
  }
}
#endif

void TransferListWidget::toggleSelectedTorrentsSequentialDownload() {
  QModelIndexList selectedIndexes = selectionModel()->selectedRows();
  foreach(const QModelIndex &index, selectedIndexes) {
    // Get the file hash
    QString hash = getHashFromRow(proxyModel->mapToSource(index).row());
    QTorrentHandle h = BTSession->getTorrentHandle(hash);
    if(h.is_valid()) {
      h.set_sequential_download(!h.is_sequential_download());
    }
  }
}

void TransferListWidget::toggleSelectedFirstLastPiecePrio() {
  QModelIndexList selectedIndexes = selectionModel()->selectedRows();
  foreach(const QModelIndex &index, selectedIndexes) {
    // Get the file hash
    QString hash = getHashFromRow(proxyModel->mapToSource(index).row());
    QTorrentHandle h = BTSession->getTorrentHandle(hash);
    if(h.is_valid() && h.num_files() == 1) {
      h.prioritize_first_last_piece(!h.first_last_piece_first());
    }
  }
}

void TransferListWidget::displayListMenu(const QPoint&) {
  // Create actions
  QAction actionStart(QIcon(QString::fromUtf8(":/Icons/skin/play.png")), tr("Start"), 0);
  connect(&actionStart, SIGNAL(triggered()), this, SLOT(startSelectedTorrents()));
  QAction actionPause(QIcon(QString::fromUtf8(":/Icons/skin/pause.png")), tr("Pause"), 0);
  connect(&actionPause, SIGNAL(triggered()), this, SLOT(pauseSelectedTorrents()));
  QAction actionDelete(QIcon(QString::fromUtf8(":/Icons/skin/delete.png")), tr("Delete"), 0);
  connect(&actionDelete, SIGNAL(triggered()), this, SLOT(deleteSelectedTorrents()));
  QAction actionPreview_file(QIcon(QString::fromUtf8(":/Icons/skin/preview.png")), tr("Preview file"), 0);
  connect(&actionPreview_file, SIGNAL(triggered()), this, SLOT(previewSelectedTorrents()));
  QAction actionSet_upload_limit(QIcon(QString::fromUtf8(":/Icons/skin/seeding.png")), tr("Limit upload rate"), 0);
  connect(&actionSet_upload_limit, SIGNAL(triggered()), this, SLOT(setUpLimitSelectedTorrents()));
  QAction actionSet_download_limit(QIcon(QString::fromUtf8(":/Icons/skin/downloading.png")), tr("Limit download rate"), 0);
  connect(&actionSet_download_limit, SIGNAL(triggered()), this, SLOT(setDlLimitSelectedTorrents()));
  QAction actionOpen_destination_folder(QIcon(QString::fromUtf8(":/Icons/oxygen/folder.png")), tr("Open destination folder"), 0);
  connect(&actionOpen_destination_folder, SIGNAL(triggered()), this, SLOT(openSelectedTorrentsFolder()));
  QAction actionBuy_it(QIcon(QString::fromUtf8(":/Icons/oxygen/wallet.png")), tr("Buy it"), 0);
  connect(&actionBuy_it, SIGNAL(triggered()), this, SLOT(buySelectedTorrents()));
  QAction actionIncreasePriority(QIcon(QString::fromUtf8(":/Icons/skin/increase.png")), tr("Increase priority"), 0);
  connect(&actionIncreasePriority, SIGNAL(triggered()), this, SLOT(increasePrioSelectedTorrents()));
  QAction actionDecreasePriority(QIcon(QString::fromUtf8(":/Icons/skin/decrease.png")), tr("Decrease priority"), 0);
  connect(&actionDecreasePriority, SIGNAL(triggered()), this, SLOT(decreasePrioSelectedTorrents()));
  QAction actionForce_recheck(QIcon(QString::fromUtf8(":/Icons/oxygen/gear.png")), tr("Force recheck"), 0);
  connect(&actionForce_recheck, SIGNAL(triggered()), this, SLOT(recheckSelectedTorrents()));
  QAction actionCopy_magnet_link(QIcon(QString::fromUtf8(":/Icons/magnet.png")), tr("Copy magnet link"), 0);
  connect(&actionCopy_magnet_link, SIGNAL(triggered()), this, SLOT(copySelectedMagnetURIs()));
  QAction actionSuper_seeding_mode(tr("Super seeding mode"), 0);
  connect(&actionSuper_seeding_mode, SIGNAL(triggered()), this, SLOT(toggleSelectedTorrentsSuperSeeding()));
  QAction actionSequential_download(tr("Download in sequential order"), 0);
  connect(&actionSequential_download, SIGNAL(triggered()), this, SLOT(toggleSelectedTorrentsSequentialDownload()));
  QAction actionFirstLastPiece_prio(tr("Download first and last piece first"), 0);
  connect(&actionFirstLastPiece_prio, SIGNAL(triggered()), this, SLOT(toggleSelectedFirstLastPiecePrio()));
  // End of actions
  QMenu listMenu(this);
  // Enable/disable pause/start action given the DL state
  QModelIndexList selectedIndexes = selectionModel()->selectedRows();
  bool has_pause = false, has_start = false, has_preview = false;
#ifdef LIBTORRENT_0_15
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
    QString hash = getHashFromRow(proxyModel->mapToSource(index).row());
    // Get handle and pause the torrent
    h = BTSession->getTorrentHandle(hash);
    if(!h.is_valid()) continue;
    if(h.has_metadata())
      one_has_metadata = true;
    if(!h.is_seed()) {
      one_not_seed = true;
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
#ifdef LIBTORRENT_0_15
    else {
      if(!one_not_seed && all_same_super_seeding) {
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
  if(one_not_seed)
    listMenu.addAction(&actionSet_download_limit);
  listMenu.addAction(&actionSet_upload_limit);
#ifdef LIBTORRENT_0_15
  if(!one_not_seed && all_same_super_seeding) {
    QIcon ico;
    if(super_seeding_mode) {
      ico = QIcon(":/Icons/oxygen/button_ok.png");
    } else {
      ico = QIcon(":/Icons/oxygen/button_cancel.png");
    }
    actionSuper_seeding_mode.setIcon(ico);
    listMenu.addAction(&actionSuper_seeding_mode);
  }
#endif
  listMenu.addSeparator();
  bool added_preview_action = false;
  if(has_preview) {
    listMenu.addAction(&actionPreview_file);
    added_preview_action = true;
  }
  if(one_not_seed) {
    if(all_same_sequential_download_mode) {
      QIcon ico;
      if(sequential_download_mode) {
        ico = QIcon(":/Icons/oxygen/button_ok.png");
      } else {
        ico = QIcon(":/Icons/oxygen/button_cancel.png");
      }
      actionSequential_download.setIcon(ico);
      listMenu.addAction(&actionSequential_download);
      added_preview_action = true;
    }
    if(all_same_prio_firstlast) {
      QIcon ico;
      if(prioritize_first_last) {
        ico = QIcon(":/Icons/oxygen/button_ok.png");
      } else {
        ico = QIcon(":/Icons/oxygen/button_cancel.png");
      }
      actionFirstLastPiece_prio.setIcon(ico);
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
    listMenu.addAction(&actionIncreasePriority);
    listMenu.addAction(&actionDecreasePriority);
  }
  listMenu.addSeparator();
  listMenu.addAction(&actionCopy_magnet_link);
  listMenu.addAction(&actionBuy_it);
  // Call menu
  listMenu.exec(QCursor::pos());
}

// Save columns width in a file to remember them
void TransferListWidget::saveColWidthList() {
  qDebug("Saving columns width in transfer list");
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  QStringList width_list;
  QStringList new_width_list;
  short nbColumns = listModel->columnCount()-1; // HASH is hidden
  QString line = settings.value("TransferListColsWidth", QString()).toString();
  if(!line.isEmpty()) {
    width_list = line.split(' ');
  }
  for(short i=0; i<nbColumns; ++i){
    if(columnWidth(i)<1 && width_list.size() == nbColumns && width_list.at(i).toInt()>=1) {
      // load the former width
      new_width_list << width_list.at(i);
    } else if(columnWidth(i)>=1) {
      // usual case, save the current width
      new_width_list << misc::toQString(columnWidth(i));
    } else {
      // default width
      resizeColumnToContents(i);
      new_width_list << misc::toQString(columnWidth(i));
    }
  }
  settings.setValue(QString::fromUtf8("TransferListColsWidth"), new_width_list.join(QString::fromUtf8(" ")));
  QVariantList visualIndexes;
  for(int i=0; i<nbColumns; ++i) {
    visualIndexes.append(header()->visualIndex(i));
  }
  settings.setValue(QString::fromUtf8("TransferListVisualIndexes"), visualIndexes);
  qDebug("Download list columns width saved");
}

// Load columns width in a file that were saved previously
bool TransferListWidget::loadColWidthList() {
  qDebug("Loading columns width for download list");
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  QString line = settings.value(QString::fromUtf8("TransferListColsWidth"), QString()).toString();
  if(line.isEmpty())
    return false;
  QStringList width_list = line.split(QString::fromUtf8(" "));
  if(width_list.size() != listModel->columnCount()-1) {
    qDebug("Corrupted values for transfer list columns sizes");
    return false;
  }
  unsigned int listSize = width_list.size();
  for(unsigned int i=0; i<listSize; ++i) {
    header()->resizeSection(i, width_list.at(i).toInt());
  }
  QVariantList visualIndexes = settings.value(QString::fromUtf8("TransferListVisualIndexes"), QVariantList()).toList();
  if(visualIndexes.size() != listModel->columnCount()-1) {
    qDebug("Corrupted values for transfer list columns indexes");
    return false;
  }
  bool change = false;
  do {
    change = false;
    for(int i=0;i<visualIndexes.size(); ++i) {
      int new_visual_index = visualIndexes.at(header()->logicalIndex(i)).toInt();
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
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  Qt::SortOrder sortOrder = header()->sortIndicatorOrder();
  QString sortOrderLetter;
  if(sortOrder == Qt::AscendingOrder)
    sortOrderLetter = QString::fromUtf8("a");
  else
    sortOrderLetter = QString::fromUtf8("d");
  int index = header()->sortIndicatorSection();
  settings.setValue(QString::fromUtf8("TransferListSortedCol"), misc::toQString(index)+sortOrderLetter);
}

void TransferListWidget::loadLastSortedColumn() {
  // Loading last sorted column
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  QString sortedCol = settings.value(QString::fromUtf8("TransferListSortedCol"), QString()).toString();
  if(!sortedCol.isEmpty()) {
    Qt::SortOrder sortOrder;
    if(sortedCol.endsWith(QString::fromUtf8("d")))
      sortOrder = Qt::DescendingOrder;
    else
      sortOrder = Qt::AscendingOrder;
    sortedCol.chop(1);
    int index = sortedCol.toInt();
    sortByColumn(index, sortOrder);
  }
}

void TransferListWidget::currentChanged(const QModelIndex& current, const QModelIndex&) {
  QTorrentHandle h;
  if(current.isValid()) {
    int row = proxyModel->mapToSource(current).row();
    h = BTSession->getTorrentHandle(getHashFromRow(row));
  }
  emit currentTorrentChanged(h);
}

void TransferListWidget::applyFilter(int f) {
  switch(f) {
  case FILTER_DOWNLOADING:
    proxyModel->setFilterRegExp(QRegExp(QString::number(STATE_DOWNLOADING)+"|"+QString::number(STATE_STALLED_DL)+"|"+
                                        QString::number(STATE_PAUSED_DL)+"|"+QString::number(STATE_CHECKING_DL)+"|"+
                                        QString::number(STATE_QUEUED_DL), Qt::CaseSensitive));
    break;
  case FILTER_COMPLETED:
    proxyModel->setFilterRegExp(QRegExp(QString::number(STATE_SEEDING)+"|"+QString::number(STATE_STALLED_UP)+"|"+
                                        QString::number(STATE_PAUSED_UP)+"|"+QString::number(STATE_CHECKING_UP)+"|"+
                                        QString::number(STATE_QUEUED_UP), Qt::CaseSensitive));
    break;
  case FILTER_ACTIVE:
    proxyModel->setFilterRegExp(QRegExp(QString::number(STATE_DOWNLOADING)+"|"+QString::number(STATE_SEEDING), Qt::CaseSensitive));
    break;
  case FILTER_INACTIVE:
    proxyModel->setFilterRegExp(QRegExp("[^"+QString::number(STATE_DOWNLOADING)+QString::number(STATE_SEEDING)+"]", Qt::CaseSensitive));
    break;
  default:
    proxyModel->setFilterRegExp(QRegExp());
  }
  // Select first item if nothing is selected
  if(selectionModel()->selectedRows(0).empty() && proxyModel->rowCount() > 0)
    selectionModel()->setCurrentIndex(proxyModel->index(0, TR_NAME), QItemSelectionModel::SelectCurrent|QItemSelectionModel::Rows);
}

