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
#include <QAction>
#include <QMessageBox>
#include <QMenu>
#include <QFileDialog>
#include <QInputDialog>
#include "propertieswidget.h"
#include "transferlistwidget.h"
#include "torrentpersistentdata.h"
#include "bittorrent.h"
#include "proplistdelegate.h"
#include "torrentfilesmodel.h"
#include "peerlistwidget.h"
#include "trackerlist.h"
#include "GUI.h"
#include "downloadedpiecesbar.h"
#include "pieceavailabilitybar.h"

#ifdef Q_WS_MAC
#define DEFAULT_BUTTON_CSS "QPushButton {border: 1px solid rgb(85, 81, 91);border-radius: 3px;padding: 2px; margin-left: 8px; margin-right: 8px;}"
#define SELECTED_BUTTON_CSS "QPushButton {border: 1px solid rgb(85, 81, 91);border-radius: 3px;padding: 2px;background-color: rgb(255, 208, 105); margin-left: 8px; margin-right: 8px;}"
#else
#define DEFAULT_BUTTON_CSS "QPushButton {border: 1px solid rgb(85, 81, 91);border-radius: 3px;padding: 2px; margin-left: 3px; margin-right: 3px;}"
#define SELECTED_BUTTON_CSS "QPushButton {border: 1px solid rgb(85, 81, 91);border-radius: 3px;padding: 2px;background-color: rgb(255, 208, 105); margin-left: 3px; margin-right: 3px;}"
#endif

PropertiesWidget::PropertiesWidget(QWidget *parent, GUI* main_window, TransferListWidget *transferList, Bittorrent* BTSession):
    QWidget(parent), transferList(transferList), main_window(main_window), BTSession(BTSession) {
  setupUi(this);
  state = VISIBLE;

  // Buttons stylesheet
  trackers_button->setStyleSheet(DEFAULT_BUTTON_CSS);
  peers_button->setStyleSheet(DEFAULT_BUTTON_CSS);
  url_seeds_button->setStyleSheet(DEFAULT_BUTTON_CSS);
  files_button->setStyleSheet(DEFAULT_BUTTON_CSS);
  main_infos_button->setStyleSheet(DEFAULT_BUTTON_CSS);
  main_infos_button->setShortcut(QKeySequence(QString::fromUtf8("Alt+P")));

  // Set Properties list model
  PropListModel = new TorrentFilesModel();
  filesList->setModel(PropListModel);
  PropDelegate = new PropListDelegate(0);
  filesList->setItemDelegate(PropDelegate);

  // QActions
  actionIgnored = new QAction(tr("Ignored"), this);
  actionNormal = new QAction(tr("Normal"), this);
  actionMaximum = new QAction(tr("Maximum"), this);
  actionHigh = new QAction(tr("High"), this);

  // SIGNAL/SLOTS
  connect(filesList, SIGNAL(clicked(const QModelIndex&)), filesList, SLOT(edit(const QModelIndex&)));
  connect(collapseAllButton, SIGNAL(clicked()), filesList, SLOT(collapseAll()));
  connect(expandAllButton, SIGNAL(clicked()), filesList, SLOT(expandAll()));
  connect(filesList, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayFilesListMenu(const QPoint&)));
  connect(actionIgnored, SIGNAL(triggered()), this, SLOT(ignoreSelection()));
  connect(actionNormal, SIGNAL(triggered()), this, SLOT(normalSelection()));
  connect(actionHigh, SIGNAL(triggered()), this, SLOT(highSelection()));
  connect(actionMaximum, SIGNAL(triggered()), this, SLOT(maximumSelection()));
  connect(addWS_button, SIGNAL(clicked()), this, SLOT(askWebSeed()));
  connect(deleteWS_button, SIGNAL(clicked()), this, SLOT(deleteSelectedUrlSeeds()));
  connect(transferList, SIGNAL(currentTorrentChanged(QTorrentHandle&)), this, SLOT(loadTorrentInfos(QTorrentHandle &)));
  connect(PropDelegate, SIGNAL(filteredFilesChanged()), this, SLOT(filteredFilesChanged()));
  connect(stackedProperties, SIGNAL(currentChanged(int)), this, SLOT(loadDynamicData()));

  // Downloaded pieces progress bar
  downloaded_pieces = new DownloadedPiecesBar(this);
  ProgressHLayout->insertWidget(1, downloaded_pieces);
  // Pieces availability bar
  pieces_availability = new PieceAvailabilityBar(this);
  ProgressHLayout_2->insertWidget(1, pieces_availability);
  // Tracker list
  trackerList = new TrackerList(this);
  verticalLayout_trackers->addWidget(trackerList);
  // Peers list
  peersList = new PeerListWidget(this);
  peerpage_layout->addWidget(peersList);
  // Dynamic data refresher
  refreshTimer = new QTimer(this);
  connect(refreshTimer, SIGNAL(timeout()), this, SLOT(loadDynamicData()));
  refreshTimer->start(3000); // 3sec
}

PropertiesWidget::~PropertiesWidget() {
  saveSettings();
  delete refreshTimer;
  delete trackerList;
  delete peersList;
  delete downloaded_pieces;
  delete pieces_availability;
  delete PropListModel;
  delete PropDelegate;
  // Delete QActions
  delete actionIgnored;
  delete actionNormal;
  delete actionMaximum;
  delete actionHigh;
}

void PropertiesWidget::showPieceBars(bool show) {
  avail_pieces_lbl->setVisible(show);
  pieces_availability->setVisible(show);
  downloaded_pieces_lbl->setVisible(show);
  downloaded_pieces->setVisible(show);
  progress_lbl->setVisible(show);
  line_2->setVisible(show);
  avail_average_lbl->setVisible(show);
}

void PropertiesWidget::reduce() {
  if(state == VISIBLE) {
    QSplitter *hSplitter = static_cast<QSplitter*>(parentWidget());
    slideSizes = hSplitter->sizes();
    stackedProperties->setVisible(false);
    QList<int> sizes;
    sizes << hSplitter->geometry().height()-30 << 30;
    hSplitter->setSizes(sizes);
    hSplitter->handle(1)->setVisible(false);
    hSplitter->handle(1)->setDisabled(true);
    state = REDUCED;
  }
}

void PropertiesWidget::slide() {
  if(state == REDUCED) {
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
  shareRatio->clear();
  listWebSeeds->clear();
  PropListModel->clear();
  showPieceBars(false);
  setEnabled(false);
}

const QTorrentHandle& PropertiesWidget::getCurrentTorrent() const {
  return h;
}

Bittorrent* PropertiesWidget::getBTSession() const {
  return BTSession;
}

void PropertiesWidget::loadTorrentInfos(QTorrentHandle &_h) {
  h = _h;
  if(!h.is_valid()) {
    clear();
    return;
  }
  setEnabled(true);

  try {
    if(!h.is_seed())
      showPieceBars(true);
    else
      showPieceBars(false);
    // Save path
    save_path->setText(TorrentPersistentData::getSavePath(h.hash()));
    // Creation date
    lbl_creationDate->setText(h.creation_date());
    // Hash
    hash_lbl->setText(h.hash());
    // Comment
    comment_text->setHtml(h.comment());
    // URL seeds
    loadUrlSeeds();
    // List files in torrent
    PropListModel->clear();
    PropListModel->setupModelData(h.get_torrent_info());
    PropListModel->updateFilesPriorities(h.file_priorities());
    // Expand first item if possible
    filesList->expand(PropListModel->index(0, 0));
  } catch(invalid_handle e) {

  }
  // Load dynamic data
  loadDynamicData();
}

void PropertiesWidget::readSettings() {
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  QVariantList contentColsWidths = settings.value(QString::fromUtf8("TorrentProperties/filesColsWidth"), QVariantList()).toList();
  if(contentColsWidths.empty()) {
    filesList->header()->resizeSection(0, filesList->width()/2.);
  } else {
    for(int i=0; i<contentColsWidths.size(); ++i) {
      filesList->setColumnWidth(i, contentColsWidths.at(i).toInt());
    }
  }
  // Restore splitter sizes
  QStringList sizes_str = settings.value(QString::fromUtf8("TorrentProperties/SplitterSizes"), QString()).toString().split(",");
  if(sizes_str.size() == 2) {
    slideSizes << sizes_str.first().toInt();
    slideSizes << sizes_str.last().toInt();
    QSplitter *hSplitter = static_cast<QSplitter*>(parentWidget());
    hSplitter->setSizes(slideSizes);
  }
  if(!settings.value("TorrentProperties/Visible", false).toBool()) {
    reduce();
  } else {
    main_infos_button->setStyleSheet(SELECTED_BUTTON_CSS);
    //setEnabled(false);
  }
}

void PropertiesWidget::saveSettings() {
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  settings.setValue("TorrentProperties/Visible", state==VISIBLE);
  QVariantList contentColsWidths;
  for(int i=0; i<PropListModel->columnCount(); ++i) {
    contentColsWidths.append(filesList->columnWidth(i));
  }
  settings.setValue(QString::fromUtf8("TorrentProperties/filesColsWidth"), contentColsWidths);
  // Splitter sizes
  QSplitter *hSplitter = static_cast<QSplitter*>(parentWidget());
  QList<int> sizes;
  if(state == VISIBLE)
    sizes = hSplitter->sizes();
  else
    sizes = slideSizes;
  if(state == VISIBLE)
    qDebug("Visible");
  qDebug("Sizes: %d", sizes.size());
  if(sizes.size() == 2) {
    settings.setValue(QString::fromUtf8("TorrentProperties/SplitterSizes"), QString::number(sizes.first())+','+QString::number(sizes.last()));
  }
}

void PropertiesWidget::reloadPreferences() {
  // Take program preferences into consideration
  peersList->updatePeerHostNameResolutionState();
  peersList->updatePeerCountryResolutionState();
}

void PropertiesWidget::loadDynamicData() {
  // Refresh only if the torrent handle is valid and if visible
  if(!h.is_valid() || main_window->getCurrentTabIndex() != TAB_TRANSFER || state != VISIBLE) return;
  try {
    // Transfer infos
    if(stackedProperties->currentIndex() == MAIN_TAB) {
      wasted->setText(misc::friendlyUnit(h.total_failed_bytes()+h.total_redundant_bytes()));
      upTotal->setText(misc::friendlyUnit(h.all_time_upload()) + " ("+misc::friendlyUnit(h.total_payload_upload())+" "+tr("this session")+")");
      dlTotal->setText(misc::friendlyUnit(h.all_time_download()) + " ("+misc::friendlyUnit(h.total_payload_download())+" "+tr("this session")+")");
      if(h.upload_limit() <= 0)
        lbl_uplimit->setText(tr("Unlimited"));
      else
        lbl_uplimit->setText(QString::number(h.upload_limit(), 'f', 1)+" "+tr("KiB/s"));
      if(h.download_limit() <= 0)
        lbl_dllimit->setText(tr("Unlimited"));
      else
        lbl_dllimit->setText(QString::number(h.download_limit(), 'f', 1)+" "+tr("KiB/s"));
      QString elapsed_txt = misc::userFriendlyDuration(h.active_time());
      if(h.is_seed()) {
        elapsed_txt += " ("+tr("Seeded for %1", "e.g. Seeded for 3m10s").arg(misc::userFriendlyDuration(h.seeding_time()))+")";
      }
      lbl_elapsed->setText(elapsed_txt);
      lbl_connections->setText(QString::number(h.num_connections())+" ("+tr("%1 max", "e.g. 10 max").arg(QString::number(h.connections_limit()))+")");
      // Update ratio info
      float ratio;
      if(h.total_payload_download() == 0){
        if(h.total_payload_upload() == 0)
          ratio = 1.;
        else
          ratio = 10.; // Max ratio
      }else{
        ratio = (double)h.total_payload_upload()/(double)h.total_payload_download();
        if(ratio > 10.){
          ratio = 10.;
        }
      }
      shareRatio->setText(QString(QByteArray::number(ratio, 'f', 1)));
      if(!h.is_seed()) {
        // Downloaded pieces
        downloaded_pieces->setProgress(h.pieces());
        // Pieces availability
        std::vector<int> avail;
        h.piece_availability(avail);
        double avail_average =  pieces_availability->setAvailability(avail);
        avail_average_lbl->setText(QString::number(avail_average, 'f', 1));
        // Progress
        progress_lbl->setText(QString::number(h.progress()*100., 'f', 1)+"%");
      }
      return;
    }
    if(stackedProperties->currentIndex() == TRACKERS_TAB) {
      // Trackers
      trackerList->loadTrackers();
      return;
    }
    if(stackedProperties->currentIndex() == PEERS_TAB) {
      // Load peers
      peersList->loadPeers(h);
      return;
    }
    if(stackedProperties->currentIndex() == FILES_TAB) {
      // Files progress
      std::vector<size_type> fp;
      h.file_progress(fp);
      PropListModel->updateFilesProgress(fp);
    }
  } catch(invalid_handle e) {}
}

void PropertiesWidget::loadUrlSeeds(){
  QStringList already_added;
  listWebSeeds->clear();
  QStringList url_seeds = h.url_seeds();
  foreach(const QString &url_seed, url_seeds){
    if(!url_seed.isEmpty()) {
      new QListWidgetItem(url_seed, listWebSeeds);
      already_added << url_seed;
    }
  }
  // Load the hard-coded url seeds
  QStringList hc_seeds = h.url_seeds();
  // Add hard coded url seeds
  foreach(const QString &hc_seed, hc_seeds){
    if(!already_added.contains(hc_seed)){
      new QListWidgetItem(hc_seed, listWebSeeds);
    }
  }
}

/* Tab buttons */
QPushButton* PropertiesWidget::getButtonFromIndex(int index) {
  switch(index) {
  case TRACKERS_TAB:
    return trackers_button;
  case PEERS_TAB:
    return peers_button;
  case URLSEEDS_TAB:
    return url_seeds_button;
  case FILES_TAB:
    return files_button;
      default:
    return main_infos_button;
  }
}

void PropertiesWidget::on_main_infos_button_clicked() {
  if(state == VISIBLE && stackedProperties->currentIndex() == MAIN_TAB) {
    reduce();
  } else {
    slide();
    getButtonFromIndex(stackedProperties->currentIndex())->setStyleSheet(DEFAULT_BUTTON_CSS);
    stackedProperties->setCurrentIndex(MAIN_TAB);
    main_infos_button->setStyleSheet(SELECTED_BUTTON_CSS);
  }
}

void PropertiesWidget::on_trackers_button_clicked() {
  if(state == VISIBLE && stackedProperties->currentIndex() == TRACKERS_TAB) {
    reduce();
  } else {
    slide();
    getButtonFromIndex(stackedProperties->currentIndex())->setStyleSheet(DEFAULT_BUTTON_CSS);
    stackedProperties->setCurrentIndex(TRACKERS_TAB);
    trackers_button->setStyleSheet(SELECTED_BUTTON_CSS);
  }
}

void PropertiesWidget::on_peers_button_clicked() {
  if(state == VISIBLE && stackedProperties->currentIndex() == PEERS_TAB) {
    reduce();
  } else {
    slide();
    getButtonFromIndex(stackedProperties->currentIndex())->setStyleSheet(DEFAULT_BUTTON_CSS);
    stackedProperties->setCurrentIndex(PEERS_TAB);
    peers_button->setStyleSheet(SELECTED_BUTTON_CSS);
  }
}

void PropertiesWidget::on_url_seeds_button_clicked() {
  if(state == VISIBLE && stackedProperties->currentIndex() == URLSEEDS_TAB) {
    reduce();
  } else {
    slide();
    getButtonFromIndex(stackedProperties->currentIndex())->setStyleSheet(DEFAULT_BUTTON_CSS);
    stackedProperties->setCurrentIndex(URLSEEDS_TAB);
    url_seeds_button->setStyleSheet(SELECTED_BUTTON_CSS);
  }
}

void PropertiesWidget::on_files_button_clicked() {
  if(state == VISIBLE && stackedProperties->currentIndex() == FILES_TAB) {
    reduce();
  } else {
    slide();
    getButtonFromIndex(stackedProperties->currentIndex())->setStyleSheet(DEFAULT_BUTTON_CSS);
    stackedProperties->setCurrentIndex(FILES_TAB);
    files_button->setStyleSheet(SELECTED_BUTTON_CSS);
  }
}

void PropertiesWidget::displayFilesListMenu(const QPoint&){
  //if(h.get_torrent_info().num_files() == 1) return;
  QMenu myFilesLlistMenu(this);
  //QModelIndex index;
  // Enable/disable pause/start action given the DL state
  //QModelIndexList selectedIndexes = filesList->selectionModel()->selectedIndexes();
  myFilesLlistMenu.setTitle(tr("Priority"));
  myFilesLlistMenu.addAction(actionIgnored);
  myFilesLlistMenu.addAction(actionNormal);
  myFilesLlistMenu.addAction(actionHigh);
  myFilesLlistMenu.addAction(actionMaximum);
  // Call menu
  myFilesLlistMenu.exec(QCursor::pos());
}

void PropertiesWidget::ignoreSelection(){
  QModelIndexList selectedIndexes = filesList->selectionModel()->selectedIndexes();
  foreach(const QModelIndex &index, selectedIndexes){
    if(index.column() == PRIORITY){
      if(PropListModel->data(index) != QVariant(IGNORED)){
        PropListModel->setData(index, QVariant(IGNORED));
        filteredFilesChanged();
      }
    }
  }
}

void PropertiesWidget::normalSelection(){
  QModelIndexList selectedIndexes = filesList->selectionModel()->selectedIndexes();
  foreach(const QModelIndex &index, selectedIndexes){
    if(index.column() == PRIORITY){
      if(PropListModel->data(index) != QVariant(NORMAL)){
        PropListModel->setData(index, QVariant(NORMAL));
        filteredFilesChanged();
      }
    }
  }
}

void PropertiesWidget::highSelection(){
  QModelIndexList selectedIndexes = filesList->selectionModel()->selectedIndexes();
  foreach(const QModelIndex &index, selectedIndexes){
    if(index.column() == PRIORITY){
      if(PropListModel->data(index) != QVariant(HIGH)){
        PropListModel->setData(index, QVariant(HIGH));
        filteredFilesChanged();
      }
    }
  }
}

void PropertiesWidget::maximumSelection(){
  QModelIndexList selectedIndexes = filesList->selectionModel()->selectedIndexes();
  foreach(const QModelIndex &index, selectedIndexes){
    if(index.column() == PRIORITY){
      if(PropListModel->data(index) != QVariant(MAXIMUM)){
        PropListModel->setData(index, QVariant(MAXIMUM));
        filteredFilesChanged();
      }
    }
  }
}

void PropertiesWidget::askWebSeed(){
  bool ok;
  // Ask user for a new url seed
  QString url_seed = QInputDialog::getText(this, tr("New url seed", "New HTTP source"),
                                           tr("New url seed:"), QLineEdit::Normal,
                                           QString::fromUtf8("http://www."), &ok);
  if(!ok) return;
  qDebug("Adding %s web seed", url_seed.toLocal8Bit().data());
  if(!listWebSeeds->findItems(url_seed, Qt::MatchFixedString).empty()) {
    QMessageBox::warning(this, tr("qBittorrent"),
                         tr("This url seed is already in the list."),
                         QMessageBox::Ok);
    return;
  }
  h.add_url_seed(url_seed);
  // Refresh the seeds list
  loadUrlSeeds();
}

void PropertiesWidget::deleteSelectedUrlSeeds(){
  QList<QListWidgetItem *> selectedItems = listWebSeeds->selectedItems();
  bool change = false;
  foreach(QListWidgetItem *item, selectedItems){
    QString url_seed = item->text();
    h.remove_url_seed(url_seed);
    change = true;
  }
  if(change){
    // Refresh list
    loadUrlSeeds();
  }
}

bool PropertiesWidget::savePiecesPriorities() {
  qDebug("Saving pieces priorities");
  std::vector<int> priorities = PropListModel->getFilesPriorities(h.get_torrent_info().num_files());
  bool first_last_piece_first = false;
  if(h.first_last_piece_first())
    first_last_piece_first = true;
  h.prioritize_files(priorities);
  if(first_last_piece_first)
    h.prioritize_first_last_piece(true);
  return true;
}


void PropertiesWidget::on_changeSavePathButton_clicked() {
  QString dir;
  QDir saveDir(h.save_path());
  if(saveDir.exists()){
    dir = QFileDialog::getExistingDirectory(this, tr("Choose save path"), h.save_path());
  }else{
    dir = QFileDialog::getExistingDirectory(this, tr("Choose save path"), QDir::homePath());
  }
  if(!dir.isNull()){
    // Check if savePath exists
    QDir savePath(dir);
    if(!savePath.exists()){
      if(!savePath.mkpath(savePath.path())){
        QMessageBox::critical(0, tr("Save path creation error"), tr("Could not create the save path"));
        return;
      }
    }
    // Save savepath
    TorrentPersistentData::saveSavePath(h.hash(), savePath.path());
    // Actually move storage
    if(!BTSession->useTemporaryFolder() || h.is_seed())
      h.move_storage(savePath.path());
    // Update save_path in dialog
    save_path->setText(savePath.path());
  }
}

void PropertiesWidget::filteredFilesChanged() {
  if(h.is_valid()) {
    savePiecesPriorities();
    transferList->updateTorrentSizeAndProgress(h.hash());
  }
}
