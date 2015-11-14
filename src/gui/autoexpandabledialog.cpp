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

#include "mainwindow.h"
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

  bool res = d.exec();
  if (ok)
    *ok = res;

  if (!res)
    return QString();

  return d.ui->textEdit->text();
}

void AutoExpandableDialog::showEvent(QShowEvent *e) {
  // Overriding showEvent is required for consistent UI with fixed size under custom DPI
  // Show dialog
  QDialog::showEvent(e);
  // and resize textbox to fit the text

  // NOTE: For some strange reason QFontMetrics gets more accurate
  // when called from showEvent. Only 6 symbols off instead of 11 symbols off.
  int textW = ui->textEdit->fontMetrics().width(ui->textEdit->text()) + 4;
  int screenW = QApplication::desktop()->width() / 4;
  int wd = textW;

  if (!windowTitle().isEmpty()) {
    int _w = fontMetrics().width(windowTitle());
    if (_w > wd)
      wd = _w;
  }

  if (!ui->textLabel->text().isEmpty()) {
    int _w = ui->textLabel->fontMetrics().width(ui->textLabel->text());
    if (_w > wd)
      wd = _w;
  }


  // Now resize the dialog to fit the contents
  // Maximum value is whichever is smaller:
  // 1. screen width / 4
  // 2. max width of text from either of: label, title, textedit
  // If the value is less than dialog default size default size is used
  wd = textW < screenW ? textW : screenW;
  if (wd > width())
    resize(width() - ui->horizontalLayout->sizeHint().width() + wd, height());

  // Use old dialog behavior: prohibit resizing the dialog
  setFixedHeight(height());

  // Update geometry: center on screen
  QDesktopWidget *desk = QApplication::desktop();
  MainWindow *wnd = qobject_cast<MainWindow*>(QApplication::activeWindow());
  QPoint p = QCursor::pos();

  int screenNum = 0;
  if (wnd == 0)
    screenNum = desk->screenNumber(p);
  else if (!wnd->isHidden())
    screenNum = desk->screenNumber(wnd);
  else
    screenNum = desk->screenNumber(p);

  QRect screenRes = desk->screenGeometry(screenNum);

  QRect geom = geometry();
  geom.moveCenter(QPoint(screenRes.width() / 2, screenRes.height() / 2));
  setGeometry(geom);
}
