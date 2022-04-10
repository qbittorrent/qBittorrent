/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2022  Mike Tzou (Chocobo1)
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

#include "ltqbitarray.h"

#include <libtorrent/bitfield.hpp>

#include <QBitArray>
#include <QVarLengthArray>

namespace
{
    unsigned char reverseByte(const unsigned char byte)
    {
        // https://graphics.stanford.edu/~seander/bithacks.html#BitReverseTable
        static const unsigned char table[] =
        {
#define R2(n) n, (n + (2 * 64)), (n + 64), (n + (3 * 64))
#define R4(n) R2(n), R2(n + (2 * 16)), R2(n + 16), R2(n + (3 * 16))
#define R6(n) R4(n), R4(n + (2 * 4)), R4(n + 4), R4(n + (3 * 4))
            R6(0), R6(2), R6(1), R6(3)
#undef R6
#undef R4
#undef R2
        };
        return table[byte];
    }
}

namespace BitTorrent::LT
{
    QBitArray toQBitArray(const lt::bitfield &bits)
    {
        const int STACK_ALLOC_SIZE = 10 * 1024;

        const char *bitsData = bits.data();
        const int dataLength = (bits.size() + 7) / 8;

        QVarLengthArray<char, STACK_ALLOC_SIZE> tmp(dataLength);
        for (int i = 0; i < dataLength; ++i)
            tmp[i] = reverseByte(bitsData[i]);

        return QBitArray::fromBits(tmp.data(), bits.size());
    }
}
