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

#include "filefilterrule.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

#include "base/global.h"

namespace BitTorrent
{
    qint64 FileFilterRule::convertToBytes(const qint64 size, const FileSizeUnit unit)
    {
        switch (unit)
        {
        case FileSizeUnit::Byte:
            return size;
        case FileSizeUnit::KiB:
            return size * 1024;
        case FileSizeUnit::MiB:
            return size * 1024 * 1024;
        case FileSizeUnit::GiB:
            return size * 1024 * 1024 * 1024;
        default:
            return size;
        }
    }

    qint64 FileFilterRule::convertFromBytes(const qint64 bytes, const FileSizeUnit unit)
    {
        switch (unit)
        {
        case FileSizeUnit::Byte:
            return bytes;
        case FileSizeUnit::KiB:
            return bytes / 1024;
        case FileSizeUnit::MiB:
            return bytes / (1024 * 1024);
        case FileSizeUnit::GiB:
            return bytes / (1024 * 1024 * 1024);
        default:
            return bytes;
        }
    }

    FileFilterRule parseFileFilterRule(const QJsonObject &jsonObj)
    {
        FileFilterRule rule;

        rule.name = jsonObj.value(u"name"_s).toString();
        rule.enabled = jsonObj.value(u"enabled"_s).toBool(false);

        const QJsonArray tagsArray = jsonObj.value(u"tags"_s).toArray();
        for (const QJsonValue &tagValue : tagsArray)
            rule.tags.insert(Tag(tagValue.toString()));

        rule.whitelistPattern = jsonObj.value(u"whitelist_pattern"_s).toString();
        rule.blacklistPattern = jsonObj.value(u"blacklist_pattern"_s).toString();
        rule.minFileSizeBytes = jsonObj.value(u"min_file_size_bytes"_s).toInteger(0);
        rule.maxFileSizeBytes = jsonObj.value(u"max_file_size_bytes"_s).toInteger(0);

        const int sizeUnitInt = jsonObj.value(u"size_unit"_s).toInt(static_cast<int>(FileSizeUnit::MiB));
        rule.sizeUnit = static_cast<FileSizeUnit>(sizeUnitInt);

        return rule;
    }

    QJsonObject serializeFileFilterRule(const FileFilterRule &rule)
    {
        QJsonObject obj;

        obj[u"name"_s] = rule.name;
        obj[u"enabled"_s] = rule.enabled;

        QJsonArray tagsArray;
        for (const Tag &tag : rule.tags)
            tagsArray.append(tag.toString());
        obj[u"tags"_s] = tagsArray;

        obj[u"whitelist_pattern"_s] = rule.whitelistPattern;
        obj[u"blacklist_pattern"_s] = rule.blacklistPattern;
        obj[u"min_file_size_bytes"_s] = rule.minFileSizeBytes;
        obj[u"max_file_size_bytes"_s] = rule.maxFileSizeBytes;
        obj[u"size_unit"_s] = static_cast<int>(rule.sizeUnit);

        return obj;
    }

    QList<FileFilterRule> parseFileFilterRules(const QJsonArray &jsonArray)
    {
        QList<FileFilterRule> rules;
        rules.reserve(jsonArray.size());

        for (const QJsonValue &value : jsonArray)
        {
            if (value.isObject())
                rules.append(parseFileFilterRule(value.toObject()));
        }

        return rules;
    }

    QJsonArray serializeFileFilterRules(const QList<FileFilterRule> &rules)
    {
        QJsonArray array;

        for (const FileFilterRule &rule : rules)
            array.append(serializeFileFilterRule(rule));

        return array;
    }
}
