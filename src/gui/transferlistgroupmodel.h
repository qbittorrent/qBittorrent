#pragma once

#include <QAbstractItemModel>
#include <QVector>
#include <QHash>

#include "transferlistmodel.h"
#include "base/torrentgroup.h"

// A proxy-like aggregate model that exposes synthetic parent rows for groups.
// Child rows are underlying torrents. Delegate still works on DisplayRole.
class TransferListGroupModel final : public QAbstractItemModel
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TransferListGroupModel)
public:
    explicit TransferListGroupModel(TransferListModel *source, QObject *parent = nullptr);

    QModelIndex index(int row, int column, const QModelIndex &parent) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    void rebuild();

    TransferListModel *sourceModel() const { return m_source; }
    BitTorrent::Torrent *torrentHandle(const QModelIndex &index) const; // nullptr for group parents
    QString groupName(const QModelIndex &index) const;
    void setGroupExpanded(const QString &name, bool expanded);

private slots:
    void handleSourceChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QList<int> &roles = {});
    void handleSourceReset();

private:
    struct GroupItem { QString name; QList<BitTorrent::Torrent*> members; };

    TransferListModel *m_source;
    QVector<GroupItem> m_groups; // ordered
    QList<BitTorrent::Torrent*> m_ungrouped; // top-level leaves after groups
    QHash<QString, int> m_groupRowByName; // name -> row in m_groups
};
