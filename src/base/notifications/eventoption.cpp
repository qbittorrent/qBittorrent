/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015 Eugene Shalygin
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

#include "eventoption.h"

#include <type_traits>
#include <QObject>

Notifications::EventDescription::EventDescription(const IdType &id)
    : m_id {id}
    , m_enabledByDefault {false}

{
}

Notifications::EventDescription::ThisType &Notifications::EventDescription::name(const QString &v)
{
    m_name = v;
    return *this;
}

Notifications::EventDescription::ThisType &Notifications::EventDescription::description(const QString &v)
{
    m_description = v;
    return *this;
}

Notifications::EventDescription::ThisType &Notifications::EventDescription::enabledByDefault(bool v)
{
    m_enabledByDefault = v;
    return *this;
}

const std::string &Notifications::EventDescription::id() const
{
    return m_id;
}

const QString &Notifications::EventDescription::name() const
{
    return m_name;
}

const QString &Notifications::EventDescription::description() const
{
    return m_description;
}

bool Notifications::EventDescription::isEnabledByDefault() const
{
    return m_enabledByDefault;
}

void Notifications::EventDescription::assertValidity() const
{
    Q_ASSERT(!m_id.empty());
    Q_ASSERT(!m_name.isEmpty());
    Q_ASSERT(!m_description.isEmpty());
}

bool Notifications::operator==(const Notifications::EventDescription &left, const Notifications::EventDescription &right)
{
    return left.id() == right.id();
}

static_assert(std::is_move_constructible<Notifications::EventDescription>::value, "...");
static_assert(std::is_move_assignable<Notifications::EventDescription>::value, "...");
