/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
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

#include "string.h"

#include <cmath>

#include <QList>
#include <QLocale>
#include <QRegularExpression>
#include <QStringList>

// to send numbers instead of strings with suffixes
QString Utils::String::fromDouble(const double n, const int precision)
{
    /* HACK because QString rounds up. Eg QString::number(0.999*100.0, 'f', 1) == 99.9
    ** but QString::number(0.9999*100.0, 'f' ,1) == 100.0 The problem manifests when
    ** the number has more digits after the decimal than we want AND the digit after
    ** our 'wanted' is >= 5. In this case our last digit gets rounded up. So for each
    ** precision we add an extra 0 behind 1 in the below algorithm. */

    const double prec = std::pow(10.0, precision);
    return QLocale::system().toString(std::floor(n * prec) / prec, 'f', precision);
}

QString Utils::String::fromLatin1(const std::string_view string)
{
    return QString::fromLatin1(string.data(), string.size());
}

QString Utils::String::fromLocal8Bit(const std::string_view string)
{
    return QString::fromLocal8Bit(string.data(), string.size());
}

QString Utils::String::wildcardToRegexPattern(const QString &pattern)
{
    return QRegularExpression::wildcardToRegularExpression(pattern, QRegularExpression::UnanchoredWildcardConversion);
}

QStringList Utils::String::splitCommand(const QString &command)
{
    QStringList ret;
    ret.reserve(32);

    bool inQuotes = false;
    QString tmp;
    for (const QChar c : command)
    {
        if (c == u' ')
        {
            if (!inQuotes)
            {
                if (!tmp.isEmpty())
                {
                    ret.append(tmp);
                    tmp.clear();
                }

                continue;
            }
        }
        else if (c == u'"')
        {
            inQuotes = !inQuotes;
        }

        tmp.append(c);
    }

    if (!tmp.isEmpty())
        ret.append(tmp);

    return ret;
}

std::optional<bool> Utils::String::parseBool(const QString &string)
{
    if (string.compare(u"true", Qt::CaseInsensitive) == 0)
        return true;
    if (string.compare(u"false", Qt::CaseInsensitive) == 0)
        return false;

    return std::nullopt;
}

std::optional<int> Utils::String::parseInt(const QString &string)
{
    bool ok = false;
    const int result = string.toInt(&ok);
    if (ok)
        return result;

    return std::nullopt;
}

std::optional<double> Utils::String::parseDouble(const QString &string)
{
    bool ok = false;
    const double result = string.toDouble(&ok);
    if (ok)
        return result;

    return std::nullopt;
}
