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

#include "torrentgroup.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "base/settingvalue.h"
#include "base/bittorrent/infohash.h"

namespace
{
    const QString kPrefKey = QStringLiteral("TorrentGroups/Groups");
}

TorrentGroupManager *TorrentGroupManager::m_instance = nullptr;

TorrentGroupManager::TorrentGroupManager(QObject *parent)
    : QObject(parent)
{
}

TorrentGroupManager *TorrentGroupManager::instance()
{
    static TorrentGroupManager guard; // static lifetime
    if (!m_instance)
        m_instance = &guard;
    return m_instance;
}

QList<TorrentGroup> TorrentGroupManager::groups() const
{
    return m_groups.values();
}

bool TorrentGroupManager::hasGroup(const QString &name) const
{
    return m_groups.contains(name);
}

TorrentGroup TorrentGroupManager::group(const QString &name) const
{
    return m_groups.value(name, {});
}

bool TorrentGroupManager::createGroup(const QString &name, const QSet<BitTorrent::TorrentID> &initialMembers)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty() || hasGroup(trimmed))
        return false;
    TorrentGroup g;
    g.name = trimmed;
    g.members = initialMembers;
    m_groups.insert(trimmed, g);
    emit groupsChanged();
    if (!initialMembers.isEmpty())
        emit groupMembershipChanged(trimmed);
    return true;
}

bool TorrentGroupManager::renameGroup(const QString &oldName, const QString &newName)
{
    if (!hasGroup(oldName))
        return false;
    const QString trimmed = newName.trimmed();
    if (trimmed.isEmpty() || hasGroup(trimmed))
        return false;
    TorrentGroup g = m_groups.take(oldName);
    g.name = trimmed;
    m_groups.insert(trimmed, g);
    // migrate expanded state if needed
    if (m_expandedGroups.contains(oldName))
    {
        m_expandedGroups.removeAll(oldName);
        if (!m_expandedGroups.contains(trimmed))
            m_expandedGroups << trimmed;
        save(); // persist change including expansion mapping
    }
    emit groupsChanged();
    return true;
}

bool TorrentGroupManager::deleteGroup(const QString &name)
{
    if (!hasGroup(name))
        return false;
    m_groups.remove(name);
    emit groupsChanged();
    return true;
}

bool TorrentGroupManager::addMembers(const QString &groupName, const QSet<BitTorrent::TorrentID> &members)
{
    if (!hasGroup(groupName) || members.isEmpty())
        return false;
    TorrentGroup &g = m_groups[groupName];
    const int oldSize = g.members.size();
    g.members.unite(members);
    if (g.members.size() != oldSize)
        emit groupMembershipChanged(groupName);
    return true;
}

bool TorrentGroupManager::removeMembers(const QString &groupName, const QSet<BitTorrent::TorrentID> &members)
{
    if (!hasGroup(groupName) || members.isEmpty())
        return false;
    TorrentGroup &g = m_groups[groupName];
    bool changed = false;
    for (const BitTorrent::TorrentID &id : members)
        changed |= g.members.remove(id) > 0;
    if (changed)
        emit groupMembershipChanged(groupName);
    return changed;
}

QString TorrentGroupManager::groupOf(const BitTorrent::TorrentID &id) const
{
    for (const TorrentGroup &g : m_groups)
    {
        if (g.members.contains(id))
            return g.name;
    }
    return {};
}

void TorrentGroupManager::load()
{
    m_groups.clear();
    m_expandedGroups.clear();
    SettingValue<QByteArray> rawSetting {kPrefKey};
    const QByteArray raw = rawSetting.get();
    if (raw.isEmpty())
        return;
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject())
        return;
    const QJsonObject root = doc.object();
    const QJsonArray groupsArr = root.value(QStringLiteral("groups")).toArray();
    for (const QJsonValue &val : groupsArr)
    {
        if (!val.isObject())
            continue;
        const QJsonObject obj = val.toObject();
        const QString name = obj.value(QStringLiteral("name")).toString();
        if (name.trimmed().isEmpty())
            continue;
        TorrentGroup g;
        g.name = name;
        const QJsonArray memArr = obj.value(QStringLiteral("members")).toArray();
        for (const QJsonValue &mVal : memArr)
            g.members.insert(BitTorrent::TorrentID::fromString(mVal.toString()));
        m_groups.insert(g.name, g);
    }
    const QJsonArray expandedArr = root.value(QStringLiteral("expanded")).toArray();
    for (const QJsonValue &v : expandedArr)
        m_expandedGroups << v.toString();
    emit groupsChanged();
}

void TorrentGroupManager::save() const
{
    QJsonArray groupsArr;
    for (const TorrentGroup &g : m_groups)
    {
        QJsonObject obj;
        obj.insert(QStringLiteral("name"), g.name);
        QJsonArray memArr;
        for (const BitTorrent::TorrentID &id : g.members)
            memArr.append(id.toString());
        obj.insert(QStringLiteral("members"), memArr);
        groupsArr.append(obj);
    }
    QJsonArray expandedArr;
    for (const QString &n : m_expandedGroups)
        expandedArr.append(n);
    QJsonObject root;
    root.insert(QStringLiteral("groups"), groupsArr);
    root.insert(QStringLiteral("expanded"), expandedArr);
    const QJsonDocument doc(root);
    SettingValue<QByteArray> rawSetting {kPrefKey};
    rawSetting = doc.toJson(QJsonDocument::Compact);
}

void TorrentGroupManager::setExpandedGroups(const QStringList &names)
{
    m_expandedGroups = names;
    save(); // persist immediately for now
}
