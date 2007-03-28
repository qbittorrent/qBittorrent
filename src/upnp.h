/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Christophe Dumez
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
 * Contact : chris@qbittorrent.org
 */
#ifndef __QBT_UPNP__
#define __QBT_UPNP__

#include <QThread>
#include <QHash>

#include <upnp/upnp.h>

class UPnPDevice{
  private:
    struct Upnp_Discovery* device;
    IXML_Document *doc;

  public:
    UPnPDevice(struct Upnp_Discovery* device, IXML_Document* doc): device(device), doc(doc){}
    IXML_Document* getDoc(){ return doc; }
    struct Upnp_Discovery* getDevice(){ return device;}
};

class UPnPHandler : public QThread {
  Q_OBJECT

  private:
    UpnpClient_Handle UPnPClientHandle;
    QHash<QString, UPnPDevice*> UPnPDevices;

  public:
    UPnPHandler(){}
    ~UPnPHandler(){}

  public slots:
    void addUPnPDevice(struct Upnp_Discovery* device, IXML_Document *doc){
      UPnPDevices.insert(QString(device->DeviceId), new UPnPDevice(device, doc));
    }

    void removeUPnPDevice(QString device_id){
      UPnPDevices.remove(device_id);
    }

  private:
    void run();
};

#endif
