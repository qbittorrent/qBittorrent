/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2010  Christophe Dumez
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
 * Contact : chris@qbittorrent.org arnaud@qbittorrent.org
 */

#include "cookiesdlg.h"
#include "ui_cookiesdlg.h"
#include "guiiconprovider.h"

#include <QNetworkCookie>

enum CookiesCols { COOKIE_KEY, COOKIE_VALUE};

CookiesDlg::CookiesDlg(QWidget *parent, const QList<QByteArray> &raw_cookies) :
    QDialog(parent),
    ui(new Ui::CookiesDlg)
{
  ui->setupUi(this);
  // Icons
  ui->add_btn->setIcon(GuiIconProvider::instance()->getIcon("list-add"));
  ui->del_btn->setIcon(GuiIconProvider::instance()->getIcon("list-remove"));

  ui->infos_lbl->setText(tr("Common keys for cookies are: '%1', '%2'.\nYou should get this information from your Web browser preferences.").arg("uid").arg("pass"));
  foreach (const QByteArray &raw_cookie, raw_cookies) {
    QList<QByteArray> cookie_parts = raw_cookie.split('=');
    if (cookie_parts.size() != 2) continue;
    const int i = ui->cookiesTable->rowCount();
    ui->cookiesTable->setRowCount(i+1);
    ui->cookiesTable->setItem(i, COOKIE_KEY, new QTableWidgetItem(cookie_parts.first().data()));
    ui->cookiesTable->setItem(i, COOKIE_VALUE, new QTableWidgetItem(cookie_parts.last().data()));
  }
}

CookiesDlg::~CookiesDlg()
{
  delete ui;
}

void CookiesDlg::on_add_btn_clicked() {
  ui->cookiesTable->setRowCount(ui->cookiesTable->rowCount()+1);
  // Edit first column
  ui->cookiesTable->editItem(ui->cookiesTable->item(ui->cookiesTable->rowCount()-1, COOKIE_KEY));
}

void CookiesDlg::on_del_btn_clicked() {
  // Get selected cookie
  QList<QTableWidgetItem*> selection = ui->cookiesTable->selectedItems();
  if (!selection.isEmpty()) {
    ui->cookiesTable->removeRow(selection.first()->row());
  }
}

QList<QByteArray> CookiesDlg::getCookies() const {
  QList<QByteArray> ret;
  for (int i=0; i<ui->cookiesTable->rowCount(); ++i) {
    QString key;
    if (ui->cookiesTable->item(i, COOKIE_KEY))
      key = ui->cookiesTable->item(i, COOKIE_KEY)->text().trimmed();
    QString value;
    if (ui->cookiesTable->item(i, COOKIE_VALUE))
      value = ui->cookiesTable->item(i, COOKIE_VALUE)->text().trimmed();
    if (!key.isEmpty() && !value.isEmpty()) {
      const QString raw_cookie = key+"="+value;
      qDebug("Cookie: %s", qPrintable(raw_cookie));
      ret << raw_cookie.toLocal8Bit();
    }
  }
  return ret;
}

QList<QByteArray> CookiesDlg::askForCookies(QWidget *parent, const QList<QByteArray> &raw_cookies, bool *ok) {
  CookiesDlg dlg(parent, raw_cookies);
  if (dlg.exec()) {
    *ok = true;
    return dlg.getCookies();
  }
  *ok = false;
  return QList<QByteArray>();
}
