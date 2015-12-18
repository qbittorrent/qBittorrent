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
#include "base/net/downloadmanager.h"

#include <QNetworkCookie>
#include <QDateTime>

enum CookiesCols { COOKIE_KEY, COOKIE_VALUE};

CookiesDlg::CookiesDlg(const QUrl &url, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CookiesDlg)
{
  ui->setupUi(this);
  // Icons
  ui->add_btn->setIcon(GuiIconProvider::instance()->getIcon("list-add"));
  ui->del_btn->setIcon(GuiIconProvider::instance()->getIcon("list-remove"));

  ui->infos_lbl->setText(tr("Common keys for cookies are: '%1', '%2'.\nYou should get this information from your Web browser preferences.").arg("uid").arg("pass"));

  QList<QNetworkCookie> cookies = Net::DownloadManager::instance()->cookiesForUrl(url);
  foreach (const QNetworkCookie &cookie, cookies) {
    const int i = ui->cookiesTable->rowCount();
    ui->cookiesTable->setRowCount(i+1);
    ui->cookiesTable->setItem(i, COOKIE_KEY, new QTableWidgetItem(QString(cookie.name())));
    ui->cookiesTable->setItem(i, COOKIE_VALUE, new QTableWidgetItem(QString(cookie.value())));
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

QList<QNetworkCookie> CookiesDlg::getCookies() const {
  QList<QNetworkCookie> ret;
  auto now = QDateTime::currentDateTime();
  for (int i=0; i<ui->cookiesTable->rowCount(); ++i) {
    QString key;
    if (ui->cookiesTable->item(i, COOKIE_KEY))
      key = ui->cookiesTable->item(i, COOKIE_KEY)->text().trimmed();
    QString value;
    if (ui->cookiesTable->item(i, COOKIE_VALUE))
      value = ui->cookiesTable->item(i, COOKIE_VALUE)->text().trimmed();
    if (!key.isEmpty() && !value.isEmpty()) {
      QNetworkCookie cookie(key.toUtf8(), value.toUtf8());
      // TODO: Delete this hack when advanced Cookie dialog will be implemented.
      cookie.setExpirationDate(now.addYears(10));
      qDebug("Cookie: %s", cookie.toRawForm().data());
      ret << cookie;
    }
  }
  return ret;
}

bool CookiesDlg::askForCookies(QWidget *parent, const QUrl &url, QList<QNetworkCookie> &out)
{
  CookiesDlg dlg(url, parent);
  if (dlg.exec()) {
      out = dlg.getCookies();
      return true;
  }

  return false;
}
