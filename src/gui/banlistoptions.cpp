/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2016  Alexandr Milovantsev <dzmat@yandex.ru>
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

#include "banlistoptions.h"
#include "ui_banlistoptions.h"

#include <QMessageBox>
#include <QHostAddress>

#include "base/bittorrent/session.h"
#include "base/utils/net.h"

BanListOptions::BanListOptions(QWidget *parent)
    : QDialog(parent)
    , m_ui(new Ui::BanListOptions)
    , m_modified(false)
{
    m_ui->setupUi(this);
    m_ui->bannedIPList->addItems(BitTorrent::Session::instance()->bannedIPs());
    m_ui->buttonBanIP->setEnabled(false);
}

BanListOptions::~BanListOptions()
{
    delete m_ui;
}

void BanListOptions::on_buttonBox_accepted()
{
    if (m_modified) {
        // save to session
        QStringList IPList;
        for (int i = 0; i < m_ui->bannedIPList->count(); ++i)
            IPList << m_ui->bannedIPList->item(i)->text();
        BitTorrent::Session::instance()->setBannedIPs(IPList);
    }
    QDialog::accept();
}

void BanListOptions::on_buttonBanIP_clicked()
{
    QString ip=m_ui->txtIP->text();
    if (!Utils::Net::isValidIP(ip)) {
        QMessageBox::warning(this, tr("Warning"), tr("The entered IP address is invalid."));
        return;
    }
    // the same IPv6 addresses could be written in different forms;
    // QHostAddress::toString() result format follows RFC5952;
    // thus we avoid duplicate entries pointing to the same address
    ip = QHostAddress(ip).toString();
    QList<QListWidgetItem *> findres = m_ui->bannedIPList->findItems(ip, Qt::MatchExactly);
    if (!findres.isEmpty()) {
        QMessageBox::warning(this, tr("Warning"), tr("The entered IP is already banned."));
        return;
    }
    m_ui->bannedIPList->addItem(ip);
    m_ui->txtIP->clear();
    m_modified = true;
}

void BanListOptions::on_buttonDeleteIP_clicked()
{
    QList<QListWidgetItem *> selection = m_ui->bannedIPList->selectedItems();
    for (auto &i : selection) {
        m_ui->bannedIPList->removeItemWidget(i);
        delete i;
    }
    m_modified = true;
}

void BanListOptions::on_txtIP_textChanged(const QString &ip)
{
    m_ui->buttonBanIP->setEnabled(Utils::Net::isValidIP(ip));
}
