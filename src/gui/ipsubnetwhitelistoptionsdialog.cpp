/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017  Thomas Piccirello <thomas.piccirello@gmail.com>
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

#include "ipsubnetwhitelistoptionsdialog.h"

#include <QMessageBox>
#include <QSortFilterProxyModel>
#include <QStringListModel>

#include "base/global.h"
#include "base/preferences.h"
#include "base/utils/net.h"
#include "ui_ipsubnetwhitelistoptionsdialog.h"
#include "utils.h"

#define SETTINGS_KEY(name) u"IPSubnetWhitelistOptionsDialog/" name

IPSubnetWhitelistOptionsDialog::IPSubnetWhitelistOptionsDialog(QWidget *parent)
    : QDialog(parent)
    , m_ui(new Ui::IPSubnetWhitelistOptionsDialog)
    , m_storeDialogSize(SETTINGS_KEY(u"Size"_s))
{
    m_ui->setupUi(this);

    connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QStringList authSubnetWhitelistStringList;
    for (const Utils::Net::Subnet &subnet : asConst(Preferences::instance()->getWebUIAuthSubnetWhitelist()))
        authSubnetWhitelistStringList << Utils::Net::subnetToString(subnet);
    m_model = new QStringListModel(authSubnetWhitelistStringList, this);

    m_sortFilter = new QSortFilterProxyModel(this);
    m_sortFilter->setDynamicSortFilter(true);
    m_sortFilter->setSourceModel(m_model);

    m_ui->whitelistedIPSubnetList->setModel(m_sortFilter);
    m_ui->whitelistedIPSubnetList->sortByColumn(0, Qt::AscendingOrder);
    m_ui->buttonWhitelistIPSubnet->setEnabled(false);

    if (const QSize dialogSize = m_storeDialogSize; dialogSize.isValid())
        resize(dialogSize);
}

IPSubnetWhitelistOptionsDialog::~IPSubnetWhitelistOptionsDialog()
{
    m_storeDialogSize = size();
    delete m_ui;
}

void IPSubnetWhitelistOptionsDialog::on_buttonBox_accepted()
{
    if (m_modified)
    {
        // save to session
        QStringList subnets;
        // Operate on the m_sortFilter to grab the strings in sorted order
        for (int i = 0; i < m_sortFilter->rowCount(); ++i)
            subnets.append(m_sortFilter->index(i, 0).data().toString());
        Preferences::instance()->setWebUIAuthSubnetWhitelist(subnets);
        QDialog::accept();
    }
    else
    {
        QDialog::reject();
    }
}

void IPSubnetWhitelistOptionsDialog::on_buttonWhitelistIPSubnet_clicked()
{
    const std::optional<Utils::Net::Subnet> subnet = Utils::Net::parseSubnet(m_ui->txtIPSubnet->text());
    if (!subnet)
    {
        QMessageBox::critical(this, tr("Error"), tr("The entered subnet is invalid."));
        return;
    }

    m_model->insertRow(m_model->rowCount());
    m_model->setData(m_model->index(m_model->rowCount() - 1, 0), Utils::Net::subnetToString(subnet.value()));
    m_ui->txtIPSubnet->clear();
    m_modified = true;
}

void IPSubnetWhitelistOptionsDialog::on_buttonDeleteIPSubnet_clicked()
{
    for (const auto &i : asConst(m_ui->whitelistedIPSubnetList->selectionModel()->selectedIndexes()))
        m_sortFilter->removeRow(i.row());

    m_modified = true;
}

void IPSubnetWhitelistOptionsDialog::on_txtIPSubnet_textChanged(const QString &subnetStr)
{
    m_ui->buttonWhitelistIPSubnet->setEnabled(Utils::Net::parseSubnet(subnetStr).has_value());
}
