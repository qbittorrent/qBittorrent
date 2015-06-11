/*
 * Bittorrent Client using Qt and libtorrent.
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

#include <QStringList>
#include <QMessageBox>
#include <QFile>
#include <QUrl>

#include "core/utils/misc.h"
#include "core/utils/fs.h"
#include "core/net/downloadmanager.h"
#include "core/net/downloadhandler.h"
#include "core/bittorrent/trackerentry.h"
#include "core/bittorrent/torrenthandle.h"
#include "guiiconprovider.h"
#include "trackersadditiondlg.h"

TrackersAdditionDlg::TrackersAdditionDlg(BitTorrent::TorrentHandle *const torrent, QWidget *parent)
    : QDialog(parent)
    , m_torrent(torrent)
{
    setupUi(this);
    // Icons
    uTorrentListButton->setIcon(GuiIconProvider::instance()->getIcon("download"));
}

QStringList TrackersAdditionDlg::newTrackers() const
{
    return trackers_list->toPlainText().trimmed().split("\n");
}

void TrackersAdditionDlg::on_uTorrentListButton_clicked()
{
    uTorrentListButton->setEnabled(false);
    Net::DownloadHandler *handler = Net::DownloadManager::instance()->downloadUrl(QString("http://www.torrentz.com/announce_%1").arg(m_torrent->hash()));
    connect(handler, SIGNAL(downloadFinished(QString, QString)), this, SLOT(parseUTorrentList(QString, QString)));
    connect(handler, SIGNAL(downloadFailed(QString, QString)), this, SLOT(getTrackerError(QString, QString)));
    //Just to show that it takes times
    setCursor(Qt::WaitCursor);
}

void TrackersAdditionDlg::parseUTorrentList(const QString &, const QString &path)
{
    QFile list_file(path);
    if (!list_file.open(QFile::ReadOnly)) {
        QMessageBox::warning(this, tr("I/O Error"), tr("Error while trying to open the downloaded file."), QMessageBox::Ok);
        setCursor(Qt::ArrowCursor);
        uTorrentListButton->setEnabled(true);
        Utils::Fs::forceRemove(path);
        return;
    }

    // Load from torrent handle
    QList<BitTorrent::TrackerEntry> existingTrackers = m_torrent->trackers();
    // Load from current user list
    QStringList tmp = trackers_list->toPlainText().split("\n");
    foreach (const QString &user_url, tmp) {
        BitTorrent::TrackerEntry userTracker(user_url);
        if (!existingTrackers.contains(userTracker))
            existingTrackers << userTracker;
    }

    // Add new trackers to the list
    if (!trackers_list->toPlainText().isEmpty() && !trackers_list->toPlainText().endsWith("\n"))
        trackers_list->insertPlainText("\n");
    int nb = 0;
    while (!list_file.atEnd()) {
        const QByteArray line = list_file.readLine().trimmed();
        if (line.isEmpty()) continue;
        BitTorrent::TrackerEntry newTracker(line);
        if (!existingTrackers.contains(newTracker)) {
            trackers_list->insertPlainText(line + "\n");
            ++nb;
        }
    }
    // Clean up
    list_file.close();
    Utils::Fs::forceRemove(path);
    //To restore the cursor ...
    setCursor(Qt::ArrowCursor);
    uTorrentListButton->setEnabled(true);
    // Display information message if necessary
    if (nb == 0)
        QMessageBox::information(this, tr("No change"), tr("No additional trackers were found."), QMessageBox::Ok);
}

void TrackersAdditionDlg::getTrackerError(const QString &, const QString &error)
{
    //To restore the cursor ...
    setCursor(Qt::ArrowCursor);
    uTorrentListButton->setEnabled(true);
    QMessageBox::warning(this, tr("Download error"), tr("The trackers list could not be downloaded, reason: %1").arg(error), QMessageBox::Ok);
}

QStringList TrackersAdditionDlg::askForTrackers(BitTorrent::TorrentHandle *const torrent)
{
    QStringList trackers;
    TrackersAdditionDlg dlg(torrent);
    if (dlg.exec() == QDialog::Accepted)
        return dlg.newTrackers();

    return trackers;
}
