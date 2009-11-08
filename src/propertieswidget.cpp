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
#include "propertieswidget.h"
#include "TransferListWidget.h"
#include "torrentPersistentData.h"
#include "realprogressbar.h"
#include "realprogressbarthread.h"
#include "bittorrent.h"

#define DEFAULT_BUTTON_CSS "QPushButton {border: 1px solid rgb(85, 81, 91);border-radius: 3px;padding: 2px;}"
#define SELECTED_BUTTON_CSS "QPushButton {border: 1px solid rgb(85, 81, 91);border-radius: 3px;padding: 2px;background-color: rgb(255, 208, 105);}"

PropertiesWidget::PropertiesWidget(QWidget *parent, TransferListWidget *transferList, bittorrent* BTSession): QWidget(parent), transferList(transferList), BTSession(BTSession) {
  setupUi(this);
  state = VISIBLE;
  reduce();

  connect(transferList, SIGNAL(currentTorrentChanged(QTorrentHandle&)), this, SLOT(loadTorrentInfos(QTorrentHandle &)));
  connect(incrementalDownload, SIGNAL(stateChanged(int)), this, SLOT(setIncrementalDownload(int)));

  // Downloaded pieces progress bar
  progressBar = new RealProgressBar(this);
  progressBar->setForegroundColor(Qt::blue);
  progressBarVbox = new QVBoxLayout(RealProgressBox);
  progressBarVbox->addWidget(progressBar);
  progressBarUpdater = 0;
  // Dynamic data refresher
  refreshTimer = new QTimer(this);
  connect(refreshTimer, SIGNAL(timeout()), this, SLOT(loadDynamicData()));
  refreshTimer->start(10000); // 10sec
}

PropertiesWidget::~PropertiesWidget() {
  delete refreshTimer;
  if(progressBarUpdater)
    delete progressBarUpdater;
  delete progressBar;
  delete progressBarVbox;
}

void PropertiesWidget::reduce() {
  if(state == VISIBLE) {
    stackedProperties->setFixedHeight(0);
    state = REDUCED;
  }
}

void PropertiesWidget::slide() {
  if(state == REDUCED) {
    stackedProperties->setFixedHeight(232);
    state = VISIBLE;
  }
}

void PropertiesWidget::loadTorrentInfos(QTorrentHandle &_h) {
  h = _h;
  if(!h.is_valid()) return;
  if(progressBarUpdater)
    delete progressBarUpdater;
  progressBarUpdater = 0;
  try {
    // Save path
    save_path->setText(TorrentPersistentData::getSavePath(h.hash()));
    // Author
    QString author = h.creator().trimmed();
    if(author.isEmpty())
      author = tr("Unknown");
    creator->setText(author);
    // Hash
    hash_lbl->setText(h.hash());
    // Comment
    comment_lbl->setText(h.comment());
    // Sequential download
    incrementalDownload->setChecked(TorrentPersistentData::isSequentialDownload(h.hash()));
    // Trackers
    loadTrackers();
    // downloaded pieces updater
    progressBarUpdater = new RealProgressBarThread(progressBar, h);
    progressBarUpdater->start();
  } catch(invalid_handle e) {

  }
  // Load dynamic data
  loadDynamicData();
}

void PropertiesWidget::loadDynamicData() {
  if(!h.is_valid()) return;
  try {
    // Session infos
    failed->setText(misc::friendlyUnit(h.total_failed_bytes()));
    upTotal->setText(misc::friendlyUnit(h.total_payload_upload()));
    dlTotal->setText(misc::friendlyUnit(h.total_payload_download()));
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
    // Downloaded pieces
    if(progressBarUpdater)
      progressBarUpdater->refresh();
  } catch(invalid_handle e) {}
}

void PropertiesWidget::setIncrementalDownload(int checkboxState) {
  if(!h.is_valid()) return;
  h.set_sequential_download(checkboxState == Qt::Checked);
  TorrentPersistentData::saveSequentialStatus(h);
}

void PropertiesWidget::loadTrackers() {
  if(!h.is_valid()) return;
  //Trackers
  std::vector<announce_entry> trackers = h.trackers();
  trackersURLS->clear();
  QHash<QString, QString> errors = BTSession->getTrackersErrors(h.hash());
  unsigned int nbTrackers = trackers.size();
  for(unsigned int i=0; i<nbTrackers; ++i){
    QString current_tracker = misc::toQString(trackers[i].url);
    QListWidgetItem *item = new QListWidgetItem(current_tracker, trackersURLS);
    // IsThere any errors ?
    if(errors.contains(current_tracker)) {
      item->setForeground(QBrush(QColor("red")));
      // Set tooltip
      QString msg="";
      unsigned int i=0;
      foreach(QString word, errors[current_tracker].split(" ")) {
        if(i > 0 && i%5!=1) msg += " ";
        msg += word;
        if(i> 0 && i%5==0) msg += "\n";
        ++i;
      }
      item->setToolTip(msg);
    } else {
      item->setForeground(QBrush(QColor("green")));
    }
  }
  QString tracker = h.current_tracker().trimmed();
  if(!tracker.isEmpty()){
    trackerURL->setText(tracker);
  }else{
    trackerURL->setText(tr("None - Unreachable?"));
  }
}

/* Tab buttons */
QPushButton* PropertiesWidget::getButtonFromIndex(int index) {
  switch(index) {
  case TRACKERS_TAB:
    return trackers_button;
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
