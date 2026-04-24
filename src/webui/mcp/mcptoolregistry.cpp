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
    const QJsonObject MUTATE_IDEMPOTENT {
        {u"readOnlyHint"_s, false}, {u"destructiveHint"_s, false},
        {u"idempotentHint"_s, true}, {u"openWorldHint"_s, false}
    };
    const QJsonObject MUTATE_NON_IDEMPOTENT {
        {u"readOnlyHint"_s, false}, {u"destructiveHint"_s, false},
        {u"idempotentHint"_s, false}, {u"openWorldHint"_s, false}
    };
    const QJsonObject DESTRUCTIVE {
        {u"readOnlyHint"_s, false}, {u"destructiveHint"_s, true},
        {u"idempotentHint"_s, false}, {u"openWorldHint"_s, false}
    };
    const QJsonObject OPEN_WORLD {
        {u"readOnlyHint"_s, false}, {u"destructiveHint"_s, false},
        {u"idempotentHint"_s, false}, {u"openWorldHint"_s, true}
    };

    // Schema builder helper
    QJsonObject objSchema(const QJsonObject &properties, const QStringList &required = {},
                          bool additional = false)
    {
        QJsonObject s {
            {u"type"_s, u"object"_s},
            {u"properties"_s, properties},
            {u"additionalProperties"_s, additional}
        };
        if (!required.isEmpty())
            s.insert(u"required"_s, QJsonArray::fromStringList(required));
        return s;
    }

    const QJsonObject STRING_SCHEMA {{u"type"_s, u"string"_s}};
    const QJsonObject INT_SCHEMA {{u"type"_s, u"integer"_s}};
    const QJsonObject BOOL_SCHEMA {{u"type"_s, u"boolean"_s}};
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

    // ── app ──────────────────────────────────────────────────────────────────
    r.registerTool({
        .name = u"app_version"_s,
        .title = u"App version"_s,
        .description = u"Returns the qBittorrent application version."_s,
        .annotations = READ_ONLY,
        .inputSchema = objSchema({}),
        .handler = makeControllerHandler(app, u"app"_s, u"version"_s)
    });
    r.registerTool({
        .name = u"app_web_api_version"_s,
        .title = u"App Web API version"_s,
        .description = u"Returns the Web API version."_s,
        .annotations = READ_ONLY,
        .inputSchema = objSchema({}),
        .handler = makeControllerHandler(app, u"app"_s, u"webapiVersion"_s)
    });
    r.registerTool({
        .name = u"app_build_info"_s,
        .title = u"App build info"_s,
        .description = u"Returns build information for the application."_s,
        .annotations = READ_ONLY,
        .inputSchema = objSchema({}),
        .handler = makeControllerHandler(app, u"app"_s, u"buildInfo"_s)
    });
    r.registerTool({
        .name = u"app_process_info"_s,
        .title = u"App process info"_s,
        .description = u"Returns runtime process information (PID, memory, etc.)."_s,
        .annotations = READ_ONLY,
        .inputSchema = objSchema({}),
        .handler = makeControllerHandler(app, u"app"_s, u"processInfo"_s)
    });
    r.registerTool({
        .name = u"app_shutdown"_s,
        .title = u"App shutdown"_s,
        .description = u"Shuts down the qBittorrent application."_s,
        .annotations = DESTRUCTIVE,
        .inputSchema = objSchema({}),
        .handler = makeControllerHandler(app, u"app"_s, u"shutdown"_s)
    });
    r.registerTool({
        .name = u"app_preferences"_s,
        .title = u"App preferences"_s,
        .description = u"Returns all application preferences."_s,
        .annotations = READ_ONLY,
        .inputSchema = objSchema({}),
        .handler = makeControllerHandler(app, u"app"_s, u"preferences"_s)
    });
    r.registerTool({
        .name = u"app_set_preferences"_s,
        .title = u"App set preferences"_s,
        .description = u"Updates application preferences from a JSON string."_s,
        .annotations = MUTATE_IDEMPOTENT,
        .inputSchema = objSchema({{u"json"_s, STRING_SCHEMA}}, {u"json"_s}),
        .handler = makeControllerHandler(app, u"app"_s, u"setPreferences"_s)
    });
    r.registerTool({
        .name = u"app_default_save_path"_s,
        .title = u"App default save path"_s,
        .description = u"Returns the default torrent save path."_s,
        .annotations = READ_ONLY,
        .inputSchema = objSchema({}),
        .handler = makeControllerHandler(app, u"app"_s, u"defaultSavePath"_s)
    });
    r.registerTool({
        .name = u"app_send_test_email"_s,
        .title = u"App send test email"_s,
        .description = u"Sends a test email using the configured SMTP settings."_s,
        .annotations = OPEN_WORLD,
        .inputSchema = objSchema({}),
        .handler = makeControllerHandler(app, u"app"_s, u"sendTestEmail"_s)
    });
    r.registerTool({
        .name = u"app_get_directory_content"_s,
        .title = u"App get directory content"_s,
        .description = u"Lists the contents of a directory on the host filesystem."_s,
        .annotations = READ_ONLY,
        .inputSchema = objSchema({{u"dirPath"_s, STRING_SCHEMA}}, {u"dirPath"_s}),
        .handler = makeControllerHandler(app, u"app"_s, u"getDirectoryContent"_s)
    });
    r.registerTool({
        .name = u"app_get_free_space_at_path"_s,
        .title = u"App get free space at path"_s,
        .description = u"Returns the available free disk space at the given path."_s,
        .annotations = READ_ONLY,
        .inputSchema = objSchema({{u"path"_s, STRING_SCHEMA}}, {u"path"_s}),
        .handler = makeControllerHandler(app, u"app"_s, u"getFreeSpaceAtPath"_s)
    });
    r.registerTool({
        .name = u"app_cookies"_s,
        .title = u"App cookies"_s,
        .description = u"Returns the stored HTTP cookies."_s,
        .annotations = READ_ONLY,
        .inputSchema = objSchema({}),
        .handler = makeControllerHandler(app, u"app"_s, u"cookies"_s)
    });
    r.registerTool({
        .name = u"app_set_cookies"_s,
        .title = u"App set cookies"_s,
        .description = u"Replaces the stored HTTP cookies."_s,
        .annotations = MUTATE_IDEMPOTENT,
        .inputSchema = objSchema({{u"cookies"_s, STRING_SCHEMA}}),
        .handler = makeControllerHandler(app, u"app"_s, u"setCookies"_s)
    });
    r.registerTool({
        .name = u"app_rotate_api_key"_s,
        .title = u"App rotate API key"_s,
        .description = u"Generates a new API key, invalidating the current one."_s,
        .annotations = MUTATE_NON_IDEMPOTENT,
        .inputSchema = objSchema({}),
        .handler = makeControllerHandler(app, u"app"_s, u"rotateAPIKey"_s)
    });
    r.registerTool({
        .name = u"app_delete_api_key"_s,
        .title = u"App delete API key"_s,
        .description = u"Deletes the current API key."_s,
        .annotations = DESTRUCTIVE,
        .inputSchema = objSchema({}),
        .handler = makeControllerHandler(app, u"app"_s, u"deleteAPIKey"_s)
    });
    r.registerTool({
        .name = u"app_network_interface_list"_s,
        .title = u"App network interface list"_s,
        .description = u"Returns the list of available network interfaces."_s,
        .annotations = READ_ONLY,
        .inputSchema = objSchema({}),
        .handler = makeControllerHandler(app, u"app"_s, u"networkInterfaceList"_s)
    });
    r.registerTool({
        .name = u"app_network_interface_address_list"_s,
        .title = u"App network interface address list"_s,
        .description = u"Returns the addresses associated with a network interface."_s,
        .annotations = READ_ONLY,
        .inputSchema = objSchema({{u"iface"_s, STRING_SCHEMA}}),
        .handler = makeControllerHandler(app, u"app"_s, u"networkInterfaceAddressList"_s)
    });

    // ── transfer ─────────────────────────────────────────────────────────────
    r.registerTool({
        .name = u"transfer_info"_s,
        .title = u"Transfer info"_s,
        .description = u"Returns global transfer statistics (speeds, session totals)."_s,
        .annotations = READ_ONLY,
        .inputSchema = objSchema({}),
        .handler = makeControllerHandler(app, u"transfer"_s, u"info"_s)
    });
    r.registerTool({
        .name = u"transfer_speed_limits_mode"_s,
        .title = u"Transfer speed limits mode"_s,
        .description = u"Returns the current alternative speed-limits mode (0=normal, 1=alternative)."_s,
        .annotations = READ_ONLY,
        .inputSchema = objSchema({}),
        .handler = makeControllerHandler(app, u"transfer"_s, u"speedLimitsMode"_s)
    });
    r.registerTool({
        .name = u"transfer_set_speed_limits_mode"_s,
        .title = u"Transfer set speed limits mode"_s,
        .description = u"Sets the alternative speed-limits mode."_s,
        .annotations = MUTATE_IDEMPOTENT,
        .inputSchema = objSchema({{u"mode"_s, STRING_SCHEMA}}, {u"mode"_s}),
        .handler = makeControllerHandler(app, u"transfer"_s, u"setSpeedLimitsMode"_s)
    });
    r.registerTool({
        .name = u"transfer_toggle_speed_limits_mode"_s,
        .title = u"Transfer toggle speed limits mode"_s,
        .description = u"Toggles between normal and alternative speed-limits mode."_s,
        .annotations = MUTATE_NON_IDEMPOTENT,
        .inputSchema = objSchema({}),
        .handler = makeControllerHandler(app, u"transfer"_s, u"toggleSpeedLimitsMode"_s)
    });
    r.registerTool({
        .name = u"transfer_upload_limit"_s,
        .title = u"Transfer upload limit"_s,
        .description = u"Returns the global upload speed limit in bytes/s (0 = unlimited)."_s,
        .annotations = READ_ONLY,
        .inputSchema = objSchema({}),
        .handler = makeControllerHandler(app, u"transfer"_s, u"uploadLimit"_s)
    });
    r.registerTool({
        .name = u"transfer_download_limit"_s,
        .title = u"Transfer download limit"_s,
        .description = u"Returns the global download speed limit in bytes/s (0 = unlimited)."_s,
        .annotations = READ_ONLY,
        .inputSchema = objSchema({}),
        .handler = makeControllerHandler(app, u"transfer"_s, u"downloadLimit"_s)
    });
    r.registerTool({
        .name = u"transfer_set_upload_limit"_s,
        .title = u"Transfer set upload limit"_s,
        .description = u"Sets the global upload speed limit in bytes/s (0 = unlimited)."_s,
        .annotations = MUTATE_IDEMPOTENT,
        .inputSchema = objSchema({{u"limit"_s, INT_SCHEMA}}, {u"limit"_s}),
        .handler = makeControllerHandler(app, u"transfer"_s, u"setUploadLimit"_s)
    });
    r.registerTool({
        .name = u"transfer_set_download_limit"_s,
        .title = u"Transfer set download limit"_s,
        .description = u"Sets the global download speed limit in bytes/s (0 = unlimited)."_s,
        .annotations = MUTATE_IDEMPOTENT,
        .inputSchema = objSchema({{u"limit"_s, INT_SCHEMA}}, {u"limit"_s}),
        .handler = makeControllerHandler(app, u"transfer"_s, u"setDownloadLimit"_s)
    });
    r.registerTool({
        .name = u"transfer_ban_peers"_s,
        .title = u"Transfer ban peers"_s,
        .description = u"Bans one or more peers by IP (pipe-separated list)."_s,
        .annotations = DESTRUCTIVE,
        .inputSchema = objSchema({{u"peers"_s, STRING_SCHEMA}}, {u"peers"_s}),
        .handler = makeControllerHandler(app, u"transfer"_s, u"banPeers"_s)
    });

    // ── sync ─────────────────────────────────────────────────────────────────
    r.registerTool({
        .name = u"sync_maindata"_s,
        .title = u"Sync maindata"_s,
        .description = u"Returns a maindata sync response (full or incremental based on rid)."_s,
        .annotations = READ_ONLY,
        .inputSchema = objSchema({{u"rid"_s, INT_SCHEMA}}),
        .handler = makeControllerHandler(app, u"sync"_s, u"maindata"_s)
    });
    r.registerTool({
        .name = u"sync_torrent_peers"_s,
        .title = u"Sync torrent peers"_s,
        .description = u"Returns peer data for a specific torrent (full or incremental based on rid)."_s,
        .annotations = READ_ONLY,
        .inputSchema = objSchema({{u"hash"_s, STRING_SCHEMA}, {u"rid"_s, INT_SCHEMA}}, {u"hash"_s}),
        .handler = makeControllerHandler(app, u"sync"_s, u"torrentPeers"_s)
    });

    // ── log ──────────────────────────────────────────────────────────────────
    r.registerTool({
        .name = u"log_main"_s,
        .title = u"Log main"_s,
        .description = u"Returns application log entries, optionally filtered by severity."_s,
        .annotations = READ_ONLY,
        .inputSchema = objSchema({{u"normal"_s, BOOL_SCHEMA}, {u"info"_s, BOOL_SCHEMA},
                                   {u"warning"_s, BOOL_SCHEMA}, {u"critical"_s, BOOL_SCHEMA},
                                   {u"last_known_id"_s, INT_SCHEMA}}),
        .handler = makeControllerHandler(app, u"log"_s, u"main"_s)
    });
    r.registerTool({
        .name = u"log_peers"_s,
        .title = u"Log peers"_s,
        .description = u"Returns peer log entries since the given last_known_id."_s,
        .annotations = READ_ONLY,
        .inputSchema = objSchema({{u"last_known_id"_s, INT_SCHEMA}}),
        .handler = makeControllerHandler(app, u"log"_s, u"peers"_s)
    });
}
