#include "torrentpiecesmodel.h"
#include "gui/properties/piecesdelegate.h"

#include <QBitArray>

TorrentPiecesTreeModel::TorrentPiecesTreeModel()
{
}

TorrentPiecesModel::TorrentPiecesModel(BitTorrent::Torrent *torrent, int rangeStart, int rangeEnd)
    : m_torrent(torrent)
    , m_rangeStart(rangeStart)
    , m_rangeEnd(rangeEnd)
{
    Q_ASSERT(rangeStart >= 0);
    Q_ASSERT(rangeEnd >= 0);
    Q_ASSERT(rangeStart <= rangeEnd);
    m_children = QVector<TorrentPiecesModel*>();
}

void TorrentPiecesModel::appendChild(TorrentPiecesModel *child)
{
    m_children.push_back(child);
    child->m_parent = this;
}

TorrentPiecesModel* TorrentPiecesModel::child(int row) const
{
    return m_children[row];
}

TorrentPiecesModel* TorrentPiecesModel::parent() const
{
    return m_parent;
}

int TorrentPiecesModel::row() const
{
    if (m_parent)
        return m_parent->m_children.indexOf(const_cast<TorrentPiecesModel *>(this));
    return 0;
}

int TorrentPiecesModel::childCount() const
{
    return m_children.size();
}

int TorrentPiecesTreeModel::columnCount(const QModelIndex &parent) const
{
    return TorrentPiecesTreeModel::Columns::NB_COLS;
}

int TorrentPiecesTreeModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0)
        return 0;

    TorrentPiecesModel *parentItem;
    if (!parent.isValid())
        parentItem = m_root;
    else
        parentItem = static_cast<TorrentPiecesModel*>(parent.internalPointer());

    return parentItem ? parentItem->childCount() : 0;
}

QModelIndex TorrentPiecesTreeModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return {};

    auto *childItem = static_cast<TorrentPiecesModel*>(index.internalPointer());
    if (!childItem)
        return {};

    TorrentPiecesModel *parentItem = childItem->parent();
    if (parentItem == m_root)
        return {};

    return createIndex(parentItem->row(), 0, parentItem);
}

QModelIndex TorrentPiecesTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid() && (parent.column() != 0))
        return {};

    if (column >= columnCount())
        return {};

    TorrentPiecesModel *parentItem;
    if (!parent.isValid())
        parentItem = m_root;
    else
        parentItem = static_cast<TorrentPiecesModel*>(parent.internalPointer());
    Q_ASSERT(parentItem);

    if (row >= parentItem->childCount())
        return {};

    TorrentPiecesModel *childItem = parentItem->child(row);
    if (childItem)
        return createIndex(row, column, childItem);
    return {};
}

QVariant TorrentPiecesTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return {};
    auto item = static_cast<TorrentPiecesModel*>(index.internalPointer());

    switch (role)
    {
    case Qt::DisplayRole:
        switch (index.column())
        {
        case TorrentPiecesTreeModel::Columns::RANGE:
            return QString::fromLatin1("[%1..%2]").arg(item->m_rangeStart).arg(item->m_rangeEnd);
        case TorrentPiecesTreeModel::Columns::DOWNLOADED:
            {
            auto pieces = item->m_torrent->pieces();

            auto bytesOffset = item->m_rangeStart / 8;
            auto alignedStart = bytesOffset * 8;
            auto size = item->m_rangeEnd - item->m_rangeStart + 1;
            auto alignedSize = item->m_rangeEnd - alignedStart + 1;
            Q_ASSERT(alignedSize >= 0);

            auto viewPtr = pieces.bits() + bytesOffset;
            auto piecesView = QBitArray::fromBits(viewPtr, alignedSize);

            auto count = piecesView.count(true);
            for (int i = alignedStart; i < item->m_rangeStart; ++i)
                count -= pieces[i] ? 1 : 0;

            return QString::fromLatin1("%1/%2").arg(count).arg(size);
            }
        case TorrentPiecesTreeModel::Columns::DOWNLOADING:
            return {};
        case TorrentPiecesTreeModel::Columns::MISSING:
            {
                auto pieces = item->m_torrent->pieces();

                auto bytesOffset = item->m_rangeStart / 8;
                auto alignedStart = bytesOffset * 8;
                auto size = item->m_rangeEnd - item->m_rangeStart + 1;
                auto alignedSize = item->m_rangeEnd - alignedStart + 1;
                Q_ASSERT(alignedSize >= 0);

                auto viewPtr = pieces.bits() + bytesOffset;
                auto piecesView = QBitArray::fromBits(viewPtr, alignedSize);

                auto count = piecesView.count(true);
                for (int i = alignedStart; i < item->m_rangeStart; ++i)
                    count -= pieces[i] ? 1 : 0;

                return QString::fromLatin1("%1").arg(size - count);
            }
        case TorrentPiecesTreeModel::Columns::STATUSBAR:
            return QVariant();
        default:
            return {};
        }
        break;
    default:
        return {};
    }
}

QVariant TorrentPiecesTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole)
    {
        switch (section)
        {
        case TorrentPiecesTreeModel::Columns::RANGE:
            return tr("Range");
        case TorrentPiecesTreeModel::Columns::DOWNLOADED:
            return tr("Downloaded");
        case TorrentPiecesTreeModel::Columns::DOWNLOADING:
            return tr("Downloading");
        case TorrentPiecesTreeModel::Columns::MISSING:
            return tr("Missing");
        case TorrentPiecesTreeModel::Columns::STATUSBAR:
            return tr("Status");
        default:
            Q_ASSERT(false);
            return {};
        }
    }

    return {};
}
