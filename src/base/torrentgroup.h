/*
 * Virtual torrent grouping feature (experimental).
 * Lightweight grouping concept for UI aggregation (e.g. Season 1 grouping several episode torrents).
 */

#pragma once

#include <QHash>
#include <QSet>
#include <QString>
#include <QObject>

#include "base/bittorrent/infohash.h" // for BitTorrent::TorrentID

struct TorrentGroup
{
    QString name;  // unique
    QSet<BitTorrent::TorrentID> members;
    bool isValid() const { return !name.trimmed().isEmpty(); }
};

class TorrentGroupManager final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TorrentGroupManager)

    explicit TorrentGroupManager(QObject *parent = nullptr);
public:
    static TorrentGroupManager *instance();

    QList<TorrentGroup> groups() const;
    bool hasGroup(const QString &name) const;
    TorrentGroup group(const QString &name) const;

    bool createGroup(const QString &name, const QSet<BitTorrent::TorrentID> &initialMembers = {});
    bool renameGroup(const QString &oldName, const QString &newName);
    bool deleteGroup(const QString &name);
    bool addMembers(const QString &groupName, const QSet<BitTorrent::TorrentID> &members);
    bool removeMembers(const QString &groupName, const QSet<BitTorrent::TorrentID> &members);

    QString groupOf(const BitTorrent::TorrentID &id) const; // single group per torrent for MVP

    void load();
    void save() const;

    QStringList expandedGroups() const { return m_expandedGroups; }
    void setExpandedGroups(const QStringList &names);

signals:
    void groupsChanged();
    void groupMembershipChanged(const QString &groupName);

private:
    static TorrentGroupManager *m_instance;
    QHash<QString, TorrentGroup> m_groups; // key: name
    QStringList m_expandedGroups;
};
