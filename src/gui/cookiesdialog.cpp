/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2016  Vladimir Golovnev <glassez@yandex.ru>
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

#include "cookiesdialog.h"

#include <algorithm>

#include "base/global.h"
#include "base/net/downloadmanager.h"
#include "cookiesmodel.h"
#include "ui_cookiesdialog.h"
#include "uithememanager.h"
#include "utils.h"

#define SETTINGS_KEY(name) u"CookiesDialog/" name

CookiesDialog::CookiesDialog(QWidget *parent)
    : QDialog(parent)
    , m_ui(new Ui::CookiesDialog)
    , m_cookiesModel(new CookiesModel(Net::DownloadManager::instance()->allCookies(), this))
    , m_storeDialogSize(SETTINGS_KEY(u"Size"_s))
    , m_storeViewState("GUI/Qt6/" SETTINGS_KEY(u"ViewState"_s))
{
    m_ui->setupUi(this);

    connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    setWindowIcon(UIThemeManager::instance()->getIcon(u"browser-cookies"_s));

    m_ui->buttonAdd->setIcon(UIThemeManager::instance()->getIcon(u"list-add"_s));
    m_ui->buttonAdd->setIconSize(Utils::Gui::mediumIconSize());
    connect(m_ui->buttonAdd, &QToolButton::clicked, this, &CookiesDialog::onButtonAddClicked);

    m_ui->buttonDelete->setIcon(UIThemeManager::instance()->getIcon(u"list-remove"_s));
    m_ui->buttonDelete->setIconSize(Utils::Gui::mediumIconSize());
    connect(m_ui->buttonDelete, &QToolButton::clicked, this, &CookiesDialog::onButtonDeleteClicked);

    m_ui->treeView->setModel(m_cookiesModel);
    if (m_cookiesModel->rowCount() > 0)
        m_ui->treeView->selectionModel()->setCurrentIndex(
                    m_cookiesModel->index(0, 0),
                    QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);

    if (const QSize dialogSize = m_storeDialogSize; dialogSize.isValid())
        resize(dialogSize);

    m_ui->treeView->header()->restoreState(m_storeViewState);
}

CookiesDialog::~CookiesDialog()
{
    m_storeDialogSize = size();
    m_storeViewState = m_ui->treeView->header()->saveState();
    delete m_ui;
}

void CookiesDialog::accept()
{
    Net::DownloadManager::instance()->setAllCookies(m_cookiesModel->cookies());
    QDialog::accept();
}

void CookiesDialog::onButtonAddClicked()
{
    int row = m_ui->treeView->selectionModel()->currentIndex().row() + 1;

    m_cookiesModel->insertRow(row);
    m_ui->treeView->selectionModel()->setCurrentIndex(
                m_cookiesModel->index(row, 0), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

void CookiesDialog::onButtonDeleteClicked()
{
    QModelIndexList idxs = m_ui->treeView->selectionModel()->selectedRows();

    // sort in descending order
    std::sort(idxs.begin(), idxs.end(),
        [](const QModelIndex &l, const QModelIndex &r)
        {
            return (l.row() > r.row());
        }
    );

    for (const QModelIndex &idx : asConst(idxs))
        m_cookiesModel->removeRow(idx.row());
}
