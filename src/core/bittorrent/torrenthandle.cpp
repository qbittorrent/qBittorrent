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
#include <QStringList>
#include <QFile>
#include <QDir>
#include <QByteArray>
#include <QBitArray>

#include <libtorrent/version.hpp>
#include <libtorrent/entry.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/address.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/create_torrent.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <boost/bind.hpp>

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

#include "core/logger.h"
#include "core/preferences.h"
#include "core/utils/string.h"
#include "core/utils/fs.h"
#include "core/utils/misc.h"
#include "session.h"
#include "peerinfo.h"
#include "trackerentry.h"
#include "torrenthandle.h"

static const char QB_EXT[] = ".!qB";

namespace libt = libtorrent;
using namespace BitTorrent;

// TrackerInfo

TrackerInfo::TrackerInfo()
    : numPeers(0)
{
}

// TorrentState

TorrentState::TorrentState(int value)
    : m_value(value)
{
}

QString TorrentState::toString() const
{
    switch (m_value) {
    case Error:
        return QLatin1String("error");
    case Uploading:
        return QLatin1String("uploading");
    case PausedUploading:
        return QLatin1String("pausedUP");
    case QueuedUploading:
        return QLatin1String("queuedUP");
    case StalledUploading:
        return QLatin1String("stalledUP");
    case CheckingUploading:
        return QLatin1String("checkingUP");
    case ForcedUploading:
        return QLatin1String("forcedUP");
    case Allocating:
        return QLatin1String("allocating");
    case Downloading:
        return QLatin1String("downloading");
    case DownloadingMetadata:
        return QLatin1String("metaDL");
    case PausedDownloading:
        return QLatin1String("pausedDL");
    case QueuedDownloading:
        return QLatin1String("queuedDL");
    case StalledDownloading:
        return QLatin1String("stalledDL");
    case CheckingDownloading:
        return QLatin1String("checkingDL");
    case ForcedDownloading:
        return QLatin1String("forcedDL");
    case QueuedForChecking:
        return QLatin1String("queuedForChecking");
    case CheckingResumeData:
        return QLatin1String("checkingResumeData");
    default:
        return QLatin1String("unknown");
    }
}

TorrentState::operator int() const
{
    return m_value;
}

// AddTorrentData

AddTorrentData::AddTorrentData() {}

AddTorrentData::AddTorrentData(const AddTorrentParams &in)
    : resumed(false)
    , name(in.name)
    , label(in.label)
    , savePath(in.savePath)
    , disableTempPath(in.disableTempPath)
    , sequential(in.sequential)
    , hasSeedStatus(in.skipChecking)
    , addForced(in.addForced)
    , addPaused(in.addPaused)
    , filePriorities(in.filePriorities)
    , ratioLimit(in.ignoreShareRatio ? TorrentHandle::NO_RATIO_LIMIT : TorrentHandle::USE_GLOBAL_RATIO)
{
}

// TorrentHandle

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

const qreal TorrentHandle::USE_GLOBAL_RATIO = -2.;
const qreal TorrentHandle::NO_RATIO_LIMIT = -1.;

const qreal TorrentHandle::MAX_RATIO = 9999.;

TorrentHandle::TorrentHandle(Session *session, const libtorrent::torrent_handle &nativeHandle,
                                     const AddTorrentData &data)
    : QObject(session)
    , m_session(session)
    , m_nativeHandle(nativeHandle)
    , m_state(TorrentState::Unknown)
    , m_name(data.name)
    , m_addedTime(data.resumed ? data.addedTime : QDateTime::currentDateTime())
    , m_savePath(Utils::Fs::toNativePath(data.savePath))
    , m_label(data.label)
    , m_hasSeedStatus(data.resumed ? data.hasSeedStatus : false)
    , m_ratioLimit(data.ratioLimit)
    , m_tempPathDisabled(data.disableTempPath)
    , m_hasMissingFiles(false)
    , m_useDefaultSavePath(false)
    , m_pauseAfterRecheck(false)
    , m_needSaveResumeData(false)
{
    initialize();
    setSequentialDownload(data.sequential);

#ifndef DISABLE_COUNTRIES_RESOLUTION
    resolveCountries(m_session->isResolveCountriesEnabled());
#endif

    if (!data.resumed) {
        if (hasMetadata()) {
            setFirstLastPiecePriority(data.sequential);
            if (m_session->isAppendExtensionEnabled())
                appendExtensionsToIncompleteFiles();
        }
    }
}

TorrentHandle::~TorrentHandle() {}

bool TorrentHandle::isValid() const
{
    return m_nativeHandle.is_valid();
}

InfoHash TorrentHandle::hash() const
{
    return m_hash;
}

QString TorrentHandle::name() const
{
    QString name = m_name;
    if (name.isEmpty()) {
#if LIBTORRENT_VERSION_NUM < 10000
        name = m_nativeName;
#else
        name = Utils::String::fromStdString(m_nativeStatus.name);
#endif
    }

    if (name.isEmpty())
        name = m_hash;

    return name;
}

QDateTime TorrentHandle::creationDate() const
{
    return m_torrentInfo.creationDate();
}

QString TorrentHandle::creator() const
{
    return m_torrentInfo.creator();
}

QString TorrentHandle::comment() const
{
    return m_torrentInfo.comment();
}

bool TorrentHandle::isPrivate() const
{
    return m_torrentInfo.isPrivate();
}

qlonglong TorrentHandle::totalSize() const
{
    return m_torrentInfo.totalSize();
}

// get the size of the torrent without the filtered files
qlonglong TorrentHandle::wantedSize() const
{
    return m_nativeStatus.total_wanted;
}

qlonglong TorrentHandle::completedSize() const
{
    return m_nativeStatus.total_wanted_done;
}

qlonglong TorrentHandle::incompletedSize() const
{
    return (m_nativeStatus.total_wanted - m_nativeStatus.total_wanted_done);
}

qlonglong TorrentHandle::pieceLength() const
{
    return m_torrentInfo.pieceLength();
}

qlonglong TorrentHandle::wastedSize() const
{
    return (m_nativeStatus.total_failed_bytes + m_nativeStatus.total_redundant_bytes);
}

QString TorrentHandle::currentTracker() const
{
    return Utils::String::fromStdString(m_nativeStatus.current_tracker);
}

QString TorrentHandle::savePath() const
{
    return Utils::Fs::fromNativePath(m_savePath);
}

QString TorrentHandle::rootPath() const
{
    if (filesCount() > 1) {
        QString first_filepath = filePath(0);
        const int slashIndex = first_filepath.indexOf("/");
        if (slashIndex >= 0)
            return QDir(actualSavePath()).absoluteFilePath(first_filepath.left(slashIndex));
    }

    return actualSavePath();
}

QString TorrentHandle::nativeActualSavePath() const
{
#if LIBTORRENT_VERSION_NUM < 10000
    return m_nativeSavePath;
#else
    return Utils::String::fromStdString(m_nativeStatus.save_path);
#endif
}

QString TorrentHandle::actualSavePath() const
{
    return Utils::Fs::fromNativePath(nativeActualSavePath());
}

QList<TrackerEntry> TorrentHandle::trackers() const
{
    QList<TrackerEntry> entries;
    std::vector<libt::announce_entry> announces;
    SAFE_GET(announces, trackers);

    foreach (const libt::announce_entry &tracker, announces)
        entries << tracker;

    return entries;
}

QHash<QString, TrackerInfo> TorrentHandle::trackerInfos() const
{
    return m_trackerInfos;
}

void TorrentHandle::addTrackers(const QList<TrackerEntry> &trackers)
{
    QList<TrackerEntry> addedTrackers;
    foreach (const TrackerEntry &tracker, trackers) {
        if (addTracker(tracker))
            addedTrackers << tracker;
    }

    if (!addedTrackers.isEmpty())
        m_session->handleTorrentTrackersAdded(this, addedTrackers);
}

void TorrentHandle::replaceTrackers(QList<TrackerEntry> trackers)
{
    QList<TrackerEntry> existingTrackers = this->trackers();
    QList<TrackerEntry> addedTrackers;

    std::vector<libt::announce_entry> announces;
    foreach (const TrackerEntry &tracker, trackers) {
        announces.push_back(tracker.nativeEntry());
        if (!existingTrackers.contains(tracker))
            addedTrackers << tracker;
        else
            existingTrackers.removeOne(tracker);
    }

    try {
        m_nativeHandle.replace_trackers(announces);
        if (addedTrackers.isEmpty() && existingTrackers.isEmpty()) {
            m_session->handleTorrentTrackersChanged(this);
        }
        else {
            if (!existingTrackers.isEmpty())
                m_session->handleTorrentTrackersRemoved(this, existingTrackers);
            if (!addedTrackers.isEmpty())
                m_session->handleTorrentTrackersAdded(this, addedTrackers);
        }

    }
    catch (std::exception &exc) {
        qDebug("torrent_handle::replace_trackers() throws exception: %s", exc.what());
    }
}

bool TorrentHandle::addTracker(const TrackerEntry &tracker)
{
    if (trackers().contains(tracker))
        return false;

    SAFE_CALL_BOOL(add_tracker, tracker.nativeEntry());
}

QList<QUrl> TorrentHandle::urlSeeds() const
{
    QList<QUrl> urlSeeds;
    std::set<std::string> seeds;
    SAFE_GET(seeds, url_seeds);

    foreach (const std::string &urlSeed, seeds)
        urlSeeds.append(QUrl(urlSeed.c_str()));

    return urlSeeds;
}

void TorrentHandle::addUrlSeeds(const QList<QUrl> &urlSeeds)
{
    QList<QUrl> addedUrlSeeds;
    foreach (const QUrl &urlSeed, urlSeeds) {
        if (addUrlSeed(urlSeed))
            addedUrlSeeds << urlSeed;
    }

    if (!addedUrlSeeds.isEmpty())
        m_session->handleTorrentUrlSeedsAdded(this, addedUrlSeeds);
}

void TorrentHandle::removeUrlSeeds(const QList<QUrl> &urlSeeds)
{
    QList<QUrl> removedUrlSeeds;
    foreach (const QUrl &urlSeed, urlSeeds) {
        if (removeUrlSeed(urlSeed))
            removedUrlSeeds << urlSeed;
    }

    if (!removedUrlSeeds.isEmpty())
        m_session->handleTorrentUrlSeedsRemoved(this, removedUrlSeeds);
}

bool TorrentHandle::addUrlSeed(const QUrl &urlSeed)
{
    QList<QUrl> seeds = urlSeeds();
    if (seeds.contains(urlSeed)) return false;

    SAFE_CALL_BOOL(add_url_seed, Utils::String::toStdString(urlSeed.toString()));
}

bool TorrentHandle::removeUrlSeed(const QUrl &urlSeed)
{
    QList<QUrl> seeds = urlSeeds();
    if (!seeds.contains(urlSeed)) return false;

    SAFE_CALL_BOOL(remove_url_seed, Utils::String::toStdString(urlSeed.toString()));
}

bool TorrentHandle::connectPeer(const PeerAddress &peerAddress)
{
    libt::error_code ec;
    libt::address addr = libt::address::from_string(Utils::String::toStdString(peerAddress.ip.toString()), ec);
    if (ec) return false;

    libt::asio::ip::tcp::endpoint ep(addr, peerAddress.port);
    SAFE_CALL_BOOL(connect_peer, ep);
}

bool TorrentHandle::needSaveResumeData() const
{
    if (m_needSaveResumeData) return true;

    SAFE_RETURN(bool, need_save_resume_data, false);
}

void TorrentHandle::saveResumeData()
{
    SAFE_CALL(save_resume_data);
    m_needSaveResumeData = false;
}

QString TorrentHandle::savePathParsed() const
{
    QString p;
    if (hasMetadata() && (filesCount() == 1))
        p = firstFileSavePath();
    else
        p = savePath();

    return Utils::Fs::toNativePath(p);
}

int TorrentHandle::filesCount() const
{
    return m_torrentInfo.filesCount();
}

int TorrentHandle::piecesCount() const
{
    return m_torrentInfo.piecesCount();
}

int TorrentHandle::piecesHave() const
{
    return m_nativeStatus.num_pieces;
}

qreal TorrentHandle::progress() const
{
    if (!m_nativeStatus.total_wanted)
        return 0.;

    if (m_nativeStatus.total_wanted_done == m_nativeStatus.total_wanted)
        return 1.;

    float progress = (float) m_nativeStatus.total_wanted_done / (float) m_nativeStatus.total_wanted;
    Q_ASSERT((progress >= 0.f) && (progress <= 1.f));
    return progress;
}

QString TorrentHandle::label() const
{
    return m_label;
}

QDateTime TorrentHandle::addedTime() const
{
    return m_addedTime;
}

qreal TorrentHandle::ratioLimit() const
{
    return m_ratioLimit;
}


QString TorrentHandle::firstFileSavePath() const
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

QString TorrentHandle::filePath(int index) const
{
    return m_torrentInfo.filePath(index);
}

QString TorrentHandle::fileName(int index) const
{
    if (!hasMetadata()) return  QString();
    return Utils::Fs::fileName(filePath(index));
}

qlonglong TorrentHandle::fileSize(int index) const
{
    return m_torrentInfo.fileSize(index);
}

// Return a list of absolute paths corresponding
// to all files in a torrent
QStringList TorrentHandle::absoluteFilePaths() const
{
    if (!hasMetadata()) return  QStringList();

    QDir saveDir(actualSavePath());
    QStringList res;
    for (int i = 0; i < filesCount(); ++i)
        res << Utils::Fs::expandPathAbs(saveDir.absoluteFilePath(filePath(i)));
    return res;
}

QStringList TorrentHandle::absoluteFilePathsUnwanted() const
{
    if (!hasMetadata()) return  QStringList();

    QDir saveDir(actualSavePath());
    QStringList res;
    std::vector<int> fp;
    SAFE_GET(fp, file_priorities);

    int count = static_cast<int>(fp.size());
    for (int i = 0; i < count; ++i) {
        if (fp[i] == 0) {
            const QString path = Utils::Fs::expandPathAbs(saveDir.absoluteFilePath(filePath(i)));
            if (path.contains(".unwanted"))
                res << path;
        }
    }

    return res;
}

QPair<int, int> TorrentHandle::fileExtremityPieces(int index) const
{
    if (!hasMetadata()) return qMakePair(-1, -1);

    const int numPieces = piecesCount();
    const qlonglong pieceSize = pieceLength();

    // Determine the first and last piece of the file
    int firstPiece = floor((m_torrentInfo.fileOffset(index) + 1) / (float) pieceSize);
    Q_ASSERT((firstPiece >= 0) && (firstPiece < numPieces));

    int numPiecesInFile = ceil(fileSize(index) / (float) pieceSize);
    int lastPiece = firstPiece + numPiecesInFile - 1;
    Q_ASSERT((lastPiece >= 0) && (lastPiece < numPieces));

    Q_UNUSED(numPieces)
    return qMakePair(firstPiece, lastPiece);
}

QVector<int> TorrentHandle::filePriorities() const
{
    std::vector<int> fp;
    SAFE_GET(fp, file_priorities);

    return QVector<int>::fromStdVector(fp);
}

TorrentInfo TorrentHandle::info() const
{
    return m_torrentInfo;
}

bool TorrentHandle::isPaused() const
{
    return (m_nativeStatus.paused && !m_nativeStatus.auto_managed);
}

bool TorrentHandle::isResumed() const
{
    return !isPaused();
}

bool TorrentHandle::isQueued() const
{
    return (m_nativeStatus.paused && m_nativeStatus.auto_managed);
}

bool TorrentHandle::isChecking() const
{
    return ((m_nativeStatus.state == libt::torrent_status::queued_for_checking)
            || (m_nativeStatus.state == libt::torrent_status::checking_files)
            || (m_nativeStatus.state == libt::torrent_status::checking_resume_data));
}

bool TorrentHandle::isDownloading() const
{
    return m_state == TorrentState::Downloading
            || m_state == TorrentState::DownloadingMetadata
            || m_state == TorrentState::StalledDownloading
            || m_state == TorrentState::CheckingDownloading
            || m_state == TorrentState::PausedDownloading
            || m_state == TorrentState::QueuedDownloading
            || m_state == TorrentState::ForcedDownloading
            || m_state == TorrentState::Error;
}

bool TorrentHandle::isUploading() const
{
    return m_state == TorrentState::Uploading
            || m_state == TorrentState::StalledUploading
            || m_state == TorrentState::CheckingUploading
            || m_state == TorrentState::QueuedUploading
            || m_state == TorrentState::ForcedUploading;
}

bool TorrentHandle::isCompleted() const
{
    return m_state == TorrentState::Uploading
            || m_state == TorrentState::StalledUploading
            || m_state == TorrentState::CheckingUploading
            || m_state == TorrentState::PausedUploading
            || m_state == TorrentState::QueuedUploading;
}

bool TorrentHandle::isActive() const
{
    if (m_state == TorrentState::StalledDownloading)
        return (uploadPayloadRate() > 0);

    return m_state == TorrentState::DownloadingMetadata
            || m_state == TorrentState::Downloading
            || m_state == TorrentState::ForcedDownloading
            || m_state == TorrentState::Uploading
            || m_state == TorrentState::ForcedUploading;
}

bool TorrentHandle::isInactive() const
{
    return !isActive();
}

bool TorrentHandle::isSeed() const
{
    // Affected by bug http://code.rasterbar.com/libtorrent/ticket/402
    //SAFE_RETURN(bool, is_seed, false);
    // May suffer from approximation problems
    //return (progress() == 1.);
    // This looks safe
    return ((m_nativeStatus.state == libt::torrent_status::finished)
            || (m_nativeStatus.state == libt::torrent_status::seeding));
}

bool TorrentHandle::isForced() const
{
    return (!m_nativeStatus.paused && !m_nativeStatus.auto_managed);
}

bool TorrentHandle::isSequentialDownload() const
{
    return m_nativeStatus.sequential_download;
}

bool TorrentHandle::hasFirstLastPiecePriority() const
{
    if (!hasMetadata()) return false;

    // Get int first media file
    std::vector<int> fp;
    SAFE_GET(fp, file_priorities);

    QPair<int, int> extremities;
    bool found = false;
    int count = static_cast<int>(fp.size());
    for (int i = 0; i < count; ++i) {
        const QString ext = Utils::Fs::fileExtension(filePath(i));
        if (Utils::Misc::isPreviewable(ext) && (fp[i] > 0)) {
            extremities = fileExtremityPieces(i);
            found = true;
            break;
        }
    }

    if (!found) return false; // No media file

    int first = 0;
    int last = 0;
    SAFE_GET(first, piece_priority, extremities.first);
    SAFE_GET(last, piece_priority, extremities.second);

    return ((first == 7) && (last == 7));
}

TorrentState TorrentHandle::state() const
{
    return m_state;
}

void TorrentHandle::updateState()
{
    if (isPaused()) {
        if (hasError() || hasMissingFiles())
            m_state = TorrentState::Error;
        else
            m_state = isSeed() ? TorrentState::PausedUploading : TorrentState::PausedDownloading;
    }
    else {
        if (m_session->isQueueingEnabled() && isQueued() && !isChecking()) {
            m_state = isSeed() ? TorrentState::QueuedUploading : TorrentState::QueuedDownloading;
        }
        else {
            switch (m_nativeStatus.state) {
            case libt::torrent_status::finished:
            case libt::torrent_status::seeding:
                if (isForced())
                    m_state = TorrentState::ForcedUploading;
                else
                    m_state = m_nativeStatus.upload_payload_rate > 0 ? TorrentState::Uploading : TorrentState::StalledUploading;
                break;
            case libt::torrent_status::allocating:
                m_state = TorrentState::Allocating;
                break;
            case libt::torrent_status::queued_for_checking:
                m_state = TorrentState::QueuedForChecking;
                break;
            case libt::torrent_status::checking_resume_data:
                m_state = TorrentState::CheckingResumeData;
                break;
            case libt::torrent_status::checking_files:
                m_state = m_hasSeedStatus ? TorrentState::CheckingUploading : TorrentState::CheckingDownloading;
                break;
            case libt::torrent_status::downloading_metadata:
                m_state = TorrentState::DownloadingMetadata;
                break;
            case libt::torrent_status::downloading:
                if (isForced())
                    m_state = TorrentState::ForcedDownloading;
                else
                    m_state = m_nativeStatus.download_payload_rate > 0 ? TorrentState::Downloading : TorrentState::StalledDownloading;
                break;
            default:
                qWarning("Unrecognized torrent status, should not happen!!! status was %d", m_nativeStatus.state);
                m_state = TorrentState::Unknown;
            }
        }
    }
}

bool TorrentHandle::hasMetadata() const
{
    return m_nativeStatus.has_metadata;
}

bool TorrentHandle::hasMissingFiles() const
{
    return m_hasMissingFiles;
}

bool TorrentHandle::hasError() const
{
    return (m_nativeStatus.paused && !m_nativeStatus.error.empty());
}

bool TorrentHandle::hasFilteredPieces() const
{
    std::vector<int> pp;
    SAFE_GET(pp, piece_priorities);

    foreach (const int priority, pp)
        if (priority == 0) return true;

    return false;
}

int TorrentHandle::queuePosition() const
{
    if (m_nativeStatus.queue_position < 0) return 0;

    return m_nativeStatus.queue_position + 1;
}

QString TorrentHandle::error() const
{
    return Utils::String::fromStdString(m_nativeStatus.error);
}

qlonglong TorrentHandle::totalDownload() const
{
    return m_nativeStatus.all_time_download;
}

qlonglong TorrentHandle::totalUpload() const
{
    return m_nativeStatus.all_time_upload;
}

int TorrentHandle::activeTime() const
{
    return m_nativeStatus.active_time;
}

int TorrentHandle::finishedTime() const
{
    return m_nativeStatus.finished_time;
}

int TorrentHandle::seedingTime() const
{
    return m_nativeStatus.seeding_time;
}

qulonglong TorrentHandle::eta() const
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

QVector<qreal> TorrentHandle::filesProgress() const
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

int TorrentHandle::seedsCount() const
{
    return m_nativeStatus.num_seeds;
}

int TorrentHandle::peersCount() const
{
    return m_nativeStatus.num_peers;
}

int TorrentHandle::leechsCount() const
{
    return (m_nativeStatus.num_peers - m_nativeStatus.num_seeds);
}

int TorrentHandle::totalSeedsCount() const
{
	return m_nativeStatus.list_seeds;
}

int TorrentHandle::totalPeersCount() const
{
	return m_nativeStatus.list_peers;
}

int TorrentHandle::totalLeechersCount() const
{
    return (m_nativeStatus.list_peers - m_nativeStatus.list_seeds);
}

int TorrentHandle::completeCount() const
{
    return m_nativeStatus.num_complete;
}

int TorrentHandle::incompleteCount() const
{
    return m_nativeStatus.num_incomplete;
}

QDateTime TorrentHandle::lastSeenComplete() const
{
    if (m_nativeStatus.last_seen_complete > 0)
        return QDateTime::fromTime_t(m_nativeStatus.last_seen_complete);
    else
        return QDateTime();
}

QDateTime TorrentHandle::completedTime() const
{
    if (m_nativeStatus.completed_time > 0)
        return QDateTime::fromTime_t(m_nativeStatus.completed_time);
    else
        return QDateTime();
}

int TorrentHandle::timeSinceUpload() const
{
    return m_nativeStatus.time_since_upload;
}

int TorrentHandle::timeSinceDownload() const
{
    return m_nativeStatus.time_since_download;
}

int TorrentHandle::timeSinceActivity() const
{
    if (m_nativeStatus.time_since_upload < m_nativeStatus.time_since_download)
        return m_nativeStatus.time_since_upload;
    else
        return m_nativeStatus.time_since_download;
}

int TorrentHandle::downloadLimit() const
{
    SAFE_RETURN(int, download_limit, -1)
}

int TorrentHandle::uploadLimit() const
{
    SAFE_RETURN(int, upload_limit, -1)
}

bool TorrentHandle::superSeeding() const
{
    return m_nativeStatus.super_seeding;
}

QList<PeerInfo> TorrentHandle::peers() const
{
    QList<PeerInfo> peers;
    std::vector<libt::peer_info> nativePeers;

    SAFE_CALL(get_peer_info, nativePeers);

    foreach (const libt::peer_info &peer, nativePeers)
        peers << peer;

    return peers;
}

QBitArray TorrentHandle::pieces() const
{
    QBitArray result(m_nativeStatus.pieces.size());

#if LIBTORRENT_VERSION_NUM < 10000
    typedef size_t pieces_size_t;
#else
    typedef int pieces_size_t;
#endif
    for (pieces_size_t i = 0; i < m_nativeStatus.pieces.size(); ++i)
        result.setBit(i, m_nativeStatus.pieces.get_bit(i));

    return result;
}

QBitArray TorrentHandle::downloadingPieces() const
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

QVector<int> TorrentHandle::pieceAvailability() const
{
    std::vector<int> avail;
    SAFE_CALL(piece_availability, avail);

    return QVector<int>::fromStdVector(avail);
}

qreal TorrentHandle::distributedCopies() const
{
    return m_nativeStatus.distributed_copies;
}

qreal TorrentHandle::maxRatio(bool *usesGlobalRatio) const
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

qreal TorrentHandle::realRatio() const
{
    libt::size_type upload = m_nativeStatus.all_time_upload;
    // special case for a seeder who lost its stats, also assume nobody will import a 99% done torrent
    libt::size_type download = (m_nativeStatus.all_time_download < m_nativeStatus.total_done * 0.01) ? m_nativeStatus.total_done : m_nativeStatus.all_time_download;

    if (download == 0)
        return (upload == 0) ? 0.0 : MAX_RATIO;

    qreal ratio = upload / static_cast<qreal>(download);
    Q_ASSERT(ratio >= 0.0);
    return (ratio > MAX_RATIO) ? MAX_RATIO : ratio;
}

int TorrentHandle::uploadPayloadRate() const
{
    return m_nativeStatus.upload_payload_rate;
}

int TorrentHandle::downloadPayloadRate() const
{
    return m_nativeStatus.download_payload_rate;
}

int TorrentHandle::totalPayloadUpload() const
{
    return m_nativeStatus.total_payload_upload;
}

int TorrentHandle::totalPayloadDownload() const
{
    return m_nativeStatus.total_payload_download;
}

int TorrentHandle::connectionsCount() const
{
    return m_nativeStatus.num_connections;
}

int TorrentHandle::connectionsLimit() const
{
    return m_nativeStatus.connections_limit;
}

qlonglong TorrentHandle::nextAnnounce() const
{
    return m_nativeStatus.next_announce.total_seconds();
}

void TorrentHandle::setName(const QString &name)
{
    if (m_name != name) {
        m_name = name;
        m_needSaveResumeData = true;
    }
}

void TorrentHandle::setLabel(const QString &label)
{
    if (m_label != label) {
        QString oldLabel = m_label;
        m_label = label;
        m_needSaveResumeData = true;
        adjustSavePath();
        m_session->handleTorrentLabelChanged(this, oldLabel);
    }
}

void TorrentHandle::setSequentialDownload(bool b)
{
    SAFE_CALL(set_sequential_download, b);
}

void TorrentHandle::move(QString path)
{
    // now we use non-default save path
    // even if new path same as default.
    m_useDefaultSavePath = false;

    path = Utils::Fs::toNativePath(path);
    if (path == savePath()) return;

    if (!useTempPath()) {
        moveStorage(path);
    }
    else {
        m_savePath = path;
        m_needSaveResumeData = true;
        m_session->handleTorrentSavePathChanged(this);
    }
}

#if LIBTORRENT_VERSION_NUM < 10000
void TorrentHandle::forceReannounce()
{
    SAFE_CALL(force_reannounce);
}

#else

void TorrentHandle::forceReannounce(int index)
{
    SAFE_CALL(force_reannounce, 0, index);
}
#endif

void TorrentHandle::forceDHTAnnounce()
{
    SAFE_CALL(force_dht_announce);
}

void TorrentHandle::forceRecheck()
{
    if (!hasMetadata()) return;

    if (isPaused()) {
        m_pauseAfterRecheck = true;
        resume();
    }

    SAFE_CALL(force_recheck);
}

void TorrentHandle::toggleSequentialDownload()
{
    if (hasMetadata()) {
        bool was_sequential = isSequentialDownload();
        SAFE_CALL(set_sequential_download, !was_sequential);
        if (!was_sequential)
            setFirstLastPiecePriority(true);
    }
}

void TorrentHandle::toggleFirstLastPiecePriority()
{
    if (hasMetadata())
        setFirstLastPiecePriority(!hasFirstLastPiecePriority());
}

void TorrentHandle::setFirstLastPiecePriority(bool b)
{
    std::vector<int> fp;
    SAFE_GET(fp, file_priorities);

    // Download first and last pieces first for all media files in the torrent
    int nbfiles = static_cast<int>(fp.size());
    for (int index = 0; index < nbfiles; ++index) {
        const QString path = filePath(index);
        const QString ext = Utils::Fs::fileExtension(path);
        if (Utils::Misc::isPreviewable(ext) && (fp[index] > 0)) {
            qDebug() << "File" << path << "is previewable, toggle downloading of first/last pieces first";
            // Determine the priority to set
            int prio = b ? 7 : fp[index];

            QPair<int, int> extremities = fileExtremityPieces(index);
            SAFE_CALL(piece_priority, extremities.first, prio);
            SAFE_CALL(piece_priority, extremities.second, prio);
        }
    }
}

void TorrentHandle::pause()
{
    if (isPaused()) return;

    try {
        m_nativeHandle.auto_managed(false);
        m_nativeHandle.pause();
    }
    catch (std::exception &exc) {
        qDebug("torrent_handle method inside TorrentHandleImpl::pause() throws exception: %s", exc.what());
    }
}

void TorrentHandle::resume(bool forced)
{
    try {
        if (hasError())
            m_nativeHandle.clear_error();
        m_hasMissingFiles = false;
        m_nativeHandle.set_upload_mode(false);
        m_nativeHandle.auto_managed(!forced);
        m_nativeHandle.resume();
    }
    catch (std::exception &exc) {
        qDebug("torrent_handle method inside TorrentHandleImpl::resume() throws exception: %s", exc.what());
    }
}

void TorrentHandle::moveStorage(const QString &newPath)
{
    if (isMoveInProgress()) {
        qDebug("enqueue move storage to %s", qPrintable(newPath));
        m_queuedPath = newPath;
    }
    else {
        QString oldPath = nativeActualSavePath();
        if (QDir(oldPath) == QDir(newPath)) return;

        qDebug("move storage: %s to %s", qPrintable(oldPath), qPrintable(newPath));
        // Create destination directory if necessary
        // or move_storage() will fail...
        QDir().mkpath(newPath);

        try {
            // Actually move the storage
            m_nativeHandle.move_storage(newPath.toUtf8().constData());
            m_oldPath = oldPath;
            m_newPath = newPath;
        }
        catch (std::exception &exc) {
            qDebug("torrent_handle::move_storage() throws exception: %s", exc.what());
        }
    }
}

void TorrentHandle::setTrackerLogin(const QString &username, const QString &password)
{
    SAFE_CALL(set_tracker_login, std::string(username.toLocal8Bit().constData()), std::string(password.toLocal8Bit().constData()));
}

void TorrentHandle::renameFile(int index, const QString &name)
{
    qDebug() << Q_FUNC_INFO << index << name;
    SAFE_CALL(rename_file, index, Utils::String::toStdString(Utils::Fs::toNativePath(name)));
}

bool TorrentHandle::saveTorrentFile(const QString &path)
{
    if (!m_torrentInfo.isValid()) return false;

    libt::create_torrent torrentCreator(*(m_torrentInfo.nativeInfo()));
    libt::entry torrentEntry = torrentCreator.generate();

    QVector<char> out;
    libt::bencode(std::back_inserter(out), torrentEntry);
    QFile torrentFile(path);
    if (!out.empty() && torrentFile.open(QIODevice::WriteOnly))
        return (torrentFile.write(&out[0], out.size()) == out.size());

    return false;
}

void TorrentHandle::setFilePriority(int index, int priority)
{
    std::vector<int> priorities;
    SAFE_GET(priorities, file_priorities);

    if ((priorities.size() > static_cast<quint64>(index)) && (priorities[index] != priority)) {
        priorities[index] = priority;
        prioritizeFiles(QVector<int>::fromStdVector(priorities));
    }
}

void TorrentHandle::handleStateUpdate(const libt::torrent_status &nativeStatus)
{
    updateStatus(nativeStatus);
}

void TorrentHandle::handleStorageMovedAlert(libtorrent::storage_moved_alert *p)
{
    if (!isMoveInProgress()) {
        qWarning("Unexpected TorrentHandleImpl::handleStorageMoved() call.");
        return;
    }

    QString newPath = Utils::String::fromStdString(p->path);
    if (newPath != m_newPath) {
        qWarning("TorrentHandleImpl::handleStorageMoved(): New path doesn't match a path in a queue.");
        return;
    }

    qDebug("Torrent is successfully moved from %s to %s", qPrintable(m_oldPath), qPrintable(m_newPath));
    updateStatus();

    m_newPath.clear();
    if (!m_queuedPath.isEmpty()) {
        moveStorage(m_queuedPath);
        m_queuedPath.clear();
    }

    if (!useTempPath()) {
        m_savePath = newPath;
        m_session->handleTorrentSavePathChanged(this);
    }

    // Attempt to remove old folder if empty
    QDir oldSaveDir(Utils::Fs::fromNativePath(m_oldPath));
    if ((oldSaveDir != QDir(m_session->defaultSavePath())) && (oldSaveDir != QDir(m_session->tempPath()))) {
        qDebug("Attempting to remove %s", qPrintable(m_oldPath));
        QDir().rmpath(m_oldPath);
    }

    if (!isMoveInProgress()) {
        while (!m_moveStorageTriggers.isEmpty())
            m_moveStorageTriggers.takeFirst()();
    }
}

void TorrentHandle::handleStorageMovedFailedAlert(libtorrent::storage_moved_failed_alert *p)
{
    if (!isMoveInProgress()) {
        qWarning("Unexpected TorrentHandleImpl::handleStorageMovedFailed() call.");
        return;
    }

    Logger::instance()->addMessage(tr("Could not move torrent: '%1'. Reason: %2")
                                   .arg(name()).arg(Utils::String::fromStdString(p->message())), Log::CRITICAL);

    m_newPath.clear();
    if (!m_queuedPath.isEmpty()) {
        moveStorage(m_queuedPath);
        m_queuedPath.clear();
    }

    if (!isMoveInProgress()) {
        while (!m_moveStorageTriggers.isEmpty())
            m_moveStorageTriggers.takeFirst()();
    }
}

void TorrentHandle::handleTrackerReplyAlert(libtorrent::tracker_reply_alert *p)
{
    QString trackerUrl = Utils::String::fromStdString(p->url);
    qDebug("Received a tracker reply from %s (Num_peers = %d)", qPrintable(trackerUrl), p->num_peers);
    // Connection was successful now. Remove possible old errors
    m_trackerInfos[trackerUrl].lastMessage.clear(); // Reset error/warning message
    m_trackerInfos[trackerUrl].numPeers = p->num_peers;

    m_session->handleTorrentTrackerReply(this, trackerUrl);
}

void TorrentHandle::handleTrackerWarningAlert(libtorrent::tracker_warning_alert *p)
{
    QString trackerUrl = Utils::String::fromStdString(p->url);
    QString message = Utils::String::fromStdString(p->msg);
    qDebug("Received a tracker warning for %s: %s", qPrintable(trackerUrl), qPrintable(message));
    // Connection was successful now but there is a warning message
    m_trackerInfos[trackerUrl].lastMessage = message; // Store warning message

    m_session->handleTorrentTrackerWarning(this, trackerUrl);
}

void TorrentHandle::handleTrackerErrorAlert(libtorrent::tracker_error_alert *p)
{
    QString trackerUrl = Utils::String::fromStdString(p->url);
    QString message = Utils::String::fromStdString(p->msg);
    qDebug("Received a tracker error for %s: %s", qPrintable(trackerUrl), qPrintable(message));
    m_trackerInfos[trackerUrl].lastMessage = message;

    if (p->status_code == 401)
        m_session->handleTorrentTrackerAuthenticationRequired(this, trackerUrl);

    m_session->handleTorrentTrackerError(this, trackerUrl);
}

void TorrentHandle::handleTorrentCheckedAlert(libtorrent::torrent_checked_alert *p)
{
    Q_UNUSED(p);
    qDebug("%s have just finished checking", qPrintable(hash()));

    updateStatus();
    adjustActualSavePath();

    if (m_pauseAfterRecheck) {
        m_pauseAfterRecheck = false;
        pause();
    }

    m_session->handleTorrentChecked(this);
}

void TorrentHandle::handleTorrentFinishedAlert(libtorrent::torrent_finished_alert *p)
{
    Q_UNUSED(p);
    qDebug("Got a torrent finished alert for %s", qPrintable(name()));
    qDebug("Torrent has seed status: %s", m_hasSeedStatus ? "yes" : "no");
    if (m_hasSeedStatus) return;

    updateStatus();
    m_hasMissingFiles = false;
    m_hasSeedStatus = true;

    adjustActualSavePath();
    if (Preferences::instance()->recheckTorrentsOnCompletion())
        forceRecheck();

    if (!isMoveInProgress())
        m_session->handleTorrentFinished(this);
    else
        m_moveStorageTriggers.append(boost::bind(&SessionPrivate::handleTorrentFinished, m_session, this));
}

void TorrentHandle::handleTorrentPausedAlert(libtorrent::torrent_paused_alert *p)
{
    Q_UNUSED(p);
    updateStatus();
    m_speedMonitor.reset();
    m_session->handleTorrentPaused(this);
}

void TorrentHandle::handleTorrentResumedAlert(libtorrent::torrent_resumed_alert *p)
{
    Q_UNUSED(p);
    m_session->handleTorrentResumed(this);
}

void TorrentHandle::handleSaveResumeDataAlert(libtorrent::save_resume_data_alert *p)
{
    const bool useDummyResumeData = !(p && p->resume_data);
    libtorrent::entry dummyEntry;

    libtorrent::entry &resumeData = useDummyResumeData ? dummyEntry : *(p->resume_data);
    if (useDummyResumeData) {
        resumeData["qBt-magnetUri"] = Utils::String::toStdString(toMagnetUri());
        resumeData["qBt-paused"] = isPaused();
        resumeData["qBt-forced"] = isForced();
    }
    resumeData["qBt-addedTime"] = m_addedTime.toTime_t();
    resumeData["qBt-savePath"] = m_useDefaultSavePath ? "" : Utils::String::toStdString(m_savePath);
    resumeData["qBt-ratioLimit"] = Utils::String::toStdString(QString::number(m_ratioLimit));
    resumeData["qBt-label"] = Utils::String::toStdString(m_label);
    resumeData["qBt-name"] = Utils::String::toStdString(m_name);
    resumeData["qBt-seedStatus"] = m_hasSeedStatus;
    resumeData["qBt-tempPathDisabled"] = m_tempPathDisabled;

    m_session->handleTorrentResumeDataReady(this, resumeData);
}

void TorrentHandle::handleSaveResumeDataFailedAlert(libtorrent::save_resume_data_failed_alert *p)
{
    // if torrent has no metadata we should save dummy fastresume data
    // containing Magnet URI and qBittorrent own resume data only
    if (p->error.value() == libt::errors::no_metadata)
        handleSaveResumeDataAlert(0);
    else
        m_session->handleTorrentResumeDataFailed(this);
}

void TorrentHandle::handleFastResumeRejectedAlert(libtorrent::fastresume_rejected_alert *p)
{
    qDebug("/!\\ Fast resume failed for %s, reason: %s", qPrintable(name()), p->message().c_str());
    Logger *const logger = Logger::instance();

    updateStatus();
    if (p->error.value() == libt::errors::mismatching_file_size) {
        // Mismatching file size (files were probably moved)
        logger->addMessage(tr("File sizes mismatch for torrent %1, pausing it.").arg(name()), Log::CRITICAL);
        m_hasMissingFiles = true;
        if (!isPaused())
            pause();
    }
    else {
        logger->addMessage(tr("Fast resume data was rejected for torrent %1. Reason: %2. Checking again...")
                           .arg(name()).arg(Utils::String::fromStdString(p->message())), Log::CRITICAL);
    }
}

void TorrentHandle::handleFileRenamedAlert(libtorrent::file_renamed_alert *p)
{
    QString newName = Utils::Fs::fromNativePath(Utils::String::fromStdString(p->name));

    // TODO: Check this!
    if (filesCount() > 1) {
        // Check if folders were renamed
        QStringList oldPathParts = m_torrentInfo.origFilePath(p->index).split("/");
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
        m_session->handleTorrentSavePathChanged(this);
    }
}

void TorrentHandle::handleFileCompletedAlert(libtorrent::file_completed_alert *p)
{
    updateStatus();

    qDebug("A file completed download in torrent \"%s\"", qPrintable(name()));
    if (m_session->isAppendExtensionEnabled()) {
        QString name = filePath(p->index);
        if (name.endsWith(QB_EXT)) {
            const QString oldName = name;
            name.chop(QString(QB_EXT).size());
            qDebug("Renaming %s to %s", qPrintable(oldName), qPrintable(name));
            renameFile(p->index, name);
        }
    }
}

void TorrentHandle::handleStatsAlert(libtorrent::stats_alert *p)
{
    Q_ASSERT(p->interval >= 1000);
    SpeedSample transferred(p->transferred[libt::stats_alert::download_payload] * 1000LL / p->interval,
            p->transferred[libt::stats_alert::upload_payload] * 1000LL / p->interval);
    m_speedMonitor.addSample(transferred);
}

void TorrentHandle::handleMetadataReceivedAlert(libt::metadata_received_alert *p)
{
    Q_UNUSED(p);
    qDebug("Metadata received for torrent %s.", qPrintable(name()));
    updateStatus();
    if (m_session->isAppendExtensionEnabled())
        appendExtensionsToIncompleteFiles();
    m_session->handleTorrentMetadataReceived(this);

    if (isPaused()) {
        // XXX: Unfortunately libtorrent-rasterbar does not send a torrent_paused_alert
        // and the torrent can be paused when metadata is received
        m_speedMonitor.reset();
        m_session->handleTorrentPaused(this);
    }
}

void TorrentHandle::handleDefaultSavePathChanged()
{
    adjustSavePath();
}

void TorrentHandle::adjustSavePath()
{
    // If we use default save path...
    if (m_useDefaultSavePath) {
        QString defaultSavePath = m_session->defaultSavePath();
        if (m_session->useAppendLabelToSavePath() && !m_label.isEmpty())
            defaultSavePath +=  QString("%1/").arg(m_label);
        defaultSavePath = Utils::Fs::toNativePath(defaultSavePath);
        if (m_savePath != defaultSavePath) {
            if (!useTempPath()) {
                moveStorage(defaultSavePath);
            }
            else {
                m_savePath = defaultSavePath;
                m_needSaveResumeData = true;
                m_session->handleTorrentSavePathChanged(this);
            }
        }
    }
}

void TorrentHandle::handleTempPathChanged()
{
    adjustActualSavePath();
}

void TorrentHandle::handleAppendExtensionToggled()
{
    if (!hasMetadata()) return;

    if (m_session->isAppendExtensionEnabled())
        appendExtensionsToIncompleteFiles();
    else
        removeExtensionsFromIncompleteFiles();
}

#ifndef DISABLE_COUNTRIES_RESOLUTION
void TorrentHandle::handleResolveCountriesToggled()
{
    resolveCountries(m_session->isResolveCountriesEnabled());
}
#endif

void TorrentHandle::handleAlert(libtorrent::alert *a)
{
    switch (a->type()) {
    case libt::stats_alert::alert_type:
        handleStatsAlert(static_cast<libt::stats_alert*>(a));
        break;
    case libt::file_renamed_alert::alert_type:
        handleFileRenamedAlert(static_cast<libt::file_renamed_alert*>(a));
        break;
    case libt::file_completed_alert::alert_type:
        handleFileCompletedAlert(static_cast<libt::file_completed_alert*>(a));
        break;
    case libt::torrent_finished_alert::alert_type:
        handleTorrentFinishedAlert(static_cast<libt::torrent_finished_alert*>(a));
        break;
    case libt::save_resume_data_alert::alert_type:
        handleSaveResumeDataAlert(static_cast<libt::save_resume_data_alert*>(a));
        break;
    case libt::save_resume_data_failed_alert::alert_type:
        handleSaveResumeDataFailedAlert(static_cast<libt::save_resume_data_failed_alert*>(a));
        break;
    case libt::storage_moved_alert::alert_type:
        handleStorageMovedAlert(static_cast<libt::storage_moved_alert*>(a));
        break;
    case libt::storage_moved_failed_alert::alert_type:
        handleStorageMovedFailedAlert(static_cast<libt::storage_moved_failed_alert*>(a));
        break;
    case libt::torrent_paused_alert::alert_type:
        handleTorrentPausedAlert(static_cast<libt::torrent_paused_alert*>(a));
        break;
    case libt::tracker_error_alert::alert_type:
        handleTrackerErrorAlert(static_cast<libt::tracker_error_alert*>(a));
        break;
    case libt::tracker_reply_alert::alert_type:
        handleTrackerReplyAlert(static_cast<libt::tracker_reply_alert*>(a));
        break;
    case libt::tracker_warning_alert::alert_type:
        handleTrackerWarningAlert(static_cast<libt::tracker_warning_alert*>(a));
        break;
    case libt::metadata_received_alert::alert_type:
        handleMetadataReceivedAlert(static_cast<libt::metadata_received_alert*>(a));
        break;
    case libt::fastresume_rejected_alert::alert_type:
        handleFastResumeRejectedAlert(static_cast<libt::fastresume_rejected_alert*>(a));
        break;
    case libt::torrent_checked_alert::alert_type:
        handleTorrentCheckedAlert(static_cast<libt::torrent_checked_alert*>(a));
        break;
    }
}

void TorrentHandle::appendExtensionsToIncompleteFiles()
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

void TorrentHandle::removeExtensionsFromIncompleteFiles()
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

void TorrentHandle::adjustActualSavePath()
{
    if (!isMoveInProgress())
        adjustActualSavePath_impl();
    else
        m_moveStorageTriggers.append(boost::bind(&TorrentHandle::adjustActualSavePath_impl, this));
}

void TorrentHandle::adjustActualSavePath_impl()
{
    QString path;
    if (!useTempPath()) {
        // Disabling temp dir
        // Moving all torrents to their destination folder
        path = savePath();
    }
    else {
        // Moving all downloading torrents to temporary save path
        path = m_session->tempPath();
        qDebug("Moving torrent to its temp save path: %s", qPrintable(path));
    }

    moveStorage(Utils::Fs::toNativePath(path));
}

libtorrent::torrent_handle TorrentHandle::nativeHandle() const
{
    return m_nativeHandle;
}

void TorrentHandle::updateTorrentInfo()
{
    if (!hasMetadata()) return;

#if LIBTORRENT_VERSION_NUM < 10000
    try {
        m_torrentInfo = TorrentInfo(&m_nativeHandle.get_torrent_info());
    }
    catch (std::exception &exc) {
        qDebug("torrent_handle::get_torrent_info() throws exception: %s", exc.what()); \
    }
#else
    m_torrentInfo = TorrentInfo(m_nativeStatus.torrent_file);
#endif
}

void TorrentHandle::initialize()
{
    updateStatus();

    m_hash = InfoHash(m_nativeStatus.info_hash);
    if (m_savePath.isEmpty()) {
        // we use default save path
        m_savePath = nativeActualSavePath();
        m_useDefaultSavePath = true;
    }

    adjustSavePath();
    adjustActualSavePath();
}

bool TorrentHandle::isMoveInProgress() const
{
    return !m_newPath.isEmpty();
}

bool TorrentHandle::useTempPath() const
{
    return !m_tempPathDisabled && m_session->isTempPathEnabled() && !isSeed();
}

void TorrentHandle::updateStatus()
{
    libt::torrent_status status;
    SAFE_GET(status, status);

    updateStatus(status);
}

void TorrentHandle::updateStatus(const libtorrent::torrent_status &nativeStatus)
{
    m_nativeStatus = nativeStatus;
#if LIBTORRENT_VERSION_NUM < 10000
    try {
        m_nativeName = Utils::String::fromStdString(m_nativeHandle.name());
        m_nativeSavePath = Utils::Fs::fromNativePath(Utils::String::fromStdString(m_nativeHandle.save_path()));
    }
    catch (std::exception &exc) {
        qDebug("torrent_handle method inside TorrentHandleImpl::updateStatus() throws exception: %s", exc.what());
    }
#endif

    updateState();
    updateTorrentInfo();
}

void TorrentHandle::setRatioLimit(qreal limit)
{
    if (limit < USE_GLOBAL_RATIO)
        limit = NO_RATIO_LIMIT;
    else if (limit > MAX_RATIO)
        limit = MAX_RATIO;

    if (m_ratioLimit != limit) {
        m_ratioLimit = limit;
        m_needSaveResumeData = true;
        m_session->handleTorrentRatioLimitChanged(this);
    }
}

void TorrentHandle::setUploadLimit(int limit)
{
    SAFE_CALL(set_upload_limit, limit)
}

void TorrentHandle::setDownloadLimit(int limit)
{
    SAFE_CALL(set_download_limit, limit)
}

void TorrentHandle::setSuperSeeding(bool enable)
{
    SAFE_CALL(super_seeding, enable)
}

void TorrentHandle::flushCache()
{
    SAFE_CALL(flush_cache)
}

QString TorrentHandle::toMagnetUri() const
{
    return Utils::String::fromStdString(libt::make_magnet_uri(m_nativeHandle));
}

void TorrentHandle::resolveCountries(bool b)
{
    SAFE_CALL(resolve_countries, b);
}

void TorrentHandle::prioritizeFiles(const QVector<int> &priorities)
{
    qDebug() << Q_FUNC_INFO;
    if (priorities.size() != filesCount()) return;

    qDebug() << Q_FUNC_INFO << "Changing files priorities...";
    SAFE_CALL(prioritize_files, priorities.toStdVector());

    qDebug() << Q_FUNC_INFO << "Moving unwanted files to .unwanted folder and conversely...";
    QString spath = actualSavePath();
    for (int i = 0; i < priorities.size(); ++i) {
        QString filepath = filePath(i);
        // Move unwanted files to a .unwanted subfolder
        if (priorities[i] == 0) {
            QString oldAbsPath = QDir(spath).absoluteFilePath(filepath);
            QString parentAbsPath = Utils::Fs::branchPath(oldAbsPath);
            // Make sure the file does not already exists
            if (QDir(parentAbsPath).dirName() != ".unwanted") {
                QString unwantedAbsPath = parentAbsPath + "/.unwanted";
                QString newAbsPath = unwantedAbsPath + "/" + Utils::Fs::fileName(filepath);
                qDebug() << "Unwanted path is" << unwantedAbsPath;
                if (QFile::exists(newAbsPath)) {
                    qWarning() << "File" << newAbsPath << "already exists at destination.";
                    continue;
                }

                bool created = QDir().mkpath(unwantedAbsPath);
                qDebug() << "unwanted folder was created:" << created;
#ifdef Q_OS_WIN
                if (created) {
                    // Hide the folder on Windows
                    qDebug() << "Hiding folder (Windows)";
                    std::wstring winPath =  Utils::Fs::toNativePath(unwantedAbsPath).toStdWString();
                    DWORD dwAttrs = ::GetFileAttributesW(winPath.c_str());
                    bool ret = ::SetFileAttributesW(winPath.c_str(), dwAttrs | FILE_ATTRIBUTE_HIDDEN);
                    Q_ASSERT(ret != 0); Q_UNUSED(ret);
                }
#endif
                QString parentPath = Utils::Fs::branchPath(filepath);
                if (!parentPath.isEmpty() && !parentPath.endsWith("/"))
                    parentPath += "/";
                renameFile(i, parentPath + ".unwanted/" + Utils::Fs::fileName(filepath));
            }
        }

        // Move wanted files back to their original folder
        if (priorities[i] > 0) {
            QString parentRelPath = Utils::Fs::branchPath(filepath);
            if (QDir(parentRelPath).dirName() == ".unwanted") {
                QString oldName = Utils::Fs::fileName(filepath);
                QString newRelPath = Utils::Fs::branchPath(parentRelPath);
                if (newRelPath.isEmpty())
                    renameFile(i, oldName);
                else
                    renameFile(i, QDir(newRelPath).filePath(oldName));

                // Remove .unwanted directory if empty
                qDebug() << "Attempting to remove .unwanted folder at " << QDir(spath + "/" + newRelPath).absoluteFilePath(".unwanted");
                QDir(spath + "/" + newRelPath).rmdir(".unwanted");
            }
        }
    }

    updateStatus();
}
