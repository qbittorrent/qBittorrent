/*
 *   Copyright (C) 2007 by Ishan Arora
 *   ishanarora@gmail.com
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */


#include "httprequestparser.h"
#include <QUrl>
#include <QDebug>

HttpRequestParser::HttpRequestParser()
{
	headerDone = false;
	messageDone = false;
	error = false;
}

HttpRequestParser::~HttpRequestParser()
{
}

bool HttpRequestParser::isParsable() const
{
	return !error && headerDone && isValid() && (messageDone || !hasContentLength() || contentLength() == 0);
}

bool HttpRequestParser::isError() const
{
	return error;
}

QString HttpRequestParser::url() const
{
	return path;
}

QByteArray HttpRequestParser::message() const
{
	if(isParsable())
		return data;
	return QByteArray();
}

QString HttpRequestParser::get(const QString key) const
{
	return getMap[key];
}

QString HttpRequestParser::post(const QString key) const
{
	return postMap[key];
}

QByteArray HttpRequestParser::torrent() const
{
	return torrent_content;
}

void HttpRequestParser::write(QByteArray str)
{
	while (!headerDone && str.size()>0)
	{
		int index = str.indexOf('\n') + 1;
		if(index == 0)
		{
			data += str;
			str.clear();
		}
		else
		{
			data += str.left(index);
			str.remove(0, index);
			if(data.right(4) == "\r\n\r\n")
			{
				QHttpRequestHeader::operator=(QHttpRequestHeader(data));
				headerDone = true;
				data.clear();
				QUrl url = QUrl::fromEncoded(QHttpRequestHeader::path().toAscii());
				path = url.path();
				qDebug() << path;
				QListIterator<QPair<QString, QString> > i(url.queryItems());
				while (i.hasNext())
				{
					QPair<QString, QString> pair = i.next();
					getMap[pair.first] = pair.second;
					qDebug() << pair.first << "=" << get(pair.first);
				}
			}
		}
	}
	if(!messageDone && str.size()>0)
	{
		if(hasContentLength())
		{
			data += str;
			if(data.size() >= (int) contentLength())
			{
				data.resize(contentLength());
				messageDone = true;
				//parse POST data
				if(contentType() == "application/x-www-form-urlencoded")
				{
					QUrl url;
					url.setEncodedQuery(data);
					QListIterator<QPair<QString, QString> > i(url.queryItems());
					while (i.hasNext())
					{
						QPair<QString, QString> pair = i.next();
						postMap[pair.first] = pair.second;
						qDebug() << pair.first << "=" << post(pair.first);
					}
				}
				if(contentType() == "multipart/form-data")
				{
					//qDebug() << data.right(data.size()-data.indexOf("\r\n\r\n")-QByteArray("\r\n\r\n").size());
					torrent_content = data.right(data.size()-data.indexOf("\r\n\r\n")-QByteArray("\r\n\r\n").size());
				}
			}
		}
		else
			error = true;
	}
	qDebug() << "isError: " << isError();
}
