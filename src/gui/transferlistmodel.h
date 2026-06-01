/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015-2025  Vladimir Golovnev <glassez@yandex.ru>
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

#pragma once

#include <QAbstractItemModel>
#include <QColor>
#include <QHash>
#include <QIcon>
#include <QList>
#include <QSet>
#include <QStringList>
#include <QVector>

#include "base/bittorrent/infohash.h"
#include "base/bittorrent/torrent.h"

class TransferListModel final : public QAbstractItemModel
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TransferListModel)

public:
    enum Column
    {
        TR_QUEUE_POSITION,
        TR_NAME,
        TR_SIZE,
        TR_TOTAL_SIZE,
        TR_PROGRESS,
        TR_STATUS,
        TR_SEEDS,
        TR_PEERS,
        TR_DLSPEED,
        TR_UPSPEED,
        TR_ETA,
        TR_RATIO,
        TR_POPULARITY,
        TR_CATEGORY,
        TR_TAGS,
        TR_ADD_DATE,
        TR_SEED_DATE,
        TR_TRACKER,
        TR_DLLIMIT,
        TR_UPLIMIT,
        TR_AMOUNT_DOWNLOADED,
        TR_AMOUNT_UPLOADED,
        TR_AMOUNT_DOWNLOADED_SESSION,
        TR_AMOUNT_UPLOADED_SESSION,
        TR_AMOUNT_LEFT,
        TR_TIME_ELAPSED,
        TR_SAVE_PATH,
        TR_COMPLETED,
        TR_RATIO_LIMIT,
        TR_SEEN_COMPLETE_DATE,
        TR_LAST_ACTIVITY,
        TR_AVAILABILITY,
        TR_DOWNLOAD_PATH,
        TR_INFOHASH_V1,
        TR_INFOHASH_V2,
        TR_REANNOUNCE,
        TR_PRIVATE,
    TR_GROUP, // Virtual torrent group (experimental)
        TR_CREATE_DATE,

        NB_COLUMNS
    };

    enum DataRole
    {
        UnderlyingDataRole = Qt::UserRole,
        AdditionalUnderlyingDataRole
    };

    explicit TransferListModel(QObject *parent = nullptr);

    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    BitTorrent::Torrent *torrentHandle(const QModelIndex &index) const;
    QModelIndex indexForTorrent(const BitTorrent::Torrent *torrent, int column = 0) const;
    QString groupName(const QModelIndex &index) const;
    QStringList groupNames() const;
    QString groupOf(const BitTorrent::TorrentID &id) const;
    QStringList expandedGroups() const;
    bool hasGroups() const;
    bool createGroup(const QString &name, const QSet<BitTorrent::TorrentID> &initialMembers = {});
    bool renameGroup(const QString &oldName, const QString &newName);
    bool deleteGroup(const QString &name);
    bool addMembers(const QString &groupName, const QSet<BitTorrent::TorrentID> &members);
    bool removeMembers(const QSet<BitTorrent::TorrentID> &members);
    void setGroupExpanded(const QString &name, bool expanded);

signals:
    void groupsChanged();

private slots:
    void addTorrents(const QList<BitTorrent::Torrent *> &torrents);
    void handleTorrentAboutToBeRemoved(BitTorrent::Torrent *torrent);
    void handleTorrentStatusUpdated(BitTorrent::Torrent *torrent);
    void handleTorrentsUpdated(const QList<BitTorrent::Torrent *> &torrents);

private:
    void configure();
    void loadUIThemeResources();
    void loadGroups();
    void saveGroups() const;
    void rebuildGroupingLayout();
    void rebuildGroupNameIndex();
    bool isGroupIndex(const QModelIndex &index) const;
    QModelIndex makeGroupIndex(int row, int column) const;
    QModelIndex makeTorrentIndex(const BitTorrent::Torrent *torrent, int column) const;
    int groupRow(const QString &name) const;
    bool moveMemberToGroup(const BitTorrent::TorrentID &id, const QString &groupName);
    void removeMemberFromGroup(const BitTorrent::TorrentID &id);
    QString displayValue(const BitTorrent::Torrent *torrent, int column) const;
    QVariant internalValue(const BitTorrent::Torrent *torrent, int column, bool alt) const;
    QIcon getIconByState(BitTorrent::TorrentState state) const;

    struct GroupData
    {
        QString name;
        QSet<BitTorrent::TorrentID> members;
    };

    struct TorrentPosition
    {
        int topLevelRow = -1;
        int groupRow = -1;
        int childRow = -1;
    };

    QList<BitTorrent::Torrent *> m_torrentList;  // maps row number to torrent handle
    QHash<BitTorrent::Torrent *, int> m_torrentMap;  // maps torrent handle to row number
    QList<GroupData> m_groups;
    QHash<QString, int> m_groupIndexByName;
    QHash<BitTorrent::TorrentID, QString> m_groupByMember;
    QStringList m_expandedGroups;
    QVector<QList<BitTorrent::Torrent *>> m_groupedTorrents;
    QList<BitTorrent::Torrent *> m_ungroupedTorrents;
    QHash<BitTorrent::Torrent *, TorrentPosition> m_torrentPositions;
    const QHash<BitTorrent::TorrentState, QString> m_statusStrings;
    // row text colors
    QHash<BitTorrent::TorrentState, QColor> m_stateThemeColors;

    enum class HideZeroValuesMode
    {
        Never,
        Stopped,
        Always
    };

    HideZeroValuesMode m_hideZeroValuesMode = HideZeroValuesMode::Never;
    bool m_useTorrentStatesColors = false;

    // cached icons
    QIcon m_checkingIcon;
    QIcon m_completedIcon;
    QIcon m_downloadingIcon;
    QIcon m_errorIcon;
    QIcon m_movingIcon;
    QIcon m_stoppedIcon;
    QIcon m_queuedIcon;
    QIcon m_stalledDLIcon;
    QIcon m_stalledUPIcon;
    QIcon m_uploadingIcon;
};

Q_DECLARE_METATYPE(TransferListModel::Column)
