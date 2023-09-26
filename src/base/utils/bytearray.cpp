/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2023  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2018  Mike Tzou (Chocobo1)
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

#include "bytearray.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QList>

QList<QByteArrayView> Utils::ByteArray::splitToViews(const QByteArrayView in, const QByteArrayView sep, const Qt::SplitBehavior behavior)
{
    if (sep.isEmpty())
        return {in};

    QList<QByteArrayView> ret;
    ret.reserve((behavior == Qt::KeepEmptyParts)
                ? (1 + (in.size() / sep.size()))
                : (1 + (in.size() / (sep.size() + 1))));
    int head = 0;
    while (head < in.size())
    {
        int end = in.indexOf(sep, head);
        if (end < 0)
            end = in.size();

        // omit empty parts
        const QByteArrayView part = in.mid(head, (end - head));
        if (!part.isEmpty() || (behavior == Qt::KeepEmptyParts))
            ret += part;

        head = end + sep.size();
    }

    return ret;
}

QByteArray Utils::ByteArray::asQByteArray(const QByteArrayView view)
{
    // `QByteArrayView::toByteArray()` will deep copy the data
    // So we provide our own fast path for appropriate situations/code
    return QByteArray::fromRawData(view.constData(), view.size());
}

QByteArray Utils::ByteArray::toBase32(const QByteArray &in)
{
    const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    const char padchar = '=';

    const qsizetype inSize = in.size();

    auto tmp = QByteArray((inSize + 4) / 5 * 8, Qt::Uninitialized);
    qsizetype inIndex = 0;
    char *out = tmp.data();
    while (inIndex < inSize)
    {
        // encode 5 bytes at a time
        qsizetype inPadLen = 5;
        int64_t chunk = 0;
        while (inPadLen > 0)
        {
            chunk |= static_cast<int64_t>(static_cast<uchar>(in.data()[inIndex++])) << (--inPadLen * 8);
            if (inIndex == inSize)
                break;
        }

        const int outCharCounts[] = {8, 7, 5, 4, 2};
        for (int i = 7; i >= 0; --i)
        {
            if (i >= (8 - outCharCounts[inPadLen]))
            {
                const int shift = (i * 5);
                const int64_t mask = static_cast<int64_t>(0x1f) << shift;
                const int charIndex = (chunk & mask) >> shift;
                *out++ = alphabet[charIndex];
            }
            else
            {
                *out++ = padchar;
            }
        }
    }

    return tmp;
}
