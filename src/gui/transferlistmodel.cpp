/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015-2026  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2010  Christophe Dumez <chris@qbittorrent.org>
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

#include "transferlistmodel.h"

#include <QApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPalette>

#include "base/bittorrent/infohash.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/preferences.h"
#include "base/settingvalue.h"
#include "base/types.h"
#include "base/unicodestrings.h"
#include "base/utils/fs.h"
#include "base/utils/misc.h"
#include "base/utils/string.h"
#include "uithememanager.h"

using namespace Qt::Literals::StringLiterals;

namespace
{
    const QString kGroupsSettingKey = u"TorrentGroups/Groups"_s;
    constexpr quintptr GroupInternalId = static_cast<quintptr>(-1);

    struct GroupAggregates
    {
        qint64 wanted = 0;
        qint64 completed = 0;
        qint64 remaining = 0;
        qint64 dlSpeed = 0;
        qint64 upSpeed = 0;
        qint64 seeds = 0;
        qint64 totalSeeds = 0;
        qint64 peers = 0;
        qint64 totalPeers = 0;
        qint64 downloaded = 0;
        qint64 uploaded = 0;
        qint64 totalSize = 0;
    };

    GroupAggregates computeAggregates(const QList<BitTorrent::Torrent *> &members)
    {
        GroupAggregates aggr;
        for (const BitTorrent::Torrent *torrent : members)
        {
            if (!torrent)
                continue;

            aggr.wanted += torrent->wantedSize();
            aggr.completed += torrent->completedSize();
            aggr.remaining += torrent->remainingSize();
            aggr.dlSpeed += torrent->downloadPayloadRate();
            aggr.upSpeed += torrent->uploadPayloadRate();
            aggr.seeds += torrent->seedsCount();
            aggr.totalSeeds += torrent->totalSeedsCount();
            aggr.peers += torrent->leechsCount();
            aggr.totalPeers += torrent->totalLeechersCount();
            aggr.downloaded += torrent->totalDownload();
            aggr.uploaded += torrent->totalUpload();
            aggr.totalSize += torrent->totalSize();
        }
        return aggr;
    }

    QHash<BitTorrent::TorrentState, QColor> torrentStateColorsFromUITheme()
    {
        struct TorrentStateColorDescriptor
        {
            const BitTorrent::TorrentState state;
            const QString id;
        };

        const TorrentStateColorDescriptor colorDescriptors[] =
        {
            {BitTorrent::TorrentState::Downloading, u"TransferList.Downloading"_s},
            {BitTorrent::TorrentState::StalledDownloading, u"TransferList.StalledDownloading"_s},
            {BitTorrent::TorrentState::DownloadingMetadata, u"TransferList.DownloadingMetadata"_s},
            {BitTorrent::TorrentState::ForcedDownloadingMetadata, u"TransferList.ForcedDownloadingMetadata"_s},
            {BitTorrent::TorrentState::ForcedDownloading, u"TransferList.ForcedDownloading"_s},
            {BitTorrent::TorrentState::Uploading, u"TransferList.Uploading"_s},
            {BitTorrent::TorrentState::StalledUploading, u"TransferList.StalledUploading"_s},
            {BitTorrent::TorrentState::ForcedUploading, u"TransferList.ForcedUploading"_s},
            {BitTorrent::TorrentState::QueuedDownloading, u"TransferList.QueuedDownloading"_s},
            {BitTorrent::TorrentState::QueuedUploading, u"TransferList.QueuedUploading"_s},
            {BitTorrent::TorrentState::CheckingDownloading, u"TransferList.CheckingDownloading"_s},
            {BitTorrent::TorrentState::CheckingUploading, u"TransferList.CheckingUploading"_s},
            {BitTorrent::TorrentState::CheckingResumeData, u"TransferList.CheckingResumeData"_s},
            {BitTorrent::TorrentState::StoppedDownloading, u"TransferList.StoppedDownloading"_s},
            {BitTorrent::TorrentState::StoppedUploading, u"TransferList.StoppedUploading"_s},
            {BitTorrent::TorrentState::Moving, u"TransferList.Moving"_s},
            {BitTorrent::TorrentState::MissingFiles, u"TransferList.MissingFiles"_s},
            {BitTorrent::TorrentState::Error, u"TransferList.Error"_s}
        };

        QHash<BitTorrent::TorrentState, QColor> colors;
        for (const TorrentStateColorDescriptor &colorDescriptor : colorDescriptors)
        {
            const QColor themeColor = UIThemeManager::instance()->getColor(colorDescriptor.id);
            colors.insert(colorDescriptor.state, themeColor);
        }
        return colors;
    }
}

// TransferListModel

TransferListModel::TransferListModel(QObject *parent)
    : QAbstractItemModel {parent}
    , m_statusStrings {
        {BitTorrent::TorrentState::Downloading, tr("Downloading")},
        {BitTorrent::TorrentState::StalledDownloading, tr("Stalled", "Torrent is waiting for download to begin")},
        {BitTorrent::TorrentState::DownloadingMetadata, tr("Downloading metadata", "Used when loading a magnet link")},
        {BitTorrent::TorrentState::ForcedDownloadingMetadata, tr("[F] Downloading metadata", "Used when forced to load a magnet link. You probably shouldn't translate the F.")},
        {BitTorrent::TorrentState::ForcedDownloading, tr("[F] Downloading", "Used when the torrent is forced started. You probably shouldn't translate the F.")},
        {BitTorrent::TorrentState::Uploading, tr("Seeding", "Torrent is complete and in upload-only mode")},
        {BitTorrent::TorrentState::StalledUploading, tr("Seeding", "Torrent is complete and in upload-only mode")},
        {BitTorrent::TorrentState::ForcedUploading, tr("[F] Seeding", "Used when the torrent is forced started. You probably shouldn't translate the F.")},
        {BitTorrent::TorrentState::QueuedDownloading, tr("Queued", "Torrent is queued")},
        {BitTorrent::TorrentState::QueuedUploading, tr("Queued", "Torrent is queued")},
        {BitTorrent::TorrentState::CheckingDownloading, tr("Checking", "Torrent local data is being checked")},
        {BitTorrent::TorrentState::CheckingUploading, tr("Checking", "Torrent local data is being checked")},
        {BitTorrent::TorrentState::CheckingResumeData, tr("Checking resume data", "Used when loading the torrents from disk after qbt is launched. It checks the correctness of the .fastresume file. Normally it is completed in a fraction of a second, unless loading many many torrents.")},
        {BitTorrent::TorrentState::StoppedDownloading, tr("Stopped")},
        {BitTorrent::TorrentState::StoppedUploading, tr("Completed")},
        {BitTorrent::TorrentState::Moving, tr("Moving", "Torrent local data are being moved/relocated")},
        {BitTorrent::TorrentState::MissingFiles, tr("Missing Files")},
        {BitTorrent::TorrentState::Error, tr("Errored", "Torrent status, the torrent has an error")}}
{
    loadGroups();

    configure();
    connect(Preferences::instance(), &Preferences::changed, this, &TransferListModel::configure);

    loadUIThemeResources();
    connect(UIThemeManager::instance(), &UIThemeManager::themeChanged, this, [this]
    {
        loadUIThemeResources();

        if (rowCount() <= 0)
            return;

        const QList<int> roles = {Qt::DecorationRole, Qt::ForegroundRole};
        emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1), roles);
        for (int groupRow = 0; groupRow < m_groups.size(); ++groupRow)
        {
            const QModelIndex parentIndex = makeGroupIndex(groupRow, 0);
            const int childCount = rowCount(parentIndex);
            if (childCount > 0)
                emit dataChanged(index(0, 0, parentIndex), index(childCount - 1, columnCount() - 1, parentIndex), roles);
        }
    });

    // Load the torrents
    using namespace BitTorrent;
    addTorrents(Session::instance()->torrents());

    // Listen for torrent changes
    connect(Session::instance(), &Session::torrentsLoaded, this, &TransferListModel::addTorrents);
    connect(Session::instance(), &Session::torrentAboutToBeRemoved, this, &TransferListModel::handleTorrentAboutToBeRemoved);
    connect(Session::instance(), &Session::torrentsUpdated, this, &TransferListModel::handleTorrentsUpdated);

    connect(Session::instance(), &Session::torrentFinished, this, &TransferListModel::handleTorrentStatusUpdated);
    connect(Session::instance(), &Session::torrentMetadataReceived, this, &TransferListModel::handleTorrentStatusUpdated);
    connect(Session::instance(), &Session::torrentStarted, this, &TransferListModel::handleTorrentStatusUpdated);
    connect(Session::instance(), &Session::torrentStopped, this, &TransferListModel::handleTorrentStatusUpdated);
    connect(Session::instance(), &Session::torrentFinishedChecking, this, &TransferListModel::handleTorrentStatusUpdated);

    connect(Session::instance(), &Session::trackerEntryStatusesUpdated, this, &TransferListModel::handleTorrentStatusUpdated);
}

QModelIndex TransferListModel::index(const int row, const int column, const QModelIndex &parent) const
{
    if ((row < 0) || (column < 0) || (column >= columnCount()) || (parent.column() > 0))
        return {};

    if (!parent.isValid())
    {
        if (row >= rowCount())
            return {};

        if (row < m_groups.size())
            return makeGroupIndex(row, column);

        const int ungroupedRow = (row - m_groups.size());
        if ((ungroupedRow < 0) || (ungroupedRow >= m_ungroupedTorrents.size()))
            return {};

        if (BitTorrent::Torrent *const torrent = m_ungroupedTorrents.at(ungroupedRow))
            return createIndex(row, column, reinterpret_cast<quintptr>(torrent));
        return {};
    }

    if (!isGroupIndex(parent))
        return {};

    const int groupRow = parent.row();
    if ((groupRow < 0) || (groupRow >= m_groupedTorrents.size()))
        return {};

    const QList<BitTorrent::Torrent *> &members = m_groupedTorrents.at(groupRow);
    if ((row < 0) || (row >= members.size()))
        return {};

    if (BitTorrent::Torrent *const torrent = members.at(row))
        return createIndex(row, column, reinterpret_cast<quintptr>(torrent));
    return {};
}

QModelIndex TransferListModel::parent(const QModelIndex &index) const
{
    if (!index.isValid() || isGroupIndex(index))
        return {};

    const auto *const torrent = reinterpret_cast<BitTorrent::Torrent *>(index.internalId());
    if (!torrent)
        return {};

    const QString group = groupOf(torrent->id());
    if (group.isEmpty())
        return {};

    const int row = groupRow(group);
    return (row >= 0) ? makeGroupIndex(row, 0) : QModelIndex {};
}

int TransferListModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return (m_groups.size() + m_ungroupedTorrents.size());

    if ((parent.column() > 0) || !isGroupIndex(parent))
        return 0;

    const int groupRow = parent.row();
    if ((groupRow < 0) || (groupRow >= m_groupedTorrents.size()))
        return 0;

    return m_groupedTorrents.at(groupRow).size();
}

int TransferListModel::columnCount(const QModelIndex &) const
{
    return NB_COLUMNS;
}

QVariant TransferListModel::headerData(const int section, const Qt::Orientation orientation, const int role) const
{
    if (orientation == Qt::Horizontal)
    {
        if (role == Qt::DisplayRole)
        {
            switch (section)
            {
            case TR_QUEUE_POSITION: return QChar(u'#');
            case TR_NAME: return tr("Name", "i.e: torrent name");
            case TR_SIZE: return tr("Size", "i.e: torrent size");
            case TR_PROGRESS: return tr("Progress", "% Done");
            case TR_STATUS: return tr("Status", "Torrent status (e.g. downloading, seeding, stopped)");
            case TR_SEEDS: return tr("Seeds", "i.e. full sources (often untranslated)");
            case TR_PEERS: return tr("Peers", "i.e. partial sources (often untranslated)");
            case TR_DLSPEED: return tr("Down Speed", "i.e: Download speed");
            case TR_UPSPEED: return tr("Up Speed", "i.e: Upload speed");
            case TR_RATIO: return tr("Ratio", "Share ratio");
            case TR_POPULARITY: return tr("Popularity");
            case TR_ETA: return tr("ETA", "i.e: Estimated Time of Arrival / Time left");
            case TR_CATEGORY: return tr("Category");
            case TR_TAGS: return tr("Tags");
            case TR_CREATE_DATE: return tr("Created On", "Torrent was initially created on 01/01/2010 08:00");
            case TR_ADD_DATE: return tr("Added On", "Torrent was added to transfer list on 01/01/2010 08:00");
            case TR_SEED_DATE: return tr("Completed On", "Torrent was completed on 01/01/2010 08:00");
            case TR_TRACKER: return tr("Tracker");
            case TR_DLLIMIT: return tr("Down Limit", "i.e: Download limit");
            case TR_UPLIMIT: return tr("Up Limit", "i.e: Upload limit");
            case TR_AMOUNT_DOWNLOADED: return tr("Downloaded", "Amount of data downloaded (e.g. in MB)");
            case TR_AMOUNT_UPLOADED: return tr("Uploaded", "Amount of data uploaded (e.g. in MB)");
            case TR_AMOUNT_DOWNLOADED_SESSION: return tr("Session Downloaded", "Amount of data downloaded since program open (e.g. in MB)");
            case TR_AMOUNT_UPLOADED_SESSION: return tr("Session Uploaded", "Amount of data uploaded since program open (e.g. in MB)");
            case TR_AMOUNT_LEFT: return tr("Remaining", "Amount of data left to download (e.g. in MB)");
            case TR_TIME_ELAPSED: return tr("Time Active", "Time (duration) the torrent is active (not stopped)");
            case TR_SAVE_PATH: return tr("Save Path", "Torrent save path");
            case TR_DOWNLOAD_PATH: return tr("Incomplete Save Path", "Torrent incomplete save path");
            case TR_COMPLETED: return tr("Completed", "Amount of data completed (e.g. in MB)");
            case TR_RATIO_LIMIT: return tr("Ratio Limit", "Upload share ratio limit");
            case TR_SEEN_COMPLETE_DATE: return tr("Last Seen Complete", "Indicates the time when the torrent was last seen complete/whole");
            case TR_LAST_ACTIVITY: return tr("Last Activity", "Time passed since a chunk was downloaded/uploaded");
            case TR_TOTAL_SIZE: return tr("Total Size", "i.e. Size including unwanted data");
            case TR_AVAILABILITY: return tr("Availability", "The number of distributed copies of the torrent");
            case TR_INFOHASH_V1: return tr("Info Hash v1", "i.e: torrent info hash v1");
            case TR_INFOHASH_V2: return tr("Info Hash v2", "i.e: torrent info hash v2");
            case TR_REANNOUNCE: return tr("Reannounce In", "Indicates the time until next trackers reannounce");
            case TR_PRIVATE: return tr("Private", "Flags private torrents");
            case TR_GROUP: return tr("Group", "Virtual group name aggregating multiple torrents");
            default: return {};
            }
        }
        else if (role == Qt::ToolTipRole)
        {
            switch (section)
            {
            case TR_POPULARITY: return tr("Ratio / Time Active (in months), indicates how popular the torrent is");
            default: return {};
            }
        }
        else if (role == Qt::TextAlignmentRole)
        {
            switch (section)
            {
            case TR_AMOUNT_DOWNLOADED:
            case TR_AMOUNT_UPLOADED:
            case TR_AMOUNT_DOWNLOADED_SESSION:
            case TR_AMOUNT_UPLOADED_SESSION:
            case TR_AMOUNT_LEFT:
            case TR_COMPLETED:
            case TR_SIZE:
            case TR_TOTAL_SIZE:
            case TR_ETA:
            case TR_SEEDS:
            case TR_PEERS:
            case TR_UPSPEED:
            case TR_DLSPEED:
            case TR_UPLIMIT:
            case TR_DLLIMIT:
            case TR_RATIO_LIMIT:
            case TR_RATIO:
            case TR_POPULARITY:
            case TR_QUEUE_POSITION:
            case TR_LAST_ACTIVITY:
            case TR_AVAILABILITY:
            case TR_REANNOUNCE:
                return QVariant(Qt::AlignRight | Qt::AlignVCenter);
            default:
                return QAbstractItemModel::headerData(section, orientation, role);
            }
        }
    }

    return QAbstractItemModel::headerData(section, orientation, role);
}

QString TransferListModel::displayValue(const BitTorrent::Torrent *torrent, const int column) const
{
    bool hideValues = false;
    if (m_hideZeroValuesMode == HideZeroValuesMode::Always)
        hideValues = true;
    else if (m_hideZeroValuesMode == HideZeroValuesMode::Stopped)
        hideValues = (torrent->state() == BitTorrent::TorrentState::StoppedDownloading);

    const auto availabilityString = [hideValues](const qreal value) -> QString
    {
        if (hideValues && (value == 0))
            return {};
        return (value >= 0)
            ? Utils::String::fromDouble(value, 3)
            : tr("N/A");
    };

    const auto unitString = [hideValues](const qint64 value, const bool isSpeedUnit = false) -> QString
    {
        return (hideValues && (value == 0))
                ? QString {} : Utils::Misc::friendlyUnit(value, isSpeedUnit);
    };

    const auto limitString = [hideValues](const qint64 value) -> QString
    {
        if (hideValues && (value <= 0))
            return {};

        return (value > 0)
                ? Utils::Misc::friendlyUnit(value, true)
                : C_INFINITY;
    };

    const auto amountString = [hideValues](const qint64 value, const qint64 total) -> QString
    {
        if (hideValues && (value == 0) && (total == 0))
            return {};
        return u"%1 (%2)"_s.arg(QString::number(value), QString::number(total));
    };

    const auto etaString = [hideValues](const qlonglong value) -> QString
    {
        if (hideValues && (value >= MAX_ETA))
            return {};
        return Utils::Misc::userFriendlyDuration(value, MAX_ETA);
    };

    const auto ratioString = [hideValues](const qreal value) -> QString
    {
        if (hideValues && (value <= 0))
            return {};

         return ((static_cast<int>(value) == -1) || (value >= BitTorrent::Torrent::MAX_RATIO))
                  ? C_INFINITY : Utils::String::fromDouble(value, 2);
    };

    const auto queuePositionString = [](const qint64 value) -> QString
    {
        return (value >= 0) ? QString::number(value + 1) : u"*"_s;
    };

    const auto lastActivityString = [hideValues](qint64 value) -> QString
    {
        if (hideValues && ((value < 0) || (value >= MAX_ETA)))
            return {};

        // Show '< 1m ago' when elapsed time is 0
        if (value == 0)
            value = 1;

        return (value >= 0)
            ? tr("%1 ago", "e.g.: 1h 20m ago").arg(Utils::Misc::userFriendlyDuration(value))
            : Utils::Misc::userFriendlyDuration(value);
    };

    const auto timeElapsedString = [hideValues](const qint64 elapsedTime, const qint64 seedingTime) -> QString
    {
        if (seedingTime <= 0)
        {
            if (hideValues && (elapsedTime == 0))
                return {};
            return Utils::Misc::userFriendlyDuration(elapsedTime);
        }

        return tr("%1 (seeded for %2)", "e.g. 4m39s (seeded for 3m10s)")
                .arg(Utils::Misc::userFriendlyDuration(elapsedTime)
                     , Utils::Misc::userFriendlyDuration(seedingTime));
    };

    const auto progressString = [](const qreal progress) -> QString
    {
        return (progress >= 1)
                ? u"100%"_s
                : (Utils::String::fromDouble((progress * 100), 1) + u'%');
    };

    const auto statusString = [this](const BitTorrent::TorrentState state, const QString &errorMessage) -> QString
    {
        return (state == BitTorrent::TorrentState::Error)
                   ? m_statusStrings[state] + u": " + errorMessage
                   : m_statusStrings[state];
    };

    const auto hashString = [hideValues](const auto &hash) -> QString
    {
        if (hideValues && !hash.isValid())
            return {};
        return hash.isValid() ? hash.toString() : tr("N/A");
    };

    const auto reannounceString = [hideValues](const qint64 time) -> QString
    {
        if (hideValues && (time == 0))
            return {};
        return Utils::Misc::userFriendlyDuration(time);
    };

    const auto privateString = [hideValues](const bool isPrivate, const bool hasMetadata) -> QString
    {
        if (hideValues && !isPrivate)
            return {};
        if (hasMetadata)
            return isPrivate ? tr("Yes") : tr("No");
        return tr("N/A");
    };

    switch (column)
    {
    case TR_NAME:
        return torrent->name();
    case TR_QUEUE_POSITION:
        return queuePositionString(torrent->queuePosition());
    case TR_SIZE:
        return unitString(torrent->wantedSize());
    case TR_PROGRESS:
        return progressString(torrent->progress());
    case TR_STATUS:
        return statusString(torrent->state(), torrent->error());
    case TR_SEEDS:
        return amountString(torrent->seedsCount(), torrent->totalSeedsCount());
    case TR_PEERS:
        return amountString(torrent->leechsCount(), torrent->totalLeechersCount());
    case TR_DLSPEED:
        return unitString(torrent->downloadPayloadRate(), true);
    case TR_UPSPEED:
        return unitString(torrent->uploadPayloadRate(), true);
    case TR_ETA:
        return etaString(torrent->eta());
    case TR_RATIO:
        return ratioString(torrent->realRatio());
    case TR_RATIO_LIMIT:
        return ratioString(torrent->effectiveShareLimits().ratioLimit);
    case TR_POPULARITY:
        return ratioString(torrent->popularity());
    case TR_CATEGORY:
        return torrent->category();
    case TR_TAGS:
        return Utils::String::joinIntoString(torrent->tags(), u", "_s);
    case TR_CREATE_DATE:
        return QLocale().toString(torrent->creationDate().toLocalTime(), QLocale::ShortFormat);
    case TR_ADD_DATE:
        return QLocale().toString(torrent->addedTime().toLocalTime(), QLocale::ShortFormat);
    case TR_SEED_DATE:
        return QLocale().toString(torrent->completedTime().toLocalTime(), QLocale::ShortFormat);
    case TR_TRACKER:
        return torrent->currentTracker();
    case TR_DLLIMIT:
        return limitString(torrent->downloadLimit());
    case TR_UPLIMIT:
        return limitString(torrent->uploadLimit());
    case TR_AMOUNT_DOWNLOADED:
        return unitString(torrent->totalDownload());
    case TR_AMOUNT_UPLOADED:
        return unitString(torrent->totalUpload());
    case TR_AMOUNT_DOWNLOADED_SESSION:
        return unitString(torrent->totalPayloadDownload());
    case TR_AMOUNT_UPLOADED_SESSION:
        return unitString(torrent->totalPayloadUpload());
    case TR_AMOUNT_LEFT:
        return unitString(torrent->remainingSize());
    case TR_TIME_ELAPSED:
        return timeElapsedString(torrent->activeTime(), torrent->finishedTime());
    case TR_SAVE_PATH:
        return torrent->savePath().toString();
    case TR_DOWNLOAD_PATH:
        return torrent->downloadPath().toString();
    case TR_COMPLETED:
        return unitString(torrent->completedSize());
    case TR_SEEN_COMPLETE_DATE:
        return QLocale().toString(torrent->lastSeenComplete().toLocalTime(), QLocale::ShortFormat);
    case TR_LAST_ACTIVITY:
        return lastActivityString(torrent->timeSinceActivity());
    case TR_AVAILABILITY:
        return availabilityString(torrent->distributedCopies());
    case TR_TOTAL_SIZE:
        return unitString(torrent->totalSize());
    case TR_INFOHASH_V1:
        return hashString(torrent->infoHash().v1());
    case TR_INFOHASH_V2:
        return hashString(torrent->infoHash().v2());
    case TR_REANNOUNCE:
        return reannounceString(torrent->nextAnnounce());
    case TR_PRIVATE:
        return privateString(torrent->isPrivate(), torrent->hasMetadata());
    case TR_GROUP:
        return groupOf(torrent->id());
    }

    return {};
}

QVariant TransferListModel::internalValue(const BitTorrent::Torrent *torrent, const int column, const bool alt) const
{
    switch (column)
    {
    case TR_NAME:
        return torrent->name();
    case TR_QUEUE_POSITION:
        return torrent->queuePosition();
    case TR_SIZE:
        return torrent->wantedSize();
    case TR_PROGRESS:
        return torrent->progress() * 100;
    case TR_STATUS:
        return QVariant::fromValue(torrent->state());
    case TR_SEEDS:
        return !alt ? torrent->seedsCount() : torrent->totalSeedsCount();
    case TR_PEERS:
        return !alt ? torrent->leechsCount() : torrent->totalLeechersCount();
    case TR_DLSPEED:
        return torrent->downloadPayloadRate();
    case TR_UPSPEED:
        return torrent->uploadPayloadRate();
    case TR_ETA:
        return torrent->eta();
    case TR_RATIO:
        return torrent->realRatio();
    case TR_POPULARITY:
        return torrent->popularity();
    case TR_CATEGORY:
        return torrent->category();
    case TR_TAGS:
        return QVariant::fromValue(torrent->tags());
    case TR_CREATE_DATE:
        return torrent->creationDate();
    case TR_ADD_DATE:
        return torrent->addedTime();
    case TR_SEED_DATE:
        return torrent->completedTime();
    case TR_TRACKER:
        return torrent->currentTracker();
    case TR_DLLIMIT:
        return torrent->downloadLimit();
    case TR_UPLIMIT:
        return torrent->uploadLimit();
    case TR_AMOUNT_DOWNLOADED:
        return torrent->totalDownload();
    case TR_AMOUNT_UPLOADED:
        return torrent->totalUpload();
    case TR_AMOUNT_DOWNLOADED_SESSION:
        return torrent->totalPayloadDownload();
    case TR_AMOUNT_UPLOADED_SESSION:
        return torrent->totalPayloadUpload();
    case TR_AMOUNT_LEFT:
        return torrent->remainingSize();
    case TR_TIME_ELAPSED:
        return !alt ? torrent->activeTime() : torrent->finishedTime();
    case TR_DOWNLOAD_PATH:
        return torrent->downloadPath().data();
    case TR_SAVE_PATH:
        return torrent->savePath().data();
    case TR_COMPLETED:
        return torrent->completedSize();
    case TR_RATIO_LIMIT:
        return torrent->effectiveShareLimits().ratioLimit;
    case TR_SEEN_COMPLETE_DATE:
        return torrent->lastSeenComplete();
    case TR_LAST_ACTIVITY:
        return torrent->timeSinceActivity();
    case TR_AVAILABILITY:
        return torrent->distributedCopies();
    case TR_TOTAL_SIZE:
        return torrent->totalSize();
    case TR_INFOHASH_V1:
        return QVariant::fromValue(torrent->infoHash().v1());
    case TR_INFOHASH_V2:
        return QVariant::fromValue(torrent->infoHash().v2());
    case TR_REANNOUNCE:
        return torrent->nextAnnounce();
    case TR_PRIVATE:
        return (torrent->hasMetadata() ? torrent->isPrivate() : QVariant());
    case TR_GROUP:
        return groupOf(torrent->id());
    }

    return {};
}

QVariant TransferListModel::data(const QModelIndex &index, const int role) const
{
    if (!index.isValid())
        return {};

    if (isGroupIndex(index))
    {
        const int groupRow = index.row();
        if ((groupRow < 0) || (groupRow >= m_groups.size()))
            return {};

        const GroupData &group = m_groups.at(groupRow);
        const QList<BitTorrent::Torrent *> &members = m_groupedTorrents.value(groupRow);

        if (role == Qt::BackgroundRole)
        {
            QColor baseColor = QApplication::palette().color(QPalette::Base);
            baseColor = (baseColor.lightness() < 128) ? baseColor.lighter(135) : baseColor.darker(115);
            return QBrush(baseColor);
        }

        if ((role == Qt::DecorationRole) && (index.column() == TR_NAME))
        {
            const bool expanded = m_expandedGroups.contains(group.name);
            return UIThemeManager::instance()->getIcon(expanded ? u"folder-documents"_s : u"directory"_s);
        }

        if ((role != Qt::DisplayRole) && (role != UnderlyingDataRole) && (role != AdditionalUnderlyingDataRole))
            return {};

        const GroupAggregates aggr = computeAggregates(members);
        const int column = index.column();

        if (column == TR_NAME)
        {
            if (role == Qt::DisplayRole)
                return u"%1 (%2)"_s.arg(group.name).arg(members.size());
            return group.name;
        }

        if (role == UnderlyingDataRole)
        {
            switch (column)
            {
            case TR_SIZE: return aggr.wanted;
            case TR_TOTAL_SIZE: return aggr.totalSize;
            case TR_PROGRESS: return (aggr.wanted > 0) ? ((aggr.completed * 100.0) / aggr.wanted) : 0.0;
            case TR_SEEDS: return aggr.seeds;
            case TR_PEERS: return aggr.peers;
            case TR_DLSPEED: return aggr.dlSpeed;
            case TR_UPSPEED: return aggr.upSpeed;
            case TR_ETA: return (aggr.dlSpeed > 0) ? (aggr.remaining / qMax<qint64>(1, aggr.dlSpeed)) : MAX_ETA;
            case TR_RATIO: return (aggr.downloaded > 0) ? (static_cast<double>(aggr.uploaded) / aggr.downloaded) : BitTorrent::Torrent::MAX_RATIO;
            case TR_AMOUNT_DOWNLOADED: return aggr.downloaded;
            case TR_AMOUNT_UPLOADED: return aggr.uploaded;
            case TR_AMOUNT_LEFT: return aggr.remaining;
            case TR_COMPLETED: return aggr.completed;
            default: return {};
            }
        }

        if (role == AdditionalUnderlyingDataRole)
        {
            switch (column)
            {
            case TR_SEEDS: return aggr.totalSeeds;
            case TR_PEERS: return aggr.totalPeers;
            default: return {};
            }
        }

        switch (column)
        {
        case TR_SIZE:
            return Utils::Misc::friendlyUnit(aggr.wanted);
        case TR_TOTAL_SIZE:
            return Utils::Misc::friendlyUnit(aggr.totalSize);
        case TR_PROGRESS:
        {
            if (aggr.wanted <= 0)
                return QString {};
            const double progress = static_cast<double>(aggr.completed) / static_cast<double>(aggr.wanted);
            return (progress >= 1.0) ? QStringLiteral("100%") : (Utils::String::fromDouble(progress * 100.0, 1) + QLatin1Char('%'));
        }
        case TR_SEEDS:
            return u"%1 (%2)"_s.arg(aggr.seeds).arg(aggr.totalSeeds);
        case TR_PEERS:
            return u"%1 (%2)"_s.arg(aggr.peers).arg(aggr.totalPeers);
        case TR_DLSPEED:
            return Utils::Misc::friendlyUnit(aggr.dlSpeed, true);
        case TR_UPSPEED:
            return Utils::Misc::friendlyUnit(aggr.upSpeed, true);
        case TR_ETA:
        {
            const qint64 eta = (aggr.dlSpeed > 0) ? (aggr.remaining / qMax<qint64>(1, aggr.dlSpeed)) : MAX_ETA;
            return Utils::Misc::userFriendlyDuration(eta, MAX_ETA);
        }
        case TR_RATIO:
        {
            if (aggr.downloaded <= 0)
                return C_INFINITY;
            const double ratio = static_cast<double>(aggr.uploaded) / aggr.downloaded;
            if ((static_cast<int>(ratio) == -1) || (ratio >= BitTorrent::Torrent::MAX_RATIO))
                return C_INFINITY;
            return Utils::String::fromDouble(ratio, 2);
        }
        case TR_AMOUNT_DOWNLOADED:
            return Utils::Misc::friendlyUnit(aggr.downloaded);
        case TR_AMOUNT_UPLOADED:
            return Utils::Misc::friendlyUnit(aggr.uploaded);
        case TR_AMOUNT_LEFT:
            return Utils::Misc::friendlyUnit(aggr.remaining);
        case TR_COMPLETED:
            return Utils::Misc::friendlyUnit(aggr.completed);
        default:
            return {};
        }
    }

    const auto *const torrent = reinterpret_cast<BitTorrent::Torrent *>(index.internalId());
    if (!torrent)
        return {};

    switch (role)
    {
    case Qt::ForegroundRole:
        if (m_useTorrentStatesColors)
            return m_stateThemeColors.value(torrent->state());
        break;
    case Qt::DisplayRole:
        return displayValue(torrent, index.column());
    case UnderlyingDataRole:
        return internalValue(torrent, index.column(), false);
    case AdditionalUnderlyingDataRole:
        return internalValue(torrent, index.column(), true);
    case Qt::DecorationRole:
        if (index.column() == TR_NAME)
            return getIconByState(torrent->state());
        break;
    case Qt::ToolTipRole:
        switch (index.column())
        {
        case TR_NAME:
        case TR_STATUS:
        case TR_CATEGORY:
        case TR_TAGS:
        case TR_TRACKER:
        case TR_SAVE_PATH:
        case TR_DOWNLOAD_PATH:
        case TR_INFOHASH_V1:
        case TR_INFOHASH_V2:
            return displayValue(torrent, index.column());
        case TR_GROUP:
            return displayValue(torrent, index.column());
        }
        break;
    case Qt::BackgroundRole:
        if (!groupOf(torrent->id()).isEmpty())
        {
            QColor baseColor = QApplication::palette().color(QPalette::Base);
            baseColor = (baseColor.lightness() < 128) ? baseColor.lighter(120) : baseColor.darker(105);
            return QBrush(baseColor);
        }
        break;
    case Qt::TextAlignmentRole:
        switch (index.column())
        {
        case TR_AMOUNT_DOWNLOADED:
        case TR_AMOUNT_UPLOADED:
        case TR_AMOUNT_DOWNLOADED_SESSION:
        case TR_AMOUNT_UPLOADED_SESSION:
        case TR_AMOUNT_LEFT:
        case TR_COMPLETED:
        case TR_SIZE:
        case TR_TOTAL_SIZE:
        case TR_ETA:
        case TR_SEEDS:
        case TR_PEERS:
        case TR_UPSPEED:
        case TR_DLSPEED:
        case TR_UPLIMIT:
        case TR_DLLIMIT:
        case TR_RATIO_LIMIT:
        case TR_RATIO:
        case TR_POPULARITY:
        case TR_QUEUE_POSITION:
        case TR_LAST_ACTIVITY:
        case TR_AVAILABILITY:
        case TR_REANNOUNCE:
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        }
        break;
    default:
        break;
    }

    return {};
}

bool TransferListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || (role != Qt::DisplayRole) || isGroupIndex(index))
        return false;

    BitTorrent::Torrent *const torrent = torrentHandle(index);
    if (!torrent)
        return false;

    // Category and Name columns can be edited
    switch (index.column())
    {
    case TR_NAME:
        torrent->setName(value.toString());
        break;
    case TR_CATEGORY:
        torrent->setCategory(value.toString());
        break;
    default:
        return false;
    }

    return true;
}

void TransferListModel::addTorrents(const QList<BitTorrent::Torrent *> &torrents)
{
    if (torrents.isEmpty())
        return;

    beginResetModel();
    m_torrentList.reserve(m_torrentList.size() + torrents.size());
    int row = m_torrentList.size();
    for (BitTorrent::Torrent *torrent : torrents)
    {
        if (m_torrentMap.contains(torrent))
            continue;

        m_torrentList.append(torrent);
        m_torrentMap[torrent] = row++;
    }
    rebuildGroupingLayout();
    endResetModel();
}

Qt::ItemFlags TransferListModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    return QAbstractItemModel::flags(index);
}

BitTorrent::Torrent *TransferListModel::torrentHandle(const QModelIndex &index) const
{
    if (!index.isValid() || isGroupIndex(index))
        return nullptr;

    return reinterpret_cast<BitTorrent::Torrent *>(index.internalId());
}

QModelIndex TransferListModel::indexForTorrent(const BitTorrent::Torrent *torrent, const int column) const
{
    return makeTorrentIndex(torrent, column);
}

QString TransferListModel::groupName(const QModelIndex &index) const
{
    if (!isGroupIndex(index))
        return {};

    const int row = index.row();
    if ((row < 0) || (row >= m_groups.size()))
        return {};
    return m_groups.at(row).name;
}

QStringList TransferListModel::groupNames() const
{
    QStringList names;
    names.reserve(m_groups.size());
    for (const GroupData &group : m_groups)
        names.append(group.name);
    return names;
}

QString TransferListModel::groupOf(const BitTorrent::TorrentID &id) const
{
    return m_groupByMember.value(id);
}

QStringList TransferListModel::expandedGroups() const
{
    return m_expandedGroups;
}

bool TransferListModel::hasGroups() const
{
    return !m_groups.isEmpty();
}

bool TransferListModel::createGroup(const QString &name, const QSet<BitTorrent::TorrentID> &initialMembers)
{
    const QString trimmedName = name.trimmed();
    if (trimmedName.isEmpty() || m_groupIndexByName.contains(trimmedName))
        return false;

    beginResetModel();
    GroupData group;
    group.name = trimmedName;
    m_groups.append(group);
    rebuildGroupNameIndex();

    for (const BitTorrent::TorrentID &id : initialMembers)
        moveMemberToGroup(id, trimmedName);

    rebuildGroupingLayout();
    endResetModel();

    saveGroups();
    emit groupsChanged();
    return true;
}

bool TransferListModel::renameGroup(const QString &oldName, const QString &newName)
{
    const QString trimmedName = newName.trimmed();
    if (trimmedName.isEmpty())
        return false;

    const int oldIndex = m_groupIndexByName.value(oldName, -1);
    if ((oldIndex < 0) || ((oldName != trimmedName) && m_groupIndexByName.contains(trimmedName)))
        return false;

    beginResetModel();
    GroupData &group = m_groups[oldIndex];
    group.name = trimmedName;
    rebuildGroupNameIndex();

    for (auto it = m_groupByMember.begin(); it != m_groupByMember.end(); ++it)
    {
        if (it.value() == oldName)
            it.value() = trimmedName;
    }

    if (m_expandedGroups.contains(oldName))
    {
        m_expandedGroups.removeAll(oldName);
        if (!m_expandedGroups.contains(trimmedName))
            m_expandedGroups.append(trimmedName);
    }

    rebuildGroupingLayout();
    endResetModel();

    saveGroups();
    emit groupsChanged();
    return true;
}

bool TransferListModel::deleteGroup(const QString &name)
{
    const int groupIndex = m_groupIndexByName.value(name, -1);
    if (groupIndex < 0)
        return false;

    beginResetModel();
    const GroupData deletedGroup = m_groups.takeAt(groupIndex);
    rebuildGroupNameIndex();

    for (const BitTorrent::TorrentID &id : deletedGroup.members)
        m_groupByMember.remove(id);
    for (auto it = m_groupByMember.begin(); it != m_groupByMember.end();)
    {
        if (it.value() == name)
            it = m_groupByMember.erase(it);
        else
            ++it;
    }

    m_expandedGroups.removeAll(name);
    rebuildGroupingLayout();
    endResetModel();

    saveGroups();
    emit groupsChanged();
    return true;
}

bool TransferListModel::addMembers(const QString &groupName, const QSet<BitTorrent::TorrentID> &members)
{
    if (members.isEmpty() || !m_groupIndexByName.contains(groupName))
        return false;

    bool changed = false;
    for (const BitTorrent::TorrentID &id : members)
    {
        if (groupOf(id) != groupName)
        {
            changed = true;
            break;
        }
    }
    if (!changed)
        return false;

    beginResetModel();
    for (const BitTorrent::TorrentID &id : members)
        moveMemberToGroup(id, groupName);
    rebuildGroupingLayout();
    endResetModel();

    saveGroups();
    emit groupsChanged();
    return true;
}

bool TransferListModel::removeMembers(const QSet<BitTorrent::TorrentID> &members)
{
    if (members.isEmpty())
        return false;

    bool changed = false;
    for (const BitTorrent::TorrentID &id : members)
    {
        if (m_groupByMember.contains(id))
        {
            changed = true;
            break;
        }
    }
    if (!changed)
        return false;

    beginResetModel();
    for (const BitTorrent::TorrentID &id : members)
    {
        if (m_groupByMember.contains(id))
            removeMemberFromGroup(id);
    }
    rebuildGroupingLayout();
    endResetModel();

    saveGroups();
    emit groupsChanged();
    return true;
}

void TransferListModel::setGroupExpanded(const QString &name, const bool expanded)
{
    if (!m_groupIndexByName.contains(name))
        return;

    const bool contains = m_expandedGroups.contains(name);
    if (expanded == contains)
        return;

    if (expanded)
        m_expandedGroups.append(name);
    else
        m_expandedGroups.removeAll(name);

    saveGroups();

    const int row = groupRow(name);
    if (row >= 0)
        emit dataChanged(makeGroupIndex(row, 0), makeGroupIndex(row, columnCount() - 1), {Qt::DecorationRole});
}

void TransferListModel::handleTorrentAboutToBeRemoved(BitTorrent::Torrent *const torrent)
{
    const int row = m_torrentMap.value(torrent, -1);
    if (row < 0)
        return;

    const bool hadGroup = !groupOf(torrent->id()).isEmpty();

    beginResetModel();
    m_torrentList.removeAt(row);
    m_torrentMap.remove(torrent);
    for (int &value : m_torrentMap)
    {
        if (value > row)
            --value;
    }
    removeMemberFromGroup(torrent->id());
    rebuildGroupingLayout();
    endResetModel();

    if (hadGroup)
    {
        saveGroups();
        emit groupsChanged();
    }
}

void TransferListModel::handleTorrentStatusUpdated(BitTorrent::Torrent *const torrent)
{
    const QModelIndex torrentIndex = makeTorrentIndex(torrent, 0);
    if (!torrentIndex.isValid())
        return;

    emit dataChanged(torrentIndex, torrentIndex.sibling(torrentIndex.row(), columnCount() - 1));

    const QModelIndex parentIndex = parent(torrentIndex);
    if (parentIndex.isValid())
        emit dataChanged(parentIndex, parentIndex.sibling(parentIndex.row(), columnCount() - 1));
}

void TransferListModel::handleTorrentsUpdated(const QList<BitTorrent::Torrent *> &torrents)
{
    for (BitTorrent::Torrent *const torrent : torrents)
        handleTorrentStatusUpdated(torrent);
}

void TransferListModel::loadGroups()
{
    m_groups.clear();
    m_groupIndexByName.clear();
    m_groupByMember.clear();
    m_expandedGroups.clear();

    const SettingValue<QByteArray> groupsSetting {kGroupsSettingKey};
    const QByteArray rawData = groupsSetting.get();
    if (rawData.isEmpty())
        return;

    const QJsonDocument document = QJsonDocument::fromJson(rawData);
    if (!document.isObject())
        return;

    const QJsonObject root = document.object();
    const QJsonArray groupsArray = root.value(u"groups"_s).toArray();
    for (const QJsonValue &value : groupsArray)
    {
        if (!value.isObject())
            continue;

        const QJsonObject groupObject = value.toObject();
        const QString groupName = groupObject.value(u"name"_s).toString().trimmed();
        if (groupName.isEmpty() || m_groupIndexByName.contains(groupName))
            continue;

        GroupData group;
        group.name = groupName;

        const QJsonArray members = groupObject.value(u"members"_s).toArray();
        for (const QJsonValue &memberValue : members)
        {
            const BitTorrent::TorrentID id = BitTorrent::TorrentID::fromString(memberValue.toString());
            if (!id.isValid() || m_groupByMember.contains(id))
                continue;

            group.members.insert(id);
            m_groupByMember.insert(id, groupName);
        }

        m_groups.append(group);
        m_groupIndexByName.insert(groupName, (m_groups.size() - 1));
    }

    const QJsonArray expandedArray = root.value(u"expanded"_s).toArray();
    for (const QJsonValue &value : expandedArray)
    {
        const QString groupName = value.toString();
        if (m_groupIndexByName.contains(groupName) && !m_expandedGroups.contains(groupName))
            m_expandedGroups.append(groupName);
    }

    rebuildGroupingLayout();
}

void TransferListModel::saveGroups() const
{
    QJsonArray groupsArray;
    for (const GroupData &group : m_groups)
    {
        QJsonObject groupObject;
        groupObject.insert(u"name"_s, group.name);

        QJsonArray membersArray;
        for (const BitTorrent::TorrentID &id : group.members)
            membersArray.append(id.toString());
        groupObject.insert(u"members"_s, membersArray);

        groupsArray.append(groupObject);
    }

    QJsonArray expandedArray;
    for (const QString &groupName : m_expandedGroups)
    {
        if (m_groupIndexByName.contains(groupName))
            expandedArray.append(groupName);
    }

    QJsonObject root;
    root.insert(u"groups"_s, groupsArray);
    root.insert(u"expanded"_s, expandedArray);

    SettingValue<QByteArray> groupsSetting {kGroupsSettingKey};
    groupsSetting = QJsonDocument(root).toJson(QJsonDocument::Compact);
}

void TransferListModel::rebuildGroupingLayout()
{
    m_groupedTorrents = QVector<QList<BitTorrent::Torrent *>>(m_groups.size());
    m_ungroupedTorrents.clear();
    m_torrentPositions.clear();
    m_torrentPositions.reserve(m_torrentList.size());

    int ungroupedRow = m_groups.size();
    for (BitTorrent::Torrent *const torrent : m_torrentList)
    {
        const QString groupName = m_groupByMember.value(torrent->id());
        const int row = groupRow(groupName);

        TorrentPosition position;
        if (row >= 0)
        {
            QList<BitTorrent::Torrent *> &groupMembers = m_groupedTorrents[row];
            position.groupRow = row;
            position.childRow = groupMembers.size();
            position.topLevelRow = row;
            groupMembers.append(torrent);
        }
        else
        {
            position.topLevelRow = ungroupedRow++;
            m_ungroupedTorrents.append(torrent);
        }

        m_torrentPositions.insert(torrent, position);
    }
}

void TransferListModel::rebuildGroupNameIndex()
{
    m_groupIndexByName.clear();
    for (int i = 0; i < m_groups.size(); ++i)
        m_groupIndexByName.insert(m_groups.at(i).name, i);
}

bool TransferListModel::isGroupIndex(const QModelIndex &index) const
{
    return index.isValid() && (index.internalId() == GroupInternalId);
}

QModelIndex TransferListModel::makeGroupIndex(const int row, const int column) const
{
    if ((row < 0) || (row >= m_groups.size()) || (column < 0) || (column >= columnCount()))
        return {};
    return createIndex(row, column, GroupInternalId);
}

QModelIndex TransferListModel::makeTorrentIndex(const BitTorrent::Torrent *torrent, const int column) const
{
    if (!torrent || (column < 0) || (column >= columnCount()))
        return {};

    const TorrentPosition position = m_torrentPositions.value(const_cast<BitTorrent::Torrent *>(torrent), {});
    if (position.topLevelRow < 0)
        return {};

    auto *const mutableTorrent = const_cast<BitTorrent::Torrent *>(torrent);
    if (position.groupRow >= 0)
        return createIndex(position.childRow, column, reinterpret_cast<quintptr>(mutableTorrent));
    return createIndex(position.topLevelRow, column, reinterpret_cast<quintptr>(mutableTorrent));
}

int TransferListModel::groupRow(const QString &name) const
{
    return m_groupIndexByName.value(name, -1);
}

bool TransferListModel::moveMemberToGroup(const BitTorrent::TorrentID &id, const QString &groupName)
{
    const int targetGroupIndex = m_groupIndexByName.value(groupName, -1);
    if (targetGroupIndex < 0)
        return false;

    const QString oldGroup = m_groupByMember.value(id);
    if (oldGroup == groupName)
        return false;

    if (!oldGroup.isEmpty())
    {
        const int oldGroupIndex = m_groupIndexByName.value(oldGroup, -1);
        if (oldGroupIndex >= 0)
            m_groups[oldGroupIndex].members.remove(id);
    }

    m_groups[targetGroupIndex].members.insert(id);
    m_groupByMember.insert(id, groupName);
    return true;
}

void TransferListModel::removeMemberFromGroup(const BitTorrent::TorrentID &id)
{
    const QString oldGroup = m_groupByMember.take(id);
    if (oldGroup.isEmpty())
        return;

    const int groupIndex = m_groupIndexByName.value(oldGroup, -1);
    if (groupIndex >= 0)
        m_groups[groupIndex].members.remove(id);
}

void TransferListModel::configure()
{
    const Preferences *pref = Preferences::instance();

    HideZeroValuesMode hideZeroValuesMode = HideZeroValuesMode::Never;
    if (pref->getHideZeroValues())
    {
        if (pref->getHideZeroComboValues() == 1)
            hideZeroValuesMode = HideZeroValuesMode::Stopped;
        else
            hideZeroValuesMode = HideZeroValuesMode::Always;
    }

    bool isDataChanged = false;

    if (m_hideZeroValuesMode != hideZeroValuesMode)
    {
        m_hideZeroValuesMode = hideZeroValuesMode;
        isDataChanged = true;
    }

    if (const bool useTorrentStatesColors = pref->useTorrentStatesColors(); m_useTorrentStatesColors != useTorrentStatesColors)
    {
        m_useTorrentStatesColors = useTorrentStatesColors;
        isDataChanged = true;
    }

    if (isDataChanged && (rowCount() > 0))
    {
        emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
        for (int groupRow = 0; groupRow < m_groups.size(); ++groupRow)
        {
            const QModelIndex parentIndex = makeGroupIndex(groupRow, 0);
            const int childCount = rowCount(parentIndex);
            if (childCount > 0)
                emit dataChanged(index(0, 0, parentIndex), index(childCount - 1, columnCount() - 1, parentIndex));
        }
    }
}

void TransferListModel::loadUIThemeResources()
{
    m_stateThemeColors = torrentStateColorsFromUITheme();

    const auto *themeManager = UIThemeManager::instance();
    m_checkingIcon = themeManager->getIcon(u"force-recheck"_s, u"checking"_s);
    m_completedIcon = themeManager->getIcon(u"checked-completed"_s, u"completed"_s);
    m_downloadingIcon = themeManager->getIcon(u"downloading"_s);
    m_errorIcon = themeManager->getIcon(u"error"_s);
    m_movingIcon = themeManager->getIcon(u"set-location"_s);
    m_stoppedIcon = themeManager->getIcon(u"stopped"_s, u"media-playback-pause"_s);
    m_queuedIcon = themeManager->getIcon(u"queued"_s);
    m_stalledDLIcon = themeManager->getIcon(u"stalledDL"_s);
    m_stalledUPIcon = themeManager->getIcon(u"stalledUP"_s);
    m_uploadingIcon = themeManager->getIcon(u"upload"_s, u"uploading"_s);
}

QIcon TransferListModel::getIconByState(const BitTorrent::TorrentState state) const
{
    switch (state)
    {
    case BitTorrent::TorrentState::Downloading:
    case BitTorrent::TorrentState::ForcedDownloading:
    case BitTorrent::TorrentState::DownloadingMetadata:
    case BitTorrent::TorrentState::ForcedDownloadingMetadata:
        return m_downloadingIcon;
    case BitTorrent::TorrentState::StalledDownloading:
        return m_stalledDLIcon;
    case BitTorrent::TorrentState::StalledUploading:
        return m_stalledUPIcon;
    case BitTorrent::TorrentState::Uploading:
    case BitTorrent::TorrentState::ForcedUploading:
        return m_uploadingIcon;
    case BitTorrent::TorrentState::StoppedDownloading:
        return m_stoppedIcon;
    case BitTorrent::TorrentState::StoppedUploading:
        return m_completedIcon;
    case BitTorrent::TorrentState::QueuedDownloading:
    case BitTorrent::TorrentState::QueuedUploading:
        return m_queuedIcon;
    case BitTorrent::TorrentState::CheckingDownloading:
    case BitTorrent::TorrentState::CheckingUploading:
    case BitTorrent::TorrentState::CheckingResumeData:
        return m_checkingIcon;
    case BitTorrent::TorrentState::Moving:
        return m_movingIcon;
    case BitTorrent::TorrentState::Unknown:
    case BitTorrent::TorrentState::MissingFiles:
    case BitTorrent::TorrentState::Error:
        return m_errorIcon;
    default:
        Q_UNREACHABLE();
        break;
    }

    return {};
}
