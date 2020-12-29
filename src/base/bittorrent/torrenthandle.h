/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
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

#pragma once

#include <QMetaType>
#include <QString>
#include <QtContainerFwd>

#include "abstractfilestorage.h"

class QBitArray;
class QDateTime;
class QUrl;

namespace BitTorrent
{
    enum class DownloadPriority;
    class InfoHash;
    class PeerInfo;
    class TorrentInfo;
    class TrackerEntry;
    struct PeerAddress;

    enum class TorrentOperatingMode
    {
        AutoManaged = 0,
        Forced = 1
    };

    enum class TorrentState
    {
        Unknown = -1,

        ForcedDownloading,
        Downloading,
        DownloadingMetadata,
        StalledDownloading,

        ForcedUploading,
        Uploading,
        StalledUploading,

        CheckingResumeData,
        QueuedDownloading,
        QueuedUploading,

        CheckingUploading,
        CheckingDownloading,

        PausedDownloading,
        PausedUploading,

        Moving,

        MissingFiles,
        Error
    };

    struct TrackerInfo
    {
        QString lastMessage;
        int numPeers = 0;
    };

    uint qHash(TorrentState key, uint seed);

    class TorrentHandle : public AbstractFileStorage
    {
    public:
        static const qreal USE_GLOBAL_RATIO;
        static const qreal NO_RATIO_LIMIT;

        static const int USE_GLOBAL_SEEDING_TIME;
        static const int NO_SEEDING_TIME_LIMIT;

        static const qreal MAX_RATIO;
        static const int MAX_SEEDING_TIME;

        virtual ~TorrentHandle() = default;

        virtual InfoHash hash() const = 0;
        virtual QString name() const = 0;
        virtual QDateTime creationDate() const = 0;
        virtual QString creator() const = 0;
        virtual QString comment() const = 0;
        virtual bool isPrivate() const = 0;
        virtual qlonglong totalSize() const = 0;
        virtual qlonglong wantedSize() const = 0;
        virtual qlonglong completedSize() const = 0;
        virtual qlonglong pieceLength() const = 0;
        virtual qlonglong wastedSize() const = 0;
        virtual QString currentTracker() const = 0;

        // 1. savePath() - the path where all the files and subfolders of torrent are stored (as always).
        // 2. rootPath() - absolute path of torrent file tree (save path + first item from 1st torrent file path).
        // 3. contentPath() - absolute path of torrent content (root path for multifile torrents, absolute file path for singlefile torrents).
        //
        // These methods have 'actual' parameter (defaults to false) which allow to get actual or final path variant.
        //
        // Examples.
        // Suppose we have three torrent with following structures and save path `/home/user/torrents`:
        //
        // Torrent A (multifile)
        //
        // torrentA/
        //    subdir1/
        //       subdir2/
        //          file1
        //          file2
        //       file3
        //    file4
        //
        //
        // Torrent A* (Torrent A in "strip root folder" mode)
        //
        //
        // Torrent B (singlefile)
        //
        // torrentB/
        //    subdir1/
        //           file1
        //
        //
        // Torrent C (singlefile)
        //
        // file1
        //
        //
        // Results:
        // |   |           rootPath           |                contentPath                 |
        // |---|------------------------------|--------------------------------------------|
        // | A | /home/user/torrents/torrentA | /home/user/torrents/torrentA               |
        // | A*|           <empty>            | /home/user/torrents                        |
        // | B | /home/user/torrents/torrentB | /home/user/torrents/torrentB/subdir1/file1 |
        // | C | /home/user/torrents/file1    | /home/user/torrents/file1                  |

        virtual QString savePath(bool actual = false) const = 0;
        virtual QString rootPath(bool actual = false) const = 0;
        virtual QString contentPath(bool actual = false) const = 0;

        virtual bool useTempPath() const = 0;

        virtual bool isAutoTMMEnabled() const = 0;
        virtual void setAutoTMMEnabled(bool enabled) = 0;
        virtual QString category() const = 0;
        virtual bool belongsToCategory(const QString &category) const = 0;
        virtual bool setCategory(const QString &category) = 0;

        virtual QSet<QString> tags() const = 0;
        virtual bool hasTag(const QString &tag) const = 0;
        virtual bool addTag(const QString &tag) = 0;
        virtual bool removeTag(const QString &tag) = 0;
        virtual void removeAllTags() = 0;

        virtual int piecesCount() const = 0;
        virtual int piecesHave() const = 0;
        virtual qreal progress() const = 0;
        virtual QDateTime addedTime() const = 0;
        virtual qreal ratioLimit() const = 0;
        virtual int seedingTimeLimit() const = 0;

        virtual QStringList absoluteFilePaths() const = 0;
        virtual QVector<DownloadPriority> filePriorities() const = 0;

        virtual TorrentInfo info() const = 0;
        virtual bool isSeed() const = 0;
        virtual bool isPaused() const = 0;
        virtual bool isQueued() const = 0;
        virtual bool isForced() const = 0;
        virtual bool isChecking() const = 0;
        virtual bool isDownloading() const = 0;
        virtual bool isUploading() const = 0;
        virtual bool isCompleted() const = 0;
        virtual bool isActive() const = 0;
        virtual bool isInactive() const = 0;
        virtual bool isErrored() const = 0;
        virtual bool isSequentialDownload() const = 0;
        virtual bool hasFirstLastPiecePriority() const = 0;
        virtual TorrentState state() const = 0;
        virtual bool hasMetadata() const = 0;
        virtual bool hasMissingFiles() const = 0;
        virtual bool hasError() const = 0;
        virtual bool hasFilteredPieces() const = 0;
        virtual int queuePosition() const = 0;
        virtual QVector<TrackerEntry> trackers() const = 0;
        virtual QHash<QString, TrackerInfo> trackerInfos() const = 0;
        virtual QVector<QUrl> urlSeeds() const = 0;
        virtual QString error() const = 0;
        virtual qlonglong totalDownload() const = 0;
        virtual qlonglong totalUpload() const = 0;
        virtual qlonglong activeTime() const = 0;
        virtual qlonglong finishedTime() const = 0;
        virtual qlonglong seedingTime() const = 0;
        virtual qlonglong eta() const = 0;
        virtual QVector<qreal> filesProgress() const = 0;
        virtual int seedsCount() const = 0;
        virtual int peersCount() const = 0;
        virtual int leechsCount() const = 0;
        virtual int totalSeedsCount() const = 0;
        virtual int totalPeersCount() const = 0;
        virtual int totalLeechersCount() const = 0;
        virtual int completeCount() const = 0;
        virtual int incompleteCount() const = 0;
        virtual QDateTime lastSeenComplete() const = 0;
        virtual QDateTime completedTime() const = 0;
        virtual qlonglong timeSinceUpload() const = 0;
        virtual qlonglong timeSinceDownload() const = 0;
        virtual qlonglong timeSinceActivity() const = 0;
        virtual int downloadLimit() const = 0;
        virtual int uploadLimit() const = 0;
        virtual bool superSeeding() const = 0;
        virtual bool isDHTDisabled() const = 0;
        virtual bool isPEXDisabled() const = 0;
        virtual bool isLSDDisabled() const = 0;
        virtual QVector<PeerInfo> peers() const = 0;
        virtual QBitArray pieces() const = 0;
        virtual QBitArray downloadingPieces() const = 0;
        virtual QVector<int> pieceAvailability() const = 0;
        virtual qreal distributedCopies() const = 0;
        virtual qreal maxRatio() const = 0;
        virtual int maxSeedingTime() const = 0;
        virtual qreal realRatio() const = 0;
        virtual int uploadPayloadRate() const = 0;
        virtual int downloadPayloadRate() const = 0;
        virtual qlonglong totalPayloadUpload() const = 0;
        virtual qlonglong totalPayloadDownload() const = 0;
        virtual int connectionsCount() const = 0;
        virtual int connectionsLimit() const = 0;
        virtual qlonglong nextAnnounce() const = 0;
        /**
         * @brief fraction of file pieces that are available at least from one peer
         *
         * This is not the same as torrrent availability, it is just a fraction of pieces
         * that can be downloaded right now. It varies between 0 to 1.
         */
        virtual QVector<qreal> availableFileFractions() const = 0;

        virtual void setName(const QString &name) = 0;
        virtual void setSequentialDownload(bool enable) = 0;
        virtual void setFirstLastPiecePriority(bool enabled) = 0;
        virtual void pause() = 0;
        virtual void resume(TorrentOperatingMode mode = TorrentOperatingMode::AutoManaged) = 0;
        virtual void move(QString path) = 0;
        virtual void forceReannounce(int index = -1) = 0;
        virtual void forceDHTAnnounce() = 0;
        virtual void forceRecheck() = 0;
        virtual void prioritizeFiles(const QVector<DownloadPriority> &priorities) = 0;
        virtual void setRatioLimit(qreal limit) = 0;
        virtual void setSeedingTimeLimit(int limit) = 0;
        virtual void setUploadLimit(int limit) = 0;
        virtual void setDownloadLimit(int limit) = 0;
        virtual void setSuperSeeding(bool enable) = 0;
        virtual void setDHTDisabled(bool disable) = 0;
        virtual void setPEXDisabled(bool disable) = 0;
        virtual void setLSDDisabled(bool disable) = 0;
        virtual void flushCache() const = 0;
        virtual void addTrackers(const QVector<TrackerEntry> &trackers) = 0;
        virtual void replaceTrackers(const QVector<TrackerEntry> &trackers) = 0;
        virtual void addUrlSeeds(const QVector<QUrl> &urlSeeds) = 0;
        virtual void removeUrlSeeds(const QVector<QUrl> &urlSeeds) = 0;
        virtual bool connectPeer(const PeerAddress &peerAddress) = 0;
        virtual void clearPeers() = 0;

        virtual QString createMagnetURI() const = 0;

        bool isResumed() const;
        qlonglong remainingSize() const;

        void toggleSequentialDownload();
        void toggleFirstLastPiecePriority();
    };
}

Q_DECLARE_METATYPE(BitTorrent::TorrentState)
