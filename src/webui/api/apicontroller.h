/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2018-2024  Vladimir Golovnev <glassez@yandex.ru>
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

#pragma once

#include <QtContainerFwd>
#include <QObject>
#include <QString>
#include <QVariant>

#include "base/applicationcomponent.h"

using DataMap = QHash<QString, QByteArray>;
using StringMap = QHash<QString, QString>;

struct APIResult
{
    QVariant data;
    QString mimeType;
    QString filename;

    void clear();
};

class APIController : public ApplicationComponent<QObject>
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(APIController)

public:
    explicit APIController(IApplication *app, QObject *parent = nullptr);

    APIResult run(const QString &action, const StringMap &params, const DataMap &data = {});

protected:
    const StringMap &params() const;
    const DataMap &data() const;
    void requireParams(const QList<QString> &requiredParams) const;

    void setResult(const QString &result);
    void setResult(const QJsonArray &result);
    void setResult(const QJsonObject &result);
    void setResult(const QByteArray &result, const QString &mimeType = {}, const QString &filename = {});

private:
    StringMap m_params;
    DataMap m_data;
    APIResult m_result;
};
