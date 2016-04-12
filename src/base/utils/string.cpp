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

#include <QByteArray>
#include <QString>
#include <QLocale>
#include <cmath>
#include "string.h"

QString Utils::String::fromStdString(const std::string &str)
{
    return QString::fromUtf8(str.c_str());
}

std::string Utils::String::toStdString(const QString &str)
{
    QByteArray utf8 = str.toUtf8();
    return std::string(utf8.constData(), utf8.length());
}

Utils::String::NaturalCompare::NaturalCompare()
{
#if (QT_VERSION >= QT_VERSION_CHECK(5, 2, 0))
#if defined(Q_OS_WIN)
    // Without ICU library, QCollator doesn't support `setNumericMode(true)` on OS older than Win7
    if (QSysInfo::windowsVersion() < QSysInfo::WV_WINDOWS7)
        return;
#endif
    m_collator.setNumericMode(true);
    m_collator.setCaseSensitivity(Qt::CaseInsensitive);
#endif
}

bool Utils::String::NaturalCompare::operator()(const QString &left, const QString &right)
{
    // case-insensitive comparison
#if (QT_VERSION >= QT_VERSION_CHECK(5, 2, 0))
#if defined(Q_OS_WIN)
    // Without ICU library, QCollator doesn't support `setNumericMode(true)` on OS older than Win7
    if (QSysInfo::windowsVersion() < QSysInfo::WV_WINDOWS7)
        return lessThan(left, right);
#endif
    return (m_collator.compare(left, right) < 0);
#else
    return lessThan(left, right);
#endif
}

bool Utils::String::NaturalCompare::lessThan(const QString &left, const QString &right)
{
    // Return value `false` indicates `right` should go before `left`, otherwise, after
    // case-insensitive comparison
    int posL = 0;
    int posR = 0;
    while (true) {
        while (true) {
            if ((posL == left.size()) || (posR == right.size()))
                return (left.size() < right.size());  // when a shorter string is another string's prefix, shorter string place before longer string

            QChar leftChar = left[posL].toLower();
            QChar rightChar = right[posR].toLower();
            if (leftChar == rightChar)
                ;  // compare next character
            else if (leftChar.isDigit() && rightChar.isDigit())
                break; // Both are digits, break this loop and compare numbers
            else
                return leftChar < rightChar;

            ++posL;
            ++posR;
        }

        int startL = posL;
        while ((posL < left.size()) && left[posL].isDigit())
            ++posL;
#if QT_VERSION >= QT_VERSION_CHECK(5, 1, 0)
        int numL = left.midRef(startL, posL - startL).toInt();
#else
        int numL = left.mid(startL, posL - startL).toInt();
#endif

        int startR = posR;
        while ((posR < right.size()) && right[posR].isDigit())
            ++posR;
#if QT_VERSION >= QT_VERSION_CHECK(5, 1, 0)
        int numR = right.midRef(startR, posR - startR).toInt();
#else
        int numR = right.mid(startR, posR - startR).toInt();
#endif

        if (numL != numR)
            return (numL < numR);

        // Strings + digits do match and we haven't hit string end
        // Do another round
    }
    return false;
}

bool Utils::String::naturalCompare(const QString &left, const QString &right)
{
    // provide a single `NaturalCompare` instance for easy use
    static NaturalCompare nCmp;  // this is thread-safe in C++11 (stated in spec 6.7.4)
    return nCmp(left, right);
}

// to send numbers instead of strings with suffixes
QString Utils::String::fromDouble(double n, int precision)
{
    /* HACK because QString rounds up. Eg QString::number(0.999*100.0, 'f' ,1) == 99.9
    ** but QString::number(0.9999*100.0, 'f' ,1) == 100.0 The problem manifests when
    ** the number has more digits after the decimal than we want AND the digit after
    ** our 'wanted' is >= 5. In this case our last digit gets rounded up. So for each
    ** precision we add an extra 0 behind 1 in the below algorithm. */

    double prec = std::pow(10.0, precision);
    return QLocale::system().toString(std::floor(n * prec) / prec, 'f', precision);
}

// Implements constant-time comparison to protect against timing attacks
// Taken from https://crackstation.net/hashing-security.htm
bool Utils::String::slowEquals(const QByteArray &a, const QByteArray &b)
{
    int lengthA = a.length();
    int lengthB = b.length();

    int diff = lengthA ^ lengthB;
    for (int i = 0; (i < lengthA) && (i < lengthB); i++)
        diff |= a[i] ^ b[i];

    return (diff == 0);
}
