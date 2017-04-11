/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017  Michael Ziminsky
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
#ifndef RSSDOWNLOADRULE_P_H
#define RSSDOWNLOADRULE_P_H

#include "rssdownloadrule.h"

#include <QHash>
#include <QStringList>

namespace Rss
{
    class DownloadRuleData : public QSharedData
    {
    public:
        QString name;
        QString savePath;
        QString category;
        QStringList rssFeeds;
        bool enabled = false;
        DownloadRule::AddPausedState apstate = DownloadRule::USE_GLOBAL;
        QDateTime lastMatch;
        int ignoreDays = 0;

        QStringList mustContain() const { return m_mustContain; }
        void setMustContain(const QString &tokens);

        QStringList mustNotContain() const { return m_mustNotContain; }
        void setMustNotContain(const QString &tokens);

        QString episodeFilter() const { return m_episodeFilter; }
        void setEpisodeFilter(const QString &episodeFilter);

        bool useRegex() const { return m_useRegex; }
        void setUseRegex(bool useRegex);

        QRegularExpression cachedRegex(const QString &expression, bool isRegex = true) const;

        ~DownloadRuleData();

    private:
        QStringList m_mustContain;
        QStringList m_mustNotContain;
        QString m_episodeFilter;
        bool m_useRegex = false;
        mutable QHash<QString, QRegularExpression> m_cachedRegexes;
    };
}

#endif // RSSDOWNLOADRULE_P_H
