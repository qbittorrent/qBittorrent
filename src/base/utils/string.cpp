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
#include <QtGlobal>
#include <QLocale>
#ifdef QBT_USES_QT5
#include <QCollator>
#endif
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
#ifdef QBT_USES_QT5
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
#endif
        }

        bool operator()(const QString &left, const QString &right) const
        {
#ifdef QBT_USES_QT5
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
#else
            return lessThan(left, right);
#endif
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
#ifdef QBT_USES_QT5
                int numL = left.midRef(startL, posL - startL).toInt();
#else
                int numL = left.mid(startL, posL - startL).toInt();
#endif

                int startR = posR;
                while ((posR < right.size()) && right[posR].isDigit())
                    ++posR;
#ifdef QBT_USES_QT5
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

    private:
#ifdef QBT_USES_QT5
        QCollator m_collator;
#endif
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

QString Utils::String::fromStdString(const std::string &str)
{
    return QString::fromUtf8(str.c_str());
}

std::string Utils::String::toStdString(const QString &str)
{
#ifdef QBT_USES_QT5
    return str.toStdString();
#else
    QByteArray utf8 = str.toUtf8();
    return std::string(utf8.constData(), utf8.length());
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

QString Utils::String::toHtmlEscaped(const QString &str)
{
#ifdef QBT_USES_QT5
    return str.toHtmlEscaped();
#else
    return Qt::escape(str);
#endif
}
