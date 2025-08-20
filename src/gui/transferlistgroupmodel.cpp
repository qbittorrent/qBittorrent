#include "transferlistgroupmodel.h"

#include <QIcon>
#include <QApplication>
#include <QPalette>

#include "base/torrentgroup.h"
#include "transferlistmodel.h"
#include "uithememanager.h"
#include "base/utils/misc.h"
#include "base/utils/string.h"
#include "base/unicodestrings.h"
#include "base/types.h"

// Helper to aggregate group stats (computed on demand; inexpensive for typical group sizes)
namespace
{
    struct GroupAggregates
    {
        qint64 wanted = 0;
        qint64 completed = 0;
        qint64 remaining = 0;
        qint64 dlSpeed = 0;
        qint64 upSpeed = 0;
        qint64 seeds = 0; qint64 totalSeeds = 0;
        qint64 peers = 0; qint64 totalPeers = 0;
        qint64 downloaded = 0; qint64 uploaded = 0;
        qint64 totalSize = 0;
    };

    GroupAggregates computeAggregates(const QList<BitTorrent::Torrent*> &members)
    {
        GroupAggregates aggr;
        for (BitTorrent::Torrent *t : members)
        {
            if (!t) continue;
            aggr.wanted += t->wantedSize();
            aggr.completed += t->completedSize();
            aggr.remaining += t->remainingSize();
            aggr.dlSpeed += t->downloadPayloadRate();
            aggr.upSpeed += t->uploadPayloadRate();
            aggr.seeds += t->seedsCount();
            aggr.totalSeeds += t->totalSeedsCount();
            aggr.peers += t->leechsCount();
            aggr.totalPeers += t->totalLeechersCount();
            aggr.downloaded += t->totalDownload();
            aggr.uploaded += t->totalUpload();
            aggr.totalSize += t->totalSize();
        }
        return aggr;
    }
}

TransferListGroupModel::TransferListGroupModel(TransferListModel *source, QObject *parent)
    : QAbstractItemModel(parent)
    , m_source(source)
{
    connect(source, &QAbstractItemModel::dataChanged, this, &TransferListGroupModel::handleSourceChanged);
    connect(source, &QAbstractItemModel::modelReset, this, &TransferListGroupModel::handleSourceReset);
    connect(TorrentGroupManager::instance(), &TorrentGroupManager::groupsChanged, this, &TransferListGroupModel::rebuild);
    connect(TorrentGroupManager::instance(), &TorrentGroupManager::groupMembershipChanged, this, &TransferListGroupModel::rebuild);
    rebuild();
}

void TransferListGroupModel::rebuild()
{
    beginResetModel();
    m_groups.clear();
    m_ungrouped.clear();
    m_groupRowByName.clear();

    const auto *mgr = TorrentGroupManager::instance();
    // Preserve insertion order of groups as defined by manager
    const QList<TorrentGroup> groups = mgr->groups();
    for (const auto &g : groups)
    {
        GroupItem gi; gi.name = g.name; gi.members.clear();
        m_groupRowByName.insert(g.name, m_groups.size());
        m_groups.push_back(gi);
    }

    // Iterate torrents from source preserving original order for members & ungrouped
    for (int r = 0; r < m_source->rowCount(); ++r)
    {
        BitTorrent::Torrent *t = m_source->torrentHandle(m_source->index(r, 0));
        if (!t) continue;
        const QString groupName = mgr->groupOf(t->id());
        if (groupName.isEmpty())
        {
            m_ungrouped.append(t);
        }
        else
        {
            const int idx = m_groupRowByName.value(groupName, -1);
            if (idx >= 0)
                m_groups[idx].members.append(t);
            else
                m_ungrouped.append(t); // fallback safety
        }
    }
    endResetModel();
}

QModelIndex TransferListGroupModel::index(int row, int column, const QModelIndex &parent) const
{
    if ((row < 0) || (column < 0) || (column >= m_source->columnCount())) return {};

    // Top-level
    if (!parent.isValid())
    {
        const int topLevelCount = m_groups.size() + m_ungrouped.size();
        if (row >= topLevelCount) return {};
        if (row < m_groups.size())
            return createIndex(row, column, static_cast<quintptr>(-1)); // group parent sentinel
        // Ungrouped torrent leaf at top level: encode pointer
        BitTorrent::Torrent *t = m_ungrouped[row - m_groups.size()];
        return createIndex(row, column, reinterpret_cast<quintptr>(t));
    }

    // Parent is a group
    if (parent.internalId() == static_cast<quintptr>(-1))
    {
        const int gIndex = parent.row();
        if ((gIndex < 0) || (gIndex >= m_groups.size())) return {};
        if (row >= m_groups[gIndex].members.size()) return {};
        BitTorrent::Torrent *t = m_groups[gIndex].members[row];
        return createIndex(row, column, reinterpret_cast<quintptr>(t));
    }

    // Leaf has no children
    return {};
}

QModelIndex TransferListGroupModel::parent(const QModelIndex &child) const
{
    if (!child.isValid()) return {};
    const quintptr id = child.internalId();
    if (id == static_cast<quintptr>(-1)) return {}; // top-level group parent has no parent

    BitTorrent::Torrent *t = reinterpret_cast<BitTorrent::Torrent*>(id);
    // Determine if torrent belongs to a group
    const auto *mgr = TorrentGroupManager::instance();
    const QString groupName = mgr->groupOf(t->id());
    if (groupName.isEmpty()) return {}; // ungrouped => top-level leaf
    const int gRow = m_groupRowByName.value(groupName, -1);
    if (gRow < 0) return {};
    return createIndex(gRow, 0, static_cast<quintptr>(-1));
}

int TransferListGroupModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return m_groups.size() + m_ungrouped.size();
    if (parent.internalId() == static_cast<quintptr>(-1))
    {
        const int gIndex = parent.row();
        if ((gIndex < 0) || (gIndex >= m_groups.size())) return 0;
        return m_groups[gIndex].members.size();
    }
    return 0; // leaf
}

int TransferListGroupModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_source->columnCount();
}

QVariant TransferListGroupModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) return {};
    const quintptr id = index.internalId();
    if (id == static_cast<quintptr>(-1))
    {
        // Group parent row
        const GroupItem &g = m_groups.value(index.row());
        if (role == Qt::BackgroundRole)
        {
            QColor base = QApplication::palette().color(QPalette::Base);
            // Create a higher-contrast variant: lighten on dark themes, darken on light themes
            if (base.lightness() < 128)
                base = base.lighter(135); // noticeable light pad on dark background
            else
                base = base.darker(115); // subtle darker band on light background
            return QBrush(base);
        }
        if ((role == Qt::DecorationRole) && (index.column() == TransferListModel::TR_NAME))
        {
            const bool expanded = TorrentGroupManager::instance()->expandedGroups().contains(g.name);
            return UIThemeManager::instance()->getIcon(expanded ? u"folder-documents"_s : u"directory"_s);
        }
        if ((role != Qt::DisplayRole) && (role != TransferListModel::UnderlyingDataRole))
            return {};

        // Aggregate lazily for display / underlying roles
        const GroupAggregates aggr = computeAggregates(g.members);
        const int col = index.column();
        if (col == TransferListModel::TR_NAME)
        {
            if (role == Qt::DisplayRole)
                return QStringLiteral("%1 (%2)").arg(g.name).arg(g.members.size());
            return g.name; // underlying for sorting by name
        }

        // Provide aggregated numeric underlying values for logical sorting
        if (role == TransferListModel::UnderlyingDataRole)
        {
            switch (col)
            {
            case TransferListModel::TR_SIZE: return aggr.wanted;
            case TransferListModel::TR_TOTAL_SIZE: return aggr.totalSize;
            case TransferListModel::TR_PROGRESS: return (aggr.wanted > 0) ? ((aggr.completed * 100.0) / aggr.wanted) : 0.0;
            case TransferListModel::TR_SEEDS: return aggr.seeds; // primary value
            case TransferListModel::TR_PEERS: return aggr.peers;
            case TransferListModel::TR_DLSPEED: return aggr.dlSpeed;
            case TransferListModel::TR_UPSPEED: return aggr.upSpeed;
            case TransferListModel::TR_ETA: return (aggr.dlSpeed > 0) ? (aggr.remaining / qMax<qint64>(1, aggr.dlSpeed)) : MAX_ETA;
            case TransferListModel::TR_RATIO: return (aggr.downloaded > 0) ? (static_cast<double>(aggr.uploaded) / aggr.downloaded) : BitTorrent::Torrent::MAX_RATIO;
            case TransferListModel::TR_AMOUNT_DOWNLOADED: return aggr.downloaded;
            case TransferListModel::TR_AMOUNT_UPLOADED: return aggr.uploaded;
            case TransferListModel::TR_AMOUNT_LEFT: return aggr.remaining;
            case TransferListModel::TR_COMPLETED: return aggr.completed;
            default: return {}; // unsupported columns -> empty underlying data
            }
        }

        // DisplayRole formatting mirrors TransferListModel for key columns (basic subset)
        switch (col)
        {
        case TransferListModel::TR_SIZE:
            return Utils::Misc::friendlyUnit(aggr.wanted);
        case TransferListModel::TR_TOTAL_SIZE:
            return Utils::Misc::friendlyUnit(aggr.totalSize);
        case TransferListModel::TR_PROGRESS:
        {
            if (aggr.wanted <= 0) return QString();
            const double p = static_cast<double>(aggr.completed) / static_cast<double>(aggr.wanted);
            return (p >= 1.0) ? QStringLiteral("100%") : (Utils::String::fromDouble(p * 100.0, 1) + QLatin1Char('%'));
        }
        case TransferListModel::TR_SEEDS:
            return QStringLiteral("%1 (%2)").arg(QString::number(aggr.seeds), QString::number(aggr.totalSeeds));
        case TransferListModel::TR_PEERS:
            return QStringLiteral("%1 (%2)").arg(QString::number(aggr.peers), QString::number(aggr.totalPeers));
        case TransferListModel::TR_DLSPEED:
            return Utils::Misc::friendlyUnit(aggr.dlSpeed, true);
        case TransferListModel::TR_UPSPEED:
            return Utils::Misc::friendlyUnit(aggr.upSpeed, true);
        case TransferListModel::TR_ETA:
        {
            const qint64 eta = (aggr.dlSpeed > 0) ? (aggr.remaining / qMax<qint64>(1, aggr.dlSpeed)) : MAX_ETA;
            return Utils::Misc::userFriendlyDuration(eta, MAX_ETA);
        }
        case TransferListModel::TR_RATIO:
        {
            if (aggr.downloaded <= 0) return C_INFINITY;
            const double r = static_cast<double>(aggr.uploaded) / aggr.downloaded;
            if ((static_cast<int>(r) == -1) || (r >= BitTorrent::Torrent::MAX_RATIO)) return C_INFINITY;
            return Utils::String::fromDouble(r, 2);
        }
        case TransferListModel::TR_AMOUNT_DOWNLOADED:
            return Utils::Misc::friendlyUnit(aggr.downloaded);
        case TransferListModel::TR_AMOUNT_UPLOADED:
            return Utils::Misc::friendlyUnit(aggr.uploaded);
        case TransferListModel::TR_AMOUNT_LEFT:
            return Utils::Misc::friendlyUnit(aggr.remaining);
        case TransferListModel::TR_COMPLETED:
            return Utils::Misc::friendlyUnit(aggr.completed);
        default:
            return {}; // leave others blank for now
        }
    }

    BitTorrent::Torrent *t = reinterpret_cast<BitTorrent::Torrent*>(id);
    if (!t) return {};
    const QModelIndex sourceIdx = m_source->indexForTorrent(t);
    if (!sourceIdx.isValid()) return {};
    // Map to correct column in source
    const QModelIndex mapped = m_source->index(sourceIdx.row(), index.column());
    if (role == Qt::BackgroundRole)
    {
        const auto *mgr = TorrentGroupManager::instance();
        if (!mgr->groupOf(t->id()).isEmpty())
        {
            QColor base = QApplication::palette().color(QPalette::Base);
            if (base.lightness() < 128)
                base = base.lighter(120);
            else
                base = base.darker(105);
            return QBrush(base);
        }
    }
    return m_source->data(mapped, role);
}

Qt::ItemFlags TransferListGroupModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;
    if (index.internalId() == static_cast<quintptr>(-1))
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable; // group row selectable
    // Forward from source
    const auto *t = reinterpret_cast<BitTorrent::Torrent*>(index.internalId());
    if (!t) return Qt::NoItemFlags;
    const QModelIndex sourceIdx = m_source->indexForTorrent(t);
    if (!sourceIdx.isValid()) return Qt::NoItemFlags;
    return m_source->flags(m_source->index(sourceIdx.row(), index.column()));
}

QVariant TransferListGroupModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    return m_source->headerData(section, orientation, role);
}

BitTorrent::Torrent *TransferListGroupModel::torrentHandle(const QModelIndex &index) const
{
    if (!index.isValid()) return nullptr;
    const quintptr id = index.internalId();
    if (id == static_cast<quintptr>(-1)) return nullptr; // group parent
    return reinterpret_cast<BitTorrent::Torrent*>(id);
}

QString TransferListGroupModel::groupName(const QModelIndex &index) const
{
    if (!index.isValid() || (index.internalId() != static_cast<quintptr>(-1))) return {};
    if ((index.row() < 0) || (index.row() >= m_groups.size())) return {};
    return m_groups[index.row()].name;
}

void TransferListGroupModel::setGroupExpanded(const QString &name, bool expanded)
{
    QStringList expandedList = TorrentGroupManager::instance()->expandedGroups();
    const bool already = expandedList.contains(name);
    if (expanded && !already)
        expandedList << name;
    else if (!expanded && already)
        expandedList.removeAll(name);
    else
        return; // no change
    TorrentGroupManager::instance()->setExpandedGroups(expandedList);
    // update icon
    for (int i = 0; i < m_groups.size(); ++i)
    {
        if (m_groups[i].name == name)
        {
            const QModelIndex topLeft = createIndex(i, 0, static_cast<quintptr>(-1));
            emit dataChanged(topLeft, createIndex(i, columnCount({}) - 1, static_cast<quintptr>(-1)), {Qt::DecorationRole, Qt::DisplayRole});
            break;
        }
    }
}

void TransferListGroupModel::handleSourceChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QList<int> &roles)
{
    Q_UNUSED(topLeft); Q_UNUSED(bottomRight); Q_UNUSED(roles);
    // Simplicity: full rebuild
    rebuild();
}

void TransferListGroupModel::handleSourceReset()
{
    rebuild();
}
