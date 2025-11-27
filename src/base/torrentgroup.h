/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2025  AlfEspadero
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

#include <QHash>
#include <QSet>
#include <QString>
#include <QObject>

#include "base/bittorrent/infohash.h" // for BitTorrent::TorrentID

struct TorrentGroup
{
    QString name;  // unique
    QSet<BitTorrent::TorrentID> members;
    bool isValid() const
    {
        return !name.trimmed().isEmpty();
    }
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

    QStringList expandedGroups() const
    {
        return m_expandedGroups;
    }
    void setExpandedGroups(const QStringList &names);

signals:
    void groupsChanged();
    void groupMembershipChanged(const QString &groupName);

private:
    static TorrentGroupManager *m_instance;
    QHash<QString, TorrentGroup> m_groups; // key: name
    QStringList m_expandedGroups;
};
