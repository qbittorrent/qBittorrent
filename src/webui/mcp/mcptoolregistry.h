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

#pragma once

#include <functional>
#include <vector>

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include "webui/api/apicontroller.h"  // APIResult

class IApplication;

namespace MCP
{
    // Self-contained tool handler: receives JSON arguments, returns an APIResult.
    // Bound at registration time to whatever qBittorrent logic should run.
    // Keeping handlers as callables decouples the registry from MCPServer —
    // a future library swap consumes these directly.
    using ToolHandler = std::function<APIResult(const QJsonObject &arguments)>;

    struct ToolDescriptor
    {
        QString name;
        QString title;
        QString description;
        QJsonObject annotations;
        QJsonObject inputSchema;
        QJsonObject outputSchema;   // empty = not advertised
        ToolHandler handler;
    };

    class ToolRegistry
    {
    public:
        Q_DISABLE_COPY_MOVE(ToolRegistry)

        static ToolRegistry &instance();

        void registerTool(const ToolDescriptor &desc);

        /** @return JSON array of tool objects formatted for tools/list. */
        QJsonArray asCatalog() const;

        /** @return nullptr if no tool with that name. */
        const ToolDescriptor *find(const QString &name) const;

        const std::vector<ToolDescriptor> &all() const { return m_tools; }

    private:
        ToolRegistry() = default;
        std::vector<ToolDescriptor> m_tools;
    };

    /**
     * Populate the given registry with every built-in qBittorrent MCP tool.
     * Handlers bind to `app` via closure. Safe to call once per process.
     */
    void registerBuiltinTools(ToolRegistry &registry, IApplication *app);
}
