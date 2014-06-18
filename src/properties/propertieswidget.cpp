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

#include <QTimer>
#include <QListWidgetItem>
#include <QVBoxLayout>
#include <QStackedWidget>
#include <QSplitter>
#include <QHeaderView>
#include <QAction>
#include <QMessageBox>
#include <QMenu>
#include <QFileDialog>
#include <QDesktopServices>
#include <libtorrent/version.hpp>
#include "propertieswidget.h"
#include "transferlistwidget.h"
#include "torrentpersistentdata.h"
#include "qbtsession.h"
#include "proplistdelegate.h"
#include "torrentcontentfiltermodel.h"
#include "torrentcontentmodel.h"
#include "peerlistwidget.h"
#include "trackerlist.h"
#include "mainwindow.h"
#include "downloadedpiecesbar.h"
#include "pieceavailabilitybar.h"
#include "qinisettings.h"
#include "proptabbar.h"
#include "iconprovider.h"
#include "lineedit.h"
#include "fs_utils.h"
#include "autoexpandabledialog.h"

using namespace libtorrent;

PropertiesWidget::PropertiesWidget(QWidget *parent, MainWindow* main_window, TransferListWidget *transferList):
  QWidget(parent), transferList(transferList), main_window(main_window) {
  setupUi(this);

  // Icons
  trackerUpButton->setIcon(IconProvider::instance()->getIcon("go-up"));
  trackerDownButton->setIcon(IconProvider::instance()->getIcon("go-down"));

  state = VISIBLE;

  // Set Properties list model
  PropListModel = new TorrentContentFilterModel();
  filesList->setModel(PropListModel);
  PropDelegate = new PropListDelegate(this);
  filesList->setItemDelegate(PropDelegate);
  filesList->setSortingEnabled(true);
  // Torrent content filtering
  m_contentFilerLine = new LineEdit(this);
  connect(m_contentFilerLine, SIGNAL(textChanged(QString)), this, SLOT(filterText(QString)));
  contentFilterLayout->insertWidget(1, m_contentFilerLine);

  // SIGNAL/SLOTS
  connect(filesList, SIGNAL(clicked(const QModelIndex&)), filesList, SLOT(edit(const QModelIndex&)));
  connect(selectAllButton, SIGNAL(clicked()), PropListModel, SLOT(selectAll()));
  connect(selectNoneButton, SIGNAL(clicked()), PropListModel, SLOT(selectNone()));
  connect(filesList, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayFilesListMenu(const QPoint&)));
  connect(filesList, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(openDoubleClickedFile(QModelIndex)));
  connect(PropListModel, SIGNAL(filteredFilesChanged()), this, SLOT(filteredFilesChanged()));
  connect(listWebSeeds, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayWebSeedListMenu(const QPoint&)));
  connect(transferList, SIGNAL(currentTorrentChanged(QTorrentHandle)), this, SLOT(loadTorrentInfos(QTorrentHandle)));
  connect(PropDelegate, SIGNAL(filteredFilesChanged()), this, SLOT(filteredFilesChanged()));
  connect(stackedProperties, SIGNAL(currentChanged(int)), this, SLOT(loadDynamicData()));
  connect(QBtSession::instance(), SIGNAL(savePathChanged(QTorrentHandle)), this, SLOT(updateSavePath(QTorrentHandle)));
  connect(QBtSession::instance(), SIGNAL(metadataReceived(QTorrentHandle)), this, SLOT(updateTorrentInfos(QTorrentHandle)));

  // Downloaded pieces progress bar
  downloaded_pieces = new DownloadedPiecesBar(this);
  ProgressHLayout->insertWidget(1, downloaded_pieces);
  // Pieces availability bar
  pieces_availability = new PieceAvailabilityBar(this);
  ProgressHLayout_2->insertWidget(1, pieces_availability);
  // Tracker list
  trackerList = new TrackerList(this);
  connect(trackerUpButton, SIGNAL(clicked()), trackerList, SLOT(moveSelectionUp()));
  connect(trackerDownButton, SIGNAL(clicked()), trackerList, SLOT(moveSelectionDown()));
  horizontalLayout_trackers->insertWidget(0, trackerList);
  // Peers list
  peersList = new PeerListWidget(this);
  peerpage_layout->addWidget(peersList);
  // Tab bar
  m_tabBar = new PropTabBar();
  verticalLayout->addLayout(m_tabBar);
  connect(m_tabBar, SIGNAL(tabChanged(int)), stackedProperties, SLOT(setCurrentIndex(int)));
  connect(m_tabBar, SIGNAL(visibilityToggled(bool)), SLOT(setVisibility(bool)));
  // Dynamic data refresher
  refreshTimer = new QTimer(this);
  connect(refreshTimer, SIGNAL(timeout()), this, SLOT(loadDynamicData()));
  refreshTimer->start(3000); // 3sec
  editHotkeyFile = new QShortcut(QKeySequence("F2"), filesList, 0, 0, Qt::WidgetShortcut);
  connect(editHotkeyFile, SIGNAL(activated()), SLOT(renameSelectedFile()));
  editHotkeyWeb = new QShortcut(QKeySequence("F2"), listWebSeeds, 0, 0, Qt::WidgetShortcut);
  connect(editHotkeyWeb, SIGNAL(activated()), SLOT(editWebSeed()));
  connect(listWebSeeds, SIGNAL(doubleClicked(QModelIndex)), SLOT(editWebSeed()));
  deleteHotkeyWeb = new QShortcut(QKeySequence(QKeySequence::Delete), listWebSeeds, 0, 0, Qt::WidgetShortcut);
  connect(deleteHotkeyWeb, SIGNAL(activated()), SLOT(deleteSelectedUrlSeeds()));
}

PropertiesWidget::~PropertiesWidget() {
  qDebug() << Q_FUNC_INFO << "ENTER";
  delete refreshTimer;
  delete trackerList;
  delete peersList;
  delete downloaded_pieces;
  delete pieces_availability;
  delete PropListModel;
  delete PropDelegate;
  delete m_tabBar;
  delete editHotkeyFile;
  delete editHotkeyWeb;
  delete deleteHotkeyWeb;
  qDebug() << Q_FUNC_INFO << "EXIT";
}

void PropertiesWidget::showPiecesAvailability(bool show) {
  avail_pieces_lbl->setVisible(show);
  pieces_availability->setVisible(show);
  avail_average_lbl->setVisible(show);
  if (show || (!show && !downloaded_pieces->isVisible()))
    line_2->setVisible(show);
}

void PropertiesWidget::showPiecesDownloaded(bool show) {
  downloaded_pieces_lbl->setVisible(show);
  downloaded_pieces->setVisible(show);
  progress_lbl->setVisible(show);
  if (show || (!show && !pieces_availability->isVisible()))
    line_2->setVisible(show);
}

void PropertiesWidget::setVisibility(bool visible) {
  if (!visible && state == VISIBLE) {
    QSplitter *hSplitter = static_cast<QSplitter*>(parentWidget());
    stackedProperties->setVisible(false);
    slideSizes = hSplitter->sizes();
    hSplitter->handle(1)->setVisible(false);
    hSplitter->handle(1)->setDisabled(true);
    QList<int> sizes = QList<int>() << hSplitter->geometry().height()-30 << 30;
    hSplitter->setSizes(sizes);
    state = REDUCED;
    return;
  }

  if (visible && state == REDUCED) {
    stackedProperties->setVisible(true);
    QSplitter *hSplitter = static_cast<QSplitter*>(parentWidget());
    hSplitter->handle(1)->setDisabled(false);
    hSplitter->handle(1)->setVisible(true);
    hSplitter->setSizes(slideSizes);
    state = VISIBLE;
    // Force refresh
    loadDynamicData();
  }
}

void PropertiesWidget::clear() {
  qDebug("Clearing torrent properties");
  save_path->clear();
  lbl_creationDate->clear();
  pieceSize_lbl->clear();
  hash_lbl->clear();
  comment_text->clear();
  progress_lbl->clear();
  trackerList->clear();
  downloaded_pieces->clear();
  pieces_availability->clear();
  avail_average_lbl->clear();
  wasted->clear();
  upTotal->clear();
  dlTotal->clear();
  peersList->clear();
  lbl_uplimit->clear();
  lbl_dllimit->clear();
  lbl_elapsed->clear();
  lbl_connections->clear();
  reannounce_lbl->clear();
  shareRatio->clear();
  listWebSeeds->clear();
  m_contentFilerLine->clear();
  PropListModel->model()->clear();
  showPiecesAvailability(false);
  showPiecesDownloaded(false);
}

QTorrentHandle PropertiesWidget::getCurrentTorrent() const {
  return h;
}

void PropertiesWidget::updateSavePath(const QTorrentHandle& _h) {
  if (h.is_valid() && h == _h) {
    save_path->setText(fsutils::toNativePath(h.save_path_parsed()));
  }
}

void PropertiesWidget::updateTorrentInfos(const QTorrentHandle& _h) {
  if (h.is_valid() && h == _h) {
    loadTorrentInfos(h);
  }
}

void PropertiesWidget::loadTorrentInfos(const QTorrentHandle& _h)
{
  clear();
  h = _h;
  if (!h.is_valid())
    return;

  try {
    // Save path
    updateSavePath(h);
    // Hash
    hash_lbl->setText(h.hash());
    PropListModel->model()->clear();
    if (h.has_metadata()) {
      // Creation date
      lbl_creationDate->setText(h.creation_date());
      // Pieces size
      pieceSize_lbl->setText(misc::friendlyUnit(h.piece_length()));
      // Comment
      comment_text->setHtml(misc::parseHtmlLinks(h.comment()));
      // URL seeds
      loadUrlSeeds();
      // List files in torrent
#if LIBTORRENT_VERSION_NUM < 10000
      PropListModel->model()->setupModelData(h.get_torrent_info());
#else
      PropListModel->model()->setupModelData(*h.torrent_file());
#endif
      filesList->setExpanded(PropListModel->index(0, 0), true);
      // Load file priorities
      PropListModel->model()->updateFilesPriorities(h.file_priorities());
    }
  } catch(const invalid_handle& e) { }
  // Load dynamic data
  loadDynamicData();
}

void PropertiesWidget::readSettings() {
  QIniSettings settings;
  // Restore splitter sizes
  QStringList sizes_str = settings.value(QString::fromUtf8("TorrentProperties/SplitterSizes"), QString()).toString().split(",");
  if (sizes_str.size() == 2) {
    slideSizes << sizes_str.first().toInt();
    slideSizes << sizes_str.last().toInt();
    QSplitter *hSplitter = static_cast<QSplitter*>(parentWidget());
    hSplitter->setSizes(slideSizes);
  }
  if (!filesList->header()->restoreState(settings.value("TorrentProperties/FilesListState").toByteArray())) {
    filesList->header()->resizeSection(0, 400); //Default
  }
  const int current_tab = settings.value("TorrentProperties/CurrentTab", -1).toInt();
  m_tabBar->setCurrentIndex(current_tab);
  if (!settings.value("TorrentProperties/Visible", false).toBool()) {
    setVisibility(false);
  }
}

void PropertiesWidget::saveSettings() {
  QIniSettings settings;
  settings.setValue("TorrentProperties/Visible", state==VISIBLE);
  // Splitter sizes
  QSplitter *hSplitter = static_cast<QSplitter*>(parentWidget());
  QList<int> sizes;
  if (state == VISIBLE)
    sizes = hSplitter->sizes();
  else
    sizes = slideSizes;
  qDebug("Sizes: %d", sizes.size());
  if (sizes.size() == 2) {
    settings.setValue(QString::fromUtf8("TorrentProperties/SplitterSizes"), QVariant(QString::number(sizes.first())+','+QString::number(sizes.last())));
  }
  settings.setValue("TorrentProperties/FilesListState", filesList->header()->saveState());
  // Remember current tab
  settings.setValue("TorrentProperties/CurrentTab", m_tabBar->currentIndex());
}

void PropertiesWidget::reloadPreferences() {
  // Take program preferences into consideration
  peersList->updatePeerHostNameResolutionState();
  peersList->updatePeerCountryResolutionState();
}

void PropertiesWidget::loadDynamicData() {
  // Refresh only if the torrent handle is valid and if visible
  if (!h.is_valid() || main_window->getCurrentTabWidget() != transferList || state != VISIBLE) return;
  try {
    libtorrent::torrent_status status = h.status(torrent_handle::query_accurate_download_counters
                                                 | torrent_handle::query_distributed_copies
                                                 | torrent_handle::query_pieces);
    // Transfer infos
    if (stackedProperties->currentIndex() == PropTabBar::MAIN_TAB) {
      wasted->setText(misc::friendlyUnit(status.total_failed_bytes+status.total_redundant_bytes));
      upTotal->setText(misc::friendlyUnit(status.all_time_upload) + " ("+misc::friendlyUnit(status.total_payload_upload)+" "+tr("this session")+")");
      dlTotal->setText(misc::friendlyUnit(status.all_time_download) + " ("+misc::friendlyUnit(status.total_payload_download)+" "+tr("this session")+")");
      lbl_uplimit->setText(h.upload_limit() <= 0 ? QString::fromUtf8("∞") : misc::friendlyUnit(h.upload_limit())+tr("/s", "/second (i.e. per second)"));
      lbl_dllimit->setText(h.download_limit() <= 0 ? QString::fromUtf8("∞") : misc::friendlyUnit(h.download_limit())+tr("/s", "/second (i.e. per second)"));
      QString elapsed_txt = misc::userFriendlyDuration(status.active_time);
      if (h.is_seed(status)) {
        elapsed_txt += " ("+tr("Seeded for %1", "e.g. Seeded for 3m10s").arg(misc::userFriendlyDuration(status.seeding_time))+")";
      }
      lbl_elapsed->setText(elapsed_txt);
      if (status.connections_limit > 0)
        lbl_connections->setText(QString::number(status.num_connections)+" ("+tr("%1 max", "e.g. 10 max").arg(QString::number(status.connections_limit))+")");
      else
        lbl_connections->setText(QString::number(status.num_connections));
      // Update next announce time
      reannounce_lbl->setText(misc::userFriendlyDuration(status.next_announce.total_seconds()));
      // Update ratio info
      const qreal ratio = QBtSession::instance()->getRealRatio(status);
      shareRatio->setText(ratio > QBtSession::MAX_RATIO ? QString::fromUtf8("∞") : misc::accurateDoubleToString(ratio, 2));
      if (!h.is_seed(status) && status.has_metadata) {
        showPiecesDownloaded(true);
        // Downloaded pieces
#if LIBTORRENT_VERSION_NUM < 10000
        bitfield bf(h.get_torrent_info().num_pieces(), 0);
#else
        bitfield bf(h.torrent_file()->num_pieces(), 0);
#endif
        h.downloading_pieces(bf);
        downloaded_pieces->setProgress(status.pieces, bf);
        // Pieces availability
        if (!h.is_paused(status) && !h.is_queued(status) && !h.is_checking(status)) {
          showPiecesAvailability(true);
          std::vector<int> avail;
          h.piece_availability(avail);
          pieces_availability->setAvailability(avail);
          avail_average_lbl->setText(misc::accurateDoubleToString(status.distributed_copies, 3));
        } else {
          showPiecesAvailability(false);
        }
        // Progress
        qreal progress = h.progress(status)*100.;
        progress_lbl->setText(misc::accurateDoubleToString(progress, 1)+"%");
      } else {
        showPiecesAvailability(false);
        showPiecesDownloaded(false);
      }
      return;
    }
    if (stackedProperties->currentIndex() == PropTabBar::TRACKERS_TAB) {
      // Trackers
      trackerList->loadTrackers();
      return;
    }
    if (stackedProperties->currentIndex() == PropTabBar::PEERS_TAB) {
      // Load peers
      peersList->loadPeers(h);
      return;
    }
    if (stackedProperties->currentIndex() == PropTabBar::FILES_TAB) {
      // Files progress
      if (h.is_valid() && status.has_metadata) {
        qDebug("Updating priorities in files tab");
        filesList->setUpdatesEnabled(false);
        std::vector<size_type> fp;
        h.file_progress(fp);
        PropListModel->model()->updateFilesProgress(fp);
        // XXX: We don't update file priorities regularly for performance
        // reasons. This means that priorities will not be updated if
        // set from the Web UI.
        // PropListModel->model()->updateFilesPriorities(h.file_priorities());
        filesList->setUpdatesEnabled(true);
      }
    }
  } catch(const invalid_handle& e) {
    qWarning() << "Caught exception in PropertiesWidget::loadDynamicData(): " << e.what();
  }
}

void PropertiesWidget::loadUrlSeeds() {
  listWebSeeds->clear();
  qDebug("Loading URL seeds");
  const QStringList hc_seeds = h.url_seeds();
  // Add url seeds
  foreach (const QString &hc_seed, hc_seeds) {
    qDebug("Loading URL seed: %s", qPrintable(hc_seed));
    new QListWidgetItem(hc_seed, listWebSeeds);
  }
}

void PropertiesWidget::openDoubleClickedFile(QModelIndex index) {
  if (!index.isValid()) return;
  if (!h.is_valid() || !h.has_metadata()) return;
  if (PropListModel->itemType(index) == TorrentContentModelItem::FileType) {
    int i = PropListModel->getFileIndex(index);
    const QDir saveDir(h.save_path());
    const QString filename = h.filepath_at(i);
    const QString file_path = fsutils::expandPath(saveDir.absoluteFilePath(filename));
    qDebug("Trying to open file at %s", qPrintable(file_path));
    // Flush data
    h.flush_cache();
    if (QFile::exists(file_path)) {
      // Hack to access samba shares with QDesktopServices::openUrl
      const QString p = file_path.startsWith("//") ? QString("file:") + file_path : file_path;
      QDesktopServices::openUrl(QUrl::fromLocalFile(p));
    } else {
      QMessageBox::warning(this, tr("I/O Error"), tr("This file does not exist yet."));
    }
  } else {
    // FOLDER
    QStringList path_items;
    path_items << index.data().toString();
    QModelIndex parent = PropListModel->parent(index);
    while(parent.isValid()) {
      path_items.prepend(parent.data().toString());
      parent = PropListModel->parent(parent);
    }
    const QDir saveDir(h.save_path());
    const QString filename = path_items.join("/");
    const QString file_path = fsutils::expandPath(saveDir.absoluteFilePath(filename));
    qDebug("Trying to open folder at %s", qPrintable(file_path));
    // Flush data
    h.flush_cache();
    if (QFile::exists(file_path)) {
      // Hack to access samba shares with QDesktopServices::openUrl
      const QString p = file_path.startsWith("//") ? QString("file:") + file_path : file_path;
      QDesktopServices::openUrl(QUrl::fromLocalFile(p));
    } else {
      QMessageBox::warning(this, tr("I/O Error"), tr("This folder does not exist yet."));
    }
  }
}

void PropertiesWidget::displayFilesListMenu(const QPoint&) {
  QMenu myFilesLlistMenu;
  QModelIndexList selectedRows = filesList->selectionModel()->selectedRows(0);
  QAction *actRename = 0;
  if (selectedRows.size() == 1) {
    actRename = myFilesLlistMenu.addAction(IconProvider::instance()->getIcon("edit-rename"), tr("Rename..."));
    myFilesLlistMenu.addSeparator();
  }
  QMenu subMenu;
  if (!h.status(0x0).is_seeding) {
    subMenu.setTitle(tr("Priority"));
    subMenu.addAction(actionNot_downloaded);
    subMenu.addAction(actionNormal);
    subMenu.addAction(actionHigh);
    subMenu.addAction(actionMaximum);
    myFilesLlistMenu.addMenu(&subMenu);
  }
  // Call menu
  const QAction *act = myFilesLlistMenu.exec(QCursor::pos());
  // The selected torrent might have dissapeared during exec()
  // from the current view thus leaving invalid indices.
  if (!(selectedRows.begin()->isValid()))
    return;
  if (act) {
    if (act == actRename) {
      renameSelectedFile();
    } else {
      int prio = 1;
      if (act == actionHigh) {
        prio = prio::HIGH;
      } else {
        if (act == actionMaximum) {
          prio = prio::MAXIMUM;
        } else {
          if (act == actionNot_downloaded) {
            prio = prio::IGNORED;
          }
        }
      }
      qDebug("Setting files priority");
      foreach (QModelIndex index, selectedRows) {
        qDebug("Setting priority(%d) for file at row %d", prio, index.row());
        PropListModel->setData(PropListModel->index(index.row(), PRIORITY, index.parent()), prio);
      }
      // Save changes
      filteredFilesChanged();
    }
  }
}

void PropertiesWidget::displayWebSeedListMenu(const QPoint&) {
  QMenu seedMenu;
  QModelIndexList rows = listWebSeeds->selectionModel()->selectedRows();
  QAction *actAdd = seedMenu.addAction(IconProvider::instance()->getIcon("list-add"), tr("New Web seed"));
  QAction *actDel = 0;
  QAction *actCpy = 0;
  QAction *actEdit = 0;

  if (rows.size()) {
    actDel = seedMenu.addAction(IconProvider::instance()->getIcon("list-remove"), tr("Remove Web seed"));
    seedMenu.addSeparator();
    actCpy = seedMenu.addAction(IconProvider::instance()->getIcon("edit-copy"), tr("Copy Web seed URL"));
    actEdit = seedMenu.addAction(IconProvider::instance()->getIcon("edit-rename"), tr("Edit Web seed URL"));
  }

  const QAction *act = seedMenu.exec(QCursor::pos());
  if (act) {
    if (act == actAdd)
      askWebSeed();
    else if (act == actDel)
      deleteSelectedUrlSeeds();
    else if (act == actCpy)
      copySelectedWebSeedsToClipboard();
    else if (act == actEdit)
      editWebSeed();
  }
}

void PropertiesWidget::renameSelectedFile() {
  const QModelIndexList selectedIndexes = filesList->selectionModel()->selectedRows(0);
  if (selectedIndexes.size() != 1)
    return;
  const QModelIndex index = selectedIndexes.first();
  // Ask for new name
  bool ok;
  QString new_name_last = AutoExpandableDialog::getText(this, tr("Rename the file"),
                                                tr("New name:"), QLineEdit::Normal,
                                                index.data().toString(), &ok).trimmed();
  if (ok && !new_name_last.isEmpty()) {
    if (!fsutils::isValidFileSystemName(new_name_last)) {
      QMessageBox::warning(this, tr("The file could not be renamed"),
                           tr("This file name contains forbidden characters, please choose a different one."),
                           QMessageBox::Ok);
      return;
    }
    if (PropListModel->itemType(index) == TorrentContentModelItem::FileType) {
      // File renaming
      const int file_index = PropListModel->getFileIndex(index);
      if (!h.is_valid() || !h.has_metadata()) return;
      QString old_name = h.filepath_at(file_index);
      if (old_name.endsWith(".!qB") && !new_name_last.endsWith(".!qB")) {
        new_name_last += ".!qB";
      }
      QStringList path_items = old_name.split("/");
      path_items.removeLast();
      path_items << new_name_last;
      QString new_name = path_items.join("/");
      if (old_name == new_name) {
        qDebug("Name did not change");
        return;
      }
      new_name = fsutils::expandPath(new_name);
      // Check if that name is already used
      for (int i=0; i<h.num_files(); ++i) {
        if (i == file_index) continue;
#if defined(Q_OS_UNIX) || defined(Q_WS_QWS)
        if (h.filepath_at(i).compare(new_name, Qt::CaseSensitive) == 0) {
#else
        if (h.filepath_at(i).compare(new_name, Qt::CaseInsensitive) == 0) {
#endif
          // Display error message
          QMessageBox::warning(this, tr("The file could not be renamed"),
                               tr("This name is already in use in this folder. Please use a different name."),
                               QMessageBox::Ok);
          return;
        }
      }
      const bool force_recheck = QFile::exists(h.save_path()+"/"+new_name);
      qDebug("Renaming %s to %s", qPrintable(old_name), qPrintable(new_name));
      h.rename_file(file_index, new_name);
      // Force recheck
      if (force_recheck) h.force_recheck();
      // Rename if torrent files model too
      if (new_name_last.endsWith(".!qB"))
        new_name_last.chop(4);
      PropListModel->setData(index, new_name_last);
    } else {
      // Folder renaming
      QStringList path_items;
      path_items << index.data().toString();
      QModelIndex parent = PropListModel->parent(index);
      while(parent.isValid()) {
        path_items.prepend(parent.data().toString());
        parent = PropListModel->parent(parent);
      }
      const QString old_path = path_items.join("/");
      path_items.removeLast();
      path_items << new_name_last;
      QString new_path = path_items.join("/");
      if (!new_path.endsWith("/")) new_path += "/";
      // Check for overwriting
      const int num_files = h.num_files();
      for (int i=0; i<num_files; ++i) {
        const QString current_name = h.filepath_at(i);
#if defined(Q_OS_UNIX) || defined(Q_WS_QWS)
        if (current_name.startsWith(new_path, Qt::CaseSensitive)) {
#else
        if (current_name.startsWith(new_path, Qt::CaseInsensitive)) {
#endif
          QMessageBox::warning(this, tr("The folder could not be renamed"),
                               tr("This name is already in use in this folder. Please use a different name."),
                               QMessageBox::Ok);
          return;
        }
      }
      bool force_recheck = false;
      // Replace path in all files
      for (int i=0; i<num_files; ++i) {
        const QString current_name = h.filepath_at(i);
        if (current_name.startsWith(old_path)) {
          QString new_name = current_name;
          new_name.replace(0, old_path.length(), new_path);
          if (!force_recheck && QDir(h.save_path()).exists(new_name))
            force_recheck = true;
          new_name = fsutils::expandPath(new_name);
          qDebug("Rename %s to %s", qPrintable(current_name), qPrintable(new_name));
          h.rename_file(i, new_name);
        }
      }
      // Force recheck
      if (force_recheck) h.force_recheck();
      // Rename folder in torrent files model too
      PropListModel->setData(index, new_name_last);
      // Remove old folder
      const QDir old_folder(h.save_path()+"/"+old_path);
      int timeout = 10;
      while(!QDir().rmpath(old_folder.absolutePath()) && timeout > 0) {
        // XXX: We should not sleep here (freezes the UI for 1 second)
        SleeperThread::msleep(100);
        --timeout;
      }
    }
  }
}

void PropertiesWidget::askWebSeed() {
  bool ok;
  // Ask user for a new url seed
  const QString url_seed = AutoExpandableDialog::getText(this, tr("New url seed", "New HTTP source"),
                                                 tr("New url seed:"), QLineEdit::Normal,
                                                 QString::fromUtf8("http://www."), &ok);
  if (!ok) return;
  qDebug("Adding %s web seed", qPrintable(url_seed));
  if (!listWebSeeds->findItems(url_seed, Qt::MatchFixedString).empty()) {
    QMessageBox::warning(this, "qBittorrent",
                         tr("This url seed is already in the list."),
                         QMessageBox::Ok);
    return;
  }
  if (h.is_valid())
    h.add_url_seed(url_seed);
  // Refresh the seeds list
  loadUrlSeeds();
}

void PropertiesWidget::deleteSelectedUrlSeeds() {
  const QList<QListWidgetItem *> selectedItems = listWebSeeds->selectedItems();
  if (selectedItems.isEmpty())
    return;
  bool change = false;
  foreach (const QListWidgetItem *item, selectedItems) {
    QString url_seed = item->text();
    try {
      h.remove_url_seed(url_seed);
      change = true;
    } catch (invalid_handle&) {}
  }
  if (change) {
    // Refresh list
    loadUrlSeeds();
  }
}

void PropertiesWidget::copySelectedWebSeedsToClipboard() const {
  const QList<QListWidgetItem *> selected_items = listWebSeeds->selectedItems();
  if (selected_items.isEmpty())
    return;

  QStringList urls_to_copy;
  foreach (QListWidgetItem *item, selected_items)
    urls_to_copy << item->text();

  QApplication::clipboard()->setText(urls_to_copy.join("\n"));
}

void PropertiesWidget::editWebSeed() {
  const QList<QListWidgetItem *> selected_items = listWebSeeds->selectedItems();
  if (selected_items.size() != 1)
    return;

  const QListWidgetItem *selected_item = selected_items.last();
  const QString old_seed = selected_item->text();
  bool result;
  const QString new_seed = AutoExpandableDialog::getText(this, tr("Web seed editing"),
                                                 tr("Web seed URL:"), QLineEdit::Normal,
                                                 old_seed, &result);
  if (!result)
    return;

  if (!listWebSeeds->findItems(new_seed, Qt::MatchFixedString).empty()) {
    QMessageBox::warning(this, tr("qBittorrent"),
                         tr("This url seed is already in the list."),
                         QMessageBox::Ok);
    return;
  }

  try {
    h.remove_url_seed(old_seed);
    h.add_url_seed(new_seed);
    loadUrlSeeds();
  } catch (invalid_handle&) {}
}

bool PropertiesWidget::applyPriorities() {
  qDebug("Saving files priorities");
  const std::vector<int> priorities = PropListModel->model()->getFilesPriorities();
  // Save first/last piece first option state
  bool first_last_piece_first = h.first_last_piece_first();
  // Prioritize the files
  qDebug("prioritize files: %d", priorities[0]);
  h.prioritize_files(priorities);
  // Restore first/last piece first option if necessary
  if (first_last_piece_first)
    h.prioritize_first_last_piece(true);
  return true;
}

void PropertiesWidget::filteredFilesChanged() {
  if (h.is_valid()) {
    applyPriorities();
  }
}

void PropertiesWidget::filterText(const QString& filter) {
  PropListModel->setFilterFixedString(filter);
  if (filter.isEmpty()) {
    filesList->collapseAll();
    filesList->expand(PropListModel->index(0, 0));
  }
  else
    filesList->expandAll();
}
