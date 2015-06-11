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

#include "core/types.h"
#include "shutdownconfirm.h"

#include <QPushButton>

ShutdownConfirmDlg::ShutdownConfirmDlg(const ShutdownAction &action)
    : m_exitNow(0)
    , m_timeout(15)
    , m_action(action)
{
    // Title and button
    if (m_action == ShutdownAction::None) {
        setWindowTitle(tr("Exit confirmation"));
        m_exitNow = addButton(tr("Exit now"), QMessageBox::AcceptRole);
    }
    else {
        setWindowTitle(tr("Shutdown confirmation"));
        m_exitNow = addButton(tr("Shutdown now"), QMessageBox::AcceptRole);
    }
    // Cancel Button
    addButton(QMessageBox::Cancel);
    // Text
    updateText();
    // Icon
    setIcon(QMessageBox::Warning);
    // Always on top
    setWindowFlags(windowFlags()|Qt::WindowStaysOnTopHint);
    // Set 'Cancel' as default button.
    setDefaultButton(QMessageBox::Cancel);
    m_timer.setInterval(1000); // 1sec
    connect(&m_timer, SIGNAL(m_timeout()), this, SLOT(updateSeconds()));
    show();
    // Move to center
    move(Utils::Misc::screenCenter(this));
}

void ShutdownConfirmDlg::showEvent(QShowEvent *event)
{
    QMessageBox::showEvent(event);
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

bool ShutdownConfirmDlg::shutdown() const
{
    // This is necessary because result() in the case of QMessageBox
    // returns a type of StandardButton, but since we use a custom button
    // it will return 0 instead, even though we set the 'accept' role on it.
    if (result() != QDialog::Accepted)
        return (clickedButton() == m_exitNow);
    else
        return true;
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

    setText(text);
}

QAbstractButton *ShutdownConfirmDlg::getExit_now() const
{
    return m_exitNow;
}

void ShutdownConfirmDlg::setExit_now(QAbstractButton *value)
{
    m_exitNow = value;
}
