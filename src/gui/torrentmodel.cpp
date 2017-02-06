/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017  Tim Delaney <timothy.c.delaney@gmail.com>
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
 *
 * Contact : chris@qbittorrent.org
 */

#include <QColor>
#include <QDebug>
#include <QIcon>
#include <QLatin1String>
#include <QList>
#include <QString>

#include "base/bittorrent/session.h"
#include "base/bittorrent/torrenthandle.h"
#include "base/torrentfilter.h"
#include "base/utils/fs.h"
#include "guiiconprovider.h"
#include "torrentmodel.h"

// TorrentModel

TorrentModel::TorrentModel(QObject *parent)
    : QAbstractListModel(parent)
{
    // Load the torrents
    foreach (BitTorrent::TorrentHandle *const torrent, BitTorrent::Session::instance()->torrents())
        addTorrent(torrent);

    // Listen for torrent changes
    connect(BitTorrent::Session::instance(), SIGNAL(torrentAdded(BitTorrent::TorrentHandle * const)), SLOT(addTorrent(BitTorrent::TorrentHandle * const)));
    connect(BitTorrent::Session::instance(), SIGNAL(torrentAboutToBeRemoved(BitTorrent::TorrentHandle * const)), SLOT(handleTorrentAboutToBeRemoved(BitTorrent::TorrentHandle * const)));
    connect(BitTorrent::Session::instance(), SIGNAL(torrentsUpdated()), SLOT(handleTorrentsUpdated()));

    connect(BitTorrent::Session::instance(), SIGNAL(torrentFinished(BitTorrent::TorrentHandle * const)), SLOT(handleTorrentStatusUpdated(BitTorrent::TorrentHandle * const)));
    connect(BitTorrent::Session::instance(), SIGNAL(torrentMetadataLoaded(BitTorrent::TorrentHandle * const)), SLOT(handleTorrentStatusUpdated(BitTorrent::TorrentHandle * const)));
    connect(BitTorrent::Session::instance(), SIGNAL(torrentResumed(BitTorrent::TorrentHandle * const)), SLOT(handleTorrentStatusUpdated(BitTorrent::TorrentHandle * const)));
    connect(BitTorrent::Session::instance(), SIGNAL(torrentPaused(BitTorrent::TorrentHandle * const)), SLOT(handleTorrentStatusUpdated(BitTorrent::TorrentHandle * const)));
    connect(BitTorrent::Session::instance(), SIGNAL(torrentFinishedChecking(BitTorrent::TorrentHandle * const)), SLOT(handleTorrentStatusUpdated(BitTorrent::TorrentHandle * const)));
}

int TorrentModel::rowCount(const QModelIndex &index) const
{
    Q_UNUSED(index);
    return m_torrents.size();
}

int TorrentModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return NB_COLUMNS;
}

QVariant TorrentModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal) {
        if (role == Qt::DisplayRole) {
            switch (section) {
            case TR_PRIORITY: return "#";
            case TR_NAME: return tr("Name", "i.e: torrent name");
            case TR_SIZE: return tr("Size", "i.e: torrent size");
            case TR_PROGRESS: return tr("Done", "% Done");
            case TR_STATUS: return tr("Status", "Torrent status (e.g. downloading, seeding, paused)");
            case TR_SEEDS: return tr("Seeds", "i.e. full sources (often untranslated)");
            case TR_PEERS: return tr("Peers", "i.e. partial sources (often untranslated)");
            case TR_DLSPEED: return tr("Down Speed", "i.e: Download speed");
            case TR_UPSPEED: return tr("Up Speed", "i.e: Upload speed");
            case TR_RATIO: return tr("Ratio", "Share ratio");
            case TR_ETA: return tr("ETA", "i.e: Estimated Time of Arrival / Time left");
            case TR_CATEGORY: return tr("Category");
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
            default:
                return QVariant();
            }
        }
        else if (role == Qt::TextAlignmentRole) {
            switch (section) {
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
            case TR_PRIORITY:
            case TR_LAST_ACTIVITY:
                return QVariant(Qt::AlignRight | Qt::AlignVCenter);
            default:
                return QAbstractListModel::headerData(section, orientation, role);
            }
        }
    }

    return QVariant();
}

QVariant TorrentModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) return QVariant();

    BitTorrent::TorrentHandle *const torrent = m_torrents.value(index.row());
    if (!torrent) return QVariant();

    if ((role == Qt::DecorationRole) && (index.column() == TR_NAME))
        return getIconByState(torrent->state());

    if (role == Qt::ForegroundRole)
        return getColorByState(torrent->state());

    if ((role != Qt::DisplayRole) && (role != Qt::UserRole))
        return QVariant();

    switch (index.column()) {
    case TR_NAME:
        return torrent->name();
    case TR_PRIORITY:
        return torrent->queuePosition();
    case TR_SIZE:
        return torrent->wantedSize();
    case TR_PROGRESS:
        return torrent->progress();
    case TR_STATUS:
        return static_cast<int>(torrent->state());
    case TR_SEEDS:
        return (role == Qt::DisplayRole) ? torrent->seedsCount() : torrent->totalSeedsCount();
    case TR_PEERS:
        return (role == Qt::DisplayRole) ? torrent->leechsCount() : torrent->totalLeechersCount();
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
        return torrent->incompletedSize();
    case TR_TIME_ELAPSED:
        return (role == Qt::DisplayRole) ? torrent->activeTime() : torrent->seedingTime();
    case TR_SAVE_PATH:
        return Utils::Fs::toNativePath(torrent->savePath());
    case TR_COMPLETED:
        return torrent->completedSize();
    case TR_RATIO_LIMIT:
        return torrent->maxRatio();
    case TR_SEEN_COMPLETE_DATE:
        return torrent->lastSeenComplete();
    case TR_LAST_ACTIVITY:
        if (torrent->isPaused() || torrent->isChecking())
            return -1;
        return torrent->timeSinceActivity();
    case TR_TOTAL_SIZE:
        return torrent->totalSize();
    default:
        return QVariant();
    }

    return QVariant();
}

bool TorrentModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    qDebug() << Q_FUNC_INFO << value;
    if (!index.isValid() || (role != Qt::DisplayRole)) return false;

    qDebug("Index is valid and role is DisplayRole");
    BitTorrent::TorrentHandle *const torrent = m_torrents.value(index.row());
    if (!torrent) return false;

    // Category, seed date and Name columns can be edited
    switch (index.column()) {
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

void TorrentModel::addTorrent(BitTorrent::TorrentHandle *const torrent)
{
    if (m_torrents.indexOf(torrent) == -1) {
        const int row = m_torrents.size();
        beginInsertRows(QModelIndex(), row, row);
        m_torrents << torrent;
        endInsertRows();
    }
}

Qt::ItemFlags TorrentModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) return 0;

    // Explicitly mark as editable
    return QAbstractListModel::flags(index) | Qt::ItemIsEditable;
}

BitTorrent::TorrentHandle *TorrentModel::torrentHandle(const QModelIndex &index) const
{
    if (!index.isValid()) return 0;

    return m_torrents.value(index.row());
}

void TorrentModel::handleTorrentAboutToBeRemoved(BitTorrent::TorrentHandle *const torrent)
{
    const int row = m_torrents.indexOf(torrent);
    if (row >= 0) {
        beginRemoveRows(QModelIndex(), row, row);
        m_torrents.removeAt(row);
        endRemoveRows();
    }
}

void TorrentModel::handleTorrentStatusUpdated(BitTorrent::TorrentHandle *const torrent)
{
    const int row = m_torrents.indexOf(torrent);
    if (row >= 0)
        emit dataChanged(index(row, 0), index(row, columnCount() - 1));
}

void TorrentModel::handleTorrentsUpdated()
{
    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
}

// Static functions

static const QLatin1String downloading("downloading");
static const QLatin1String stalledDL("stalledDL");
static const QLatin1String stalledUP("stalledUP");
static const QLatin1String uploading("uploading");
static const QLatin1String paused("paused");
static const QLatin1String completed("completed");
static const QLatin1String queued("queued");
static const QLatin1String checking("checking");
static const QLatin1String error("error");

QString TorrentModel::getIconId(const BitTorrent::TorrentState &state)
{
    switch (state) {
    case BitTorrent::TorrentState::Downloading:
    case BitTorrent::TorrentState::ForcedDownloading:
    case BitTorrent::TorrentState::DownloadingMetadata:
        return downloading;
    case BitTorrent::TorrentState::Allocating:
    case BitTorrent::TorrentState::StalledDownloading:
        return stalledDL;
    case BitTorrent::TorrentState::StalledUploading:
        return stalledUP;
    case BitTorrent::TorrentState::Uploading:
    case BitTorrent::TorrentState::ForcedUploading:
        return uploading;
    case BitTorrent::TorrentState::PausedDownloading:
        return paused;
    case BitTorrent::TorrentState::PausedUploading:
        return completed;
    case BitTorrent::TorrentState::QueuedDownloading:
    case BitTorrent::TorrentState::QueuedUploading:
        return queued;
    case BitTorrent::TorrentState::CheckingDownloading:
    case BitTorrent::TorrentState::CheckingUploading:
    case BitTorrent::TorrentState::QueuedForChecking:
    case BitTorrent::TorrentState::CheckingResumeData:
        return checking;
    case BitTorrent::TorrentState::Unknown:
    case BitTorrent::TorrentState::MissingFiles:
    case BitTorrent::TorrentState::Error:
        return error;
    default:
        Q_ASSERT(false);
        return error;
    }
}

QIcon TorrentModel::getIconByState(const BitTorrent::TorrentState &state)
{
    return GuiIconProvider::instance()->getStatusIcon(getIconId(state), state);
}

// Color names taken from http://cloford.com/resources/colours/500col.htm
static const QColor black(0, 0, 0);
static const QColor cyan3(0, 205, 205);
static const QColor darkBlue(0, 0, 139);
static const QColor forestGreen(34, 139, 34);
static const QColor gray80(204, 204, 204);
static const QColor limeGreen(50, 205, 50);
static const QColor maroon(128, 0, 0);
static const QColor red(255, 0, 0);
static const QColor royalBlue(65, 105, 225);
static const QColor salmon(250, 128, 114);
static const QColor steelBlue1(99, 184, 255);
static const QColor steelBlue3(79, 148, 205);
static const QColor teal(0, 128, 128);

QColor TorrentModel::getColorByState(const BitTorrent::TorrentState &state)
{
    bool dark = GuiIconProvider::instance()->getThemeFlags() & GuiIconProvider::DarkTheme;

    switch (state) {
    case BitTorrent::TorrentState::Downloading:
    case BitTorrent::TorrentState::ForcedDownloading:
    case BitTorrent::TorrentState::DownloadingMetadata:
        if (!dark)
            return forestGreen;
        else
            return limeGreen;
    case BitTorrent::TorrentState::Allocating:
    case BitTorrent::TorrentState::StalledDownloading:
        if (!dark)
            return maroon;
        else
            return salmon;
    case BitTorrent::TorrentState::Uploading:
    case BitTorrent::TorrentState::ForcedUploading:
    case BitTorrent::TorrentState::StalledUploading:
        if (!dark)
            return royalBlue;
        else
            return steelBlue1;
    case BitTorrent::TorrentState::PausedDownloading:
        if (!dark)
            return black;
        else
            return gray80;
    case BitTorrent::TorrentState::PausedUploading:
        if (!dark)
            return darkBlue;
        else
            return steelBlue3;
    case BitTorrent::TorrentState::Error:
    case BitTorrent::TorrentState::MissingFiles:
        return red;
    case BitTorrent::TorrentState::QueuedDownloading:
    case BitTorrent::TorrentState::QueuedUploading:
    case BitTorrent::TorrentState::CheckingDownloading:
    case BitTorrent::TorrentState::CheckingUploading:
    case BitTorrent::TorrentState::QueuedForChecking:
    case BitTorrent::TorrentState::CheckingResumeData:
        if (!dark)
            return teal;
        else
            return cyan3;
    case BitTorrent::TorrentState::Unknown:
        return red;
    default:
        Q_ASSERT(false);
        return red;
    }
}
