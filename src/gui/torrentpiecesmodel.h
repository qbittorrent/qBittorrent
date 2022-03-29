#pragma once

#include <QAbstractItemModel>

#include "base/bittorrent/torrent.h"

class TorrentPiecesModel final
{
public:
    TorrentPiecesModel(BitTorrent::Torrent *torrent, int rangeStart, int rangeEnd);

    TorrentPiecesModel* parent() const;
    void appendChild(TorrentPiecesModel *child);
    int childCount() const;
    TorrentPiecesModel* child(int row) const;
    int row() const;

    BitTorrent::Torrent *m_torrent;
    int m_rangeStart;
    int m_rangeEnd;

private:
    TorrentPiecesModel *m_parent;
    QVector<TorrentPiecesModel*> m_children;
};

class TorrentPiecesTreeModel final : public QAbstractItemModel
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TorrentPiecesTreeModel)

public:
    enum Columns {
        RANGE,
        DOWNLOADED,
        DOWNLOADING,
        MISSING,
        STATUSBAR,

        NB_COLS
    };

    TorrentPiecesTreeModel();

    int columnCount(const QModelIndex &parent = {}) const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    TorrentPiecesModel *m_root;

private:
};
