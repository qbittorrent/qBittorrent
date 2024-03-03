/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2023  Vladimir Golovnev <glassez@yandex.ru>
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

#include "tag.h"

#include <QDataStream>

#include "base/concepts/stringable.h"

namespace
{
    QString cleanTag(const QString &tag)
    {
        return tag.trimmed();
    }
}

// `Tag` should satisfy `Stringable` concept in order to be stored in settings as string
static_assert(Stringable<Tag>);

Tag::Tag(const QString &tagStr)
    : m_tagStr {cleanTag(tagStr)}
{
}

Tag::Tag(const std::string &tagStr)
    : Tag(QString::fromStdString(tagStr))
{
}

bool Tag::isValid() const
{
    if (isEmpty())
        return false;

    return !m_tagStr.contains(u',');
}

bool Tag::isEmpty() const
{
    return m_tagStr.isEmpty();
}

QString Tag::toString() const noexcept
{
    return m_tagStr;
}

Tag::operator QString() const noexcept
{
    return toString();
}

QDataStream &operator<<(QDataStream &out, const Tag &tag)
{
    out << tag.toString();
    return out;
}

QDataStream &operator>>(QDataStream &in, Tag &tag)
{
    QString tagStr;
    in >> tagStr;
    tag = Tag(tagStr);
    return in;
}
