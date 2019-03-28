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

#ifndef DELETIONCONFIRMATIONDIALOG_H
#define DELETIONCONFIRMATIONDIALOG_H

#include <QDialog>
#include <QPushButton>

#include "base/preferences.h"
#include "guiiconprovider.h"
#include "ui_deletionconfirmationdialog.h"
#include "utils.h"

class DeletionConfirmationDialog : public QDialog
{
    Q_OBJECT

public:
    DeletionConfirmationDialog(QWidget *parent, const int &size, const QString &name, bool defaultDeleteFiles)
        : QDialog(parent)
        , m_ui(new Ui::DeletionConfirmationDialog)
    {
        m_ui->setupUi(this);
        if (size == 1)
            m_ui->label->setText(tr("Are you sure you want to delete '%1' from the transfer list?", "Are you sure you want to delete 'ubuntu-linux-iso' from the transfer list?").arg(name.toHtmlEscaped()));
        else
            m_ui->label->setText(tr("Are you sure you want to delete these %1 torrents from the transfer list?", "Are you sure you want to delete these 5 torrents from the transfer list?").arg(QString::number(size)));
        // Icons
        const QSize iconSize = Utils::Gui::largeIconSize();
        m_ui->labelWarning->setPixmap(GuiIconProvider::instance()->getIcon("dialog-warning").pixmap(iconSize));
        m_ui->labelWarning->setFixedWidth(iconSize.width());
        m_ui->rememberBtn->setIcon(GuiIconProvider::instance()->getIcon("object-locked"));
        m_ui->rememberBtn->setIconSize(Utils::Gui::mediumIconSize());

        m_ui->checkPermDelete->setChecked(defaultDeleteFiles || Preferences::instance()->deleteTorrentFilesAsDefault());
        connect(m_ui->checkPermDelete, &QCheckBox::clicked, this, &DeletionConfirmationDialog::updateRememberButtonState);
        m_ui->buttonBox->button(QDialogButtonBox::Cancel)->setFocus();

        Utils::Gui::resize(this);
    }

    ~DeletionConfirmationDialog()
    {
        delete m_ui;
    }

    bool shouldDeleteLocalFiles() const
    {
        return m_ui->checkPermDelete->isChecked();
    }

    static bool askForDeletionConfirmation(QWidget *parent, bool &deleteLocalFiles, const int &size, const QString &name)
    {
        DeletionConfirmationDialog dlg(parent, size, name, deleteLocalFiles);
        if (dlg.exec() == QDialog::Accepted) {
            deleteLocalFiles = dlg.shouldDeleteLocalFiles();
            return true;
        }
        return false;
    }

private slots:
    void updateRememberButtonState()
    {
        m_ui->rememberBtn->setEnabled(m_ui->checkPermDelete->isChecked() != Preferences::instance()->deleteTorrentFilesAsDefault());
    }

    void on_rememberBtn_clicked()
    {
        Preferences::instance()->setDeleteTorrentFilesAsDefault(m_ui->checkPermDelete->isChecked());
        m_ui->rememberBtn->setEnabled(false);
    }

private:
    Ui::DeletionConfirmationDialog *m_ui;
};

#endif // DELETIONCONFIRMATIONDIALOG_H
