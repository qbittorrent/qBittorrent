/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Ortes <malo.allee@gmail.com>
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

#include "mcptoolregistry.h"

#include <algorithm>
#include <memory>

#include <QJsonDocument>
#include <QMetaType>
#include <QVariant>

#include "webui/api/apierror.h"
#include "webui/api/appcontroller.h"
#include "webui/api/logcontroller.h"
#include "webui/api/rsscontroller.h"
#include "webui/api/searchcontroller.h"
#include "webui/api/synccontroller.h"
#include "webui/api/torrentscontroller.h"
#include "webui/api/transfercontroller.h"
#include "base/global.h"
#include "base/interfaces/iapplication.h"

namespace
{
    std::unique_ptr<APIController> createController(IApplication *app, const QString &scope)
    {
        if (scope == u"app"_s)            return std::make_unique<AppController>(app);
        if (scope == u"log"_s)            return std::make_unique<LogController>(app);
        if (scope == u"rss"_s)            return std::make_unique<RSSController>(app);
        if (scope == u"search"_s)         return std::make_unique<SearchController>(app);
        if (scope == u"sync"_s)           return std::make_unique<SyncController>(app);
        if (scope == u"torrents"_s)       return std::make_unique<TorrentsController>(app);
        if (scope == u"transfer"_s)       return std::make_unique<TransferController>(app);
        // auth, clientdata and torrentcreator need extra constructor args; skipped in v1.
        return nullptr;
    }

    QString jsonValueToString(const QJsonValue &v)
    {
        if (v.isString()) return v.toString();
        if (v.isBool())   return v.toBool() ? u"true"_s : u"false"_s;
        if (v.isDouble()) return QString::number(v.toDouble());
        // Fall back: re-serialize any nested object/array as JSON text
        return QString::fromUtf8(QJsonDocument::fromVariant(v.toVariant()).toJson(QJsonDocument::Compact));
    }

    MCP::ToolHandler makeControllerHandler(IApplication *app, const QString &scope, const QString &action)
    {
        return [app, scope, action](const QJsonObject &args) -> APIResult {
            StringMap params;
            for (auto it = args.constBegin(); it != args.constEnd(); ++it)
                params.insert(it.key(), jsonValueToString(it.value()));

            auto controller = createController(app, scope);
            if (!controller)
                throw APIError(APIErrorType::NotFound,
                    u"Tool scope not available: %1"_s.arg(scope));
            return controller->run(action, params);
        };
    }

    // Annotation presets — exposed as file-local constants
    const QJsonObject READ_ONLY {
        {u"readOnlyHint"_s, true}, {u"destructiveHint"_s, false},
        {u"idempotentHint"_s, true}, {u"openWorldHint"_s, false}
    };
}

MCP::ToolRegistry &MCP::ToolRegistry::instance()
{
    static ToolRegistry reg;
    return reg;
}

void MCP::ToolRegistry::registerTool(const ToolDescriptor &desc)
{
    m_tools.push_back(desc);
}

QJsonArray MCP::ToolRegistry::asCatalog() const
{
    QJsonArray out;
    for (const auto &t : m_tools)
    {
        QJsonObject obj {
            {u"name"_s, t.name},
            {u"title"_s, t.title},
            {u"description"_s, t.description},
            {u"inputSchema"_s, t.inputSchema},
            {u"annotations"_s, t.annotations}
        };
        if (!t.outputSchema.isEmpty())
            obj.insert(u"outputSchema"_s, t.outputSchema);
        out.append(obj);
    }
    return out;
}

const MCP::ToolDescriptor *MCP::ToolRegistry::find(const QString &name) const
{
    auto it = std::find_if(m_tools.begin(), m_tools.end(),
        [&](const ToolDescriptor &t) { return t.name == name; });
    return (it != m_tools.end()) ? &*it : nullptr;
}

void MCP::registerBuiltinTools(ToolRegistry &r, IApplication *app)
{
    // Guard against double-registration if WebApplication is reconstructed.
    if (!r.all().empty())
        return;

    r.registerTool({
        .name = u"app_version"_s,
        .title = u"App version"_s,
        .description = u"Returns the qBittorrent application version."_s,
        .annotations = READ_ONLY,
        .inputSchema = {
            {u"type"_s, u"object"_s},
            {u"properties"_s, QJsonObject{}},
            {u"additionalProperties"_s, false}
        },
        .outputSchema = {},
        .handler = makeControllerHandler(app, u"app"_s, u"version"_s)
    });
}
