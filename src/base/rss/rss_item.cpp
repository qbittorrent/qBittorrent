/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2010  Christophe Dumez <chris@qbittorrent.org>
 * Copyright (C) 2010  Arnaud Demaiziere <arnaud@qbittorrent.org>
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

#include "rss_item.h"

#include <QDebug>
#include <QRegularExpression>
#include <QStringList>

#include "base/global.h"

using namespace RSS;

const QChar Item::PathSeparator = u'\\';

Item::Item(const QString &path)
    : m_path(path)
{
}

void Item::setPath(const QString &path)
{
    if (path != m_path)
    {
        m_path = path;
        emit pathChanged(this);
    }
}

QString Item::path() const
{
    return m_path;
}

QString Item::name() const
{
    return relativeName(path());
}

bool Item::isValidPath(const QString &path)
{
    const QRegularExpression re(
                uR"(\A[^\%1]+(\%1[^\%1]+)*\z)"_s.arg(Item::PathSeparator)
                , QRegularExpression::DontCaptureOption);

    if (path.isEmpty() || !re.match(path).hasMatch())
    {
        qDebug() << "Incorrect RSS Item path:" << path;
        return false;
    }

    return true;
}

QString Item::joinPath(const QString &path1, const QString &path2)
{
    if (path1.isEmpty())
        return path2;

    return (path1 + Item::PathSeparator + path2);
}

QStringList Item::expandPath(const QString &path)
{
    QStringList result;
    if (path.isEmpty()) return result;
    //    if (!isValidRSSFolderName(folder))
    //        return result;

    int index = 0;
    while ((index = path.indexOf(Item::PathSeparator, index)) >= 0)
    {
        result << path.left(index);
        ++index;
    }
    result << path;

    return result;
}

QString Item::parentPath(const QString &path)
{
    const int pos = path.lastIndexOf(Item::PathSeparator);
    return (pos >= 0) ? path.left(pos) : QString();
}

QString Item::relativeName(const QString &path)
{
    int pos;
    return ((pos = path.lastIndexOf(Item::PathSeparator)) >= 0 ? path.right(path.size() - (pos + 1)) : path);
}
