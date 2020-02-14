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

#include "torrenthandle.h"

#include <algorithm>
#include <type_traits>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include <libtorrent/address.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/entry.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/time.hpp>
#include <libtorrent/version.hpp>

#if (LIBTORRENT_VERSION_NUM >= 10200)
#include <libtorrent/storage_defs.hpp>
#include <libtorrent/write_resume_data.hpp>
#else
#include <libtorrent/storage.hpp>
#endif

#include <QBitArray>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QStringList>
#include <QUrl>

#include "base/global.h"
#include "base/logger.h"
#include "base/preferences.h"
#include "base/profile.h"
#include "base/tristatebool.h"
#include "base/utils/fs.h"
#include "base/utils/string.h"
#include "downloadpriority.h"
#include "peeraddress.h"
#include "peerinfo.h"
#include "private/ltunderlyingtype.h"
#include "session.h"
#include "trackerentry.h"

const QString QB_EXT {QStringLiteral(".!qB")};

using namespace BitTorrent;

#if (LIBTORRENT_VERSION_NUM >= 10200)
namespace libtorrent
{
    namespace aux
    {
        template <typename T, typename Tag>
        uint qHash(const strong_typedef<T, Tag> &key, const uint seed)
        {
            return ::qHash((std::hash<strong_typedef<T, Tag>> {})(key), seed);
        }
    }
}
#endif

namespace
{
#if (LIBTORRENT_VERSION_NUM < 10200)
    using LTDownloadPriority = int;
    using LTPieceIndex = int;
    using LTQueuePosition = int;
#else
    using LTDownloadPriority = lt::download_priority_t;
    using LTPieceIndex = lt::piece_index_t;
    using LTQueuePosition = lt::queue_position_t;
#endif

    std::vector<LTDownloadPriority> toLTDownloadPriorities(const QVector<DownloadPriority> &priorities)
    {
        std::vector<LTDownloadPriority> out;
        out.reserve(priorities.size());

        std::transform(priorities.cbegin(), priorities.cend()
                       , std::back_inserter(out), [](BitTorrent::DownloadPriority priority)
        {
            return static_cast<LTDownloadPriority>(
                        static_cast<LTUnderlyingType<LTDownloadPriority>>(priority));
        });
        return out;
    }

    using ListType = lt::entry::list_type;

    ListType setToEntryList(const QSet<QString> &input)
    {
        ListType entryList;
        for (const QString &setValue : input)
            entryList.emplace_back(setValue.toStdString());
        return entryList;
    }
}

// AddTorrentData

CreateTorrentParams::CreateTorrentParams()
    : restored(false)
    , disableTempPath(false)
    , sequential(false)
    , firstLastPiecePriority(false)
    , hasSeedStatus(false)
    , skipChecking(false)
    , hasRootFolder(true)
    , forced(false)
    , paused(false)
    , uploadLimit(-1)
    , downloadLimit(-1)
    , ratioLimit(TorrentHandle::USE_GLOBAL_RATIO)
    , seedingTimeLimit(TorrentHandle::USE_GLOBAL_SEEDING_TIME)
{
}

CreateTorrentParams::CreateTorrentParams(const AddTorrentParams &params)
    : restored(false)
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
    , forced(params.addForced == TriStateBool::True)
    , paused(params.addPaused == TriStateBool::Undefined
                ? Session::instance()->isAddTorrentPaused()
                : params.addPaused == TriStateBool::True)
    , uploadLimit(params.uploadLimit)
    , downloadLimit(params.downloadLimit)
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

uint BitTorrent::qHash(const BitTorrent::TorrentState key, const uint seed)
{
    return ::qHash(static_cast<std::underlying_type_t<TorrentState>>(key), seed);
}

// TorrentHandle

const qreal TorrentHandle::USE_GLOBAL_RATIO = -2.;
const qreal TorrentHandle::NO_RATIO_LIMIT = -1.;

const int TorrentHandle::USE_GLOBAL_SEEDING_TIME = -2;
const int TorrentHandle::NO_SEEDING_TIME_LIMIT = -1;

const qreal TorrentHandle::MAX_RATIO = 9999.;
const int TorrentHandle::MAX_SEEDING_TIME = 525600;

TorrentHandle::TorrentHandle(Session *session, const lt::torrent_handle &nativeHandle,
                                     const CreateTorrentParams &params)
    : QObject(session)
    , m_session(session)
    , m_nativeHandle(nativeHandle)
    , m_state(TorrentState::Unknown)
    , m_renameCount(0)
    , m_useAutoTMM(params.savePath.isEmpty())
    , m_name(params.name)
    , m_savePath(Utils::Fs::toNativePath(params.savePath))
    , m_category(params.category)
    , m_tags(params.tags)
    , m_hasSeedStatus(params.hasSeedStatus)
    , m_ratioLimit(params.ratioLimit)
    , m_seedingTimeLimit(params.seedingTimeLimit)
    , m_tempPathDisabled(params.disableTempPath)
    , m_fastresumeDataRejected(false)
    , m_hasMissingFiles(false)
    , m_hasRootFolder(params.hasRootFolder)
    , m_needsToSetFirstLastPiecePriority(false)
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
    if (!params.restored || params.sequential)
        setSequentialDownload(params.sequential);
    if (!params.restored || params.firstLastPiecePriority)
        setFirstLastPiecePriority(params.firstLastPiecePriority);

    if (!params.restored && hasMetadata()) {
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
    if (!name.isEmpty()) return name;

    name = QString::fromStdString(m_nativeStatus.name);
    if (!name.isEmpty()) return name;

    if (hasMetadata()) {
        name = QString::fromStdString(m_torrentInfo.nativeInfo()->orig_files().name());
        if (!name.isEmpty()) return name;
    }

    return m_hash;
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

// size without the "don't download" files
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
        return Utils::Fs::toUniformPath(actualStorageLocation());
    else
        return Utils::Fs::toUniformPath(m_savePath);
}

QString TorrentHandle::rootPath(bool actual) const
{
    if ((filesCount() > 1) && !hasRootFolder())
        return {};

    const QString firstFilePath = filePath(0);
    const int slashIndex = firstFilePath.indexOf('/');
    if (slashIndex >= 0)
        return QDir(savePath(actual)).absoluteFilePath(firstFilePath.left(slashIndex));
    else
        return QDir(savePath(actual)).absoluteFilePath(firstFilePath);
}

QString TorrentHandle::contentPath(const bool actual) const
{
    if (filesCount() == 1)
        return QDir(savePath(actual)).absoluteFilePath(filePath(0));

    if (hasRootFolder())
        return rootPath(actual);

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
        move_impl(m_session->categorySavePath(m_category), MoveStorageMode::Overwrite);
}

bool TorrentHandle::hasRootFolder() const
{
    return m_hasRootFolder;
}

QString TorrentHandle::actualStorageLocation() const
{
    return QString::fromStdString(m_nativeStatus.save_path);
}

bool TorrentHandle::isAutoManaged() const
{
#if (LIBTORRENT_VERSION_NUM < 10200)
    return m_nativeStatus.auto_managed;
#else
    return bool {m_nativeStatus.flags & lt::torrent_flags::auto_managed};
#endif
}

void TorrentHandle::setAutoManaged(const bool enable)
{
#if (LIBTORRENT_VERSION_NUM < 10200)
    m_nativeHandle.auto_managed(enable);
#else
    if (enable)
        m_nativeHandle.set_flags(lt::torrent_flags::auto_managed);
    else
        m_nativeHandle.unset_flags(lt::torrent_flags::auto_managed);
#endif
}

QVector<TrackerEntry> TorrentHandle::trackers() const
{
    const std::vector<lt::announce_entry> nativeTrackers = m_nativeHandle.trackers();

    QVector<TrackerEntry> entries;
    entries.reserve(nativeTrackers.size());

    for (const lt::announce_entry &tracker : nativeTrackers)
        entries << tracker;

    return entries;
}

QHash<QString, TrackerInfo> TorrentHandle::trackerInfos() const
{
    return m_trackerInfos;
}

void TorrentHandle::addTrackers(const QVector<TrackerEntry> &trackers)
{
    QSet<TrackerEntry> currentTrackers;
    for (const lt::announce_entry &entry : m_nativeHandle.trackers())
        currentTrackers << entry;

    QVector<TrackerEntry> newTrackers;
    newTrackers.reserve(trackers.size());

    for (const TrackerEntry &tracker : trackers) {
        if (!currentTrackers.contains(tracker)) {
            m_nativeHandle.add_tracker(tracker.nativeEntry());
            newTrackers << tracker;
        }
    }

    if (!newTrackers.isEmpty())
        m_session->handleTorrentTrackersAdded(this, newTrackers);
}

void TorrentHandle::replaceTrackers(const QVector<TrackerEntry> &trackers)
{
    QVector<TrackerEntry> currentTrackers = this->trackers();

    QVector<TrackerEntry> newTrackers;
    newTrackers.reserve(trackers.size());

    std::vector<lt::announce_entry> nativeTrackers;
    nativeTrackers.reserve(trackers.size());

    for (const TrackerEntry &tracker : trackers) {
        nativeTrackers.emplace_back(tracker.nativeEntry());

        if (!currentTrackers.removeOne(tracker))
            newTrackers << tracker;
    }

    m_nativeHandle.replace_trackers(nativeTrackers);

    if (newTrackers.isEmpty() && currentTrackers.isEmpty()) {
        // when existing tracker reorders
        m_session->handleTorrentTrackersChanged(this);
    }
    else {
        if (!currentTrackers.isEmpty())
            m_session->handleTorrentTrackersRemoved(this, currentTrackers);

        if (!newTrackers.isEmpty())
            m_session->handleTorrentTrackersAdded(this, newTrackers);
    }
}

QVector<QUrl> TorrentHandle::urlSeeds() const
{
    const std::set<std::string> currentSeeds = m_nativeHandle.url_seeds();

    QVector<QUrl> urlSeeds;
    urlSeeds.reserve(currentSeeds.size());

    for (const std::string &urlSeed : currentSeeds)
        urlSeeds.append(QUrl(urlSeed.c_str()));

    return urlSeeds;
}

void TorrentHandle::addUrlSeeds(const QVector<QUrl> &urlSeeds)
{
    const std::set<std::string> currentSeeds = m_nativeHandle.url_seeds();

    QVector<QUrl> addedUrlSeeds;
    addedUrlSeeds.reserve(urlSeeds.size());

    for (const QUrl &url : urlSeeds) {
        const std::string nativeUrl = url.toString().toStdString();
        if (currentSeeds.find(nativeUrl) == currentSeeds.end()) {
            m_nativeHandle.add_url_seed(nativeUrl);
            addedUrlSeeds << url;
        }
    }

    if (!addedUrlSeeds.isEmpty())
        m_session->handleTorrentUrlSeedsAdded(this, addedUrlSeeds);
}

void TorrentHandle::removeUrlSeeds(const QVector<QUrl> &urlSeeds)
{
    const std::set<std::string> currentSeeds = m_nativeHandle.url_seeds();

    QVector<QUrl> removedUrlSeeds;
    removedUrlSeeds.reserve(urlSeeds.size());

    for (const QUrl &url : urlSeeds) {
        const std::string nativeUrl = url.toString().toStdString();
        if (currentSeeds.find(nativeUrl) != currentSeeds.end()) {
            m_nativeHandle.remove_url_seed(nativeUrl);
            removedUrlSeeds << url;
        }
    }

    if (!removedUrlSeeds.isEmpty())
        m_session->handleTorrentUrlSeedsRemoved(this, removedUrlSeeds);
}

bool TorrentHandle::connectPeer(const PeerAddress &peerAddress)
{
    lt::error_code ec;
#if (LIBTORRENT_VERSION_NUM < 10200)
    const lt::address addr = lt::address::from_string(peerAddress.ip.toString().toStdString(), ec);
#else
    const lt::address addr = lt::make_address(peerAddress.ip.toString().toStdString(), ec);
#endif
    if (ec) return false;

    const lt::tcp::endpoint endpoint(addr, peerAddress.port);
    try {
        m_nativeHandle.connect_peer(endpoint);
    }
#if (LIBTORRENT_VERSION_NUM < 10200)
    catch (const boost::system::system_error &err) {
#else
    catch (const lt::system_error &err) {
#endif
        LogMsg(tr("Failed to add peer \"%1\" to torrent \"%2\". Reason: %3")
            .arg(peerAddress.toString(), name(), QString::fromLocal8Bit(err.what())), Log::WARNING);
        return false;
    }

    LogMsg(tr("Peer \"%1\" is added to torrent \"%2\"").arg(peerAddress.toString(), name()));
    return true;
}

bool TorrentHandle::needSaveResumeData() const
{
    return m_nativeHandle.need_save_resume_data();
}

void TorrentHandle::saveResumeData()
{
    m_nativeHandle.save_resume_data();
    m_session->handleTorrentSaveResumeDataRequested(this);
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
    if (!isChecking()) {
        if (!m_nativeStatus.total_wanted)
            return 0.;

        if (m_nativeStatus.total_wanted_done == m_nativeStatus.total_wanted)
            return 1.;

        const qreal progress = static_cast<qreal>(m_nativeStatus.total_wanted_done) / m_nativeStatus.total_wanted;
        Q_ASSERT((progress >= 0.f) && (progress <= 1.f));
        return progress;
    }

    return m_nativeStatus.progress;
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

    if (m_session->isSubcategoriesEnabled() && m_category.startsWith(category + '/'))
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
        return true;
    }
    return false;
}

bool TorrentHandle::removeTag(const QString &tag)
{
    if (m_tags.remove(tag)) {
        m_session->handleTorrentTagRemoved(this, tag);
        return true;
    }
    return false;
}

void TorrentHandle::removeAllTags()
{
    for (const QString &tag : asConst(tags()))
        removeTag(tag);
}

QDateTime TorrentHandle::addedTime() const
{
    return QDateTime::fromSecsSinceEpoch(m_nativeStatus.added_time);
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
    if (!hasMetadata()) return {};
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
    if (!hasMetadata()) return {};

    const QDir saveDir(savePath(true));
    QStringList res;
    for (int i = 0; i < filesCount(); ++i)
        res << Utils::Fs::expandPathAbs(saveDir.absoluteFilePath(filePath(i)));
    return res;
}

QStringList TorrentHandle::absoluteFilePathsUnwanted() const
{
    if (!hasMetadata()) return {};

    const QDir saveDir(savePath(true));
#if (LIBTORRENT_VERSION_NUM < 10200)
    const std::vector<LTDownloadPriority> fp = m_nativeHandle.file_priorities();
#else
    const std::vector<LTDownloadPriority> fp = m_nativeHandle.get_file_priorities();
#endif

    QStringList res;
    for (int i = 0; i < static_cast<int>(fp.size()); ++i) {
        if (fp[i] == LTDownloadPriority {0}) {
            const QString path = Utils::Fs::expandPathAbs(saveDir.absoluteFilePath(filePath(i)));
            if (path.contains(".unwanted"))
                res << path;
        }
    }

    return res;
}

QVector<DownloadPriority> TorrentHandle::filePriorities() const
{
#if (LIBTORRENT_VERSION_NUM < 10200)
    const std::vector<LTDownloadPriority> fp = m_nativeHandle.file_priorities();
#else
    const std::vector<LTDownloadPriority> fp = m_nativeHandle.get_file_priorities();
#endif

    QVector<DownloadPriority> ret;
    std::transform(fp.cbegin(), fp.cend(), std::back_inserter(ret), [](LTDownloadPriority priority)
    {
        return static_cast<DownloadPriority>(LTUnderlyingType<LTDownloadPriority> {priority});
    });
    return ret;
}

TorrentInfo TorrentHandle::info() const
{
    return m_torrentInfo;
}

bool TorrentHandle::isPaused() const
{
#if (LIBTORRENT_VERSION_NUM < 10200)
    return (m_nativeStatus.paused && !isAutoManaged());
#else
    return ((m_nativeStatus.flags & lt::torrent_flags::paused)
            && !isAutoManaged());
#endif
}

bool TorrentHandle::isResumed() const
{
    return !isPaused();
}

bool TorrentHandle::isQueued() const
{
#if (LIBTORRENT_VERSION_NUM < 10200)
    return (m_nativeStatus.paused && isAutoManaged());
#else
    return ((m_nativeStatus.flags & lt::torrent_flags::paused)
            && isAutoManaged());
#endif
}

bool TorrentHandle::isChecking() const
{
    return ((m_nativeStatus.state == lt::torrent_status::checking_files)
            || (m_nativeStatus.state == lt::torrent_status::checking_resume_data));
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
            || m_state == TorrentState::ForcedUploading
            || m_state == TorrentState::Moving;
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
    return ((m_nativeStatus.state == lt::torrent_status::finished)
            || (m_nativeStatus.state == lt::torrent_status::seeding));
}

bool TorrentHandle::isForced() const
{
#if (LIBTORRENT_VERSION_NUM < 10200)
    return (!m_nativeStatus.paused && !isAutoManaged());
#else
    return (!(m_nativeStatus.flags & lt::torrent_flags::paused)
            && !isAutoManaged());
#endif
}

bool TorrentHandle::isSequentialDownload() const
{
#if (LIBTORRENT_VERSION_NUM < 10200)
    return m_nativeStatus.sequential_download;
#else
    return bool {m_nativeStatus.flags & lt::torrent_flags::sequential_download};
#endif
}

bool TorrentHandle::hasFirstLastPiecePriority() const
{
    if (!hasMetadata())
        return m_needsToSetFirstLastPiecePriority;

#if (LIBTORRENT_VERSION_NUM < 10200)
    const std::vector<LTDownloadPriority> filePriorities = nativeHandle().file_priorities();
#else
    const std::vector<LTDownloadPriority> filePriorities = nativeHandle().get_file_priorities();
#endif
    for (int i = 0; i < static_cast<int>(filePriorities.size()); ++i) {
        if (filePriorities[i] <= LTDownloadPriority {0})
            continue;

        const TorrentInfo::PieceRange extremities = info().filePieces(i);
        const LTDownloadPriority firstPiecePrio = nativeHandle().piece_priority(LTPieceIndex {extremities.first()});
        const LTDownloadPriority lastPiecePrio = nativeHandle().piece_priority(LTPieceIndex {extremities.last()});
        return ((firstPiecePrio == LTDownloadPriority {7}) && (lastPiecePrio == LTDownloadPriority {7}));
    }

    return false;
}

TorrentState TorrentHandle::state() const
{
    return m_state;
}

void TorrentHandle::updateState()
{
    if (hasError()) {
        m_state = TorrentState::Error;
    }
    else if (m_nativeStatus.state == lt::torrent_status::checking_resume_data) {
        m_state = TorrentState::CheckingResumeData;
    }
    else if (isMoveInProgress()) {
        m_state = TorrentState::Moving;
    }
    else if (hasMissingFiles()) {
        m_state = TorrentState::MissingFiles;
    }
    else if (isPaused()) {
        m_state = isSeed() ? TorrentState::PausedUploading : TorrentState::PausedDownloading;
    }
    else if (m_session->isQueueingSystemEnabled() && isQueued() && !isChecking()) {
        m_state = isSeed() ? TorrentState::QueuedUploading : TorrentState::QueuedDownloading;
    }
    else {
        switch (m_nativeStatus.state) {
        case lt::torrent_status::finished:
        case lt::torrent_status::seeding:
            if (isForced())
                m_state = TorrentState::ForcedUploading;
            else
                m_state = m_nativeStatus.upload_payload_rate > 0 ? TorrentState::Uploading : TorrentState::StalledUploading;
            break;
        case lt::torrent_status::allocating:
            m_state = TorrentState::Allocating;
            break;
        case lt::torrent_status::checking_files:
            m_state = m_hasSeedStatus ? TorrentState::CheckingUploading : TorrentState::CheckingDownloading;
            break;
        case lt::torrent_status::downloading_metadata:
            m_state = TorrentState::DownloadingMetadata;
            break;
        case lt::torrent_status::downloading:
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
    return static_cast<bool>(m_nativeStatus.errc);
}

bool TorrentHandle::hasFilteredPieces() const
{
#if (LIBTORRENT_VERSION_NUM < 10200)
    const std::vector<LTDownloadPriority> pp = m_nativeHandle.piece_priorities();
#else
    const std::vector<LTDownloadPriority> pp = m_nativeHandle.get_piece_priorities();
#endif
    return std::any_of(pp.cbegin(), pp.cend(), [](const LTDownloadPriority priority)
    {
        return (priority == LTDownloadPriority {0});
    });
}

int TorrentHandle::queuePosition() const
{
    if (m_nativeStatus.queue_position < LTQueuePosition {0}) return 0;

    return static_cast<int>(m_nativeStatus.queue_position) + 1;
}

QString TorrentHandle::error() const
{
    return QString::fromStdString(m_nativeStatus.errc.message());
}

qlonglong TorrentHandle::totalDownload() const
{
    return m_nativeStatus.all_time_download;
}

qlonglong TorrentHandle::totalUpload() const
{
    return m_nativeStatus.all_time_upload;
}

qlonglong TorrentHandle::activeTime() const
{
#if (LIBTORRENT_VERSION_NUM < 10200)
    return m_nativeStatus.active_time;
#else
    return lt::total_seconds(m_nativeStatus.active_duration);
#endif
}

qlonglong TorrentHandle::finishedTime() const
{
#if (LIBTORRENT_VERSION_NUM < 10200)
    return m_nativeStatus.finished_time;
#else
    return lt::total_seconds(m_nativeStatus.finished_duration);
#endif
}

qlonglong TorrentHandle::seedingTime() const
{
#if (LIBTORRENT_VERSION_NUM < 10200)
    return m_nativeStatus.seeding_time;
#else
    return lt::total_seconds(m_nativeStatus.seeding_duration);
#endif
}

qlonglong TorrentHandle::eta() const
{
    if (isPaused()) return MAX_ETA;

    const SpeedSampleAvg speedAverage = m_speedMonitor.average();

    if (isSeed()) {
        const qreal maxRatioValue = maxRatio();
        const int maxSeedingTimeValue = maxSeedingTime();
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
#if (LIBTORRENT_VERSION_NUM < 10200)
    std::vector<boost::int64_t> fp;
#else
    std::vector<int64_t> fp;
#endif
    m_nativeHandle.file_progress(fp, lt::torrent_handle::piece_granularity);

    const int count = static_cast<int>(fp.size());
    QVector<qreal> result;
    result.reserve(count);
    for (int i = 0; i < count; ++i) {
        const qlonglong size = fileSize(i);
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
    const int peers = m_nativeStatus.num_complete + m_nativeStatus.num_incomplete;
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
        return QDateTime::fromSecsSinceEpoch(m_nativeStatus.last_seen_complete);
    else
        return {};
}

QDateTime TorrentHandle::completedTime() const
{
    if (m_nativeStatus.completed_time > 0)
        return QDateTime::fromSecsSinceEpoch(m_nativeStatus.completed_time);
    else
        return {};
}

qlonglong TorrentHandle::timeSinceUpload() const
{
#if (LIBTORRENT_VERSION_NUM < 10200)
    return m_nativeStatus.time_since_upload;
#else
    if (m_nativeStatus.last_upload.time_since_epoch().count() == 0)
        return -1;
    return lt::total_seconds(lt::clock_type::now() - m_nativeStatus.last_upload);
#endif
}

qlonglong TorrentHandle::timeSinceDownload() const
{
#if (LIBTORRENT_VERSION_NUM < 10200)
    return m_nativeStatus.time_since_download;
#else
    if (m_nativeStatus.last_download.time_since_epoch().count() == 0)
        return -1;
    return lt::total_seconds(lt::clock_type::now() - m_nativeStatus.last_download);
#endif
}

qlonglong TorrentHandle::timeSinceActivity() const
{
    const qlonglong upTime = timeSinceUpload();
    const qlonglong downTime = timeSinceDownload();
    return ((upTime < 0) != (downTime < 0))
        ? std::max(upTime, downTime)
        : std::min(upTime, downTime);
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
#if (LIBTORRENT_VERSION_NUM < 10200)
    return m_nativeStatus.super_seeding;
#else
    return bool {m_nativeStatus.flags & lt::torrent_flags::super_seeding};
#endif
}

QVector<PeerInfo> TorrentHandle::peers() const
{
    std::vector<lt::peer_info> nativePeers;
    m_nativeHandle.get_peer_info(nativePeers);

    QVector<PeerInfo> peers;
    peers.reserve(nativePeers.size());
    for (const lt::peer_info &peer : nativePeers)
        peers << PeerInfo(this, peer);
    return peers;
}

QBitArray TorrentHandle::pieces() const
{
    QBitArray result(m_nativeStatus.pieces.size());
    for (int i = 0; i < result.size(); ++i) {
        if (m_nativeStatus.pieces[LTPieceIndex {i}])
            result.setBit(i, true);
    }
    return result;
}

QBitArray TorrentHandle::downloadingPieces() const
{
    QBitArray result(piecesCount());

    std::vector<lt::partial_piece_info> queue;
    m_nativeHandle.get_download_queue(queue);

    for (const lt::partial_piece_info &info : queue)
#if (LIBTORRENT_VERSION_NUM < 10200)
        result.setBit(info.piece_index);
#else
        result.setBit(LTUnderlyingType<LTPieceIndex> {info.piece_index});
#endif

    return result;
}

QVector<int> TorrentHandle::pieceAvailability() const
{
    std::vector<int> avail;
    m_nativeHandle.piece_availability(avail);

    return Vector::fromStdVector(avail);
}

qreal TorrentHandle::distributedCopies() const
{
    return m_nativeStatus.distributed_copies;
}

qreal TorrentHandle::maxRatio() const
{
    if (m_ratioLimit == USE_GLOBAL_RATIO)
        return m_session->globalMaxRatio();

    return m_ratioLimit;
}

int TorrentHandle::maxSeedingTime() const
{
    if (m_seedingTimeLimit == USE_GLOBAL_SEEDING_TIME)
        return m_session->globalMaxSeedingMinutes();

    return m_seedingTimeLimit;
}

qreal TorrentHandle::realRatio() const
{
#if (LIBTORRENT_VERSION_NUM < 10200)
    const boost::int64_t upload = m_nativeStatus.all_time_upload;
    // special case for a seeder who lost its stats, also assume nobody will import a 99% done torrent
    const boost::int64_t download = (m_nativeStatus.all_time_download < (m_nativeStatus.total_done * 0.01))
        ? m_nativeStatus.total_done
        : m_nativeStatus.all_time_download;
#else
    const int64_t upload = m_nativeStatus.all_time_upload;
    // special case for a seeder who lost its stats, also assume nobody will import a 99% done torrent
    const int64_t download = (m_nativeStatus.all_time_download < (m_nativeStatus.total_done * 0.01))
        ? m_nativeStatus.total_done
        : m_nativeStatus.all_time_download;
#endif

    if (download == 0)
        return (upload == 0) ? 0 : MAX_RATIO;

    const qreal ratio = upload / static_cast<qreal>(download);
    Q_ASSERT(ratio >= 0);
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
    return lt::total_seconds(m_nativeStatus.next_announce);
}

void TorrentHandle::setName(const QString &name)
{
    if (m_name != name) {
        m_name = name;
        m_session->handleTorrentNameChanged(this);
    }
}

bool TorrentHandle::setCategory(const QString &category)
{
    if (m_category != category) {
        if (!category.isEmpty() && !m_session->categories().contains(category))
            return false;

        const QString oldCategory = m_category;
        m_category = category;
        m_session->handleTorrentCategoryChanged(this, oldCategory);

        if (m_useAutoTMM) {
            if (!m_session->isDisableAutoTMMWhenCategoryChanged())
                move_impl(m_session->categorySavePath(m_category), MoveStorageMode::Overwrite);
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

    path = Utils::Fs::toUniformPath(path.trimmed());
    if (path.isEmpty())
        path = m_session->defaultSavePath();
    if (!path.endsWith('/'))
        path += '/';

    move_impl(path, MoveStorageMode::KeepExistingFiles);
}

void TorrentHandle::move_impl(QString path, const MoveStorageMode mode)
{
    if (path == savePath()) return;
    path = Utils::Fs::toNativePath(path);

    if (!useTempPath())
        moveStorage(path, mode);

    m_savePath = path;
    m_session->handleTorrentSavePathChanged(this);
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

    m_nativeHandle.force_recheck();
    m_unchecked = false;
}

void TorrentHandle::setSequentialDownload(const bool enable)
{
#if (LIBTORRENT_VERSION_NUM < 10200)
    m_nativeHandle.set_sequential_download(enable);
    m_nativeStatus.sequential_download = enable;  // prevent return cached value
#else
    if (enable) {
        m_nativeHandle.set_flags(lt::torrent_flags::sequential_download);
        m_nativeStatus.flags |= lt::torrent_flags::sequential_download;  // prevent return cached value
    }
    else {
        m_nativeHandle.unset_flags(lt::torrent_flags::sequential_download);
        m_nativeStatus.flags &= ~lt::torrent_flags::sequential_download;  // prevent return cached value
    }
#endif

    saveResumeData();
}

void TorrentHandle::toggleSequentialDownload()
{
    setSequentialDownload(!isSequentialDownload());
}

void TorrentHandle::setFirstLastPiecePriority(const bool enabled)
{
    setFirstLastPiecePriorityImpl(enabled);
}

void TorrentHandle::setFirstLastPiecePriorityImpl(const bool enabled, const QVector<DownloadPriority> &updatedFilePrio)
{
    // Download first and last pieces first for every file in the torrent

    if (!hasMetadata()) {
        m_needsToSetFirstLastPiecePriority = enabled;
        return;
    }

#if (LIBTORRENT_VERSION_NUM < 10200)
    const std::vector<LTDownloadPriority> filePriorities = !updatedFilePrio.isEmpty() ? toLTDownloadPriorities(updatedFilePrio)
                                                                           : nativeHandle().file_priorities();
    std::vector<LTDownloadPriority> piecePriorities = nativeHandle().piece_priorities();
#else
    const std::vector<LTDownloadPriority> filePriorities = !updatedFilePrio.isEmpty() ? toLTDownloadPriorities(updatedFilePrio)
                                                                           : nativeHandle().get_file_priorities();
    std::vector<LTDownloadPriority> piecePriorities = nativeHandle().get_piece_priorities();
#endif
    // Updating file priorities is an async operation in libtorrent, when we just updated it and immediately query it
    // we might get the old/wrong values, so we rely on `updatedFilePrio` in this case.
    for (int index = 0; index < static_cast<int>(filePriorities.size()); ++index) {
        const LTDownloadPriority filePrio = filePriorities[index];
        if (filePrio <= LTDownloadPriority {0})
            continue;

        // Determine the priority to set
        const LTDownloadPriority newPrio = enabled ? LTDownloadPriority {7} : filePrio;
        const TorrentInfo::PieceRange extremities = info().filePieces(index);

        // worst case: AVI index = 1% of total file size (at the end of the file)
        const int nNumPieces = std::ceil(fileSize(index) * 0.01 / pieceLength());
        for (int i = 0; i < nNumPieces; ++i) {
            piecePriorities[extremities.first() + i] = newPrio;
            piecePriorities[extremities.last() - i] = newPrio;
        }
    }

    m_nativeHandle.prioritize_pieces(piecePriorities);

    LogMsg(tr("Download first and last piece first: %1, torrent: '%2'")
        .arg((enabled ? tr("On") : tr("Off")), name()));

    saveResumeData();
}

void TorrentHandle::toggleFirstLastPiecePriority()
{
    setFirstLastPiecePriority(!hasFirstLastPiecePriority());
}

void TorrentHandle::pause()
{
    if (isPaused()) return;

    setAutoManaged(false);
    m_nativeHandle.pause();

    // Libtorrent doesn't emit a torrent_paused_alert when the
    // torrent is queued (no I/O)
    // We test on the cached m_nativeStatus
    if (isQueued())
        m_session->handleTorrentPaused(this);
}

void TorrentHandle::resume(bool forced)
{
    resume_impl(forced);
}

void TorrentHandle::resume_impl(bool forced)
{
    if (hasError())
        m_nativeHandle.clear_error();

    if (m_hasMissingFiles) {
        m_hasMissingFiles = false;
        m_nativeHandle.force_recheck();
    }

    setAutoManaged(!forced);
    if (forced)
        m_nativeHandle.resume();
}

void TorrentHandle::moveStorage(const QString &newPath, const MoveStorageMode mode)
{
    if (m_session->addMoveTorrentStorageJob(this, newPath, mode))
        m_storageIsMoving = true;
}

void TorrentHandle::renameFile(const int index, const QString &name)
{
    m_oldPath[LTFileIndex {index}].push_back(filePath(index));
    ++m_renameCount;
    m_nativeHandle.rename_file(LTFileIndex {index}, Utils::Fs::toNativePath(name).toStdString());
}

void TorrentHandle::handleStateUpdate(const lt::torrent_status &nativeStatus)
{
    updateStatus(nativeStatus);
}

void TorrentHandle::handleStorageMoved(const QString &newPath, const QString &errorMessage)
{
    m_storageIsMoving = false;

    if (!errorMessage.isEmpty())
        LogMsg(tr("Could not move torrent: %1. Reason: %2").arg(name(), errorMessage), Log::CRITICAL);
    else
        LogMsg(tr("Successfully moved torrent: %1. New path: %2").arg(name(), newPath));

    updateStatus();

    while ((m_renameCount == 0) && !m_moveFinishedTriggers.isEmpty())
        m_moveFinishedTriggers.takeFirst()();
}

void TorrentHandle::handleTrackerReplyAlert(const lt::tracker_reply_alert *p)
{
    const QString trackerUrl(p->tracker_url());
    qDebug("Received a tracker reply from %s (Num_peers = %d)", qUtf8Printable(trackerUrl), p->num_peers);
    // Connection was successful now. Remove possible old errors
    m_trackerInfos[trackerUrl] = {{}, p->num_peers};

    m_session->handleTorrentTrackerReply(this, trackerUrl);
}

void TorrentHandle::handleTrackerWarningAlert(const lt::tracker_warning_alert *p)
{
    const QString trackerUrl = p->tracker_url();
    const QString message = p->warning_message();

    // Connection was successful now but there is a warning message
    m_trackerInfos[trackerUrl].lastMessage = message; // Store warning message

    m_session->handleTorrentTrackerWarning(this, trackerUrl);
}

void TorrentHandle::handleTrackerErrorAlert(const lt::tracker_error_alert *p)
{
    const QString trackerUrl = p->tracker_url();
    const QString message = p->error_message();

    m_trackerInfos[trackerUrl].lastMessage = message;

    // Starting with libtorrent 1.2.x each tracker has multiple local endpoints from which
    // an announce is attempted. Some endpoints might succeed while others might fail.
    // Emit the signal only if all endpoints have failed.
    const QVector<TrackerEntry> trackerList = trackers();
    const auto iter = std::find_if(trackerList.cbegin(), trackerList.cend(), [&trackerUrl](const TrackerEntry &entry)
    {
        return (entry.url() == trackerUrl);
    });
    if ((iter != trackerList.cend()) && (iter->status() == TrackerEntry::NotWorking))
        m_session->handleTorrentTrackerError(this, trackerUrl);
}

void TorrentHandle::handleTorrentCheckedAlert(const lt::torrent_checked_alert *p)
{
    Q_UNUSED(p);
    qDebug("\"%s\" have just finished checking", qUtf8Printable(name()));

    if (m_fastresumeDataRejected && !m_hasMissingFiles) {
        saveResumeData();
        m_fastresumeDataRejected = false;
    }

    updateStatus();

    if (!m_hasMissingFiles) {
        if ((progress() < 1.0) && (wantedSize() > 0))
            m_hasSeedStatus = false;
        else if (progress() == 1.0)
            m_hasSeedStatus = true;

        adjustActualSavePath();
        manageIncompleteFiles();
    }

    m_session->handleTorrentChecked(this);
}

void TorrentHandle::handleTorrentFinishedAlert(const lt::torrent_finished_alert *p)
{
    Q_UNUSED(p);
    qDebug("Got a torrent finished alert for \"%s\"", qUtf8Printable(name()));
    qDebug("Torrent has seed status: %s", m_hasSeedStatus ? "yes" : "no");
    m_hasMissingFiles = false;
    if (m_hasSeedStatus) return;

    updateStatus();
    m_hasSeedStatus = true;

    adjustActualSavePath();
    manageIncompleteFiles();

    const bool recheckTorrentsOnCompletion = Preferences::instance()->recheckTorrentsOnCompletion();
    if (isMoveInProgress() || (m_renameCount > 0)) {
        if (recheckTorrentsOnCompletion)
            m_moveFinishedTriggers.append([this]() { forceRecheck(); });
        m_moveFinishedTriggers.append([this]() { m_session->handleTorrentFinished(this); });
    }
    else {
        if (recheckTorrentsOnCompletion && m_unchecked)
            forceRecheck();
        m_session->handleTorrentFinished(this);
    }
}

void TorrentHandle::handleTorrentPausedAlert(const lt::torrent_paused_alert *p)
{
    Q_UNUSED(p);

    updateStatus();
    m_speedMonitor.reset();

    m_session->handleTorrentPaused(this);
}

void TorrentHandle::handleTorrentResumedAlert(const lt::torrent_resumed_alert *p)
{
    Q_UNUSED(p);

    m_session->handleTorrentResumed(this);
}

void TorrentHandle::handleSaveResumeDataAlert(const lt::save_resume_data_alert *p)
{
#if (LIBTORRENT_VERSION_NUM < 10200)
    const bool useDummyResumeData = !(p && p->resume_data);
    lt::entry dummyEntry;

    lt::entry &resumeData = useDummyResumeData ? dummyEntry : *(p->resume_data);
#else
    const bool useDummyResumeData = !p;

    lt::entry resumeData = useDummyResumeData ? lt::entry() : lt::write_resume_data(p->params);
#endif

    updateStatus();

    if (useDummyResumeData) {
        resumeData["qBt-magnetUri"] = toMagnetUri().toStdString();
        resumeData["paused"] = isPaused();
        resumeData["auto_managed"] = isAutoManaged();
        // Both firstLastPiecePriority and sequential need to be stored in the
        // resume data if there is no metadata, otherwise they won't be
        // restored if qBittorrent quits before the metadata are retrieved:
        resumeData["qBt-firstLastPiecePriority"] = hasFirstLastPiecePriority();
        resumeData["qBt-sequential"] = isSequentialDownload();

        resumeData["qBt-addedTime"] = addedTime().toSecsSinceEpoch();
    }
    else {
        const auto savePath = resumeData.find_key("save_path")->string();
        resumeData["save_path"] = Profile::instance()->toPortablePath(QString::fromStdString(savePath)).toStdString();
    }
    resumeData["qBt-savePath"] = m_useAutoTMM ? "" : Profile::instance()->toPortablePath(m_savePath).toStdString();
    resumeData["qBt-ratioLimit"] = static_cast<int>(m_ratioLimit * 1000);
    resumeData["qBt-seedingTimeLimit"] = m_seedingTimeLimit;
    resumeData["qBt-category"] = m_category.toStdString();
    resumeData["qBt-tags"] = setToEntryList(m_tags);
    resumeData["qBt-name"] = m_name.toStdString();
    resumeData["qBt-seedStatus"] = m_hasSeedStatus;
    resumeData["qBt-tempPathDisabled"] = m_tempPathDisabled;
    resumeData["qBt-queuePosition"] = (static_cast<int>(nativeHandle().queue_position()) + 1); // qBt starts queue at 1
    resumeData["qBt-hasRootFolder"] = m_hasRootFolder;

#if (LIBTORRENT_VERSION_NUM < 10200)
    if (m_nativeStatus.stop_when_ready) {
#else
    if (m_nativeStatus.flags & lt::torrent_flags::stop_when_ready) {
#endif
        // We need to redefine these values when torrent starting/rechecking
        // in "paused" state since native values can be logically wrong
        // (torrent can be not paused and auto_managed when it is checking).
        resumeData["paused"] = true;
        resumeData["auto_managed"] = false;
    }

    m_session->handleTorrentResumeDataReady(this, resumeData);
}

void TorrentHandle::handleSaveResumeDataFailedAlert(const lt::save_resume_data_failed_alert *p)
{
    // if torrent has no metadata we should save dummy fastresume data
    // containing Magnet URI and qBittorrent own resume data only
    if (p->error.value() == lt::errors::no_metadata) {
        handleSaveResumeDataAlert(nullptr);
    }
    else {
        LogMsg(tr("Save resume data failed. Torrent: \"%1\", error: \"%2\"")
            .arg(name(), QString::fromLocal8Bit(p->error.message().c_str())), Log::CRITICAL);
        m_session->handleTorrentResumeDataFailed(this);
    }
}

void TorrentHandle::handleFastResumeRejectedAlert(const lt::fastresume_rejected_alert *p)
{
    m_fastresumeDataRejected = true;

    if (p->error.value() == lt::errors::mismatching_file_size) {
        // Mismatching file size (files were probably moved)
        m_hasMissingFiles = true;
        LogMsg(tr("File sizes mismatch for torrent '%1', pausing it.").arg(name()), Log::CRITICAL);
    }
    else {
        LogMsg(tr("Fast resume data was rejected for torrent '%1'. Reason: %2. Checking again...")
            .arg(name(), QString::fromStdString(p->message())), Log::WARNING);
    }
}

void TorrentHandle::handleFileRenamedAlert(const lt::file_renamed_alert *p)
{
    // We don't really need to call updateStatus() in this place.
    // All we need to do is make sure we have a valid instance of the TorrentInfo object.
    m_torrentInfo = TorrentInfo {m_nativeHandle.torrent_file()};

    // remove empty leftover folders
    // for example renaming "a/b/c" to "d/b/c", then folders "a/b" and "a" will
    // be removed if they are empty
    const QString oldFilePath = m_oldPath[p->index].takeFirst();
    const QString newFilePath = Utils::Fs::toUniformPath(p->new_name());

    if (m_oldPath[p->index].isEmpty())
        m_oldPath.remove(p->index);

    QVector<QStringRef> oldPathParts = oldFilePath.splitRef('/', QString::SkipEmptyParts);
    oldPathParts.removeLast();  // drop file name part
    QVector<QStringRef> newPathParts = newFilePath.splitRef('/', QString::SkipEmptyParts);
    newPathParts.removeLast();  // drop file name part

#if defined(Q_OS_WIN)
    const Qt::CaseSensitivity caseSensitivity = Qt::CaseInsensitive;
#else
    const Qt::CaseSensitivity caseSensitivity = Qt::CaseSensitive;
#endif

    int pathIdx = 0;
    while ((pathIdx < oldPathParts.size()) && (pathIdx < newPathParts.size())) {
        if (oldPathParts[pathIdx].compare(newPathParts[pathIdx], caseSensitivity) != 0)
            break;
        ++pathIdx;
    }

    for (int i = (oldPathParts.size() - 1); i >= pathIdx; --i) {
        QDir().rmdir(savePath() + Utils::String::join(oldPathParts, QLatin1String("/")));
        oldPathParts.removeLast();
    }

    --m_renameCount;
    while (!isMoveInProgress() && (m_renameCount == 0) && !m_moveFinishedTriggers.isEmpty())
        m_moveFinishedTriggers.takeFirst()();

    if (isPaused() && (m_renameCount == 0))
        saveResumeData();  // otherwise the new path will not be saved
}

void TorrentHandle::handleFileRenameFailedAlert(const lt::file_rename_failed_alert *p)
{
    LogMsg(tr("File rename failed. Torrent: \"%1\", file: \"%2\", reason: \"%3\"")
        .arg(name(), filePath(LTUnderlyingType<LTFileIndex> {p->index})
             , QString::fromLocal8Bit(p->error.message().c_str())), Log::WARNING);

    m_oldPath[p->index].removeFirst();
    if (m_oldPath[p->index].isEmpty())
        m_oldPath.remove(p->index);

    --m_renameCount;
    while (!isMoveInProgress() && (m_renameCount == 0) && !m_moveFinishedTriggers.isEmpty())
        m_moveFinishedTriggers.takeFirst()();

    if (isPaused() && (m_renameCount == 0))
        saveResumeData();  // otherwise the new path will not be saved
}

void TorrentHandle::handleFileCompletedAlert(const lt::file_completed_alert *p)
{
    // We don't really need to call updateStatus() in this place.
    // All we need to do is make sure we have a valid instance of the TorrentInfo object.
    m_torrentInfo = TorrentInfo {m_nativeHandle.torrent_file()};

    qDebug("A file completed download in torrent \"%s\"", qUtf8Printable(name()));
    if (m_session->isAppendExtensionEnabled()) {
        QString name = filePath(LTUnderlyingType<LTFileIndex> {p->index});
        if (name.endsWith(QB_EXT)) {
            const QString oldName = name;
            name.chop(QB_EXT.size());
            qDebug("Renaming %s to %s", qUtf8Printable(oldName), qUtf8Printable(name));
            renameFile(LTUnderlyingType<LTFileIndex> {p->index}, name);
        }
    }
}

void TorrentHandle::handleMetadataReceivedAlert(const lt::metadata_received_alert *p)
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

void TorrentHandle::handlePerformanceAlert(const lt::performance_alert *p) const
{
    LogMsg((tr("Performance alert: ") + QString::fromStdString(p->message()))
           , Log::INFO);
}

void TorrentHandle::handleTempPathChanged()
{
    adjustActualSavePath();
}

void TorrentHandle::handleCategorySavePathChanged()
{
    if (m_useAutoTMM)
        move_impl(m_session->categorySavePath(m_category), MoveStorageMode::Overwrite);
}

void TorrentHandle::handleAppendExtensionToggled()
{
    if (!hasMetadata()) return;

    manageIncompleteFiles();
}

void TorrentHandle::handleAlert(const lt::alert *a)
{
    switch (a->type()) {
    case lt::file_renamed_alert::alert_type:
        handleFileRenamedAlert(static_cast<const lt::file_renamed_alert*>(a));
        break;
    case lt::file_rename_failed_alert::alert_type:
        handleFileRenameFailedAlert(static_cast<const lt::file_rename_failed_alert*>(a));
        break;
    case lt::file_completed_alert::alert_type:
        handleFileCompletedAlert(static_cast<const lt::file_completed_alert*>(a));
        break;
    case lt::torrent_finished_alert::alert_type:
        handleTorrentFinishedAlert(static_cast<const lt::torrent_finished_alert*>(a));
        break;
    case lt::save_resume_data_alert::alert_type:
        handleSaveResumeDataAlert(static_cast<const lt::save_resume_data_alert*>(a));
        break;
    case lt::save_resume_data_failed_alert::alert_type:
        handleSaveResumeDataFailedAlert(static_cast<const lt::save_resume_data_failed_alert*>(a));
        break;
    case lt::torrent_paused_alert::alert_type:
        handleTorrentPausedAlert(static_cast<const lt::torrent_paused_alert*>(a));
        break;
    case lt::torrent_resumed_alert::alert_type:
        handleTorrentResumedAlert(static_cast<const lt::torrent_resumed_alert*>(a));
        break;
    case lt::tracker_error_alert::alert_type:
        handleTrackerErrorAlert(static_cast<const lt::tracker_error_alert*>(a));
        break;
    case lt::tracker_reply_alert::alert_type:
        handleTrackerReplyAlert(static_cast<const lt::tracker_reply_alert*>(a));
        break;
    case lt::tracker_warning_alert::alert_type:
        handleTrackerWarningAlert(static_cast<const lt::tracker_warning_alert*>(a));
        break;
    case lt::metadata_received_alert::alert_type:
        handleMetadataReceivedAlert(static_cast<const lt::metadata_received_alert*>(a));
        break;
    case lt::fastresume_rejected_alert::alert_type:
        handleFastResumeRejectedAlert(static_cast<const lt::fastresume_rejected_alert*>(a));
        break;
    case lt::torrent_checked_alert::alert_type:
        handleTorrentCheckedAlert(static_cast<const lt::torrent_checked_alert*>(a));
        break;
    case lt::performance_alert::alert_type:
        handlePerformanceAlert(static_cast<const lt::performance_alert*>(a));
        break;
    }
}

void TorrentHandle::manageIncompleteFiles()
{
    const bool isAppendExtensionEnabled = m_session->isAppendExtensionEnabled();
    const QVector<qreal> fp = filesProgress();
    if (fp.size() != filesCount()) {
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
        m_moveFinishedTriggers.append([this]() { adjustActualSavePath_impl(); });
}

void TorrentHandle::adjustActualSavePath_impl()
{
    const bool needUseTempDir = useTempPath();
    const QDir tempDir {m_session->torrentTempPath(info())};
    const QDir currentDir {actualStorageLocation()};
    const QDir targetDir {needUseTempDir ? tempDir : QDir {savePath()}};

    if (targetDir == currentDir) return;

    if (!needUseTempDir) {
        if ((currentDir == tempDir) && (currentDir != QDir {m_session->tempPath()})) {
            // torrent without root folder still has it in its temporary save path
            // so its temp path isn't equal to temp path root
            const QString currentDirPath = currentDir.absolutePath();
            m_moveFinishedTriggers.append([currentDirPath]
            {
                qDebug() << "Removing torrent temp folder:" << currentDirPath;
                Utils::Fs::smartRemoveEmptyFolderTree(currentDirPath);
            });
        }
    }

    moveStorage(Utils::Fs::toNativePath(targetDir.absolutePath()), MoveStorageMode::Overwrite);
}

lt::torrent_handle TorrentHandle::nativeHandle() const
{
    return m_nativeHandle;
}

void TorrentHandle::updateTorrentInfo()
{
    if (!hasMetadata()) return;

    m_torrentInfo = TorrentInfo(m_nativeStatus.torrent_file.lock());
}

bool TorrentHandle::isMoveInProgress() const
{
    return m_storageIsMoving;
}

bool TorrentHandle::useTempPath() const
{
    return !m_tempPathDisabled && m_session->isTempPathEnabled() && !(isSeed() || m_hasSeedStatus);
}

void TorrentHandle::updateStatus()
{
    updateStatus(m_nativeHandle.status());
}

void TorrentHandle::updateStatus(const lt::torrent_status &nativeStatus)
{
    m_nativeStatus = nativeStatus;

    updateState();
    updateTorrentInfo();

    // NOTE: Don't change the order of these conditionals!
    // Otherwise it will not work properly since torrent can be CheckingDownloading.
    if (isChecking())
        m_unchecked = false;
    else if (isDownloading())
        m_unchecked = true;

    m_speedMonitor.addSample({nativeStatus.download_payload_rate
        , nativeStatus.upload_payload_rate});
}

void TorrentHandle::setRatioLimit(qreal limit)
{
    if (limit < USE_GLOBAL_RATIO)
        limit = NO_RATIO_LIMIT;
    else if (limit > MAX_RATIO)
        limit = MAX_RATIO;

    if (m_ratioLimit != limit) {
        m_ratioLimit = limit;
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
        m_session->handleTorrentShareLimitChanged(this);
    }
}

void TorrentHandle::setUploadLimit(const int limit)
{
    m_nativeHandle.set_upload_limit(limit);
}

void TorrentHandle::setDownloadLimit(const int limit)
{
    m_nativeHandle.set_download_limit(limit);
}

void TorrentHandle::setSuperSeeding(const bool enable)
{
#if (LIBTORRENT_VERSION_NUM < 10200)
    m_nativeHandle.super_seeding(enable);
#else
    if (enable)
        m_nativeHandle.set_flags(lt::torrent_flags::super_seeding);
    else
        m_nativeHandle.unset_flags(lt::torrent_flags::super_seeding);
#endif
}

void TorrentHandle::flushCache() const
{
    m_nativeHandle.flush_cache();
}

QString TorrentHandle::toMagnetUri() const
{
    return QString::fromStdString(lt::make_magnet_uri(m_nativeHandle));
}

void TorrentHandle::prioritizeFiles(const QVector<DownloadPriority> &priorities)
{
    if (!hasMetadata()) return;
    if (priorities.size() != filesCount()) return;

    // Save first/last piece first option state
    const bool firstLastPieceFirst = hasFirstLastPiecePriority();

    // Reset 'm_hasSeedStatus' if needed in order to react again to
    // 'torrent_finished_alert' and eg show tray notifications
    const QVector<qreal> progress = filesProgress();
    const QVector<DownloadPriority> oldPriorities = filePriorities();
    for (int i = 0; i < oldPriorities.size(); ++i) {
        if ((oldPriorities[i] == DownloadPriority::Ignored)
            && (priorities[i] > DownloadPriority::Ignored)
            && (progress[i] < 1.0)) {
            m_hasSeedStatus = false;
            break;
        }
    }

    qDebug() << Q_FUNC_INFO << "Changing files priorities...";
    m_nativeHandle.prioritize_files(toLTDownloadPriorities(priorities));

    qDebug() << Q_FUNC_INFO << "Moving unwanted files to .unwanted folder and conversely...";
    const QString spath = savePath(true);
    for (int i = 0; i < priorities.size(); ++i) {
        const QString filepath = filePath(i);
        // Move unwanted files to a .unwanted subfolder
        if (priorities[i] == DownloadPriority::Ignored) {
            const QString oldAbsPath = QDir(spath).absoluteFilePath(filepath);
            const QString parentAbsPath = Utils::Fs::branchPath(oldAbsPath);
            // Make sure the file does not already exists
            if (QDir(parentAbsPath).dirName() != ".unwanted") {
                const QString unwantedAbsPath = parentAbsPath + "/.unwanted";
                const QString newAbsPath = unwantedAbsPath + '/' + Utils::Fs::fileName(filepath);
                qDebug() << "Unwanted path is" << unwantedAbsPath;
                if (QFile::exists(newAbsPath)) {
                    qWarning() << "File" << newAbsPath << "already exists at destination.";
                    continue;
                }

                const bool created = QDir().mkpath(unwantedAbsPath);
                qDebug() << "unwanted folder was created:" << created;
#ifdef Q_OS_WIN
                if (created) {
                    // Hide the folder on Windows
                    qDebug() << "Hiding folder (Windows)";
                    std::wstring winPath = Utils::Fs::toNativePath(unwantedAbsPath).toStdWString();
                    DWORD dwAttrs = ::GetFileAttributesW(winPath.c_str());
                    bool ret = ::SetFileAttributesW(winPath.c_str(), dwAttrs | FILE_ATTRIBUTE_HIDDEN);
                    Q_ASSERT(ret != 0); Q_UNUSED(ret);
                }
#endif
                QString parentPath = Utils::Fs::branchPath(filepath);
                if (!parentPath.isEmpty() && !parentPath.endsWith('/'))
                    parentPath += '/';
                renameFile(i, parentPath + ".unwanted/" + Utils::Fs::fileName(filepath));
            }
        }

        // Move wanted files back to their original folder
        if (priorities[i] > DownloadPriority::Ignored) {
            const QString parentRelPath = Utils::Fs::branchPath(filepath);
            if (QDir(parentRelPath).dirName() == ".unwanted") {
                const QString oldName = Utils::Fs::fileName(filepath);
                const QString newRelPath = Utils::Fs::branchPath(parentRelPath);
                if (newRelPath.isEmpty())
                    renameFile(i, oldName);
                else
                    renameFile(i, QDir(newRelPath).filePath(oldName));

                // Remove .unwanted directory if empty
                qDebug() << "Attempting to remove .unwanted folder at " << QDir(spath + '/' + newRelPath).absoluteFilePath(".unwanted");
                QDir(spath + '/' + newRelPath).rmdir(".unwanted");
            }
        }
    }

    // Restore first/last piece first option if necessary
    if (firstLastPieceFirst)
        setFirstLastPiecePriorityImpl(true, priorities);
}

QVector<qreal> TorrentHandle::availableFileFractions() const
{
    const int filesCount = this->filesCount();
    if (filesCount < 0) return {};

    const QVector<int> piecesAvailability = pieceAvailability();
    // libtorrent returns empty array for seeding only torrents
    if (piecesAvailability.empty()) return QVector<qreal>(filesCount, -1.);

    QVector<qreal> res;
    res.reserve(filesCount);
    const TorrentInfo info = this->info();
    for (int i = 0; i < filesCount; ++i) {
        const TorrentInfo::PieceRange filePieces = info.filePieces(i);

        int availablePieces = 0;
        for (int piece = filePieces.first(); piece <= filePieces.last(); ++piece) {
            availablePieces += (piecesAvailability[piece] > 0) ? 1 : 0;
        }
        res.push_back(static_cast<qreal>(availablePieces) / filePieces.size());
    }
    return res;
}
