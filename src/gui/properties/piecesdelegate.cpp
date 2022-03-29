#include "piecesdelegate.h"

#include <QBitArray>
#include <QDebug>
#include <QPainter>
#include <QPaintEvent>

#include "gui/torrentcontentmodel.h"
#include "gui/torrentcontentmodelfile.h"
#include "gui/torrentcontentfiltermodel.h"
#include "gui/torrentpiecesmodel.h"

PiecesDelegate::PiecesDelegate(QTreeView *parent)
    : QStyledItemDelegate {parent}
    , m_parent(parent)
    , m_bar(new PieceAvailabilityBar(parent))
{
    m_bar->setVisible(false);
}

void PiecesDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    auto pieces = index.data(Qt::UserRole).toBitArray();
    QVector<int> availability(pieces.size());
    for (int i = 0; i < pieces.size(); ++i) {
        availability[i] = pieces.at(i) ? 1 : 0;
    }

    m_bar->setAvailability(availability);
    m_bar->setFixedSize(option.rect.width(), option.rect.height());
    painter->save();
    painter->translate(option.rect.x(), option.rect.y());
    m_bar->paintEvent(painter);
    painter->restore();
}

void PiecesDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, PieceStatusBar *m_bar, BitTorrent::Torrent *m_torrent, int rangeStart, int rangeEnd)
{
    auto size = rangeEnd - rangeStart;
    Q_ASSERT(size >= 0);

    auto pieces = m_torrent->pieces();
    QBitArray downloaded(size);
    for (int i = 0; i < size; ++i)
        downloaded[i] = pieces[rangeStart + i];
    auto availability = m_torrent->pieceAvailability();
    auto downloadingPieces = m_torrent->downloadingPieces();
    QBitArray downloading(size);
    for (int i = 0; i < size; ++i)
        downloading[i] = downloadingPieces[rangeStart + i];
    QBitArray available(size);
    for (int i = 0; i < size; ++i)
        available[i] = availability[rangeStart + i] > 0;
    auto piecePriorities = m_torrent->piecePriorities();
    QBitArray enabled(size);
    for (int i = 0; i < size; ++i)
        enabled[i] = piecePriorities[rangeStart + i] != BitTorrent::DownloadPriority::Ignored;

    m_bar->setStatus(downloaded, downloading, available, enabled);
    m_bar->setFixedSize(option.rect.width(), option.rect.height());
    painter->save();
    painter->translate(option.rect.x(), option.rect.y());
    m_bar->paintEvent(painter);
    painter->restore();
}

TorrentPiecesModelDelegate::TorrentPiecesModelDelegate(QTreeView *parent)
    : QStyledItemDelegate {parent}
    , m_parent(parent)
    , m_bar(new PieceStatusBar(parent))
{
    m_bar->setVisible(false);
}

void TorrentPiecesModelDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    auto item  = static_cast<TorrentPiecesModel*>(index.internalPointer());
    PiecesDelegate::paint(painter, option, m_bar, item->m_torrent, item->m_rangeStart, item->m_rangeEnd);
}

TorrentFilePiecesDelegate::TorrentFilePiecesDelegate(QTreeView *parent)
    : QStyledItemDelegate {parent}
    , m_parent(parent)
    , m_bar(new PieceStatusBar(parent))
{
    m_bar->setVisible(false);
}

void TorrentFilePiecesDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (!m_torrent)
        return;

    const auto filterModel = static_cast<const TorrentContentFilterModel*>(index.model());
    if (filterModel->itemType(index) != TorrentContentModelItem::FileType)
        return;
    auto fileIndex = filterModel->getFileIndex(index);

    auto range = m_torrent->info().filePieces(fileIndex);
    auto rangeStart = *range.begin();
    auto rangeEnd = *range.end();

    PiecesDelegate::paint(painter, option, m_bar, m_torrent, rangeStart, rangeEnd);
}
