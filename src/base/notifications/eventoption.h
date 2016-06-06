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

#ifndef QBT_NOTIFICATIONS_EVENTOPTION_H
#define QBT_NOTIFICATIONS_EVENTOPTION_H

#include <string>
#include <QString>

class QObject;

namespace Notifications
{
    class EventDescription
    {
    public:
        using ThisType = EventDescription;
        using IdType = std::string;
        /// For the sake of code readability this constructor creates *invalid* object
        /// All the setters have to be invoked with valid parameters prior to any manipulations with
        /// this object.
        explicit EventDescription(const IdType &id);


        ThisType &name(const QString &v);
        ThisType &description(const QString &v);
        ThisType &enabledByDefault(bool v);

        const IdType &id() const;
        const QString &name() const;
        const QString &description() const;
        bool isEnabledByDefault() const;

    protected:
        void assertValidity() const;

    private:
        IdType m_id;
        QString m_name;
        QString m_description;
        bool m_enabledByDefault;
    };

    // compares by id
    bool operator==(const EventDescription &left, const EventDescription &right);
}

#endif // QBT_NOTIFICATIONS_EVENTOPTION_H
