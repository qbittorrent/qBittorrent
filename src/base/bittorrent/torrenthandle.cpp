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

#include <type_traits>

#include <QDebug>
#include <QStringList>
#include <QFile>
#include <QDir>
#include <QByteArray>
#include <QBitArray>

#include <libtorrent/entry.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/address.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/create_torrent.hpp>
#include <libtorrent/magnet_uri.hpp>
#if LIBTORRENT_VERSION_NUM >= 10100
#include <libtorrent/time.hpp>
#endif

#include <boost/bind.hpp>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "base/logger.h"
#include "base/preferences.h"
#include "base/profile.h"
#include "base/utils/string.h"
#include "base/utils/fs.h"
#include "base/utils/misc.h"
#include "session.h"
#include "peerinfo.h"
#include "trackerentry.h"
#include "torrenthandle.h"

const QString QB_EXT {".!qB"};

namespace libt = libtorrent;
using namespace BitTorrent;

namespace
{
    using ListType = libt::entry::list_type;

    ListType setToEntryList(const QSet<QString> &input)
    {
        ListType entryList;
        foreach (const QString &setValue, input)
            entryList.emplace_back(setValue.toStdString());
        return entryList;
    }
}

// AddTorrentData

AddTorrentData::AddTorrentData()
    : resumed(false)
    , disableTempPath(false)
    , sequential(false)
    , firstLastPiecePriority(false)
    , hasSeedStatus(false)
    , skipChecking(false)
    , hasRootFolder(true)
    , addForced(false)
    , addPaused(false)
    , ratioLimit(TorrentHandle::USE_GLOBAL_RATIO)
    , seedingTimeLimit(TorrentHandle::USE_GLOBAL_SEEDING_TIME)
{
}

AddTorrentData::AddTorrentData(const AddTorrentParams &params)
    : resumed(false)
    , name(params.name)
    , category(params.category)
    , tags(params.tags)
    , savePath(params.savePath)
    , disableTempPath(params.disableTempPath)
    , sequential(params.sequential)
    , firstLastPiecePriority(params.firstLastPiecePriority)
    , hasSeedStatus(params.skipChecking) // do not react on 'torrent_finished_alert' when skipping
    , skipChecking(params.skipChecking)
    , hasRootFolder(params.createSubfolder == TriStateBool::Undefined
                    ? Session::instance()->isCreateTorrentSubfolder()
                    : params.createSubfolder == TriStateBool::True)
    , addForced(params.addForced == TriStateBool::True)
    , addPaused(params.addPaused == TriStateBool::Undefined
                ? Session::instance()->isAddTorrentPaused()
                : params.addPaused == TriStateBool::True)
    , filePriorities(params.filePriorities)
    , ratioLimit(params.ignoreShareLimits ? TorrentHandle::NO_RATIO_LIMIT : TorrentHandle::USE_GLOBAL_RATIO)
    , seedingTimeLimit(params.ignoreShareLimits ? TorrentHandle::NO_SEEDING_TIME_LIMIT : TorrentHandle::USE_GLOBAL_SEEDING_TIME)
{
    bool useAutoTMM = (params.useAutoTMM == TriStateBool::Undefined
                       ? !Session::instance()->isAutoTMMDisabledByDefault()
                       : params.useAutoTMM == TriStateBool::True);
    if (useAutoTMM)
        savePath = "";
    else if (savePath.trimmed().isEmpty())
        savePath = Session::instance()->defaultSavePath();
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
    case MissingFiles:
        return QLatin1String("missingFiles");
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
#if LIBTORRENT_VERSION_NUM < 10100
    case QueuedForChecking:
        return QLatin1String("queuedForChecking");
#endif
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

// TorrentHandle

const qreal TorrentHandle::USE_GLOBAL_RATIO = -2.;
const qreal TorrentHandle::NO_RATIO_LIMIT = -1.;

const int TorrentHandle::USE_GLOBAL_SEEDING_TIME = -2;
const int TorrentHandle::NO_SEEDING_TIME_LIMIT = -1;

const qreal TorrentHandle::MAX_RATIO = 9999.;
const int TorrentHandle::MAX_SEEDING_TIME = 525600;

// The new libtorrent::create_torrent constructor appeared after 1.0.11 in RC_1_0
// and after 1.1.1 in RC_1_1. Since it fixed an ABI incompatibility with previous versions
// distros might choose to backport it onto 1.0.11 and 1.1.1 respectively.
// So we need a way to detect its presence without relying solely on the LIBTORRENT_VERSION_NUM.
// Relevant links:
// 1. https://github.com/arvidn/libtorrent/issues/1696
// 2. https://github.com/qbittorrent/qBittorrent/issues/6406
// The following can be removed after one or two libtorrent releases on each branch.
namespace
{
    // new constructor is available
    template<typename T, typename std::enable_if<std::is_constructible<T, libt::torrent_info, bool>::value, int>::type = 0>
    T makeTorrentCreator(const libtorrent::torrent_info & ti)
    {
        return T(ti, true);
    }

    // new constructor isn't available
    template<typename T, typename std::enable_if<!std::is_constructible<T, libt::torrent_info, bool>::value, int>::type = 0>
    T makeTorrentCreator(const libtorrent::torrent_info & ti)
    {
        return T(ti);
    }
}

TorrentHandle::TorrentHandle(Session *session, const libtorrent::torrent_handle &nativeHandle,
                                     const AddTorrentData &data)
    : QObject(session)
    , m_session(session)
    , m_nativeHandle(nativeHandle)
    , m_state(TorrentState::Unknown)
    , m_renameCount(0)
    , m_useAutoTMM(data.savePath.isEmpty())
    , m_name(data.name)
    , m_savePath(Utils::Fs::toNativePath(data.savePath))
    , m_category(data.category)
    , m_tags(data.tags)
    , m_hasSeedStatus(data.hasSeedStatus)
    , m_ratioLimit(data.ratioLimit)
    , m_seedingTimeLimit(data.seedingTimeLimit)
    , m_tempPathDisabled(data.disableTempPath)
    , m_hasMissingFiles(false)
    , m_hasRootFolder(data.hasRootFolder)
    , m_needsToSetFirstLastPiecePriority(false)
    , m_pauseAfterRecheck(false)
    , m_needSaveResumeData(false)
{
    if (m_useAutoTMM)
        m_savePath = Utils::Fs::toNativePath(m_session->categorySavePath(m_category));

    updateStatus();
    m_hash = InfoHash(m_nativeStatus.info_hash);

    // NB: the following two if statements are present because we don't want
    // to set either sequential download or first/last piece priority to false
    // if their respective flags in data are false when a torrent is being
    // resumed. This is because, in that circumstance, this constructor is
    // called with those flags set to false, even if the torrent was set to
    // download sequentially or have first/last piece priority enabled when
    // its resume data was saved. These two settings are restored later. But
    // if we set them to false now, both will erroneously not be restored.
    if (!data.resumed || data.sequential)
        setSequentialDownload(data.sequential);
    if (!data.resumed || data.firstLastPiecePriority)
        setFirstLastPiecePriority(data.firstLastPiecePriority);

    if (!data.resumed && hasMetadata()) {
        if (filesCount() == 1)
            m_hasRootFolder = false;
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
    if (name.isEmpty())
        name = QString::fromStdString(m_nativeStatus.name);

    if (name.isEmpty() && hasMetadata())
        name = QString::fromStdString(m_torrentInfo.nativeInfo()->orig_files().name());

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
    return QString::fromStdString(m_nativeStatus.current_tracker);
}

QString TorrentHandle::savePath(bool actual) const
{
    if (actual)
        return Utils::Fs::fromNativePath(nativeActualSavePath());
    else
        return Utils::Fs::fromNativePath(m_savePath);
}

QString TorrentHandle::rootPath(bool actual) const
{
    if ((filesCount() > 1) && !hasRootFolder())
        return QString();

    QString firstFilePath = filePath(0);
    const int slashIndex = firstFilePath.indexOf("/");
    if (slashIndex >= 0)
        return QDir(savePath(actual)).absoluteFilePath(firstFilePath.left(slashIndex));
    else
        return QDir(savePath(actual)).absoluteFilePath(firstFilePath);
}

QString TorrentHandle::contentPath(bool actual) const
{
    if (filesCount() == 1)
        return QDir(savePath(actual)).absoluteFilePath(filePath(0));
    else if (hasRootFolder())
        return rootPath(actual);
    else
        return savePath(actual);
}

bool TorrentHandle::isAutoTMMEnabled() const
{
    return m_useAutoTMM;
}

void TorrentHandle::setAutoTMMEnabled(bool enabled)
{
    if (m_useAutoTMM == enabled) return;

    m_useAutoTMM = enabled;
    m_session->handleTorrentSavingModeChanged(this);

    if (m_useAutoTMM)
        move_impl(m_session->categorySavePath(m_category), true);
}

bool TorrentHandle::hasRootFolder() const
{
    return m_hasRootFolder;
}

QString TorrentHandle::nativeActualSavePath() const
{
    return QString::fromStdString(m_nativeStatus.save_path);
}

QList<TrackerEntry> TorrentHandle::trackers() const
{
    QList<TrackerEntry> entries;
    std::vector<libt::announce_entry> announces;

    announces = m_nativeHandle.trackers();
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

bool TorrentHandle::addTracker(const TrackerEntry &tracker)
{
    if (trackers().contains(tracker))
        return false;

    m_nativeHandle.add_tracker(tracker.nativeEntry());
    return true;
}

QList<QUrl> TorrentHandle::urlSeeds() const
{
    QList<QUrl> urlSeeds;
    std::set<std::string> seeds = m_nativeHandle.url_seeds();

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

    m_nativeHandle.add_url_seed(urlSeed.toString().toStdString());
    return true;
}

bool TorrentHandle::removeUrlSeed(const QUrl &urlSeed)
{
    QList<QUrl> seeds = urlSeeds();
    if (!seeds.contains(urlSeed)) return false;

    m_nativeHandle.remove_url_seed(urlSeed.toString().toStdString());
    return true;
}

bool TorrentHandle::connectPeer(const PeerAddress &peerAddress)
{
    libt::error_code ec;
    libt::address addr = libt::address::from_string(peerAddress.ip.toString().toStdString(), ec);
    if (ec) return false;

    boost::asio::ip::tcp::endpoint ep(addr, peerAddress.port);
    m_nativeHandle.connect_peer(ep);
    return true;
}

bool TorrentHandle::needSaveResumeData() const
{
    if (m_needSaveResumeData) return true;

    return m_nativeHandle.need_save_resume_data();
}

void TorrentHandle::saveResumeData(bool updateStatus)
{
    if (updateStatus) // to update queue_position, see discussion in PR #6154
        this->updateStatus();

    m_nativeHandle.save_resume_data();
    m_needSaveResumeData = false;
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

    float progress = static_cast<float>(m_nativeStatus.total_wanted_done) / m_nativeStatus.total_wanted;
    Q_ASSERT((progress >= 0.f) && (progress <= 1.f));
    return progress;
}

QString TorrentHandle::category() const
{
    return m_category;
}

bool TorrentHandle::belongsToCategory(const QString &category) const
{
    if (m_category.isEmpty()) return category.isEmpty();
    if (!Session::isValidCategoryName(category)) return false;

    if (m_category == category) return true;

    if (m_session->isSubcategoriesEnabled() && m_category.startsWith(category + "/"))
        return true;

    return false;
}

QSet<QString> TorrentHandle::tags() const
{
    return m_tags;
}

bool TorrentHandle::hasTag(const QString &tag) const
{
    return m_tags.contains(tag);
}

bool TorrentHandle::addTag(const QString &tag)
{
    if (!Session::isValidTag(tag))
        return false;

    if (!hasTag(tag)) {
        if (!m_session->hasTag(tag))
            if (!m_session->addTag(tag))
                return false;
        m_tags.insert(tag);
        m_session->handleTorrentTagAdded(this, tag);
        m_needSaveResumeData = true;
        return true;
    }
    return false;
}

bool TorrentHandle::removeTag(const QString &tag)
{
    if (m_tags.remove(tag)) {
        m_session->handleTorrentTagRemoved(this, tag);
        m_needSaveResumeData = true;
        return true;
    }
    return false;
}

void TorrentHandle::removeAllTags()
{
    // QT automatically copies the container in foreach, so it's safe to mutate it.
    foreach (const QString &tag, m_tags)
        removeTag(tag);
}

QDateTime TorrentHandle::addedTime() const
{
    return QDateTime::fromTime_t(m_nativeStatus.added_time);
}

qreal TorrentHandle::ratioLimit() const
{
    return m_ratioLimit;
}

int TorrentHandle::seedingTimeLimit() const
{
    return m_seedingTimeLimit;
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

    QDir saveDir(savePath(true));
    QStringList res;
    for (int i = 0; i < filesCount(); ++i)
        res << Utils::Fs::expandPathAbs(saveDir.absoluteFilePath(filePath(i)));
    return res;
}

QStringList TorrentHandle::absoluteFilePathsUnwanted() const
{
    if (!hasMetadata()) return  QStringList();

    QDir saveDir(savePath(true));
    QStringList res;
    std::vector<int> fp;
    fp = m_nativeHandle.file_priorities();

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

QVector<int> TorrentHandle::filePriorities() const
{
    std::vector<int> fp;
    fp = m_nativeHandle.file_priorities();

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
    return ((m_nativeStatus.state == libt::torrent_status::checking_files)
            || (m_nativeStatus.state == libt::torrent_status::checking_resume_data)
#if LIBTORRENT_VERSION_NUM < 10100
            || (m_nativeStatus.state == libt::torrent_status::queued_for_checking)
#endif
            );
}

bool TorrentHandle::isDownloading() const
{
    return m_state == TorrentState::Downloading
            || m_state == TorrentState::DownloadingMetadata
            || m_state == TorrentState::StalledDownloading
            || m_state == TorrentState::CheckingDownloading
            || m_state == TorrentState::PausedDownloading
            || m_state == TorrentState::QueuedDownloading
            || m_state == TorrentState::ForcedDownloading;
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
            || m_state == TorrentState::QueuedUploading
            || m_state == TorrentState::ForcedUploading;
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

bool TorrentHandle::isErrored() const
{
    return m_state == TorrentState::MissingFiles
            || m_state == TorrentState::Error;
}

bool TorrentHandle::isSeed() const
{
    // Affected by bug http://code.rasterbar.com/libtorrent/ticket/402
    //bool result;
    //result = m_nativeHandle.is_seed());
    //return result;
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
    if (!hasMetadata())
        return m_needsToSetFirstLastPiecePriority;

    // Get int first media file
    std::vector<int> fp;
    fp = m_nativeHandle.file_priorities();

    TorrentInfo::PieceRange extremities;
    bool found = false;
    int count = static_cast<int>(fp.size());
    for (int i = 0; i < count; ++i) {
        const QString ext = Utils::Fs::fileExtension(filePath(i));
        if (Utils::Misc::isPreviewable(ext) && (fp[i] > 0)) {
            extremities = info().filePieces(i);
            found = true;
            break;
        }
    }

    if (!found) return false; // No media file

    int first = m_nativeHandle.piece_priority(extremities.first());
    int last = m_nativeHandle.piece_priority(extremities.last());

    return ((first == 7) && (last == 7));
}

TorrentState TorrentHandle::state() const
{
    return m_state;
}

void TorrentHandle::updateState()
{
    if (isPaused()) {
        if (hasMissingFiles())
            m_state = TorrentState::MissingFiles;
        else if (hasError())
            m_state = TorrentState::Error;
        else
            m_state = isSeed() ? TorrentState::PausedUploading : TorrentState::PausedDownloading;
    }
    else {
        if (m_session->isQueueingSystemEnabled() && isQueued() && !isChecking()) {
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
#if LIBTORRENT_VERSION_NUM < 10100
            case libt::torrent_status::queued_for_checking:
                m_state = TorrentState::QueuedForChecking;
                break;
#endif
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
#if LIBTORRENT_VERSION_NUM < 10100
    return (m_nativeStatus.paused && !m_nativeStatus.error.empty());
#else
    return (m_nativeStatus.paused && m_nativeStatus.errc);
#endif
}

bool TorrentHandle::hasFilteredPieces() const
{
    std::vector<int> pp = m_nativeHandle.piece_priorities();

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
#if LIBTORRENT_VERSION_NUM < 10100
    return QString::fromStdString(m_nativeStatus.error);
#else
    return QString::fromStdString(m_nativeStatus.errc.message());
#endif
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

    const SpeedSampleAvg speedAverage = m_speedMonitor.average();

    if (isSeed()) {
        qreal maxRatioValue = maxRatio();
        int maxSeedingTimeValue = maxSeedingTime();
        if ((maxRatioValue < 0) && (maxSeedingTimeValue < 0)) return MAX_ETA;

        qlonglong ratioEta = MAX_ETA;

        if ((speedAverage.upload > 0) && (maxRatioValue >= 0)) {

            qlonglong realDL = totalDownload();
            if (realDL <= 0)
                realDL = wantedSize();

            ratioEta = ((realDL * maxRatioValue) - totalUpload()) / speedAverage.upload;
        }

        qlonglong seedingTimeEta = MAX_ETA;

        if (maxSeedingTimeValue >= 0) {
            seedingTimeEta = (maxSeedingTimeValue * 60) - seedingTime();
            if (seedingTimeEta < 0)
                seedingTimeEta = 0;
        }

        return qMin(ratioEta, seedingTimeEta);
    }

    if (!speedAverage.download) return MAX_ETA;

    return (wantedSize() - completedSize()) / speedAverage.download;
}

QVector<qreal> TorrentHandle::filesProgress() const
{
    std::vector<boost::int64_t> fp;
    QVector<qreal> result;
    m_nativeHandle.file_progress(fp, libt::torrent_handle::piece_granularity);

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
    return (m_nativeStatus.num_complete > 0) ? m_nativeStatus.num_complete : m_nativeStatus.list_seeds;
}

int TorrentHandle::totalPeersCount() const
{
    int peers = m_nativeStatus.num_complete + m_nativeStatus.num_incomplete;
    return (peers > 0) ? peers : m_nativeStatus.list_peers;
}

int TorrentHandle::totalLeechersCount() const
{
    return (m_nativeStatus.num_incomplete > 0) ? m_nativeStatus.num_incomplete : (m_nativeStatus.list_peers - m_nativeStatus.list_seeds);
}

int TorrentHandle::completeCount() const
{
    // additional info: https://github.com/qbittorrent/qBittorrent/pull/5300#issuecomment-267783646
    return m_nativeStatus.num_complete;
}

int TorrentHandle::incompleteCount() const
{
    // additional info: https://github.com/qbittorrent/qBittorrent/pull/5300#issuecomment-267783646
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
    return m_nativeHandle.download_limit();
}

int TorrentHandle::uploadLimit() const
{
    return m_nativeHandle.upload_limit();
}

bool TorrentHandle::superSeeding() const
{
    return m_nativeStatus.super_seeding;
}

QList<PeerInfo> TorrentHandle::peers() const
{
    QList<PeerInfo> peers;
    std::vector<libt::peer_info> nativePeers;

    m_nativeHandle.get_peer_info(nativePeers);

    foreach (const libt::peer_info &peer, nativePeers)
        peers << PeerInfo(this, peer);

    return peers;
}

QBitArray TorrentHandle::pieces() const
{
    QBitArray result(m_nativeStatus.pieces.size());

    for (int i = 0; i < m_nativeStatus.pieces.size(); ++i)
        result.setBit(i, m_nativeStatus.pieces.get_bit(i));

    return result;
}

QBitArray TorrentHandle::downloadingPieces() const
{
    QBitArray result(piecesCount());

    std::vector<libt::partial_piece_info> queue;
    m_nativeHandle.get_download_queue(queue);

    std::vector<libt::partial_piece_info>::const_iterator it = queue.begin();
    std::vector<libt::partial_piece_info>::const_iterator itend = queue.end();
    for (; it != itend; ++it)
        result.setBit(it->piece_index);

    return result;
}

QVector<int> TorrentHandle::pieceAvailability() const
{
    std::vector<int> avail;
    m_nativeHandle.piece_availability(avail);

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

int TorrentHandle::maxSeedingTime(bool *usesGlobalSeedingTime) const
{
    int seedingTimeLimit = m_seedingTimeLimit;

    if (seedingTimeLimit == USE_GLOBAL_SEEDING_TIME) {
        seedingTimeLimit = m_session->globalMaxSeedingMinutes();
        if (usesGlobalSeedingTime)
            *usesGlobalSeedingTime = true;
    }
    else {
        if (usesGlobalSeedingTime)
            *usesGlobalSeedingTime = false;
    }

    return seedingTimeLimit;
}

qreal TorrentHandle::realRatio() const
{
    boost::int64_t upload = m_nativeStatus.all_time_upload;
    // special case for a seeder who lost its stats, also assume nobody will import a 99% done torrent
    boost::int64_t download = (m_nativeStatus.all_time_download < m_nativeStatus.total_done * 0.01) ? m_nativeStatus.total_done : m_nativeStatus.all_time_download;

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

qlonglong TorrentHandle::totalPayloadUpload() const
{
    return m_nativeStatus.total_payload_upload;
}

qlonglong TorrentHandle::totalPayloadDownload() const
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
#if LIBTORRENT_VERSION_NUM < 10100
    return m_nativeStatus.next_announce.total_seconds();
#else
    return libt::duration_cast<libt::seconds>(m_nativeStatus.next_announce).count();
#endif
}

void TorrentHandle::setName(const QString &name)
{
    if (m_name != name) {
        m_name = name;
        m_needSaveResumeData = true;
    }
}

bool TorrentHandle::setCategory(const QString &category)
{
    if (m_category != category) {
        if (!category.isEmpty()) {
            if (!Session::isValidCategoryName(category)) return false;
            if (!m_session->categories().contains(category))
                if (!m_session->addCategory(category))
                    return false;
        }

        QString oldCategory = m_category;
        m_category = category;
        m_needSaveResumeData = true;
        m_session->handleTorrentCategoryChanged(this, oldCategory);

        if (m_useAutoTMM) {
            if (!m_session->isDisableAutoTMMWhenCategoryChanged())
                move_impl(m_session->categorySavePath(m_category), true);
            else
                setAutoTMMEnabled(false);
        }
    }

    return true;
}

void TorrentHandle::move(QString path)
{
    m_useAutoTMM = false;
    m_session->handleTorrentSavingModeChanged(this);

    path = Utils::Fs::fromNativePath(path.trimmed());
    if (path.isEmpty())
        path = m_session->defaultSavePath();
    if (!path.endsWith('/'))
        path += '/';

    move_impl(path, false);
}

void TorrentHandle::move_impl(QString path, bool overwrite)
{
    if (path == savePath()) return;
    path = Utils::Fs::toNativePath(path);

    if (!useTempPath()) {
        moveStorage(path, overwrite);
    }
    else {
        m_savePath = path;
        m_needSaveResumeData = true;
        m_session->handleTorrentSavePathChanged(this);
    }
}

void TorrentHandle::forceReannounce(int index)
{
    m_nativeHandle.force_reannounce(0, index);
}

void TorrentHandle::forceDHTAnnounce()
{
    m_nativeHandle.force_dht_announce();
}

void TorrentHandle::forceRecheck()
{
    if (!hasMetadata()) return;

    if (isPaused()) {
        m_pauseAfterRecheck = true;
        resume();
    }

    m_nativeHandle.force_recheck();
}

void TorrentHandle::setSequentialDownload(bool b)
{
    if (b != isSequentialDownload()) {
        m_nativeHandle.set_sequential_download(b);
        m_nativeStatus.sequential_download = b; // prevent return cached value
    }
}

void TorrentHandle::toggleSequentialDownload()
{
    setSequentialDownload(!isSequentialDownload());
}

void TorrentHandle::setFirstLastPiecePriority(bool b)
{
    if (!hasMetadata()) {
        m_needsToSetFirstLastPiecePriority = b;
        return;
    }

    std::vector<int> fp = m_nativeHandle.file_priorities();
    std::vector<int> pp = m_nativeHandle.piece_priorities();

    // Download first and last pieces first for all media files in the torrent
    int nbfiles = static_cast<int>(fp.size());
    for (int index = 0; index < nbfiles; ++index) {
        const QString path = filePath(index);
        const QString ext = Utils::Fs::fileExtension(path);
        if (Utils::Misc::isPreviewable(ext) && (fp[index] > 0)) {
            qDebug() << "File" << path << "is previewable, toggle downloading of first/last pieces first";

            // Determine the priority to set
            int prio = b ? 7 : fp[index];

            TorrentInfo::PieceRange extremities = info().filePieces(index);

            // worst case: AVI index = 1% of total file size (at the end of the file)
            int nNumPieces = ceil(fileSize(index) * 0.01 / pieceLength());
            for (int i = 0; i < nNumPieces; ++i) {
                pp[extremities.first() + i] = prio;
                pp[extremities.last() - i] = prio;
            }
        }
    }

    m_nativeHandle.prioritize_pieces(pp);
}

void TorrentHandle::toggleFirstLastPiecePriority()
{
    setFirstLastPiecePriority(!hasFirstLastPiecePriority());
}

void TorrentHandle::pause()
{
    if (isPaused()) return;

    m_nativeHandle.auto_managed(false);
    m_nativeHandle.pause();
}

void TorrentHandle::resume(bool forced)
{
    if (hasError())
        m_nativeHandle.clear_error();
    m_hasMissingFiles = false;
    m_nativeHandle.set_upload_mode(false);
    m_nativeHandle.auto_managed(!forced);
    m_nativeHandle.resume();
}

void TorrentHandle::moveStorage(const QString &newPath, bool overwrite)
{
    if (isMoveInProgress()) {
        qDebug("enqueue move storage to %s", qUtf8Printable(newPath));
        m_moveStorageInfo.queuedPath = newPath;
        m_moveStorageInfo.queuedOverwrite = overwrite;
    }
    else {
        const QString oldPath = nativeActualSavePath();
        if (QDir(oldPath) == QDir(newPath)) return;

        qDebug("move storage: %s to %s", qUtf8Printable(oldPath), qUtf8Printable(newPath));
        // Actually move the storage
        m_nativeHandle.move_storage(newPath.toUtf8().constData()
                                    , (overwrite ? libt::always_replace_files : libt::dont_replace));
        m_moveStorageInfo.oldPath = oldPath;
        m_moveStorageInfo.newPath = newPath;
    }
}

#if LIBTORRENT_VERSION_NUM < 10100
void TorrentHandle::setTrackerLogin(const QString &username, const QString &password)
{
    m_nativeHandle.set_tracker_login(std::string(username.toLocal8Bit().constData())
                                     , std::string(password.toLocal8Bit().constData()));
}
#endif

void TorrentHandle::renameFile(int index, const QString &name)
{
    ++m_renameCount;
    qDebug() << Q_FUNC_INFO << index << name;
    m_nativeHandle.rename_file(index, Utils::Fs::toNativePath(name).toStdString());
}

bool TorrentHandle::saveTorrentFile(const QString &path)
{
    if (!m_torrentInfo.isValid()) return false;

    libt::create_torrent torrentCreator = makeTorrentCreator<libt::create_torrent>(*(m_torrentInfo.nativeInfo()));
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
    std::vector<int> priorities = m_nativeHandle.file_priorities();

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
        qWarning() << "Unexpected " << Q_FUNC_INFO << " call.";
        return;
    }

#if LIBTORRENT_VERSION_NUM < 10100
    const QString newPath = QString::fromStdString(p->path);
#else
    const QString newPath(p->storage_path());
#endif
    if (newPath != m_moveStorageInfo.newPath) {
        qWarning() << Q_FUNC_INFO << ": New path doesn't match a path in a queue.";
        return;
    }

    qDebug("Torrent is successfully moved from %s to %s"
        , qUtf8Printable(m_moveStorageInfo.oldPath), qUtf8Printable(m_moveStorageInfo.newPath));
    const QDir oldDir {m_moveStorageInfo.oldPath};
    if ((oldDir == QDir(m_session->torrentTempPath(info())))
            && (oldDir != QDir(m_session->tempPath()))) {
        // torrent without root folder still has it in its temporary save path
        // so its temp path isn't equal to temp path root
        qDebug() << "Removing torrent temp folder:" << m_moveStorageInfo.oldPath;
        Utils::Fs::smartRemoveEmptyFolderTree(m_moveStorageInfo.oldPath);
    }
    updateStatus();

    m_moveStorageInfo.newPath.clear();
    if (!m_moveStorageInfo.queuedPath.isEmpty()) {
        moveStorage(m_moveStorageInfo.queuedPath, m_moveStorageInfo.queuedOverwrite);
        m_moveStorageInfo.queuedPath.clear();
    }

    if (!useTempPath()) {
        m_savePath = newPath;
        m_session->handleTorrentSavePathChanged(this);
    }

    while (!isMoveInProgress() && (m_renameCount == 0) && !m_moveFinishedTriggers.isEmpty())
        m_moveFinishedTriggers.takeFirst()();
}

void TorrentHandle::handleStorageMovedFailedAlert(libtorrent::storage_moved_failed_alert *p)
{
    if (!isMoveInProgress()) {
        qWarning() << "Unexpected " << Q_FUNC_INFO << " call.";
        return;
    }

    Logger::instance()->addMessage(tr("Could not move torrent: '%1'. Reason: %2")
                                   .arg(name()).arg(QString::fromStdString(p->message())), Log::CRITICAL);

    m_moveStorageInfo.newPath.clear();
    if (!m_moveStorageInfo.queuedPath.isEmpty()) {
        moveStorage(m_moveStorageInfo.queuedPath, m_moveStorageInfo.queuedOverwrite);
        m_moveStorageInfo.queuedPath.clear();
    }

    while (!isMoveInProgress() && (m_renameCount == 0) && !m_moveFinishedTriggers.isEmpty())
        m_moveFinishedTriggers.takeFirst()();
}

void TorrentHandle::handleTrackerReplyAlert(libtorrent::tracker_reply_alert *p)
{
#if LIBTORRENT_VERSION_NUM < 10100
    QString trackerUrl = QString::fromStdString(p->url);
#else
    QString trackerUrl(p->tracker_url());
#endif
    qDebug("Received a tracker reply from %s (Num_peers = %d)", qUtf8Printable(trackerUrl), p->num_peers);
    // Connection was successful now. Remove possible old errors
    m_trackerInfos[trackerUrl].lastMessage.clear(); // Reset error/warning message
    m_trackerInfos[trackerUrl].numPeers = p->num_peers;

    m_session->handleTorrentTrackerReply(this, trackerUrl);
}

void TorrentHandle::handleTrackerWarningAlert(libtorrent::tracker_warning_alert *p)
{
#if LIBTORRENT_VERSION_NUM < 10100
    QString trackerUrl = QString::fromStdString(p->url);
    QString message = QString::fromStdString(p->msg);
#else
    QString trackerUrl(p->tracker_url());
    QString message = QString::fromStdString(p->message());
#endif
    qDebug("Received a tracker warning for %s: %s", qUtf8Printable(trackerUrl), qUtf8Printable(message));
    // Connection was successful now but there is a warning message
    m_trackerInfos[trackerUrl].lastMessage = message; // Store warning message

    m_session->handleTorrentTrackerWarning(this, trackerUrl);
}

void TorrentHandle::handleTrackerErrorAlert(libtorrent::tracker_error_alert *p)
{
#if LIBTORRENT_VERSION_NUM < 10100
    QString trackerUrl = QString::fromStdString(p->url);
    QString message = QString::fromStdString(p->msg);
#else
    QString trackerUrl(p->tracker_url());
    QString message = QString::fromStdString(p->message());
#endif
    qDebug("Received a tracker error for %s: %s", qUtf8Printable(trackerUrl), qUtf8Printable(message));
    m_trackerInfos[trackerUrl].lastMessage = message;

    if (p->status_code == 401)
        m_session->handleTorrentTrackerAuthenticationRequired(this, trackerUrl);

    m_session->handleTorrentTrackerError(this, trackerUrl);
}

void TorrentHandle::handleTorrentCheckedAlert(libtorrent::torrent_checked_alert *p)
{
    Q_UNUSED(p);
    qDebug("%s have just finished checking", qUtf8Printable(hash()));

    updateStatus();

    if ((progress() < 1.0) && (wantedSize() > 0))
        m_hasSeedStatus = false;
    else if (progress() == 1.0)
        m_hasSeedStatus = true;

    adjustActualSavePath();
    manageIncompleteFiles();

    if (m_pauseAfterRecheck) {
        m_pauseAfterRecheck = false;
        pause();
    }

    m_session->handleTorrentChecked(this);
}

void TorrentHandle::handleTorrentFinishedAlert(libtorrent::torrent_finished_alert *p)
{
    Q_UNUSED(p);
    qDebug("Got a torrent finished alert for %s", qUtf8Printable(name()));
    qDebug("Torrent has seed status: %s", m_hasSeedStatus ? "yes" : "no");
    if (m_hasSeedStatus) return;

    updateStatus();
    m_hasMissingFiles = false;
    m_hasSeedStatus = true;

    adjustActualSavePath();
    manageIncompleteFiles();

    const bool recheckTorrentsOnCompletion = Preferences::instance()->recheckTorrentsOnCompletion();
    if (isMoveInProgress() || m_renameCount > 0) {
        if (recheckTorrentsOnCompletion)
            m_moveFinishedTriggers.append(boost::bind(&TorrentHandle::forceRecheck, this));
        m_moveFinishedTriggers.append(boost::bind(&Session::handleTorrentFinished, m_session, this));
    }
    else {
        if (recheckTorrentsOnCompletion)
            forceRecheck();
        m_session->handleTorrentFinished(this);
    }
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
        resumeData["qBt-magnetUri"] = toMagnetUri().toStdString();
        resumeData["qBt-paused"] = isPaused();
        resumeData["qBt-forced"] = isForced();
        // Both firstLastPiecePriority and sequential need to be stored in the
        // resume data if there is no metadata, otherwise they won't be
        // restored if qBittorrent quits before the metadata are retrieved:
        resumeData["qBt-firstLastPiecePriority"] = hasFirstLastPiecePriority();
        resumeData["qBt-sequential"] = isSequentialDownload();
    }
    else {
        auto savePath = resumeData.find_key("save_path")->string();
        resumeData["save_path"] = Profile::instance().toPortablePath(QString::fromStdString(savePath)).toStdString();
    }
    resumeData["qBt-savePath"] = m_useAutoTMM ? "" : Profile::instance().toPortablePath(m_savePath).toStdString();
    resumeData["qBt-ratioLimit"] = QString::number(m_ratioLimit).toStdString();
    resumeData["qBt-seedingTimeLimit"] = QString::number(m_seedingTimeLimit).toStdString();
    resumeData["qBt-category"] = m_category.toStdString();
    resumeData["qBt-tags"] = setToEntryList(m_tags);
    resumeData["qBt-name"] = m_name.toStdString();
    resumeData["qBt-seedStatus"] = m_hasSeedStatus;
    resumeData["qBt-tempPathDisabled"] = m_tempPathDisabled;
    resumeData["qBt-queuePosition"] = queuePosition();
    resumeData["qBt-hasRootFolder"] = m_hasRootFolder;

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
    qDebug("/!\\ Fast resume failed for %s, reason: %s", qUtf8Printable(name()), p->message().c_str());
    Logger *const logger = Logger::instance();

    updateStatus();
    if (p->error.value() == libt::errors::mismatching_file_size) {
        // Mismatching file size (files were probably moved)
        logger->addMessage(tr("File sizes mismatch for torrent '%1', pausing it.").arg(name()), Log::CRITICAL);
        m_hasMissingFiles = true;
        if (!isPaused())
            pause();
    }
    else {
        logger->addMessage(tr("Fast resume data was rejected for torrent '%1'. Reason: %2. Checking again...")
                           .arg(name()).arg(QString::fromStdString(p->message())), Log::CRITICAL);
    }
}

void TorrentHandle::handleFileRenamedAlert(libtorrent::file_renamed_alert *p)
{
#if LIBTORRENT_VERSION_NUM < 10100
    QString newName = Utils::Fs::fromNativePath(QString::fromStdString(p->name));
#else
    QString newName = Utils::Fs::fromNativePath(p->new_name());
#endif

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
            qDebug("oldPath(%s) != newPath(%s)", qUtf8Printable(oldPath), qUtf8Printable(newPath));
            oldPath = QString("%1/%2").arg(savePath(true)).arg(oldPath);
            qDebug("Detected folder renaming, attempt to delete old folder: %s", qUtf8Printable(oldPath));
            QDir().rmpath(oldPath);
        }
    }

    updateStatus();

    --m_renameCount;
    while (!isMoveInProgress() && (m_renameCount == 0) && !m_moveFinishedTriggers.isEmpty())
        m_moveFinishedTriggers.takeFirst()();
}

void TorrentHandle::handleFileRenameFailedAlert(libtorrent::file_rename_failed_alert *p)
{
    Q_UNUSED(p);

    --m_renameCount;
    while (!isMoveInProgress() && (m_renameCount == 0) && !m_moveFinishedTriggers.isEmpty())
        m_moveFinishedTriggers.takeFirst()();
}

void TorrentHandle::handleFileCompletedAlert(libtorrent::file_completed_alert *p)
{
    updateStatus();

    qDebug("A file completed download in torrent \"%s\"", qUtf8Printable(name()));
    if (m_session->isAppendExtensionEnabled()) {
        QString name = filePath(p->index);
        if (name.endsWith(QB_EXT)) {
            const QString oldName = name;
            name.chop(QB_EXT.size());
            qDebug("Renaming %s to %s", qUtf8Printable(oldName), qUtf8Printable(name));
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
    qDebug("Metadata received for torrent %s.", qUtf8Printable(name()));
    updateStatus();
    if (m_session->isAppendExtensionEnabled())
        manageIncompleteFiles();
    if (!m_hasRootFolder)
        m_torrentInfo.stripRootFolder();
    if (filesCount() == 1)
        m_hasRootFolder = false;
    m_session->handleTorrentMetadataReceived(this);

    if (isPaused()) {
        // XXX: Unfortunately libtorrent-rasterbar does not send a torrent_paused_alert
        // and the torrent can be paused when metadata is received
        m_speedMonitor.reset();
        m_session->handleTorrentPaused(this);
    }

    // If first/last piece priority was specified when adding this torrent, we can set it
    // now that we have metadata:
    if (m_needsToSetFirstLastPiecePriority) {
        setFirstLastPiecePriority(true);
        m_needsToSetFirstLastPiecePriority = false;
    }
}

void TorrentHandle::handleTempPathChanged()
{
    adjustActualSavePath();
}

void TorrentHandle::handleCategorySavePathChanged()
{
    if (m_useAutoTMM)
        move_impl(m_session->categorySavePath(m_category), true);
}

void TorrentHandle::handleAppendExtensionToggled()
{
    if (!hasMetadata()) return;

    manageIncompleteFiles();
}

void TorrentHandle::handleAlert(libtorrent::alert *a)
{
    switch (a->type()) {
    case libt::stats_alert::alert_type:
        handleStatsAlert(static_cast<libt::stats_alert*>(a));
        break;
    case libt::file_renamed_alert::alert_type:
        handleFileRenamedAlert(static_cast<libt::file_renamed_alert*>(a));
        break;
    case libt::file_rename_failed_alert::alert_type:
        handleFileRenameFailedAlert(static_cast<libt::file_rename_failed_alert*>(a));
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

void TorrentHandle::manageIncompleteFiles()
{
    const bool isAppendExtensionEnabled = m_session->isAppendExtensionEnabled();
    QVector<qreal> fp = filesProgress();
    if( fp.size() != filesCount() ) {
        qDebug() << "skip manageIncompleteFiles because of invalid torrent meta-data or empty file-progress";
        return;
    }

    for (int i = 0; i < filesCount(); ++i) {
        QString name = filePath(i);
        if (isAppendExtensionEnabled && (fileSize(i) > 0) && (fp[i] < 1)) {
            if (!name.endsWith(QB_EXT)) {
                const QString newName = name + QB_EXT;
                qDebug() << "Renaming" << name << "to" << newName;
                renameFile(i, newName);
            }
        }
        else {
            if (name.endsWith(QB_EXT)) {
                const QString oldName = name;
                name.chop(QB_EXT.size());
                qDebug() << "Renaming" << oldName << "to" << name;
                renameFile(i, name);
            }
        }
    }
}

void TorrentHandle::adjustActualSavePath()
{
    if (!isMoveInProgress())
        adjustActualSavePath_impl();
    else
        m_moveFinishedTriggers.append(boost::bind(&TorrentHandle::adjustActualSavePath_impl, this));
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
        // Moving all downloading torrents to temporary folder
        path = m_session->torrentTempPath(info());
        qDebug() << "Moving torrent to its temporary folder:" << path;
    }

    moveStorage(Utils::Fs::toNativePath(path), true);
}

libtorrent::torrent_handle TorrentHandle::nativeHandle() const
{
    return m_nativeHandle;
}

void TorrentHandle::updateTorrentInfo()
{
    if (!hasMetadata()) return;
#if LIBTORRENT_VERSION_NUM < 10100
    m_torrentInfo = TorrentInfo(m_nativeStatus.torrent_file);
#else
    m_torrentInfo = TorrentInfo(m_nativeStatus.torrent_file.lock());
#endif
}

bool TorrentHandle::isMoveInProgress() const
{
    return !m_moveStorageInfo.newPath.isEmpty();
}

bool TorrentHandle::useTempPath() const
{
    return !m_tempPathDisabled && m_session->isTempPathEnabled() && !(isSeed() || m_hasSeedStatus);
}

void TorrentHandle::updateStatus()
{
    updateStatus(m_nativeHandle.status());
}

void TorrentHandle::updateStatus(const libtorrent::torrent_status &nativeStatus)
{
    m_nativeStatus = nativeStatus;

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
        m_session->handleTorrentShareLimitChanged(this);
    }
}

void TorrentHandle::setSeedingTimeLimit(int limit)
{
    if (limit < USE_GLOBAL_SEEDING_TIME)
        limit = NO_SEEDING_TIME_LIMIT;
    else if (limit > MAX_SEEDING_TIME)
        limit = MAX_SEEDING_TIME;

    if (m_seedingTimeLimit != limit) {
        m_seedingTimeLimit = limit;
        m_needSaveResumeData = true;
        m_session->handleTorrentShareLimitChanged(this);
    }
}

void TorrentHandle::setUploadLimit(int limit)
{
    m_nativeHandle.set_upload_limit(limit);
}

void TorrentHandle::setDownloadLimit(int limit)
{
    m_nativeHandle.set_download_limit(limit);
}

void TorrentHandle::setSuperSeeding(bool enable)
{
    m_nativeHandle.super_seeding(enable);
    if (superSeeding() != enable)
        updateStatus();
}

void TorrentHandle::flushCache()
{
    m_nativeHandle.flush_cache();
}

QString TorrentHandle::toMagnetUri() const
{
    return QString::fromStdString(libt::make_magnet_uri(m_nativeHandle));
}

void TorrentHandle::prioritizeFiles(const QVector<int> &priorities)
{
    if (!hasMetadata()) return;
    if (priorities.size() != filesCount()) return;

    // Save first/last piece first option state
    bool firstLastPieceFirst = hasFirstLastPiecePriority();

    // Reset 'm_hasSeedStatus' if needed in order to react again to
    // 'torrent_finished_alert' and eg show tray notifications
    QVector<qreal> progress = filesProgress();
    QVector<int> oldPriorities = filePriorities();
    for (int i = 0; i < oldPriorities.size(); ++i) {
        if ((oldPriorities[i] == 0) && (priorities[i] > 0) && (progress[i] < 1.0)) {
            m_hasSeedStatus = false;
            break;
        }
    }

    qDebug() << Q_FUNC_INFO << "Changing files priorities...";
    m_nativeHandle.prioritize_files(priorities.toStdVector());

    qDebug() << Q_FUNC_INFO << "Moving unwanted files to .unwanted folder and conversely...";
    QString spath = savePath(true);
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

    // Restore first/last piece first option if necessary
    if (firstLastPieceFirst)
        setFirstLastPiecePriority(true);

    updateStatus();
}

QVector<qreal> TorrentHandle::availableFileFractions() const
{
    const auto filesCount = this->filesCount();
    if (filesCount < 0) return {};

    const QVector<int> piecesAvailability = pieceAvailability();
    // libtorrent returns empty array for seeding only torrents
    if (piecesAvailability.empty()) return QVector<qreal>(filesCount, -1.);

    QVector<qreal> res;
    res.reserve(filesCount);
    TorrentInfo info = this->info();
    for (int file = 0; file < filesCount; ++file) {
        TorrentInfo::PieceRange filePieces = info.filePieces(file);
        int availablePieces = 0;
        for (int piece = filePieces.first(); piece <= filePieces.last(); ++piece) {
            availablePieces += piecesAvailability[piece] > 0 ? 1 : 0;
        }
        res.push_back(static_cast<qreal>(availablePieces) / filePieces.size());
    }
    return res;
}
