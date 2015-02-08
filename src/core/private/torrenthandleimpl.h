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

#ifndef TORRENTHANDLEIMPL_H
#define TORRENTHANDLEIMPL_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QDateTime>

#include <libtorrent/torrent_handle.hpp>

#include "core/tristatebool.h"
#include "core/bittorrent/torrenthandle.h"
#include "speedmonitor.h"

namespace BitTorrent
{

struct PeerAddress;
class PeerInfo;
class TrackerEntry;
class TorrentState;
struct TrackerInfo;
struct AddTorrentParams;

}

struct AddTorrentData
{
    bool resumed;
    // for both new and resumed torrents
    QString name;
    QString label;
    QString savePath;
    bool disableTempPath;
    bool sequential;
    bool hasSeedStatus;
    // for new torrents
    TriStateBool addPaused;
    QVector<int> filePriorities;
    bool ignoreShareRatio;
    // for resumed torrents
    QDateTime addedTime;
    qreal ratioLimit;

    AddTorrentData() {}
    AddTorrentData(const BitTorrent::AddTorrentParams &in);
};

class SessionImpl;

class TorrentHandleImpl : public BitTorrent::TorrentHandle
{
    Q_DISABLE_COPY(TorrentHandleImpl)

public:
    TorrentHandleImpl(SessionImpl *session, const libtorrent::torrent_handle &nativeHandle,
                      const AddTorrentData &data);
    ~TorrentHandleImpl();

    bool isValid() const;
    BitTorrent::InfoHash hash() const;
    QString name() const;
    QDateTime creationDate() const;
    QString creator() const;
    QString comment() const;
    bool isPrivate() const;
    qlonglong totalSize() const;
    qlonglong wantedSize() const;
    qlonglong completedSize() const;
    qlonglong uncompletedSize() const;
    qlonglong pieceLength() const;
    qlonglong wastedSize() const;
    QString currentTracker() const;
    QString actualSavePath() const;
    QString savePath() const;
    QString rootPath() const;
    QString savePathParsed() const;
    int filesCount() const;
    int piecesCount() const;
    qreal progress() const;
    QString label() const;
    QDateTime addedTime() const;
    qreal ratioLimit() const;

    QString firstFileSavePath() const;
    QString filePath(int index) const;
    QString fileName(int index) const;
    QString origFilePath(int index) const;
    qlonglong fileSize(int index) const;
    QStringList absoluteFilePaths() const;
    QStringList absoluteFilePathsUnneeded() const;
    QPair<int, int> fileExtremityPieces(int index) const;
    QVector<int> filePriorities() const;

    BitTorrent::TorrentInfo info() const;
    bool isPaused() const;
    bool isQueued() const;
    bool isSeed() const;
    bool isForced() const;
    bool isChecking() const;
    bool isSequentialDownload() const;
    bool hasFirstLastPiecePriority() const;
    BitTorrent::TorrentState state() const;
    bool hasMetadata() const;
    bool hasMissingFiles() const;
    bool hasError() const;
    bool hasFilteredPieces() const;
    bool isFilePreviewPossible() const;
    int queuePosition() const;
    QStringList trackers() const;
    QList<BitTorrent::TrackerEntry> trackerEntries() const;
    QHash<QString, BitTorrent::TrackerInfo> trackerInfos() const;
    QStringList urlSeeds() const;
    QString error() const;
    qlonglong totalDownload() const;
    qlonglong totalUpload() const;
    int activeTime() const;
    int seedingTime() const;
    qulonglong eta() const;
    QVector<qreal> filesProgress() const;
    int seedsCount() const;
    int peersCount() const;
    int leechsCount() const;
    int completeCount() const;
    int incompleteCount() const;
    QDateTime lastSeenComplete() const;
    QDateTime completedTime() const;
    int timeSinceUpload() const;
    int timeSinceDownload() const;
    int downloadLimit() const;
    int uploadLimit() const;
    bool superSeeding() const;
    QList<BitTorrent::PeerInfo> peers() const;
    QBitArray pieces() const;
    QBitArray downloadingPieces() const;
    QVector<int> pieceAvailability() const;
    qreal distributedCopies() const;
    qreal maxRatio(bool *usesGlobalRatio = 0) const;
    qreal realRatio() const;
    int uploadPayloadRate() const;
    int downloadPayloadRate() const;
    int totalPayloadUpload() const;
    int totalPayloadDownload() const;
    int connectionsCount() const;
    int connectionsLimit() const;
    qlonglong nextAnnounce() const;

    void setName(const QString &name);
    void setLabel(const QString &label);
    void setSequentialDownload(bool b);
    void toggleSequentialDownload();
    void toggleFirstLastPiecePriority();
    void setFirstLastPiecePriority(bool b);
    void pause();
    void resume(bool forced = false);
    void move(QString path);
    void forceReannounce(int secs = 0, int index = -1);
    void forceDHTAnnounce();
    void forceRecheck();
    void setTrackerLogin(const QString &username, const QString &password);
    void renameFile(int index, const QString &name);
    bool saveTorrentFile(const QString &path);
    void prioritizeFiles(const QVector<int> &files);
    void setFilePriority(int index, int priority);
    void setRatioLimit(qreal limit);
    void setUploadLimit(int limit);
    void setDownloadLimit(int limit);
    void setSuperSeeding(bool enable);
    void flushCache();
    void replaceTrackers(QList<BitTorrent::TrackerEntry> trackers);
    bool addUrlSeed(const QString &urlSeed);
    void removeUrlSeed(const QString &urlSeed);
    bool connectPeer(const BitTorrent::PeerAddress &peerAddress);
    bool addTracker(const QString &tracker);

    QString toMagnetUri() const;

    // Session interface
    void updateStatus();

    inline void resolveCountries(bool b) { m_nativeHandle.resolve_countries(b); }
    inline void setMaxConnections(int i) { m_nativeHandle.set_max_connections(i); }
    inline void setMaxUploads(int i) { m_nativeHandle.set_max_uploads(i); }
    inline void queuePositionTop() { m_nativeHandle.queue_position_top(); }
    inline void queuePositionBottom() { m_nativeHandle.queue_position_bottom(); }
    inline void queuePositionUp() { m_nativeHandle.queue_position_up(); }
    inline void queuePositionDown() { m_nativeHandle.queue_position_down(); }

    bool needSaveResumeData() const;
    void saveResumeData();

    void handleStateUpdate(const libtorrent::torrent_status &nativeStatus);
    void handleStorageMoved(const QString &newPath);
    void handleStorageMovedFailed();
    void handleTrackerReply(const QString &trackerUrl, int peersCount);
    void handleTrackerWarning(const QString &trackerUrl, const QString &message);
    void handleTrackerError(const QString &trackerUrl, const QString &message);
    void handleTorrentChecked();
    void handleTorrentFinished();
    void handleTorrentPaused();
    void handleSaveResumeData(boost::shared_ptr<libtorrent::entry> data);
    void handleFastResumeRejected(bool hasMissingFiles);
    void handleFileRenamed(int index, const QString &newName);
    void handleFileCompleted(int index);
    void handleMetadataReceived();
    void handleStats(int downloadPayload, int uploadPayload, int interval);
    void handleDefaultSavePathChanged();
    void handleTempPathChanged();
    void handleAppendExtensionToggled();

    libtorrent::torrent_handle nativeHandle() const;
    const libtorrent::torrent_info *torrentInfo() const;

private:
    void initialize();
    void updateSeedStatus();
    void setHasMissingFiles(bool b);

    bool isMoveInProgress() const;
    bool useTempPath() const;
    qlonglong fileOffset(int index) const;

    void adjustSavePath();
    void adjustActualSavePath();
    void moveStorage(const QString &newPath);
    void appendExtensionsToIncompleteFiles();
    void removeExtensionsFromIncompleteFiles();

    SessionImpl *const m_session;
    libtorrent::torrent_handle m_nativeHandle;
    libtorrent::torrent_status m_nativeStatus;
    SpeedMonitor m_speedMonitor;
#if LIBTORRENT_VERSION_NUM < 10000
    QString m_nativeName;
    QString m_nativeSavePath;
#endif

    BitTorrent::InfoHash m_hash;

    QString m_newPath;
    // m_queuedPath is where files should be moved to,
    // when current moving is completed
    QString m_queuedPath;

    // Persistent data
    QString m_name;
    QDateTime m_addedTime;
    QString m_savePath;
    QString m_label;
    bool m_hasSeedStatus;
    qreal m_ratioLimit;
    bool m_tempPathDisabled;
    bool m_hasMissingFiles;

    bool m_useDefaultSavePath;
    bool m_pauseAfterRecheck;
    QHash<QString, BitTorrent::TrackerInfo> m_trackerInfos;
};

#endif // TORRENTHANDLEIMPL_H
