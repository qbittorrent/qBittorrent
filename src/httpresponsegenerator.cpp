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


#include "httpresponsegenerator.h"

void HttpResponseGenerator::setMessage(const QByteArray message)
{
	HttpResponseGenerator::message = message;
	setContentLength(message.size());
}

void HttpResponseGenerator::setMessage(const QString message)
{
	setMessage(message.QString::toUtf8());
}

void HttpResponseGenerator::stripMessage()
{
	message.clear();
}

QByteArray HttpResponseGenerator::toByteArray() const
{
	return QHttpResponseHeader::toString().toUtf8() + message;
}

void HttpResponseGenerator::setContentTypeByExt(const QString ext)
{
	if(ext == "css")
	{
		setContentType("text/css");
		return;
	}
	if(ext == "gif")
	{
		setContentType("image/gif");
		return;
	}
	if(ext == "htm" || ext == "html")
	{
		setContentType("text/html");
		return;
	}
	if(ext == "js")
	{
		setContentType("text/javascript");
		return;
	}
	if(ext == "png")
	{
		setContentType("image/x-png");
		return;
	}
}
