/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Tim Sylvester <t.j.sylvester@gmail.com>
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

#include "discoveryroots.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include "base/global.h"
#include "base/logger.h"
#include "base/profile.h"
#include "base/utils/io.h"

const QString CONF_FILE_NAME = u"discovery_roots.json"_s;

const QString OPTION_PATH = u"path"_s;
const QString OPTION_RECURSIVE = u"recursive"_s;

DiscoveryRoots *DiscoveryRoots::m_instance = nullptr;

void DiscoveryRoots::initInstance()
{
    if (!m_instance)
        m_instance = new DiscoveryRoots;
}

void DiscoveryRoots::freeInstance()
{
    delete m_instance;
    m_instance = nullptr;
}

DiscoveryRoots *DiscoveryRoots::instance()
{
    return m_instance;
}

DiscoveryRoots::DiscoveryRoots(QObject *parent)
    : QObject(parent)
{
    load();
}

QList<DiscoveryRoot> DiscoveryRoots::roots() const
{
    return m_roots;
}

void DiscoveryRoots::setRoots(const QList<DiscoveryRoot> &roots)
{
    if (roots == m_roots)
        return;

    m_roots = roots;
    store();

    emit rootsChanged();
}

void DiscoveryRoots::load()
{
    const int fileMaxSize = 10 * 1024 * 1024;
    const Path path = specialFolderLocation(SpecialFolder::Config) / Path(CONF_FILE_NAME);

    const auto readResult = Utils::IO::readFile(path, fileMaxSize);
    if (!readResult)
    {
        if (readResult.error().status == Utils::IO::ReadError::NotExist)
            return;

        LogMsg(tr("Failed to load Discovery Roots configuration. %1").arg(readResult.error().message), Log::WARNING);
        return;
    }

    QJsonParseError jsonError;
    const QJsonDocument jsonDoc = QJsonDocument::fromJson(readResult.value(), &jsonError);
    if (jsonError.error != QJsonParseError::NoError)
    {
        LogMsg(tr("Failed to parse Discovery Roots configuration from %1. Error: \"%2\"")
            .arg(path.toString(), jsonError.errorString()), Log::WARNING);
        return;
    }

    if (!jsonDoc.isArray())
    {
        LogMsg(tr("Failed to load Discovery Roots configuration from %1. Error: \"Invalid data format.\"")
            .arg(path.toString()), Log::WARNING);
        return;
    }

    m_roots = parseDiscoveryRoots(jsonDoc.array());
}

void DiscoveryRoots::store() const
{
    const Path path = specialFolderLocation(SpecialFolder::Config) / Path(CONF_FILE_NAME);
    const QByteArray data = QJsonDocument(serializeDiscoveryRoots(m_roots)).toJson();
    const nonstd::expected<void, QString> result = Utils::IO::saveToFile(path, data);
    if (!result)
    {
        LogMsg(tr("Couldn't store Discovery Roots configuration to %1. Error: %2")
            .arg(path.toString(), result.error()), Log::WARNING);
    }
}

QList<DiscoveryRoot> parseDiscoveryRoots(const QJsonArray &jsonArray)
{
    QList<DiscoveryRoot> roots;
    for (const QJsonValue &value : jsonArray)
    {
        if (!value.isObject())
            continue;

        const QJsonObject jsonObj = value.toObject();
        const Path path {jsonObj.value(OPTION_PATH).toString()};
        if (path.isEmpty() || path.isRelative())
            continue;

        bool duplicate = false;
        for (const DiscoveryRoot &root : asConst(roots))
        {
            if (root.path == path)
            {
                duplicate = true;
                break;
            }
        }
        if (duplicate)
            continue;

        roots.append(DiscoveryRoot {.path = path
                , .options = {.recursive = jsonObj.value(OPTION_RECURSIVE).toBool()}});
    }

    return roots;
}

QJsonArray serializeDiscoveryRoots(const QList<DiscoveryRoot> &roots)
{
    QJsonArray jsonArray;
    for (const DiscoveryRoot &root : roots)
    {
        jsonArray.append(QJsonObject {{OPTION_PATH, root.path.data()}
                , {OPTION_RECURSIVE, root.options.recursive}});
    }

    return jsonArray;
}
