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
}

QVariant EventManager::getEventList() const {
	QVariantList list;
	foreach(QVariantMap event, event_list.values()) {
		list << QVariant(event);
	}
	return QVariant(list);
}

void EventManager::addedTorrent(QTorrentHandle& h)
{
	QVariantMap event;
	QString hash = h.hash();
	event["hash"] = QVariant(hash);
	event["name"] = QVariant(h.name());
	event["seed"] = QVariant(h.is_seed());
	event["size"] = QVariant((qlonglong)h.actual_size());
	if(!h.is_seed()) {
		event["progress"] = QVariant(h.progress());
		event["dlspeed"] = QVariant(h.download_payload_rate());
	}
	event["upspeed"] = QVariant(h.upload_payload_rate());
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
	event_list[hash] = event;
}

void EventManager::deletedTorrent(QString hash)
{
	event_list.remove(hash);
}

void EventManager::modifiedTorrent(QTorrentHandle h)
{
	QString hash = h.hash();
	QVariantMap event;
	
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
	event["name"] = QVariant(h.name());
	event["size"] = QVariant((qlonglong)h.actual_size());
	if(!h.is_seed()) {
		event["progress"] = QVariant(h.progress());
		event["dlspeed"] = QVariant(h.download_payload_rate());
	}
	event["upspeed"] = QVariant(h.upload_payload_rate());
	event["seed"] = QVariant(h.is_seed());
	event["hash"] = QVariant(hash);
	event_list[hash] = event;
}
