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

#include <QCollator>
#include <QLocale>
#include <QRegExp>
#include <QtGlobal>
#include <QVector>

#if defined(Q_OS_MACOS) || defined(__MINGW32__)
#define QBT_USES_QTHREADSTORAGE
#include <QThreadStorage>
#endif

namespace
{
    class NaturalCompare
    {
    public:
        explicit NaturalCompare(const Qt::CaseSensitivity caseSensitivity = Qt::CaseSensitive)
            : m_caseSensitivity(caseSensitivity)
        {
#ifdef Q_OS_WIN
            // Without ICU library, QCollator uses the native API on Windows 7+. But that API
            // sorts older versions of μTorrent differently than the newer ones because the
            // 'μ' character is encoded differently and the native API can't cope with that.
            // So default to using our custom natural sorting algorithm instead.
            // See #5238 and #5240
            // Without ICU library, QCollator doesn't support `setNumericMode(true)` on an OS older than Win7
#else
            m_collator.setNumericMode(true);
            m_collator.setCaseSensitivity(caseSensitivity);
#endif
        }

        int operator()(const QString &left, const QString &right) const
        {
#ifdef Q_OS_WIN
            return compare(left, right);
#else
            return m_collator.compare(left, right);
#endif
        }

    private:
        int compare(const QString &left, const QString &right) const
        {
            // Return value <0: `left` is smaller than `right`
            // Return value >0: `left` is greater than `right`
            // Return value =0: both strings are equal

            int posL = 0;
            int posR = 0;
            while (true)
            {
                if ((posL == left.size()) || (posR == right.size()))
                    return (left.size() - right.size());  // when a shorter string is another string's prefix, shorter string place before longer string

                const QChar leftChar = (m_caseSensitivity == Qt::CaseSensitive) ? left[posL] : left[posL].toLower();
                const QChar rightChar = (m_caseSensitivity == Qt::CaseSensitive) ? right[posR] : right[posR].toLower();
                // Compare only non-digits.
                // Numbers should be compared as a whole
                // otherwise the string->int conversion can yield a wrong value
                if ((leftChar == rightChar) && !leftChar.isDigit())
                {
                    // compare next character
                    ++posL;
                    ++posR;
                }
                else if (leftChar.isDigit() && rightChar.isDigit())
                {
                    // Both are digits, compare the numbers

                    const auto numberView = [](const QString &str, int &pos) -> QStringRef
                    {
                        const int start = pos;
                        while ((pos < str.size()) && str[pos].isDigit())
                            ++pos;
                        return str.midRef(start, (pos - start));
                    };

                    const QStringRef numViewL = numberView(left, posL);
                    const QStringRef numViewR = numberView(right, posR);

                    if (numViewL.length() != numViewR.length())
                        return (numViewL.length() - numViewR.length());

                    // both string/view has the same length
                    for (int i = 0; i < numViewL.length(); ++i)
                    {
                        const QChar numL = numViewL[i];
                        const QChar numR = numViewR[i];

                        if (numL != numR)
                            return (numL.unicode() - numR.unicode());
                    }

                    // String + digits do match and we haven't hit the end of both strings
                    // then continue to consume the remainings
                }
                else
                {
                    return (leftChar.unicode() - rightChar.unicode());
                }
            }
        }

        QCollator m_collator;
        const Qt::CaseSensitivity m_caseSensitivity;
    };
}

int Utils::String::naturalCompare(const QString &left, const QString &right, const Qt::CaseSensitivity caseSensitivity)
{
    // provide a single `NaturalCompare` instance for easy use
    // https://doc.qt.io/qt-5/threads-reentrancy.html
    if (caseSensitivity == Qt::CaseSensitive)
    {
#ifdef QBT_USES_QTHREADSTORAGE
        static QThreadStorage<NaturalCompare> nCmp;
        if (!nCmp.hasLocalData())
            nCmp.setLocalData(NaturalCompare(Qt::CaseSensitive));
        return (nCmp.localData())(left, right);
#else
        thread_local NaturalCompare nCmp(Qt::CaseSensitive);
        return nCmp(left, right);
#endif
    }

#ifdef QBT_USES_QTHREADSTORAGE
    static QThreadStorage<NaturalCompare> nCmp;
    if (!nCmp.hasLocalData())
        nCmp.setLocalData(NaturalCompare(Qt::CaseInsensitive));
    return (nCmp.localData())(left, right);
#else
    thread_local NaturalCompare nCmp(Qt::CaseInsensitive);
    return nCmp(left, right);
#endif
}

// to send numbers instead of strings with suffixes
QString Utils::String::fromDouble(const double n, const int precision)
{
    /* HACK because QString rounds up. Eg QString::number(0.999*100.0, 'f' ,1) == 99.9
    ** but QString::number(0.9999*100.0, 'f' ,1) == 100.0 The problem manifests when
    ** the number has more digits after the decimal than we want AND the digit after
    ** our 'wanted' is >= 5. In this case our last digit gets rounded up. So for each
    ** precision we add an extra 0 behind 1 in the below algorithm. */

    const double prec = std::pow(10.0, precision);
    return QLocale::system().toString(std::floor(n * prec) / prec, 'f', precision);
}

// This is marked as internal in QRegExp.cpp, but is exported. The alternative would be to
// copy the code from QRegExp::wc2rx().
QString qt_regexp_toCanonical(const QString &pattern, QRegExp::PatternSyntax patternSyntax);

QString Utils::String::wildcardToRegex(const QString &pattern)
{
    return qt_regexp_toCanonical(pattern, QRegExp::Wildcard);
}

std::optional<bool> Utils::String::parseBool(const QString &string)
{
    if (string.compare("true", Qt::CaseInsensitive) == 0)
        return true;
    if (string.compare("false", Qt::CaseInsensitive) == 0)
        return false;

    return std::nullopt;
}

QString Utils::String::join(const QVector<QStringRef> &strings, const QString &separator)
{
    if (strings.empty())
        return {};

    QString ret = strings[0].toString();
    for (int i = 1; i < strings.count(); ++i)
        ret += (separator + strings[i]);
    return ret;
}
