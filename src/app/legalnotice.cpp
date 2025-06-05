/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2023  Mike Tzou (Chocobo1)
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

#include "legalnotice.h"

#ifdef DISABLE_GUI
#include <cstdio>
#endif // DISABLE_GUI

#include <QCoreApplication>
#include <QString>

#ifndef DISABLE_GUI
#include <QMessageBox>
#endif // DISABLE_GUI

#include "base/global.h"

#ifndef DISABLE_GUI
#include "gui/utils.h"
#endif // DISABLE_GUI

void showLegalNotice(const bool isInteractive)
{
    const QString noticeTitle = QCoreApplication::translate("LegalNotice", "Legal Notice");
    const QString noticeBody = QCoreApplication::translate("LegalNotice", "qBittorrent is a file sharing program. When you run a torrent, its data will be made available to others by means of upload. Any content you share is your sole responsibility.");
    const QString noticeEnd = QCoreApplication::translate("LegalNotice", "No further notices will be issued.");

    if (!isInteractive)
    {
        const QString legalNotice = u"\n*** %1 ***\n"_s.arg(noticeTitle)
            + noticeBody + u"\n\n"
            + QCoreApplication::translate("LegalNotice", "If you have read the legal notice, you can use command line option `--confirm-legal-notice` to suppress this message.");
        printf("%s\n\n", qUtf8Printable(legalNotice));
        return;
    }

#ifdef DISABLE_GUI
    const QString legalNotice = u"\n*** %1 ***\n"_s.arg(noticeTitle)
        + noticeBody + u"\n\n"
        + noticeEnd + u"\n\n"
        + QCoreApplication::translate("LegalNotice", "Press 'Enter' key to continue...");
    printf("%s", qUtf8Printable(legalNotice));
    getchar();
#else // DISABLE_GUI
    const QString messageBody = noticeBody + u"\n\n" + noticeEnd;
    QMessageBox msgBox {QMessageBox::NoIcon, noticeTitle, messageBody, QMessageBox::Ok};
    msgBox.show();  // Need to be shown first or moveToCenter does not work
    msgBox.move(Utils::Gui::screenCenter(&msgBox));
    msgBox.exec();
#endif // DISABLE_GUI
}
