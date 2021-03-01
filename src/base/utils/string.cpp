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

#include <QLocale>
#include <QVector>

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
#include <QRegularExpression>
#else
#include <QRegExp>
#endif

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

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
QString Utils::String::wildcardToRegexPattern(const QString &pattern)
{
    return QRegularExpression::wildcardToRegularExpression(pattern, QRegularExpression::UnanchoredWildcardConversion);
}
#else
// This is marked as internal in QRegExp.cpp, but is exported. The alternative would be to
// copy the code from QRegExp::wc2rx().
QString qt_regexp_toCanonical(const QString &pattern, QRegExp::PatternSyntax patternSyntax);

QString Utils::String::wildcardToRegexPattern(const QString &pattern)
{
    return qt_regexp_toCanonical(pattern, QRegExp::Wildcard);
}
#endif

std::optional<bool> Utils::String::parseBool(const QString &string)
{
    if (string.compare("true", Qt::CaseInsensitive) == 0)
        return true;
    if (string.compare("false", Qt::CaseInsensitive) == 0)
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

QString Utils::String::join(const QVector<QStringView> &strings, const QStringView separator)
{
    if (strings.empty())
        return {};

    QString ret = strings[0].toString();
    for (int i = 1; i < strings.count(); ++i)
        ret += (separator + strings[i]);
    return ret;
}
