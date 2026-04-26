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
    const QJsonObject HASHES_SCHEMA {
        {u"type"_s, u"string"_s},
        {u"description"_s, u"Torrent hashes, separated by '|', or 'all'."_s}
    };
    const QJsonObject HASH_SCHEMA {
        {u"type"_s, u"string"_s},
        {u"description"_s, u"Single torrent hash."_s}
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
        .inputSchema = objSchema({{u"dirPath"_s, STRING_SCHEMA}, {u"mode"_s, STRING_SCHEMA}, {u"withMetadata"_s, BOOL_SCHEMA}}, {u"dirPath"_s}),
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

    // ── torrents (read) ───────────────────────────────────────────────────────
    r.registerTool({
        .name = u"torrents_info"_s,
        .title = u"Torrents info"_s,
        .description = u"Returns a list of torrents with optional filtering and sorting."_s,
        .annotations = READ_ONLY,
        .inputSchema = objSchema({{u"filter"_s, STRING_SCHEMA}, {u"category"_s, STRING_SCHEMA}, {u"tag"_s, STRING_SCHEMA}, {u"sort"_s, STRING_SCHEMA}, {u"reverse"_s, BOOL_SCHEMA}, {u"limit"_s, INT_SCHEMA}, {u"offset"_s, INT_SCHEMA}, {u"hashes"_s, STRING_SCHEMA}, {u"private"_s, BOOL_SCHEMA}, {u"includeFiles"_s, BOOL_SCHEMA}, {u"includeTrackers"_s, BOOL_SCHEMA}}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"info"_s)
    });
    r.registerTool({
        .name = u"torrents_count"_s,
        .title = u"Torrents count"_s,
        .description = u"Returns the total number of torrents."_s,
        .annotations = READ_ONLY,
        .inputSchema = objSchema({}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"count"_s)
    });
    r.registerTool({
        .name = u"torrents_properties"_s,
        .title = u"Torrents properties"_s,
        .description = u"Returns detailed properties for a single torrent."_s,
        .annotations = READ_ONLY,
        .inputSchema = objSchema({{u"hash"_s, HASH_SCHEMA}}, {u"hash"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"properties"_s)
    });
    r.registerTool({
        .name = u"torrents_trackers"_s,
        .title = u"Torrents trackers"_s,
        .description = u"Returns the list of trackers for a torrent."_s,
        .annotations = READ_ONLY,
        .inputSchema = objSchema({{u"hash"_s, HASH_SCHEMA}}, {u"hash"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"trackers"_s)
    });
    r.registerTool({
        .name = u"torrents_webseeds"_s,
        .title = u"Torrents webseeds"_s,
        .description = u"Returns the list of web seeds for a torrent."_s,
        .annotations = READ_ONLY,
        .inputSchema = objSchema({{u"hash"_s, HASH_SCHEMA}}, {u"hash"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"webseeds"_s)
    });
    r.registerTool({
        .name = u"torrents_files"_s,
        .title = u"Torrents files"_s,
        .description = u"Returns the file list for a torrent, optionally filtered by indexes."_s,
        .annotations = READ_ONLY,
        .inputSchema = objSchema({{u"hash"_s, HASH_SCHEMA}, {u"indexes"_s, STRING_SCHEMA}}, {u"hash"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"files"_s)
    });
    r.registerTool({
        .name = u"torrents_piece_hashes"_s,
        .title = u"Torrents piece hashes"_s,
        .description = u"Returns the SHA-1 hash of each piece for a torrent."_s,
        .annotations = READ_ONLY,
        .inputSchema = objSchema({{u"hash"_s, HASH_SCHEMA}}, {u"hash"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"pieceHashes"_s)
    });
    r.registerTool({
        .name = u"torrents_piece_states"_s,
        .title = u"Torrents piece states"_s,
        .description = u"Returns the download state of each piece for a torrent."_s,
        .annotations = READ_ONLY,
        .inputSchema = objSchema({{u"hash"_s, HASH_SCHEMA}}, {u"hash"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"pieceStates"_s)
    });
    r.registerTool({
        .name = u"torrents_piece_availability"_s,
        .title = u"Torrents piece availability"_s,
        .description = u"Returns the availability (swarm copy count) of each piece."_s,
        .annotations = READ_ONLY,
        .inputSchema = objSchema({{u"hash"_s, HASH_SCHEMA}}, {u"hash"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"pieceAvailability"_s)
    });
    r.registerTool({
        .name = u"torrents_categories"_s,
        .title = u"Torrents categories"_s,
        .description = u"Returns all defined torrent categories."_s,
        .annotations = READ_ONLY,
        .inputSchema = objSchema({}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"categories"_s)
    });
    r.registerTool({
        .name = u"torrents_tags"_s,
        .title = u"Torrents tags"_s,
        .description = u"Returns all defined torrent tags."_s,
        .annotations = READ_ONLY,
        .inputSchema = objSchema({}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"tags"_s)
    });
    r.registerTool({
        .name = u"torrents_upload_limit"_s,
        .title = u"Torrents upload limit"_s,
        .description = u"Returns the upload speed limit for the specified torrents."_s,
        .annotations = READ_ONLY,
        .inputSchema = objSchema({{u"hashes"_s, HASHES_SCHEMA}}, {u"hashes"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"uploadLimit"_s)
    });
    r.registerTool({
        .name = u"torrents_download_limit"_s,
        .title = u"Torrents download limit"_s,
        .description = u"Returns the download speed limit for the specified torrents."_s,
        .annotations = READ_ONLY,
        .inputSchema = objSchema({{u"hashes"_s, HASHES_SCHEMA}}, {u"hashes"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"downloadLimit"_s)
    });
    r.registerTool({
        .name = u"torrents_ssl_parameters"_s,
        .title = u"Torrents SSL parameters"_s,
        .description = u"Returns SSL parameters (certificate, key, DH params) for a torrent."_s,
        .annotations = READ_ONLY,
        .inputSchema = objSchema({{u"hash"_s, HASH_SCHEMA}}, {u"hash"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"SSLParameters"_s)
    });
    r.registerTool({
        .name = u"torrents_export"_s,
        .title = u"Torrents export"_s,
        .description = u"Exports a torrent as a .torrent file (returned as a base64 resource blob)."_s,
        .annotations = READ_ONLY,
        .inputSchema = objSchema({{u"hash"_s, HASH_SCHEMA}}, {u"hash"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"export"_s)
    });

    // ── torrents (mutation) ───────────────────────────────────────────────────
    r.registerTool({
        .name = u"torrents_add"_s,
        .title = u"Torrents add"_s,
        .description = u"Adds one or more torrents by URL/magnet link."_s,
        .annotations = OPEN_WORLD,
        .inputSchema = objSchema({{u"urls"_s, STRING_SCHEMA}, {u"savepath"_s, STRING_SCHEMA}, {u"category"_s, STRING_SCHEMA}, {u"tags"_s, STRING_SCHEMA}, {u"rename"_s, STRING_SCHEMA}, {u"sequentialDownload"_s, STRING_SCHEMA}, {u"firstLastPiecePrio"_s, STRING_SCHEMA}, {u"stopped"_s, STRING_SCHEMA}, {u"autoTMM"_s, STRING_SCHEMA}, {u"upLimit"_s, INT_SCHEMA}, {u"dlLimit"_s, INT_SCHEMA}, {u"ratioLimit"_s, STRING_SCHEMA}, {u"seedingTimeLimit"_s, INT_SCHEMA}, {u"inactiveSeedingTimeLimit"_s, INT_SCHEMA}, {u"contentLayout"_s, STRING_SCHEMA}, {u"skip_checking"_s, STRING_SCHEMA}, {u"downloadPath"_s, STRING_SCHEMA}, {u"useDownloadPath"_s, BOOL_SCHEMA}, {u"forced"_s, BOOL_SCHEMA}, {u"addToTopOfQueue"_s, BOOL_SCHEMA}, {u"stopCondition"_s, STRING_SCHEMA}, {u"shareLimitAction"_s, STRING_SCHEMA}, {u"filePriorities"_s, STRING_SCHEMA}, {u"downloader"_s, STRING_SCHEMA}}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"add"_s)
    });
    r.registerTool({
        .name = u"torrents_delete"_s,
        .title = u"Torrents delete"_s,
        .description = u"Deletes one or more torrents, optionally removing their files."_s,
        .annotations = DESTRUCTIVE,
        .inputSchema = objSchema({{u"hashes"_s, HASHES_SCHEMA}, {u"deleteFiles"_s, BOOL_SCHEMA}}, {u"hashes"_s, u"deleteFiles"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"delete"_s)
    });
    r.registerTool({
        .name = u"torrents_start"_s,
        .title = u"Torrents start"_s,
        .description = u"Starts (resumes) the specified torrents."_s,
        .annotations = MUTATE_IDEMPOTENT,
        .inputSchema = objSchema({{u"hashes"_s, HASHES_SCHEMA}}, {u"hashes"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"start"_s)
    });
    r.registerTool({
        .name = u"torrents_stop"_s,
        .title = u"Torrents stop"_s,
        .description = u"Stops (pauses) the specified torrents."_s,
        .annotations = MUTATE_IDEMPOTENT,
        .inputSchema = objSchema({{u"hashes"_s, HASHES_SCHEMA}}, {u"hashes"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"stop"_s)
    });
    r.registerTool({
        .name = u"torrents_recheck"_s,
        .title = u"Torrents recheck"_s,
        .description = u"Forces a hash recheck of the specified torrents."_s,
        .annotations = MUTATE_IDEMPOTENT,
        .inputSchema = objSchema({{u"hashes"_s, HASHES_SCHEMA}}, {u"hashes"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"recheck"_s)
    });
    r.registerTool({
        .name = u"torrents_reannounce"_s,
        .title = u"Torrents reannounce"_s,
        .description = u"Forces a tracker reannounce for the specified torrents."_s,
        .annotations = MUTATE_NON_IDEMPOTENT,
        .inputSchema = objSchema({{u"hashes"_s, HASHES_SCHEMA}, {u"urls"_s, STRING_SCHEMA}}, {u"hashes"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"reannounce"_s)
    });
    r.registerTool({
        .name = u"torrents_rename"_s,
        .title = u"Torrents rename"_s,
        .description = u"Renames a torrent."_s,
        .annotations = MUTATE_IDEMPOTENT,
        .inputSchema = objSchema({{u"hash"_s, HASH_SCHEMA}, {u"name"_s, STRING_SCHEMA}}, {u"hash"_s, u"name"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"rename"_s)
    });
    r.registerTool({
        .name = u"torrents_set_comment"_s,
        .title = u"Torrents set comment"_s,
        .description = u"Sets the user-visible comment on a torrent."_s,
        .annotations = MUTATE_IDEMPOTENT,
        .inputSchema = objSchema({{u"hashes"_s, HASHES_SCHEMA}, {u"comment"_s, STRING_SCHEMA}}, {u"hashes"_s, u"comment"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"setComment"_s)
    });
    r.registerTool({
        .name = u"torrents_set_category"_s,
        .title = u"Torrents set category"_s,
        .description = u"Assigns a category to one or more torrents."_s,
        .annotations = MUTATE_IDEMPOTENT,
        .inputSchema = objSchema({{u"hashes"_s, HASHES_SCHEMA}, {u"category"_s, STRING_SCHEMA}}, {u"hashes"_s, u"category"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"setCategory"_s)
    });
    r.registerTool({
        .name = u"torrents_create_category"_s,
        .title = u"Torrents create category"_s,
        .description = u"Creates a new torrent category."_s,
        .annotations = MUTATE_IDEMPOTENT,
        .inputSchema = objSchema({{u"category"_s, STRING_SCHEMA}, {u"savePath"_s, STRING_SCHEMA}, {u"downloadPathEnabled"_s, BOOL_SCHEMA}, {u"downloadPath"_s, STRING_SCHEMA}}, {u"category"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"createCategory"_s)
    });
    r.registerTool({
        .name = u"torrents_edit_category"_s,
        .title = u"Torrents edit category"_s,
        .description = u"Edits an existing torrent category."_s,
        .annotations = MUTATE_IDEMPOTENT,
        .inputSchema = objSchema({{u"category"_s, STRING_SCHEMA}, {u"savePath"_s, STRING_SCHEMA}, {u"downloadPathEnabled"_s, BOOL_SCHEMA}, {u"downloadPath"_s, STRING_SCHEMA}}, {u"category"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"editCategory"_s)
    });
    r.registerTool({
        .name = u"torrents_remove_categories"_s,
        .title = u"Torrents remove categories"_s,
        .description = u"Removes one or more torrent categories (pipe-separated names)."_s,
        .annotations = DESTRUCTIVE,
        .inputSchema = objSchema({{u"categories"_s, STRING_SCHEMA}}, {u"categories"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"removeCategories"_s)
    });
    r.registerTool({
        .name = u"torrents_add_tags"_s,
        .title = u"Torrents add tags"_s,
        .description = u"Adds tags to one or more torrents."_s,
        .annotations = MUTATE_IDEMPOTENT,
        .inputSchema = objSchema({{u"hashes"_s, HASHES_SCHEMA}, {u"tags"_s, STRING_SCHEMA}}, {u"hashes"_s, u"tags"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"addTags"_s)
    });
    r.registerTool({
        .name = u"torrents_set_tags"_s,
        .title = u"Torrents set tags"_s,
        .description = u"Replaces all tags on one or more torrents."_s,
        .annotations = MUTATE_IDEMPOTENT,
        .inputSchema = objSchema({{u"hashes"_s, HASHES_SCHEMA}, {u"tags"_s, STRING_SCHEMA}}, {u"hashes"_s, u"tags"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"setTags"_s)
    });
    r.registerTool({
        .name = u"torrents_remove_tags"_s,
        .title = u"Torrents remove tags"_s,
        .description = u"Removes tags from one or more torrents."_s,
        .annotations = MUTATE_IDEMPOTENT,
        .inputSchema = objSchema({{u"hashes"_s, HASHES_SCHEMA}, {u"tags"_s, STRING_SCHEMA}}, {u"hashes"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"removeTags"_s)
    });
    r.registerTool({
        .name = u"torrents_create_tags"_s,
        .title = u"Torrents create tags"_s,
        .description = u"Creates one or more tags (pipe-separated)."_s,
        .annotations = MUTATE_IDEMPOTENT,
        .inputSchema = objSchema({{u"tags"_s, STRING_SCHEMA}}, {u"tags"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"createTags"_s)
    });
    r.registerTool({
        .name = u"torrents_delete_tags"_s,
        .title = u"Torrents delete tags"_s,
        .description = u"Deletes one or more tags (pipe-separated)."_s,
        .annotations = DESTRUCTIVE,
        .inputSchema = objSchema({{u"tags"_s, STRING_SCHEMA}}, {u"tags"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"deleteTags"_s)
    });
    r.registerTool({
        .name = u"torrents_add_trackers"_s,
        .title = u"Torrents add trackers"_s,
        .description = u"Adds tracker URLs to a torrent."_s,
        .annotations = OPEN_WORLD,
        .inputSchema = objSchema({{u"hash"_s, HASH_SCHEMA}, {u"urls"_s, STRING_SCHEMA}}, {u"hash"_s, u"urls"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"addTrackers"_s)
    });
    r.registerTool({
        .name = u"torrents_edit_tracker"_s,
        .title = u"Torrents edit tracker"_s,
        .description = u"Replaces a tracker URL on a torrent."_s,
        .annotations = MUTATE_IDEMPOTENT,
        .inputSchema = objSchema({{u"hash"_s, HASH_SCHEMA}, {u"url"_s, STRING_SCHEMA}, {u"newUrl"_s, STRING_SCHEMA}, {u"tier"_s, STRING_SCHEMA}}, {u"hash"_s, u"url"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"editTracker"_s)
    });
    r.registerTool({
        .name = u"torrents_remove_trackers"_s,
        .title = u"Torrents remove trackers"_s,
        .description = u"Removes tracker URLs from a torrent."_s,
        .annotations = DESTRUCTIVE,
        .inputSchema = objSchema({{u"hash"_s, HASH_SCHEMA}, {u"urls"_s, STRING_SCHEMA}}, {u"hash"_s, u"urls"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"removeTrackers"_s)
    });
    r.registerTool({
        .name = u"torrents_add_peers"_s,
        .title = u"Torrents add peers"_s,
        .description = u"Adds peers to one or more torrents."_s,
        .annotations = MUTATE_NON_IDEMPOTENT,
        .inputSchema = objSchema({{u"hashes"_s, HASHES_SCHEMA}, {u"peers"_s, STRING_SCHEMA}}, {u"hashes"_s, u"peers"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"addPeers"_s)
    });
    r.registerTool({
        .name = u"torrents_file_prio"_s,
        .title = u"Torrents file priority"_s,
        .description = u"Sets the download priority for one or more files within a torrent."_s,
        .annotations = MUTATE_IDEMPOTENT,
        .inputSchema = objSchema({{u"hash"_s, HASH_SCHEMA}, {u"id"_s, STRING_SCHEMA}, {u"priority"_s, INT_SCHEMA}}, {u"hash"_s, u"id"_s, u"priority"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"filePrio"_s)
    });
    r.registerTool({
        .name = u"torrents_set_upload_limit"_s,
        .title = u"Torrents set upload limit"_s,
        .description = u"Sets the upload speed limit for one or more torrents."_s,
        .annotations = MUTATE_IDEMPOTENT,
        .inputSchema = objSchema({{u"hashes"_s, HASHES_SCHEMA}, {u"limit"_s, INT_SCHEMA}}, {u"hashes"_s, u"limit"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"setUploadLimit"_s)
    });
    r.registerTool({
        .name = u"torrents_set_download_limit"_s,
        .title = u"Torrents set download limit"_s,
        .description = u"Sets the download speed limit for one or more torrents."_s,
        .annotations = MUTATE_IDEMPOTENT,
        .inputSchema = objSchema({{u"hashes"_s, HASHES_SCHEMA}, {u"limit"_s, INT_SCHEMA}}, {u"hashes"_s, u"limit"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"setDownloadLimit"_s)
    });
    r.registerTool({
        .name = u"torrents_set_share_limits"_s,
        .title = u"Torrents set share limits"_s,
        .description = u"Sets ratio and seeding time limits for one or more torrents."_s,
        .annotations = MUTATE_IDEMPOTENT,
        .inputSchema = objSchema({{u"hashes"_s, HASHES_SCHEMA}, {u"ratioLimit"_s, STRING_SCHEMA}, {u"seedingTimeLimit"_s, INT_SCHEMA}, {u"inactiveSeedingTimeLimit"_s, INT_SCHEMA}, {u"shareLimitAction"_s, STRING_SCHEMA}, {u"shareLimitsMode"_s, STRING_SCHEMA}}, {u"hashes"_s, u"ratioLimit"_s, u"seedingTimeLimit"_s, u"inactiveSeedingTimeLimit"_s, u"shareLimitAction"_s, u"shareLimitsMode"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"setShareLimits"_s)
    });
    r.registerTool({
        .name = u"torrents_increase_prio"_s,
        .title = u"Torrents increase priority"_s,
        .description = u"Increases the queue priority of the specified torrents."_s,
        .annotations = MUTATE_NON_IDEMPOTENT,
        .inputSchema = objSchema({{u"hashes"_s, HASHES_SCHEMA}}, {u"hashes"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"increasePrio"_s)
    });
    r.registerTool({
        .name = u"torrents_decrease_prio"_s,
        .title = u"Torrents decrease priority"_s,
        .description = u"Decreases the queue priority of the specified torrents."_s,
        .annotations = MUTATE_NON_IDEMPOTENT,
        .inputSchema = objSchema({{u"hashes"_s, HASHES_SCHEMA}}, {u"hashes"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"decreasePrio"_s)
    });
    r.registerTool({
        .name = u"torrents_top_prio"_s,
        .title = u"Torrents top priority"_s,
        .description = u"Moves the specified torrents to the top of the download queue."_s,
        .annotations = MUTATE_NON_IDEMPOTENT,
        .inputSchema = objSchema({{u"hashes"_s, HASHES_SCHEMA}}, {u"hashes"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"topPrio"_s)
    });
    r.registerTool({
        .name = u"torrents_bottom_prio"_s,
        .title = u"Torrents bottom priority"_s,
        .description = u"Moves the specified torrents to the bottom of the download queue."_s,
        .annotations = MUTATE_NON_IDEMPOTENT,
        .inputSchema = objSchema({{u"hashes"_s, HASHES_SCHEMA}}, {u"hashes"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"bottomPrio"_s)
    });
    r.registerTool({
        .name = u"torrents_set_location"_s,
        .title = u"Torrents set location"_s,
        .description = u"Sets the save location for one or more torrents."_s,
        .annotations = MUTATE_IDEMPOTENT,
        .inputSchema = objSchema({{u"hashes"_s, HASHES_SCHEMA}, {u"location"_s, STRING_SCHEMA}}, {u"hashes"_s, u"location"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"setLocation"_s)
    });
    r.registerTool({
        .name = u"torrents_set_save_path"_s,
        .title = u"Torrents set save path"_s,
        .description = u"Sets the save path for one or more torrents."_s,
        .annotations = MUTATE_IDEMPOTENT,
        .inputSchema = objSchema({{u"id"_s, HASHES_SCHEMA}, {u"path"_s, STRING_SCHEMA}}, {u"id"_s, u"path"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"setSavePath"_s)
    });
    r.registerTool({
        .name = u"torrents_set_download_path"_s,
        .title = u"Torrents set download path"_s,
        .description = u"Sets the incomplete download path for one or more torrents."_s,
        .annotations = MUTATE_IDEMPOTENT,
        .inputSchema = objSchema({{u"id"_s, HASHES_SCHEMA}, {u"path"_s, STRING_SCHEMA}}, {u"id"_s, u"path"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"setDownloadPath"_s)
    });
    r.registerTool({
        .name = u"torrents_set_auto_management"_s,
        .title = u"Torrents set auto management"_s,
        .description = u"Enables or disables automatic torrent management for one or more torrents."_s,
        .annotations = MUTATE_IDEMPOTENT,
        .inputSchema = objSchema({{u"hashes"_s, HASHES_SCHEMA}, {u"enable"_s, BOOL_SCHEMA}}, {u"hashes"_s, u"enable"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"setAutoManagement"_s)
    });
    r.registerTool({
        .name = u"torrents_set_super_seeding"_s,
        .title = u"Torrents set super seeding"_s,
        .description = u"Enables or disables super seeding mode for one or more torrents."_s,
        .annotations = MUTATE_IDEMPOTENT,
        .inputSchema = objSchema({{u"hashes"_s, HASHES_SCHEMA}, {u"value"_s, BOOL_SCHEMA}}, {u"hashes"_s, u"value"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"setSuperSeeding"_s)
    });
    r.registerTool({
        .name = u"torrents_set_force_start"_s,
        .title = u"Torrents set force start"_s,
        .description = u"Enables or disables force-start for one or more torrents."_s,
        .annotations = MUTATE_IDEMPOTENT,
        .inputSchema = objSchema({{u"hashes"_s, HASHES_SCHEMA}, {u"value"_s, BOOL_SCHEMA}}, {u"hashes"_s, u"value"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"setForceStart"_s)
    });
    r.registerTool({
        .name = u"torrents_toggle_sequential_download"_s,
        .title = u"Torrents toggle sequential download"_s,
        .description = u"Toggles sequential download mode for one or more torrents."_s,
        .annotations = MUTATE_NON_IDEMPOTENT,
        .inputSchema = objSchema({{u"hashes"_s, HASHES_SCHEMA}}, {u"hashes"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"toggleSequentialDownload"_s)
    });
    r.registerTool({
        .name = u"torrents_toggle_first_last_piece_prio"_s,
        .title = u"Torrents toggle first/last piece priority"_s,
        .description = u"Toggles first/last piece priority for one or more torrents."_s,
        .annotations = MUTATE_NON_IDEMPOTENT,
        .inputSchema = objSchema({{u"hashes"_s, HASHES_SCHEMA}}, {u"hashes"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"toggleFirstLastPiecePrio"_s)
    });
    r.registerTool({
        .name = u"torrents_rename_file"_s,
        .title = u"Torrents rename file"_s,
        .description = u"Renames a file within a torrent."_s,
        .annotations = MUTATE_IDEMPOTENT,
        .inputSchema = objSchema({{u"hash"_s, HASH_SCHEMA}, {u"oldPath"_s, STRING_SCHEMA}, {u"newPath"_s, STRING_SCHEMA}}, {u"hash"_s, u"oldPath"_s, u"newPath"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"renameFile"_s)
    });
    r.registerTool({
        .name = u"torrents_rename_folder"_s,
        .title = u"Torrents rename folder"_s,
        .description = u"Renames a folder within a torrent."_s,
        .annotations = MUTATE_IDEMPOTENT,
        .inputSchema = objSchema({{u"hash"_s, HASH_SCHEMA}, {u"oldPath"_s, STRING_SCHEMA}, {u"newPath"_s, STRING_SCHEMA}}, {u"hash"_s, u"oldPath"_s, u"newPath"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"renameFolder"_s)
    });
    r.registerTool({
        .name = u"torrents_set_ssl_parameters"_s,
        .title = u"Torrents set SSL parameters"_s,
        .description = u"Sets SSL certificate, private key, and DH params for a torrent."_s,
        .annotations = MUTATE_IDEMPOTENT,
        .inputSchema = objSchema({{u"hash"_s, HASH_SCHEMA}, {u"ssl_certificate"_s, STRING_SCHEMA}, {u"ssl_private_key"_s, STRING_SCHEMA}, {u"ssl_dh_params"_s, STRING_SCHEMA}}, {u"hash"_s, u"ssl_certificate"_s, u"ssl_private_key"_s, u"ssl_dh_params"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"setSSLParameters"_s)
    });
    r.registerTool({
        .name = u"torrents_fetch_metadata"_s,
        .title = u"Torrents fetch metadata"_s,
        .description = u"Fetches torrent metadata from a URL or magnet link (network I/O)."_s,
        .annotations = OPEN_WORLD,
        .inputSchema = objSchema({{u"source"_s, STRING_SCHEMA}, {u"downloader"_s, STRING_SCHEMA}}, {u"source"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"fetchMetadata"_s)
    });
    r.registerTool({
        .name = u"torrents_parse_metadata"_s,
        .title = u"Torrents parse metadata"_s,
        .description = u"Parses torrent metadata from a local .torrent file or magnet URI without network I/O."_s,
        .annotations = READ_ONLY,
        .inputSchema = objSchema({{u"source"_s, STRING_SCHEMA}}, {u"source"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"parseMetadata"_s)
    });
    r.registerTool({
        .name = u"torrents_save_metadata"_s,
        .title = u"Torrents save metadata"_s,
        .description = u"Saves previously fetched metadata to a .torrent file on disk."_s,
        .annotations = MUTATE_IDEMPOTENT,
        .inputSchema = objSchema({{u"source"_s, STRING_SCHEMA}}, {u"source"_s}),
        .handler = makeControllerHandler(app, u"torrents"_s, u"saveMetadata"_s)
    });

    // ── RSS ──────────────────────────────────────────────────────────────────
    r.registerTool({
        .name = u"rss_add_folder"_s,
        .title = u"RSS add folder"_s,
        .description = u"Creates a new RSS folder at the given path."_s,
        .annotations = MUTATE_IDEMPOTENT,
        .inputSchema = objSchema({{u"path"_s, STRING_SCHEMA}}, {u"path"_s}),
        .handler = makeControllerHandler(app, u"rss"_s, u"addFolder"_s)
    });
    r.registerTool({
        .name = u"rss_add_feed"_s,
        .title = u"RSS add feed"_s,
        .description = u"Subscribes to an RSS feed by URL, optionally placing it at a folder path."_s,
        .annotations = OPEN_WORLD,
        .inputSchema = objSchema({{u"url"_s, STRING_SCHEMA}, {u"path"_s, STRING_SCHEMA}, {u"refreshInterval"_s, INT_SCHEMA}}, {u"url"_s, u"path"_s}),
        .handler = makeControllerHandler(app, u"rss"_s, u"addFeed"_s)
    });
    r.registerTool({
        .name = u"rss_set_feed_url"_s,
        .title = u"RSS set feed URL"_s,
        .description = u"Changes the URL of an existing RSS feed."_s,
        .annotations = MUTATE_IDEMPOTENT,
        .inputSchema = objSchema({{u"path"_s, STRING_SCHEMA}, {u"url"_s, STRING_SCHEMA}}, {u"path"_s, u"url"_s}),
        .handler = makeControllerHandler(app, u"rss"_s, u"setFeedURL"_s)
    });
    r.registerTool({
        .name = u"rss_set_feed_refresh_interval"_s,
        .title = u"RSS set feed refresh interval"_s,
        .description = u"Sets the refresh interval (in minutes) for an RSS feed or folder."_s,
        .annotations = MUTATE_IDEMPOTENT,
        .inputSchema = objSchema({{u"itemPath"_s, STRING_SCHEMA}, {u"refreshInterval"_s, INT_SCHEMA}}, {u"itemPath"_s, u"refreshInterval"_s}),
        .handler = makeControllerHandler(app, u"rss"_s, u"setFeedRefreshInterval"_s)
    });
    r.registerTool({
        .name = u"rss_remove_item"_s,
        .title = u"RSS remove item"_s,
        .description = u"Removes an RSS feed or folder at the given path."_s,
        .annotations = DESTRUCTIVE,
        .inputSchema = objSchema({{u"path"_s, STRING_SCHEMA}}, {u"path"_s}),
        .handler = makeControllerHandler(app, u"rss"_s, u"removeItem"_s)
    });
    r.registerTool({
        .name = u"rss_move_item"_s,
        .title = u"RSS move item"_s,
        .description = u"Moves or renames an RSS feed or folder to a new path."_s,
        .annotations = MUTATE_IDEMPOTENT,
        .inputSchema = objSchema({{u"itemPath"_s, STRING_SCHEMA}, {u"destPath"_s, STRING_SCHEMA}}, {u"itemPath"_s, u"destPath"_s}),
        .handler = makeControllerHandler(app, u"rss"_s, u"moveItem"_s)
    });
    r.registerTool({
        .name = u"rss_items"_s,
        .title = u"RSS items"_s,
        .description = u"Returns the RSS tree (feeds and folders). Pass withData=true to include article data."_s,
        .annotations = READ_ONLY,
        .inputSchema = objSchema({{u"withData"_s, BOOL_SCHEMA}}),
        .handler = makeControllerHandler(app, u"rss"_s, u"items"_s)
    });
    r.registerTool({
        .name = u"rss_mark_as_read"_s,
        .title = u"RSS mark as read"_s,
        .description = u"Marks an RSS feed item (or a specific article) as read."_s,
        .annotations = MUTATE_IDEMPOTENT,
        .inputSchema = objSchema({{u"itemPath"_s, STRING_SCHEMA}, {u"articleId"_s, STRING_SCHEMA}}, {u"itemPath"_s}),
        .handler = makeControllerHandler(app, u"rss"_s, u"markAsRead"_s)
    });
    r.registerTool({
        .name = u"rss_refresh_item"_s,
        .title = u"RSS refresh item"_s,
        .description = u"Forces an immediate refresh of an RSS feed or folder (network I/O)."_s,
        .annotations = OPEN_WORLD,
        .inputSchema = objSchema({{u"itemPath"_s, STRING_SCHEMA}}, {u"itemPath"_s}),
        .handler = makeControllerHandler(app, u"rss"_s, u"refreshItem"_s)
    });
    r.registerTool({
        .name = u"rss_set_rule"_s,
        .title = u"RSS set rule"_s,
        .description = u"Creates or updates an auto-download rule for RSS feeds."_s,
        .annotations = MUTATE_IDEMPOTENT,
        .inputSchema = objSchema({{u"ruleName"_s, STRING_SCHEMA}, {u"ruleDef"_s, STRING_SCHEMA}}, {u"ruleName"_s, u"ruleDef"_s}),
        .handler = makeControllerHandler(app, u"rss"_s, u"setRule"_s)
    });
    r.registerTool({
        .name = u"rss_rename_rule"_s,
        .title = u"RSS rename rule"_s,
        .description = u"Renames an existing RSS auto-download rule."_s,
        .annotations = MUTATE_IDEMPOTENT,
        .inputSchema = objSchema({{u"ruleName"_s, STRING_SCHEMA}, {u"newRuleName"_s, STRING_SCHEMA}}, {u"ruleName"_s, u"newRuleName"_s}),
        .handler = makeControllerHandler(app, u"rss"_s, u"renameRule"_s)
    });
    r.registerTool({
        .name = u"rss_remove_rule"_s,
        .title = u"RSS remove rule"_s,
        .description = u"Deletes an RSS auto-download rule by name."_s,
        .annotations = DESTRUCTIVE,
        .inputSchema = objSchema({{u"ruleName"_s, STRING_SCHEMA}}, {u"ruleName"_s}),
        .handler = makeControllerHandler(app, u"rss"_s, u"removeRule"_s)
    });
    r.registerTool({
        .name = u"rss_rules"_s,
        .title = u"RSS rules"_s,
        .description = u"Returns all RSS auto-download rules."_s,
        .annotations = READ_ONLY,
        .inputSchema = objSchema({}),
        .handler = makeControllerHandler(app, u"rss"_s, u"rules"_s)
    });
    r.registerTool({
        .name = u"rss_matching_articles"_s,
        .title = u"RSS matching articles"_s,
        .description = u"Returns articles that would be matched by a given RSS auto-download rule."_s,
        .annotations = READ_ONLY,
        .inputSchema = objSchema({{u"ruleName"_s, STRING_SCHEMA}}, {u"ruleName"_s}),
        .handler = makeControllerHandler(app, u"rss"_s, u"matchingArticles"_s)
    });

    // ── Search ───────────────────────────────────────────────────────────────
    r.registerTool({
        .name = u"search_start"_s,
        .title = u"Search start"_s,
        .description = u"Starts a torrent search job with the given pattern, plugins, and category."_s,
        .annotations = OPEN_WORLD,
        .inputSchema = objSchema({{u"pattern"_s, STRING_SCHEMA}, {u"plugins"_s, STRING_SCHEMA}, {u"category"_s, STRING_SCHEMA}}, {u"pattern"_s, u"plugins"_s, u"category"_s}),
        .handler = makeControllerHandler(app, u"search"_s, u"start"_s)
    });
    r.registerTool({
        .name = u"search_stop"_s,
        .title = u"Search stop"_s,
        .description = u"Stops a running search job by its ID."_s,
        .annotations = MUTATE_NON_IDEMPOTENT,
        .inputSchema = objSchema({{u"id"_s, INT_SCHEMA}}, {u"id"_s}),
        .handler = makeControllerHandler(app, u"search"_s, u"stop"_s)
    });
    r.registerTool({
        .name = u"search_status"_s,
        .title = u"Search status"_s,
        .description = u"Returns the status of a search job (or all jobs if no ID is given)."_s,
        .annotations = READ_ONLY,
        .inputSchema = objSchema({{u"id"_s, INT_SCHEMA}}),
        .handler = makeControllerHandler(app, u"search"_s, u"status"_s)
    });
    r.registerTool({
        .name = u"search_results"_s,
        .title = u"Search results"_s,
        .description = u"Returns the results of a search job, with optional limit and offset for pagination."_s,
        .annotations = READ_ONLY,
        .inputSchema = objSchema({{u"id"_s, INT_SCHEMA}, {u"limit"_s, INT_SCHEMA}, {u"offset"_s, INT_SCHEMA}}, {u"id"_s}),
        .handler = makeControllerHandler(app, u"search"_s, u"results"_s)
    });
    r.registerTool({
        .name = u"search_delete"_s,
        .title = u"Search delete"_s,
        .description = u"Deletes a search job and its results by ID."_s,
        .annotations = DESTRUCTIVE,
        .inputSchema = objSchema({{u"id"_s, INT_SCHEMA}}, {u"id"_s}),
        .handler = makeControllerHandler(app, u"search"_s, u"delete"_s)
    });
    r.registerTool({
        .name = u"search_download_torrent"_s,
        .title = u"Search download torrent"_s,
        .description = u"Downloads a torrent from a URL returned by a search result."_s,
        .annotations = OPEN_WORLD,
        .inputSchema = objSchema({{u"torrentUrl"_s, STRING_SCHEMA}, {u"pluginName"_s, STRING_SCHEMA}}, {u"torrentUrl"_s, u"pluginName"_s}),
        .handler = makeControllerHandler(app, u"search"_s, u"downloadTorrent"_s)
    });
    r.registerTool({
        .name = u"search_plugins"_s,
        .title = u"Search plugins"_s,
        .description = u"Returns a list of installed search plugins and their status."_s,
        .annotations = READ_ONLY,
        .inputSchema = objSchema({}),
        .handler = makeControllerHandler(app, u"search"_s, u"plugins"_s)
    });
    r.registerTool({
        .name = u"search_install_plugin"_s,
        .title = u"Search install plugin"_s,
        .description = u"Installs one or more search plugins from the given sources (URLs or local paths)."_s,
        .annotations = OPEN_WORLD,
        .inputSchema = objSchema({{u"sources"_s, STRING_SCHEMA}}, {u"sources"_s}),
        .handler = makeControllerHandler(app, u"search"_s, u"installPlugin"_s)
    });
    r.registerTool({
        .name = u"search_uninstall_plugin"_s,
        .title = u"Search uninstall plugin"_s,
        .description = u"Uninstalls one or more search plugins by name."_s,
        .annotations = DESTRUCTIVE,
        .inputSchema = objSchema({{u"names"_s, STRING_SCHEMA}}, {u"names"_s}),
        .handler = makeControllerHandler(app, u"search"_s, u"uninstallPlugin"_s)
    });
    r.registerTool({
        .name = u"search_enable_plugin"_s,
        .title = u"Search enable plugin"_s,
        .description = u"Enables or disables one or more search plugins by name."_s,
        .annotations = MUTATE_IDEMPOTENT,
        .inputSchema = objSchema({{u"names"_s, STRING_SCHEMA}, {u"enable"_s, BOOL_SCHEMA}}, {u"names"_s, u"enable"_s}),
        .handler = makeControllerHandler(app, u"search"_s, u"enablePlugin"_s)
    });
    r.registerTool({
        .name = u"search_update_plugins"_s,
        .title = u"Search update plugins"_s,
        .description = u"Triggers an update check for all installed search plugins (network I/O)."_s,
        .annotations = OPEN_WORLD,
        .inputSchema = objSchema({}),
        .handler = makeControllerHandler(app, u"search"_s, u"updatePlugins"_s)
    });
}
