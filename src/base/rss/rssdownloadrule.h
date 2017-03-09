/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2010  Christophe Dumez
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
 *
 * Contact : chris@qbittorrent.org
 */

#ifndef RSSDOWNLOADRULE_H
#define RSSDOWNLOADRULE_H

#include <QDateTime>
#include <QVariantHash>
#include <QSharedDataPointer>
#include <QStringList>

namespace Rss
{
    class Feed;
    using FeedPtr = QSharedPointer<Feed>;
    class DownloadRuleData;

    class DownloadRule
    {
    public:
        enum AddPausedState
        {
            USE_GLOBAL = 0,
            ALWAYS_PAUSED,
            NEVER_PAUSED
        };

        /*!
         * Default constructor represents null
         * Convenient when used in a collection
         */
        DownloadRule();
        DownloadRule(const QString &name);
        ~DownloadRule();

        static DownloadRule fromVariantHash(const QVariantHash &ruleHash);
        QVariantHash toVariantHash() const;
        bool matches(const QString &articleTitle) const;
        void setMustContain(const QString &tokens);
        void setMustNotContain(const QString &tokens);
        QStringList rssFeeds() const;
        void setRssFeeds(const QStringList &rssFeeds);
        QString name() const;
        void setName(const QString &name);
        QString savePath() const;
        void setSavePath(const QString &savePath);
        AddPausedState addPaused() const;
        void setAddPaused(const AddPausedState &aps);
        QString category() const;
        void setCategory(const QString &category);
        bool isEnabled() const;
        void setEnabled(bool enable);
        void setLastMatch(const QDateTime &date);
        QDateTime lastMatch() const;
        void setIgnoreDays(int days);
        int ignoreDays() const;
        QString mustContain() const;
        QString mustNotContain() const;
        bool useRegex() const;
        void setUseRegex(bool enabled);
        QString episodeFilter() const;
        void setEpisodeFilter(const QString &e);
        QStringList findMatchingArticles(const FeedPtr &feed) const;
        bool isSame(const DownloadRule &other) const;

        // Operators
        bool operator==(const DownloadRule &other) const;
        operator bool() const;

    private:
        bool matches(const QString &articleTitle, const QString &expression) const;

    private:
        QSharedDataPointer<DownloadRuleData> d;
    };
}

#endif // RSSDOWNLOADRULE_H
