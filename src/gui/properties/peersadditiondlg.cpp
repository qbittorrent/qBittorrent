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

#include "peersadditiondlg.h"
#include "ui_peersadditiondlg.h"


PeersAdditionDlg::PeersAdditionDlg(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::PeersAdditionDlg)
{
    ui->setupUi(this);
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(validateInput()));

#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
    ui->label_format->hide();
    ui->peers_list->setPlaceholderText("Format: IPv4:port / [IPv6]:port");
#endif
}

PeersAdditionDlg::~PeersAdditionDlg()
{
    delete ui;
}

QMap<QString, int> PeersAdditionDlg::askForPeersEndpoints()
{
    PeersAdditionDlg dlg;
    dlg.exec();
    return dlg.endpoints;
}

void PeersAdditionDlg::validateInput()
{
    if (ui->peers_list->toPlainText().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("No peer entered"),
                    tr("Please type at least one peer."),
                    QMessageBox::Ok);
    return;
    }

    QStringList peers = ui->peers_list->toPlainText().trimmed().split("\n");
    bool ok;
    boost::system::error_code ec;
    bool all_valid = true;
    QString invalid_peer;
    foreach (const QString &peer, peers) {
        if (peer.lastIndexOf(":") == -1) {
            qDebug("Unable to parse the provided peer: %s", qPrintable(peer));
            all_valid = false;
	    invalid_peer = peer;
            break;
        }
        QString str_ip_port = peer;
        str_ip_port.remove(QChar('[')).remove(QChar(']'));
        QString str_ip = str_ip_port.mid(0, str_ip_port.lastIndexOf(":"));
        QString str_port = str_ip_port.mid(str_ip_port.lastIndexOf(":")+1);
        QHostAddress ip(str_ip);
        if (ip.isNull()) {
            qDebug("Unable to parse the provided IP: %s", qPrintable(str_ip));
            all_valid = false;
	    invalid_peer = peer;
            break;
        }
        libtorrent::address::from_string(qPrintable(ip.toString()), ec);
        if (ec) {
            qDebug("Unable to parse the provided IP: %s", qPrintable(str_ip));
            all_valid = false;
	    invalid_peer = peer;
            break;
        }
        int port = str_port.toInt(&ok);
        if (!ok || port < 1 || port > 65535) {
            qDebug("Unable to parse the provided port: %s", qPrintable(str_port));
            all_valid = false;
	    invalid_peer = peer;
            break;
        }
        qDebug("Valid peer: %s", qPrintable(peer));
        endpoints.insert(ip.toString(), port);
    }

    if (all_valid == false) {
        endpoints.clear();
        QMessageBox::warning(this, tr("Invalid peer"),
                        tr("The peer %1 is invalid.").arg(invalid_peer),
                        QMessageBox::Ok);
    } else {
        accept();
    }
}
