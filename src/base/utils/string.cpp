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

#include <QByteArray>
#include <QCollator>
#include <QtGlobal>
#include <QLocale>
#include <QRegExp>
#ifdef Q_OS_MAC
#include <QThreadStorage>
#endif

namespace
{
    class NaturalCompare
    {
    public:
        explicit NaturalCompare(const bool caseSensitive = true)
            : m_caseSensitive(caseSensitive)
        {
#if defined(Q_OS_WIN)
            // Without ICU library, QCollator uses the native API on Windows 7+. But that API
            // sorts older versions of μTorrent differently than the newer ones because the
            // 'μ' character is encoded differently and the native API can't cope with that.
            // So default to using our custom natural sorting algorithm instead.
            // See #5238 and #5240
            // Without ICU library, QCollator doesn't support `setNumericMode(true)` on OS older than Win7
            // if (QSysInfo::windowsVersion() < QSysInfo::WV_WINDOWS7)
                return;
#endif
            m_collator.setNumericMode(true);
            m_collator.setCaseSensitivity(caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
        }

        bool operator()(const QString &left, const QString &right) const
        {
#if defined(Q_OS_WIN)
            // Without ICU library, QCollator uses the native API on Windows 7+. But that API
            // sorts older versions of μTorrent differently than the newer ones because the
            // 'μ' character is encoded differently and the native API can't cope with that.
            // So default to using our custom natural sorting algorithm instead.
            // See #5238 and #5240
            // Without ICU library, QCollator doesn't support `setNumericMode(true)` on OS older than Win7
            // if (QSysInfo::windowsVersion() < QSysInfo::WV_WINDOWS7)
                return lessThan(left, right);
#endif
            return (m_collator.compare(left, right) < 0);
        }

        bool lessThan(const QString &left, const QString &right) const
        {
            // Return value `false` indicates `right` should go before `left`, otherwise, after
            int posL = 0;
            int posR = 0;
            while (true) {
                while (true) {
                    if ((posL == left.size()) || (posR == right.size()))
                        return (left.size() < right.size());  // when a shorter string is another string's prefix, shorter string place before longer string

                    QChar leftChar = m_caseSensitive ? left[posL] : left[posL].toLower();
                    QChar rightChar = m_caseSensitive ? right[posR] : right[posR].toLower();
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
                int numL = left.midRef(startL, posL - startL).toInt();

                int startR = posR;
                while ((posR < right.size()) && right[posR].isDigit())
                    ++posR;
                int numR = right.midRef(startR, posR - startR).toInt();

                if (numL != numR)
                    return (numL < numR);

                // Strings + digits do match and we haven't hit string end
                // Do another round
            }
            return false;
        }

    private:
        QCollator m_collator;
        const bool m_caseSensitive;
    };
}

bool Utils::String::naturalCompareCaseSensitive(const QString &left, const QString &right)
{
    // provide a single `NaturalCompare` instance for easy use
    // https://doc.qt.io/qt-5/threads-reentrancy.html
#ifdef Q_OS_MAC  // workaround for Apple xcode: https://stackoverflow.com/a/29929949
    static QThreadStorage<NaturalCompare> nCmp;
    if (!nCmp.hasLocalData()) nCmp.setLocalData(NaturalCompare(true));
    return (nCmp.localData())(left, right);
#else
    thread_local NaturalCompare nCmp(true);
    return nCmp(left, right);
#endif
}

bool Utils::String::naturalCompareCaseInsensitive(const QString &left, const QString &right)
{
    // provide a single `NaturalCompare` instance for easy use
    // https://doc.qt.io/qt-5/threads-reentrancy.html
#ifdef Q_OS_MAC  // workaround for Apple xcode: https://stackoverflow.com/a/29929949
    static QThreadStorage<NaturalCompare> nCmp;
    if (!nCmp.hasLocalData()) nCmp.setLocalData(NaturalCompare(false));
    return (nCmp.localData())(left, right);
#else
    thread_local NaturalCompare nCmp(false);
    return nCmp(left, right);
#endif
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

// This is marked as internal in QRegExp.cpp, but is exported. The alternative would be to
// copy the code from QRegExp::wc2rx().
QString qt_regexp_toCanonical(const QString &pattern, QRegExp::PatternSyntax patternSyntax);

QString Utils::String::wildcardToRegex(const QString &pattern)
{
    return qt_regexp_toCanonical(pattern, QRegExp::Wildcard);
}

