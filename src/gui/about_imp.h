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
#include "base/utils/misc.h"
#include "base/unicodestrings.h"

class about: public QDialog, private Ui::AboutDlg
{
    Q_OBJECT

public:
    about(QWidget *parent) : QDialog(parent)
    {
        setupUi(this);
        setAttribute(Qt::WA_DeleteOnClose);

        // Title
        lb_name->setText("<b><h2>qBittorrent " VERSION "</h2></b>");

        // About
        QString aboutText = QString(
            "<p style=\"white-space: pre-wrap;\">"
            "%1\n\n"
            "%2\n\n"
            "<table>"
            "<tr><td>%3</td><td><a href=\"http://www.qbittorrent.org\">http://www.qbittorrent.org</a></td></tr>"
            "<tr><td>%4</td><td><a href=\"http://forum.qbittorrent.org\">http://forum.qbittorrent.org</a></td></tr>"
            "<tr><td>%5</td><td><a href=\"http://bugs.qbittorrent.org\">http://bugs.qbittorrent.org</a></td></tr>"
            "</table>"
            "</p>")
            .arg(tr("An advanced BitTorrent client programmed in C++, based on Qt toolkit and libtorrent-rasterbar."))
            .arg(tr("Copyright %1 2006-2016 The qBittorrent project").arg(QString::fromUtf8(C_COPYRIGHT)))
            .arg(tr("Home Page:"))
            .arg(tr("Forum:"))
            .arg(tr("Bug Tracker:"));
        lb_about->setText(aboutText);

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
        label_12->setText(Utils::Misc::libtorrentVersionString());
        label_13->setText(Utils::Misc::boostVersionString());

        show();
    }
};

#endif
