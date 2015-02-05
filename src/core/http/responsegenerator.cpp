/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2014  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Ishan Arora and Christophe Dumez
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
 *
 * Contact : chris@qbittorrent.org
 */

#include <zlib.h>
#include "responsegenerator.h"

bool gCompress(QByteArray data, QByteArray& dest_buffer);

using namespace Http;

QByteArray ResponseGenerator::generate(Response response)
{
    if (response.headers[HEADER_CONTENT_ENCODING] == "gzip") {
        // A gzip seems to have 23 bytes overhead.
        // Also "Content-Encoding: gzip\r\n" is 26 bytes long
        // So we only benefit from gzip if the message is bigger than 23+26 = 49
        // If the message is smaller than 49 bytes we actually send MORE data if we gzip
        QByteArray dest_buf;
        if ((response.content.size() > 49) && (gCompress(response.content, dest_buf)))
            response.content = dest_buf;
        else
            response.headers.remove(HEADER_CONTENT_ENCODING);
    }

    if (response.content.length() > 0)
        response.headers[HEADER_CONTENT_LENGTH] = QString::number(response.content.length());

    QString ret(QLatin1String("HTTP/1.1 %1 %2\r\n%3\r\n"));

    QString header;
    foreach (const QString& key, response.headers.keys())
        header += QString("%1: %2\r\n").arg(key).arg(response.headers[key]);

    ret = ret.arg(response.status.code).arg(response.status.text).arg(header);

    //  qDebug() << Q_FUNC_INFO;
    //  qDebug() << "HTTP Response header:";
    //  qDebug() << ret;

    return ret.toUtf8() + response.content;
}

bool gCompress(QByteArray data, QByteArray& dest_buffer)
{
    static const int BUFSIZE = 128 * 1024;
    char tmp_buf[BUFSIZE];
    int ret;

    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.next_in = reinterpret_cast<unsigned char*>(data.data());
    strm.avail_in = data.length();
    strm.next_out = reinterpret_cast<unsigned char*>(tmp_buf);
    strm.avail_out = BUFSIZE;

    //windowBits = 15+16 to enable gzip
    //From the zlib manual: windowBits can also be greater than 15 for optional gzip encoding. Add 16 to windowBits
    //to write a simple gzip header and trailer around the compressed data instead of a zlib wrapper.
    ret = deflateInit2(&strm, Z_BEST_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY);

    if (ret != Z_OK)
        return false;

    while (strm.avail_in != 0) {
        ret = deflate(&strm, Z_NO_FLUSH);
        if (ret != Z_OK)
            return false;

        if (strm.avail_out == 0) {
            dest_buffer.append(tmp_buf, BUFSIZE);
            strm.next_out = reinterpret_cast<unsigned char*>(tmp_buf);
            strm.avail_out = BUFSIZE;
        }
    }

    int deflate_res = Z_OK;
    while (deflate_res == Z_OK) {
        if (strm.avail_out == 0) {
            dest_buffer.append(tmp_buf, BUFSIZE);
            strm.next_out = reinterpret_cast<unsigned char*>(tmp_buf);
            strm.avail_out = BUFSIZE;
        }

        deflate_res = deflate(&strm, Z_FINISH);
    }

    if (deflate_res != Z_STREAM_END)
        return false;

    dest_buffer.append(tmp_buf, BUFSIZE - strm.avail_out);
    deflateEnd(&strm);

    return true;
}
