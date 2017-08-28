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
#include "trackersadditiondlg.h"

#include <QStringList>
#include <QMessageBox>
#include <QFile>
#include <QUrl>

#include "base/utils/misc.h"
#include "base/utils/fs.h"
#include "base/net/downloadmanager.h"
#include "base/net/downloadhandler.h"
#include "base/bittorrent/trackerentry.h"
#include "base/bittorrent/torrenthandle.h"
#include "guiiconprovider.h"
#include "ui_trackersadditiondlg.h"

TrackersAdditionDlg::TrackersAdditionDlg(QWidget *parent, BitTorrent::TorrentHandle *const torrent)
    : QDialog(parent)
    , m_ui(new Ui::TrackersAdditionDlg())
    , m_torrent(torrent)
{
    m_ui->setupUi(this);
    // Icons
    m_ui->uTorrentListButton->setIcon(GuiIconProvider::instance()->getIcon("download"));
}

TrackersAdditionDlg::~TrackersAdditionDlg()
{
    delete m_ui;
}

QStringList TrackersAdditionDlg::newTrackers() const
{
    QStringList cleanTrackers;
    foreach (QString url, m_ui->trackers_list->toPlainText().split("\n")) {
        url = url.trimmed();
        if (!url.isEmpty())
            cleanTrackers << url;
    }
    return cleanTrackers;
}

void TrackersAdditionDlg::on_uTorrentListButton_clicked()
{
    m_ui->uTorrentListButton->setEnabled(false);
    Net::DownloadHandler *handler = Net::DownloadManager::instance()->downloadUrl(m_ui->list_url->text(), true);
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
        m_ui->uTorrentListButton->setEnabled(true);
        Utils::Fs::forceRemove(path);
        return;
    }

    // Load from torrent handle
    QList<BitTorrent::TrackerEntry> existingTrackers = m_torrent->trackers();
    // Load from current user list
    QStringList tmp = m_ui->trackers_list->toPlainText().split("\n");
    foreach (const QString &user_url, tmp) {
        BitTorrent::TrackerEntry userTracker(user_url);
        if (!existingTrackers.contains(userTracker))
            existingTrackers << userTracker;
    }

    // Add new trackers to the list
    if (!m_ui->trackers_list->toPlainText().isEmpty() && !m_ui->trackers_list->toPlainText().endsWith("\n"))
        m_ui->trackers_list->insertPlainText("\n");
    int nb = 0;
    while (!list_file.atEnd()) {
        const QString line = list_file.readLine().trimmed();
        if (line.isEmpty()) continue;
        BitTorrent::TrackerEntry newTracker(line);
        if (!existingTrackers.contains(newTracker)) {
            m_ui->trackers_list->insertPlainText(line + "\n");
            ++nb;
        }
    }
    // Clean up
    list_file.close();
    Utils::Fs::forceRemove(path);
    //To restore the cursor ...
    setCursor(Qt::ArrowCursor);
    m_ui->uTorrentListButton->setEnabled(true);
    // Display information message if necessary
    if (nb == 0)
        QMessageBox::information(this, tr("No change"), tr("No additional trackers were found."), QMessageBox::Ok);
}

void TrackersAdditionDlg::getTrackerError(const QString &, const QString &error)
{
    //To restore the cursor ...
    setCursor(Qt::ArrowCursor);
    m_ui->uTorrentListButton->setEnabled(true);
    QMessageBox::warning(this, tr("Download error"), tr("The trackers list could not be downloaded, reason: %1").arg(error), QMessageBox::Ok);
}

QStringList TrackersAdditionDlg::askForTrackers(QWidget *parent, BitTorrent::TorrentHandle *const torrent)
{
    QStringList trackers;
    TrackersAdditionDlg dlg(parent, torrent);
    if (dlg.exec() == QDialog::Accepted)
        return dlg.newTrackers();

    return trackers;
}
