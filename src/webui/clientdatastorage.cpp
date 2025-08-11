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

#include "clientdatastorage.h"

#include <QJsonDocument>
#include <QJsonObject>

#include "base/global.h"
#include "base/logger.h"
#include "base/path.h"
#include "base/profile.h"
#include "base/utils/io.h"

const int CLIENT_DATA_FILE_MAX_SIZE = 1024 * 1024; // 1 MiB
const QString CLIENT_DATA_FILE_NAME = u"web_clientdata.json"_s;

ClientDataStorage::ClientDataStorage(QObject *parent)
    : QObject(parent)
    , m_clientDataFilePath(specialFolderLocation(SpecialFolder::Data) / Path(CLIENT_DATA_FILE_NAME))
{
    if (!m_clientDataFilePath.exists())
        return;

    const auto readResult = Utils::IO::readFile(m_clientDataFilePath, CLIENT_DATA_FILE_MAX_SIZE);
    if (!readResult)
    {
        LogMsg(tr("Failed to load web client data. %1").arg(readResult.error().message), Log::WARNING);
        return;
    }

    QJsonParseError jsonError;
    const QJsonDocument jsonDoc = QJsonDocument::fromJson(readResult.value(), &jsonError);
    if (jsonError.error != QJsonParseError::NoError)
    {
        LogMsg(tr("Failed to parse web client data. File: \"%1\". Error: \"%2\"")
            .arg(m_clientDataFilePath.toString(), jsonError.errorString()), Log::WARNING);
        return;
    }

    if (!jsonDoc.isObject())
    {
        LogMsg(tr("Failed to load web client data. File: \"%1\". Error: \"Invalid data format\"")
            .arg(m_clientDataFilePath.toString()), Log::WARNING);
        return;
    }

    m_clientData = jsonDoc.object();
}

nonstd::expected<void, QString> ClientDataStorage::storeData(const QJsonObject &object)
{
    QJsonObject clientData = m_clientData;
    bool dataModified = false;
    for (auto it = object.constBegin(), end = object.constEnd(); it != end; ++it)
    {
        const QString &key = it.key();
        const QJsonValue &value = it.value();

        if (value.isNull())
        {
            if (auto it = clientData.find(key); it != clientData.end())
            {
                clientData.erase(it);
                dataModified = true;
            }
        }
        else
        {
            const auto &existingValue = clientData.find(key);
            if (existingValue == clientData.end())
            {
                clientData.insert(key, value);
                dataModified = true;
            }
            else if (existingValue.value() != value)
            {
                existingValue.value() = value;
                dataModified = true;
            }
        }
    }

    if (dataModified)
    {
        const QByteArray json = QJsonDocument(clientData).toJson(QJsonDocument::Compact);
        if (json.size() > CLIENT_DATA_FILE_MAX_SIZE)
            return nonstd::make_unexpected(tr("Total web client data must not be larger than %1 bytes").arg(CLIENT_DATA_FILE_MAX_SIZE));
        const nonstd::expected<void, QString> result = Utils::IO::saveToFile(m_clientDataFilePath, json);
        if (!result)
            return nonstd::make_unexpected(tr("Failed to save web client data. Error: \"%1\"").arg(result.error()));

        m_clientData = clientData;
    }

    return {};
}

QJsonObject ClientDataStorage::loadData() const
{
    return m_clientData;
}

QJsonObject ClientDataStorage::loadData(const QStringList &keys) const
{
    QJsonObject clientData;
    for (const QString &key : keys)
    {
        if (const auto iter = m_clientData.constFind(key); iter != m_clientData.constEnd())
            clientData.insert(key, iter.value());
    }
    return clientData;
}
