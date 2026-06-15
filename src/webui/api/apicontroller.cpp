/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2018-2026  Vladimir Golovnev <glassez@yandex.ru>
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

#include <utility>

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QMetaObject>
#include <QScopeGuard>
#include <QStringList>
#include <QUrl>

#include "base/utils/json.h"
#include "apierror.h"

using namespace Qt::Literals::StringLiterals;

APIController::APIController(IApplication *app, QObject *parent)
    : ApplicationComponent(app, parent)
{
}

APIResult APIController::run(const QString &action, const APIRequest &request)
{
    m_params = request.params;
    m_data = request.data;
    m_isJson = request.isJson;
    m_jsonBody = request.jsonBody;
    [[maybe_unused]] const auto inputsGuard = qScopeGuard([this]
    {
        m_params = {};
        m_data = {};
        m_isJson = false;
        m_jsonBody = {};
    });

    const QByteArray methodName = action.toLatin1() + "Action";
    if (!QMetaObject::invokeMethod(this, methodName.constData()))
        throw APIError(APIErrorType::NotFound, tr("Endpoint does not exist"));

    return std::exchange(m_result, {});
}

const StringMap &APIController::params() const
{
    return m_params;
}

const DataMap &APIController::data() const
{
    return m_data;
}

bool APIController::isJsonRequest() const
{
    return m_isJson;
}

void APIController::rejectArrayParam(const QString &key) const
{
    if (m_jsonBody.value(key).isArray())
        throw APIError(APIErrorType::BadParams, tr("Parameter '%1' must not be a JSON array").arg(key));
}

void APIController::requireParams(const QList<QString> &requiredParams) const
{
    QStringList missingParams;
    missingParams.reserve(requiredParams.size());

    for (const QString &requiredParam : requiredParams)
    {
        if (!params().contains(requiredParam))
            missingParams.append(requiredParam);
    }

    if (!missingParams.isEmpty())
        throw APIError(APIErrorType::BadParams, tr("Missing required parameters: %1").arg(missingParams.join(u", ")));
}

QStringList APIController::parseList(const QString &key, const QChar separator, const Qt::SplitBehavior behavior) const
{
    if (isJsonRequest())
    {
        const QJsonValue value = m_jsonBody.value(key);
        // undefined/null = not provided
        if (value.isUndefined() || value.isNull())
            return {};
        if (!value.isArray())
            throw APIError(APIErrorType::BadParams, tr("Parameter '%1' must be a JSON array").arg(key));

        const QJsonArray array = value.toArray();
        QStringList result;
        result.reserve(array.size());
        for (const QJsonValue &item : array)
        {
            if (!item.isString() && !item.isBool() && !item.isDouble())
                throw APIError(APIErrorType::BadParams, tr("List parameters must only contain strings, numbers or booleans"));
            result.append(Utils::Json::flattenValue(item));
        }
        if (behavior == Qt::SkipEmptyParts)
            result.removeIf([](const QString &element) { return element.isEmpty(); });
        return result;
    }

    return params().value(key).split(separator, behavior);
}

QStringList APIController::parseUrlList(const QString &key, const QChar separator, const Qt::SplitBehavior behavior) const
{
    QStringList urls = parseList(key, separator, behavior);
    for (QString &url : urls)
        url = decodeUrl(url);
    return urls;
}

QString APIController::decodeUrl(const QString &value) const
{
    return isJsonRequest() ? value : QUrl::fromPercentEncoding(value.toLatin1());
}

void APIController::setResult(const QString &result)
{
    m_result = RegularAPIResult {.data = result};
}

void APIController::setResult(const QJsonArray &result)
{
    m_result = RegularAPIResult {.data = QJsonDocument(result)};
}

void APIController::setResult(const QJsonObject &result)
{
    m_result = RegularAPIResult {.data = QJsonDocument(result)};
}

void APIController::setResult(const QByteArray &result, const QString &mimeType, const QString &filename)
{
    m_result = RegularAPIResult {.data = result, .mimeType = mimeType, .filename = filename};
}

void APIController::setResult(const Path &filePath)
{
    m_result = StreamFileAPIResult {.filePath = filePath};
}

void APIController::setStatus(const APIStatus status)
{
    Q_ASSERT(std::holds_alternative<RegularAPIResult>(m_result));

    std::get<RegularAPIResult>(m_result).status = status;
}
