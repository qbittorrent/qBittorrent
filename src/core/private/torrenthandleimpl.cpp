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

#include <QDebug>
#include <QString>
#include <QStringList>
#include <QFile>
#include <QDir>
#include <QByteArray>
#include <QBitArray>

#include <libtorrent/version.hpp>
#include <libtorrent/entry.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/address.hpp>

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

#include "core/fs_utils.h"
#include "core/misc.h"
#include "core/bittorrent/peerinfo.h"
#include "core/bittorrent/trackerentry.h"
#include "core/bittorrent/trackerinfo.h"
#include "core/bittorrent/torrenthandle.h"
#include "sessionimpl.h"
#include "torrenthandleimpl.h"

static const char QB_EXT[] = ".!qB";

namespace libt = libtorrent;
using namespace BitTorrent;

AddTorrentData::AddTorrentData(const AddTorrentParams &in)
    : resumed(false)
    , name(in.name)
    , label(in.label)
    , savePath(in.savePath)
    , disableTempPath(in.disableTempPath)
    , sequential(in.sequential)
    , hasSeedStatus(in.skipChecking)
    , addPaused(in.addPaused)
    , filePriorities(in.filePriorities)
    , ratioLimit(TorrentHandle::USE_GLOBAL_RATIO)
{
}

#define CALL(func, ...) m_nativeHandle.func(__VA_ARGS__)

#define SAFE_CALL(func, ...) \
    try { \
        m_nativeHandle.func(__VA_ARGS__); \
    } \
    catch (std::exception &exc) { \
        qDebug("torrent_handle::"#func"() throws exception: %s", exc.what()); \
    }

#define SAFE_CALL_BOOL(func, ...) \
    try { \
        m_nativeHandle.func(__VA_ARGS__); \
        return true; \
    } \
    catch (std::exception &exc) { \
        qDebug("torrent_handle::"#func"() throws exception: %s", exc.what()); \
        return false; \
    }

#define SAFE_RETURN(type, func, val) \
    type result = val; \
    try { \
        result = m_nativeHandle.func(); \
    } \
    catch (std::exception &exc) { \
        qDebug("torrent_handle::"#func"() throws exception: %s", exc.what()); \
    } \
    return result;

#define SAFE_GET(var, func, ...) \
    try { \
        var = m_nativeHandle.func(__VA_ARGS__); \
    } \
    catch (std::exception &exc) { \
        qDebug("torrent_handle::"#func"() throws exception: %s", exc.what()); \
    }

TorrentHandleImpl::TorrentHandleImpl(SessionImpl *session, const libtorrent::torrent_handle &nativeHandle,
                                     const AddTorrentData &data)
    : TorrentHandle(session)
    , m_session(session)
    , m_nativeHandle(nativeHandle)
    , m_name(data.name)
    , m_addedTime(data.resumed ? data.addedTime : QDateTime::currentDateTime())
    , m_savePath(fsutils::toNativePath(data.savePath))
    , m_label(data.label)
    , m_hasSeedStatus(data.resumed ? data.hasSeedStatus : false)
    , m_ratioLimit(data.resumed ? data.ratioLimit : (data.ignoreShareRatio ? NO_RATIO_LIMIT : USE_GLOBAL_RATIO))
    , m_tempPathDisabled(data.disableTempPath)
    , m_pauseAfterRecheck(false)
{
    initialize();
    setSequentialDownload(data.sequential);

    if (!data.resumed) {
        if (hasMetadata()) {
            prioritizeFiles(data.filePriorities);
            setFirstLastPiecePriority(data.sequential);
            appendExtensionsToIncompleteFiles();
        }

        if (needSaveResumeData())
            m_session->torrentNotifyNeedSaveResumeData(this);
    }
}

TorrentHandleImpl::~TorrentHandleImpl()
{
}

bool TorrentHandleImpl::isValid() const
{
    return CALL(is_valid);
}

InfoHash TorrentHandleImpl::hash() const
{
    return m_hash;
}

QString TorrentHandleImpl::name() const
{
    QString name = m_name;
    if (name.isEmpty()) {
#if LIBTORRENT_VERSION_NUM < 10000
        name = m_nativeName;
#else
        name = misc::toQStringU(m_nativeStatus.name);
#endif
    }

    if (name.isEmpty())
        name = m_hash;

    return name;
}

QDateTime TorrentHandleImpl::creationDate() const
{
    if (!hasMetadata()) return QDateTime();

    boost::optional<time_t> t = torrentInfo()->creation_date();
    return t ? QDateTime::fromTime_t(*t) : QDateTime();
}

QString TorrentHandleImpl::creator() const
{
    if (!hasMetadata()) return QString();
    return misc::toQStringU(torrentInfo()->creator());
}

QString TorrentHandleImpl::comment() const
{
    if (!hasMetadata()) return QString();
    return misc::toQStringU(torrentInfo()->comment());
}

bool TorrentHandleImpl::isPrivate() const
{
    if (!hasMetadata()) return false;
    return torrentInfo()->priv();
}

qlonglong TorrentHandleImpl::totalSize() const
{
    if (!hasMetadata()) return -1;
    return torrentInfo()->total_size();
}

// get the size of the torrent without the filtered files
qlonglong TorrentHandleImpl::wantedSize() const
{
    return m_nativeStatus.total_wanted;
}

qlonglong TorrentHandleImpl::completedSize() const
{
    return m_nativeStatus.total_wanted_done;
}

qlonglong TorrentHandleImpl::uncompletedSize() const
{
    return (m_nativeStatus.total_wanted - m_nativeStatus.total_wanted_done);
}

qlonglong TorrentHandleImpl::pieceLength() const
{
    if (!hasMetadata()) return -1;
    return torrentInfo()->piece_length();
}

qlonglong TorrentHandleImpl::wastedSize() const
{
    return (m_nativeStatus.total_failed_bytes + m_nativeStatus.total_redundant_bytes);
}

QString TorrentHandleImpl::currentTracker() const
{
    return misc::toQString(m_nativeStatus.current_tracker);
}

QString TorrentHandleImpl::savePath() const
{
    return fsutils::fromNativePath(m_savePath);
}

QString TorrentHandleImpl::rootPath() const
{
    if (filesCount() > 1) {
        QString first_filepath = filePath(0);
        const int slashIndex = first_filepath.indexOf("/");
        if (slashIndex >= 0)
            return QDir(actualSavePath()).absoluteFilePath(first_filepath.left(slashIndex));
    }

    return actualSavePath();
}

QString TorrentHandleImpl::actualSavePath() const
{
#if LIBTORRENT_VERSION_NUM < 10000
    return m_nativeSavePath;
#else
    return fsutils::fromNativePath(misc::toQStringU(m_nativeStatus.save_path));
#endif
}

QList<TrackerEntry> TorrentHandleImpl::trackerEntries() const
{
    QList<TrackerEntry> entries;
    std::vector<libt::announce_entry> announces;
    SAFE_GET(announces, trackers);

    foreach (const libt::announce_entry &tracker, announces)
        entries << tracker;

    return entries;
}

QHash<QString, TrackerInfo> TorrentHandleImpl::trackerInfos() const
{
    return m_trackerInfos;
}

QStringList TorrentHandleImpl::trackers() const
{
    QStringList trackers;
    std::vector<libt::announce_entry> announces;
    SAFE_GET(announces, trackers);

    foreach (const libt::announce_entry &tracker, announces)
        trackers.append(misc::toQStringU(tracker.url));

    return trackers;
}

bool TorrentHandleImpl::addTracker(const QString &tracker)
{
    std::vector<libt::announce_entry> announces;
    SAFE_GET(announces, trackers);

    // Check if torrent has this tracker
    foreach (const libt::announce_entry &existingTracker, announces) {
        if (QUrl(tracker) == QUrl(existingTracker.url.c_str()))
            return false;
    }

    try {
        CALL(add_tracker, libt::announce_entry(tracker.toStdString()));
        m_session->torrentNotifyTrackerAdded(this, tracker);
        return true;
    }
    catch (std::exception &exc) {
        qDebug("torrent_handle::add_tracker() throws exception: %s", exc.what());
        return false;
    }

}

QStringList TorrentHandleImpl::urlSeeds() const
{
    QStringList urlSeeds;
    std::set<std::string> seeds;
    SAFE_GET(seeds, url_seeds);

    foreach (const std::string &urlSeed, seeds)
        urlSeeds.append(QString::fromUtf8(urlSeed.c_str()));

    return urlSeeds;
}

bool TorrentHandleImpl::addUrlSeed(const QString &urlSeed)
{
    std::set<std::string> seeds;
    SAFE_GET(seeds, url_seeds);

    // Check if torrent has this url seed
    foreach (const std::string &existingUrlSeed, seeds) {
        if (QUrl(urlSeed) == QUrl(existingUrlSeed.c_str()))
            return false;
    }

    SAFE_CALL_BOOL(add_url_seed, urlSeed.toStdString());
}

void TorrentHandleImpl::removeUrlSeed(const QString &urlSeed)
{
    SAFE_CALL(remove_url_seed, urlSeed.toStdString());
}

bool TorrentHandleImpl::connectPeer(const PeerAddress &peerAddress)
{
    libt::error_code ec;
    libt::address addr = libt::address::from_string(peerAddress.ip.toString().toStdString(), ec);
    if (ec) return false;

    libt::asio::ip::tcp::endpoint ep(addr, peerAddress.port);
    SAFE_CALL_BOOL(connect_peer, ep);
}

bool TorrentHandleImpl::needSaveResumeData() const
{
    SAFE_RETURN(bool, need_save_resume_data, false);
}

void TorrentHandleImpl::saveResumeData()
{
    SAFE_CALL(save_resume_data);
}

QString TorrentHandleImpl::savePathParsed() const
{
    QString p;
    if (hasMetadata() && (filesCount() == 1))
        p = firstFileSavePath();
    else
        p = savePath();

    return p;
}

int TorrentHandleImpl::filesCount() const
{
    if (!hasMetadata()) return -1;
    return torrentInfo()->num_files();
}

int TorrentHandleImpl::piecesCount() const
{
    if (!hasMetadata()) return -1;
    return torrentInfo()->num_pieces();
}

qreal TorrentHandleImpl::progress() const
{
    if (!m_nativeStatus.total_wanted)
        return 0.;

    if (m_nativeStatus.total_wanted_done == m_nativeStatus.total_wanted)
        return 1.;

    float progress = (float) m_nativeStatus.total_wanted_done / (float) m_nativeStatus.total_wanted;
    Q_ASSERT((progress >= 0.f) && (progress <= 1.f));
    return progress;
}

QString TorrentHandleImpl::label() const
{
    return m_label;
}

QDateTime TorrentHandleImpl::addedTime() const
{
    return m_addedTime;
}

qreal TorrentHandleImpl::ratioLimit() const
{
    return m_ratioLimit;
}


QString TorrentHandleImpl::firstFileSavePath() const
{
    Q_ASSERT(hasMetadata());

    QString fSavePath = savePath();
    if (!fSavePath.endsWith("/"))
        fSavePath += "/";
    fSavePath += filePath(0);
    // Remove .!qB extension
    if (fSavePath.endsWith(".!qB", Qt::CaseInsensitive))
        fSavePath.chop(4);

    return fSavePath;
}

QString TorrentHandleImpl::filePath(int index) const
{
    if (!hasMetadata()) return QString();
    return fsutils::fromNativePath(misc::toQStringU(torrentInfo()->files().file_path(index)));
}

QString TorrentHandleImpl::fileName(int index) const
{
    if (!hasMetadata()) return  QString();
    return fsutils::fileName(filePath(index));
}

QString TorrentHandleImpl::origFilePath(int index) const
{
    if (!hasMetadata()) return  QString();
    return fsutils::fromNativePath(misc::toQStringU(torrentInfo()->orig_files().file_path(index)));
}

qlonglong TorrentHandleImpl::fileSize(int index) const
{
    if (!hasMetadata()) return  -1;
    return torrentInfo()->files().file_size(index);
}

qlonglong TorrentHandleImpl::fileOffset(int index) const
{
    if (!hasMetadata()) return  -1;
    return torrentInfo()->file_at(index).offset;
}

// Return a list of absolute paths corresponding
// to all files in a torrent
QStringList TorrentHandleImpl::absoluteFilePaths() const
{
    if (!hasMetadata()) return  QStringList();

    QDir saveDir(actualSavePath());
    QStringList res;
    for (int i = 0; i < filesCount(); ++i)
        res << fsutils::expandPathAbs(saveDir.absoluteFilePath(filePath(i)));
    return res;
}

QStringList TorrentHandleImpl::absoluteFilePathsUnneeded() const
{
    if (!hasMetadata()) return  QStringList();

    QDir saveDir(actualSavePath());
    QStringList res;
    std::vector<int> fp;
    SAFE_GET(fp, file_priorities);

    int count = static_cast<int>(fp.size());
    for (int i = 0; i < count; ++i) {
        if (fp[i] == 0) {
            const QString path = fsutils::expandPathAbs(saveDir.absoluteFilePath(filePath(i)));
            if (path.contains(".unwanted"))
                res << path;
        }
    }

    return res;
}

QPair<int, int> TorrentHandleImpl::fileExtremityPieces(int index) const
{
    if (!hasMetadata()) return qMakePair(-1, -1);

    const int numPieces = piecesCount();
    const qlonglong pieceSize = pieceLength();

    // Determine the first and last piece of the file
    int firstPiece = floor((fileOffset(index) + 1) / (float) pieceSize);
    Q_ASSERT((firstPiece >= 0) && (firstPiece < numPieces));

    int numPiecesInFile = ceil(fileSize(index) / (float) pieceSize);
    int lastPiece = firstPiece + numPiecesInFile - 1;
    Q_ASSERT((lastPiece >= 0) && (lastPiece < numPieces));

    Q_UNUSED(numPieces)
    return qMakePair(firstPiece, lastPiece);
}

QVector<int> TorrentHandleImpl::filePriorities() const
{
    QVector<int> result;
    std::vector<int> fp;
    SAFE_GET(fp, file_priorities);

    foreach (int prio, fp)
        result.append(prio);

    return result;
}

TorrentInfo TorrentHandleImpl::info() const
{
    return TorrentInfo(torrentInfo());
}

bool TorrentHandleImpl::isPaused() const
{
    return (m_nativeStatus.paused && !m_nativeStatus.auto_managed);
}

bool TorrentHandleImpl::isQueued() const
{
    return (m_nativeStatus.paused && m_nativeStatus.auto_managed);
}

bool TorrentHandleImpl::isChecking() const
{
    return ((m_nativeStatus.state == libt::torrent_status::queued_for_checking)
            || (m_nativeStatus.state == libt::torrent_status::checking_files)
            || (m_nativeStatus.state == libt::torrent_status::checking_resume_data));
}

bool TorrentHandleImpl::isSeed() const
{
    // Affected by bug http://code.rasterbar.com/libtorrent/ticket/402
    //SAFE_RETURN(bool, is_seed, false);
    // May suffer from approximation problems
    //return (progress() == 1.);
    // This looks safe
    return ((m_nativeStatus.state == libt::torrent_status::finished)
            || (m_nativeStatus.state == libt::torrent_status::seeding));
}

bool TorrentHandleImpl::isForced() const
{
    return (!m_nativeStatus.paused && !m_nativeStatus.auto_managed);
}

bool TorrentHandleImpl::isSequentialDownload() const
{
    return m_nativeStatus.sequential_download;
}

bool TorrentHandleImpl::hasFirstLastPiecePriority() const
{
    if (!hasMetadata()) return false;

    // Get int first media file
    std::vector<int> fp;
    SAFE_GET(fp, file_priorities);

    QPair<int, int> extremities;
    bool found = false;
    int count = static_cast<int>(fp.size());
    for (int i = 0; i < count; ++i) {
        const QString ext = fsutils::fileExtension(filePath(i));
        if (misc::isPreviewable(ext) && (fp[i] > 0)) {
            extremities = fileExtremityPieces(i);
            found = true;
        }
    }

    if (!found) return false; // No media file

    int first = 0;
    int last = 0;
    SAFE_GET(first, piece_priority, extremities.first);
    SAFE_GET(last, piece_priority, extremities.second);

    return ((first == 7) && (last == 7));
}

TorrentState TorrentHandleImpl::state() const
{
    TorrentState state = TorrentState::Unknown;

    if (isPaused()) {
        if (hasError() || hasMissingFiles())
            state = TorrentState::Error;
        else
            state = isSeed() ? TorrentState::PausedUploading : TorrentState::PausedDownloading;
    }
    else {
        if (m_session->isQueueingEnabled() && isQueued() && !isChecking()) {
            state = isSeed() ? TorrentState::QueuedUploading : TorrentState::QueuedDownloading;
        }
        else {
            switch (m_nativeStatus.state) {
            case libt::torrent_status::finished:
            case libt::torrent_status::seeding:
                if (isForced())
                    state = TorrentState::ForcedUploading;
                else
                    state = m_nativeStatus.upload_payload_rate > 0 ? TorrentState::Uploading : TorrentState::StalledUploading;
                break;
            case libt::torrent_status::allocating:
                state = TorrentState::Allocating;
                break;
            case libt::torrent_status::checking_files:
            case libt::torrent_status::queued_for_checking:
            case libt::torrent_status::checking_resume_data:
                state = isSeed() ? TorrentState::CheckingUploading : TorrentState::CheckingDownloading;
                break;
            case libt::torrent_status::downloading_metadata:
                state = TorrentState::DownloadingMetadata;
                break;
            case libt::torrent_status::downloading:
                if (isForced())
                    state = TorrentState::ForcedDownloading;
                else
                    state = m_nativeStatus.download_payload_rate > 0 ? TorrentState::Downloading : TorrentState::StalledDownloading;
                break;
            default:
                qWarning("Unrecognized torrent status, should not happen!!! status was %d", m_nativeStatus.state);
            }
        }
    }

    return state;
}

bool TorrentHandleImpl::hasMetadata() const
{
    return m_nativeStatus.has_metadata;
}

bool TorrentHandleImpl::hasMissingFiles() const
{
    return m_hasMissingFiles;
}

bool TorrentHandleImpl::hasError() const
{
    return (m_nativeStatus.paused && !m_nativeStatus.error.empty());
}

bool TorrentHandleImpl::hasFilteredPieces() const
{
    std::vector<int> pp;
    SAFE_GET(pp, piece_priorities);

    foreach (const int priority, pp)
        if (priority == 0) return true;

    return false;
}

int TorrentHandleImpl::queuePosition() const
{
    if (m_nativeStatus.queue_position < 0) return 0;

    return m_nativeStatus.queue_position - m_session->extraLimit() + 1;
}

QString TorrentHandleImpl::error() const
{
    return misc::toQString(m_nativeStatus.error);
}

qlonglong TorrentHandleImpl::totalDownload() const
{
    return m_nativeStatus.all_time_download;
}

qlonglong TorrentHandleImpl::totalUpload() const
{
    return m_nativeStatus.all_time_upload;
}

int TorrentHandleImpl::activeTime() const
{
    return m_nativeStatus.active_time;
}

int TorrentHandleImpl::seedingTime() const
{
    return m_nativeStatus.seeding_time;
}

qulonglong TorrentHandleImpl::eta() const
{
    if (isPaused()) return MAX_ETA;

    const SpeedSampleAvg speed_average = m_speedMonitor.average();

    if (isSeed()) {
        if (speed_average.upload == 0) return MAX_ETA;

        qreal max_ratio = maxRatio();
        if (max_ratio < 0) return MAX_ETA;

        qlonglong realDL = totalDownload();
        if (realDL <= 0)
            realDL = wantedSize();

        return ((realDL * max_ratio) - totalUpload()) / speed_average.upload;
    }

    if (!speed_average.download) return MAX_ETA;

    return (wantedSize() - completedSize()) / speed_average.download;
}

QVector<qreal> TorrentHandleImpl::filesProgress() const
{
    std::vector<libt::size_type> fp;
    QVector<qreal> result;
    SAFE_CALL(file_progress, fp, libt::torrent_handle::piece_granularity);

    int count = static_cast<int>(fp.size());
    for (int i = 0; i < count; ++i) {
        qlonglong size = fileSize(i);
        if ((size <= 0) || (fp[i] == size))
            result << 1;
        else
            result << (fp[i] / static_cast<qreal>(size));
    }

    return result;
}

int TorrentHandleImpl::seedsCount() const
{
    return m_nativeStatus.num_seeds;
}

int TorrentHandleImpl::peersCount() const
{
    return m_nativeStatus.num_peers;
}

int TorrentHandleImpl::leechsCount() const
{
    return (m_nativeStatus.num_peers - m_nativeStatus.num_seeds);
}

int TorrentHandleImpl::completeCount() const
{
    return m_nativeStatus.num_complete;
}

int TorrentHandleImpl::incompleteCount() const
{
    return m_nativeStatus.num_incomplete;
}

QDateTime TorrentHandleImpl::lastSeenComplete() const
{
    return QDateTime::fromTime_t(m_nativeStatus.last_seen_complete);
}

QDateTime TorrentHandleImpl::completedTime() const
{
    return QDateTime::fromTime_t(m_nativeStatus.completed_time);
}

int TorrentHandleImpl::timeSinceUpload() const
{
    return m_nativeStatus.time_since_upload;
}

int TorrentHandleImpl::timeSinceDownload() const
{
    return m_nativeStatus.time_since_download;
}

int TorrentHandleImpl::downloadLimit() const
{
    SAFE_RETURN(int, download_limit, -1)
}

int TorrentHandleImpl::uploadLimit() const
{
    SAFE_RETURN(int, upload_limit, -1)
}

bool TorrentHandleImpl::superSeeding() const
{
    return m_nativeStatus.super_seeding;
}

QList<PeerInfo> TorrentHandleImpl::peers() const
{
    QList<PeerInfo> peers;
    std::vector<libt::peer_info> nativePeers;

    SAFE_CALL(get_peer_info, nativePeers);

    foreach (const libt::peer_info &peer, nativePeers)
        peers << peer;

    return peers;
}

QBitArray TorrentHandleImpl::pieces() const
{
    QBitArray result(m_nativeStatus.pieces.size());

    for (int i = 0; i < m_nativeStatus.pieces.size(); ++i)
        result.setBit(i, m_nativeStatus.pieces.get_bit(i));

    return result;
}

QBitArray TorrentHandleImpl::downloadingPieces() const
{
    QBitArray result(piecesCount());

    std::vector<libt::partial_piece_info> queue;
    SAFE_CALL(get_download_queue, queue);

    std::vector<libt::partial_piece_info>::const_iterator it = queue.begin();
    std::vector<libt::partial_piece_info>::const_iterator itend = queue.end();
    for (; it != itend; ++it)
        result.setBit(it->piece_index);

    return result;
}

QVector<int> TorrentHandleImpl::pieceAvailability() const
{
    std::vector<int> avail;
    SAFE_CALL(piece_availability, avail);

    return QVector<int>::fromStdVector(avail);
}

qreal TorrentHandleImpl::distributedCopies() const
{
    return m_nativeStatus.distributed_copies;
}

qreal TorrentHandleImpl::maxRatio(bool *usesGlobalRatio) const
{
    qreal ratioLimit = m_ratioLimit;

    if (ratioLimit == USE_GLOBAL_RATIO) {
        ratioLimit = m_session->globalMaxRatio();
        if (usesGlobalRatio)
            *usesGlobalRatio = true;
    }
    else {
        if (usesGlobalRatio)
            *usesGlobalRatio = false;
    }

    return ratioLimit;
}

qreal TorrentHandleImpl::realRatio() const
{
    libt::size_type all_time_upload = m_nativeStatus.all_time_upload;
    libt::size_type all_time_download = m_nativeStatus.all_time_download;
    libt::size_type total_done = m_nativeStatus.total_done;

    if (all_time_download < total_done) {
        // We have more data on disk than we downloaded
        // either because the user imported the file
        // or because of crash the download histroy was lost.
        // Otherwise will get weird ratios
        // eg when downloaded 1KB and uploaded 700MB of a
        // 700MB torrent.
        all_time_download = total_done;
    }

    if (all_time_download == 0) {
        if (all_time_upload == 0) return 0.0;
        else return MAX_RATIO + 1;
    }

    qreal ratio = all_time_upload / static_cast<qreal>(all_time_download);
    Q_ASSERT(ratio >= 0.);
    if (ratio > MAX_RATIO)
        ratio = MAX_RATIO;

    return ratio;
}

int TorrentHandleImpl::uploadPayloadRate() const
{
    return m_nativeStatus.upload_payload_rate;
}

int TorrentHandleImpl::downloadPayloadRate() const
{
    return m_nativeStatus.download_payload_rate;
}

int TorrentHandleImpl::totalPayloadUpload() const
{
    return m_nativeStatus.total_payload_upload;
}

int TorrentHandleImpl::totalPayloadDownload() const
{
    return m_nativeStatus.total_payload_download;
}

int TorrentHandleImpl::connectionsCount() const
{
    return m_nativeStatus.num_connections;
}

int TorrentHandleImpl::connectionsLimit() const
{
    return m_nativeStatus.connections_limit;
}

qlonglong TorrentHandleImpl::nextAnnounce() const
{
    return m_nativeStatus.next_announce.total_seconds();
}

void TorrentHandleImpl::setName(const QString &name)
{
    if (m_name != name) {
        m_name = name;
        m_session->torrentNotifyNeedSaveResumeData(this);
    }
}

void TorrentHandleImpl::setLabel(const QString &label)
{
    if (m_label != label) {
        m_label = label;
        m_session->torrentNotifyNeedSaveResumeData(this);
        adjustSavePath();
    }
}

void TorrentHandleImpl::setSequentialDownload(bool b)
{
    SAFE_CALL(set_sequential_download, b);
}

void TorrentHandleImpl::move(QString path)
{
    // now we use non-default save path
    // even if new path same as default.
    m_useDefaultSavePath = false;

    path = fsutils::toNativePath(path);
    if (path == savePath()) return;

    if (!useTempPath()) {
        moveStorage(path);
    }
    else {
        m_savePath = path;
        m_session->torrentNotifyNeedSaveResumeData(this);
        m_session->torrentNotifySavePathChanged(this);
    }
}

void TorrentHandleImpl::forceReannounce(int secs, int index)
{
    SAFE_CALL(force_reannounce, secs, index);
}

void TorrentHandleImpl::forceDHTAnnounce()
{
    SAFE_CALL(force_dht_announce);
}

void TorrentHandleImpl::forceRecheck()
{
    if (!hasMetadata()) return;

    if (isPaused()) {
        m_pauseAfterRecheck = true;
        resume();
    }

    SAFE_CALL(force_recheck);
}

void TorrentHandleImpl::toggleSequentialDownload()
{
    if (hasMetadata()) {
        bool was_sequential = isSequentialDownload();
        SAFE_CALL(set_sequential_download, !was_sequential);
        if (!was_sequential)
            setFirstLastPiecePriority(true);
    }
}

void TorrentHandleImpl::toggleFirstLastPiecePriority()
{
    if (hasMetadata())
        setFirstLastPiecePriority(!hasFirstLastPiecePriority());
}

void TorrentHandleImpl::setFirstLastPiecePriority(bool b)
{
    std::vector<int> fp;
    SAFE_GET(fp, file_priorities);

    // Download first and last pieces first for all media files in the torrent
    int nbfiles = static_cast<int>(fp.size());
    for (int index = 0; index < nbfiles; ++index) {
        const QString path = filePath(index);
        const QString ext = fsutils::fileExtension(path);
        if (misc::isPreviewable(ext) && (fp[index] > 0)) {
            qDebug() << "File" << path << "is previewable, toggle downloading of first/last pieces first";
            // Determine the priority to set
            int prio = b ? 7 : fp[index];

            QPair<int, int> extremities = fileExtremityPieces(index);
            SAFE_CALL(piece_priority, extremities.first, prio);
            SAFE_CALL(piece_priority, extremities.second, prio);
        }
    }
}

void TorrentHandleImpl::pause()
{
    if (isPaused()) return;

    try {
        CALL(auto_managed, false);
        CALL(pause);
    }
    catch (std::exception &exc) {
        qDebug("torrent_handle method inside TorrentHandleImpl::pause() throws exception: %s", exc.what());
    }
}

void TorrentHandleImpl::resume(bool forced)
{
    try {
        if (hasError())
            CALL(clear_error);
        CALL(auto_managed, !forced);
        CALL(resume);
    }
    catch (std::exception &exc) {
        qDebug("torrent_handle method inside TorrentHandleImpl::resume() throws exception: %s", exc.what());
    }
}

void TorrentHandleImpl::moveStorage(const QString &newPath)
{
    if (isMoveInProgress()) {
        qDebug("enqueue move storage to %s", qPrintable(newPath));
        m_queuedPath = newPath;
    }
    else {
        QString oldPath = actualSavePath();
        qDebug("move storage: %s to %s", qPrintable(oldPath), qPrintable(newPath));

        if (QDir(oldPath) == QDir(newPath)) return;

        // Create destination directory if necessary
        // or move_storage() will fail...
        QDir().mkpath(newPath);

        try {
            // Actually move the storage
            CALL(move_storage, newPath.toUtf8().constData());
            m_newPath = newPath;
        }
        catch (std::exception &exc) {
            qDebug("torrent_handle::move_storage() throws exception: %s", exc.what());
        }
    }
}

void TorrentHandleImpl::setTrackerLogin(const QString &username, const QString &password)
{
    SAFE_CALL(set_tracker_login, std::string(username.toLocal8Bit().constData()), std::string(password.toLocal8Bit().constData()));
}

void TorrentHandleImpl::renameFile(int index, const QString &name)
{
    qDebug() << Q_FUNC_INFO << index << name;
    SAFE_CALL(rename_file, index, fsutils::toNativePath(name).toStdString());
}

bool TorrentHandleImpl::saveTorrentFile(const QString &path)
{
    const libt::torrent_info *nativeInfo = torrentInfo();
    if (!nativeInfo) return false;

    libt::entry meta = libt::bdecode(nativeInfo->metadata().get(),
                                     nativeInfo->metadata().get() + nativeInfo->metadata_size());
    libt::entry torrentEntry(libt::entry::dictionary_t);
    torrentEntry["info"] = meta;
    if (!nativeInfo->trackers().empty())
        torrentEntry["announce"] = nativeInfo->trackers().front().url;

    QVector<char> out;
    libt::bencode(std::back_inserter(out), torrentEntry);
    QFile torrentFile(path);
    if (!out.empty() && torrentFile.open(QIODevice::WriteOnly))
        return (torrentFile.write(&out[0], out.size()) == out.size());

    return false;
}

void TorrentHandleImpl::setFilePriority(int index, int priority)
{
    std::vector<int> priorities;
    SAFE_GET(priorities, file_priorities);

    if ((priorities.size() > static_cast<quint64>(index)) && (priorities[index] != priority)) {
        priorities[index] = priority;
        prioritizeFiles(QVector<int>::fromStdVector(priorities));
    }
}

void TorrentHandleImpl::handleStateUpdate(const libt::torrent_status &nativeStatus)
{
    m_nativeStatus = nativeStatus;
#if LIBTORRENT_VERSION_NUM < 10000
    try {
        m_nativeName = misc::toQStringU(CALL(name);
        m_nativeSavePath = fsutils::fromNativePath(misc::toQStringU(CALL(save_path));
    }
    catch (std::exception &exc) {
        qDebug("torrent_handle method inside TorrentHandleImpl::updateStatus() throws exception: %s", exc.what());
    }
#endif

    updateSeedStatus();
}

void TorrentHandleImpl::handleStorageMoved(const QString &newPath)
{
    qDebug() << Q_FUNC_INFO;

    if (!isMoveInProgress()) {
        qWarning("Unexpected TorrentHandleImpl::handleStorageMoved() call.");
        return;
    }

    if (newPath != m_newPath) {
        qWarning("TorrentHandleImpl::handleStorageMoved(): New path doesn't match a path in a queue.");
        return;
    }

    // We still have torrent old save path cached here
    QString oldPath = actualSavePath();
    qDebug("Torrent is successfully moved from %s to %s", qPrintable(oldPath), qPrintable(m_newPath));

    updateStatus();
    m_session->torrentNotifyNeedSaveResumeData(this);

    m_newPath.clear();
    if (!m_queuedPath.isEmpty()) {
        moveStorage(m_queuedPath);
        m_queuedPath.clear();
    }

    if (!useTempPath()) {
        m_savePath = newPath;
        m_session->torrentNotifySavePathChanged(this);
    }

    // Attempt to remove old folder if empty
    QDir oldSaveDir(fsutils::fromNativePath(oldPath));
    if ((oldSaveDir != QDir(m_session->defaultSavePath())) && (oldSaveDir != QDir(m_session->tempPath()))) {
        qDebug("Attempting to remove %s", qPrintable(oldPath));
        QDir().rmpath(oldPath);
    }

//    forceRecheck();
}

void TorrentHandleImpl::handleStorageMovedFailed()
{
    qDebug() << Q_FUNC_INFO;

    if (!isMoveInProgress()) {
        qWarning("Unexpected TorrentHandleImpl::handleStorageMovedFailed() call.");
        return;
    }

    m_newPath.clear();
    if (!m_queuedPath.isEmpty()) {
        moveStorage(m_queuedPath);
        m_queuedPath.clear();
    }
}

void TorrentHandleImpl::handleTrackerReply(const QString &trackerUrl, int peersCount)
{
    qDebug("Received a tracker reply from %s (Num_peers = %d)", qPrintable(trackerUrl), peersCount);
    // Connection was successful now. Remove possible old errors
    m_trackerInfos[trackerUrl].lastMessage.clear(); // Reset error/warning message
    m_trackerInfos[trackerUrl].numPeers = peersCount;
}

void TorrentHandleImpl::handleTrackerWarning(const QString &trackerUrl, const QString &message)
{
    qDebug("Received a tracker warning for %s: %s", qPrintable(trackerUrl), qPrintable(message));
    // Connection was successful now but there is a warning message
    m_trackerInfos[trackerUrl].lastMessage = message; // Store warning message
}

void TorrentHandleImpl::handleTrackerError(const QString &trackerUrl, const QString &message)
{
    qDebug("Received a tracker error for %s: %s", qPrintable(trackerUrl), qPrintable(message));
    m_trackerInfos[trackerUrl].lastMessage = message;
}

void TorrentHandleImpl::handleTorrentChecked()
{
    qDebug("%s have just finished checking", qPrintable(hash()));

    updateStatus();

    // Move to temp directory if necessary
    if (useTempPath()) {
        // Check if directory is different
        const QDir currentDir(actualSavePath());
        const QDir saveDir(savePath());
        if (currentDir == saveDir) {
            qDebug("Moving the torrent to the temp directory...");
            moveStorage(m_session->tempPath());
        }
    }

    if (m_pauseAfterRecheck) {
        m_pauseAfterRecheck = false;
        pause();
    }
}

void TorrentHandleImpl::handleTorrentFinished()
{
    updateStatus();
    m_session->torrentNotifyNeedSaveResumeData(this);

    adjustActualSavePath();
}

void TorrentHandleImpl::handleTorrentPaused()
{
    if (!hasError() && !hasMissingFiles())
        m_session->torrentNotifyNeedSaveResumeData(this);

    m_speedMonitor.reset();
}

void TorrentHandleImpl::handleSaveResumeData(boost::shared_ptr<libt::entry> data)
{
    (*data)["qBt-addedTime"] = m_addedTime.toTime_t();
    (*data)["qBt-savePath"] = m_savePath.toUtf8().constData();
    (*data)["qBt-ratioLimit"] = QString::number(m_ratioLimit).toUtf8().constData();
    (*data)["qBt-label"] = m_label.toUtf8().constData();
    (*data)["qBt-name"] = m_name.toUtf8().constData();
    (*data)["qBt-seedStatus"] = m_hasSeedStatus;
    (*data)["qBt-tempPathDisabled"] = m_tempPathDisabled;
}

void TorrentHandleImpl::handleFastResumeRejected(bool hasMissingFiles)
{
    updateStatus();
    setHasMissingFiles(hasMissingFiles);
}

void TorrentHandleImpl::handleFileRenamed(int index, const QString &newName)
{
    if (filesCount() > 1) {
        // Check if folders were renamed
        QStringList oldPathParts = origFilePath(index).split("/");
        oldPathParts.removeLast();
        QString oldPath = oldPathParts.join("/");
        QStringList newPathParts = newName.split("/");
        newPathParts.removeLast();
        QString newPath = newPathParts.join("/");
        if (!newPathParts.isEmpty() && (oldPath != newPath)) {
            qDebug("oldPath(%s) != newPath(%s)", qPrintable(oldPath), qPrintable(newPath));
            oldPath = QString("%1/%2").arg(actualSavePath()).arg(oldPath);
            qDebug("Detected folder renaming, attempt to delete old folder: %s", qPrintable(oldPath));
            QDir().rmpath(oldPath);
        }
    }

    updateStatus();

    if (filesCount() == 1) {
        // Single-file torrent
        // Renaming a file corresponds to changing the save path
        m_session->torrentNotifyNeedSaveResumeData(this);
        m_session->torrentNotifySavePathChanged(this);
    }
}

void TorrentHandleImpl::handleFileCompleted(int index)
{
    qDebug("A file completed download in torrent \"%s\"", qPrintable(name()));
    if (m_session->isAppendExtensionEnabled()) {
        qDebug("appendExtension is true");
        QString name = filePath(index);
        if (name.endsWith(QB_EXT)) {
            const QString oldName = name;
            name.chop(QString(QB_EXT).size());
            qDebug("Renaming %s to %s", qPrintable(oldName), qPrintable(name));
            renameFile(index, name);
        }
    }
}

void TorrentHandleImpl::handleMetadataReceived()
{
    updateStatus();
    if (needSaveResumeData())
        m_session->torrentNotifyNeedSaveResumeData(this);
    appendExtensionsToIncompleteFiles();
}

void TorrentHandleImpl::handleStats(int downloadPayload, int uploadPayload, int interval)
{
    SpeedSample transferred(downloadPayload * 1000LL / interval, uploadPayload * 1000LL / interval);
    m_speedMonitor.addSample(transferred);
}

void TorrentHandleImpl::handleDefaultSavePathChanged()
{
    adjustSavePath();
}

void TorrentHandleImpl::adjustSavePath()
{
    // If we use default save path...
    if (m_useDefaultSavePath) {
        QString defaultSavePath = fsutils::toNativePath(m_session->torrentDefaultSavePath(this));
        if (m_savePath != defaultSavePath) {
            if (!useTempPath()) {
                moveStorage(defaultSavePath);
            }
            else {
                m_savePath = defaultSavePath;
                m_session->torrentNotifyNeedSaveResumeData(this);
                m_session->torrentNotifySavePathChanged(this);
            }
        }
    }
}

void TorrentHandleImpl::handleTempPathChanged()
{
    adjustActualSavePath();
}

void TorrentHandleImpl::handleAppendExtensionToggled()
{
    if (!hasMetadata()) return;

    if (m_session->isAppendExtensionEnabled())
        appendExtensionsToIncompleteFiles();
    else
        removeExtensionsFromIncompleteFiles();
}

void TorrentHandleImpl::appendExtensionsToIncompleteFiles()
{
    QVector<qreal> fp = filesProgress();
    for (int i = 0; i < filesCount(); ++i) {
        if ((fileSize(i) > 0) && (fp[i] < 1)) {
            const QString name = filePath(i);
            if (!name.endsWith(QB_EXT)) {
                const QString newName = name + QB_EXT;
                qDebug("Renaming %s to %s", qPrintable(name), qPrintable(newName));
                renameFile(i, newName);
            }
        }
    }
}

void TorrentHandleImpl::removeExtensionsFromIncompleteFiles()
{
    for (int i = 0; i < filesCount(); ++i) {
        QString name = filePath(i);
        if (name.endsWith(QB_EXT)) {
            const QString oldName = name;
            name.chop(QString(QB_EXT).size());
            qDebug("Renaming %s to %s", qPrintable(oldName), qPrintable(name));
            renameFile(i, name);
        }
    }
}

void TorrentHandleImpl::adjustActualSavePath()
{
    QString path;
    if (!useTempPath()) {
        // Disabling temp dir
        // Moving all torrents to their destination folder
        path = savePath();
    }
    else {
        // Moving all downloading torrents to temporary save path
        qDebug("Moving torrent to its temp save path: %s", qPrintable(path));
        path = m_session->tempPath();
    }

    moveStorage(fsutils::toNativePath(path));
}

libtorrent::torrent_handle TorrentHandleImpl::nativeHandle() const
{
    return m_nativeHandle;
}

const libtorrent::torrent_info *TorrentHandleImpl::torrentInfo() const
{
    if (!hasMetadata()) return 0;

    const libtorrent::torrent_info *result;
#if LIBTORRENT_VERSION_NUM < 10000
    result = &SAFE_CALL(get_torrent_info);
#else
    result = m_nativeStatus.torrent_file.get();
#endif

    return  result;
}

void TorrentHandleImpl::initialize()
{
    updateStatus();

    m_hash = InfoHash(m_nativeStatus.info_hash);
    if (m_savePath.isEmpty()) {
        // we use default save path
        m_savePath = actualSavePath();
        m_useDefaultSavePath = true;
    }

    adjustActualSavePath();
    adjustSavePath();
}

bool TorrentHandleImpl::isMoveInProgress() const
{
    return !m_newPath.isEmpty();
}

bool TorrentHandleImpl::useTempPath() const
{
    return !m_tempPathDisabled && m_session->isTempPathEnabled() && !isSeed();
}

void TorrentHandleImpl::updateStatus()
{
    libt::torrent_status status;
    SAFE_GET(status, status);

    handleStateUpdate(status);
}

void TorrentHandleImpl::updateSeedStatus()
{
    bool b = isSeed();
    if (m_hasSeedStatus != b) {
        m_hasSeedStatus = b;
        m_session->torrentNotifyNeedSaveResumeData(this);
        adjustActualSavePath();
    }
}

void TorrentHandleImpl::setHasMissingFiles(bool b)
{
    m_hasMissingFiles = b;
    if (m_hasMissingFiles && !isPaused())
        pause();
}

void TorrentHandleImpl::setRatioLimit(qreal limit)
{
    if (limit < USE_GLOBAL_RATIO)
        limit = NO_RATIO_LIMIT;
    else if (limit > MAX_RATIO)
        limit = MAX_RATIO;

    if (m_ratioLimit != limit) {
        m_ratioLimit = limit;
        m_session->torrentNotifyNeedSaveResumeData(this);
        m_session->torrentNotifyRatioLimitChanged();
    }
}

void TorrentHandleImpl::setUploadLimit(int limit)
{
    SAFE_CALL(set_upload_limit, limit)
}

void TorrentHandleImpl::setDownloadLimit(int limit)
{
    SAFE_CALL(set_download_limit, limit)
}

void TorrentHandleImpl::setSuperSeeding(bool enable)
{
    SAFE_CALL(super_seeding, enable)
}

void TorrentHandleImpl::flushCache()
{
    SAFE_CALL(flush_cache)
}

void TorrentHandleImpl::replaceTrackers(QList<TrackerEntry> trackers)
{
    std::vector<libt::announce_entry> announces;
    foreach (const TrackerEntry &entry, trackers)
        announces.push_back(entry.nativeEntry());

    SAFE_CALL(replace_trackers, announces)
}

QString TorrentHandleImpl::toMagnetUri() const
{
    if (!hasMetadata()) return QString();
    return misc::toQString(libt::make_magnet_uri(*torrentInfo()));
}

void TorrentHandleImpl::prioritizeFiles(const QVector<int> &files)
{
    qDebug() << Q_FUNC_INFO;
    if (files.size() != filesCount()) return;

    qDebug() << Q_FUNC_INFO << "Changing files priorities...";
    SAFE_CALL(prioritize_files, files.toStdVector());
    qDebug() << Q_FUNC_INFO << "Moving unwanted files to .unwanted folder and conversely...";

    QString spath = actualSavePath();
    for (int i = 0; i < files.size(); ++i) {
        QString filepath = filePath(i);
        // Move unwanted files to a .unwanted subfolder
        if (files[i] == 0) {
            QString old_abspath = QDir(spath).absoluteFilePath(filepath);
            QString parent_abspath = fsutils::branchPath(old_abspath);
            // Make sure the file does not already exists
            if (QDir(parent_abspath).dirName() != ".unwanted") {
                QString unwanted_abspath = parent_abspath + "/.unwanted";
                QString new_abspath = unwanted_abspath + "/" + fsutils::fileName(filepath);
                qDebug() << "Unwanted path is" << unwanted_abspath;
                if (QFile::exists(new_abspath)) {
                    qWarning() << "File" << new_abspath << "already exists at destination.";
                    continue;
                }

#ifdef Q_OS_WIN
                bool created = QDir().mkpath(unwanted_abspath);
                qDebug() << "unwanted folder was created:" << created;
                if (created) {
                    // Hide the folder on Windows
                    qDebug() << "Hiding folder (Windows)";
                    std::wstring win_path =  fsutils::toNativePath(unwanted_abspath).toStdWString();
                    DWORD dwAttrs = ::GetFileAttributesW(win_path.c_str());
                    bool ret = ::SetFileAttributesW(win_path.c_str(), dwAttrs | FILE_ATTRIBUTE_HIDDEN);
                    Q_ASSERT(ret != 0); Q_UNUSED(ret);
                }
#endif
                QString parent_path = fsutils::branchPath(filepath);
                if (!parent_path.isEmpty() && !parent_path.endsWith("/"))
                    parent_path += "/";
                renameFile(i, parent_path + ".unwanted/" + fsutils::fileName(filepath));
            }
        }

        // Move wanted files back to their original folder
        if (files[i] > 0) {
            QString parent_relpath = fsutils::branchPath(filepath);
            if (QDir(parent_relpath).dirName() == ".unwanted") {
                QString old_name = fsutils::fileName(filepath);
                QString new_relpath = fsutils::branchPath(parent_relpath);
                if (new_relpath.isEmpty())
                    renameFile(i, old_name);
                else
                    renameFile(i, QDir(new_relpath).filePath(old_name));

                // Remove .unwanted directory if empty
                qDebug() << "Attempting to remove .unwanted folder at " << QDir(spath + "/" + new_relpath).absoluteFilePath(".unwanted");
                QDir(spath + "/" + new_relpath).rmdir(".unwanted");
            }
        }
    }

    updateStatus();
}

// See if there are supported files in the torrent
bool TorrentHandleImpl::isFilePreviewPossible() const
{
    int nbFiles = filesCount();
    for (int i = 0; i < nbFiles; ++i) {
        if (misc::isPreviewable(fsutils::fileExtension(fileName(i))))
            return true;
    }

    return false;
}
