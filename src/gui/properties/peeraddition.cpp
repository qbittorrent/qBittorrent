/*
 * Bittorrent Client using Qt and libtorrent.
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

#include <QRegExp>
#include <QMessageBox>

#include "core/bittorrent/peerinfo.h"
#include "peeraddition.h"

PeerAdditionDlg::PeerAdditionDlg(QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(validateInput()));
}

QHostAddress PeerAdditionDlg::getAddress() const
{
    return QHostAddress(lineIP->text());
}


ushort PeerAdditionDlg::getPort() const
{
    return spinPort->value();
}


BitTorrent::PeerAddress PeerAdditionDlg::askForPeerAddress()
{
    BitTorrent::PeerAddress addr;

    PeerAdditionDlg dlg;
    if (dlg.exec() == QDialog::Accepted) {
        addr.ip = dlg.getAddress();
        if (addr.ip.isNull())
            qDebug("Unable to parse the provided IP.");
        else
            qDebug("Provided IP is correct");
        addr.port = dlg.getPort();
    }

    return addr;
}


void PeerAdditionDlg::validateInput()
{
    if (getAddress().isNull())
        QMessageBox::warning(this, tr("Invalid IP"), tr("The IP you provided is invalid."), QMessageBox::Ok);
    else
        accept();
}
