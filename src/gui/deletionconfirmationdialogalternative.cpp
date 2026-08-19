/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2024-2026  Vladimir Golovnev <glassez@yandex.ru>
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

#include "deletionconfirmationdialogalternative.h"

#include <QPushButton>

#include "base/bittorrent/session.h"
#include "base/preferences.h"
#include "uithememanager.h"
#include "utils.h"

using namespace Qt::Literals::StringLiterals;

DeletionConfirmationDialogAlternative::DeletionConfirmationDialogAlternative(QWidget *parent, const int torrentsCount, const QString &name)
    : QDialog(parent)
    , m_ui {new Ui::DeletionConfirmationDialogAlternative}
{
    m_ui->setupUi(this);

    if (torrentsCount == 1)
        m_ui->label->setText(tr("Are you sure you want to remove '%1' from the transfer list?", "Are you sure you want to remove 'ubuntu-linux-iso' from the transfer list?").arg(name.toHtmlEscaped()));
    else
        m_ui->label->setText(tr("Are you sure you want to remove these %1 torrents from the transfer list?", "Are you sure you want to remove these 5 torrents from the transfer list?").arg(QString::number(torrentsCount)));

    // Warning Icon Setup
    const QSize iconSize = Utils::Gui::largeIconSize();
    m_ui->labelWarning->setPixmap(UIThemeManager::instance()->getIcon(u"dialog-warning"_s).pixmap(iconSize));
    m_ui->labelWarning->setFixedWidth(iconSize.width());

    // Connect custom removal buttons directly to accept/reject flow
    // (Assumes m_ui contains btnRemoveTorrent, btnRemoveTorrentAndFiles, and btnCancel)
    connect(m_ui->btnRemoveTorrentAndFiles, &QPushButton::clicked, this, [this] {
        m_removeContent = true;
        accept();
    });

    connect(m_ui->btnRemoveTorrent, &QPushButton::clicked, this, [this] {
        m_removeContent = false;
        accept();
    });

    connect(m_ui->btnCancel, &QPushButton::clicked, this, &QDialog::reject);
}

DeletionConfirmationDialogAlternative::~DeletionConfirmationDialogAlternative()
{
    delete m_ui;
}

bool DeletionConfirmationDialogAlternative::isRemoveContentSelected() const
{
    return m_removeContent;
}
