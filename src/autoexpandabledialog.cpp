/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2013  Nick Tiskov
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
 * Contact : daymansmail@gmail.com
 */

#include <QDesktopWidget>

#include "autoexpandabledialog.h"
#include "ui_autoexpandabledialog.h"

AutoExpandableDialog::AutoExpandableDialog(QWidget *parent) : QDialog(parent), ui(new Ui::AutoExpandableDialog) {
  ui->setupUi(this);
}

AutoExpandableDialog::~AutoExpandableDialog() {
  delete ui;
}

QString AutoExpandableDialog::getText(QWidget *parent, const QString &title, const QString &label,
                                      QLineEdit::EchoMode mode, const QString &text, bool *ok,
                                      Qt::InputMethodHints inputMethodHints) {

  AutoExpandableDialog d(parent);
  d.setWindowTitle(title);
  d.ui->textLabel->setText(label);
  d.ui->textEdit->setText(text);
  d.ui->textEdit->setEchoMode(mode);
  d.ui->textEdit->setInputMethodHints(inputMethodHints);

  int textW = d.ui->textEdit->fontMetrics().width(text) + 4;
  int screenW = QApplication::desktop()->width() / 4;
  int wd = textW;

  if (!title.isEmpty()) {
    int _w = d.fontMetrics().width(title);
    if (_w > wd)
      wd = _w;
  }

  if (!label.isEmpty()) {
    int _w = d.ui->textLabel->fontMetrics().width(label);
    if (_w > wd)
      wd = _w;
  }


  // Now resize the dialog to fit the contents
  // Maximum value is whichever is smaller:
  // 1. screen width / 4
  // 2. max width of text from either of: label, title, textedit
  // If the value is less than dialog default size default size is used
  wd = textW < screenW ? textW : screenW;
  if (wd > d.width())
    d.resize(d.width() - d.ui->horizontalLayout->sizeHint().width() + wd, d.height());

  bool res = d.exec();
  if (ok)
    *ok = res;

  if (!res)
    return QString();

  return d.ui->textEdit->text();
}
