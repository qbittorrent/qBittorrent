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

#include <QByteArray>

#ifndef ZLIB_CONST
#define ZLIB_CONST  // make z_stream.next_in const
#endif
#include <zlib.h>

QByteArray Utils::Gzip::compress(const QByteArray &data, const int level, bool *ok)
{
    if (ok) *ok = false;

    if (data.isEmpty())
        return {};

    const int BUFSIZE = 128 * 1024;
    char tmpBuf[BUFSIZE] = {0};

    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.next_in = reinterpret_cast<const Bytef *>(data.constData());
    strm.avail_in = uInt(data.size());
    strm.next_out = reinterpret_cast<Bytef *>(tmpBuf);
    strm.avail_out = BUFSIZE;

    // windowBits = 15 + 16 to enable gzip
    // From the zlib manual: windowBits can also be greater than 15 for optional gzip encoding. Add 16 to windowBits
    // to write a simple gzip header and trailer around the compressed data instead of a zlib wrapper.
    int result = deflateInit2(&strm, level, Z_DEFLATED, (15 + 16), 9, Z_DEFAULT_STRATEGY);
    if (result != Z_OK)
        return {};

    QByteArray output;
    output.reserve(deflateBound(&strm, data.size()));

    // feed to deflate
    while (strm.avail_in > 0) {
        result = deflate(&strm, Z_NO_FLUSH);

        if (result != Z_OK) {
            deflateEnd(&strm);
            return {};
        }

        output.append(tmpBuf, (BUFSIZE - strm.avail_out));
        strm.next_out = reinterpret_cast<Bytef *>(tmpBuf);
        strm.avail_out = BUFSIZE;
    }

    // flush the rest from deflate
    while (result != Z_STREAM_END) {
        result = deflate(&strm, Z_FINISH);

        output.append(tmpBuf, (BUFSIZE - strm.avail_out));
        strm.next_out = reinterpret_cast<Bytef *>(tmpBuf);
        strm.avail_out = BUFSIZE;
    }

    deflateEnd(&strm);

    if (ok) *ok = true;
    return output;
}

bool Utils::Gzip::uncompress(QByteArray src, QByteArray &dest)
{
    dest.clear();

    if (src.size() <= 4) {
        qWarning("uncompress: Input data is truncated");
        return false;
    }

    z_stream strm;
    static const int CHUNK_SIZE = 1024;
    char out[CHUNK_SIZE];

    /* allocate inflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = static_cast<uint>(src.size());
    strm.next_in = reinterpret_cast<uchar *>(src.data());

    const int windowBits = 15;
    const int ENABLE_ZLIB_GZIP = 32;

    int ret = inflateInit2(&strm, windowBits | ENABLE_ZLIB_GZIP); // gzip decoding
    if (ret != Z_OK)
        return false;

    // run inflate()
    do {
        strm.avail_out = CHUNK_SIZE;
        strm.next_out = reinterpret_cast<uchar *>(out);

        ret = inflate(&strm, Z_NO_FLUSH);
        Q_ASSERT(ret != Z_STREAM_ERROR); // state not clobbered

        switch (ret) {
        case Z_NEED_DICT:
        case Z_DATA_ERROR:
        case Z_MEM_ERROR:
            inflateEnd(&strm);
            return false;
        }

        dest.append(out, CHUNK_SIZE - strm.avail_out);
    }
    while (!strm.avail_out);

    // clean up and return
    inflateEnd(&strm);
    return true;
}
