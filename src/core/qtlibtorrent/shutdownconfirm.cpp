/*
 * Bittorrent Client using Qt4 and libtorrent.
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

#include "shutdownconfirm.h"

#include <QPushButton>

ShutdownConfirmDlg::ShutdownConfirmDlg(const shutDownAction &action): exit_now(NULL), timeout(15), action0(action) {
    // Title and button
    if (action0 == NO_SHUTDOWN) {
        setWindowTitle(tr("Exit confirmation"));
        exit_now = addButton(tr("Exit now"), QMessageBox::AcceptRole);
    }
    else {
        setWindowTitle(tr("Shutdown confirmation"));
        exit_now = addButton(tr("Shutdown now"), QMessageBox::AcceptRole);
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
    timer.setInterval(1000); // 1sec
    connect(&timer, SIGNAL(timeout()), this, SLOT(updateSeconds()));
    show();
    // Move to center
    move(misc::screenCenter(this));
}

void ShutdownConfirmDlg::showEvent(QShowEvent *event) {
    QMessageBox::showEvent(event);
    timer.start();
}

bool ShutdownConfirmDlg::askForConfirmation(const shutDownAction &action) {
    ShutdownConfirmDlg dlg(action);
    dlg.exec();
    return dlg.shutdown();
}

void ShutdownConfirmDlg::updateSeconds() {
    --timeout;
    updateText();
    if (timeout == 0) {
        timer.stop();
        accept();
    }
}

bool ShutdownConfirmDlg::shutdown() const {
    // This is necessary because result() in the case of QMessageBox
    // returns a type of StandardButton, but since we use a custom button
    // it will return 0 instead, even though we set the 'accept' role on it.
    if (result() != QDialog::Accepted)
        return (clickedButton() == exit_now);
    else
        return true;
}

void ShutdownConfirmDlg::updateText() {
    QString text;

    switch (action0) {
    case NO_SHUTDOWN:
        text = tr("qBittorrent will now exit unless you cancel within the next %1 seconds.").arg(QString::number(timeout));
        break;
    case SHUTDOWN_COMPUTER:
        text = tr("The computer will now be switched off unless you cancel within the next %1 seconds.").arg(QString::number(timeout));
        break;
    case SUSPEND_COMPUTER:
        text = tr("The computer will now go to sleep mode unless you cancel within the next %1 seconds.").arg(QString::number(timeout));
        break;
    case HIBERNATE_COMPUTER:
        text = tr("The computer will now go to hibernation mode unless you cancel within the next %1 seconds.").arg(QString::number(timeout));
        break;
    }

    setText(text);
}

