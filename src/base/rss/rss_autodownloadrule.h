/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017-2023  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2010  Christophe Dumez <chris@qbittorrent.org>
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

#include <optional>

#include <QSharedDataPointer>
#include <QVariant>

#include "base/global.h"
#include "base/bittorrent/addtorrentparams.h"
#include "base/pathfwd.h"

class QDateTime;
class QJsonObject;
class QRegularExpression;

namespace RSS
{
    struct AutoDownloadRuleData;

    class AutoDownloadRule
    {
    public:
        explicit AutoDownloadRule(const QString &name = {});
        AutoDownloadRule(const AutoDownloadRule &other);
        ~AutoDownloadRule();

        AutoDownloadRule &operator=(const AutoDownloadRule &other);

        QString name() const;
        void setName(const QString &name);

        bool isEnabled() const;
        void setEnabled(bool enable);

        int priority() const;
        void setPriority(int value);

        QString mustContain() const;
        void setMustContain(const QString &tokens);
        QString mustNotContain() const;
        void setMustNotContain(const QString &tokens);
        QStringList feedURLs() const;
        void setFeedURLs(const QStringList &urls);
        int ignoreDays() const;
        void setIgnoreDays(int d);
        QDateTime lastMatch() const;
        void setLastMatch(const QDateTime &lastMatch);
        bool useRegex() const;
        void setUseRegex(bool enabled);
        bool useSmartFilter() const;
        void setUseSmartFilter(bool enabled);
        QString episodeFilter() const;
        void setEpisodeFilter(const QString &e);

        QStringList previouslyMatchedEpisodes() const;
        void setPreviouslyMatchedEpisodes(const QStringList &previouslyMatchedEpisodes);

        BitTorrent::AddTorrentParams addTorrentParams() const;
        void setAddTorrentParams(BitTorrent::AddTorrentParams addTorrentParams);

        bool matches(const QVariantHash &articleData) const;
        bool accepts(const QVariantHash &articleData);

        friend bool operator==(const AutoDownloadRule &left, const AutoDownloadRule &right);

        QJsonObject toJsonObject() const;
        static AutoDownloadRule fromJsonObject(const QJsonObject &jsonObj, const QString &name = {});

        QVariantHash toLegacyDict() const;
        static AutoDownloadRule fromLegacyDict(const QVariantHash &dict);

    private:
        bool matchesMustContainExpression(const QString &articleTitle) const;
        bool matchesMustNotContainExpression(const QString &articleTitle) const;
        bool matchesEpisodeFilterExpression(const QString &articleTitle) const;
        bool matchesSmartEpisodeFilter(const QString &articleTitle) const;
        bool matchesExpression(const QString &articleTitle, const QString &expression) const;
        QRegularExpression cachedRegex(const QString &expression, bool isRegex = true) const;

        QSharedDataPointer<AutoDownloadRuleData> m_dataPtr;
    };
}
