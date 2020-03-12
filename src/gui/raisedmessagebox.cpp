/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2014  sledgehammer999 <hammered999@gmail.com>
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

#include "raisedmessagebox.h"

RaisedMessageBox::RaisedMessageBox(QMessageBox::Icon icon, const QString &title, const QString &text,
                                   QMessageBox::StandardButtons buttons, QWidget *parent, Qt::WindowFlags f)
  : QMessageBox(icon, title, text, buttons, parent, f)
{
}

QMessageBox::StandardButton RaisedMessageBox::impl(const QMessageBox::Icon &icon, QWidget *parent, const QString &title, const QString &text, QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton)
{
    RaisedMessageBox dlg(icon, title, text, buttons, parent);
    dlg.setDefaultButton(defaultButton);
    return static_cast<QMessageBox::StandardButton>(dlg.exec());
}

QMessageBox::StandardButton RaisedMessageBox::critical(QWidget *parent, const QString &title, const QString &text, QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton)
{
    return impl(Critical, parent, title, text, buttons, defaultButton);
}

QMessageBox::StandardButton RaisedMessageBox::information(QWidget *parent, const QString &title, const QString &text, QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton)
{
    return impl(Information, parent, title, text, buttons, defaultButton);
}

QMessageBox::StandardButton RaisedMessageBox::question(QWidget *parent, const QString &title, const QString &text, QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton)
{
    return impl(Question, parent, title, text, buttons, defaultButton);
}

QMessageBox::StandardButton RaisedMessageBox::warning(QWidget *parent, const QString &title, const QString &text, QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton)
{
    return impl(Warning, parent, title, text, buttons, defaultButton);
}

void RaisedMessageBox::showEvent(QShowEvent *event)
{
    QMessageBox::showEvent(event);
    activateWindow();
    raise();
}
