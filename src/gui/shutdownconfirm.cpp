/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2011  Christophe Dumez
 * Copyright (C) 2014  sledgehammer999
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
 * Contact : hammered999@gmail.com
 */

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStyle>
#include <QIcon>
#include <QLabel>
#include <QDialogButtonBox>
#include <QCheckBox>
#include <QPushButton>

#include "base/preferences.h"
#include "base/types.h"

#include "shutdownconfirm.h"
#include "ui_confirmshutdowndlg.h"

ShutdownConfirmDlg::ShutdownConfirmDlg(const ShutdownAction &action)
    : ui(new Ui::confirmShutdownDlg)
    , m_timeout(15)
    , m_action(action)
{
    ui->setupUi(this);

    QIcon warningIcon(style()->standardIcon(QStyle::SP_MessageBoxWarning));
    ui->warningLabel->setPixmap(warningIcon.pixmap(32));

    updateText();

    if (m_action == ShutdownAction::None)
        ui->neverShowAgainCheckbox->setVisible(true);
    else
        ui->neverShowAgainCheckbox->setVisible(false);

    // Cancel Button
    QPushButton *cancelButton = ui->buttonBox->button(QDialogButtonBox::Cancel);
    cancelButton->setFocus();
    cancelButton->setDefault(true);
    // Always on top
    setWindowFlags(windowFlags()|Qt::WindowStaysOnTopHint);
    m_timer.setInterval(1000); // 1sec
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(updateSeconds()));
    // Move to center
    move(Utils::Misc::screenCenter(this));
}

ShutdownConfirmDlg::~ShutdownConfirmDlg()
{
  delete ui;
}

void ShutdownConfirmDlg::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    m_timer.start();
}

bool ShutdownConfirmDlg::askForConfirmation(const ShutdownAction &action)
{
    ShutdownConfirmDlg dlg(action);
    dlg.exec();
    return dlg.shutdown();
}

void ShutdownConfirmDlg::updateSeconds()
{
    --m_timeout;
    updateText();
    if (m_timeout == 0) {
        m_timer.stop();
        accept();
    }
}

void ShutdownConfirmDlg::accept()
{
    Preferences::instance()->setDontConfirmAutoExit(ui->neverShowAgainCheckbox->isChecked());
    QDialog::accept();
}

bool ShutdownConfirmDlg::shutdown() const
{
    return (result() == QDialog::Accepted);
}

void ShutdownConfirmDlg::updateText()
{
    QString text;
    QPushButton *okButton = ui->buttonBox->button(QDialogButtonBox::Ok);

    switch (m_action) {
    case ShutdownAction::None:
        text = tr("qBittorrent will now exit.");

        okButton->setText(tr("E&xit Now"));
        setWindowTitle(tr("Exit confirmation"));
        break;
    case ShutdownAction::Shutdown:
        text = tr("The computer is going to shutdown.");

        okButton->setText(tr("&Shutdown Now"));
        setWindowTitle(tr("Shutdown confirmation"));
        break;
    case ShutdownAction::Suspend:
        text = tr("The computer is going to enter suspend mode.");

        okButton->setText(tr("&Suspend Now"));
        setWindowTitle(tr("Suspend confirmation"));
        break;
    case ShutdownAction::Hibernate:
        text = tr("The computer is going to enter hibernation mode.");

        okButton->setText(tr("&Hibernate Now"));
        setWindowTitle(tr("Hibernate confirmation"));
        break;
    }

    text += "\n" + tr("You can cancel the action within %1 seconds.").arg(QString::number(m_timeout)) + "\n";
    ui->shutdownText->setText(text);
}
