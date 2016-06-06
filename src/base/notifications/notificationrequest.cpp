/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2016 Eugene Shalygin
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

#include "notificationrequest.h"

#include <utility>
#include <QDebug>

#include "base/bittorrent/torrenthandle.h"
#include "notificationsmanager.h"
#include "notifier.h"

Notifications::Request::Request()
    : m_title(QObject::tr("Notification"))
    , m_message(QObject::tr("Notification from qBittorrent"))
    , m_category(Category::Generic)
    , m_urgency(Urgency::Normal)
    , m_severity(Severity::No)
    , m_torrentHash(BitTorrent::InfoHash())
    , m_timeout(-1)
{
}

QString Notifications::Request::title() const
{
    return m_title;
}

QString Notifications::Request::message() const
{
    return m_message;
}

Notifications::Category Notifications::Request::category() const
{
    return m_category;
}

Notifications::Urgency Notifications::Request::urgency() const
{
    return m_urgency;
}

Notifications::Severity Notifications::Request::severity() const
{
    return m_severity;
}

BitTorrent::InfoHash Notifications::Request::torrent() const
{
    return m_torrentHash;
}

const Notifications::Request::ActionsMap &Notifications::Request::actions() const
{
    return m_actions;
}

int Notifications::Request::timeout() const
{
    return m_timeout;
}

const QVariant &Notifications::Request::userData() const
{
    return m_userData;
}

Notifications::Request::this_type &Notifications::Request::title(const QString &title)
{
    m_title = title;
    return *this;
}

Notifications::Request::this_type &Notifications::Request::message(const QString &message)
{
    m_message = message;
    return *this;
}

Notifications::Request::this_type &Notifications::Request::category(Category category)
{
    m_category = category;
    return *this;
}

Notifications::Request::this_type &Notifications::Request::urgency(Urgency urgency)
{
    m_urgency = urgency;
    return *this;
}

Notifications::Request::this_type &Notifications::Request::severity(Severity severity)
{
    m_severity = severity;
    return *this;
}

Notifications::Request::this_type &Notifications::Request::torrent(BitTorrent::InfoHash &infoHash)
{
    m_torrentHash = infoHash;
    return *this;
}

Notifications::Request::this_type &Notifications::Request::torrent(BitTorrent::TorrentHandle *torrent)
{
    m_torrentHash = torrent ? torrent->info().hash() : BitTorrent::InfoHash();
    return *this;
}

Notifications::Request::this_type &Notifications::Request::timeout(int timeout)
{
    m_timeout = timeout;
    return *this;
}

Notifications::Request::this_type &Notifications::Request::userData(const QVariant &data)
{
    m_userData = data;
    return *this;
}

Notifications::Request::this_type &Notifications::Request::addAction(
    const QString &id, const QString &label, ActionHandler handler)
{
    if (!m_actions.contains(id))
        m_actions[id] = {handler, label};
    else
        qDebug() << "Action with id " << id << " already exists. Skipping.";
    return *this;
}

void Notifications::Request::exec() const
& {
    Manager::instance().notifier()->showNotification(*this);
}

void Notifications::Request::exec() &&
{
    Manager::instance().notifier()->showNotification(std::move(*this));
}
