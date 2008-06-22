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


#ifndef HTTPREQUESTPARSER_H
#define HTTPREQUESTPARSER_H

#include<QHttpRequestHeader>

class HttpRequestParser : public QHttpRequestHeader
{
	private:
		bool headerDone;
		bool messageDone;
		bool error;
		QString data;
		QString path;
		QMap<QString, QString> postMap;
		QMap<QString, QString> getMap;

	public:
		HttpRequestParser();
		~HttpRequestParser();
		bool isParsable() const;
		bool isError() const;
		QString url() const;
		QString message() const;
		QString get(const QString key) const;
		QString post(const QString key) const;
		void write(QString str);
};

#endif
