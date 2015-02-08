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

#ifndef BITTORRENT_TORRENTHANDLE_H
#define BITTORRENT_TORRENTHANDLE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QDateTime>

#include "infohash.h"
#include "torrentinfo.h"

namespace BitTorrent
{

class TorrentState
{
public:
    enum
    {
        Unknown = -1,

        Error,

        Uploading,
        PausedUploading,
        QueuedUploading,
        StalledUploading,
        CheckingUploading,
        ForcedUploading,

        Allocating,

        DownloadingMetadata,
        Downloading,
        PausedDownloading,
        QueuedDownloading,
        StalledDownloading,
        CheckingDownloading,
        ForcedDownloading
    };

    TorrentState(int value);

    operator int() const;
    QString toString() const;

private:
    int m_value;
};

struct PeerAddress;
class Session;
class PeerInfo;
class TrackerEntry;
struct TrackerInfo;

class TorrentHandle : public QObject
{
public:
    static const qreal USE_GLOBAL_RATIO;
    static const qreal NO_RATIO_LIMIT;

    static const qreal MAX_RATIO;

    explicit TorrentHandle(QObject *parent = 0);

    virtual bool isValid() const = 0;
    virtual InfoHash hash() const = 0;
    virtual QString name() const = 0;
    virtual QDateTime creationDate() const = 0;
    virtual QString creator() const = 0;
    virtual QString comment() const = 0;
    virtual bool isPrivate() const = 0;
    virtual qlonglong totalSize() const = 0;
    virtual qlonglong wantedSize() const = 0;
    virtual qlonglong completedSize() const = 0;
    virtual qlonglong uncompletedSize() const = 0;
    virtual qlonglong pieceLength() const = 0;
    virtual qlonglong wastedSize() const = 0;
    virtual QString currentTracker() const = 0;
    virtual QString actualSavePath() const = 0;
    virtual QString savePath() const = 0;
    virtual QString rootPath() const = 0;
    virtual QString savePathParsed() const = 0;
    virtual int filesCount() const = 0;
    virtual int piecesCount() const = 0;
    virtual qreal progress() const = 0;
    virtual QString label() const = 0;
    virtual QDateTime addedTime() const = 0;
    virtual qreal ratioLimit() const = 0;

    virtual QString firstFileSavePath() const = 0;
    virtual QString filePath(int index) const = 0;
    virtual QString fileName(int index) const = 0;
    virtual QString origFilePath(int index) const = 0;
    virtual qlonglong fileSize(int index) const = 0;
    virtual QStringList absoluteFilePaths() const = 0;
    virtual QStringList absoluteFilePathsUnneeded() const = 0;
    virtual QPair<int, int> fileExtremityPieces(int index) const = 0;
    virtual QVector<int> filePriorities() const = 0;

    virtual TorrentInfo info() const = 0;
    virtual bool isPaused() const = 0;
    virtual bool isQueued() const = 0;
    virtual bool isSeed() const = 0;
    virtual bool isForced() const = 0;
    virtual bool isChecking() const = 0;
    virtual bool isSequentialDownload() const = 0;
    virtual bool hasFirstLastPiecePriority() const = 0;
    virtual TorrentState state() const = 0;
    virtual bool hasMetadata() const = 0;
    virtual bool hasMissingFiles() const = 0;
    virtual bool hasError() const = 0;
    virtual bool hasFilteredPieces() const = 0;
    virtual bool isFilePreviewPossible() const = 0;
    virtual int queuePosition() const = 0;
    virtual QStringList trackers() const = 0;
    virtual QList<TrackerEntry> trackerEntries() const = 0;
    virtual QHash<QString, TrackerInfo> trackerInfos() const = 0;
    virtual QStringList urlSeeds() const = 0;
    virtual QString error() const = 0;
    virtual qlonglong totalDownload() const = 0;
    virtual qlonglong totalUpload() const = 0;
    virtual int activeTime() const = 0;
    virtual int seedingTime() const = 0;
    virtual qulonglong eta() const = 0;
    virtual QVector<qreal> filesProgress() const = 0;
    virtual int seedsCount() const = 0;
    virtual int peersCount() const = 0;
    virtual int leechsCount() const = 0;
    virtual int completeCount() const = 0;
    virtual int incompleteCount() const = 0;
    virtual QDateTime lastSeenComplete() const = 0;
    virtual QDateTime completedTime() const = 0;
    virtual int timeSinceUpload() const = 0;
    virtual int timeSinceDownload() const = 0;
    virtual int downloadLimit() const = 0;
    virtual int uploadLimit() const = 0;
    virtual bool superSeeding() const = 0;
    virtual QList<BitTorrent::PeerInfo> peers() const = 0;
    virtual QBitArray pieces() const = 0;
    virtual QBitArray downloadingPieces() const = 0;
    virtual QVector<int> pieceAvailability() const = 0;
    virtual qreal distributedCopies() const = 0;
    virtual qreal maxRatio(bool *usesGlobalRatio = 0) const = 0;
    virtual qreal realRatio() const = 0;
    virtual int uploadPayloadRate() const = 0;
    virtual int downloadPayloadRate() const = 0;
    virtual int totalPayloadUpload() const = 0;
    virtual int totalPayloadDownload() const = 0;
    virtual int connectionsCount() const = 0;
    virtual int connectionsLimit() const = 0;
    virtual qlonglong nextAnnounce() const = 0;

    virtual void setName(const QString &name) = 0;
    virtual void setLabel(const QString &label) = 0;
    virtual void setSequentialDownload(bool b) = 0;
    virtual void toggleSequentialDownload() = 0;
    virtual void toggleFirstLastPiecePriority() = 0;
    virtual void setFirstLastPiecePriority(bool b) = 0;
    virtual void pause() = 0;
    virtual void resume(bool forced = false) = 0;
    virtual void move(QString path) = 0;
    virtual void forceReannounce(int secs = 0, int index = -1) = 0;
    virtual void forceDHTAnnounce() = 0;
    virtual void forceRecheck() = 0;
    virtual void setTrackerLogin(const QString &username, const QString &password) = 0;
    virtual void renameFile(int index, const QString &name) = 0;
    virtual bool saveTorrentFile(const QString &path) = 0;
    virtual void prioritizeFiles(const QVector<int> &files) = 0;
    virtual void setFilePriority(int index, int priority) = 0;
    virtual void setRatioLimit(qreal limit) = 0;
    virtual void setUploadLimit(int limit) = 0;
    virtual void setDownloadLimit(int limit) = 0;
    virtual void setSuperSeeding(bool enable) = 0;
    virtual void flushCache() = 0;
    virtual void replaceTrackers(QList<TrackerEntry> trackers) = 0;
    virtual bool addUrlSeed(const QString &urlSeed) = 0;
    virtual void removeUrlSeed(const QString &urlSeed) = 0;
    virtual bool connectPeer(const PeerAddress &peerAddress) = 0;
    virtual bool addTracker(const QString &tracker) = 0;

    virtual QString toMagnetUri() const = 0;
};

}

#endif // BITTORRENT_TORRENTHANDLE_H
