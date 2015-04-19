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

#include "core/bittorrent/torrenthandle.h"
#include "trackerlogin.h"

trackerLogin::trackerLogin(QWidget *parent, BitTorrent::TorrentHandle *const torrent)
  : QDialog(parent)
  , m_torrent(torrent)
{
  setupUi(this);
  setAttribute(Qt::WA_DeleteOnClose);
  login_logo->setPixmap(QPixmap(QString::fromUtf8(":/icons/oxygen/encrypted.png")));
  tracker_url->setText(torrent->currentTracker());
  connect(this, SIGNAL(trackerLoginCancelled(QPair<BitTorrent::TorrentHandle *const, QString>)), parent, SLOT(addUnauthenticatedTracker(QPair<BitTorrent::TorrentHandle *const, QString>)));
  show();
}

trackerLogin::~trackerLogin() {}

void trackerLogin::on_loginButton_clicked() {
  // login
  m_torrent->setTrackerLogin(lineUsername->text(), linePasswd->text());
  close();
}

void trackerLogin::on_cancelButton_clicked() {
  // Emit a signal to GUI to stop asking for authentication
  emit trackerLoginCancelled(QPair<BitTorrent::TorrentHandle *const, QString>(m_torrent, m_torrent->currentTracker()));
  close();
}
