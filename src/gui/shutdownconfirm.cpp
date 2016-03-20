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

#include "base/types.h"
#include "shutdownconfirm.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStyle>
#include <QIcon>
#include <QLabel>
#include <QDialogButtonBox>
#include <QCheckBox>
#include <QPushButton>

ShutdownConfirmDlg::ShutdownConfirmDlg(const ShutdownAction &action)
    : m_neverShowAgain(false)
    , m_timeout(15)
    , m_action(action)
{
    QVBoxLayout *myLayout = new QVBoxLayout();
    //Warning Icon and Text
    QHBoxLayout *messageRow = new QHBoxLayout();
    QLabel *warningLabel = new QLabel();
    QIcon warningIcon(style()->standardIcon(QStyle::SP_MessageBoxWarning, 0, this));
    warningLabel->setPixmap(warningIcon.pixmap(warningIcon.actualSize(QSize(32, 32))));
    messageRow->addWidget(warningLabel);
    m_text = new QLabel();
    messageRow->addWidget(m_text);
    myLayout->addLayout(messageRow);
    updateText();
    QDialogButtonBox *buttons = new QDialogButtonBox(Qt::Horizontal);
    // Never show again checkbox, Title, and button
    if (m_action == ShutdownAction::None) {
        //Never show again checkbox (shown only when exitting without shutdown)
        QCheckBox *neverShowAgainCheckbox = new QCheckBox(tr("Never show again"));
        myLayout->addWidget(neverShowAgainCheckbox, 0, Qt::AlignHCenter);
        //Title and button
        connect(neverShowAgainCheckbox, SIGNAL(clicked(bool)), this, SLOT(handleNeverShowAgainCheckboxToggled(bool)));
        setWindowTitle(tr("Exit confirmation"));
        buttons->addButton(new QPushButton(tr("Exit Now")), QDialogButtonBox::AcceptRole);
    }
    else {
        setWindowTitle(tr("Shutdown confirmation"));
        buttons->addButton(new QPushButton(tr("Shutdown Now")), QDialogButtonBox::AcceptRole);
    }
    // Cancel Button
    QPushButton *cancelButton = buttons->addButton(QDialogButtonBox::Cancel);
    cancelButton->setDefault(true);
    myLayout->addWidget(buttons, 0, Qt::AlignHCenter);
    connect(buttons, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttons, SIGNAL(rejected()), this, SLOT(reject()));
    // Always on top
    setWindowFlags(windowFlags()|Qt::WindowStaysOnTopHint);
    // Set 'Cancel' as default button.
    m_timer.setInterval(1000); // 1sec
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(updateSeconds()));
    // Move to center
    move(Utils::Misc::screenCenter(this));
    setLayout(myLayout);
    cancelButton->setFocus();
}

bool ShutdownConfirmDlg::neverShowAgain() const
{
    return m_neverShowAgain;
}

void ShutdownConfirmDlg::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    m_timer.start();
}

void ShutdownConfirmDlg::askForConfirmation(const ShutdownAction &action, bool *shutdownConfirmed, bool *neverShowAgain)
{
    ShutdownConfirmDlg dlg(action);
    dlg.exec();
    *shutdownConfirmed = dlg.shutdown();
    if (neverShowAgain)
        *neverShowAgain = dlg.neverShowAgain();
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

void ShutdownConfirmDlg::handleNeverShowAgainCheckboxToggled(bool checked)
{
    m_neverShowAgain = checked;
}

bool ShutdownConfirmDlg::shutdown() const
{
    return (result() == QDialog::Accepted);
}

void ShutdownConfirmDlg::updateText()
{
    QString text;

    switch (m_action) {
    case ShutdownAction::None:
        text = tr("qBittorrent will now exit unless you cancel within the next %1 seconds.").arg(QString::number(m_timeout));
        break;
    case ShutdownAction::Shutdown:
        text = tr("The computer will now be switched off unless you cancel within the next %1 seconds.").arg(QString::number(m_timeout));
        break;
    case ShutdownAction::Suspend:
        text = tr("The computer will now go to sleep mode unless you cancel within the next %1 seconds.").arg(QString::number(m_timeout));
        break;
    case ShutdownAction::Hibernate:
        text = tr("The computer will now go to hibernation mode unless you cancel within the next %1 seconds.").arg(QString::number(m_timeout));
        break;
    }

    m_text->setText(text);
}
