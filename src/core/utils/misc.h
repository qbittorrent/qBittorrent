/*
 * Bittorrent Client using Qt and libtorrent.
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

#ifndef UTILS_MISC_H
#define UTILS_MISC_H

#include <vector>
#include <QString>
#include <QStringList>
#include <ctime>
#include <QPoint>
#include <QFile>
#include <QDir>
#include <QUrl>
#include "core/types.h"

/*  Miscellaneous functions that can be useful */

namespace Utils
{
    namespace Misc
    {
        QString parseHtmlLinks(const QString &raw_text);
        bool isUrl(const QString &s);

#ifndef DISABLE_GUI
        void shutdownComputer(ShutdownAction action);
        // Get screen center
        QPoint screenCenter(QWidget *win);
#endif
        int pythonVersion();
        // return best userfriendly storage unit (B, KiB, MiB, GiB, TiB)
        // use Binary prefix standards from IEC 60027-2
        // see http://en.wikipedia.org/wiki/Kilobyte
        // value must be given in bytes
        QString friendlyUnit(qreal val, bool is_speed = false);
        bool isPreviewable(const QString& extension);

        QString bcLinkToMagnet(QString bc_link);
        // Take a number of seconds and return an user-friendly
        // time duration like "1d 2h 10m".
        QString userFriendlyDuration(qlonglong seconds);
        QString getUserIDString();

        // Convert functions
        QStringList toStringList(const QList<bool> &l);
        QList<int> intListfromStringList(const QStringList &l);
        QList<bool> boolListfromStringList(const QStringList &l);

        void msleep(unsigned long msecs);
    }
}

#endif
