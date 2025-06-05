/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
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
 */

#include "peersadditiondialog.h"

#include <QHostAddress>
#include <QMessageBox>

#include "base/bittorrent/peeraddress.h"
#include "base/global.h"
#include "ui_peersadditiondialog.h"

PeersAdditionDialog::PeersAdditionDialog(QWidget *parent)
    : QDialog(parent)
    , m_ui(new Ui::PeersAdditionDialog())
{
    m_ui->setupUi(this);

    connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &PeersAdditionDialog::validateInput);
    connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

PeersAdditionDialog::~PeersAdditionDialog()
{
    delete m_ui;
}

QList<BitTorrent::PeerAddress> PeersAdditionDialog::askForPeers(QWidget *parent)
{
    PeersAdditionDialog dlg(parent);
    dlg.exec();
    return dlg.m_peersList;
}

void PeersAdditionDialog::validateInput()
{
    if (m_ui->textEditPeers->toPlainText().trimmed().isEmpty())
    {
        QMessageBox::warning(this, tr("No peer entered"),
                    tr("Please type at least one peer."),
                    QMessageBox::Ok);
        return;
    }
    for (const QString &peer : asConst(m_ui->textEditPeers->toPlainText().trimmed().split(u'\n')))
    {
        const BitTorrent::PeerAddress addr = BitTorrent::PeerAddress::parse(peer);
        if (!addr.ip.isNull())
        {
            m_peersList.append(addr);
        }
        else
        {
            QMessageBox::warning(this, tr("Invalid peer"),
                    tr("The peer '%1' is invalid.").arg(peer),
                    QMessageBox::Ok);
            m_peersList.clear();
            return;
        }
    }
    accept();
}
