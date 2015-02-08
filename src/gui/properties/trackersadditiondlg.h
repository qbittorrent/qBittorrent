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

#ifndef TRACKERSADDITION_H
#define TRACKERSADDITION_H

#include <QDialog>
#include <QStringList>
#include <QMessageBox>
#include <QFile>
#include <QUrl>
#include "guiiconprovider.h"
#include "core/misc.h"
#include "ui_trackersadditiondlg.h"
#include "core/net/downloadmanager.h"
#include "core/net/downloadhandler.h"
#include "core/bittorrent/torrenthandle.h"
#include "core/fs_utils.h"

class TrackersAdditionDlg : public QDialog, private Ui::TrackersAdditionDlg{
  Q_OBJECT

private:
  BitTorrent::TorrentHandle *const m_torrent;

public:
  TrackersAdditionDlg(BitTorrent::TorrentHandle *const torrent, QWidget *parent = 0): QDialog(parent), m_torrent(torrent) {
    setupUi(this);
    // Icons
    uTorrentListButton->setIcon(GuiIconProvider::instance()->getIcon("download"));
  }

  ~TrackersAdditionDlg() {}

  QStringList newTrackers() const {
    return trackers_list->toPlainText().trimmed().split("\n");
  }

public slots:
  void on_uTorrentListButton_clicked() {
    uTorrentListButton->setEnabled(false);
    Net::DownloadHandler *handler = Net::DownloadManager::instance()->downloadUrl(QString("http://www.torrentz.com/announce_%1").arg(m_torrent->hash()));
    connect(handler, SIGNAL(downloadFinished(QString, QString)), this, SLOT(parseUTorrentList(QString, QString)));
    connect(handler, SIGNAL(downloadFailed(QString, QString)), this, SLOT(getTrackerError(QString, QString)));
    //Just to show that it takes times
    setCursor(Qt::WaitCursor);
  }

  void parseUTorrentList(const QString &, const QString &path) {
    QFile list_file(path);
    if (!list_file.open(QFile::ReadOnly)) {
      QMessageBox::warning(this, tr("I/O Error"), tr("Error while trying to open the downloaded file."), QMessageBox::Ok);
      setCursor(Qt::ArrowCursor);
      uTorrentListButton->setEnabled(true);
      fsutils::forceRemove(path);
      return;
    }
    QList<QUrl> existingTrackers;
    // Load from torrent handle
    foreach (const QString &tracker, m_torrent->trackers())
      existingTrackers << tracker;

    // Load from current user list
    QStringList tmp = trackers_list->toPlainText().split("\n");
    foreach (const QString &user_url_str, tmp) {
      QUrl user_url(user_url_str);
      if (!existingTrackers.contains(user_url))
        existingTrackers << user_url;
    }
    // Add new trackers to the list
    if (!trackers_list->toPlainText().isEmpty() && !trackers_list->toPlainText().endsWith("\n"))
      trackers_list->insertPlainText("\n");
    int nb = 0;
    while (!list_file.atEnd()) {
      const QByteArray line = list_file.readLine().trimmed();
      if (line.isEmpty()) continue;
      QUrl url(line);
      if (!existingTrackers.contains(url)) {
        trackers_list->insertPlainText(line + "\n");
        ++nb;
      }
    }
    // Clean up
    list_file.close();
    fsutils::forceRemove(path);
    //To restore the cursor ...
    setCursor(Qt::ArrowCursor);
    uTorrentListButton->setEnabled(true);
    // Display information message if necessary
    if (nb == 0) {
      QMessageBox::information(this, tr("No change"), tr("No additional trackers were found."), QMessageBox::Ok);
    }
  }

  void getTrackerError(const QString &, const QString &error) {
    //To restore the cursor ...
    setCursor(Qt::ArrowCursor);
    uTorrentListButton->setEnabled(true);
    QMessageBox::warning(this, tr("Download error"), tr("The trackers list could not be downloaded, reason: %1").arg(error), QMessageBox::Ok);
  }

public:

  static QStringList askForTrackers(BitTorrent::TorrentHandle *const torrent) {
    QStringList trackers;
    TrackersAdditionDlg dlg(torrent);
    if (dlg.exec() == QDialog::Accepted) {
      return dlg.newTrackers();
    }
    return trackers;
  }
};

#endif
