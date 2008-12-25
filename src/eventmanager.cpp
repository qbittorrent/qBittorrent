/*
 *   Copyright (C) 2007 by Ishan Arora & Christophe Dumez
 *   <ishan@qbittorrent.org>, <chris@qbittorrent.org>
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
#include <QDebug>

EventManager::EventManager(QObject *parent, bittorrent *BTSession)
	: QObject(parent), BTSession(BTSession)
{
}

QList<QVariantMap> EventManager::getEventList() const {
        return event_list.values();
}

void EventManager::addedTorrent(QTorrentHandle& h)
{
    modifiedTorrent(h);
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
        event["state"] = QVariant("paused");
    } else {
        if(BTSession->isQueueingEnabled() && h.is_queued()) {
            event["state"] = QVariant("queued");
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
    }
	event["name"] = QVariant(h.name());
	event["size"] = QVariant((qlonglong)h.actual_size());
        if(h.progress() < 1.) {
		event["progress"] = QVariant(h.progress());
		event["dlspeed"] = QVariant(h.download_payload_rate());
	}
	event["upspeed"] = QVariant(h.upload_payload_rate());
        event["seed"] = QVariant(h.progress() == 1.);
	event["hash"] = QVariant(hash);
	event_list[hash] = event;
}
