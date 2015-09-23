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
 *
 * Contact : chris@qbittorrent.org
 */

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QIcon>
#include <QPalette>
#include <QProcessEnvironment>
#include <QSettings>

#include "core/bittorrent/session.h"
#include "core/bittorrent/torrenthandle.h"
#include "core/torrentfilter.h"
#include "core/utils/fs.h"
#include "torrentmodel.h"

static QIcon getIconByState(BitTorrent::TorrentState state);
static QColor getColorByState(BitTorrent::TorrentState state);

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

namespace {
class TorrentStateColorsImpl {
public:
    TorrentStateColorsImpl();
    QColor color(BitTorrent::TorrentState state)
    {
        return stateColors.value(state, Qt::red);
    }
private:
    void setDefaultColors();
#ifdef Q_OS_UNIX
    bool readKDEColors(const QProcessEnvironment& env);
    static QColor decodeKDEColor(const QVariant& v, bool &ok);
#endif
    QMap<BitTorrent::TorrentState, QColor> stateColors;
};

Q_GLOBAL_STATIC(TorrentStateColorsImpl, TorrentStateColors)

}

// TorrentModel

TorrentModel::TorrentModel(QObject *parent)
    : QAbstractListModel(parent)
{
    // Load the torrents
    foreach (BitTorrent::TorrentHandle *const torrent, BitTorrent::Session::instance()->torrents())
        addTorrent(torrent);

    // Listen for torrent changes
    connect(BitTorrent::Session::instance(), SIGNAL(torrentAdded(BitTorrent::TorrentHandle *const)), SLOT(addTorrent(BitTorrent::TorrentHandle *const)));
    connect(BitTorrent::Session::instance(), SIGNAL(torrentAboutToBeRemoved(BitTorrent::TorrentHandle *const)), SLOT(handleTorrentAboutToBeRemoved(BitTorrent::TorrentHandle *const)));
    connect(BitTorrent::Session::instance(), SIGNAL(torrentStatusUpdated(BitTorrent::TorrentHandle *const)), this, SLOT(handleTorrentStatusUpdated(BitTorrent::TorrentHandle *const)));

    connect(BitTorrent::Session::instance(), SIGNAL(torrentFinished(BitTorrent::TorrentHandle *const)), SLOT(handleTorrentStatusUpdated(BitTorrent::TorrentHandle *const)));
    connect(BitTorrent::Session::instance(), SIGNAL(torrentMetadataLoaded(BitTorrent::TorrentHandle *const)), SLOT(handleTorrentStatusUpdated(BitTorrent::TorrentHandle *const)));
    connect(BitTorrent::Session::instance(), SIGNAL(torrentResumed(BitTorrent::TorrentHandle *const)), SLOT(handleTorrentStatusUpdated(BitTorrent::TorrentHandle *const)));
    connect(BitTorrent::Session::instance(), SIGNAL(torrentPaused(BitTorrent::TorrentHandle *const)), SLOT(handleTorrentStatusUpdated(BitTorrent::TorrentHandle *const)));
    connect(BitTorrent::Session::instance(), SIGNAL(torrentFinishedChecking(BitTorrent::TorrentHandle *const)), SLOT(handleTorrentStatusUpdated(BitTorrent::TorrentHandle *const)));
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
            switch(section) {
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
            case TR_LABEL: return tr("Label");
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
            switch(section) {
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

    switch(index.column()) {
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
        return (role == Qt::DisplayRole) ? torrent->seedsCount() : torrent->completeCount();
    case TR_PEERS:
        return (role == Qt::DisplayRole) ? (torrent->peersCount() - torrent->seedsCount()) : torrent->incompleteCount();
    case TR_DLSPEED:
        return torrent->downloadPayloadRate();
    case TR_UPSPEED:
        return torrent->uploadPayloadRate();
    case TR_ETA:
        return torrent->eta();
    case TR_RATIO:
        return torrent->realRatio();
    case TR_LABEL:
        return torrent->label();
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
        return Utils::Fs::toNativePath(torrent->rootPath());
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
        return  torrent->totalSize();
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

    // Label, seed date and Name columns can be edited
    switch(index.column()) {
    case TR_NAME:
        torrent->setName(value.toString());
        break;
    case TR_LABEL:
        torrent->setLabel(value.toString());
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

// Static functions

QIcon getIconByState(BitTorrent::TorrentState state)
{
    switch (state) {
    case BitTorrent::TorrentState::Downloading:
    case BitTorrent::TorrentState::ForcedDownloading:
    case BitTorrent::TorrentState::DownloadingMetadata:
        return getDownloadingIcon();
    case BitTorrent::TorrentState::Allocating:
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
    case BitTorrent::TorrentState::QueuedForChecking:
    case BitTorrent::TorrentState::CheckingResumeData:
        return getCheckingIcon();
    case BitTorrent::TorrentState::Unknown:
    case BitTorrent::TorrentState::Error:
        return getErrorIcon();
    default:
        Q_ASSERT(false);
        return getErrorIcon();
    }
}

QColor getColorByState(BitTorrent::TorrentState state)
{
   return TorrentStateColors()->color(state);
}

QIcon getPausedIcon()
{
    static QIcon cached = QIcon(":/icons/skin/paused.png");
    return cached;
}

QIcon getQueuedIcon()
{
    static QIcon cached = QIcon(":/icons/skin/queued.png");
    return cached;
}

QIcon getDownloadingIcon()
{
    static QIcon cached = QIcon(":/icons/skin/downloading.png");
    return cached;
}

QIcon getStalledDownloadingIcon()
{
    static QIcon cached = QIcon(":/icons/skin/stalledDL.png");
    return cached;
}

QIcon getUploadingIcon()
{
    static QIcon cached = QIcon(":/icons/skin/uploading.png");
    return cached;
}

QIcon getStalledUploadingIcon()
{
    static QIcon cached = QIcon(":/icons/skin/stalledUP.png");
    return cached;
}

QIcon getCompletedIcon()
{
    static QIcon cached = QIcon(":/icons/skin/completed.png");
    return cached;
}

QIcon getCheckingIcon()
{
    static QIcon cached = QIcon(":/icons/skin/checking.png");
    return cached;
}

QIcon getErrorIcon()
{
    static QIcon cached = QIcon(":/icons/skin/error.png");
    return cached;
}

bool isDarkTheme()
{
    QPalette pal = QApplication::palette();
    // QPalette::Base is used for the background of the Treeview
    QColor color = pal.color(QPalette::Active, QPalette::Base);
    return (color.lightness() < 127);
}

TorrentStateColorsImpl::TorrentStateColorsImpl()
{
    QProcessEnvironment env(QProcessEnvironment::systemEnvironment());
    bool specificColoursRead = false;
#ifdef Q_OS_UNIX
    if (env.contains(QLatin1String("XDG_CURRENT_DESKTOP"))) {
        QString desktop = env.value(QLatin1String("XDG_CURRENT_DESKTOP"));
        if (desktop == QLatin1String("KDE")) {
            specificColoursRead = readKDEColors(env);
        }
    }
#endif
    if (!specificColoursRead) {
        setDefaultColors();
    }
}

void TorrentStateColorsImpl::setDefaultColors()
{
    using BitTorrent::TorrentState;
    bool dark = isDarkTheme();

    stateColors[TorrentState::Downloading] =
    stateColors[TorrentState::ForcedDownloading] =
    stateColors[TorrentState::DownloadingMetadata] =
         QColor(34, 139, 34); // Forest Green

    stateColors[TorrentState::Allocating] =
    stateColors[TorrentState::StalledDownloading] =
    stateColors[TorrentState::StalledUploading] =
        !dark ? QColor(0, 0, 0) : // Black
                QColor(255, 255, 255); // White

    stateColors[TorrentState::Uploading] =
    stateColors[TorrentState::ForcedUploading] =
        !dark ? QColor(65, 105, 225) :// Royal Blue
               QColor(99, 184, 255); // Steel Blue 1

    stateColors[TorrentState::PausedDownloading] =
        QColor(250, 128, 114); // Salmon

    stateColors[TorrentState::PausedUploading] =
        !dark ? QColor(0, 0, 139) : // Dark Blue
            QColor(79, 148, 205); // Steel Blue 3

    stateColors[TorrentState::Error] =
        QColor(255, 0, 0); // red

    stateColors[TorrentState::QueuedDownloading] =
    stateColors[TorrentState::QueuedUploading] =
    stateColors[TorrentState::CheckingDownloading] =
    stateColors[TorrentState::CheckingUploading] =
    stateColors[TorrentState::QueuedForChecking] =
    stateColors[TorrentState::CheckingResumeData] =
        !dark ? QColor(0, 128, 128): // Teal
            QColor(0, 205, 205); // Cyan 3
    stateColors[TorrentState::Unknown] =
        QColor(255, 0, 0); // red
}

#ifdef Q_OS_UNIX
bool TorrentStateColorsImpl::readKDEColors(const QProcessEnvironment& env)
{
    bool versionDecodedOk = false;
    int kdeVersion = env.value(QLatin1String("KDE_SESSION_VERSION"), QLatin1String("4"))
            .toInt(&versionDecodedOk);
    if (!versionDecodedOk) {
        return false;
    }
    QString kdeCfgFile;
    switch (kdeVersion) {
    case 4:
        kdeCfgFile = QLatin1String(".kde4/share/config/kdeglobals");
        break;
    case 5:
        kdeCfgFile = QLatin1String(".config/kdeglobals");
        break;
    default:
        return false;
    }
    QString configPath = QDir(QDir::homePath()).absoluteFilePath(kdeCfgFile);
    if (!QFileInfo(configPath).exists()) {
        return false;
    }

    QSettings kdeGlobals(configPath, QSettings::IniFormat);
    kdeGlobals.beginGroup(QLatin1String("Colors:View"));
    bool allColorsWereReadOk = true;
    bool colorReadOk = false;
    // we currently do not use the following colors:
#if 0
    QColor BackgroundAlternate = decodeKDEColor(kdeGlobals.value(QLatin1String("BackgroundAlternate")), colorReadOk);
    allColorsWereReadOk &= colorReadOk;
    QColor BackgroundNormal = decodeKDEColor(kdeGlobals.value(QLatin1String("BackgroundNormal")), colorReadOk);
    allColorsWereReadOk &= colorReadOk;
    QColor DecorationFocus = decodeKDEColor(kdeGlobals.value(QLatin1String("DecorationFocus")), colorReadOk);
    allColorsWereReadOk &= colorReadOk;
    QColor ForegroundVisited = decodeKDEColor(kdeGlobals.value(QLatin1String("ForegroundVisited")), colorReadOk);
    allColorsWereReadOk &= colorReadOk;
    QColor ForegroundNormal = decodeKDEColor(kdeGlobals.value(QLatin1String("ForegroundNormal")), colorReadOk);
    allColorsWereReadOk &= colorReadOk;
#endif
    QColor DecorationHover = decodeKDEColor(kdeGlobals.value(QLatin1String("DecorationHover")), colorReadOk);
    allColorsWereReadOk &= colorReadOk;
    QColor ForegroundActive = decodeKDEColor(kdeGlobals.value(QLatin1String("ForegroundActive")), colorReadOk);
    allColorsWereReadOk &= colorReadOk;
    QColor ForegroundInactive = decodeKDEColor(kdeGlobals.value(QLatin1String("ForegroundInactive")), colorReadOk);
    allColorsWereReadOk &= colorReadOk;
    QColor ForegroundLink = decodeKDEColor(kdeGlobals.value(QLatin1String("ForegroundLink")), colorReadOk);
    allColorsWereReadOk &= colorReadOk;
    QColor ForegroundNegative = decodeKDEColor(kdeGlobals.value(QLatin1String("ForegroundNegative")), colorReadOk);
    allColorsWereReadOk &= colorReadOk;
    QColor ForegroundNeutral = decodeKDEColor(kdeGlobals.value(QLatin1String("ForegroundNeutral")), colorReadOk);
    allColorsWereReadOk &= colorReadOk;
    QColor ForegroundPositive = decodeKDEColor(kdeGlobals.value(QLatin1String("ForegroundPositive")), colorReadOk);
    allColorsWereReadOk &= colorReadOk;
    kdeGlobals.endGroup();

    if(!allColorsWereReadOk) { // this situation must be extremely rare, therefore we do not check every color
        return false;
    }
    using BitTorrent::TorrentState;

    stateColors[TorrentState::Downloading] =
    stateColors[TorrentState::ForcedDownloading] =
    stateColors[TorrentState::DownloadingMetadata] =
         ForegroundPositive;

    stateColors[TorrentState::Allocating] =
    stateColors[TorrentState::StalledDownloading] =
    stateColors[TorrentState::StalledUploading] =
        ForegroundInactive;

    stateColors[TorrentState::Uploading] =
    stateColors[TorrentState::ForcedUploading] =
        ForegroundNeutral;

    stateColors[TorrentState::PausedDownloading] =
        DecorationHover;

    stateColors[TorrentState::PausedUploading] =
        ForegroundActive;

    stateColors[TorrentState::Error] =
        ForegroundNegative;

    stateColors[TorrentState::QueuedDownloading] =
    stateColors[TorrentState::QueuedUploading] =
    stateColors[TorrentState::CheckingDownloading] =
    stateColors[TorrentState::CheckingUploading] =
    stateColors[TorrentState::QueuedForChecking] =
    stateColors[TorrentState::CheckingResumeData] =
        ForegroundLink;
    stateColors[TorrentState::Unknown] =
        ForegroundNegative;
    return true;
}

QColor TorrentStateColorsImpl::decodeKDEColor(const QVariant& v, bool &ok)
{
    QVariantList components(v.toList());
    if(components.size() == 3) {
        bool rOk, gOk, bOk;
        const int r = components[0].toString().toInt(&rOk);
        const int g = components[1].toString().toInt(&gOk);
        const int b = components[2].toString().toInt(&bOk);
        if (rOk && gOk && bOk) {
            ok = true;
            return QColor::fromRgb(r, g, b);
        }
    }
    ok = false;
    return Qt::black;
}
#endif
