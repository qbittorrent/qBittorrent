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
#include <zlib.h>

#include "gzip.h"

bool Utils::Gzip::compress(QByteArray src, QByteArray &dest)
{
    static const int BUFSIZE = 128 * 1024;
    char tmpBuf[BUFSIZE];
    int ret;

    dest.clear();

    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.next_in = reinterpret_cast<uchar *>(src.data());
    strm.avail_in = src.length();
    strm.next_out = reinterpret_cast<uchar *>(tmpBuf);
    strm.avail_out = BUFSIZE;

    // windowBits = 15 + 16 to enable gzip
    // From the zlib manual: windowBits can also be greater than 15 for optional gzip encoding. Add 16 to windowBits
    // to write a simple gzip header and trailer around the compressed data instead of a zlib wrapper.
    ret = deflateInit2(&strm, Z_BEST_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY);

    if (ret != Z_OK)
        return false;

    while (strm.avail_in != 0) {
        ret = deflate(&strm, Z_NO_FLUSH);
        if (ret != Z_OK)
            return false;

        if (strm.avail_out == 0) {
            dest.append(tmpBuf, BUFSIZE);
            strm.next_out = reinterpret_cast<uchar *>(tmpBuf);
            strm.avail_out = BUFSIZE;
        }
    }

    int deflateRes = Z_OK;
    while (deflateRes == Z_OK) {
        if (strm.avail_out == 0) {
            dest.append(tmpBuf, BUFSIZE);
            strm.next_out = reinterpret_cast<uchar *>(tmpBuf);
            strm.avail_out = BUFSIZE;
        }

        deflateRes = deflate(&strm, Z_FINISH);
    }

    if (deflateRes != Z_STREAM_END)
        return false;

    dest.append(tmpBuf, BUFSIZE - strm.avail_out);
    deflateEnd(&strm);

    return true;
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
