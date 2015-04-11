/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2014  Vladimir Golovnev <glassez@yandex.ru>
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

#ifndef JSONUTILS_H
#define JSONUTILS_H

#include <QVariant>
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#else
#include <QString>
#ifndef USE_SYSTEM_QJSON
#include "qjson/parser.h"
#include "qjson/serializer.h"
#else // USE_SYSTEM_QJSON
#include <qjson/parser.h>
#include <qjson/serializer.h>
#endif // USE_SYSTEM_QJSON
#endif

namespace json {

    inline QByteArray toJson(const QVariant& var)
    {
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
        return QJsonDocument::fromVariant(var).toJson(QJsonDocument::Compact);
#else
        QJson::Serializer serializer;
        serializer.setIndentMode(QJson::IndentCompact);
        return serializer.serialize(var);
#endif
    }

    inline QVariant fromJson(const QString& json)
    {
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
        return QJsonDocument::fromJson(json.toUtf8()).toVariant();
#else
        return QJson::Parser().parse(json.toUtf8());
#endif
    }

}

#endif // JSONUTILS_H
