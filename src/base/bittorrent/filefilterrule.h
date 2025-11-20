/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2024  qBittorrent project
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
 */

#pragma once

#include <QJsonObject>
#include <QSet>
#include <QString>
#include <QStringList>

#include "base/tagset.h"

namespace BitTorrent
{
    enum class FileSizeUnit
    {
        Byte,
        KiB,
        MiB,
        GiB
    };

    struct FileFilterRule
    {
        QString name;                    // Rule name for identification
        bool enabled = false;            // Whether this rule is enabled
        TagSet tags;                     // Apply rule only to torrents with these tags (empty = all torrents)
        QString whitelistPattern;        // Regex pattern for whitelist (matched files are kept)
        QString blacklistPattern;        // Regex pattern for blacklist (matched files are excluded)
        qint64 minFileSizeBytes = 0;     // Minimum file size (files smaller are excluded, 0 = no limit)
        qint64 maxFileSizeBytes = 0;     // Maximum file size (files larger are excluded, 0 = no limit)
        FileSizeUnit sizeUnit = FileSizeUnit::MiB;  // Unit for display purposes

        // Helper to convert size with unit to bytes
        static qint64 convertToBytes(qint64 size, FileSizeUnit unit);
        static qint64 convertFromBytes(qint64 bytes, FileSizeUnit unit);

        friend bool operator==(const FileFilterRule &lhs, const FileFilterRule &rhs) = default;
    };

    // Serialization functions
    FileFilterRule parseFileFilterRule(const QJsonObject &jsonObj);
    QJsonObject serializeFileFilterRule(const FileFilterRule &rule);

    QList<FileFilterRule> parseFileFilterRules(const QJsonArray &jsonArray);
    QJsonArray serializeFileFilterRules(const QList<FileFilterRule> &rules);
}
