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


#include "eventmanager.h"
#include "bittorrent.h"
#include "json.h"
#include <QDebug>

EventManager::EventManager(QObject *parent, bittorrent *BTSession)
	: QObject(parent), BTSession(BTSession)
{
	revision = 0;
}

void EventManager::update(QVariantMap event)
{
	++revision;
	events << QPair<ulong, QVariantMap>(revision, event);
	emit updated();
	//qDebug("Added the following event");
	//qDebug() << event;
/*	QLinkedList<QPair<ulong, QVariantMap> >::iterator i;
	for (i = events.begin(); i != events.end(); i++)
		qDebug() << *i;*/
}

QVariant EventManager::querySince(ulong r) const
{
	QVariantList list;
	QLinkedListIterator<QPair<ulong, QVariantMap> > i(events);
	i.toBack();
	while (i.hasPrevious())
	{
		QPair<ulong, QVariantMap> pair = i.previous();
		if (pair.first <= r)
			break;
		list.prepend(QVariant(pair.second));
	}
	QVariantMap map;
	map["events"] = QVariant(list);
	map["revision"] = QVariant((qulonglong) revision);
	return QVariant(map);
}

bool EventManager::isUpdated(ulong r) const
{
	return (r < revision);
}

void EventManager::addedTorrent(QTorrentHandle& h)
{
	QVariantMap event;
	QString hash = h.hash();
	event["type"] = QVariant("add");
	event["hash"] = QVariant(hash);
	event["name"] = QVariant(h.name());
	event["seed"] = QVariant(h.is_seed());
	if(h.is_paused()) {
		if(BTSession->isQueueingEnabled() && (BTSession->isDownloadQueued(hash) || BTSession->isUploadQueued(hash)))
			event["state"] = QVariant("queued");
		else
			event["state"] = QVariant("paused");
	} else {
		switch(h.state())
		{
			case torrent_status::finished:
			case torrent_status::seeding:
				event["state"] = QVariant("seeding");
				break;
			case torrent_status::checking_files:
			case torrent_status::queued_for_checking:
				event["state"] = QVariant("checking");
				break;
			case torrent_status::connecting_to_tracker:
				if(h.download_payload_rate() > 0)
					event["state"] = QVariant("downloading");
				else
					event["state"] = QVariant("connecting");
				break;
			case torrent_status::downloading:
			case torrent_status::downloading_metadata:
				if(h.download_payload_rate() > 0)
					event["state"] = QVariant("downloading");
				else
					event["state"] = QVariant("stalled");
				break;
			default:
				qDebug("No status, should not happen!!! status is %d", h.state());
				event["state"] = QVariant();
		}
	}
	update(event);
}

void EventManager::torrentSwitchedtoUnfinished(QString hash) {
	QVariantMap event;
	QTorrentHandle h = BTSession->getTorrentHandle(hash);
	event["type"] = QVariant("unfinish");
	event["hash"] = QVariant(h.hash());
	event["name"] = QVariant(h.name());
	if(h.is_paused()) {
		if(BTSession->isQueueingEnabled() && (BTSession->isDownloadQueued(hash) || BTSession->isUploadQueued(hash)))
			event["state"] = QVariant("queued");
		else
			event["state"] = QVariant("paused");
	} else {
		switch(h.state())
		{
			case torrent_status::finished:
			case torrent_status::seeding:
				event["state"] = QVariant("seeding");
				break;
			case torrent_status::checking_files:
			case torrent_status::queued_for_checking:
				event["state"] = QVariant("checking");
				break;
			case torrent_status::connecting_to_tracker:
				if(h.download_payload_rate() > 0)
					event["state"] = QVariant("downloading");
				else
					event["state"] = QVariant("connecting");
				break;
			case torrent_status::downloading:
			case torrent_status::downloading_metadata:
				if(h.download_payload_rate() > 0)
					event["state"] = QVariant("downloading");
				else
					event["state"] = QVariant("stalled");
				break;
			default:
				qDebug("No status, should not happen!!! status is %d", h.state());
				event["state"] = QVariant();
		}
	}
	event["size"] = QVariant((qlonglong)h.actual_size());
	event["progress"] = QVariant(h.progress());
	event["dlspeed"] = QVariant(h.download_payload_rate());
	event["upspeed"] = QVariant(h.upload_payload_rate());
	update(event);
}

void EventManager::torrentSwitchedtoFinished(QString hash) {
	QVariantMap event;
	QTorrentHandle h = BTSession->getTorrentHandle(hash);
	event["type"] = QVariant("finish");
	event["hash"] = QVariant(h.hash());
	event["name"] = QVariant(h.name());
	if(h.is_paused()) {
		if(BTSession->isQueueingEnabled() && (BTSession->isDownloadQueued(hash) || BTSession->isUploadQueued(hash)))
			event["state"] = QVariant("queued");
		else
			event["state"] = QVariant("paused");
	} else {
		switch(h.state())
		{
			case torrent_status::finished:
			case torrent_status::seeding:
				event["state"] = QVariant("seeding");
				break;
			case torrent_status::checking_files:
			case torrent_status::queued_for_checking:
				event["state"] = QVariant("checking");
				break;
			case torrent_status::connecting_to_tracker:
				if(h.download_payload_rate() > 0)
					event["state"] = QVariant("downloading");
				else
					event["state"] = QVariant("connecting");
				break;
			case torrent_status::downloading:
			case torrent_status::downloading_metadata:
				if(h.download_payload_rate() > 0)
					event["state"] = QVariant("downloading");
				else
					event["state"] = QVariant("stalled");
				break;
			default:
				qDebug("No status, should not happen!!! status is %d", h.state());
				event["state"] = QVariant();
		}
	}
	event["size"] = QVariant((qlonglong)h.actual_size());
	event["upspeed"] = QVariant(h.upload_payload_rate());
	update(event);
}

void EventManager::deletedTorrent(QString hash)
{
	QVariantMap event;
	QTorrentHandle h = BTSession->getTorrentHandle(hash);
	event["type"] = QVariant("delete");
	event["hash"] = QVariant(hash);
	event["seed"] = QVariant(h.is_seed());
	QLinkedList<QPair<ulong, QVariantMap> >::iterator i = events.end();
	bool loop = true;
	while (loop && i != events.begin()) {
		--i;
		QVariantMap oldevent = i->second;
		if(oldevent["hash"] == QVariant(hash))
		{
			if(oldevent["type"] == QVariant("add"))
				loop = false;
			i = events.erase(i);
		}
	}
	update(event);
}

void EventManager::modifiedTorrent(QTorrentHandle h)
{
	QString hash = h.hash();
	QVariantMap event;
	QVariant v;
	
	if(h.is_paused()) {
		if(BTSession->isQueueingEnabled() && (BTSession->isDownloadQueued(hash) || BTSession->isUploadQueued(hash)))
			v = QVariant("queued");
		else
			v = QVariant("paused");
	} else {
		switch(h.state())
		{
			case torrent_status::finished:
			case torrent_status::seeding:
				v = QVariant("seeding");
				break;
			case torrent_status::checking_files:
			case torrent_status::queued_for_checking:
				v = QVariant("checking");
				break;
			case torrent_status::connecting_to_tracker:
				if(h.download_payload_rate() > 0)
					v = QVariant("downloading");
				else
					v = QVariant("connecting");
				break;
			case torrent_status::downloading:
			case torrent_status::downloading_metadata:
				if(h.download_payload_rate() > 0)
					v = QVariant("downloading");
				else
					v = QVariant("stalled");
				break;
			default:
			  qDebug("No status, should not happen!!! status is %d", h.state());
				v = QVariant();
		}
	}
	if(modify(hash, "state", v))
		event["state"] = v;
	
	v = QVariant((qlonglong)h.actual_size());
	if(modify(hash, "size", v))
		event["size"] = v;
	if(!h.is_seed()) {
		v = QVariant(h.progress());
		if(modify(hash, "progress", v))
			event["progress"] = v;
		
		v = QVariant(h.download_payload_rate());
		if(modify(hash, "dlspeed", v))
			event["dlspeed"] = v;
	}
	v = QVariant(h.upload_payload_rate());
	if(modify(hash, "upspeed", v)) {
		event["upspeed"] = v;
		if(h.is_seed())
		  qDebug("upspeed changed for seed");
	} else {
		if(h.is_seed())
		  qDebug("upspeed did not change for seed");
	}
	v = QVariant(h.is_seed());
	event["seed"] = v;
	
	if(event.size() > 0)
	{
		event["type"] = QVariant("modify");
		event["hash"] = QVariant(hash);
		update(event);
	}
}

bool EventManager::modify(QString hash, QString key, QVariant value)
{
	QLinkedList<QPair<ulong, QVariantMap> >::iterator i = events.end();
	while (i != events.begin()) {
		--i;
		QVariantMap event = i->second;
		if(event["hash"] == QVariant(hash))
		{
			if(event["type"] == QVariant("add"))
				return true;
			if(event.contains(key))
			{
				if(event[key] == value)
					return false;
				else
				{
					if(event.size() <= 3)
						i = events.erase(i);
					else
						i->second.remove(key);
					return true;
				}
			}
		}
	}
	return true;
}
