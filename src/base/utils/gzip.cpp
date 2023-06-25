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

#include "gzip.h"

#include <vector>

#include <QtAssert>
#include <QBuffer>
#include <QByteArray>
#include <QIODevice>

#ifndef ZLIB_CONST
#define ZLIB_CONST  // make z_stream.next_in const
#endif
#include <zlib.h>

bool Utils::Gzip::compress(QIODevice &source, QIODevice &dest, const int level)
{
    const unsigned int chunkSize = 128 * 1024;

    z_stream strm {};
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;

    std::vector<char> in(chunkSize);
    std::vector<char> out(chunkSize);

    // windowBits = 15 + 16 to enable gzip
    // From the zlib manual: windowBits can also be greater than 15 for optional gzip encoding. Add 16 to windowBits
    // to write a simple gzip header and trailer around the compressed data instead of a zlib wrapper.
    int ret = deflateInit2(&strm, level, Z_DEFLATED, (15 + 16), 9, Z_DEFAULT_STRATEGY);
    if (ret != Z_OK)
        return false;

    int flush = Z_NO_FLUSH;
    do
    {
        Q_ASSERT(source.isReadable());
        const qsizetype readBytes = source.read(in.data(), chunkSize);
        if (readBytes == -1)
        {
            deflateEnd(&strm);
            return false;
        }

        flush = source.atEnd() ? Z_FINISH : Z_NO_FLUSH;
        strm.avail_in = readBytes;
        strm.next_in = reinterpret_cast<const Bytef *>(in.data());

        do
        {
            strm.avail_out = chunkSize;
            strm.next_out = reinterpret_cast<Bytef *>(out.data());

            ret = deflate(&strm, flush);
            Q_ASSERT(ret != Z_STREAM_ERROR);

            Q_ASSERT(dest.isWritable());
            const qsizetype have = chunkSize - strm.avail_out;
            if (dest.write(out.data(), have) == -1)
            {
                deflateEnd(&strm);
                return false;
            }
        } while (strm.avail_out == 0);
        Q_ASSERT(strm.avail_in == 0);
    } while (flush != Z_FINISH);
    Q_ASSERT(ret == Z_STREAM_END);

    deflateEnd(&strm);
    return true;
}

QByteArray Utils::Gzip::compress(const QByteArray &data, const int level, bool *ok)
{
    if (ok)
        *ok = false;

    if (data.isEmpty())
        return {};

    z_stream strm {};
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.next_in = reinterpret_cast<const Bytef *>(data.constData());
    strm.avail_in = static_cast<uInt>(data.size());

    // windowBits = 15 + 16 to enable gzip
    // From the zlib manual: windowBits can also be greater than 15 for optional gzip encoding. Add 16 to windowBits
    // to write a simple gzip header and trailer around the compressed data instead of a zlib wrapper.
    const int initResult = deflateInit2(&strm, level, Z_DEFLATED, (15 + 16), 9, Z_DEFAULT_STRATEGY);
    if (initResult != Z_OK)
        return {};

    QByteArray ret {static_cast<qsizetype>(deflateBound(&strm, data.size())), Qt::Uninitialized};
    strm.next_out = reinterpret_cast<Bytef *>(ret.data());
    strm.avail_out = ret.size();

    // From the zlib manual: Z_FINISH can be used in the first deflate call after deflateInit if all the compression
    // is to be done in a single step. In order to complete in one call, avail_out must be at least the value
    // returned by deflateBound (see below). Then deflate is guaranteed to return Z_STREAM_END.
    const int deflateResult = deflate(&strm, Z_FINISH);
    Q_ASSERT(deflateResult == Z_STREAM_END);

    deflateEnd(&strm);
    ret.truncate(strm.total_out);

    if (ok)
        *ok = true;
    return ret;
}

bool Utils::Gzip::decompress(QIODevice &source, QIODevice &dest)
{
    const unsigned int chunkSize = 128 * 1024;

    z_stream strm {};
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;

    std::vector<char> in(chunkSize);
    std::vector<char> out(chunkSize);

    // windowBits must be greater than or equal to the windowBits value provided to deflateInit2() while compressing
    // Add 32 to windowBits to enable zlib and gzip decoding with automatic header detection
    int ret = inflateInit2(&strm, (15 + 32));
    if (ret != Z_OK)
        return false;

    do
    {
        Q_ASSERT(source.isReadable());
        const qsizetype readBytes = source.read(in.data(), chunkSize);
        if (readBytes == -1)
        {
            inflateEnd(&strm);
            return false;
        }

        if (readBytes == 0)
        {
            break;
        }

        strm.avail_in = readBytes;
        strm.next_in = reinterpret_cast<const Bytef *>(in.data());

        do
        {
            strm.avail_out = chunkSize;
            strm.next_out = reinterpret_cast<Bytef *>(out.data());

            ret = inflate(&strm, Z_NO_FLUSH);
            Q_ASSERT(ret != Z_STREAM_ERROR);

            switch (ret)
            {
            case Z_NEED_DICT:
                ret = Z_DATA_ERROR;
                break;
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
                inflateEnd(&strm);
                return false;
            }

            Q_ASSERT(dest.isWritable());
            const qsizetype have = chunkSize - strm.avail_out;
            if (dest.write(out.data(), have) == -1)
            {
                inflateEnd(&strm);
                return false;
            }
        } while (strm.avail_out == 0);
        Q_ASSERT(strm.avail_in == 0);
    } while (ret != Z_STREAM_END);

    inflateEnd(&strm);
    return ret == Z_STREAM_END;
}

QByteArray Utils::Gzip::decompress(const QByteArray &data, bool *ok)
{
    if (ok)
        *ok = false;

    if (data.isEmpty())
        return {};

    QByteArray output;
    // from lzbench, level 9 average compression ratio is: 31.92%, which decompression ratio is: 1 / 0.3192 = 3.13
    output.reserve(data.size() * 3);
    QBuffer source {const_cast<QByteArray *>(&data)};
    source.open(QIODevice::ReadOnly);
    QBuffer dest {&output};
    dest.open(QIODevice::WriteOnly);

    if (Utils::Gzip::decompress(source, dest) && ok)
        *ok = true;

    source.close();
    dest.close();
    return output;
}
