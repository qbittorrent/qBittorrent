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

#include "apicontroller.h"

#include <algorithm>

#include <QHash>
#include <QJsonDocument>
#include <QList>
#include <QMetaObject>

#include "apierror.h"

void APIResult::clear()
{
    data.clear();
    mimeType.clear();
    filename.clear();
}

APIController::APIController(IApplication *app, QObject *parent)
    : ApplicationComponent(app, parent)
{
}

APIResult APIController::run(const QString &action, const StringMap &params, const DataMap &data)
{
    m_result.clear(); // clear result
    m_params = params;
    m_data = data;

    const QByteArray methodName = action.toLatin1() + "Action";
    if (!QMetaObject::invokeMethod(this, methodName.constData()))
        throw APIError(APIErrorType::NotFound);

    return m_result;
}

const StringMap &APIController::params() const
{
    return m_params;
}

const DataMap &APIController::data() const
{
    return m_data;
}

void APIController::requireParams(const QList<QString> &requiredParams) const
{
    const bool hasAllRequiredParams = std::all_of(requiredParams.cbegin(), requiredParams.cend()
        , [this](const QString &requiredParam)
    {
        return params().contains(requiredParam);
    });

    if (!hasAllRequiredParams)
        throw APIError(APIErrorType::BadParams);
}

void APIController::setResult(const QString &result)
{
    m_result.data = result;
}

void APIController::setResult(const QJsonArray &result)
{
    m_result.data = QJsonDocument(result);
}

void APIController::setResult(const QJsonObject &result)
{
    m_result.data = QJsonDocument(result);
}

void APIController::setResult(const QByteArray &result, const QString &mimeType, const QString &filename)
{
    m_result.data = result;
    m_result.mimeType = mimeType;
    m_result.filename = filename;
}
