/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2010  Christophe Dumez
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

#include <QDebug>
#include <QApplication>
#include <QPalette>
#include <QIcon>

#include "core/bittorrent/session.h"
#include "core/torrentfilter.h"
#include "core/fs_utils.h"
#include "torrentmodel.h"

namespace {
QIcon get_paused_icon() {
    static QIcon cached = QIcon(":/icons/skin/paused.png");
    return cached;
}

QIcon get_queued_icon() {
    static QIcon cached = QIcon(":/icons/skin/queued.png");
    return cached;
}

QIcon get_downloading_icon() {
    static QIcon cached = QIcon(":/icons/skin/downloading.png");
    return cached;
}

QIcon get_stalled_downloading_icon() {
    static QIcon cached = QIcon(":/icons/skin/stalledDL.png");
    return cached;
}

QIcon get_uploading_icon() {
    static QIcon cached = QIcon(":/icons/skin/uploading.png");
    return cached;
}

QIcon get_stalled_uploading_icon() {
    static QIcon cached = QIcon(":/icons/skin/stalledUP.png");
    return cached;
}

QIcon get_completed_icon() {
    static QIcon cached = QIcon(":/icons/skin/completed.png");
    return cached;
}

QIcon get_checking_icon() {
    static QIcon cached = QIcon(":/icons/skin/checking.png");
    return cached;
}

QIcon get_error_icon() {
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
}

TorrentModelItem::TorrentModelItem(BitTorrent::TorrentHandle *torrent)
    : m_torrent(torrent)
{
}

BitTorrent::TorrentState TorrentModelItem::state() const
{
    return m_torrent->state();
}

QIcon TorrentModelItem::getIconByState(BitTorrent::TorrentState state)
{
    switch (state) {
    case BitTorrent::TorrentState::Downloading:
    case BitTorrent::TorrentState::ForcedDownloading:
    case BitTorrent::TorrentState::DownloadingMetadata:
        return get_downloading_icon();
    case BitTorrent::TorrentState::Allocating:
    case BitTorrent::TorrentState::StalledDownloading:
        return get_stalled_downloading_icon();
    case BitTorrent::TorrentState::StalledUploading:
        return get_stalled_uploading_icon();
    case BitTorrent::TorrentState::Uploading:
    case BitTorrent::TorrentState::ForcedUploading:
        return get_uploading_icon();
    case BitTorrent::TorrentState::PausedDownloading:
        return get_paused_icon();
    case BitTorrent::TorrentState::PausedUploading:
        return get_completed_icon();
    case BitTorrent::TorrentState::QueuedDownloading:
    case BitTorrent::TorrentState::QueuedUploading:
        return get_queued_icon();
    case BitTorrent::TorrentState::CheckingDownloading:
    case BitTorrent::TorrentState::CheckingUploading:
        return get_checking_icon();
    case BitTorrent::TorrentState::Unknown:
    case BitTorrent::TorrentState::Error:
        return get_error_icon();
    default:
        Q_ASSERT(false);
        return get_error_icon();
    }
}

QColor TorrentModelItem::getColorByState(BitTorrent::TorrentState state)
{
    bool dark = isDarkTheme();

    switch (state) {
    case BitTorrent::TorrentState::Downloading:
    case BitTorrent::TorrentState::ForcedDownloading:
    case BitTorrent::TorrentState::DownloadingMetadata:
        return QColor(34, 139, 34); // Forest Green
    case BitTorrent::TorrentState::Allocating:
    case BitTorrent::TorrentState::StalledDownloading:
    case BitTorrent::TorrentState::StalledUploading:
        if (!dark)
            return QColor(0, 0, 0); // Black
        else
            return QColor(255, 255, 255); // White
    case BitTorrent::TorrentState::Uploading:
    case BitTorrent::TorrentState::ForcedUploading:
        if (!dark)
            return QColor(65, 105, 225); // Royal Blue
        else
            return QColor(100, 149, 237); // Cornflower Blue
    case BitTorrent::TorrentState::PausedDownloading:
        return QColor(250, 128, 114); // Salmon
    case BitTorrent::TorrentState::PausedUploading:
        if (!dark)
            return QColor(0, 0, 139); // Dark Blue
        else
            return QColor(65, 105, 225); // Royal Blue
    case BitTorrent::TorrentState::Error:
        return QColor(255, 0, 0); // red
    case BitTorrent::TorrentState::QueuedDownloading:
    case BitTorrent::TorrentState::QueuedUploading:
    case BitTorrent::TorrentState::CheckingDownloading:
    case BitTorrent::TorrentState::CheckingUploading:
        return QColor(0, 128, 128); // Teal
    case BitTorrent::TorrentState::Unknown:
        return QColor(255, 0, 0); // red
    default:
        Q_ASSERT(false);
        return QColor(255, 0, 0); // red
    }
}

bool TorrentModelItem::setData(int column, const QVariant &value, int role)
{
    qDebug() << Q_FUNC_INFO << column << value;
    if (role != Qt::DisplayRole) return false;

    // Label, seed date and Name columns can be edited
    switch(column) {
    case TR_NAME:
        m_torrent->setName(value.toString());
        return true;
    case TR_LABEL: {
        QString new_label = value.toString();
        if (m_torrent->label() != new_label) {
            QString old_label = m_torrent->label();
            m_torrent->setLabel(new_label);
            emit labelChanged(old_label, new_label);
        }
        return true;
    }
    default:
        break;
    }
    return false;
}

QVariant TorrentModelItem::data(int column, int role) const
{
    if ((role == Qt::DecorationRole) && (column == TR_NAME))
        return getIconByState(state());

    if (role == Qt::ForegroundRole)
        return getColorByState(state());

    if ((role != Qt::DisplayRole) && (role != Qt::UserRole))
        return QVariant();

    switch(column) {
    case TR_NAME:
        return m_torrent->name();
    case TR_PRIORITY:
        return m_torrent->queuePosition();
    case TR_SIZE:
        return m_torrent->hasMetadata() ? m_torrent->wantedSize() : -1;
    case TR_PROGRESS:
        return m_torrent->progress();
    case TR_STATUS:
        return static_cast<int>(m_torrent->state());
    case TR_SEEDS:
        return (role == Qt::DisplayRole) ? m_torrent->seedsCount() : m_torrent->completeCount();
    case TR_PEERS:
        return (role == Qt::DisplayRole) ? (m_torrent->peersCount() - m_torrent->seedsCount()) : m_torrent->incompleteCount();
    case TR_DLSPEED:
        return m_torrent->downloadPayloadRate();
    case TR_UPSPEED:
        return m_torrent->uploadPayloadRate();
    case TR_ETA:
        return m_torrent->eta();
    case TR_RATIO:
        return m_torrent->realRatio();
    case TR_LABEL:
        return m_torrent->label();
    case TR_ADD_DATE:
        return m_torrent->addedTime();
    case TR_SEED_DATE:
        return m_torrent->completedTime();
    case TR_TRACKER:
        return m_torrent->currentTracker();
    case TR_DLLIMIT:
        return m_torrent->downloadLimit();
    case TR_UPLIMIT:
        return m_torrent->uploadLimit();
    case TR_AMOUNT_DOWNLOADED:
        return m_torrent->totalDownload();
    case TR_AMOUNT_UPLOADED:
        return m_torrent->totalUpload();
    case TR_AMOUNT_DOWNLOADED_SESSION:
        return m_torrent->totalPayloadDownload();
    case TR_AMOUNT_UPLOADED_SESSION:
        return m_torrent->totalPayloadUpload();
    case TR_AMOUNT_LEFT:
        return m_torrent->incompletedSize();
    case TR_TIME_ELAPSED:
        return (role == Qt::DisplayRole) ? m_torrent->activeTime() : m_torrent->seedingTime();
    case TR_SAVE_PATH:
        return m_torrent->savePathParsed();
    case TR_COMPLETED:
        return m_torrent->completedSize();
    case TR_RATIO_LIMIT:
        return m_torrent->maxRatio();
    case TR_SEEN_COMPLETE_DATE:
        return m_torrent->lastSeenComplete();
    case TR_LAST_ACTIVITY:
        if (m_torrent->isPaused() || m_torrent->isChecking())
            return -1;
        if (m_torrent->timeSinceUpload() < m_torrent->timeSinceDownload())
            return m_torrent->timeSinceUpload();
        else
            return m_torrent->timeSinceDownload();
    case TR_TOTAL_SIZE:
        return m_torrent->hasMetadata() ? m_torrent->totalSize() : -1;
    default:
        return QVariant();
    }
}

BitTorrent::TorrentHandle *TorrentModelItem::torrentHandle() const
{
    return m_torrent;
}

// TORRENT MODEL

TorrentModel::TorrentModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void TorrentModel::populate()
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

TorrentModel::~TorrentModel() {
    qDebug() << Q_FUNC_INFO << "ENTER";
    qDeleteAll(m_items);
    m_items.clear();
    qDebug() << Q_FUNC_INFO << "EXIT";
}

QVariant TorrentModel::headerData(int section, Qt::Orientation orientation,
                                  int role) const
{
    if (orientation == Qt::Horizontal) {
        if (role == Qt::DisplayRole) {
            switch(section) {
            case TorrentModelItem::TR_NAME: return tr("Name", "i.e: torrent name");
            case TorrentModelItem::TR_PRIORITY: return "#";
            case TorrentModelItem::TR_SIZE: return tr("Size", "i.e: torrent size");
            case TorrentModelItem::TR_PROGRESS: return tr("Done", "% Done");
            case TorrentModelItem::TR_STATUS: return tr("Status", "Torrent status (e.g. downloading, seeding, paused)");
            case TorrentModelItem::TR_SEEDS: return tr("Seeds", "i.e. full sources (often untranslated)");
            case TorrentModelItem::TR_PEERS: return tr("Peers", "i.e. partial sources (often untranslated)");
            case TorrentModelItem::TR_DLSPEED: return tr("Down Speed", "i.e: Download speed");
            case TorrentModelItem::TR_UPSPEED: return tr("Up Speed", "i.e: Upload speed");
            case TorrentModelItem::TR_RATIO: return tr("Ratio", "Share ratio");
            case TorrentModelItem::TR_ETA: return tr("ETA", "i.e: Estimated Time of Arrival / Time left");
            case TorrentModelItem::TR_LABEL: return tr("Label");
            case TorrentModelItem::TR_ADD_DATE: return tr("Added On", "Torrent was added to transfer list on 01/01/2010 08:00");
            case TorrentModelItem::TR_SEED_DATE: return tr("Completed On", "Torrent was completed on 01/01/2010 08:00");
            case TorrentModelItem::TR_TRACKER: return tr("Tracker");
            case TorrentModelItem::TR_DLLIMIT: return tr("Down Limit", "i.e: Download limit");
            case TorrentModelItem::TR_UPLIMIT: return tr("Up Limit", "i.e: Upload limit");
            case TorrentModelItem::TR_AMOUNT_DOWNLOADED: return tr("Downloaded", "Amount of data downloaded (e.g. in MB)");
            case TorrentModelItem::TR_AMOUNT_UPLOADED: return tr("Uploaded", "Amount of data uploaded (e.g. in MB)");
            case TorrentModelItem::TR_AMOUNT_DOWNLOADED_SESSION: return tr("Session Download", "Amount of data downloaded since program open (e.g. in MB)");
            case TorrentModelItem::TR_AMOUNT_UPLOADED_SESSION: return tr("Session Upload", "Amount of data uploaded since program open (e.g. in MB)");
            case TorrentModelItem::TR_AMOUNT_LEFT: return tr("Remaining", "Amount of data left to download (e.g. in MB)");
            case TorrentModelItem::TR_TIME_ELAPSED: return tr("Time Active", "Time (duration) the torrent is active (not paused)");
            case TorrentModelItem::TR_SAVE_PATH: return tr("Save path", "Torrent save path");
            case TorrentModelItem::TR_COMPLETED: return tr("Completed", "Amount of data completed (e.g. in MB)");
            case TorrentModelItem::TR_RATIO_LIMIT: return tr("Ratio Limit", "Upload share ratio limit");
            case TorrentModelItem::TR_SEEN_COMPLETE_DATE: return tr("Last Seen Complete", "Indicates the time when the torrent was last seen complete/whole");
            case TorrentModelItem::TR_LAST_ACTIVITY: return tr("Last Activity", "Time passed since a chunk was downloaded/uploaded");
            case TorrentModelItem::TR_TOTAL_SIZE: return tr("Total Size", "i.e. Size including unwanted data");
            default:
                return QVariant();
            }
        }
        else if (role == Qt::TextAlignmentRole) {
            switch(section) {
            case TorrentModelItem::TR_AMOUNT_DOWNLOADED:
            case TorrentModelItem::TR_AMOUNT_UPLOADED:
            case TorrentModelItem::TR_AMOUNT_DOWNLOADED_SESSION:
            case TorrentModelItem::TR_AMOUNT_UPLOADED_SESSION:
            case TorrentModelItem::TR_AMOUNT_LEFT:
            case TorrentModelItem::TR_COMPLETED:
            case TorrentModelItem::TR_SIZE:
            case TorrentModelItem::TR_TOTAL_SIZE:
            case TorrentModelItem::TR_ETA:
            case TorrentModelItem::TR_SEEDS:
            case TorrentModelItem::TR_PEERS:
            case TorrentModelItem::TR_UPSPEED:
            case TorrentModelItem::TR_DLSPEED:
            case TorrentModelItem::TR_UPLIMIT:
            case TorrentModelItem::TR_DLLIMIT:
            case TorrentModelItem::TR_RATIO_LIMIT:
            case TorrentModelItem::TR_RATIO:
            case TorrentModelItem::TR_PRIORITY:
            case TorrentModelItem::TR_LAST_ACTIVITY:
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

    if (index.row() >= 0 && index.row() < rowCount() && index.column() >= 0 && index.column() < columnCount())
        return m_items[index.row()]->data(index.column(), role);

    return QVariant();
}

bool TorrentModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    qDebug() << Q_FUNC_INFO << value;
    if (!index.isValid() || (role != Qt::DisplayRole)) return false;

    qDebug("Index is valid and role is DisplayRole");
    if ((index.row() >= 0) && (index.row() < rowCount()) && (index.column() >= 0) && (index.column() < columnCount())) {
        bool change = m_items[index.row()]->setData(index.column(), value, role);
        if (change)
            notifyTorrentChanged(index.row());
        return change;
    }

    return false;
}

int TorrentModel::torrentRow(const BitTorrent::InfoHash &hash) const
{
    int row = 0;

    foreach (TorrentModelItem *const item, m_items) {
        if (item->hash() == hash) return row;
        ++row;
    }
    return -1;
}

void TorrentModel::addTorrent(BitTorrent::TorrentHandle *const torrent)
{
    if (torrentRow(torrent->hash()) < 0) {
        beginInsertTorrent(m_items.size());
        TorrentModelItem *item = new TorrentModelItem(torrent);
        connect(item, SIGNAL(labelChanged(QString, QString)), SLOT(handleTorrentLabelChange(QString, QString)));
        m_items << item;
        emit torrentAdded(item);
        endInsertTorrent();
    }
}

void TorrentModel::beginInsertTorrent(int row)
{
    beginInsertRows(QModelIndex(), row, row);
}

void TorrentModel::endInsertTorrent()
{
    endInsertRows();
}

void TorrentModel::beginRemoveTorrent(int row)
{
    beginRemoveRows(QModelIndex(), row, row);
}

void TorrentModel::endRemoveTorrent()
{
    endRemoveRows();
}

void TorrentModel::notifyTorrentChanged(int row)
{
    emit dataChanged(index(row, 0), index(row, columnCount() - 1));
}

Qt::ItemFlags TorrentModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;
    // Explicitely mark as editable
    return QAbstractListModel::flags(index) | Qt::ItemIsEditable;
}

void TorrentModel::handleTorrentLabelChange(QString previous, QString current)
{
    emit torrentChangedLabel(static_cast<TorrentModelItem*>(sender()), previous, current);
}

QString TorrentModel::torrentHash(int row) const
{
    if (row >= 0 && row < rowCount())
        return m_items.at(row)->hash();
    return QString();
}

BitTorrent::TorrentHandle *TorrentModel::torrentHandle(const QModelIndex &index) const
{
    if (index.isValid() && (index.row() >= 0) && (index.row() < rowCount()))
        return m_items[index.row()]->torrentHandle();

    return 0;
}

void TorrentModel::handleTorrentAboutToBeRemoved(BitTorrent::TorrentHandle *const torrent)
{
    const int row = torrentRow(torrent->hash());
    qDebug() << Q_FUNC_INFO << row;
    if (row >= 0) {
        emit torrentAboutToBeRemoved(m_items.at(row));

        beginRemoveTorrent(row);
        delete m_items.takeAt(row);
        endRemoveTorrent();
    }
}

void TorrentModel::handleTorrentStatusUpdated(BitTorrent::TorrentHandle *const torrent)
{
    const int row = torrentRow(torrent->hash());
    if (row >= 0)
        notifyTorrentChanged(row);
}
