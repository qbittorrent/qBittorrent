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


#ifndef EVENTMANAGER_H
#define EVENTMANAGER_H

#include "qtorrenthandle.h"
#include <QLinkedList>
#include <QPair>
#include <QVariant>

struct bittorrent;

class EventManager : public QObject
{
	Q_OBJECT
	private:
		ulong revision;
		QLinkedList<QPair <ulong, QVariantMap> > events;
		bool modify(QString hash, QString key, QVariant value);
		bittorrent* BTSession;

	protected:
		void update(QVariantMap event);

	public:
		EventManager(QObject *parent, bittorrent* BTSession);
		QVariant querySince(ulong r) const;
		bool isUpdated(ulong r) const;

	signals:
		void updated();

	public slots:
		void addedTorrent(QString path, QTorrentHandle& h);
		void deletedTorrent(QString hash);
		void modifiedTorrent(QTorrentHandle h);
};

#endif
