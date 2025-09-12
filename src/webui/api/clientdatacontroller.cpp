/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2025  Thomas Piccirello <thomas@piccirello.com>
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

#include "clientdatacontroller.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "base/global.h"
#include "base/interfaces/iapplication.h"
#include "base/logger.h"
#include "apierror.h"
#include "webui/clientdatastorage.h"

ClientDataController::ClientDataController(ClientDataStorage *clientDataStorage, IApplication *app, QObject *parent)
    : APIController(app, parent)
    , m_clientDataStorage {clientDataStorage}
{
}

void ClientDataController::loadAction()
{
    const QString keysParam {params()[u"keys"_s]};
    if (keysParam.isEmpty())
    {
        setResult(m_clientDataStorage->loadData());
        return;
    }

    QJsonParseError jsonError;
    const auto keysJsonDocument = QJsonDocument::fromJson(keysParam.toUtf8(), &jsonError);
    if (jsonError.error != QJsonParseError::NoError)
        throw APIError(APIErrorType::BadParams, jsonError.errorString());
    if (!keysJsonDocument.isArray())
        throw APIError(APIErrorType::BadParams, tr("`keys` must be an array"));

    QStringList keys;
    for (const QJsonValue &keysJsonVal : asConst(keysJsonDocument.array()))
    {
        if (!keysJsonVal.isString())
            throw APIError(APIErrorType::BadParams, tr("Items of `keys` must be strings"));

        keys << keysJsonVal.toString();
    }

    setResult(m_clientDataStorage->loadData(keys));
}

void ClientDataController::storeAction()
{
    requireParams({u"data"_s});
    QJsonParseError jsonError;
    const auto dataJsonDocument = QJsonDocument::fromJson(params()[u"data"_s].toUtf8(), &jsonError);
    if (jsonError.error != QJsonParseError::NoError)
        throw APIError(APIErrorType::BadParams, jsonError.errorString());
    if (!dataJsonDocument.isObject())
        throw APIError(APIErrorType::BadParams, tr("`data` must be an object"));

    const nonstd::expected<void, QString> result = m_clientDataStorage->storeData(dataJsonDocument.object());
    if (!result)
        throw APIError(APIErrorType::Conflict, result.error());
}
