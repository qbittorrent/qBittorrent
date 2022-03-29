#pragma once

#include <QStyledItemDelegate>
#include <QTreeView>

#include "pieceavailabilitybar.h"
#include "downloadedpiecesbar.h"
#include "gui/progressbarpainter.h"
#include "base/bittorrent/torrent.h"
#include "base/bittorrent/torrentinfo.h"

class QAbstractItemModel;
class QModelIndex;
class QStyleOptionViewItem;

class PiecesDelegate final : public QStyledItemDelegate
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PiecesDelegate)

public:
    explicit PiecesDelegate(QTreeView *parent);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    static void paint(QPainter *painter, const QStyleOptionViewItem &option, PieceStatusBar *m_bar, BitTorrent::Torrent *m_torrent, int rangeStart, int rangeEnd);

private:
    QTreeView *m_parent;
    PieceAvailabilityBar *m_bar;
};

class TorrentPiecesModelDelegate final : public QStyledItemDelegate
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TorrentPiecesModelDelegate)

public:
    explicit TorrentPiecesModelDelegate(QTreeView *parent);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    QTreeView *m_parent;
    PieceStatusBar *m_bar;
};


class TorrentFilePiecesDelegate final : public QStyledItemDelegate
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TorrentFilePiecesDelegate)

public:
    explicit TorrentFilePiecesDelegate(QTreeView *parent);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    BitTorrent::Torrent *m_torrent;

private:
    QTreeView *m_parent;
    PieceStatusBar *m_bar;
};
