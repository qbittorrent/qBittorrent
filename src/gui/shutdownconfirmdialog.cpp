/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2014  sledgehammer999 <hammered999@gmail.com>
 * Copyright (C) 2011  Christophe Dumez <chris@qbittorrent.org>
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

#include "shutdownconfirmdialog.h"

#include <chrono>

#include <QDialogButtonBox>
#include <QIcon>
#include <QPushButton>
#include <QStyle>

#include "base/preferences.h"
#include "ui_shutdownconfirmdialog.h"
#include "utils.h"

using namespace std::chrono_literals;

ShutdownConfirmDialog::ShutdownConfirmDialog(QWidget *parent, const ShutdownDialogAction &action)
    : QDialog(parent)
    , m_ui(new Ui::ShutdownConfirmDialog)
    , m_action(action)
{
    m_ui->setupUi(this);

    initText();
    QIcon warningIcon(style()->standardIcon(QStyle::SP_MessageBoxWarning));
    m_ui->warningLabel->setPixmap(warningIcon.pixmap(32));

    if (m_action == ShutdownDialogAction::Exit)
        m_ui->neverShowAgainCheckbox->setVisible(true);
    else
        m_ui->neverShowAgainCheckbox->setVisible(false);

    // Cancel Button
    QPushButton *cancelButton = m_ui->buttonBox->button(QDialogButtonBox::Cancel);
    cancelButton->setFocus();
    cancelButton->setDefault(true);

    connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Always on top
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
    move(Utils::Gui::screenCenter(this));

    m_timer.setInterval(1s);
    connect(&m_timer, &QTimer::timeout, this, &ShutdownConfirmDialog::updateSeconds);
}

ShutdownConfirmDialog::~ShutdownConfirmDialog()
{
    delete m_ui;
}

void ShutdownConfirmDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    m_timer.start();
}

bool ShutdownConfirmDialog::askForConfirmation(QWidget *parent, const ShutdownDialogAction &action)
{
    ShutdownConfirmDialog dlg(parent, action);
    return (dlg.exec() == QDialog::Accepted);
}

void ShutdownConfirmDialog::updateSeconds()
{
    --m_timeout;
    updateText();
    if (m_timeout == 0)
    {
        m_timer.stop();
        accept();
    }
}

void ShutdownConfirmDialog::accept()
{
    Preferences::instance()->setDontConfirmAutoExit(m_ui->neverShowAgainCheckbox->isChecked());
    QDialog::accept();
}

void ShutdownConfirmDialog::initText()
{
    QPushButton *okButton = m_ui->buttonBox->button(QDialogButtonBox::Ok);

    switch (m_action)
    {
    case ShutdownDialogAction::Exit:
        m_msg = tr("qBittorrent will now exit.");
        okButton->setText(tr("E&xit Now"));
        setWindowTitle(tr("Exit confirmation"));
        break;
    case ShutdownDialogAction::Shutdown:
        m_msg = tr("The computer is going to shutdown.");
        okButton->setText(tr("&Shutdown Now"));
        setWindowTitle(tr("Shutdown confirmation"));
        break;
    case ShutdownDialogAction::Suspend:
        m_msg = tr("The computer is going to enter suspend mode.");
        okButton->setText(tr("&Suspend Now"));
        setWindowTitle(tr("Suspend confirmation"));
        break;
    case ShutdownDialogAction::Hibernate:
        m_msg = tr("The computer is going to enter hibernation mode.");
        okButton->setText(tr("&Hibernate Now"));
        setWindowTitle(tr("Hibernate confirmation"));
        break;
    }

    m_msg += u'\n';
    updateText();
}

void ShutdownConfirmDialog::updateText()
{
    QString t = tr("You can cancel the action within %1 seconds.").arg(QString::number(m_timeout)) + u'\n';
    m_ui->shutdownText->setText(m_msg + t);
}
