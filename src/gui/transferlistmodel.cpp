/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
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
#include <QDebug>
#include <QIcon>
#include <QPalette>

#include "base/bittorrent/session.h"
#include "base/bittorrent/torrenthandle.h"
#include "base/global.h"
#include "base/preferences.h"
#include "base/unicodestrings.h"
#include "base/utils/fs.h"
#include "base/utils/misc.h"
#include "base/utils/string.h"
#include "uithememanager.h"

static QIcon getIconByState(BitTorrent::TorrentState state);
static QColor getDefaultColorByState(BitTorrent::TorrentState state);

static QIcon getPausedIcon();
static QIcon getQueuedIcon();
static QIcon getDownloadingIcon();
static QIcon getStalledDownloadingIcon();
static QIcon getUploadingIcon();
static QIcon getStalledUploadingIcon();
static QIcon getCompletedIcon();
static QIcon getCheckingIcon();
static QIcon getErrorIcon();

static bool isDarkTheme();

namespace
{
    QHash<BitTorrent::TorrentState, QColor> torrentStateColorsFromUITheme()
    {
        struct TorrentStateColorDescriptor
        {
            const BitTorrent::TorrentState state;
            const QString id;
        };

        const TorrentStateColorDescriptor colorDescriptors[] =
        {
            {BitTorrent::TorrentState::Downloading, QLatin1String("TransferList.Downloading")},
            {BitTorrent::TorrentState::StalledDownloading, QLatin1String("TransferList.StalledDownloading")},
            {BitTorrent::TorrentState::DownloadingMetadata, QLatin1String("TransferList.DownloadingMetadata")},
            {BitTorrent::TorrentState::ForcedDownloading, QLatin1String("TransferList.ForcedDownloading")},
            {BitTorrent::TorrentState::Uploading, QLatin1String("TransferList.Uploading")},
            {BitTorrent::TorrentState::StalledUploading, QLatin1String("TransferList.StalledUploading")},
            {BitTorrent::TorrentState::ForcedUploading, QLatin1String("TransferList.ForcedUploading")},
            {BitTorrent::TorrentState::QueuedDownloading, QLatin1String("TransferList.QueuedDownloading")},
            {BitTorrent::TorrentState::QueuedUploading, QLatin1String("TransferList.QueuedUploading")},
            {BitTorrent::TorrentState::CheckingDownloading, QLatin1String("TransferList.CheckingDownloading")},
            {BitTorrent::TorrentState::CheckingUploading, QLatin1String("TransferList.CheckingUploading")},
            {BitTorrent::TorrentState::CheckingResumeData, QLatin1String("TransferList.CheckingResumeData")},
            {BitTorrent::TorrentState::PausedDownloading, QLatin1String("TransferList.PausedDownloading")},
            {BitTorrent::TorrentState::PausedUploading, QLatin1String("TransferList.PausedUploading")},
            {BitTorrent::TorrentState::Moving, QLatin1String("TransferList.Moving")},
            {BitTorrent::TorrentState::MissingFiles, QLatin1String("TransferList.MissingFiles")},
            {BitTorrent::TorrentState::Error, QLatin1String("TransferList.Error")}
        };

        QHash<BitTorrent::TorrentState, QColor> colors;
        for (const TorrentStateColorDescriptor &colorDescriptor : colorDescriptors)
        {
            const QColor themeColor = UIThemeManager::instance()->getColor(colorDescriptor.id, QColor());
            if (themeColor.isValid())
                colors.insert(colorDescriptor.state, themeColor);
        }
        return colors;
    }
}

// TransferListModel

TransferListModel::TransferListModel(QObject *parent)
    : QAbstractListModel {parent}
    , m_statusStrings
    {
          {BitTorrent::TorrentState::Downloading, tr("Downloading")},
          {BitTorrent::TorrentState::StalledDownloading, tr("Stalled", "Torrent is waiting for download to begin")},
          {BitTorrent::TorrentState::DownloadingMetadata, tr("Downloading metadata", "Used when loading a magnet link")},
          {BitTorrent::TorrentState::ForcedDownloading, tr("[F] Downloading", "Used when the torrent is forced started. You probably shouldn't translate the F.")},
          {BitTorrent::TorrentState::Uploading, tr("Seeding", "Torrent is complete and in upload-only mode")},
          {BitTorrent::TorrentState::StalledUploading, tr("Seeding", "Torrent is complete and in upload-only mode")},
          {BitTorrent::TorrentState::ForcedUploading, tr("[F] Seeding", "Used when the torrent is forced started. You probably shouldn't translate the F.")},
          {BitTorrent::TorrentState::QueuedDownloading, tr("Queued", "Torrent is queued")},
          {BitTorrent::TorrentState::QueuedUploading, tr("Queued", "Torrent is queued")},
          {BitTorrent::TorrentState::CheckingDownloading, tr("Checking", "Torrent local data is being checked")},
          {BitTorrent::TorrentState::CheckingUploading, tr("Checking", "Torrent local data is being checked")},
          {BitTorrent::TorrentState::CheckingResumeData, tr("Checking resume data", "Used when loading the torrents from disk after qbt is launched. It checks the correctness of the .fastresume file. Normally it is completed in a fraction of a second, unless loading many many torrents.")},
          {BitTorrent::TorrentState::PausedDownloading, tr("Paused")},
          {BitTorrent::TorrentState::PausedUploading, tr("Completed")},
          {BitTorrent::TorrentState::Moving, tr("Moving", "Torrent local data are being moved/relocated")},
          {BitTorrent::TorrentState::MissingFiles, tr("Missing Files")},
          {BitTorrent::TorrentState::Error, tr("Errored", "Torrent status, the torrent has an error")}
      }
    , m_stateThemeColors {torrentStateColorsFromUITheme()}
{
    configure();
    connect(Preferences::instance(), &Preferences::changed, this, &TransferListModel::configure);

    // Load the torrents
    using namespace BitTorrent;
    for (TorrentHandle *const torrent : asConst(Session::instance()->torrents()))
        addTorrent(torrent);

    // Listen for torrent changes
    connect(Session::instance(), &Session::torrentLoaded, this, &TransferListModel::addTorrent);
    connect(Session::instance(), &Session::torrentAboutToBeRemoved, this, &TransferListModel::handleTorrentAboutToBeRemoved);
    connect(Session::instance(), &Session::torrentsUpdated, this, &TransferListModel::handleTorrentsUpdated);

    connect(Session::instance(), &Session::torrentFinished, this, &TransferListModel::handleTorrentStatusUpdated);
    connect(Session::instance(), &Session::torrentMetadataReceived, this, &TransferListModel::handleTorrentStatusUpdated);
    connect(Session::instance(), &Session::torrentResumed, this, &TransferListModel::handleTorrentStatusUpdated);
    connect(Session::instance(), &Session::torrentPaused, this, &TransferListModel::handleTorrentStatusUpdated);
    connect(Session::instance(), &Session::torrentFinishedChecking, this, &TransferListModel::handleTorrentStatusUpdated);
}

int TransferListModel::rowCount(const QModelIndex &) const
{
    return m_torrentList.size();
}

int TransferListModel::columnCount(const QModelIndex &) const
{
    return NB_COLUMNS;
}

QVariant TransferListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal)
    {
        if (role == Qt::DisplayRole)
        {
            switch (section)
            {
            case TR_QUEUE_POSITION: return QChar('#');
            case TR_NAME: return tr("Name", "i.e: torrent name");
            case TR_SIZE: return tr("Size", "i.e: torrent size");
            case TR_PROGRESS: return tr("Progress", "% Done");
            case TR_STATUS: return tr("Status", "Torrent status (e.g. downloading, seeding, paused)");
            case TR_SEEDS: return tr("Seeds", "i.e. full sources (often untranslated)");
            case TR_PEERS: return tr("Peers", "i.e. partial sources (often untranslated)");
            case TR_DLSPEED: return tr("Down Speed", "i.e: Download speed");
            case TR_UPSPEED: return tr("Up Speed", "i.e: Upload speed");
            case TR_RATIO: return tr("Ratio", "Share ratio");
            case TR_ETA: return tr("ETA", "i.e: Estimated Time of Arrival / Time left");
            case TR_CATEGORY: return tr("Category");
            case TR_TAGS: return tr("Tags");
            case TR_ADD_DATE: return tr("Added On", "Torrent was added to transfer list on 01/01/2010 08:00");
            case TR_SEED_DATE: return tr("Completed On", "Torrent was completed on 01/01/2010 08:00");
            case TR_TRACKER: return tr("Tracker");
            case TR_DLLIMIT: return tr("Down Limit", "i.e: Download limit");
            case TR_UPLIMIT: return tr("Up Limit", "i.e: Upload limit");
            case TR_AMOUNT_DOWNLOADED: return tr("Downloaded", "Amount of data downloaded (e.g. in MB)");
            case TR_AMOUNT_UPLOADED: return tr("Uploaded", "Amount of data uploaded (e.g. in MB)");
            case TR_AMOUNT_DOWNLOADED_SESSION: return tr("Session Download", "Amount of data downloaded since program open (e.g. in MB)");
            case TR_AMOUNT_UPLOADED_SESSION: return tr("Session Upload", "Amount of data uploaded since program open (e.g. in MB)");
            case TR_AMOUNT_LEFT: return tr("Remaining", "Amount of data left to download (e.g. in MB)");
            case TR_TIME_ELAPSED: return tr("Time Active", "Time (duration) the torrent is active (not paused)");
            case TR_SAVE_PATH: return tr("Save path", "Torrent save path");
            case TR_COMPLETED: return tr("Completed", "Amount of data completed (e.g. in MB)");
            case TR_RATIO_LIMIT: return tr("Ratio Limit", "Upload share ratio limit");
            case TR_SEEN_COMPLETE_DATE: return tr("Last Seen Complete", "Indicates the time when the torrent was last seen complete/whole");
            case TR_LAST_ACTIVITY: return tr("Last Activity", "Time passed since a chunk was downloaded/uploaded");
            case TR_TOTAL_SIZE: return tr("Total Size", "i.e. Size including unwanted data");
            case TR_AVAILABILITY: return tr("Availability", "The number of distributed copies of the torrent");
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
            case TR_QUEUE_POSITION:
            case TR_LAST_ACTIVITY:
            case TR_AVAILABILITY:
                return QVariant(Qt::AlignRight | Qt::AlignVCenter);
            default:
                return QAbstractListModel::headerData(section, orientation, role);
            }
        }
    }

    return {};
}

QString TransferListModel::displayValue(const BitTorrent::TorrentHandle *torrent, const int column) const
{
    bool hideValues = false;
    if (m_hideZeroValuesMode == HideZeroValuesMode::Always)
        hideValues = true;
    else if (m_hideZeroValuesMode != HideZeroValuesMode::Never)
        hideValues = (torrent->state() == BitTorrent::TorrentState::PausedDownloading);

    const auto availabilityString = [hideValues](const qreal value) -> QString
    {
        return (hideValues && (value <= 0))
                ? QString {} : Utils::String::fromDouble(value, 3);
    };

    const auto unitString = [hideValues](const qint64 value, const bool isSpeedUnit = false) -> QString
    {
        return (hideValues && (value == 0))
                ? QString {} : Utils::Misc::friendlyUnit(value, isSpeedUnit);
    };

    const auto limitString = [hideValues](const qint64 value) -> QString
    {
        if (hideValues && (value == 0))
            return {};

        return (value > 0)
                ? Utils::Misc::friendlyUnit(value, true)
                : QString::fromUtf8(C_INFINITY);
    };

    const auto amountString = [hideValues](const qint64 value, const qint64 total) -> QString
    {
        return (hideValues && (value == 0) && (total == 0))
                ? QString {}
                : QString::number(value) + " (" + QString::number(total) + ')';
    };

    const auto ratioString = [hideValues](const qreal value) -> QString
    {
        if (hideValues && (value <= 0))
            return {};

         return ((static_cast<int>(value) == -1) || (value > BitTorrent::TorrentHandle::MAX_RATIO))
                  ? QString::fromUtf8(C_INFINITY) : Utils::String::fromDouble(value, 2);
    };

    const auto queuePositionString = [](const qint64 value) -> QString
    {
        return (value > 0) ? QString::number(value) : "*";
    };

    const auto lastActivityString = [hideValues](qint64 value) -> QString
    {
        if (hideValues && ((value < 0) || (value >= MAX_ETA)))
            return QString {};

        // Show '< 1m ago' when elapsed time is 0
        if (value == 0)
            value = 1;

        return (value >= 0)
            ? tr("%1 ago", "e.g.: 1h 20m ago").arg(Utils::Misc::userFriendlyDuration(value))
            : Utils::Misc::userFriendlyDuration(value);
    };

    const auto timeElapsedString = [](const qint64 elapsedTime, const qint64 seedingTime) -> QString
    {
        if (seedingTime <= 0)
            return Utils::Misc::userFriendlyDuration(elapsedTime);

        return tr("%1 (seeded for %2)", "e.g. 4m39s (seeded for 3m10s)")
                .arg(Utils::Misc::userFriendlyDuration(elapsedTime)
                     , Utils::Misc::userFriendlyDuration(seedingTime));
    };

    const auto tagsString = [](const QSet<QString> &tags) -> QString
    {
        QStringList tagsList = tags.values();
        tagsList.sort();
        return tagsList.join(", ");
    };

    const auto progressString = [](const qreal progress) -> QString
    {
        return (progress >= 1)
                ? QString::fromLatin1("100%")
                : Utils::String::fromDouble((progress * 100), 1) + QLatin1Char('%');
    };

    const auto statusString = [this](const BitTorrent::TorrentState state, const QString &errorMessage) -> QString
    {
        return (state == BitTorrent::TorrentState::Error)
                   ? m_statusStrings[state] + ": " + errorMessage
                   : m_statusStrings[state];
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
        return Utils::Misc::userFriendlyDuration(torrent->eta(), MAX_ETA);
    case TR_RATIO:
        return ratioString(torrent->realRatio());
    case TR_RATIO_LIMIT:
        return ratioString(torrent->maxRatio());
    case TR_CATEGORY:
        return torrent->category();
    case TR_TAGS:
        return tagsString(torrent->tags());
    case TR_ADD_DATE:
        return torrent->addedTime().toLocalTime().toString(Qt::DefaultLocaleShortDate);
    case TR_SEED_DATE:
        return torrent->completedTime().toLocalTime().toString(Qt::DefaultLocaleShortDate);
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
        return timeElapsedString(torrent->activeTime(), torrent->seedingTime());
    case TR_SAVE_PATH:
        return Utils::Fs::toNativePath(torrent->savePath());
    case TR_COMPLETED:
        return unitString(torrent->completedSize());
    case TR_SEEN_COMPLETE_DATE:
        return torrent->lastSeenComplete().toLocalTime().toString(Qt::DefaultLocaleShortDate);
    case TR_LAST_ACTIVITY:
        return lastActivityString((torrent->isPaused() || torrent->isChecking()) ? -1 : torrent->timeSinceActivity());
    case TR_AVAILABILITY:
        return availabilityString(torrent->distributedCopies());
    case TR_TOTAL_SIZE:
        return unitString(torrent->totalSize());
    }

    return {};
}

QVariant TransferListModel::internalValue(const BitTorrent::TorrentHandle *torrent, const int column, const bool alt) const
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
    case TR_CATEGORY:
        return torrent->category();
    case TR_TAGS:
        return QStringList {torrent->tags().values()};
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
        return !alt ? torrent->activeTime() : torrent->seedingTime();
    case TR_SAVE_PATH:
        return Utils::Fs::toNativePath(torrent->savePath());
    case TR_COMPLETED:
        return torrent->completedSize();
    case TR_RATIO_LIMIT:
        return torrent->maxRatio();
    case TR_SEEN_COMPLETE_DATE:
        return torrent->lastSeenComplete();
    case TR_LAST_ACTIVITY:
        return (torrent->isPaused() || torrent->isChecking()) ? -1 : torrent->timeSinceActivity();
    case TR_AVAILABILITY:
        return torrent->distributedCopies();
    case TR_TOTAL_SIZE:
        return torrent->totalSize();
    }

    return {};
}

QVariant TransferListModel::data(const QModelIndex &index, const int role) const
{
    if (!index.isValid()) return {};

    const BitTorrent::TorrentHandle *torrent = m_torrentList.value(index.row());
    if (!torrent) return {};

    switch (role)
    {
    case Qt::ForegroundRole:
        return m_stateThemeColors.value(torrent->state(), getDefaultColorByState(torrent->state()));
    case Qt::DisplayRole:
        return displayValue(torrent, index.column());
    case UnderlyingDataRole:
        return internalValue(torrent, index.column());
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
            return displayValue(torrent, index.column());
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
        case TR_QUEUE_POSITION:
        case TR_LAST_ACTIVITY:
        case TR_AVAILABILITY:
            return QVariant {Qt::AlignRight | Qt::AlignVCenter};
        }
    }

    return {};
}

bool TransferListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || (role != Qt::DisplayRole)) return false;

    BitTorrent::TorrentHandle *const torrent = m_torrentList.value(index.row());
    if (!torrent) return false;

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

void TransferListModel::addTorrent(BitTorrent::TorrentHandle *const torrent)
{
    Q_ASSERT(!m_torrentMap.contains(torrent));

    const int row = m_torrentList.size();

    beginInsertRows({}, row, row);
    m_torrentList << torrent;
    m_torrentMap[torrent] = row;
    endInsertRows();
}

Qt::ItemFlags TransferListModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;

    // Explicitly mark as editable
    return QAbstractListModel::flags(index) | Qt::ItemIsEditable;
}

BitTorrent::TorrentHandle *TransferListModel::torrentHandle(const QModelIndex &index) const
{
    if (!index.isValid()) return nullptr;

    return m_torrentList.value(index.row());
}

void TransferListModel::handleTorrentAboutToBeRemoved(BitTorrent::TorrentHandle *const torrent)
{
    const int row = m_torrentMap.value(torrent, -1);
    Q_ASSERT(row >= 0);

    beginRemoveRows({}, row, row);
    m_torrentList.removeAt(row);
    m_torrentMap.remove(torrent);
    for (int &value : m_torrentMap)
    {
        if (value > row)
            --value;
    }
    endRemoveRows();
}

void TransferListModel::handleTorrentStatusUpdated(BitTorrent::TorrentHandle *const torrent)
{
    const int row = m_torrentMap.value(torrent, -1);
    Q_ASSERT(row >= 0);

    emit dataChanged(index(row, 0), index(row, columnCount() - 1));
}

void TransferListModel::handleTorrentsUpdated(const QVector<BitTorrent::TorrentHandle *> &torrents)
{
    const int columns = (columnCount() - 1);

    if (torrents.size() <= (m_torrentList.size() * 0.5))
    {
        for (BitTorrent::TorrentHandle *const torrent : torrents)
        {
            const int row = m_torrentMap.value(torrent, -1);
            Q_ASSERT(row >= 0);

            emit dataChanged(index(row, 0), index(row, columns));
        }
    }
    else
    {
        // save the overhead when more than half of the torrent list needs update
        emit dataChanged(index(0, 0), index((rowCount() - 1), columns));
    }
}

void TransferListModel::configure()
{
    const Preferences *pref = Preferences::instance();

    HideZeroValuesMode hideZeroValuesMode = HideZeroValuesMode::Never;
    if (pref->getHideZeroValues())
    {
        if (pref->getHideZeroComboValues() == 1)
            hideZeroValuesMode = HideZeroValuesMode::Paused;
        else
            hideZeroValuesMode = HideZeroValuesMode::Always;
    }

    if (m_hideZeroValuesMode != hideZeroValuesMode)
    {
        m_hideZeroValuesMode = hideZeroValuesMode;
        emit dataChanged(index(0, 0), index((rowCount() - 1), (columnCount() - 1)));
    }
}

// Static functions

QIcon getIconByState(const BitTorrent::TorrentState state)
{
    switch (state)
    {
    case BitTorrent::TorrentState::Downloading:
    case BitTorrent::TorrentState::ForcedDownloading:
    case BitTorrent::TorrentState::DownloadingMetadata:
        return getDownloadingIcon();
    case BitTorrent::TorrentState::StalledDownloading:
        return getStalledDownloadingIcon();
    case BitTorrent::TorrentState::StalledUploading:
        return getStalledUploadingIcon();
    case BitTorrent::TorrentState::Uploading:
    case BitTorrent::TorrentState::ForcedUploading:
        return getUploadingIcon();
    case BitTorrent::TorrentState::PausedDownloading:
        return getPausedIcon();
    case BitTorrent::TorrentState::PausedUploading:
        return getCompletedIcon();
    case BitTorrent::TorrentState::QueuedDownloading:
    case BitTorrent::TorrentState::QueuedUploading:
        return getQueuedIcon();
    case BitTorrent::TorrentState::CheckingDownloading:
    case BitTorrent::TorrentState::CheckingUploading:
    case BitTorrent::TorrentState::CheckingResumeData:
    case BitTorrent::TorrentState::Moving:
        return getCheckingIcon();
    case BitTorrent::TorrentState::Unknown:
    case BitTorrent::TorrentState::MissingFiles:
    case BitTorrent::TorrentState::Error:
        return getErrorIcon();
    default:
        Q_ASSERT(false);
        return getErrorIcon();
    }
}

QColor getDefaultColorByState(const BitTorrent::TorrentState state)
{
    // Color names taken from http://cloford.com/resources/colours/500col.htm
    bool dark = isDarkTheme();

    switch (state)
    {
    case BitTorrent::TorrentState::Downloading:
    case BitTorrent::TorrentState::ForcedDownloading:
    case BitTorrent::TorrentState::DownloadingMetadata:
        if (!dark)
            return {34, 139, 34}; // Forest Green
        else
            return {50, 205, 50}; // Lime Green
    case BitTorrent::TorrentState::StalledDownloading:
    case BitTorrent::TorrentState::StalledUploading:
        if (!dark)
            return {0, 0, 0}; // Black
        else
            return {204, 204, 204}; // Gray 80
    case BitTorrent::TorrentState::Uploading:
    case BitTorrent::TorrentState::ForcedUploading:
        if (!dark)
            return {65, 105, 225}; // Royal Blue
        else
            return {99, 184, 255}; // Steel Blue 1
    case BitTorrent::TorrentState::PausedDownloading:
        return {250, 128, 114}; // Salmon
    case BitTorrent::TorrentState::PausedUploading:
        if (!dark)
            return {0, 0, 139}; // Dark Blue
        else
            return {79, 148, 205}; // Steel Blue 3
    case BitTorrent::TorrentState::Error:
    case BitTorrent::TorrentState::MissingFiles:
        return {255, 0, 0}; // red
    case BitTorrent::TorrentState::QueuedDownloading:
    case BitTorrent::TorrentState::QueuedUploading:
    case BitTorrent::TorrentState::CheckingDownloading:
    case BitTorrent::TorrentState::CheckingUploading:
    case BitTorrent::TorrentState::CheckingResumeData:
    case BitTorrent::TorrentState::Moving:
        if (!dark)
            return {0, 128, 128}; // Teal
        else
            return {0, 205, 205}; // Cyan 3
    case BitTorrent::TorrentState::Unknown:
        return {255, 0, 0}; // red
    default:
        Q_ASSERT(false);
        return {255, 0, 0}; // red
    }
}

QIcon getPausedIcon()
{
    static QIcon cached = UIThemeManager::instance()->getIcon(QLatin1String("paused"));
    return cached;
}

QIcon getQueuedIcon()
{
    static QIcon cached = UIThemeManager::instance()->getIcon(QLatin1String("queued"));
    return cached;
}

QIcon getDownloadingIcon()
{
    static QIcon cached = UIThemeManager::instance()->getIcon(QLatin1String("downloading"));
    return cached;
}

QIcon getStalledDownloadingIcon()
{
    static QIcon cached = UIThemeManager::instance()->getIcon(QLatin1String("stalledDL"));
    return cached;
}

QIcon getUploadingIcon()
{
    static QIcon cached = UIThemeManager::instance()->getIcon(QLatin1String("uploading"));
    return cached;
}

QIcon getStalledUploadingIcon()
{
    static QIcon cached = UIThemeManager::instance()->getIcon(QLatin1String("stalledUP"));
    return cached;
}

QIcon getCompletedIcon()
{
    static QIcon cached = UIThemeManager::instance()->getIcon(QLatin1String("completed"));
    return cached;
}

QIcon getCheckingIcon()
{
    static QIcon cached = UIThemeManager::instance()->getIcon(QLatin1String("checking"));
    return cached;
}

QIcon getErrorIcon()
{
    static QIcon cached = UIThemeManager::instance()->getIcon(QLatin1String("error"));
    return cached;
}

bool isDarkTheme()
{
    const QPalette pal = QApplication::palette();
    // QPalette::Base is used for the background of the Treeview
    const QColor &color = pal.color(QPalette::Active, QPalette::Base);
    return (color.lightness() < 127);
}
