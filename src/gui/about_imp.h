/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Christophe Dumez
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
 */

#ifndef ABOUT_H
#define ABOUT_H

#include "ui_about.h"
#include <QFile>
#include <QtGlobal>
#include <libtorrent/version.hpp>
#include <boost/version.hpp>
#include "core/unicodestrings.h"

class about : public QDialog, private Ui::AboutDlg{
  Q_OBJECT

  public:
    ~about() {
      qDebug("Deleting about dlg");
    }

    about(QWidget *parent): QDialog(parent) {
      setupUi(this);
      setAttribute(Qt::WA_DeleteOnClose);
      // About
      QString aboutText =
          QString::fromUtf8("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\"><html><head><meta name=\"qrichtext\" content=\"1\" /><style type=\"text/css\">p, li { white-space: pre-wrap; }</style></head><body style=\" font-size:11pt; font-weight:400; font-style:normal;\"><p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">") +
          tr("An advanced BitTorrent client programmed in <nobr>C++</nobr>, based on Qt toolkit and libtorrent-rasterbar.") +
          QString::fromUtf8(" <br /><br />") +
          trUtf8("Copyright %1 2006-2015 The qBittorrent project").arg(C_COPYRIGHT) +
          QString::fromUtf8("<br /><br />") +
          tr("Home Page: ") +
          QString::fromUtf8("<a href=\"http://www.qbittorrent.org\"><span style=\" text-decoration: underline; color:#0000ff;\">http://www.qbittorrent.org</span></a></p><p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">") +
          tr("Bug Tracker: ") +
          QString::fromUtf8("<a href=\"http://bugs.qbittorrent.org\"><span style=\" text-decoration: underline; color:#0000ff;\">http://bugs.qbittorrent.org</span></a><br />") +
          tr("Forum: ") +
          QString::fromUtf8(
              "<a href=\"http://forum.qbittorrent.org\"><span style=\" text-decoration: underline; color:#0000ff;\">http://forum.qbittorrent.org</span></a></p><p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">") +
          tr("IRC: #qbittorrent on Freenode") +
          QString::fromUtf8(
              "</p></body></html>");
      lb_about->setText(aboutText);
      // Set icons
      logo->setPixmap(QPixmap(QString::fromUtf8(":/icons/skin/qbittorrent22.png")));
      //Title
      lb_name->setText(QString::fromUtf8("<b><h1>qBittorrent")+QString::fromUtf8(" " VERSION"</h1></b>"));
      // Thanks
      QFile thanksfile(":/thanks.html");
      if (thanksfile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        te_thanks->setHtml(QString::fromUtf8(thanksfile.readAll().constData()));
        thanksfile.close();
      }
      // Translation
      QFile translatorsfile(":/translators.html");
      if (translatorsfile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        te_translation->setHtml(QString::fromUtf8(translatorsfile.readAll().constData()));
        translatorsfile.close();
      }
      // License
      QFile licensefile(":/gpl.html");
      if (licensefile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        te_license->setHtml(QString::fromUtf8(licensefile.readAll().constData()));
        licensefile.close();
      }
      // Libraries
      label_11->setText(QT_VERSION_STR);
      label_12->setText(LIBTORRENT_VERSION);
      label_13->setText(QString::number(BOOST_VERSION / 100000) + "." + QString::number((BOOST_VERSION / 100) % 1000) + "." + QString::number(BOOST_VERSION % 100));
      show();
    }
};

#endif
