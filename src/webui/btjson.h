/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2012, Christophe Dumez
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

#ifndef BTJSON_H
#define BTJSON_H

#include <QCoreApplication>
#include <QString>
#include <QVariant>

class btjson
{
    Q_DECLARE_TR_FUNCTIONS(misc)

private:
    btjson() {}

public:
    static QByteArray getTorrents(QString filter = "all", QString label = QString(),
        QString sortedColumn = "name", bool reverse = false, int limit = 0, int offset = 0);
    static QByteArray getSyncMainData(int acceptedResponseId, QVariantMap &lastData, QVariantMap &lastAcceptedData);
    static QByteArray getTrackersForTorrent(const QString& hash);
    static QByteArray getWebSeedsForTorrent(const QString& hash);
    static QByteArray getPropertiesForTorrent(const QString& hash);
    static QByteArray getFilesForTorrent(const QString& hash);
    static QByteArray getTransferInfo();
    static QByteArray getTorrentsRatesLimits(QStringList& hashes, bool downloadLimits);
}; // class btjson

#endif // BTJSON_H
