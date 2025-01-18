/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015-2024  Vladimir Golovnev <glassez@yandex.ru>
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

#include <QtContainerFwd>
#include <QtTypes>
#include <QMetaType>
#include <QString>

#include "base/3rdparty/expected.hpp"
#include "base/pathfwd.h"
#include "base/tagset.h"
#include "sharelimitaction.h"
#include "torrentcontenthandler.h"

class QBitArray;
class QByteArray;
class QDateTime;
class QUrl;

namespace BitTorrent
{
    enum class DownloadPriority;

    class InfoHash;
    class PeerInfo;
    class Session;
    class TorrentID;
    class TorrentInfo;

    struct PeerAddress;
    struct SSLParameters;
    struct TrackerEntry;
    struct TrackerEntryStatus;

    // Using `Q_ENUM_NS()` without a wrapper namespace in our case is not advised
    // since `Q_NAMESPACE` cannot be used when the same namespace resides at different files.
    // https://www.kdab.com/new-qt-5-8-meta-object-support-namespaces/#comment-143779
    inline namespace TorrentOperatingModeNS
    {
        Q_NAMESPACE

        enum class TorrentOperatingMode
        {
            AutoManaged = 0,
            Forced = 1
        };

        Q_ENUM_NS(TorrentOperatingMode)
    }

    enum class TorrentState
    {
        Unknown = -1,

        ForcedDownloading,
        Downloading,
        ForcedDownloadingMetadata,
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

        StoppedDownloading,
        StoppedUploading,

        Moving,

        MissingFiles,
        Error
    };

    std::size_t qHash(TorrentState key, std::size_t seed = 0);

    class Torrent : public TorrentContentHandler
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(Torrent)

    public:
        enum class StopCondition
        {
            None = 0,
            MetadataReceived = 1,
            FilesChecked = 2
        };
        Q_ENUM(StopCondition)

        static const qreal USE_GLOBAL_RATIO;
        static const qreal NO_RATIO_LIMIT;

        static const int USE_GLOBAL_SEEDING_TIME;
        static const int NO_SEEDING_TIME_LIMIT;

        static const int USE_GLOBAL_INACTIVE_SEEDING_TIME;
        static const int NO_INACTIVE_SEEDING_TIME_LIMIT;

        static const qreal MAX_RATIO;
        static const int MAX_SEEDING_TIME;
        static const int MAX_INACTIVE_SEEDING_TIME;

        using TorrentContentHandler::TorrentContentHandler;

        virtual Session *session() const = 0;

        virtual InfoHash infoHash() const = 0;
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

        // 1. savePath() - the path where all the files and subfolders of torrent are stored.
        // 1.1 downloadPath() - the path where all the files and subfolders of torrent are stored until torrent has finished downloading.
        // 2. rootPath() - absolute path of torrent file tree (first common subfolder of torrent files); empty string if torrent has no root folder.
        // 3. contentPath() - absolute path of torrent content (root path for multifile torrents, absolute file path for singlefile torrents).
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
        // | C |           <empty>            | /home/user/torrents/file1                  |

        virtual bool isAutoTMMEnabled() const = 0;
        virtual void setAutoTMMEnabled(bool enabled) = 0;
        virtual Path savePath() const = 0;
        virtual void setSavePath(const Path &savePath) = 0;
        virtual Path downloadPath() const = 0;
        virtual void setDownloadPath(const Path &downloadPath) = 0;
        virtual Path rootPath() const = 0;
        virtual Path contentPath() const = 0;
        virtual QString category() const = 0;
        virtual bool belongsToCategory(const QString &category) const = 0;
        virtual bool setCategory(const QString &category) = 0;

        virtual TagSet tags() const = 0;
        virtual bool hasTag(const Tag &tag) const = 0;
        virtual bool addTag(const Tag &tag) = 0;
        virtual bool removeTag(const Tag &tag) = 0;
        virtual void removeAllTags() = 0;

        virtual int piecesCount() const = 0;
        virtual int piecesHave() const = 0;
        virtual qreal progress() const = 0;

        virtual QDateTime addedTime() const = 0;
        virtual QDateTime completedTime() const = 0;
        virtual QDateTime lastSeenComplete() const = 0;
        virtual qlonglong activeTime() const = 0;
        virtual qlonglong finishedTime() const = 0;
        virtual qlonglong timeSinceUpload() const = 0;
        virtual qlonglong timeSinceDownload() const = 0;
        virtual qlonglong timeSinceActivity() const = 0;

        // Share limits
        virtual qreal ratioLimit() const = 0;
        virtual void setRatioLimit(qreal limit) = 0;
        virtual int seedingTimeLimit() const = 0;
        virtual void setSeedingTimeLimit(int limit) = 0;
        virtual int inactiveSeedingTimeLimit() const = 0;
        virtual void setInactiveSeedingTimeLimit(int limit) = 0;
        virtual ShareLimitAction shareLimitAction() const = 0;
        virtual void setShareLimitAction(ShareLimitAction action) = 0;

        virtual PathList filePaths() const = 0;
        virtual PathList actualFilePaths() const = 0;

        virtual TorrentInfo info() const = 0;
        virtual bool isFinished() const = 0;
        virtual bool isStopped() const = 0;
        virtual bool isQueued() const = 0;
        virtual bool isForced() const = 0;
        virtual bool isChecking() const = 0;
        virtual bool isDownloading() const = 0;
        virtual bool isMoving() const = 0;
        virtual bool isUploading() const = 0;
        virtual bool isCompleted() const = 0;
        virtual bool isActive() const = 0;
        virtual bool isInactive() const = 0;
        virtual bool isErrored() const = 0;
        virtual bool isSequentialDownload() const = 0;
        virtual bool hasFirstLastPiecePriority() const = 0;
        virtual TorrentState state() const = 0;
        virtual bool hasMissingFiles() const = 0;
        virtual bool hasError() const = 0;
        virtual int queuePosition() const = 0;
        virtual QList<TrackerEntryStatus> trackers() const = 0;
        virtual QList<QUrl> urlSeeds() const = 0;
        virtual QString error() const = 0;
        virtual qlonglong totalDownload() const = 0;
        virtual qlonglong totalUpload() const = 0;
        virtual qlonglong eta() const = 0;
        virtual int seedsCount() const = 0;
        virtual int peersCount() const = 0;
        virtual int leechsCount() const = 0;
        virtual int totalSeedsCount() const = 0;
        virtual int totalPeersCount() const = 0;
        virtual int totalLeechersCount() const = 0;
        virtual int downloadLimit() const = 0;
        virtual int uploadLimit() const = 0;
        virtual bool superSeeding() const = 0;
        virtual bool isDHTDisabled() const = 0;
        virtual bool isPEXDisabled() const = 0;
        virtual bool isLSDDisabled() const = 0;
        virtual QList<PeerInfo> peers() const = 0;
        virtual QBitArray pieces() const = 0;
        virtual QBitArray downloadingPieces() const = 0;
        virtual QList<int> pieceAvailability() const = 0;
        virtual qreal distributedCopies() const = 0;
        virtual qreal maxRatio() const = 0;
        virtual int maxSeedingTime() const = 0;
        virtual int maxInactiveSeedingTime() const = 0;
        virtual qreal realRatio() const = 0;
        virtual qreal popularity() const = 0;
        virtual int uploadPayloadRate() const = 0;
        virtual int downloadPayloadRate() const = 0;
        virtual qlonglong totalPayloadUpload() const = 0;
        virtual qlonglong totalPayloadDownload() const = 0;
        virtual int connectionsCount() const = 0;
        virtual int connectionsLimit() const = 0;
        virtual qlonglong nextAnnounce() const = 0;

        virtual void setName(const QString &name) = 0;
        virtual void setSequentialDownload(bool enable) = 0;
        virtual void setFirstLastPiecePriority(bool enabled) = 0;
        virtual void stop() = 0;
        virtual void start(TorrentOperatingMode mode = TorrentOperatingMode::AutoManaged) = 0;
        virtual void forceReannounce(int index = -1) = 0;
        virtual void forceDHTAnnounce() = 0;
        virtual void forceRecheck() = 0;
        virtual void setUploadLimit(int limit) = 0;
        virtual void setDownloadLimit(int limit) = 0;
        virtual void setSuperSeeding(bool enable) = 0;
        virtual void setDHTDisabled(bool disable) = 0;
        virtual void setPEXDisabled(bool disable) = 0;
        virtual void setLSDDisabled(bool disable) = 0;
        virtual void addTrackers(QList<TrackerEntry> trackers) = 0;
        virtual void removeTrackers(const QStringList &trackers) = 0;
        virtual void replaceTrackers(QList<TrackerEntry> trackers) = 0;
        virtual void addUrlSeeds(const QList<QUrl> &urlSeeds) = 0;
        virtual void removeUrlSeeds(const QList<QUrl> &urlSeeds) = 0;
        virtual bool connectPeer(const PeerAddress &peerAddress) = 0;
        virtual void clearPeers() = 0;
        virtual void setMetadata(const TorrentInfo &torrentInfo) = 0;

        virtual StopCondition stopCondition() const = 0;
        virtual void setStopCondition(StopCondition stopCondition) = 0;
        virtual SSLParameters getSSLParameters() const = 0;
        virtual void setSSLParameters(const SSLParameters &sslParams) = 0;

        virtual QString createMagnetURI() const = 0;
        virtual nonstd::expected<QByteArray, QString> exportToBuffer() const = 0;
        virtual nonstd::expected<void, QString> exportToFile(const Path &path) const = 0;

        virtual void fetchPeerInfo(std::function<void (QList<PeerInfo>)> resultHandler) const = 0;
        virtual void fetchURLSeeds(std::function<void (QList<QUrl>)> resultHandler) const = 0;
        virtual void fetchPieceAvailability(std::function<void (QList<int>)> resultHandler) const = 0;
        virtual void fetchDownloadingPieces(std::function<void (QBitArray)> resultHandler) const = 0;

        TorrentID id() const;
        bool isRunning() const;
        qlonglong remainingSize() const;

        void toggleSequentialDownload();
        void toggleFirstLastPiecePriority();
    };
}

Q_DECLARE_METATYPE(BitTorrent::TorrentState)
