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

#ifndef BITTORRENT_SESSION_H
#define BITTORRENT_SESSION_H

#include <QHash>
#include <QSet>
#include <QStringList>
#include <QVector>

#include "torrentinfo.h"
#include "core/tristatebool.h"
#include "core/types.h"

class ScanFoldersModel;

namespace BitTorrent
{

class InfoHash;
class CacheStatus;
class SessionStatus;
class TorrentHandle;
class Tracker;
class MagnetUri;

struct AddTorrentParams
{
    QString name;
    QString label;
    QString savePath;
    bool disableTempPath;
    bool sequential;
    TriStateBool addPaused;
    QVector<int> filePriorities; // used for preloaded torrents or if TorrentInfo is set
    bool ignoreShareRatio;
    bool skipChecking;

    AddTorrentParams()
        : disableTempPath(false)
        , sequential(false)
        , ignoreShareRatio(false)
        , skipChecking(false)
    {
    }
};

class Session : public QObject
{
    Q_OBJECT

public:
    static bool initInstance(QObject *parent = 0);
    static void freeInstance();
    static Session *instance();

    explicit Session(QObject *parent = 0);

    virtual bool isDHTEnabled() const = 0;
    virtual bool isLSDEnabled() const = 0;
    virtual bool isPexEnabled() const = 0;
    virtual bool isQueueingEnabled() const = 0;
    virtual bool isTempPathEnabled() const = 0;
    virtual bool isAppendExtensionEnabled() const = 0;
    virtual bool useAppendLabelToSavePath() const = 0;
    virtual QString defaultSavePath() const = 0;
    virtual QString tempPath() const = 0;
    virtual qreal globalMaxRatio() const = 0;

    virtual TorrentHandle *findTorrent(const InfoHash &hash) const = 0;
    virtual QHash<InfoHash, TorrentHandle *> torrents() const = 0;
    virtual bool hasActiveTorrents() const = 0;
    virtual bool hasDownloadingTorrents() const = 0;
    virtual SessionStatus status() const = 0;
    virtual CacheStatus cacheStatus() const = 0;
    virtual quint64 getAlltimeDL() const = 0;
    virtual quint64 getAlltimeUL() const = 0;
    virtual int downloadRateLimit() const = 0;
    virtual int uploadRateLimit() const = 0;
    virtual bool isListening() const = 0;

    virtual void changeSpeedLimitMode(bool alternative) = 0;
    virtual void setQueueingEnabled(bool enable) = 0;
    virtual void setDownloadRateLimit(int rate) = 0;
    virtual void setUploadRateLimit(int rate) = 0;
    virtual void setGlobalMaxRatio(qreal ratio) = 0;
    virtual void enableIPFilter(const QString &filterPath, bool force = false) = 0;
    virtual void disableIPFilter() = 0;
    virtual void banIP(const QString &ip) = 0;

    virtual bool isKnownTorrent(const InfoHash &hash) const = 0;
    virtual bool addTorrent(QString source, const AddTorrentParams &params = AddTorrentParams()) = 0;
    virtual bool addTorrent(const TorrentInfo &torrentInfo, const AddTorrentParams &params = AddTorrentParams()) = 0;
    virtual bool deleteTorrent(const QString &hash, bool deleteLocalFiles = false) = 0;
    virtual bool loadMetadata(const QString &magnetUri) = 0;
    virtual bool cancelLoadMetadata(const InfoHash &hash) = 0;

    virtual void recursiveTorrentDownload(const QString &hash) = 0;
    virtual void increaseTorrentsPriority(const QStringList &hashes) = 0;
    virtual void decreaseTorrentsPriority(const QStringList &hashes) = 0;
    virtual void topTorrentsPriority(const QStringList &hashes) = 0;
    virtual void bottomTorrentsPriority(const QStringList &hashes) = 0;

signals:
    void torrentsUpdated();
    void torrentAdded(BitTorrent::TorrentHandle *const torrent);
    void torrentAboutToBeRemoved(BitTorrent::TorrentHandle *const torrent);
    void torrentStatusUpdated(BitTorrent::TorrentHandle *const torrent);
    void torrentPaused(BitTorrent::TorrentHandle *const torrent);
    void torrentResumed(BitTorrent::TorrentHandle *const torrent);
    void torrentFinished(BitTorrent::TorrentHandle *const torrent);
    void torrentFinishedChecking(BitTorrent::TorrentHandle *const torrent);
    void torrentSavePathChanged(BitTorrent::TorrentHandle *const torrent);
    void metadataLoaded(const BitTorrent::TorrentInfo &info);
    void metadataLoaded(BitTorrent::TorrentHandle *const torrent);
    void fullDiskError(BitTorrent::TorrentHandle *const torrent, const QString &msg);
    void trackerSuccess(BitTorrent::TorrentHandle *const torrent, const QString &tracker);
    void trackerWarning(BitTorrent::TorrentHandle *const torrent, const QString &tracker);
    void trackerError(BitTorrent::TorrentHandle *const torrent, const QString &tracker);
    void trackerAuthenticationRequired(BitTorrent::TorrentHandle *const torrent);
    void recursiveTorrentDownloadPossible(BitTorrent::TorrentHandle *const torrent);
    void speedLimitModeChanged(bool alternative);
    void ipFilterParsed(bool error, int ruleCount);
    void trackerAdded(BitTorrent::TorrentHandle *const torrent, const QString &tracker);
    void trackerlessStateChanged(BitTorrent::TorrentHandle *const torrent, bool trackerless);
    void downloadFromUrlFailed(const QString &url, const QString &reason);
    void downloadFromUrlFinished(const QString &url);

protected:
    ~Session() {}

private:
    static Session *m_instance;
};

}

#endif // BITTORRENT_SESSION_H
