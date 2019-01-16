/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
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

#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include <QFile>

#include "base/unicodestrings.h"
#include "base/utils/misc.h"
#include "ui_aboutdialog.h"
#include "utils.h"

class AboutDialog : public QDialog, private Ui::AboutDialog
{
    Q_OBJECT

public:
    AboutDialog(QWidget *parent)
        : QDialog(parent)
    {
        setupUi(this);
        setAttribute(Qt::WA_DeleteOnClose);

        // Title
#if defined(__x86_64__) || defined(_M_X64)
        labelName->setText("<b><h2>qBittorrent " QBT_VERSION " (64-bit)</h2></b>");
#else
        labelName->setText("<b><h2>qBittorrent " QBT_VERSION " (32-bit)</h2></b>");
#endif

        logo->setPixmap(Utils::Gui::scaledPixmapSvg(":/icons/skin/qbittorrent-tray.svg", this, 32));

        // About
        QString aboutText = QString(
            "<p style=\"white-space: pre-wrap;\">"
            "%1\n\n"
            "%2\n\n"
            "<table>"
            "<tr><td>%3</td><td><a href=\"https://www.qbittorrent.org\">https://www.qbittorrent.org</a></td></tr>"
            "<tr><td>%4</td><td><a href=\"http://forum.qbittorrent.org\">http://forum.qbittorrent.org</a></td></tr>"
            "<tr><td>%5</td><td><a href=\"http://bugs.qbittorrent.org\">http://bugs.qbittorrent.org</a></td></tr>"
            "</table>"
            "</p>")
            .arg(tr("An advanced BitTorrent client programmed in C++, based on Qt toolkit and libtorrent-rasterbar.")
                , tr("Copyright %1 2006-2019 The qBittorrent project").arg(QString::fromUtf8(C_COPYRIGHT))
                , tr("Home Page:")
                , tr("Forum:")
                , tr("Bug Tracker:"));
        labelAbout->setText(aboutText);

        labelMascot->setPixmap(Utils::Gui::scaledPixmap(":/icons/skin/mascot.png", this));

        // Thanks
        QFile thanksfile(":/thanks.html");
        if (thanksfile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            textBrowserThanks->setHtml(QString::fromUtf8(thanksfile.readAll().constData()));
            thanksfile.close();
        }

        // Translation
        QFile translatorsfile(":/translators.html");
        if (translatorsfile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            textBrowserTranslation->setHtml(QString::fromUtf8(translatorsfile.readAll().constData()));
            translatorsfile.close();
        }

        // License
        QFile licensefile(":/gpl.html");
        if (licensefile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            textBrowserLicense->setHtml(QString::fromUtf8(licensefile.readAll().constData()));
            licensefile.close();
        }

        // Libraries
        labelQtVer->setText(QT_VERSION_STR);
        labelLibtVer->setText(Utils::Misc::libtorrentVersionString());
        labelBoostVer->setText(Utils::Misc::boostVersionString());

        Utils::Gui::resize(this);
        show();
    }
};

#endif // ABOUTDIALOG_H
