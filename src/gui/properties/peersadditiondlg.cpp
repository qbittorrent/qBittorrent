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

#include <QMessageBox>
#include <QHostAddress>

#include "peersadditiondlg.h"

PeersAdditionDlg::PeersAdditionDlg(QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(validateInput()));

#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
    label_format->hide();
    peers_txt->setPlaceholderText("Format: IPv4:port / [IPv6]:port");
#endif
}

QList<BitTorrent::PeerAddress> PeersAdditionDlg::askForPeers()
{
    PeersAdditionDlg dlg;
    dlg.exec();
    return dlg.m_peersList;
}

void PeersAdditionDlg::validateInput()
{
    if (peers_txt->toPlainText().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("No peer entered"),
                    tr("Please type at least one peer."),
                    QMessageBox::Ok);
        return;
    }
    foreach (const QString &peer, peers_txt->toPlainText().trimmed().split("\n")) {
        BitTorrent::PeerAddress addr = parsePeer(peer);
        if (!addr.ip.isNull()) {
            m_peersList.append(addr);
        }
        else {
            QMessageBox::warning(this, tr("Invalid peer"),
                    tr("The peer '%1' is invalid.").arg(peer),
                    QMessageBox::Ok);
            m_peersList.clear();
            return;
        }
    }
    accept();
}

BitTorrent::PeerAddress PeersAdditionDlg::parsePeer(QString peer)
{
    BitTorrent::PeerAddress addr;
    QStringList ipPort;

    if (peer[0] == '[' && peer.indexOf("]:") != -1) // IPv6
        ipPort = peer.remove(QChar('[')).split("]:");
    else if (peer.indexOf(":") != -1) // IPv4
        ipPort = peer.split(":");
    else
        return addr;

    QHostAddress ip(ipPort[0]);
    if (ip.isNull())
        return addr;

    bool ok;
    int port = ipPort[1].toInt(&ok);
    if (!ok || port < 1 || port > 65535)
        return addr;

    addr.ip = ip;
    addr.port = port;
    return addr;
}
