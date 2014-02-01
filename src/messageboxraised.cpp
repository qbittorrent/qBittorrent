/*
 * Bittorrent Client using Qt4 and libtorrent.
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
 * Contact : hammered999@gmail.com
 */

#include "messageboxraised.h"

MessageBoxRaised::MessageBoxRaised(QMessageBox::Icon icon, const QString &title, const QString &text,
                                   QMessageBox::StandardButtons buttons, QWidget *parent, Qt::WindowFlags f)
  : QMessageBox(icon, title, text, buttons, parent, f) {}

QMessageBox::StandardButton MessageBoxRaised::impl(const QMessageBox::Icon &icon, QWidget *parent, const QString &title, const QString &text, QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton) {
  MessageBoxRaised dlg(icon, title, text, buttons, parent);
  dlg.setDefaultButton(defaultButton);
  return (QMessageBox::StandardButton)dlg.exec();
}

QMessageBox::StandardButton MessageBoxRaised::critical(QWidget *parent, const QString &title, const QString &text, QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton) {
  return impl(Critical, parent, title, text, buttons, defaultButton);
}

QMessageBox::StandardButton MessageBoxRaised::information(QWidget *parent, const QString &title, const QString &text, QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton) {
  return impl(Information, parent, title, text, buttons, defaultButton);
}

QMessageBox::StandardButton MessageBoxRaised::question(QWidget *parent, const QString &title, const QString &text, QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton) {
  return impl(Question, parent, title, text, buttons, defaultButton);
}

QMessageBox::StandardButton MessageBoxRaised::warning(QWidget *parent, const QString &title, const QString &text, QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton) {
  return impl(Warning, parent, title, text, buttons, defaultButton);
}

void MessageBoxRaised::showEvent(QShowEvent *event) {
  QMessageBox::showEvent(event);
  activateWindow();
  raise();
}
