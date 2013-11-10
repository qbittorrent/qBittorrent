/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2010  Christophe Dumez
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

#include <QFileDialog>
#include <QMessageBox>

#include "torrentpersistentdata.h"
#include "torrentcreatordlg.h"
#include "fs_utils.h"
#include "misc.h"
#include "qinisettings.h"
#include "torrentcreatorthread.h"
#include "iconprovider.h"
#include "qbtsession.h"

const uint NB_PIECES_MIN = 1200;
const uint NB_PIECES_MAX = 2200;

using namespace libtorrent;

TorrentCreatorDlg::TorrentCreatorDlg(QWidget *parent): QDialog(parent), creatorThread(0) {
  setupUi(this);
  // Icons
  addFile_button->setIcon(IconProvider::instance()->getIcon("document-new"));
  addFolder_button->setIcon(IconProvider::instance()->getIcon("folder-new"));
  createButton->setIcon(IconProvider::instance()->getIcon("document-save"));
  cancelButton->setIcon(IconProvider::instance()->getIcon("dialog-cancel"));

  setAttribute(Qt::WA_DeleteOnClose);
  setModal(true);
  showProgressBar(false);
  loadTrackerList();
  // Piece sizes
  m_piece_sizes << 32 << 64 << 128 << 256 << 512 << 1024 << 2048 << 4096;
  loadSettings();
  show();
}

TorrentCreatorDlg::~TorrentCreatorDlg() {
  if (creatorThread)
    delete creatorThread;
}

void TorrentCreatorDlg::on_addFolder_button_clicked() {
  QIniSettings settings;
  QString last_path = settings.value("CreateTorrent/last_add_path", QDir::homePath()).toString();
  QString dir = QFileDialog::getExistingDirectory(this, tr("Select a folder to add to the torrent"), last_path, QFileDialog::ShowDirsOnly);
  if (!dir.isEmpty()) {
    settings.setValue("CreateTorrent/last_add_path", dir);
    textInputPath->setText(fsutils::toNativePath(dir));
    // Update piece size
    if (checkAutoPieceSize->isChecked())
      updateOptimalPieceSize();
  }
}

void TorrentCreatorDlg::on_addFile_button_clicked() {
  QIniSettings settings;
  QString last_path = settings.value("CreateTorrent/last_add_path", QDir::homePath()).toString();
  QString file = QFileDialog::getOpenFileName(this, tr("Select a file to add to the torrent"), last_path);
  if (!file.isEmpty()) {
    settings.setValue("CreateTorrent/last_add_path", fsutils::branchPath(file));
    textInputPath->setText(fsutils::toNativePath(file));
    // Update piece size
    if (checkAutoPieceSize->isChecked())
      updateOptimalPieceSize();
  }
}

int TorrentCreatorDlg::getPieceSize() const {
  return m_piece_sizes.at(comboPieceSize->currentIndex())*1024;
}

// Main function that create a .torrent file
void TorrentCreatorDlg::on_createButton_clicked() {
  QString input = fsutils::fromNativePath(textInputPath->text()).trimmed();
  if (input.endsWith("/"))
    input.chop(1);
  if (input.isEmpty()) {
    QMessageBox::critical(0, tr("No input path set"), tr("Please type an input path first"));
    return;
  }
  QStringList trackers = trackers_list->toPlainText().split("\n");
  if (!trackers_list->toPlainText().trimmed().isEmpty())
    saveTrackerList();

  QIniSettings settings;
  QString last_path = settings.value("CreateTorrent/last_save_path", QDir::homePath()).toString();

  QString destination = QFileDialog::getSaveFileName(this, tr("Select destination torrent file"), last_path, tr("Torrent Files")+QString::fromUtf8(" (*.torrent)"));
  if (!destination.isEmpty()) {
    settings.setValue("CreateTorrent/last_save_path", fsutils::branchPath(destination));
    if (!destination.toUpper().endsWith(".TORRENT"))
      destination += QString::fromUtf8(".torrent");
  } else {
    return;
  }
  // Disable dialog
  setInteractionEnabled(false);
  showProgressBar(true);
  // Set busy cursor
  setCursor(QCursor(Qt::WaitCursor));
  // Actually create the torrent
  QStringList url_seeds = URLSeeds_list->toPlainText().split("\n");
  QString comment = txt_comment->toPlainText();
  // Create the creator thread
  creatorThread = new TorrentCreatorThread(this);
  connect(creatorThread, SIGNAL(creationSuccess(QString, QString)), this, SLOT(handleCreationSuccess(QString, QString)));
  connect(creatorThread, SIGNAL(creationFailure(QString)), this, SLOT(handleCreationFailure(QString)));
  connect(creatorThread, SIGNAL(updateProgress(int)), this, SLOT(updateProgressBar(int)));
  creatorThread->create(input, destination, trackers, url_seeds, comment, check_private->isChecked(), getPieceSize());
}

void TorrentCreatorDlg::handleCreationFailure(QString msg) {
  // Remove busy cursor
  setCursor(QCursor(Qt::ArrowCursor));
  QMessageBox::information(0, tr("Torrent creation"), tr("Torrent creation was unsuccessful, reason: %1").arg(msg));
  setInteractionEnabled(true);
  showProgressBar(false);
}

void TorrentCreatorDlg::handleCreationSuccess(QString path, QString branch_path) {
  // Remove busy cursor
  setCursor(QCursor(Qt::ArrowCursor));
  if (checkStartSeeding->isChecked()) {
    // Create save path temp data
    boost::intrusive_ptr<torrent_info> t;
    try {
      t = new torrent_info(fsutils::toNativePath(path).toUtf8().data());
    } catch(std::exception&) {
      QMessageBox::critical(0, tr("Torrent creation"), tr("Created torrent file is invalid. It won't be added to download list."));
      return;
    }
    QString hash = misc::toQString(t->info_hash());
    QString save_path = branch_path;
    TorrentTempData::setSavePath(hash, save_path);
    // Enable seeding mode (do not recheck the files)
    TorrentTempData::setSeedingMode(hash, true);
    emit torrent_to_seed(path);
    if (checkIgnoreShareLimits->isChecked())
      QBtSession::instance()->setMaxRatioPerTorrent(hash, -1);
  }
  QMessageBox::information(0, tr("Torrent creation"), tr("Torrent was created successfully:")+" "+fsutils::toNativePath(path));
  close();
}

void TorrentCreatorDlg::on_cancelButton_clicked() {
  // End torrent creation thread
  if (creatorThread && creatorThread->isRunning()) {
    creatorThread->abortCreation();
    creatorThread->terminate();
    // Wait for termination
    creatorThread->wait();
  }
  // Close the dialog
  close();
}

void TorrentCreatorDlg::updateProgressBar(int progress) {
  progressBar->setValue(progress);
}

void TorrentCreatorDlg::setInteractionEnabled(bool enabled)
{
  textInputPath->setEnabled(enabled);
  addFile_button->setEnabled(enabled);
  addFolder_button->setEnabled(enabled);
  trackers_list->setEnabled(enabled);
  URLSeeds_list->setEnabled(enabled);
  txt_comment->setEnabled(enabled);
  comboPieceSize->setEnabled(enabled);
  check_private->setEnabled(enabled);
  checkStartSeeding->setEnabled(enabled);
  createButton->setEnabled(enabled);
  checkIgnoreShareLimits->setEnabled(enabled && checkStartSeeding->isChecked());
  //cancelButton->setEnabled(!enabled);
}

void TorrentCreatorDlg::showProgressBar(bool show)
{
  progressLbl->setVisible(show);
  progressBar->setVisible(show);
}

void TorrentCreatorDlg::on_checkAutoPieceSize_clicked(bool checked)
{
  comboPieceSize->setEnabled(!checked);
  if (checked) {
    updateOptimalPieceSize();
  }
}

void TorrentCreatorDlg::updateOptimalPieceSize()
{
  quint64 torrent_size = fsutils::computePathSize(textInputPath->text());
  qDebug("Torrent size is %lld", torrent_size);
  if (torrent_size == 0) return;
  int i = 0;
  qulonglong nb_pieces = 0;
  do {
    nb_pieces = (double)torrent_size/(m_piece_sizes.at(i)*1024.);
    qDebug("nb_pieces=%lld with piece_size=%s", nb_pieces, qPrintable(comboPieceSize->itemText(i)));
    if (nb_pieces <= NB_PIECES_MIN) {
      if (i > 1)
        --i;
      break;
    }
    if (nb_pieces < NB_PIECES_MAX) {
      qDebug("Good, nb_pieces=%lld < %d", nb_pieces, NB_PIECES_MAX);
      break;
    }
    ++i;
  }while(i<m_piece_sizes.size());
  comboPieceSize->setCurrentIndex(i);
}

void TorrentCreatorDlg::saveTrackerList()
{
  QIniSettings settings;
  settings.setValue("CreateTorrent/TrackerList", trackers_list->toPlainText());
}

void TorrentCreatorDlg::loadTrackerList()
{
  QIniSettings settings;
  trackers_list->setPlainText(settings.value("CreateTorrent/TrackerList", "").toString());
}

void TorrentCreatorDlg::saveSettings()
{
  QIniSettings settings;
  settings.setValue("CreateTorrent/dimensions", saveGeometry());
  settings.setValue("CreateTorrent/IgnoreRatio", checkIgnoreShareLimits->isChecked());
}

void TorrentCreatorDlg::loadSettings()
{
  QIniSettings settings;
  restoreGeometry(settings.value("CreateTorrent/dimensions").toByteArray());
  checkIgnoreShareLimits->setChecked(settings.value("CreateTorrent/IgnoreRatio").toBool());
}

void TorrentCreatorDlg::closeEvent(QCloseEvent *event)
{
  qDebug() << Q_FUNC_INFO;
  saveSettings();
  QDialog::closeEvent(event);
}
