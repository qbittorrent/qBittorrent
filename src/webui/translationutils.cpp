/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Thomas Piccirello <thomas@piccirello.com>
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

#include "translationutils.h"

#include <QString>
#include <QStringView>

using namespace Qt::Literals::StringLiterals;

// `&` is kept so that character references in the string still render
QString TranslationUtils::escapeHTML(const QStringView str)
{
    QString result;
    result.reserve(str.size());
    for (const QChar c : str)
    {
        switch (c.unicode())
        {
        case u'<':
            result += u"&lt;"_s;
            break;
        case u'>':
            result += u"&gt;"_s;
            break;
        case u'"':
            result += u"&quot;"_s;
            break;
        case u'\'':
            result += u"&#39;"_s;
            break;
        default:
            result += c;
            break;
        }
    }
    return result;
}

// the string may already contain some JS escape sequences (e.g. `\n`), those are kept
QString TranslationUtils::escapeJSString(const QStringView str)
{
    const auto escape = [](const char16_t c) -> QString
    {
        switch (c)
        {
        case u'\\':
            return u"\\\\"_s;
        case u'"':
            return u"\\\""_s;
        case u'\'':
            return u"\\'"_s;
        case u'`':
            return u"\\`"_s;
        case u'$':
            return u"\\$"_s;
        case u'\n':
            return u"\\n"_s;
        case u'\r':
            return u"\\r"_s;
        case u'\t':
            return u"\\t"_s;
        case u'<':
            return u"\\u003C"_s;
        case u' ':
            return u"\\u2028"_s;
        case u' ':
            return u"\\u2029"_s;
        default:
            if (c < 0x20)
                return u"\\u%1"_s.arg(static_cast<int>(c), 4, 16, u'0');
            return QChar(c);
        }
    };

    QString result;
    result.reserve(str.size());
    for (qsizetype i = 0; i < str.size(); ++i)
    {
        char16_t c = str[i].unicode();
        if ((c == u'\\') && ((i + 1) < str.size()))
        {
            const char16_t next = str[i + 1].unicode();
            switch (next)
            {
            case u'n':
                c = u'\n';
                ++i;
                break;
            case u'r':
                c = u'\r';
                ++i;
                break;
            case u't':
                c = u'\t';
                ++i;
                break;
            case u'\\':
            case u'"':
            case u'\'':
            case u'`':
            case u'$':
                c = next;
                ++i;
                break;
            default:
                break;
            }
        }
        result += escape(c);
    }
    return result;
}
