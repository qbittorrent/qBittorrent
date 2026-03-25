/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2024  Vladimir Golovnev <glassez@yandex.ru>
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

#include "deletionconfirmationdialog.h"

#include <QPushButton>

#include "base/bittorrent/session.h"
#include "base/global.h"
#include "base/preferences.h"
#include "uithememanager.h"
#include "utils.h"

DeletionConfirmationDialog::DeletionConfirmationDialog(QWidget *parent, const int size, const QString &name, const bool defaultDeleteFiles)
    : QDialog(parent)
    , m_ui(new Ui::DeletionConfirmationDialog)
{
    m_ui->setupUi(this);

    if (size == 1)
        m_ui->label->setText(tr("Are you sure you want to remove '%1' from the transfer list?", "Are you sure you want to remove 'ubuntu-linux-iso' from the transfer list?").arg(name.toHtmlEscaped()));
    else
        m_ui->label->setText(tr("Are you sure you want to remove these %1 torrents from the transfer list?", "Are you sure you want to remove these 5 torrents from the transfer list?").arg(QString::number(size)));

    // Icons
    const QSize iconSize = Utils::Gui::largeIconSize();
    m_ui->labelWarning->setPixmap(UIThemeManager::instance()->getIcon(u"dialog-warning"_s).pixmap(iconSize));
    m_ui->labelWarning->setFixedWidth(iconSize.width());
    m_ui->rememberBtn->setIcon(UIThemeManager::instance()->getIcon(u"object-locked"_s));
    m_ui->rememberBtn->setIconSize(Utils::Gui::mediumIconSize());

    m_ui->checkRemoveContent->setChecked(defaultDeleteFiles || Preferences::instance()->removeTorrentContent());
    connect(m_ui->checkRemoveContent, &QCheckBox::clicked, this, &DeletionConfirmationDialog::updateRememberButtonState);
    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Remove"));
    m_ui->buttonBox->button(QDialogButtonBox::Cancel)->setFocus();

    connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

DeletionConfirmationDialog::~DeletionConfirmationDialog()
{
    delete m_ui;
}

bool DeletionConfirmationDialog::isRemoveContentSelected() const
{
    return m_ui->checkRemoveContent->isChecked();
}

void DeletionConfirmationDialog::updateRememberButtonState()
{
    m_ui->rememberBtn->setEnabled(m_ui->checkRemoveContent->isChecked() != Preferences::instance()->removeTorrentContent());
}

void DeletionConfirmationDialog::on_rememberBtn_clicked()
{
    Preferences::instance()->setRemoveTorrentContent(m_ui->checkRemoveContent->isChecked());
    m_ui->rememberBtn->setEnabled(false);
}
