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

class about : public QDialog, private Ui::AboutDlg{
  Q_OBJECT

  public:
    ~about() {
      qDebug("Deleting about dlg");
    }

    about(QWidget *parent): QDialog(parent) {
      setupUi(this);
      setAttribute(Qt::WA_DeleteOnClose);
      // Set icons
      logo->setPixmap(QPixmap(QString::fromUtf8(":/Icons/skin/qbittorrent22.png")));
      //Title
      lb_name->setText(QString::fromUtf8("<b><h1>")+tr("qBittorrent")+QString::fromUtf8(" "VERSION"</h1></b>"));
      // Thanks
      QString thanks_txt;
      thanks_txt += QString::fromUtf8("<p>I would first like to thank sourceforge.net for hosting qBittorrent project and for their support.</p>");
      thanks_txt += QString::fromUtf8("<p>I am pleased that people from all over the world are contributing to qBittorrent: Ishan Arora (India), Arnaud Demaizière (France) and Stephanos Antaris (Greece). Their help is greatly appreciated</p>");
      thanks_txt += QString::fromUtf8("<p>I also want to thank Στέφανος Αντάρης (santaris@csd.auth.gr) and Mirco Chinelli (infinity89@fastwebmail.it) for working on Mac OS X packaging.</p>");
      thanks_txt += QString::fromUtf8("<p>I am grateful to Peter Koeleman (peter@qbittorrent.org) and Mohammad Dib (mdib@qbittorrent.org) for working on qBittorrent port to Windows.</p>");
      thanks_txt += QString::fromUtf8("<p>Thanks a lot to our graphist Mateusz Toboła (tobejodok@qbittorrent.org) for his great work.</p>");
      te_thanks->setHtml(thanks_txt);
      // Translation
      QString trans_txt = "<p>"+tr("I would like to thank the following people who volunteered to translate qBittorrent:")+"</p>";
      trans_txt += QString::fromUtf8("<ul><li><u>Arabic:</u> SDERAWI (abz8868@msn.com) and sn51234 (nesseyan@gmail.com)</li>\
          <li><u>Armenian:</u> Hrant Ohanyan (hrantohanyan@mail.am)</li>\
          <li><u>Basque:</u> Xabier Aramendi (azpidatziak@gmail.com)</li>\
          <li><u>Belarusian:</u> Mihas Varantsou (meequz@gmail.com)</li>\
          <li><u>Brazilian:</u> Nick Marinho (nickmarinho@gmail.com)</li>\
          <li><u>Bulgarian:</u> Tsvetan & Boyko Bankoff (emerge_life@users.sourceforge.net)</li>\
          <li><u>Catalan:</u> Francisco Luque Contreras (frannoe@ya.com)</li>\
          <li><u>Chinese (Simplified):</u> Guo Yue (yue.guo0418@gmail.com)</li>\
          <li><u>Chinese (Traditional):</u> Yi-Shun Wang (dnextstep@gmail.com)</li>\
          <li><u>Croatian:</u> Oliver Mucafir (oliver.untwist@gmail.com)</li>\
          <li><u>Czech:</u> Jirka Vilim (web@tets.cz)</li>\
          <li><u>Danish:</u> Mathias Nielsen (comoneo@gmail.com)</li>\
          <li><u>Dutch:</u> Pieter Heyvaert (pieter_heyvaert@hotmail.com)</li>\
          <li><u>Finnish:</u> Niklas Laxström (nikerabbit@users.sourceforge.net) and Pekka Niemi (pekka.niemi@iki.fi)</li>\
          <li><u>Galician:</u> Marcos Lans (marcoslansgarza@gmail.com)</li>\
          <li><u>Georgian:</u> Beqa Arabuli (arabulibeqa@yahoo.com)</li>\
          <li><u>German:</u> Niels Hoffmann (zentralmaschine@users.sourceforge.net)</li>\
          <li><u>Greek:</u> Tsvetan Bankov (emerge_life@users.sourceforge.net)</li>\
          <li><u>Hebrew:</u> David Deutsch (d.deffo@gmail.com)</li>\
          <li><u>Hungarian:</u> Majoros Péter (majoros.peterj@gmail.com)</li>\
          <li><u>Italian:</u> bovirus (bovirus@live.it) and Matteo Sechi (bu17714@gmail.com)</li>\
          <li><u>Japanese:</u> Masato Hashimoto (cabezon.hashimoto@gmail.com)</li>\
          <li><u>Korean:</u> Jin Woo Sin (jin828sin@users.sourceforge.net)</li>\
          <li><u>Lithuanian:</u> Naglis Jonaitis (njonaitis@gmail.com)</li>\
          <li><u>Norwegian:</u> Tomaso</li>\
          <li><u>Polish:</u> Mariusz Fik (fisiu@opensuse.org)</li>\
          <li><u>Portuguese:</u> Nick Marinho (nickmarinho@gmail.com)</li>\
          <li><u>Romanian:</u> Obada Denis (obadadenis@users.sourceforge.net)</li>\
          <li><u>Russian:</u> Nick Khazov (m2k3d0n@users.sourceforge.net) and Alexey Morsov (samurai@ricom.ru)</li>\
          <li><u>Serbian:</u> Anaximandar Milet (anaximandar@operamail.com)</li>\
          <li><u>Slovak:</u>  helix84</li>\
          <li><u>Spanish:</u> Francisco Luque Contreras (frannoe@ya.com)</li>\
          <li><u>Swedish:</u> Daniel Nylander (po@danielnylander.se)</li>\
          <li><u>Turkish:</u> Hasan YILMAZ (iletisim@hedefturkce.com) and Erdem Bingöl (erdem84@gmail.com)</li>\
          <li><u>Ukrainian:</u> Oleh Prypin (blaxpirit@gmail.com)</li></ul>");
      trans_txt += "<p>"+tr("Please contact me if you would like to translate qBittorrent into your own language.")+"</p>";
      te_translation->setHtml(trans_txt);
      // License
      te_license->append(QString::fromUtf8("<a name='top'></a>"));
      QFile licensefile(":/gpl.html");
      if (licensefile.open(QIODevice::ReadOnly|QIODevice::Text)) {
        te_license->setHtml(licensefile.readAll());
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
