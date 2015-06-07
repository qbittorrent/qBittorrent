/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
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
 */

#ifndef SESSIONPRIVATE_H
#define SESSIONPRIVATE_H

class QString;
class QUrl;
template<typename T> class QList;

namespace libtorrent
{
    class entry;
}

namespace BitTorrent
{
    class TorrentHandle;
    class TrackerEntry;
}

struct SessionPrivate
{
    virtual bool isQueueingEnabled() const = 0;
    virtual bool isTempPathEnabled() const = 0;
    virtual bool isAppendExtensionEnabled() const = 0;
    virtual bool useAppendLabelToSavePath() const = 0;
#ifndef DISABLE_COUNTRIES_RESOLUTION
    virtual bool isResolveCountriesEnabled() const = 0;
#endif
    virtual QString defaultSavePath() const = 0;
    virtual QString tempPath() const = 0;
    virtual qreal globalMaxRatio() const = 0;

    virtual void handleTorrentRatioLimitChanged(BitTorrent::TorrentHandle *const torrent) = 0;
    virtual void handleTorrentSavePathChanged(BitTorrent::TorrentHandle *const torrent) = 0;
    virtual void handleTorrentLabelChanged(BitTorrent::TorrentHandle *const torrent, const QString &oldLabel) = 0;
    virtual void handleTorrentMetadataReceived(BitTorrent::TorrentHandle *const torrent) = 0;
    virtual void handleTorrentPaused(BitTorrent::TorrentHandle *const torrent) = 0;
    virtual void handleTorrentResumed(BitTorrent::TorrentHandle *const torrent) = 0;
    virtual void handleTorrentChecked(BitTorrent::TorrentHandle *const torrent) = 0;
    virtual void handleTorrentFinished(BitTorrent::TorrentHandle *const torrent) = 0;
    virtual void handleTorrentTrackersAdded(BitTorrent::TorrentHandle *const torrent, const QList<BitTorrent::TrackerEntry> &newTrackers) = 0;
    virtual void handleTorrentTrackersRemoved(BitTorrent::TorrentHandle *const torrent, const QList<BitTorrent::TrackerEntry> &deletedTrackers) = 0;
    virtual void handleTorrentTrackersChanged(BitTorrent::TorrentHandle *const torrent) = 0;
    virtual void handleTorrentUrlSeedsAdded(BitTorrent::TorrentHandle *const torrent, const QList<QUrl> &newUrlSeeds) = 0;
    virtual void handleTorrentUrlSeedsRemoved(BitTorrent::TorrentHandle *const torrent, const QList<QUrl> &urlSeeds) = 0;
    virtual void handleTorrentResumeDataReady(BitTorrent::TorrentHandle *const torrent, const libtorrent::entry &data) = 0;
    virtual void handleTorrentResumeDataFailed(BitTorrent::TorrentHandle *const torrent) = 0;
    virtual void handleTorrentTrackerReply(BitTorrent::TorrentHandle *const torrent, const QString &trackerUrl) = 0;
    virtual void handleTorrentTrackerWarning(BitTorrent::TorrentHandle *const torrent, const QString &trackerUrl) = 0;
    virtual void handleTorrentTrackerError(BitTorrent::TorrentHandle *const torrent, const QString &trackerUrl) = 0;
    virtual void handleTorrentTrackerAuthenticationRequired(BitTorrent::TorrentHandle *const torrent, const QString &trackerUrl) = 0;

protected:
    ~SessionPrivate() {}
};

#endif // SESSIONPRIVATE_H
