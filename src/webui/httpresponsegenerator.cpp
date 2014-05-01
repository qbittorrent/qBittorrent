/*
 * Bittorrent Client using Qt4 and libtorrent.
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


#include "httpresponsegenerator.h"
#include <zlib.h>

void HttpResponseGenerator::setMessage(const QByteArray& message)
{
  m_message = message;
}

void HttpResponseGenerator::setMessage(const QString& message)
{
  setMessage(message.toUtf8());
}

void HttpResponseGenerator::setContentTypeByExt(const QString& ext) {
  if (ext == "css") {
		setContentType("text/css");
		return;
	}
  if (ext == "gif") {
		setContentType("image/gif");
		return;
	}
	if (ext == "htm" || ext == "html")	{
		setContentType("text/html");
		return;
	}
	if (ext == "js")	{
		setContentType("text/javascript");
		return;
	}
  if (ext == "png") {
		setContentType("image/png");
		return;
	}
}

bool HttpResponseGenerator::gCompress(QByteArray &dest_buffer) {
  static const int BUFSIZE = 128 * 1024;
  char tmp_buf[BUFSIZE];
  int ret;

  z_stream strm;
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  strm.next_in = reinterpret_cast<unsigned char*>(m_message.data());
  strm.avail_in = m_message.length();
  strm.next_out = reinterpret_cast<unsigned char*>(tmp_buf);
  strm.avail_out = BUFSIZE;

  //windowBits = 15+16 to enable gzip
  //From the zlib manual: windowBits can also be greater than 15 for optional gzip encoding. Add 16 to windowBits
  //to write a simple gzip header and trailer around the compressed data instead of a zlib wrapper.
  ret = deflateInit2(&strm, Z_BEST_COMPRESSION, Z_DEFLATED, 15+16, 8, Z_DEFAULT_STRATEGY);

  if (ret != Z_OK)
    return false;

  while (strm.avail_in != 0)
   {
    ret = deflate(&strm, Z_NO_FLUSH);
    if (ret != Z_OK)
      return false;
    if (strm.avail_out == 0)
    {
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

QByteArray HttpResponseGenerator::toByteArray() {
  // A gzip seems to have 23 bytes overhead.
  // Also "content-encoding: gzip\r\n" is 26 bytes long
  // So we only benefit from gzip if the message is bigger than 23+26 = 49
  // If the message is smaller than 49 bytes we actually send MORE data if we gzip
  if (m_gzip && m_message.size() > 49) {
    QByteArray dest_buf;
    if (gCompress(dest_buf)) {
      setValue("content-encoding", "gzip");
#if QT_VERSION < 0x040800
      m_message = dest_buf;
#else
      m_message.swap(dest_buf);
#endif
    }
  }

  setContentLength(m_message.size());
  return HttpResponseHeader::toString().toUtf8() + m_message;
}
