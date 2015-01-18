/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2010  Christophe Dumez
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

#include <QEvent>
#include <QFileOpenEvent>
#include <QUrl>
#include "qmacapplication.h"

QMacApplication::QMacApplication(QString appid, int &argc, char** argv) :
  QtSingleApplication(appid, argc, argv),
  m_readyToProcessEvents(false)
{
  qDebug("Constructing a QMacApplication to receive file open events");
}

void QMacApplication::setReadyToProcessEvents()
{
  m_readyToProcessEvents = true;
  if (!m_torrentsQueue.isEmpty()) {
    emit newFileOpenMacEvent(m_torrentsQueue.join("|"));
    m_torrentsQueue.clear();
  }
}

bool QMacApplication::event(QEvent * ev) {
  switch (ev->type()) {
  case QEvent::FileOpen:
    {
      QString path = static_cast<QFileOpenEvent *>(ev)->file();
      if (path.isEmpty()) {
        // Get the url instead
        path = static_cast<QFileOpenEvent *>(ev)->url().toString();
      }
      qDebug("Received a mac file open event: %s", qPrintable(path));
      if (m_readyToProcessEvents)
        emit newFileOpenMacEvent(path);
      else
        m_torrentsQueue.append(path);
      return true;
    }
  default:
    return QtSingleApplication::event(ev);
  }
}

